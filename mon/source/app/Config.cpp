#include <common/toolkit/StringTk.h>
#include "Config.h"

#include <../generated/MonConfigFields.inc>  // bake in data definitions

#include <sys/stat.h>

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-mon.conf"

Config::Config(int argc, char** argv): AbstractConfig(argc, argv)
{
   initConfig(argc, argv, true, false);

   // check mandatory value
   if(getSysMgmtdHost().empty())
      throw InvalidConfigException("Management host undefined.");

   // convert string to custom enum
   {
      if (this->dbType == "influxdb")
         this->dbTypeEnum = DbTypes::INFLUXDB;
      else if (this->dbType == "influxdb2")
         this->dbTypeEnum = DbTypes::INFLUXDB2;
      else if (this->dbType == "cassandra")
         this->dbTypeEnum = DbTypes::CASSANDRA;
      else
         throw InvalidConfigException("The value of config argument dbType is invalid:"
               " Must be influxdb, influxdb2, or cassandra. Got: '" + dbType + "'");
   }

   // Load auth config file
   if (!dbAuthFile.empty())
   {
      std::ifstream authConfig(dbAuthFile);

      if (!authConfig.good())
         throw InvalidConfigException("Could not open InfluxDB authentication file");

      StringMap authMap;
      MapTk::loadStringMapFromFile(dbAuthFile.c_str(), &authMap);

      for (const auto& e : authMap) {
         if (e.first == "password") {
            dbAuthPassword = e.second;
         } else if (e.first == "username") {
            dbAuthUsername = e.second;
         } else if (e.first == "organization") {
            dbAuthOrg = e.second;
         } else if (e.first == "token") {
            dbAuthToken = e.second;
         } else {
            throw InvalidConfigException("The InfluxDB authentication file may only contain "
                  "the options username and password for influxdb version 1.x "
		            "organization and token for influxdb version 2.x" );
         }
      }
   }
}

void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults(addDashes);

   commonLoadConfigDefaults(&this->configMap, ArraySlice(MonConfigFields_infos), addDashes);
}

void Config::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false, addDashes);

   commonApplyConfigMap(static_cast<MonConfigFields *>(this), &this->configMap, ArraySlice(MonConfigFields_infos).asConst(), enableException, addDashes);
}

void Config::initImplicitVals()
{
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);
}
