#include <app/config/MountConfig.h>
#include <common/toolkit/list/StrCpyList.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/Common.h>

#include <linux/parser.h>
#ifdef KERNEL_HAS_ONLY_INIT_FS_CONTEXT
#include <linux/fs_parser.h>
#endif


enum {
   /* Mount options that take string arguments */
   Opt_cfgFile,
   Opt_logStdFile,
   Opt_sysMgmtdHost,
   Opt_tunePreferredMetaFile,
   Opt_tunePreferredStorageFile,

   Opt_connInterfacesList,
   Opt_connAuthFile,
   Opt_connDisableAuthentication,
   Opt_connDisableIPv6,

   /* Mount options that take integer arguments */
   Opt_logLevel,
   Opt_connPortShift,
   Opt_connMgmtdPort,
   Opt_sysMountSanityCheckMS,

   /* Mount options that take no arguments */
   Opt_grpid,

   Opt_err
};



static match_table_t fhgfs_mount_option_tokens =
{
   /* Mount options that take string arguments */
   { Opt_cfgFile, "cfgFile=%s" },
   { Opt_logStdFile, "logStdFile=%s" },
   { Opt_sysMgmtdHost, "sysMgmtdHost=%s" },
   { Opt_tunePreferredMetaFile, "tunePreferredMetaFile=%s" },
   { Opt_tunePreferredStorageFile, "tunePreferredStorageFile=%s" },

   { Opt_connInterfacesList, "connInterfacesList=%s" },
   { Opt_connAuthFile, "connAuthFile=%s" },
   { Opt_connDisableAuthentication, "connDisableAuthentication=%s" },
   { Opt_connDisableIPv6, "connDisableIPv6=%s" },

   /* Mount options that take integer arguments */
   { Opt_logLevel, "logLevel=%d" },
   { Opt_connPortShift, "connPortShift=%d" },
   { Opt_connMgmtdPort, "connMgmtdPort=%u" },
   { Opt_sysMountSanityCheckMS, "sysMountSanityCheckMS=%u" },

   { Opt_grpid, "grpid" },

   { Opt_err, NULL }
};

#ifdef KERNEL_HAS_ONLY_INIT_FS_CONTEXT
/*
 * Use the new mount API parser directly instead of translating back through
 * the legacy match_token() table. The duplicate option description is
 * intentional while both kernel mount API paths are supported:
 * fs_parameter_spec/fs_parse() defines the structured new-API path, and
 * fhgfs_mount_option_tokens remains only for kernels that still use the old
 * raw option string path. Once support
 * for those kernels is dropped, the legacy table and parser can be removed
 * without rewriting this new-API parser.
 */
static const struct fs_parameter_spec fhgfs_mount_parameters[] =
{
   fsparam_string_empty("cfgFile", Opt_cfgFile),
   fsparam_string_empty("logStdFile", Opt_logStdFile),
   fsparam_string_empty("sysMgmtdHost", Opt_sysMgmtdHost),
   fsparam_string_empty("tunePreferredMetaFile", Opt_tunePreferredMetaFile),
   fsparam_string_empty("tunePreferredStorageFile", Opt_tunePreferredStorageFile),

   fsparam_string_empty("connInterfacesList", Opt_connInterfacesList),
   fsparam_string_empty("connAuthFile", Opt_connAuthFile),
   fsparam_string_empty("connDisableAuthentication", Opt_connDisableAuthentication),
   fsparam_string_empty("connDisableIPv6", Opt_connDisableIPv6),

   fsparam_s32("logLevel", Opt_logLevel),
   fsparam_u32("connPortShift", Opt_connPortShift),
   fsparam_u32("connMgmtdPort", Opt_connMgmtdPort),
   fsparam_u32("sysMountSanityCheckMS", Opt_sysMountSanityCheckMS),

   fsparam_flag("grpid", Opt_grpid),
   {}
};
#endif



#ifdef KERNEL_HAS_ONLY_INIT_FS_CONTEXT
static int MountConfig_setStringParam(char** field, struct fs_parameter* param)
{
   if(!param->string)
      return -ENOMEM;

   SAFE_KFREE(*field);
   *field = param->string;
   param->string = NULL;
   return 0;
}

int MountConfig_parseParam(MountConfig* this, struct fs_context* fc, struct fs_parameter* param)
{
   struct fs_parse_result result;
   int tokenID;

   tokenID = fs_parse(fc, fhgfs_mount_parameters, param, &result);
   if(tokenID < 0)
      return tokenID;

#ifdef BEEGFS_DEBUG
   if(param->string)
      printk_fhgfs(KERN_INFO, "Mount option = '%s=%s'\n", param->key, param->string);
   else
      printk_fhgfs(KERN_INFO, "Mount option = '%s'\n", param->key);
#endif

   switch(tokenID)
   {
      case Opt_cfgFile:
         return MountConfig_setStringParam(&this->cfgFile, param);

      case Opt_logStdFile:
         return MountConfig_setStringParam(&this->logStdFile, param);

      case Opt_sysMgmtdHost:
         return MountConfig_setStringParam(&this->sysMgmtdHost, param);

      case Opt_tunePreferredMetaFile:
         return MountConfig_setStringParam(&this->tunePreferredMetaFile, param);

      case Opt_tunePreferredStorageFile:
         return MountConfig_setStringParam(&this->tunePreferredStorageFile, param);

      case Opt_connInterfacesList:
         return MountConfig_setStringParam(&this->connInterfacesList, param);

      case Opt_connAuthFile:
         return MountConfig_setStringParam(&this->connAuthFile, param);

      case Opt_connDisableAuthentication:
         return MountConfig_setStringParam(&this->connDisableAuthentication, param);

      case Opt_connDisableIPv6:
         return MountConfig_setStringParam(&this->connDisableIPv6, param);

      case Opt_logLevel:
         this->logLevel = result.int_32;
         this->logLevelDefined = true;
         return 0;

      case Opt_connPortShift:
         this->connPortShift = result.uint_32;
         this->connPortShiftDefined = true;
         return 0;

      case Opt_connMgmtdPort:
         this->connMgmtdPort = result.uint_32;
         this->connMgmtdPortDefined = true;
         return 0;

      case Opt_sysMountSanityCheckMS:
         this->sysMountSanityCheckMS = result.uint_32;
         this->sysMountSanityCheckMSDefined = true;
         return 0;

      case Opt_grpid:
         this->grpid = true;
         return 0;

      default:
         return invalf(fc, "beegfs: Unknown mount option '%s'", param->key);
   }
}
#endif

bool MountConfig_parseFromRawOptions(MountConfig* this, char* mountOptions)
{
   char* currentOption;

   if(!mountOptions)
   {
      printk_fhgfs_debug(KERN_INFO, "Mount options = <none>\n");
      return true;
   }


   printk_fhgfs_debug(KERN_INFO, "Mount options = '%s'\n", mountOptions);

   while( (currentOption = strsep(&mountOptions, ",") ) != NULL)
   {
      substring_t args[MAX_OPT_ARGS];
      int tokenID;

      if(!*currentOption)
         continue; // skip empty string

      tokenID = match_token(currentOption, fhgfs_mount_option_tokens, args);

      switch(tokenID)
      {
         /* Mount options that take STRING arguments */

         case Opt_cfgFile:
         {
            SAFE_KFREE(this->cfgFile);

            this->cfgFile = match_strdup(args);// (string kalloc'ed => needs kfree later)
         } break;

         case Opt_logStdFile:
         {
            SAFE_KFREE(this->logStdFile);

            this->logStdFile = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_sysMgmtdHost:
         {
            SAFE_KFREE(this->sysMgmtdHost);

            this->sysMgmtdHost = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_tunePreferredMetaFile:
         {
            SAFE_KFREE(this->tunePreferredMetaFile);

            this->tunePreferredMetaFile = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_tunePreferredStorageFile:
         {
            SAFE_KFREE(this->tunePreferredStorageFile);

            this->tunePreferredStorageFile = match_strdup(args); // (string kalloc'ed => needs kfree later)
         } break;

         case Opt_connInterfacesList:
         {
            SAFE_KFREE(this->connInterfacesList);

            this->connInterfacesList = match_strdup(args);
         } break;

         case Opt_connAuthFile:
         {
            SAFE_KFREE(this->connAuthFile);

            this->connAuthFile = match_strdup(args);
         } break;

         case Opt_connDisableAuthentication:
         {
            SAFE_KFREE(this->connDisableAuthentication);

            this->connDisableAuthentication = match_strdup(args);
         } break;

         case Opt_connDisableIPv6:
         {
            SAFE_KFREE(this->connDisableIPv6);

            this->connDisableIPv6 = match_strdup(args);
         } break;

         /* Mount options that take INTEGER arguments */

         case Opt_logLevel:
         {
            if(match_int(args, &this->logLevel) )
               goto err_exit_invalid_option;

            this->logLevelDefined = true;
         } break;

         case Opt_connPortShift:
         {
            if(match_int(args, &this->connPortShift) )
               goto err_exit_invalid_option;

            this->connPortShiftDefined = true;
         } break;

         case Opt_connMgmtdPort:
         {
            if(match_int(args, &this->connMgmtdPort) )
               goto err_exit_invalid_option;

            this->connMgmtdPortDefined = true;
         } break;

         case Opt_sysMountSanityCheckMS:
         {
            if(match_int(args, &this->sysMountSanityCheckMS) )
               goto err_exit_invalid_option;

            this->sysMountSanityCheckMSDefined = true;
         } break;

         case Opt_grpid:
            this->grpid = true;
            break;

         default:
            goto err_exit_unknown_option;
      }
   }

   return true;


err_exit_unknown_option:
   printk_fhgfs(KERN_WARNING, "Unknown mount option: '%s'\n", currentOption);
   return false;

err_exit_invalid_option:
   printk_fhgfs(KERN_WARNING, "Invalid mount option: '%s'\n", currentOption);
   return false;
}

void MountConfig_showOptions(MountConfig* this, struct seq_file* sf)
{
   if (this->cfgFile)
      seq_printf(sf, ",cfgFile=%s", this->cfgFile);

   if (this->logStdFile)
      seq_printf(sf, ",logStdFile=%s", this->logStdFile);

   if (this->sysMgmtdHost)
      seq_printf(sf, ",sysMgmtdHost=%s", this->sysMgmtdHost);

   if (this->tunePreferredMetaFile)
      seq_printf(sf, ",tunePreferredMetaFile=%s", this->tunePreferredMetaFile);

   if (this->tunePreferredStorageFile)
      seq_printf(sf, ",tunePreferredStorageFile=%s", this->tunePreferredStorageFile);

   if (this->connInterfacesList)
      seq_printf(sf, ",connInterfacesList=%s", this->connInterfacesList);

   if (this->connAuthFile)
      seq_printf(sf, ",connAuthFile=%s", this->connAuthFile);

   if (this->connDisableAuthentication)
      seq_printf(sf, ",connDisableAuthentication=%s", this->connDisableAuthentication);

   if (this->connDisableIPv6)
      seq_printf(sf, ",connDisableIPv6=%s", this->connDisableIPv6);

   if (this->logLevelDefined)
      seq_printf(sf, ",logLevel=%d", this->logLevel);

   if (this->connPortShiftDefined)
      seq_printf(sf, ",connPortShift=%d", this->connPortShift);

   if (this->connMgmtdPortDefined)
      seq_printf(sf, ",connMgmtdPort=%u", this->connMgmtdPort);

   if (this->sysMountSanityCheckMSDefined)
      seq_printf(sf, ",sysMountSanityCheckMS=%u", this->sysMountSanityCheckMS);

   if (this->grpid)
      seq_printf(sf, ",grpid");
}
