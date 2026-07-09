#include <beegfs-codegen/ConfigCodegen.h>

int main()
{
   ConfigFieldsGenerator gen;

   gen.override_default("cfgFile",     "");
   gen.override_default("connUseRDMA", "false");

   gen.field("connInterfacesFile",         "", CFGTYPE_STRING);
   gen.field("tuneNumWorkers",             "4", CFGTYPE_UINT);
   gen.field("runDaemonized",              "false", CFGTYPE_BOOL);
   gen.field("pidFile",                    "", CFGTYPE_STRING);
   gen.field("dbType",                     "influxdb", CFGTYPE_STRING);  // there is DbType enum but we keep it as string here
   gen.field("dbHostName",                 "localhost", CFGTYPE_STRING);
   gen.field("dbHostPort",                 "8086", CFGTYPE_UINT);
   gen.field("dbDatabase",                 "beegfs_mon", CFGTYPE_STRING);
   gen.field("dbAuthFile",                 "", CFGTYPE_STRING);

   /* those are used by influxdb only but are kept like this for compatibility */
   gen.field("dbMaxPointsPerRequest",      "5000", CFGTYPE_UINT);
   gen.field("dbSetRetentionPolicy",       "true", CFGTYPE_BOOL);
   gen.field("dbRetentionDuration",        "1d", CFGTYPE_STRING);

   gen.field("dbBucket",                   "", CFGTYPE_STRING);
   gen.field("cassandraMaxInsertsPerBatch","25", CFGTYPE_UINT);
   gen.field("cassandraTTLSecs", "86400", CFGTYPE_UINT);
   gen.field("collectClientOpsByNode",     "true", CFGTYPE_BOOL);
   gen.field("collectClientOpsByUser",     "true", CFGTYPE_BOOL);
   gen.field("httpTimeoutMSecs",           "1000", CFGTYPE_UINT);
   gen.field("statsRequestIntervalSecs",   "5", CFGTYPE_UINT);
   gen.field("nodelistRequestIntervalSecs","30", CFGTYPE_UINT);
   gen.field("curlCheckSSLCertificates",   "true", CFGTYPE_BOOL);

   const char *dirpath = "../generated";  // relative to build directory
   const char *classname = "MonConfigFields";

   generate_config_sources(&gen, dirpath, classname);

   return 0;
}
