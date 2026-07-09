#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>
#include <bitset>
#include <../generated/FsckConfigFields.h>
#include "FsckConfigFields.common.h"

#define RUNMODE_HELP_KEY_STRING     "--help" /* key for usage help */
#define __RUNMODES_SIZE \
   ( (sizeof(__RunModes) ) / (sizeof(RunModesElem) ) - 1)
   /* -1 because last elem is NULL */

enum CheckFsActions
{
   CHECK_MALFORMED_CHUNK               = 0,
   CHECK_FILES_WITH_MISSING_TARGETS    = 1,
   CHECK_ORPHANED_DENTRY_BYIDFILES     = 2,
   CHECK_DIRENTRIES_WITH_BROKENIDFILE  = 3,
   CHECK_ORPHANED_CHUNK                = 4,
   CHECK_CHUNKS_IN_WRONGPATH           = 5,
   CHECK_WRONG_INODE_OWNER             = 6,
   CHECK_WRONG_OWNER_IN_DENTRY         = 7,
   CHECK_ORPHANED_CONT_DIR             = 8,
   CHECK_ORPHANED_DIR_INODE            = 9,
   CHECK_ORPHANED_FILE_INODE           = 10,
   CHECK_DANGLING_DENTRY               = 11,
   CHECK_MISSING_CONT_DIR              = 12,
   CHECK_WRONG_FILE_ATTRIBS            = 13,
   CHECK_WRONG_DIR_ATTRIBS             = 14,
   CHECK_OLD_STYLED_HARDLINKS          = 15,
   CHECK_FS_ACTIONS_COUNT              = 16
};

// Note: Keep in sync with __RunModes array
enum RunMode
{
   RunMode_CHECKFS              =     0,
   RunMode_ENABLEQUOTA          =     1,
   RunMode_HELP                 =     2,
   RunMode_INVALID              =     3 /* not valid as index in RunModes array */
};

struct RunModesElem
{
   const char* modeString;
   enum RunMode runMode;
};


extern RunModesElem const __RunModes[];

class Config : public AbstractConfig, public FsckConfigFields
{
   public:
      Config(int argc, char** argv);

      enum RunMode determineRunMode();

   private:

      // configurables

      std::bitset<CHECK_FS_ACTIONS_COUNT> checkFsActions;

      // internals
      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      std::string createDefaultCfgFilename() const;

   public:
      // getters & setters
      std::bitset<CHECK_FS_ACTIONS_COUNT> getCheckFsActions() const
      {
         return checkFsActions;
      }

      const StringMap* getUnknownConfigArgs() const
      {
         return getConfigMap();
      }

      void disableAutomaticRepairMode()
      {
         this->automatic = false;
      }

      void setReadOnly()
      {
         this->readOnly = true;
      }
};

#endif /*CONFIG_H_*/
