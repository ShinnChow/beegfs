#pragma once


#include <common/Common.h>
#include <common/toolkit/MapTk.h>
#include "InvalidConfigException.h"
#include "ConnAuthFileException.h"
#include "CommonConfig.h"

class AbstractConfig : public CommonConfig
{
   public:
      virtual ~AbstractConfig() {}

   protected:
      // internals

      StringMap configMap;
      int argc;
      char** argv;

      void addLineToConfigMap(std::string line);
      void loadFromArgs(int argc, char** argv);

      // configurables

      AbstractConfig(int argc, char** argv);

      void initConfig(int argc, char** argv, bool enableException, bool addDashes);

      virtual void loadDefaults(bool addDashes);
      void loadFromFile(const char* filename, bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes);
      virtual void initImplicitVals();

      void initInterfacesList(const std::string& connInterfacesFile,
                              std::string& inoutConnInterfacesList);
      void initConnAuthHash(const std::string& connAuthFile, uint64_t* outConnAuthHash);
      void initSocketBufferSizes();

      // getters & setters
      const StringMap* getConfigMap() const
      {
         return &configMap;
      }

      int getArgc() const
      {
         return argc;
      }

      char** getArgv() const
      {
         return argv;
      }
};

NetFilter loadNetworkList(const std::string& file);
