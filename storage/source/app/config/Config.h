#pragma once

#include <common/app/config/AbstractConfig.h>
#include <../generated/StorageConfigFields.h>

/**
 * Find out whether this distro hash sync_file_range() support (added in linux-2.6.17, glibc 2.6).
 * Note: Problem is that RHEL 5 defines SYNC_FILE_RANGE_WRITE, but uses glibc 2.5 which has no
 * sync_file_range support, so linker complains about undefined reference.
 */
#ifdef __GNUC__
   #include <features.h>
   #include <fcntl.h>
   #if __GLIBC_PREREQ(2, 6) && defined(SYNC_FILE_RANGE_WRITE)
      #define CONFIG_DISTRO_HAS_SYNC_FILE_RANGE
   #endif
#endif


class Config : public AbstractConfig, public StorageConfigFields
{
   public:
      Config(int argc, char** argv);

   private:

      // configurables

      std::list<Path> storageDirectories;
      std::list<std::string> storeFsUUIDs;

      // internals

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;

   public:
      // getters & setters
      const std::string& getConnInterfacesList() const
      {
         return connInterfacesList;
      }

      const std::list<Path>& getStorageDirectories() const { return storageDirectories; }

      const std::list<std::string>& getStoreFsUUIDs() const { return storeFsUUIDs; }
};
