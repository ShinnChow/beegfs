#pragma once

#include <string>
#include <vector>
#include <beegfs-base/CommonConfig.common.h>

// CfgDef is the type used to auto-generate various types and values.
// From this information, a class containing fields with getters and setters is produced.
// Also, a metadata structure is produced that contains the offset within the class for each field.
struct CfgDef
{
   std::string name;
   std::string defaultval;
   CfgType cfgtype;
   uint32_t cfgflags;
};


struct ConfigFieldsGenerator
{
   std::vector<CfgDef> cfgDefs;

   void field(std::string name, std::string defaultval, CfgType cfgtype, unsigned cfgflags = 0)
   {
      cfgDefs.push_back({ name, defaultval, cfgtype, cfgflags });
   }

   void override_default(std::string name, std::string defaultval)
   {
      field(name, defaultval, CFGTYPE_NONE, CFGFLAG_OVERRIDE);
   }

   void obsolete_field(std::string name)
   {
      field(name, "", CFGTYPE_NONE, CFGFLAG_OBSOLETE_FIELD);
   }
};

void generate_config_sources(ConfigFieldsGenerator *fields, const char *dirpath, const char *classname);

