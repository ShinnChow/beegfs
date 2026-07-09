#include "RequestMetaDataWork.h"

#include <common/toolkit/MessagingTk.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/mon/RequestMetaDataMsg.h>
#include <common/net/message/mon/RequestMetaDataRespMsg.h>
#include <components/StatsCollector.h>

void RequestMetaDataWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{

   if (!node->getIsResponding())
   {
      HeartbeatRequestMsg heartbeatRequestMsg;
      if(MessagingTk::requestResponse(*node, heartbeatRequestMsg,
            NETMSGTYPE_Heartbeat))
      {
         LOG(GENERAL, DEBUG, "Node is responding again.",
               ("NodeID", node->getNodeIDWithTypeStr()));
         node->setIsResponding(true);
      }
   }

   Result result = {};
   result.data.isResponding = false;

   if (node->getIsResponding())
   {
      // generate the RequestDataMsg with the lastStatsTime
      RequestMetaDataMsg requestDataMsg(node->getLastStatRequestTime().count());
      auto respMsg = MessagingTk::requestResponse(*node, requestDataMsg,
            NETMSGTYPE_RequestMetaDataResp);

      if (!respMsg)
      {
         LOG(GENERAL, DEBUG, "Node is not responding.", ("NodeID", node->getNodeIDWithTypeStr()));
         node->setIsResponding(false);
      }
      else
      {
         // get response and process it
         auto metaRspMsg = static_cast<RequestMetaDataRespMsg*>(respMsg.get());
         result.highResStatsList = std::move(metaRspMsg->getStatsList());

         if (metaRspMsg->isMsgHeaderCompatFeatureFlagSet(REQUESTMETADATARESP_HAS_META_TARGETS))
            result.metaTargetList = std::move(metaRspMsg->getMetaTargets());

         result.data.isResponding = true;
         result.data.indirectWorkListSize = metaRspMsg->getIndirectWorkListSize();
         result.data.directWorkListSize = metaRspMsg->getDirectWorkListSize();
         result.data.sessionCount = metaRspMsg->getSessionCount();
         result.data.hostnameid = metaRspMsg->gethostnameid();

         if (metaRspMsg->isMsgHeaderCompatFeatureFlagSet(REQUESTMETADATARESP_HAS_CACHE_STATS))
         {
            result.data.invalWatchStat = metaRspMsg->getInvalWatchData();
         }

         if (!result.highResStatsList.empty())
         {
            auto lastStatsRequestTime = std::chrono::milliseconds(
                  result.highResStatsList.front().rawVals.statsTimeMS);
            node->setLastStatRequestTime(lastStatsRequestTime);
         }

         bool useClientStatsV2 = metaRspMsg->isMsgHeaderFeatureFlagSet(RequestMetaDataRespMsg::MsgFlags::USE_CLIENT_STATS_V2);

         if (collectClientOpsByNode)
            result.ipOpsUnorderedMap = ClientOpsRequestor::request(*node, false, useClientStatsV2);

         if (collectClientOpsByUser)
            result.userOpsUnorderedMap = ClientOpsRequestor::request(*node, true, useClientStatsV2);
      }
   }

   result.node = std::move(node);

   statsCollector->insertMetaData(std::move(result));
}
