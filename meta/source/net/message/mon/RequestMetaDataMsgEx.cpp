#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/toolkit/StorageTk.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include <components/InvalWatch.h>
#include "RequestMetaDataMsgEx.h"

bool RequestMetaDataMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RequestMetaDataMsg incoming");

   App *app = Program::getApp();
   Node& node = app->getLocalNode();
   MultiWorkQueue *workQueue = app->getWorkQueue();

   unsigned sessionCount = app->getSessions()->getSize() + app->getMirroredSessions()->getSize();

   NicAddressList nicList(node.getNicList());
   std::string hostnameid = System::getHostname();

   // highresStats
   HighResStatsList statsHistory;
   uint64_t lastStatsMS = getValue();

   // get stats history
   StatsCollector* statsCollector = app->getStatsCollector();
   statsCollector->getStatsSince(lastStatsMS, statsHistory);

   // InvalWatch monitoring stats
   InvalWatchStat invalWatchStat;
   get_invalwatch_mon_data(&invalWatchStat);

   // build per-node capacity info (meta uses nodeNumID as targetID, same as publishNodeCapacity())
   std::string metaPath = app->getMetaPath();
   int64_t sizeTotal = 0, sizeFree = 0, inodesTotal = 0, inodesFree = 0;

   if (!StorageTk::statStoragePath(metaPath, &sizeTotal, &sizeFree, &inodesTotal, &inodesFree))
   {
      log.logErr("Unable to statfs() storage path: " + metaPath +
         " (SysErr: " + System::getErrString() + ")");
      sizeTotal = sizeFree = inodesTotal = inodesFree = -1;
   }
   StorageTk::statStoragePathOverride(metaPath, &sizeFree, &inodesFree);

   TargetConsistencyState consistencyState =
      app->getInternodeSyncer()->getNodeConsistencyState();

   StorageTargetInfo metaTargetInfo(node.getNumID().val(), metaPath,
      sizeTotal, sizeFree, inodesTotal, inodesFree, consistencyState);
   StorageTargetInfoList metaTargetInfoList(1, metaTargetInfo);

   RequestMetaDataRespMsg requestMetaDataRespMsg(node.getAlias(), hostnameid, node.getNumID(),
      &nicList, app->getMetaRoot().getID() == node.getNumID(),
      workQueue->getIndirectWorkListSize(), workQueue->getDirectWorkListSize(),
      sessionCount, &statsHistory, &metaTargetInfoList);
   requestMetaDataRespMsg.setInvalWatchStat(std::move(invalWatchStat));
   ctx.sendResponse(requestMetaDataRespMsg);

   LOG_DEBUG_CONTEXT(log, 5, std::string("Sent a message with type: " ) +
      StringTk::uintToStr(requestMetaDataRespMsg.getMsgType() ) + std::string(" to mon") );

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_REQUESTMETADATA,
      getMsgHeaderUserID() );

   return true;
}
