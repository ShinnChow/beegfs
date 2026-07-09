#include <linux/ratelimit.h>
#include <app/App.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/Metadata.h> // META_ROOTDIR_ID_STR
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/threading/RWLock.h>
#include "InvalReader.h"

#define INOERR_NONE              0
#define INOERR_INVALID_READPOS   1
#define INOERR_QUEUE_OVERFLOW    2
#define INOERR_UNAVAILABLE       3
#define INOERR_COMM_FAILURE      4

static const char *inoerror_string[5] = { "none", "invalid-readpos", "queue-overflow", "unavailable", "comm-failure" };

struct InvalReaderErrState
{
   int lastClass;
   unsigned sameClassCount;
};

struct InvalReaderPerMeta
{
   Thread thread; // base class
   NumNodeID metaID;
   App* app;  // FIXME do we need app and cfg in this struct?
   Logger* log;
   Config* cfg;
   struct list_head listElem;
   bool sessionValid;
   uint32_t lastErrorFlags;
   atomic_t  sessionCnt; // local counter, incremented on every (re-)registration
   bool stopOnStateChange; //false during startup, true if state changes to POFFLINE/OFFLINE
   struct ratelimit_state queueOverflowWarnRateLimit;
   bool queueOverflowWarnSuppressing;
};

struct InvalReader
{
   Thread thread;
   App* app;
   Config* cfg;
   unsigned numMetas;
   NodeStoreEx* metaNodes;
   struct list_head metaThreads;
   struct list_head stoppedThreads; //threads that are stopped, but still need to be joined/freed
   Mutex metaMutex; //protects lifecycle and cond var logic
   RWLock metaThreadsRWLock; //protects metaThreads list access
   bool shuttingDown;
   Condition wakeupCond;
   // Quickfix for Per-Meta object getting deleted: global sessionCnt source stream
   // Otherwise sessionCnt would restart over at 0 when Per-Meta object gets recreated.
   atomic_t globalSessionCnt;
};

static inline void __inval_levelaware_log_fv(Logger *log, LogLevel level, const char *fmt, va_list ap)
{
   if (level <= Logger_getLogLevel(log))
   {
      vprintk(fmt, ap);
   }
}
static inline void __inval_levelaware_log_f(Logger *log, LogLevel level, const char *fmt, ...)
{
      va_list ap;
      va_start(ap, fmt);
      __inval_levelaware_log_fv(log, level, fmt, ap);
      va_end(ap);
}
#define inval_levelaware_log_f(log, level, fmt, ...) do { \
   switch (level) \
   { \
      default: case LOG_NOTHING: break; \
         case Log_ERR:      __inval_levelaware_log_f((log), (level), KERN_ERR fmt, ##__VA_ARGS__); break; \
         case Log_CRITICAL: __inval_levelaware_log_f((log), (level), KERN_CRIT fmt, ##__VA_ARGS__); break; \
         case Log_WARNING:  __inval_levelaware_log_f((log), (level), KERN_WARNING fmt, ##__VA_ARGS__); break; \
         case Log_NOTICE:   __inval_levelaware_log_f((log), (level), KERN_NOTICE fmt, ##__VA_ARGS__); break; \
         case Log_DEBUG:    __inval_levelaware_log_f((log), (level), KERN_DEBUG fmt, ##__VA_ARGS__); break; \
         case Log_SPAM:     __inval_levelaware_log_f((log), (level), KERN_DEBUG fmt, ##__VA_ARGS__); break; \
   } \
} while(0)
#define im_logFormatted(im, level, fmt, ...) \
      inval_levelaware_log_f((im)->log, level, \
            BEEGFS_MODULE_NAME_STR ": InvalRd%d: " fmt, \
            (im)->metaID.value, ##__VA_ARGS__)


static int __InvalReaderErr_update(struct InvalReaderErrState* st, uint32_t errorFlags,
   FhgfsOpsErr remoteRes, unsigned* outLimit)
{
   int errClass = INOERR_NONE;
   unsigned limit = 0;

   if (errorFlags & ReadInvalidations_InvalidReadPos)
   {
      errClass = INOERR_INVALID_READPOS;
      limit = 2;
   }
   else if (errorFlags & ReadInvalidations_QueueOverflow)
   {
      errClass = INOERR_QUEUE_OVERFLOW;
      limit = 2;
   }
   else if (remoteRes == FhgfsOpsErr_COMMTIMEDOUT)
   {
      errClass = INOERR_COMM_FAILURE;
      limit = 10;
   }

   if (errClass == INOERR_NONE)
   {
      st->lastClass = INOERR_NONE;
      st->sameClassCount = 0;
      *outLimit = 0;
      return INOERR_NONE;
   }

   if (st->lastClass == errClass)
      st->sameClassCount++;
   else
   {
      st->lastClass = errClass;
      st->sameClassCount = 1;
   }

   *outLimit = limit;
   return errClass;
}

static bool __InvalReaderErr_reachedLimit(const struct InvalReaderErrState* st, unsigned limit)
{
   return limit && st->sameClassCount >= limit;
}

static void invalidateSession(InvalReaderPerMeta *im)
{
   struct ratelimit_state *rl = &im->queueOverflowWarnRateLimit;
   int missed;
#ifdef KERNEL_HAS_RATELIMIT_STATE_GET_MISS
   missed = ratelimit_state_get_miss(rl);
#else  // before ratelimit_state_get_miss() was added, the missed field was int
   missed = READ_ONCE(rl->missed);
#endif
   if (im->sessionValid)
      atomic_inc(&im->sessionCnt); //increment to even (invalid)
   im->sessionValid = false;
   if (missed)
   {
      im_logFormatted(im, Log_WARNING,
            "%d queue overflow warnings have been suppressed\n", missed);
   }
   im->queueOverflowWarnSuppressing = false;
}

static void delayNextRequest(InvalReaderPerMeta *im, int milliseconds)
{
    const int tickMS = 200;
   int n = 0;  // makeshift request throttling
   while (n < milliseconds &&
      !Thread_getSelfTerminate(&im->thread))
   {
      Thread_sleep(tickMS);
      n += tickMS;
   }
}

static bool __InvalReaderHandleError(InvalReaderPerMeta* im, InvalReader* inval,
   Logger* log, struct InvalReaderErrState* errState,
   uint32_t errorFlags, FhgfsOpsErr remoteRes, bool* shouldBreak)
{
   int errClass;
   unsigned errLimit;

   if (errorFlags & ReadInvalidations_ResyncNeeded)
   {
      im_logFormatted(im, Log_WARNING,
            "Lost sync with meta node. Invalidating cache.\n");
      invalidateSession(im);
      return true;
   }

   if (errorFlags & ReadInvalidations_Disabled)
   {
      if ((im->lastErrorFlags & ReadInvalidations_Disabled) == 0)
      {
         im_logFormatted(im, Log_ERR,
               "Remote cache invalidation disabled on meta\n");
      }
      invalidateSession(im);
      delayNextRequest(im, 10000);
      return true;
   }

   if (errorFlags & ReadInvalidations_Unavailable)
   {
      if ((im->lastErrorFlags & ReadInvalidations_Unavailable) == 0)
      {
         im_logFormatted(im, Log_WARNING,
               "Remote cache invalidation unavailable on meta (server shutting down?)\n");
      }
      invalidateSession(im);
      delayNextRequest(im, 3000);
      return true;
   }

   if (errorFlags & ReadInvalidations_QueueOverflow)
   {
      if (__ratelimit(&im->queueOverflowWarnRateLimit))
      {
         im_logFormatted(im, Log_WARNING,
               "Invalidation queue overflow event. Resetting cache.\n");
         im->queueOverflowWarnSuppressing = false;
      }
      else if (! im->queueOverflowWarnSuppressing)
      {
         im->queueOverflowWarnSuppressing = true;
         im_logFormatted(im, Log_WARNING,
               "Invalidation queue overflow follow-up warnings rate-limited \n");
      }
   }

   if (remoteRes != FhgfsOpsErr_SUCCESS)
   {
      TargetStateStore* metaStateStore = App_getMetaStateStore(im->app);
      CombinedTargetState state;
      bool haveState = TargetStateStore_getState(metaStateStore, im->metaID.value, &state);
      bool isOnline = haveState && state.reachabilityState == TargetReachabilityState_ONLINE;

      //skip error counting on startup
      if (!haveState && !im->stopOnStateChange)
         return true;

      // if node is not online and we are not in startup, terminate
      if (!isOnline && haveState)
      {
         invalidateSession(im);
         if (im->stopOnStateChange)
         {
            im_logFormatted(im, Log_WARNING,
               "Stopping InvalReader, meta node is %s",
               TargetStateStore_reachabilityStateToStr(state.reachabilityState));
            Thread_selfTerminate(&im->thread);
            *shouldBreak = true;
         }
         return true;
      }
      if (remoteRes != FhgfsOpsErr_COMMTIMEDOUT)
      {
         delayNextRequest(im, 3000);
         return true; // retry until state changes, no fallback counting
      }
   }

   errClass = __InvalReaderErr_update(errState, errorFlags, remoteRes, &errLimit);
   if (errClass == INOERR_NONE)
      return false;

   WARN(1, "beegfs: meta=%u inodeInval error class=%d streak=%u/%u flags=0x%x res=%d\n",
         im->metaID.value, errClass, errState->sameClassCount, errLimit, errorFlags, remoteRes);

   im_logFormatted(im, Log_WARNING,
         "received error class=%s streak=%u/%u flags=0x%x res=%d\n",
         inoerror_string[errClass], errState->sameClassCount, errLimit, errorFlags, remoteRes);

   // any tracked error,  invalidate current session and resync from 0
   invalidateSession(im);
   if (__InvalReaderErr_reachedLimit(errState, errLimit))
   {
      im_logFormatted(im, Log_WARNING,
         "Turning off client meta caching feature and falling back to time-based caching due to repeated errors: class=%s streak=%u/%u",
         inoerror_string[errClass], errState->sameClassCount, errLimit);
      App_setInvalWatchFallback(im->app);
      InvalReader_stopInvalReader(inval);
      *shouldBreak = true;
   }
   return true;
}

static bool __InvalReader_metaIsPrimaryOrUnmirrored(InvalReader* this, NumNodeID nodeID)
{
   MirrorBuddyGroupMapper* groups = App_getMetaBuddyGroupMapper(this->app);
   uint16_t groupID;
   uint16_t primaryID;

   if (!groups)
      return true;

   groupID = MirrorBuddyGroupMapper_getBuddyGroupID(groups, nodeID.value);

   if (!groupID)
      return true; // not mirrored

   primaryID = MirrorBuddyGroupMapper_getPrimaryTargetID(groups, groupID);

   return primaryID == nodeID.value;
}

static void __InvalReader_requestStopAllThreads(InvalReader* this)
{
   InvalReaderPerMeta* im;
   RWLock_readLock(&this->metaThreadsRWLock);
   list_for_each_entry(im, &this->metaThreads, listElem)
      Thread_selfTerminate((Thread*)&im->thread);
   RWLock_readUnlock(&this->metaThreadsRWLock);
}

static void __InvalReader_joinStoppedMetas(InvalReader* this)
{
   InvalReaderPerMeta* im;

   for (;;)
   {
      Mutex_lock(&this->metaMutex);
      if (list_empty(&this->stoppedThreads))
      {
         Mutex_unlock(&this->metaMutex);
         break;
      }

      im = list_first_entry(&this->stoppedThreads, InvalReaderPerMeta, listElem);
      list_del_init(&im->listElem);
      Mutex_unlock(&this->metaMutex);

      Thread_join((Thread*)&im->thread);
      Thread_uninit((Thread*)&im->thread);
      kfree(im);
   }
}

static void __InvalReader_joinAllThreads(InvalReader* this)
{
   for (;;)
   {
      // join already stopped threads
      __InvalReader_joinStoppedMetas(this);

      Mutex_lock(&this->metaMutex);
      if (!this->numMetas)
      {
         Mutex_unlock(&this->metaMutex);
         break; // no active threads
      }

      Condition_waitKillable(&this->wakeupCond, &this->metaMutex);
      Mutex_unlock(&this->metaMutex);
   }

   // drain stopped threads again
   __InvalReader_joinStoppedMetas(this);
}
   
// InvalReader thread, mostly sitting around bored.
static void __InvalReader_run(Thread* thread)
{
   InvalReader* this = (InvalReader*)thread;

   while (!Thread_getSelfTerminate(thread))
   {
      Mutex_lock(&this->metaMutex);

      while (list_empty(&this->stoppedThreads) && !Thread_getSelfTerminate(thread))
         Condition_waitKillable(&this->wakeupCond, &this->metaMutex);
      Mutex_unlock(&this->metaMutex);

      __InvalReader_joinStoppedMetas(this);
   }
   __InvalReader_requestStopAllThreads(this); // defensive, no-op when stopInvalReader was called
   __InvalReader_joinAllThreads(this);
}

static void __InvalReaderPerMeta_run(Thread* this)
{
   InvalReaderPerMeta* im = (InvalReaderPerMeta*)this;
   struct InvalReaderErrState errState = { INOERR_NONE, 0 };
   App *app = im->app;
   Logger* log = App_getLogger(app);
   uint32_t readPos = 0;
   FhgfsOpsErr remoteRes;
   InvalReader* inval = app->invalReader;
   bool shouldBreak = false;
   InodeId* inodeBuf;
   uint32_t localNodeID = 0;

   inodeBuf = os_kmalloc(sizeof(*inodeBuf) * READINVALIDATIONS_MAX_IDS);

   if (unlikely(!inodeBuf))
   {
      Logger_logFormatted(log, Log_ERR, __func__,
         "Failed to allocate invalidation buffer for metaID %u", im->metaID.value);
      Logger_logFormatted(log, Log_ERR,  __func__,
         "Turning off client meta caching feature and falling back to time-based caching.");
      App_setInvalWatchFallback(app);
      InvalReader_stopInvalReader(inval);
      shouldBreak = true;
   }

   while (!Thread_getSelfTerminate(this)  && !shouldBreak)
   {
      bool resync = false;
      ReadInvalidationsData outData;
      outData.errorFlags = 0;
      outData.numUpdates = 0;
      outData.inodeIds = inodeBuf;
      outData.outReadPos = readPos;
      localNodeID  = Node_getNumID(App_getLocalNode(app)).value;


      if (!localNodeID)
      {
         //nodeID is an invalid value, skip request
         delayNextRequest(im, 1000);
         goto nextRequest;
      }
      if (! im->sessionValid)
      {
         readPos = 0;
         resync = true;
      }

      remoteRes = FhgfsOpsRemoting_readInvalidations(app, im->metaID,
         readPos, resync, &outData);

      if (__InvalReaderHandleError(im, inval, log, &errState,
         outData.errorFlags, remoteRes, &shouldBreak))
      {
         goto nextRequest;
      }

      im->stopOnStateChange = true;

      if (resync)
      {
         // draw new unique sessionCnt from global source. +1 to make it an odd number
         // (meaning valid).
         int sessionCnt = atomic_fetch_add(2, &inval->globalSessionCnt);
         atomic_set(&im->sessionCnt, sessionCnt + 1);

         im->sessionValid = true;
      }

      {
         int32_t i;
         for (i = 0; i < outData.numUpdates; i++)
         {
            char entryID[27];
            struct inode *inode = NULL;
            {
               InodeId *id = &outData.inodeIds[i];
               //special treatment for root inode
               if (id->a == 0 && id->b == 0 && id->c == 0)
               {
                  strscpy(entryID, META_ROOTDIR_ID_STR, sizeof(entryID));
               }
               else
               {
                  snprintf(entryID, sizeof entryID, "%X-%X-%X", id->a, id->b, id->c);

               }
               inode = FhgfsInode_GetInodeFromEntryID(app->superBlock, entryID);
            }

            LOG_DEBUG_FORMATTED(log, Log_DEBUG, __func__, "Invalidating inode %s", entryID);

            if (inode)
            {
               FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
               FhgfsInode_invalidate(fhgfsInode);
               iput(inode);
            }
         }

         atomic64_fetch_add(outData.numUpdates, &app->numRemoteInvals);
      }

      readPos = outData.outReadPos;

nextRequest:
      im->lastErrorFlags = outData.errorFlags;
   }

   kfree(inodeBuf);
   Mutex_lock(&inval->metaMutex);
   RWLock_writeLock(&inval->metaThreadsRWLock);
   list_del_init(&im->listElem);
   list_add_tail(&im->listElem, &inval->stoppedThreads);
   if (inval->numMetas)
      inval->numMetas--;
   im_logFormatted(im, Log_NOTICE, "inval thread shutting down: numMetas=%u\n", inval->numMetas);
   RWLock_writeUnlock(&inval->metaThreadsRWLock);
   Condition_signal(&inval->wakeupCond);
   Mutex_unlock(&inval->metaMutex);
}

static void __InvalReader_init(InvalReader* this, App* app)
{
   Thread_init(&this->thread, BEEGFS_THREAD_NAME_PREFIX_STR "InvalMain", __InvalReader_run);
   this->app = app;
   this->cfg = App_getConfig(app);
   this->metaNodes = App_getMetaNodes(app);

   INIT_LIST_HEAD(&this->metaThreads);
   INIT_LIST_HEAD(&this->stoppedThreads);
   this->numMetas = 0;
   Mutex_init(&this->metaMutex);
   RWLock_init(&this->metaThreadsRWLock);
   this->shuttingDown = false;
   Condition_init(&this->wakeupCond);
   atomic_set(&this->globalSessionCnt, 0);
   // Per-meta threads are NOT started here: the meta NodeStoreEx is still empty at construction time

}

//caller must hold metaThreadsRWLock
static InvalReaderPerMeta* __InvalReader_find(InvalReader* this, NumNodeID id)
{
   InvalReaderPerMeta* metaEntry;
   list_for_each_entry(metaEntry, &this->metaThreads, listElem)
   {
      if (metaEntry->metaID.value == id.value)
      {
         return metaEntry;
      }
   }
   return NULL;
}

int32_t InvalReader_getMetaSessionCnt(InvalReader* this, NumNodeID nodeId)
{
   int32_t ret = 0;
   InvalReaderPerMeta* im;
   RWLock_readLock(&this->metaThreadsRWLock);
   im = __InvalReader_find(this, nodeId);
   if (im)
      ret = atomic_read(&im->sessionCnt);
   RWLock_readUnlock(&this->metaThreadsRWLock);
   return ret;
}

void InvalReader_stopInvalReader(InvalReader* this)
{
   Thread_selfTerminate( (Thread*)this);
   //wake up main thread
   Mutex_lock(&this->metaMutex);
   this->shuttingDown = true;
   Condition_signal(&this->wakeupCond);
   Mutex_unlock(&this->metaMutex);
   __InvalReader_requestStopAllThreads(this);

}

void InvalReader_construct(App* app)
{
   NodeStoreEx *nodes = App_getMetaNodes(app);
   InvalReader *this;

   if (!nodes)
   {
      Logger_logFormatted(App_getLogger(app), Log_WARNING, __func__, "nodes is NULL\n");
      return;
   }

   this = (InvalReader*)os_kmalloc(sizeof(*this));
   app->invalReader = this;

   if(likely(this))
      __InvalReader_init(this, app);
}

static void __InvalReader_uninit(InvalReader* this)
{
   Thread_uninit(&this->thread);
   Mutex_uninit(&this->metaMutex);
}

void InvalReader_destruct(InvalReader* this)
{
   Logger_logFormatted(App_getLogger(this->app), Log_NOTICE, __func__, "destroying (numMetas=%u)\n", this->numMetas);
   __InvalReader_uninit(this);
   kfree(this);
}

void InvalReader_startMetaThread(InvalReader* this, NumNodeID nodeID)
{
   InvalReaderPerMeta *im = NULL;

   if (!__InvalReader_metaIsPrimaryOrUnmirrored(this, nodeID))
      return; //return if node is secondary in mirror group

   Mutex_lock(&this->metaMutex);
   RWLock_readLock(&this->metaThreadsRWLock);
   im = __InvalReader_find(this, nodeID);

   if (im)
   {
      if (!Thread_getSelfTerminate((Thread*)&im->thread))
      {
         RWLock_readUnlock(&this->metaThreadsRWLock);
         Mutex_unlock(&this->metaMutex);
         return; // thread already active
      }
      while ((im = __InvalReader_find(this, nodeID)) &&
         Thread_getSelfTerminate((Thread*)&im->thread) &&
         !this->shuttingDown)
      {
         RWLock_readUnlock(&this->metaThreadsRWLock);
         Condition_waitKillable(&this->wakeupCond, &this->metaMutex);
         RWLock_readLock(&this->metaThreadsRWLock);
      }

      if (im) // thread was shutting down, became active again
      {
         RWLock_readUnlock(&this->metaThreadsRWLock);
         Mutex_unlock(&this->metaMutex);
         return;
      }
   }
   RWLock_readUnlock(&this->metaThreadsRWLock);
   Mutex_unlock(&this->metaMutex);

   im = (InvalReaderPerMeta*)os_kmalloc(sizeof(*im));
   if (!im)
   {
      return;
   }

   {
      char name[256];
      snprintf(name, sizeof name, BEEGFS_THREAD_NAME_PREFIX_STR "Inval%u", nodeID.value);
      Thread_init((Thread*)&im->thread, name, __InvalReaderPerMeta_run);
   }
   im->metaID = nodeID;
   im->app = this->app;
   im->log = App_getLogger(this->app);
   im->cfg = this->cfg;

   Mutex_lock(&this->metaMutex);
   {
      //need to check if thread is already active to avoid duplicate entry
      InvalReaderPerMeta* existing;
      RWLock_readLock(&this->metaThreadsRWLock);
      existing = __InvalReader_find(this, nodeID);

      if (existing)
      {
         while (existing &&
            Thread_getSelfTerminate((Thread*)&existing->thread) &&
            !this->shuttingDown)
         {
            //thread active but shutting down, wait
            RWLock_readUnlock(&this->metaThreadsRWLock);
            Condition_waitKillable(&this->wakeupCond, &this->metaMutex);
            RWLock_readLock(&this->metaThreadsRWLock);
            existing = __InvalReader_find(this, nodeID);
         }

         if (existing)
         {
            RWLock_readUnlock(&this->metaThreadsRWLock);
            Mutex_unlock(&this->metaMutex);
            Thread_uninit((Thread*)&im->thread);
            kfree(im);
            return; // still active
         }
      }
      RWLock_readUnlock(&this->metaThreadsRWLock);
      if (this->shuttingDown || App_getInvalWatchFallback(this->app))
      {
         Mutex_unlock(&this->metaMutex);
         Thread_uninit((Thread*)&im->thread);
         kfree(im);
         return; // shutdown in progress, do not start
      }
   }
   RWLock_writeLock(&this->metaThreadsRWLock);
   // First request always resyncs with meta. sessionCnt starts at 0 and
   // is incremented to 1 (or higher) after the very first request.
   im->sessionValid = false;
   im->lastErrorFlags = 0;
   im->stopOnStateChange = false;
   ratelimit_state_init(&im->queueOverflowWarnRateLimit, 20 * HZ, 2);  // 2 warnings per 20 seconds
   im->queueOverflowWarnSuppressing = false;
   atomic_set(&im->sessionCnt, 0); //initialize as even (invalid)
   list_add(&im->listElem, &this->metaThreads);
   this->numMetas++;
   if (!Thread_start((Thread*)&im->thread))
   {
      list_del_init(&im->listElem);
      if (this->numMetas)
         this->numMetas--;

      RWLock_writeUnlock(&this->metaThreadsRWLock);
      Mutex_unlock(&this->metaMutex);

      Thread_uninit((Thread*)&im->thread);
      kfree(im);
      return;
   }
   RWLock_writeUnlock(&this->metaThreadsRWLock);
   Mutex_unlock(&this->metaMutex);
   im_logFormatted(im, Log_NOTICE, "started. (numMetas=%u)", this->numMetas);
}

void InvalReader_stopMetaThread(InvalReader* this, NumNodeID nodeID)
{
   InvalReaderPerMeta* im;
   RWLock_readLock(&this->metaThreadsRWLock);
   im = __InvalReader_find(this, nodeID);
   if (im)
   {
      if (im->sessionValid)
         atomic_inc(&im->sessionCnt);

      im->sessionValid = false;
      Thread_selfTerminate((Thread*)&im->thread);
   }
   RWLock_readUnlock(&this->metaThreadsRWLock);
}

/**
 * Returns true only if the meta target state is known and ONLINE.
 *
 * Used to avoid spawning per-meta reader threads against metas that are down/unreachable. Such a
 * thread would never complete its initial request, never flip stopOnStateChange, and so retry
 * indefinitely instead of terminating - which can in turn block mount/unmount. Metas whose state
 * is not (yet) known to be ONLINE are skipped here and started later by the InternodeSyncer
 * target-state path once they transition to ONLINE.
 */
static bool __InvalReader_metaIsOnline(InvalReader* this, NumNodeID nodeID)
{
   TargetStateStore* metaStateStore = App_getMetaStateStore(this->app);
   CombinedTargetState state;
   bool haveState = TargetStateStore_getState(metaStateStore, nodeID.value, &state);

   return haveState && state.reachabilityState == TargetReachabilityState_ONLINE;
}

void InvalReader_updateMetaNodes(InvalReader* this,
   NumNodeIDList* added, NumNodeIDList* removed)
{
   NumNodeIDListIter iter;

   Mutex_lock(&this->metaMutex);
   if (this->shuttingDown || App_getInvalWatchFallback(this->app))
   {
      Mutex_unlock(&this->metaMutex);
      return;
   }
   Mutex_unlock(&this->metaMutex);

   NumNodeIDListIter_init(&iter, added);
   while (!NumNodeIDListIter_end(&iter))
   {
      NumNodeID nodeID = NumNodeIDListIter_value(&iter);
      NumNodeIDListIter_next(&iter);

      // Only start a reader thread for metas currently known to be ONLINE.
      if (!__InvalReader_metaIsOnline(this, nodeID))
         continue;

      InvalReader_startMetaThread(this, nodeID);
   }
   
   NumNodeIDListIter_init(&iter, removed);
   while (!NumNodeIDListIter_end(&iter))
   {
      NumNodeID nodeID = NumNodeIDListIter_value(&iter);
      NumNodeIDListIter_next(&iter);

      InvalReader_stopMetaThread(this, nodeID);
   }
   Mutex_lock(&this->metaMutex);
   Logger_logFormatted(App_getLogger(this->app), Log_NOTICE, __func__, "called. (numMetas=%u)\n", this->numMetas);
   Mutex_unlock(&this->metaMutex);
}
