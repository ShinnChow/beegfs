#pragma once

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageTargetInfo.h>

// Compat feature flag: set by meta in the response to indicate metaTargets list is present.
// Receiver (mon) must check this flag before deserializing metaTargets.
#define REQUESTMETADATARESP_HAS_META_TARGETS   1

// Compat feature flag: set by meta when the response carries the trailing invalWatchStat
// (inode cache invalidation stats) block. Compat (not hard) so older beegfs-mon skips
// unknown trailing bytes instead of rejecting. Receiver (mon) must check this flag before
// deserializing invalWatchStat.
#define REQUESTMETADATARESP_HAS_CACHE_STATS    2

/**
 * Per-watcher invalidation monitoring data.
 */
struct InvalWatcherStat
{
   uint32_t clientId{0};
   uint32_t watchedTargetCount{0};
   uint64_t numInvalidations{0};

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->clientId
         % obj->watchedTargetCount
         % obj->numInvalidations;
   }
};

/**
 * All invalidation monitoring data collected from a single meta server.
 * Contains both server-wide aggregate fields and per-watcher details.
 */
struct InvalWatchStat
{
   // Server-wide aggregate stats
   uint32_t numWatchers{0};            // registered invalidation subscribers
   uint32_t numTargets{0};             // targets tracked in the cache

   // Per-watcher breakdown
   std::vector<InvalWatcherStat> perWatcherData;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->numWatchers
         % obj->numTargets
         % obj->perWatcherData;
   }
};


class RequestMetaDataRespMsg : public NetMessageSerdes<RequestMetaDataRespMsg>
{
   public:
      struct MsgFlags {
         static const unsigned USE_CLIENT_STATS_V2 = 1;
      };

      /**
       * @param hostnameid it will get the hostname of server
       * @param nicList just a reference, so do not free it as long as you use this object!
       * @param statsList just a reference, so do not free it as long as you use this object!
       * @param metaTargets just a reference, so do not free it as long as you use this object!
       */
      RequestMetaDataRespMsg(const std::string& nodeID, const std::string& hostnameid,
         NumNodeID nodeNumID, NicAddressList *nicList,
         bool isRoot, unsigned IndirectWorkListSize, unsigned DirectWorkListSize,
         unsigned sessionCount, HighResStatsList* statsList,
         StorageTargetInfoList* metaTargets)
         : BaseType(NETMSGTYPE_RequestMetaDataResp)
      {
         this->nodeID = nodeID;
         this->hostnameid = hostnameid;
         this->nodeNumID = nodeNumID;
         this->nicList = nicList;
         this->isRoot = isRoot;
         this->indirectWorkListSize = IndirectWorkListSize;
         this->directWorkListSize = DirectWorkListSize;
         this->sessionCount = sessionCount;
         this->statsList = statsList;
         this->metaTargets = metaTargets;

         addMsgHeaderFeatureFlag(MsgFlags::USE_CLIENT_STATS_V2);
         addMsgHeaderCompatFeatureFlag(REQUESTMETADATARESP_HAS_META_TARGETS);
      }

      RequestMetaDataRespMsg() : BaseType(NETMSGTYPE_RequestMetaDataResp)
      {
         this->nicList = NULL;
         this->isRoot = 0;
         this->indirectWorkListSize = 0;
         this->directWorkListSize = 0;
         this->sessionCount = 0;
         this->metaTargets = NULL;

         addMsgHeaderFeatureFlag(MsgFlags::USE_CLIENT_STATS_V2);
      }

      /**
       * Set invalidation monitoring data and enable the REQUESTMETADATARESP_HAS_CACHE_STATS
       * compat feature flag.
       */
      void setInvalWatchStat(InvalWatchStat&& data)
      {
         this->invalWatchStat = std::move(data);
         addMsgHeaderCompatFeatureFlag(REQUESTMETADATARESP_HAS_CACHE_STATS);
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeID
            % obj->hostnameid
            % obj->nodeNumID
            % serdesNicAddressList(obj->nicList, obj->parsed.nicList)
            % obj->isRoot
            % obj->indirectWorkListSize
            % obj->directWorkListSize
            % obj->sessionCount
            % serdes::backedPtr(obj->statsList, obj->parsed.statsList);

         if (obj->isMsgHeaderCompatFeatureFlagSet(REQUESTMETADATARESP_HAS_META_TARGETS))
         {
            ctx % serdes::backedPtr(obj->metaTargets, obj->parsed.metaTargets);
         }

         if (obj->isMsgHeaderCompatFeatureFlagSet(REQUESTMETADATARESP_HAS_CACHE_STATS))
         {
            ctx % obj->invalWatchStat;
         }
      }

      virtual unsigned getSupportedHeaderFeatureFlagsMask() const {
         return MsgFlags::USE_CLIENT_STATS_V2;
      }

   private:
      std::string nodeID;
      std::string hostnameid;
      NumNodeID nodeNumID;
      bool isRoot;
      uint32_t indirectWorkListSize;
      uint32_t directWorkListSize;
      uint32_t sessionCount;
      NicAddressList* nicList;

      InvalWatchStat invalWatchStat;

      // for serialization
      HighResStatsList* statsList;    // not owned by this object!
      StorageTargetInfoList* metaTargets; // not owned by this object!

      // for deserialization
      struct {
         HighResStatsList statsList;
         NicAddressList nicList;
         StorageTargetInfoList metaTargets;
      } parsed;

   public:
      NicAddressList& getNicList()
      {
         return *nicList;
      }

      HighResStatsList& getStatsList()
      {
         return *statsList;
      }

      StorageTargetInfoList& getMetaTargets()
      {
         return *metaTargets;
      }

      const std::string& getNodeID() const
      {
         return nodeID;
      }

      const std::string& gethostnameid() const
      {
         return hostnameid;
      }

      NumNodeID getNodeNumID() const
      {
         return nodeNumID;
      }

      NicAddressList* getNicList() const
      {
         return this->nicList;
      }

      bool getIsRoot() const
      {
         return isRoot;
      }

      unsigned getIndirectWorkListSize() const
      {
         return indirectWorkListSize;
      }

      unsigned getDirectWorkListSize() const
      {
         return directWorkListSize;
      }

      unsigned getSessionCount() const
      {
         return sessionCount;
      }

      const InvalWatchStat& getInvalWatchData() const
      {
         return invalWatchStat;
      }
};
