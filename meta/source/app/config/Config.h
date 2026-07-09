#pragma once

#include <common/app/config/AbstractConfig.h>
#include <common/nodes/TargetCapacityPools.h>
#include <../generated/MetaConfigFields.h>

enum TargetChooserType
{
   TargetChooserType_RANDOMIZED = 0,
   TargetChooserType_ROUNDROBIN = 1, // round-robin in ID order
   TargetChooserType_RANDOMROBIN = 2, // randomized round-robin (round-robin, but shuffle result)
   TargetChooserType_RANDOMINTERNODE = 3, // select random targets from different nodes/domains
   TargetChooserType_RANDOMINTRANODE = 4, // select random targets from the same node/domain
};

class Config : public AbstractConfig, public MetaConfigFields
{
   public:
      Config(int argc, char** argv);
      virtual ~Config();

   private:

      TargetMap*        sysTargetAttachmentMap; /* implicitly by sysTargetAttachmentFile, NULL if
                                                   unset */

      TargetChooserType tuneTargetChooserNum;  // auto-generated based on tuneTargetChooser

      // internals

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      void initSysTargetAttachmentMap();
      void initTuneTargetChooserNum();

   public:
      const TargetMap* getSysTargetAttachmentMap() const
      {
         return sysTargetAttachmentMap;
      }
      TargetChooserType getTuneTargetChooserNum() const
      {
         return tuneTargetChooserNum;
      }
};
