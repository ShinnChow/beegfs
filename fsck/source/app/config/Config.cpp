#include <common/toolkit/StringTk.h>
#include "Config.h"

#include <../generated/FsckConfigFields.inc>  // bake in data definitions

// Note: Keep in sync with enum RunMode
RunModesElem const __RunModes[] =
{
   { "--checkfs", RunMode_CHECKFS },
   { "--enablequota", RunMode_ENABLEQUOTA },
   { NULL, RunMode_INVALID }
};

Config::Config(int argc, char** argv) :
   AbstractConfig(argc, argv)
{
   initConfig(argc, argv, false, true);
   logType = LogType_LOGFILE;
}

/**
 * Determine RunMode from config.
 * If a valid RunMode exists in the config, the corresponding config element will be erased.
 */
enum RunMode Config::determineRunMode()
{
   /* test for given help argument, e.g. in case the user wants to see mode-specific help with
      arguments "--help --<mode>". */

   StringMapIter iter = configMap.find(RUNMODE_HELP_KEY_STRING);
   if(iter != configMap.end() )
   { // user did specify "--help"
      /* note: it's important to remove the help arg here, because mode help will call this again
         to find out whether user wants to see mode-specific help. */
      configMap.erase(iter);
      return RunMode_HELP;
   }

   // walk all defined modes to check whether we find any of them in the config

   for(int i=0; __RunModes[i].modeString != NULL; i++)
   {
      iter = configMap.find(__RunModes[i].modeString);
      if(iter != configMap.end() )
      { // we found a valid mode in the config
         configMap.erase(iter);
         return __RunModes[i].runMode;
      }
   }

   // no valid mode found

   return RunMode_INVALID;
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes true to prepend "--" to all config keys.
 */
void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults(addDashes);

   commonLoadConfigDefaults(&this->configMap, ArraySlice(FsckConfigFields_infos), addDashes);

   // weird special case: adding config value dynamically
   configMapRedefine(&this->configMap, "cfgFile", createDefaultCfgFilename(), addDashes);
}

/**
 * @param addDashes true to prepend "--" to tested config keys for matching.
 */
void Config::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false, addDashes);

   commonApplyConfigMap(static_cast<FsckConfigFields *>(this), &this->configMap, ArraySlice(FsckConfigFields_infos), enableException, addDashes);

   // Making the older bitset from the generated config, for compatibility
   this->checkFsActions.reset();
   if (this->checkMalformedChunk)
      this->checkFsActions.set(CHECK_MALFORMED_CHUNK);
   if (this->checkFilesWithMissingTargets)
     this->checkFsActions.set(CHECK_FILES_WITH_MISSING_TARGETS);
   if (this->checkOrphanedDentryByIDFiles)
     this->checkFsActions.set(CHECK_ORPHANED_DENTRY_BYIDFILES);
   if (this->checkDirEntriesWithBrokenIDFile)
     this->checkFsActions.set(CHECK_DIRENTRIES_WITH_BROKENIDFILE);
   if (this->checkOrphanedChunk)
     this->checkFsActions.set(CHECK_ORPHANED_CHUNK);
   if (this->checkChunksInWrongPath)
     this->checkFsActions.set(CHECK_CHUNKS_IN_WRONGPATH);
   if (this->checkWrongInodeOwner)
     this->checkFsActions.set(CHECK_WRONG_INODE_OWNER);
   if (this->checkWrongOwnerInDentry)
     this->checkFsActions.set(CHECK_WRONG_OWNER_IN_DENTRY);
   if (this->checkOrphanedContDir)
     this->checkFsActions.set(CHECK_ORPHANED_CONT_DIR);
   if (this->checkOrphanedDirInode)
     this->checkFsActions.set(CHECK_ORPHANED_DIR_INODE);
   if (this->checkOrphanedFileInode)
     this->checkFsActions.set(CHECK_ORPHANED_FILE_INODE);
   if (this->checkDanglingDentry)
     this->checkFsActions.set(CHECK_DANGLING_DENTRY);
   if (this->checkMissingContDir)
     this->checkFsActions.set(CHECK_MISSING_CONT_DIR);
   if (this->checkWrongFileAttribs)
     this->checkFsActions.set(CHECK_WRONG_FILE_ATTRIBS);
   if (this->checkWrongDirAttribs)
     this->checkFsActions.set(CHECK_WRONG_DIR_ATTRIBS);
   if (this->checkOldStyledHardlinks)
     this->checkFsActions.set(CHECK_OLD_STYLED_HARDLINKS);
}

void Config::initImplicitVals()
{
   // tuneNumWorkers
   if (!tuneNumWorkers)
      tuneNumWorkers = BEEGFS_MAX(System::getNumOnlineCPUs() * 2, 4);

   if (!tuneDbFragmentSize)
      tuneDbFragmentSize = uint64_t(sysconf(_SC_PHYS_PAGES) ) * sysconf(_SC_PAGESIZE) / 2;

   // just blindly assume that 384 bytes will be enough for a single cache entry. should be
   if (!tuneDentryCacheSize)
      tuneDentryCacheSize = tuneDbFragmentSize / 384;

   // read in connAuthFile only if we are running as root.
   // if not root, the program will abort anyway
   if(!geteuid())
   {
      AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);
   }
}

std::string Config::createDefaultCfgFilename() const
{
   struct stat statBuf;

   const int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if (!statRes && S_ISREG(statBuf.st_mode))
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

   return ""; // no default file otherwise
}

