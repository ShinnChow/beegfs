#include <common/nodes/TargetCapacityPools.h>
#include <common/system/System.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/ArraySlice.h>
#include <common/toolkit/ArrayTypeTraits.h>
#include "Config.h"

#include <../generated/MetaConfigFields.inc>  // bake in data definitions

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-meta.conf"

#define TARGETCHOOSERTYPE_RANDOMIZED_STR        "randomized"
#define TARGETCHOOSERTYPE_ROUNDROBIN_STR        "roundrobin"
#define TARGETCHOOSERTYPE_RANDOMROBIN_STR       "randomrobin"
#define TARGETCHOOSERTYPE_RANDOMINTERNODE_STR   "randominternode"
#define TARGETCHOOSERTYPE_RANDOMINTRANODE_STR   "randomintranode"


Config::Config(int argc, char** argv):
      AbstractConfig(argc, argv)
{
   sysTargetAttachmentMap = NULL;

   initConfig(argc, argv, true, false);
}

Config::~Config()
{
   SAFE_DELETE(sysTargetAttachmentMap);
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes currently unused
 */
void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults(addDashes);

   commonLoadConfigDefaults(&this->configMap, ArraySlice(MetaConfigFields_infos), addDashes);
}

/**
 * @param addDashes currently usused
 */
void Config::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false, addDashes);

   commonApplyConfigMap(static_cast<MetaConfigFields *>(this), &this->configMap, ArraySlice(MetaConfigFields_infos).asConst(), enableException, addDashes);

   // Additional sanity checks after the parsing.

   if (sysTargetOfflineTimeoutSecs < 30)
   {
      throw InvalidConfigException("Invalid sysTargetOfflineTimeoutSecs value "
            + std::to_string(sysTargetOfflineTimeoutSecs) + " (must be at least 30)");
   }
}

void Config::initImplicitVals()
{
   // tuneNumWorkers (note: twice the number of cpu cores is default, but at least 4)
   if(!tuneNumWorkers)
      tuneNumWorkers = BEEGFS_MAX(System::getNumOnlineCPUs()*2, 4);

   // tuneNumCommSlaves
   if(!tuneNumCommSlaves)
      tuneNumCommSlaves = tuneNumWorkers * 2;

   // tuneTargetChooserNum
   initTuneTargetChooserNum();

   // connInterfacesList(/File)
   AbstractConfig::initInterfacesList(connInterfacesFile, connInterfacesList);

   AbstractConfig::initSocketBufferSizes();

   // connAuthHash
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);

   // sysTargetAttachmentMap
   initSysTargetAttachmentMap();
}

void Config::initSysTargetAttachmentMap()
{
   if(sysTargetAttachmentFile.empty() )
      return; // no file given => nothing to do here

   // check if file exists

   if(!StorageTk::pathExists(sysTargetAttachmentFile) )
      throw InvalidConfigException("sysTargetAttachmentFile not found: " +
         sysTargetAttachmentFile);

   // load as string map

   StringMap attachmentStrMap;

   MapTk::loadStringMapFromFile(sysTargetAttachmentFile.c_str(), &attachmentStrMap);

   // convert from string map to target map

   sysTargetAttachmentMap = new TargetMap();

   for(StringMapCIter iter = attachmentStrMap.begin(); iter != attachmentStrMap.end(); iter++)
   {
      (*sysTargetAttachmentMap)[StringTk::strToUInt(iter->first)] =
         NumNodeID(StringTk::strToUInt(iter->second) );
   }
}

void Config::initTuneTargetChooserNum()
{
   if (this->tuneTargetChooser == TARGETCHOOSERTYPE_RANDOMIZED_STR)
      this->tuneTargetChooserNum = TargetChooserType_RANDOMIZED;
   else if (this->tuneTargetChooser == TARGETCHOOSERTYPE_ROUNDROBIN_STR)
      this->tuneTargetChooserNum = TargetChooserType_ROUNDROBIN;
   else if (this->tuneTargetChooser == TARGETCHOOSERTYPE_RANDOMROBIN_STR)
      this->tuneTargetChooserNum = TargetChooserType_RANDOMROBIN;
   else if (this->tuneTargetChooser == TARGETCHOOSERTYPE_RANDOMINTERNODE_STR)
      this->tuneTargetChooserNum = TargetChooserType_RANDOMINTERNODE;
   // Don't allow RANDOMINTRANODE Target Chooser
   else
   {
      // invalid chooser specified
      throw InvalidConfigException("Invalid storage target chooser specified: " 
            + tuneTargetChooser);
   }
}
