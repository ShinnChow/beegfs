#include <beegfs-base/CommonConfig.common.h>
#include <beegfs-codegen/ConfigCodegen.h>

int main()
{
   ConfigFieldsGenerator gen;

   gen.field("cfgFile",            "",         CFGTYPE_STRING);
   gen.field("logType",            "logfile",  CFGTYPE_LOGTYPE);
   gen.field("logLevel",           "3",        CFGTYPE_INT);
   gen.field("logNoDate",          "true",     CFGTYPE_BOOL);
   gen.field("logStdFile",         "",         CFGTYPE_STRING);
   gen.field("logNumLines",        "50000",    CFGTYPE_INT);
   gen.field("logNumRotatedFiles", "2",        CFGTYPE_INT);
   gen.field("connPortShift",      "0",        CFGTYPE_INT);

   // To be able to merge these with the legacy settings later, we set them to
   // -1 here. Otherwise it is impossible to detect if they have actually been
   // set or just loaded the default.  The actual default values are applied
   // during the post processing in applyConfigMap.
   // We don't generate getters for those because there are legacy custom
   // getters that add "connPortShift".
   gen.field("connClientPort",     "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8004 */;
   gen.field("connStoragePort",    "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8003 */;
   gen.field("connMetaPort",       "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8005 */;
   gen.field("connMonPort",        "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8007 */;
   gen.field("connMgmtdPort",      "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8008 */;

   // Note: deprecated UDP and TCP specific variants.
   gen.field("connClientPortTCP",   "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8004 */;
   gen.field("connStoragePortTCP",  "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8003 */;
   gen.field("connMetaPortTCP",     "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8005 */;
   gen.field("connMonPortTCP",      "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8007 */;
   gen.field("connMgmtdPortTCP",    "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8008 */;
   gen.field("connClientPortUDP",   "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8004 */;
   gen.field("connStoragePortUDP",  "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8003 */;
   gen.field("connMetaPortUDP",     "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8005 */;
   gen.field("connMonPortUDP",      "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8007 */;
   gen.field("connMgmtdPortUDP",    "-1", CFGTYPE_INT, CFGFLAG_NO_GETTER | CFGFLAG_NO_SETTER) /* 8008 */;

   // (end deprecated variants)
   gen.field("connUseRDMA",                    "true",      CFGTYPE_BOOL);
   gen.field("connBacklogTCP",                 "64",        CFGTYPE_UINT);
   gen.field("connMaxInternodeNum",            "6",         CFGTYPE_UINT);
   gen.obsolete_field("connNonPrimaryExpiration");
   gen.field("connFallbackExpirationSecs",     "900",       CFGTYPE_UINT);
   gen.field("connTCPRcvBufSize",              "0",         CFGTYPE_INT);
   gen.field("connUDPRcvBufSize",              "0",         CFGTYPE_INT);
   gen.field("connRDMABufSize",                "8192",      CFGTYPE_UINT);
   gen.field("connRDMABufNum",                 "70",        CFGTYPE_UINT);
   gen.field("connRDMATypeOfService",          "0",         CFGTYPE_UINT8);
   gen.field("connNetFilterFile",              "",          CFGTYPE_STRING);
   gen.field("connAuthFile",                   "",          CFGTYPE_STRING);
   gen.field("connDisableAuthentication",      "false",     CFGTYPE_BOOL);
   gen.field("connTcpOnlyFilterFile",          "",          CFGTYPE_STRING);
   gen.field("connRestrictOutboundInterfaces", "false",     CFGTYPE_BOOL);
   gen.field("connNoDefaultRoute",             "::/0",      CFGTYPE_STRING);
   gen.field("connDisableIPv6",                "false",     CFGTYPE_BOOL);

   // connMessagingTimeouts: default to zero, indicating that constants
   // specified in Common.h are used.
   gen.field("connMessagingTimeouts",      "0,0,0",         CFGTYPE_INT, CFG_TUPLE_TYPE(3));

   // connRDMATimeouts: zero values are interpreted as the defaults specified in IBVSocket.cpp
   // YUCK. beegfs-ctl and beegfs-fsck use beegfs-client.conf. That file defines
   // connRDMATimeouts as 5 comma-separated values, but user space only has 3
   // values. The utils are supposed to ignore the configuration.
   gen.field("connRDMATimeouts",           "0,0,0",         CFGTYPE_INT, CFGFLAG_IS_LIST);
   gen.field("sysMgmtdHost",               "",              CFGTYPE_STRING);
   gen.field("sysUpdateTargetStatesSecs",  "0",             CFGTYPE_UINT);

   // relative to build directory
   const char *dirpath = "../generated";  // relative to build directory
   const char *classname = "CommonConfigFields";

   generate_config_sources(&gen, dirpath, classname);

   return 0;
}
