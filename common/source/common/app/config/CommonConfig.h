#pragma once

#include <beegfs-base/CommonConfig.common.h>
#include <common/toolkit/ArraySlice.h>
#include <../generated/CommonConfigFields.h>

class CommonConfig : public CommonConfigFields
{
   public:
      virtual ~CommonConfig() {}

      static void loadStringListFile(const char* filename, StringList& outList);

   protected:
      CommonConfig() {}

      // Doesn't seem like these are set by the config file at all
      int         connectionRejectionRate;
      int         connectionRejectionCount;

      uint64_t    connAuthHash; // implicitly set based on hash of connAuthFile contents

   public:
      int getConnClientPort() const
      {
         return connClientPort ? (connClientPort + connPortShift) : 0;
      }

      int getConnStoragePort() const
      {
         return connStoragePort ? (connStoragePort + connPortShift) : 0;
      }

      int getConnMetaPort() const
      {
         return connMetaPort ? (connMetaPort + connPortShift) : 0;
      }

      int getConnMonPort() const
      {
         return connMonPort ? (connMonPort + connPortShift) : 0;
      }

      int getConnMgmtdPort() const
      {
         return connMgmtdPort ? (connMgmtdPort + connPortShift) : 0;
      }

      int getConnMsgLongTimeout() const
      {
         return connMessagingTimeouts.at(0);
      }

      int getConnMsgMediumTimeout() const
      {
         return connMessagingTimeouts.at(1);
      }

      int getConnMsgShortTimeout() const
      {
         return connMessagingTimeouts.at(2);
      }

      int getConnRDMATimeoutConnect() const
      {
        return connRDMATimeouts.at(0);
      }

      int getConnRDMATimeoutFlowSend() const
      {
        return connRDMATimeouts.at(1);
      }

      int getConnRDMATimeoutPoll() const
      {
        return connRDMATimeouts.at(2);
      }

      uint64_t getConnAuthHash() const
      {
         return connAuthHash;
      }

      unsigned getConnectionRejectionRate() const
      {
         return connectionRejectionRate;
      }

      void setConnectionRejectionRate(unsigned rate)
      {
         connectionRejectionRate = rate;
         connectionRejectionCount = 0;
      }
};

bool testConfigMapKeyMatch(const std::string& name, const std::string& testKey, bool addDashes);
void commonLoadConfigDefaults(StringMap *configMap, ArraySlice<const CfgInfo> cfgInfos, bool addDashes);
void configMapRedefine(StringMap *configMap, std::string keyStr, std::string valueStr, bool addDashes);
void commonApplyConfigMap(void *containerStruct, StringMap *configMap, ArraySlice<const CfgInfo> cfgInfo, bool enableException, bool addDashes);
