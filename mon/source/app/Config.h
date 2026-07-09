#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>
#include <../generated/MonConfigFields.h>

class Config : public AbstractConfig, public MonConfigFields
{
   public:
      Config(int argc, char** argv);

      enum DbTypes
      {
         INFLUXDB,
         INFLUXDB2,
         CASSANDRA
      };

   private:

      DbTypes dbTypeEnum = INFLUXDB;

      // seems like those have never been set so far
      std::string dbAuthUsername;
      std::string dbAuthPassword;
      std::string dbAuthOrg;
      std::string dbAuthToken;

   public:
      const std::string& getDbAuthUsername() const
      {
         return dbAuthUsername;
      }

      const std::string& getDbAuthPassword() const
      {
         return dbAuthPassword;
      }

      const std::string& getDbAuthOrg() const
      {
         return dbAuthOrg;
      }

      const std::string& getDbAuthToken() const
      {
         return dbAuthToken;
      }

   private:

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;

   public:

      // override
      DbTypes getDbType() const
      {
         return dbTypeEnum;
      }

      std::chrono::milliseconds getHttpTimeout() const
      {
         return std::chrono::milliseconds(httpTimeoutMSecs);
      }

      std::chrono::seconds getNodelistRequestInterval() const
      {
         return std::chrono::seconds(nodelistRequestIntervalSecs);
      }

      std::chrono::seconds getStatsRequestInterval() const
      {
         return std::chrono::seconds(statsRequestIntervalSecs);
      }
};

#endif /*CONFIG_H_*/
