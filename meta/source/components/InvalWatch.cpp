#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <common/app/log/LogContext.h>
#include <common/components/worker/JustWork.h>
#include <common/storage/Metadata.h>
#include <components/InvalWatch.h>
#include <program/Program.h>

// 64 bytes is the cache line size on x86-64 and ARM64.
// For portability, std::hardware_destructive_interference_size (C++17) could
// be used, but compilers warn it's implementation-defined.
static constexpr size_t BEEGFS_CACHELINE_SIZE = 64;
#define BEEGFS_CACHEALIGN alignas(BEEGFS_CACHELINE_SIZE)

// The number of simultaneously connected clients won't exceed 2^16 for the
// time being. So we can do a local mapping of the 32-bit client ids commonly
// used elsewhere in the codebase (NumNodeID), to 16-bit ids that we use
// locally only in this module.
struct Id16
{
   uint16_t index;
};

// invalidation queue (per-watcher / per-session)
struct InvalQueue  // NOLINT(clang-analyzer-optin.performance.Padding)
{
   // queue byte cap is global and configurable: see invalwatch_max_queue_bytes
   // actual queue, including "infinite" cursors
   std::deque<char> queue;
   uint32_t read_pos = 0;
   uint32_t write_pos = 0;
   bool queue_overflow = false;
   bool synced = false;
   BEEGFS_CACHEALIGN std::mutex mutex;  // serializes (read or write) accesses to the contained deque.
   std::condition_variable writecond;  // triggered whenever there is a write. (Should we constrain this?)

   // Reset queue data to initial empty state. Caller must hold mutex.
   // Leaves mutex and writecond intact (they may be referenced by concurrent
   // writers and must not be destroyed/reconstructed).
   void reset()
   {
      queue.clear();
      read_pos = 0;
      write_pos = 0;
      queue_overflow = false;
      synced = false;
   }
};

struct Watcher  // NOLINT(clang-analyzer-optin.performance.Padding)
{
   // TODO: if subscriptions ever hold back-references on their watcher, the
   // refcount could reach the number of subscribed InvalTargets (potentially
   // hundreds of millions). Switch to uint64_t at that point.
   std::atomic<uint32_t> refcount = 1;
   uint32_t sessionId;
   Id16 id16;
   // Mutex to ensure only one read request for this watcher is served at any
   // given time.
   // The InvalQueue itself has another mutex to serialize any access
   // (read or write) to the queue data structure.
   // But because of condvar waits (which release the mutex temporarily),
   // we can't use that mutex to protect the entire read transaction.
   // That's why we need this second mutex here.
   BEEGFS_CACHEALIGN std::mutex invalidationReadMutex;
};

struct InvalFanout
{
   InodeId inodeId;
   std::vector<Id16> watchers;
   InvalFanout *next = NULL;
};

struct InvalFanoutQueue
{
   std::mutex queue_mutex;
   InvalFanout *head = NULL;
   InvalFanout *tail = NULL;
};

struct WatcherStore  // NOLINT(clang-analyzer-optin.performance.Padding)
{
   BEEGFS_CACHEALIGN std::mutex mutex;

   // 32-bit client id => Id16
   BEEGFS_CACHEALIGN std::unordered_map<uint32_t, Id16> id_map;

   // Id16 => Watcher
   BEEGFS_CACHEALIGN Watcher *array[1<<16];

   // keep track of unused slots in array in a fixed size ringbuffer
   BEEGFS_CACHEALIGN uint16_t free_ids[1<<16];
   BEEGFS_CACHEALIGN uint32_t free_ids_writepos;
   BEEGFS_CACHEALIGN uint32_t free_ids_readpos;
};

struct InvalTarget
{
   std::mutex mutex;
   std::atomic<uint32_t> refcount = 1;
   InodeId inodeId;
   InvalFanout *fanout = NULL;
   // target gets linked in LRU cache while refcount == 0.
   InvalTarget *lruPrev = NULL;
   InvalTarget *lruNext = NULL;
};

struct InodeIdHasher { size_t operator()(InodeId const& inodeId) const; };
struct InodeIdComparator { bool operator()(InodeId const& a, InodeId const& b) const; };
using InvalTargetMap = std::unordered_map<InodeId, InvalTarget *, InodeIdHasher, InodeIdComparator>;


/* Eviction pool for InvalTarget objects.
 * This holds un-referenced inodes, which are candidates for eviction.
 * Currently this holds a single linked list of all such inodes.
 * In the future we should group inodes by eviction "score". Highest eviction
 * score objects are the primary candidates for eviction.
 * We should probably also shard the pool to allow for better concurrency.
 */
struct InvalTargetEvictionPool
{
   BEEGFS_CACHEALIGN
   InvalTarget *lruFirst = NULL;
   InvalTarget *lruLast = NULL;
};

struct InvalTargetStore
{
   std::mutex mutex;
   InvalTargetMap targets;

   // LRU cache for unreferenced nodes (refcount == 0)
   InvalTargetEvictionPool evictionPool;
};

struct PerWatcherStats
{
   // written by add_target_watch, read by mon.
   // We currently accept small chance of "tearing" i.e. the variables get read and written independently.
   std::atomic<uint32_t> sessionId;
   std::atomic<uint32_t> watchedTargetCount = 0;     // current watched inodes
   std::atomic<uint64_t> numInvalidations = 0;  // cumulative invalidations sent
};

struct InvalWatchCacheStats
{
   // directly written using atomics, maybe not the most efficient
   BEEGFS_CACHEALIGN std::atomic<uint64_t> currentSubscriptionCount = 0;
   BEEGFS_CACHEALIGN std::atomic<uint32_t> currentWatcherCount = 0;
   BEEGFS_CACHEALIGN std::atomic<uint64_t> currentTargetCount = 0;
   BEEGFS_CACHEALIGN std::atomic<uint64_t> totalInvalidationsCount = 0;
};

// Note, the InvalQueue array, and to a lesser degree the watcher_stats array,
// are currently separately stored, and unsynchronized from the watcher array in the watcher_store.
// This allows watchers to be deleted when there are still watch list entries in the inodes,
// and invalidations being broadcast from watch lists to invalidation queues.
// This simplifies watcher deregistration but it currently means that future
// watcher objects can inherit spurious invalidations.
BEEGFS_CACHEALIGN static WatcherStore watcher_store;
BEEGFS_CACHEALIGN static InvalQueue inval_queue_array[1<<16]; // Id16 => InvalQueue
BEEGFS_CACHEALIGN static PerWatcherStats watcher_stats[1<<16]; // Id16 => PerWatcherStats
BEEGFS_CACHEALIGN static InvalTargetStore global_invaltarget_store;
BEEGFS_CACHEALIGN static InvalFanoutQueue inval_fanout_queue;
BEEGFS_CACHEALIGN static std::atomic<bool> global_shutdown_flag;

BEEGFS_CACHEALIGN
static std::atomic<bool> invalwatch_enabled;
static std::atomic<uint64_t> invalwatch_max_cached_objects;
// Configurable via tuneInvalWatchQueueSize;
// default is the hardcoded 4 MB.
static std::atomic<uint32_t> invalwatch_max_queue_bytes{ (uint32_t) 1 << 22 };

BEEGFS_CACHEALIGN static InvalWatchCacheStats inval_watch_cache_stats;


static void InvalWatch_logmsgfv(LogLevel logLevel, const char *fmt, va_list ap)
{
   char buf[1024];
   int ret = vsnprintf(buf, sizeof buf, fmt, ap);
   if (ret >= (int) sizeof buf)
      buf[sizeof buf - 1] = 0;
   LogContext("InvalWatch").log(logLevel, buf);
}

__attribute__((format(printf, 2, 3)))
static void InvalWatch_logmsgf(LogLevel logLevel, const char *fmt, ...)  // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   InvalWatch_logmsgfv(logLevel, fmt, ap);
   va_end(ap);
}

static void watcher_logmsgfv(Watcher *watcher, LogLevel logLevel, const char *fmt, va_list ap)
{
   char buf[1024];
   int ret = snprintf(buf, sizeof buf, "watcher %" PRIu32 "(%" PRIu16 "): ",
         watcher->sessionId, watcher->id16.index);
   if (ret < (int) sizeof buf)
      vsnprintf(buf + ret, sizeof buf - ret, fmt, ap);
   LogContext("InvalWatch").log(logLevel, buf);
}

__attribute__((format(printf, 3, 4)))
static void watcher_logmsgf(Watcher *watcher, LogLevel logLevel, const char *fmt, ...)  // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   watcher_logmsgfv(watcher, logLevel, fmt, ap);
   va_end(ap);
}

#ifdef BEEGFS_DEBUG
#define InvalWatch_logmsgf_debug(...) InvalWatch_logmsgf(__VA_ARGS__)
#define watcher_logmsgf_debug(...) watcher_logmsgf(__VA_ARGS__)
#else
#define InvalWatch_logmsgf_debug(...)
#define watcher_logmsgf_debug(...)
#endif

// Caller must hold mutex. Mutex remains locked on return.
static std::cv_status condvar_wait_until(
      std::mutex *mutex,
      std::condition_variable *condvar,
      std::chrono::time_point<std::chrono::steady_clock> deadline)
{
   std::unique_lock<std::mutex> lock(*mutex, std::adopt_lock);
   std::cv_status status = condvar->wait_until(lock, deadline);
   lock.release();  // NOTE: does NOT unlock
   return status;
}

size_t InodeIdHasher::operator()(InodeId const& inodeId) const
{
   std::string_view sv = std::string_view((char *) inodeId.buffer, sizeof inodeId.buffer);
   return std::hash<std::string_view>()(sv);
}

bool InodeIdComparator::operator()(InodeId const& a, InodeId const& b) const
{
   return memcmp(a.buffer, b.buffer, sizeof a.buffer) == 0;
}

struct Reader
{
   const unsigned char *buffer;
};

struct Writer
{
   unsigned char *buffer;
};

static bool read_char(Reader *reader, char c)
{
   assert(c != 0);
   if ((char) *reader->buffer != c)
      return false;
   ++ reader->buffer;
   return true;
}

static bool read_hex_uint32(Reader *reader, uint32_t *out)
{
   char *buffer = (char *) reader->buffer;
   char *end = NULL;
   errno = 0;
   unsigned long val = std::strtoul(buffer, &end, 16);
   if (errno || end == buffer)
      return false;
   if (val > std::numeric_limits<uint32_t>::max())
      return false;
   reader->buffer = (unsigned char *) end;
   *out = val;
   return true;
}

static void write_uint32_le(Writer *writer, uint32_t val)
{
   *writer->buffer++ = (unsigned char) (val >> 0);
   *writer->buffer++ = (unsigned char) (val >> 8);
   *writer->buffer++ = (unsigned char) (val >> 16);
   *writer->buffer++ = (unsigned char) (val >> 24);
}

bool read_inodeid(std::string const& entryID, InodeId *out)
{

   if (entryID == META_ROOTDIR_ID_STR)
   {
      // root has a non-hex entryID ("root").
      // we map it to the reserved all-zero InodeId
      // matches what is used by the NFS export path
      // (see __FhgfsOpsExport_parseEntryIDForNfsHandle)
      memset(out->buffer, 0, sizeof out->buffer);
      return true;
   }

   uint32_t a, b, c;
   Reader reader = { (unsigned char *) entryID.c_str() };
   if (! read_hex_uint32(&reader, &a))
      return false;
   if (! read_char(&reader, '-'))
      return false;
   if (! read_hex_uint32(&reader, &b))
      return false;
   if (! read_char(&reader, '-'))
      return false;
   if (! read_hex_uint32(&reader, &c))
      return false;
   Writer writer = { (unsigned char *) out->buffer };
   write_uint32_le(&writer, a);
   write_uint32_le(&writer, b);
   write_uint32_le(&writer, c);
   return true;
}

static void __delete_unreferenced_watcher_wstore_unlocked(Watcher *watcher)
{
   assert(watcher->refcount.load() == 0);
   watcher_logmsgf_debug(watcher, Log_DEBUG, "deleting unreferenced watcher");

   // TODO: we need to delete watch list entries (16-bit watcher ids) for this
   // watcher. For now, we keep the entries in the watch lists. Because the
   // whole 16-bit id space will be used before we wrap around, those ids might
   // stay linked in there for a long time. And when that happens, new watchers
   // will inherit old subscriptions. This is currently accepted because getting
   // spurious invalidations is not a correctness problem. But the bigger problem
   // is memory consumption. We will probably need to clean watch lists in a
   // background task periodically.

   Id16 id16 = watcher->id16;
   uint32_t sessionId = watcher->sessionId;

   // Make all changes, including resetting the queue and resetting the
   // monitoring data, under the wstore lock.
   assert(watcher_store.array[id16.index] == watcher);
   watcher_store.array[id16.index] = NULL;
   watcher_store.id_map.erase(sessionId);
   watcher_store.free_ids[(uint16_t) watcher_store.free_ids_writepos ++] = id16.index;
   // reset inval queue.
   {
      InvalQueue *ivq = &inval_queue_array[id16.index];
      ivq->mutex.lock();
      ivq->reset();
      ivq->mutex.unlock();
   }
   // reset monitoring data
   watcher_stats[id16.index].sessionId.store(0, std::memory_order_relaxed);
   watcher_stats[id16.index].numInvalidations.store(0, std::memory_order_relaxed);
   watcher_stats[id16.index].watchedTargetCount.store(0, std::memory_order_relaxed);
   //
   inval_watch_cache_stats.currentWatcherCount.fetch_sub(1, std::memory_order_relaxed);

   delete watcher;
}

void put_watcher(Watcher *watcher)
{
   uint32_t refcount = watcher->refcount.fetch_sub(1, std::memory_order_acq_rel) - 1;
   watcher_logmsgf_debug(watcher, Log_DEBUG, "put_watcher(): new refcount=%" PRIu32, refcount);
   if (refcount == 0)
   {
      // New concurrent inc-refs could still happen (lookup_watcher())
      // Lock the watcher store and check again.
      watcher_store.mutex.lock();
      if (watcher->refcount.load(std::memory_order_relaxed) == 0)
      {
         __delete_unreferenced_watcher_wstore_unlocked(watcher);
      }
      watcher_store.mutex.unlock();
   }
}

// Force the client to resync. Used when watch registration fails (OOM) —
// the client was never registered for future invalidations, so its cached
// data may become stale without notification.
//
// NOTE: setting synced=false semantically implies a meta restart, which is
// not the actual cause. This is a deliberate approximation — the client's
// response (full resync) is correct even if the reason is imprecise.
// A proper per-cause signal would require a protocol extension. TODO.
//
// NOTE: this does not relieve memory pressure. Watch list entries
// referencing this watcher are not cleaned up; that requires a future
// background cleanup mechanism.
static void force_watcher_resync(Watcher *watcher)
{
   InvalQueue *ivq = &inval_queue_array[watcher->id16.index];
   ivq->mutex.lock();
   ivq->reset();
   ivq->mutex.unlock();
   watcher_logmsgf(watcher, Log_WARNING, "Forced resync due to OOM in watch registration");
}

uint32_t get_watcher(Watcher *watcher)
{
   uint32_t oldcount = watcher->refcount.fetch_add(1, std::memory_order_relaxed);
   return oldcount + 1;
}

Watcher *lookup_watcher(uint32_t sessionId)
{
   if (!invalwatch_enabled.load(std::memory_order_relaxed))  // check required?
      return NULL;
   Watcher *watcher = NULL;
   uint32_t refcount;
   watcher_store.mutex.lock();
   {
      // if app is already shutting down, we don't create the watcher.
      // otherwise, if shutdown flag is set afterwards, shutdown process will
      // be able to drop the watcher we're creating here, since we're
      // synchronizing through watcher_store.mutex.
      if (global_shutdown_flag.load(std::memory_order_relaxed))
      {
         goto out_wstore_unlock;
      }

      auto it = watcher_store.id_map.find(sessionId);
      if (it != watcher_store.id_map.end())
      {
         uint16_t idval = it->second.index;
         watcher = watcher_store.array[idval];
         refcount = get_watcher(watcher); // this must happen under the wstore lock!
      }
      else
      {
         if (watcher_store.free_ids_readpos == watcher_store.free_ids_writepos)
         {
            // no free slots available.
            // Give up and return NULL pointer.
            // NOTE: For the future we can consider evicting a watcher from the
            // 16-bit registration space to make room for a new one.
            // We would still keep the object around and linked using the 32-bit
            // session id, but marked as invalid.
            goto out_wstore_unlock;
         }

         uint16_t idval = watcher_store.free_ids[(uint16_t) watcher_store.free_ids_readpos ++];

         watcher = new Watcher;
         watcher->sessionId = sessionId;
         watcher->id16 = {idval};

         refcount = watcher->refcount.load();
         assert(refcount == 1);

         assert(watcher_store.array[idval] == NULL);
         watcher_store.array[idval] = watcher;
         watcher_store.id_map.emplace(sessionId, Id16{idval});

         {
            InvalQueue *ivq = &inval_queue_array[idval];
            ivq->mutex.lock();
            assert(ivq->queue.empty()); // should be reset
            assert(ivq->read_pos == 0 && ivq->write_pos == 0); // should be reset
            assert(ivq->queue_overflow == false);  // should be reset
            assert(!ivq->synced); // should be reset
            ivq->mutex.unlock();
         }

         // make sure counters are reset... TODO review this, we shouldn't have to resort to such "safety measures"
         watcher_stats[idval].sessionId.store(sessionId, std::memory_order_relaxed);
         watcher_stats[idval].watchedTargetCount.store(0, std::memory_order_relaxed);
         watcher_stats[idval].numInvalidations.store(0, std::memory_order_relaxed);

         inval_watch_cache_stats.currentWatcherCount.fetch_add(1, std::memory_order_relaxed);
      }
   }
out_wstore_unlock:
   watcher_store.mutex.unlock();

   if (watcher)
   {
      InvalWatch_logmsgf_debug(Log_DEBUG,
            "lookup_watcher(%" PRIu32 "): "
            "Id16=%" PRIu16 " refcount=%" PRIu32,
            watcher->sessionId, watcher->id16.index, refcount);
   }
   else if (global_shutdown_flag.load(std::memory_order_relaxed))
   {
      InvalWatch_logmsgf_debug(Log_DEBUG,
            "lookup_watcher(sessionId=%" PRIu32 ") failed: "
            "system shutting down", sessionId);
   }
   else
   {
      InvalWatch_logmsgf(Log_WARNING,
            "lookup_watcher() failed: sessionID %" PRIu32, sessionId);
   }
   return watcher;
}

static void shut_down_watcher_store()
{
   // We need to interrupt all threads reading from watcher invalidation queues.
   // Another way to achieve the same thing with less boilerplate _could_ be
   // to make a mechanism where threads can register themselves for certain events
   // (such as global shutdown) prior to waiting on condvars, so they can be
   // interrupted when these events happen.
   // I've made such a mechanism before, but I wasn't entirely happy: My conclusion
   // is that we should "terminate" data structures (like this function does),
   // not threads. It somehow feels wrong to just stop a thread, there's
   // context missing.
   // Of course, as with all correct theory, in practice we don't follow it entirely:
   // we still have the global_shutdown_flag here.
   watcher_store.mutex.lock();
   {
      for (auto it : watcher_store.id_map)
      {
         Id16 id16 = it.second;
         InvalQueue *ivq = &inval_queue_array[id16.index];
         ivq->mutex.lock();
         {
            ivq->writecond.notify_one();
         }
         ivq->mutex.unlock();
      }
   }
   watcher_store.mutex.unlock();
}

static void initialize_watcher_store()
{
   for (uint32_t i = 0; i < 1<<16; i++)
      watcher_store.free_ids[i] = (uint16_t) i;
   watcher_store.free_ids_readpos = 0;
   watcher_store.free_ids_writepos = 1 << 16;
}

static void fanout_invals(void *)
{
   InvalFanoutQueue *fq = &inval_fanout_queue;

   // maybe sleep for a tiny amount. The hope is that this will amortize the
   // overhead of creating and running this job, and dequeuing the entries.
   std::this_thread::sleep_for(std::chrono::microseconds(1000));

   // Take all pending entries at once under a short lock, then process without holding it.
   InvalFanout *list = NULL;
   fq->queue_mutex.lock();
   list = fq->head;
   fq->head = NULL;
   fq->tail = NULL;
   fq->queue_mutex.unlock();

   // For each 16-bit watcher id in each watch set, we only access the static
   // inval_queue_array[] and watcher_stats[] arrays (indexed by Id16), which
   // are always valid. We do NOT dereference Watcher* pointers here;
   // the watcher object itself may have been deleted in the meantime.

   for (InvalFanout *entry = list; entry != NULL; )
   {
      InvalFanout *next = entry->next;
      InodeId inodeId = entry->inodeId;
      for (Id16 id16 : entry->watchers)
      {
         // Update stats
         watcher_stats[id16.index].numInvalidations.fetch_add(1, std::memory_order_relaxed);
         watcher_stats[id16.index].watchedTargetCount.fetch_sub(1, std::memory_order_relaxed);
         // push invalidations to watcher
         InvalQueue *ivq = &inval_queue_array[id16.index];
         ivq->mutex.lock();
         {
            char *ptr = (char *) &inodeId;
            size_t size = sizeof inodeId;
            if (ivq->synced && ! ivq->queue_overflow)
            {
               if (ivq->queue.size() + size >
                     invalwatch_max_queue_bytes.load(std::memory_order_relaxed) )
               {
                  ivq->queue_overflow = true;
               }
               else
               {
                  ivq->queue.insert(ivq->queue.end(), ptr, ptr + size);
                  ivq->write_pos += size;
                  ivq->writecond.notify_one();
               }
            }
         }
         ivq->mutex.unlock();
      }
      delete entry;
      entry = next;
   }
}

// Get target for inodeId from the target cache.
// This may:
//  - increase the reference count if the target was previously referenced.
//  - resurrect a previously referenced target (refcount == 0) from the LRU cache.
//  - cause the LRU cache to drop an unrelated unreferenced node entirely.
//  - allocate a new target if it wasn't in the cache.
InvalTarget *get_invaltarget(InodeId const& inodeId)
{
   InvalTargetStore *store = &global_invaltarget_store;
   InvalTarget *target = NULL;
   store->mutex.lock();
   auto it = store->targets.find(inodeId);
   if (it == store->targets.end())
   {
      // The target was not found in the cache. Allocate a new InvalTarget.
      uint64_t maxCachedObjects = invalwatch_max_cached_objects.load(std::memory_order_relaxed);
      if (store->targets.size() < maxCachedObjects)
      {
         target = new InvalTarget;
      }
      else
      {
         InvalTargetEvictionPool *evictionPool = &store->evictionPool;
         if (evictionPool->lruFirst)
         {
            // Replace an unreferenced target
            InvalTarget *dropTarget = evictionPool->lruFirst;
            InvalTarget *dropNext = dropTarget->lruNext;
            evictionPool->lruFirst = dropNext;
            if (dropNext)
               dropNext->lruPrev = NULL;
            else
               evictionPool->lruLast = NULL;
            dropTarget->lruNext = NULL;

            invalidate_target(dropTarget);
            store->targets.erase(dropTarget->inodeId);
            inval_watch_cache_stats.currentTargetCount.fetch_sub(1, std::memory_order_relaxed);

            // Recycle the target. Carefully re-initialize it.
            // I'd like to do *target = {} but due to the atomic uint32_t in it, seems that assignment operator is deleted.
            // We could instead delete the object and 'new' another one. But I want to avoid allocations.
            // Instead I'm calling the destructor and the constructor again, in-place.
            // This might be walking on thin ice because I don't fully understand the language semantics around this.
            dropTarget->~InvalTarget();
            target = new (dropTarget) InvalTarget();
            assert(target->fanout == NULL);
            assert(target->lruPrev == NULL);
            assert(target->lruNext == NULL);
         }
         else
         {
            // can't get a target. target stays NULL
            // should we try waiting just a little bit?
            // The wait time will serialize on an individual request context (i.e. client node)
            // so let's not try that for now.
            // TODO: log?
         }
      }

      if (target)
      {
         target->inodeId = inodeId;
         store->targets[inodeId] = target;  // insert
         inval_watch_cache_stats.currentTargetCount.fetch_add(1, std::memory_order_relaxed);
      }
   }
   else
   {
      // Target found in the cache.
      target = it->second;
      if (target->refcount.fetch_add(1, std::memory_order_acq_rel) == 0)  // NOTE: can't use memory_order_relaxed, need precise old value
      {
         // Refcount was 0. Resurrect target from LRU cache (remove from list)
         InvalTargetEvictionPool *evictionPool = &store->evictionPool;
         {
            if (target->lruPrev)
               target->lruPrev->lruNext = target->lruNext;
            else
            {
               assert(evictionPool->lruFirst == target);
               evictionPool->lruFirst = target->lruNext;
            }
            if (target->lruNext)
               target->lruNext->lruPrev = target->lruPrev;
            else
            {
               assert(evictionPool->lruLast == target);
               evictionPool->lruLast = target->lruPrev;
            }
            target->lruPrev = NULL;
            target->lruNext = NULL;
         }
      }
   }
   store->mutex.unlock();
   return target;
}

void put_invaltarget(InvalTarget *target)
{
   if (! target)
      return;
   InvalTargetStore *store = &global_invaltarget_store;
   InvalTargetEvictionPool *evictionPool = &store->evictionPool;
   store->mutex.lock();
   if (target->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
   {
      // Target unreferenced. Put it in LRU cache (link to list). Later it will
      // be either resurrected or dropped from the cache.
      target->lruPrev = evictionPool->lruLast;
      target->lruNext = NULL;
      if (evictionPool->lruLast)
         evictionPool->lruLast->lruNext = target;
      else
         evictionPool->lruFirst = target;
      evictionPool->lruLast = target;
   }
   store->mutex.unlock();
}

void add_target_watch(InvalTarget *target, Watcher *watcher)
{
   bool oom = false;
   target->mutex.lock();
   InvalFanout *fanout = target->fanout;
   if (!fanout)
   {
      fanout = new (std::nothrow) InvalFanout;
      if (fanout)
      {
         fanout->inodeId = target->inodeId;
         target->fanout = fanout;
      }
      else
         oom = true;
   }
   if (!oom)
   {
      try { fanout->watchers.push_back(watcher->id16); }
      catch (std::bad_alloc&) { oom = true; }
   }
   target->mutex.unlock();

   if (oom)
   {
      // OOM: the watcher was not registered and will not receive future
      // invalidations for this target. Force a resync so it doesn't cache
      // stale data indefinitely.
      //
      // Also trigger invalidation to free the fanout memory, which may slightly
      // relieve memory pressure. It is unclear whether this is actually
      // helpful in practice.
      force_watcher_resync(watcher);
      invalidate_target(target);
      return;
   }

   // Update per-watcher monitoring counter (outside target lock)
   watcher_stats[watcher->id16.index].watchedTargetCount.fetch_add(1, std::memory_order_relaxed);

   inval_watch_cache_stats.currentSubscriptionCount.fetch_add(1, std::memory_order_relaxed);
}

void invalidate_target(InvalTarget *target)
{
   InvalFanoutQueue *fq = &inval_fanout_queue;
   InvalFanout *fanout;
   bool drain;

   // Detach the fanout from the target and push it to the global fanout queue.
   // Note: concurrent accesses to target->fanout protected by target->mutex.

   target->mutex.lock();
   fanout = target->fanout;
   target->fanout = NULL;
   target->mutex.unlock();

   if (!fanout || fanout->watchers.empty())
   {
      delete fanout;
      return;  // no watcher needs updating
   }

   fq->queue_mutex.lock();
   drain = (fq->head == NULL);
   fanout->next = NULL;
   if (fq->tail)
      fq->tail->next = fanout;
   else
      fq->head = fanout;
   fq->tail = fanout;
   fq->queue_mutex.unlock();
   if (drain)
      just_enqueue_work(fanout_invals, NULL);

   inval_watch_cache_stats.totalInvalidationsCount.fetch_add(1, std::memory_order_relaxed);
}

// Called with watcher->invalidationReadMutex and ivq->mutex both locked.
// result->end_pos is initialised to request->read_pos (no progress) and
// updated to the final cursor position after actual reading.
static void read_locked(
      InvalidationReadRequest const *request,
      InvalQueue *ivq,
      std::chrono::time_point<std::chrono::steady_clock> deadlineNonEmpty,
      InvalidationReadResult *result) noexcept
{
   uint32_t read_pos = request->read_pos;
   result->end_pos = read_pos;

   // Handle watcher resync. The watcher sets resync=true on its first request
   // and after receiving ReadInvalidations_ResyncNeeded.
   // Meta always honours the request: reset the invalidation queue and
   // mark the watcher as synced. The watcher sends readPos=0 on resync,
   // and after reset both ivq->read_pos and ivq->write_pos are 0,
   // so the validity check below passes naturally.
   if (request->resync)
   {
      ivq->reset();
      ivq->synced = true;
   }
   else if (!ivq->synced)
   {
      // Watcher has not yet synced (most likely a meta restart event:
      // the Watcher was newly created via handshake but has not yet sent
      // resync=true). Tell it to resync.
      //
      // We do NOT clean up cached watchset entries that may still
      // reference this watcher's Id16 here; stale watchset entries will
      // naturally be discarded on the next invalidate_target()
      // for the affected inodes.
      result->resync_needed = true;
      return;
   }

   if (!IvqValidDataRange_contains({ivq->read_pos, ivq->write_pos}, read_pos))
   {
      result->invalid_read_pos = true;
      return;
   }

   if (ivq->queue_overflow)
   {
      result->queue_overflow = true;
      return;
   }

   // discard ack'ed bytes
   size_t ndiscard = read_pos - ivq->read_pos;
   ivq->queue.erase(ivq->queue.begin(), ivq->queue.begin() + ndiscard);
   ivq->read_pos = read_pos;

   // wait for the requested time for the invalidation queue to become non-empty.
   while (ivq->write_pos == read_pos)
   {
      if (global_shutdown_flag.load(std::memory_order_relaxed))
      {
         result->unavailable = true;
         return;
      }
      if (condvar_wait_until(&ivq->mutex, &ivq->writecond, deadlineNonEmpty)
            == std::cv_status::timeout)
      {
         break;
      }
      if (!ivq->synced)  // check for asynchronous watcher reset (e.g. OOM)
      {
         result->resync_needed = true;
         return;
      }
   }

   // Now the actual reading. To avoid the overhead of reading too small
   // portions, we employ a second deadline, which is supposed to be a much
   // smaller duration.

   std::chrono::time_point deadlineFillBuffer = std::chrono::steady_clock::now()
      + std::chrono::microseconds(request->waitMicrosecondsFillBuffer);

   // read invalidation entries from the queue into the output buffer
   char *buffer = (char *) request->buffer;
   uint32_t bytes_read = 0;
   uint32_t bytes_to_read = request->bufferSize - (request->bufferSize % sizeof(InodeId));  // read whole InodeIds only
   while (bytes_to_read)
   {
      if (ivq->write_pos == read_pos)
      {
         if (condvar_wait_until(&ivq->mutex, &ivq->writecond, deadlineFillBuffer)
               == std::cv_status::timeout)
         {
            break;
         }
         if (!ivq->synced)  // check for asynchronous watcher reset (e.g. OOM)
         {
            result->resync_needed = true;
            return;
         }
      }
      uint32_t bytes_avail = ivq->write_pos - read_pos;
      uint32_t n = bytes_avail;
      if (n > bytes_to_read)
      {
         n = bytes_to_read;
         result->buffer_full = true;
      }
      auto cursor = ivq->queue.begin() + (read_pos - ivq->read_pos);
      auto end_cursor = cursor + n;
      std::copy(cursor, end_cursor, buffer + bytes_read);
      bytes_read += n;
      bytes_to_read -= n;
      read_pos += n;
   }

   result->bytes_read = bytes_read;
   result->end_pos = read_pos;
}

static void read_invalidations_internal(
      InvalidationReadRequest const *request,
      InvalidationReadResult *out_result)
{
   Watcher *watcher = request->watcher;
   InvalQueue *ivq = &inval_queue_array[watcher->id16.index];
   InvalidationReadResult result = {};

   if (!invalwatch_enabled.load(std::memory_order_relaxed))
   {
      result.unavailable = true;
   }
   else
   {
      auto deadlineNonEmpty = std::chrono::steady_clock::now()
         + std::chrono::microseconds(request->waitMicrosecondsNonEmpty);

      // Take per-watcher read lock. This lock prevents concurrent accesses to
      // this watcher's queue when processing other requests. Note, the remote
      // client should never willingly do multiple simultaneous read requests,
      // but we don't trust that here. Also, even with a conforming client,
      // multiple concurrent read requests can happen when there was a
      // connection breakage and the client re-attempted the request.
      watcher->invalidationReadMutex.lock();

      // Take lock on invalidation queue itself. This lock prevents concurrent
      // changes to the invalidation queue by writers (as part of
      // invalidate_target()).
      // Note, the lock will be temporarily released when waiting on the
      // condvar below.
      ivq->mutex.lock();
      read_locked(request, ivq, deadlineNonEmpty, &result);
      ivq->mutex.unlock();
      watcher->invalidationReadMutex.unlock();
   }

   if (result.queue_overflow)
      watcher_logmsgf(watcher, Log_WARNING, "InvalQueue overflow");

   *out_result = result;
}

void read_invalidations(InvalidationReadRequest const *request,
      InvalidationReadResult *out_result)
{
   InvalidationReadResult result = {};

   if (! Program::getApp()->getConfig()->getSysRemoteInvalEnabled())
   {
      result.disabled = true;
   }
   else if (request->watcher == NULL)
   {
      // Handle NULL watcher. We should not accept a NULL watcher ideally
      // but due to code organization we have to enable it here (after invalwatch_enabled test).
      // Two possibilities:
      //  - InvalWatch disabled by config. That currently caused Handshake to fail, resulting in NULL watcher
      //    => send "unavailable" for now, just like we do when there is concurrent shutdown.
      //  - watcher did not actually attempt handshake.
      //    => We can assume it doesn't happen in practice, but proper handling
      //    would be to send some "invalid input" error.
      result.unavailable = true;
   }
   else
   {
      read_invalidations_internal(request, &result);
   }
   *out_result = result;
}

// helper doing read_inodeid(), get_invaltarget().
InvalTarget *get_invaltarget_by_entryid(std::string const& entryID)
{
   InvalTarget *target = NULL;
   InodeId inodeId;
   if (read_inodeid(entryID, &inodeId))
   {
      target = get_invaltarget(inodeId);
   }
   return target;
}

bool add_target_watch_by_entryid(std::string const& entryID, Watcher *watcher)
{
   bool ret = false;
   InvalTarget *target = get_invaltarget_by_entryid(entryID);
   if (target)
   {
      add_target_watch(target, watcher);
      ret = true;
   }
   put_invaltarget(target);
   return ret;
}

bool invalidate_target_by_entryid(std::string const& entryID)
{
   if (!invalwatch_enabled.load(std::memory_order_relaxed))  // check required?
   {
      // not sure if true or false should be returned. Probably calling this
      // function should better be "undefined" if !invalwatch_enabled
      return true;
   }
   bool ret = false;
   InvalTarget *target = get_invaltarget_by_entryid(entryID);
   if (target)
   {
      // This function currently gets called where a FileInode or DirInode
      // is in the locked state. So we do not need to explicitly lock the InvalTarget object.
      invalidate_target(target);
      ret = true;
   }
   put_invaltarget(target);
   return ret;
}

void get_invalwatch_mon_data(InvalWatchStat* out)
{
   InvalWatchStat iws;
   for (uint32_t i = 0; i < (1 << 16); i++)
   {
      uint32_t sessionId = watcher_stats[i].sessionId.load(std::memory_order_relaxed);
      uint32_t watched = watcher_stats[i].watchedTargetCount.load(std::memory_order_relaxed);
      uint64_t invalidations = watcher_stats[i].numInvalidations.load(std::memory_order_relaxed);
      if (watched > 0 || invalidations > 0)
         iws.perWatcherData.push_back({sessionId, watched, invalidations});
   }
   iws.numWatchers = inval_watch_cache_stats.currentWatcherCount.load(std::memory_order_relaxed);
   iws.numTargets = inval_watch_cache_stats.currentTargetCount.load(std::memory_order_relaxed);
   *out = std::move(iws);
}

void initialize_invalwatch(uint64_t maxCachedObjects, uint32_t maxQueueBytes)
{
   initialize_watcher_store();
   invalwatch_max_cached_objects = maxCachedObjects;
   if (maxQueueBytes)
      invalwatch_max_queue_bytes = maxQueueBytes;
   invalwatch_enabled.store(true);
}

void uninitialize_invalwatch()
{
   global_shutdown_flag.store(true);
   shut_down_watcher_store();
}
