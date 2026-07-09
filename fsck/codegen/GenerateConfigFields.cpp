#include <beegfs-codegen/ConfigCodegen.h>
#include <app/config/FsckConfigFields.common.h>

int main()
{
   ConfigFieldsGenerator gen;

   gen.obsolete_field("logClientID");
   gen.obsolete_field("logType");
   gen.obsolete_field("connNumCommRetries");
   gen.obsolete_field("connUnmountRetries");
   gen.obsolete_field("connCommRetrySecs");
   gen.obsolete_field("connMaxConcurrentAttempts");
   gen.obsolete_field("connRDMAInterfacesFile");
   gen.obsolete_field("connTCPFallbackEnabled");
   gen.obsolete_field("connDisableIPv6");
   gen.obsolete_field("connMessagingTimeouts");
   gen.obsolete_field("connInterfacesList");
   gen.obsolete_field("connRDMATimeouts");
   gen.obsolete_field("connRDMAFragmentSize");
   gen.obsolete_field("connRDMAKeyType");
   gen.obsolete_field("connRDMAMetaFragmentSize");
   gen.obsolete_field("connRDMAMetaBufNum");
   gen.obsolete_field("connRDMAMetaBufSize");
   gen.obsolete_field("tuneFileCacheType");
   gen.obsolete_field("tunePagedIOBufSize");
   gen.obsolete_field("tunePagedIOBufNum");
   gen.obsolete_field("tuneFileCacheBufSize");
   gen.obsolete_field("tuneFileCacheBufNum");
   gen.obsolete_field("tunePageCacheValidityMS");
   gen.obsolete_field("tuneAttribCacheValidityMS");
   gen.obsolete_field("tuneMaxWriteWorks");
   gen.obsolete_field("tuneMaxReadWorks");
   gen.obsolete_field("tuneAllowMultiSetWrite");
   gen.obsolete_field("tuneAllowMultiSetRead");
   gen.obsolete_field("tunePathBufSize");
   gen.obsolete_field("tunePathBufNum");
   gen.obsolete_field("tuneMaxReadWriteNum");
   gen.obsolete_field("tuneMaxReadWriteNodesNum");
   gen.obsolete_field("tuneMsgBufSize");
   gen.obsolete_field("tuneMsgBufNum");
   gen.obsolete_field("tuneRemoteFSync");
   gen.obsolete_field("tunePreferredMetaFile");
   gen.obsolete_field("tunePreferredStorageFile");
   gen.obsolete_field("tuneUseGlobalFileLocks");
   gen.obsolete_field("tuneRefreshOnGetAttr");
   gen.obsolete_field("tuneInodeBlockBits");
   gen.obsolete_field("tuneInodeBlockSize");
   gen.obsolete_field("tuneMaxClientMirrorSize"); // was removed, kept here for compat
   gen.obsolete_field("tuneEarlyCloseResponse");
   gen.obsolete_field("tuneUseGlobalAppendLocks");
   gen.obsolete_field("tuneUseBufferedAppend");
   gen.obsolete_field("tuneStatFsCacheSecs");
   gen.obsolete_field("sysCacheInvalidationVersion");
   gen.obsolete_field("sysCreateHardlinksAsSymlinks");
   gen.obsolete_field("sysMountSanityCheckMS");
   gen.obsolete_field("sysSyncOnClose");
   gen.obsolete_field("sysSessionCheckOnClose");
   gen.obsolete_field("sysSessionChecksEnabled");
   gen.obsolete_field("sysTargetOfflineTimeoutSecs");
   gen.obsolete_field("sysInodeIDStyle");
   gen.obsolete_field("sysSELinuxEnabled");
   gen.obsolete_field("sysSELinuxRevalidate");
   gen.obsolete_field("sysACLsEnabled");
   gen.obsolete_field("sysACLsRevalidate");
   gen.obsolete_field("sysXAttrsEnabled");
   gen.obsolete_field("sysNFSv4ACLsEnabled");
   gen.obsolete_field("sysBypassFileAccessCheckOnMeta");
   gen.obsolete_field("sysXAttrsCheckCapabilities");
   gen.obsolete_field("tuneDirSubentryCacheValidityMS");
   gen.obsolete_field("tuneFileSubentryCacheValidityMS");
   gen.obsolete_field("tuneENOENTCacheValidityMS");
   gen.obsolete_field("tuneCoherentBuffers");
   gen.obsolete_field("sysFileEventLogMask");
   gen.obsolete_field("sysRenameEbusyAsXdev");
   gen.obsolete_field("tuneNumRetryWorkers");
   gen.obsolete_field("connHelperdPortTCP"); // was removed, kept here for compat
   gen.obsolete_field("tuneFileOpenRetryTimeoutMS");
   gen.obsolete_field("tuneFileOpenRetryIntervalMS");
   gen.obsolete_field("sysRemoteInvalEnabled");


   gen.field("connInterfacesFile",       "",                     CFGTYPE_STRING);
   gen.field("tuneNumWorkers",           "32",                   CFGTYPE_UINT);
   gen.field("tunePreferredNodesFile",   "",                     CFGTYPE_STRING);
   gen.field("tuneDbFragmentSize",       "0",                    CFGTYPE_UINT64);
   gen.field("tuneDentryCacheSize",      "0",                    CFGTYPE_UINT64);
   gen.field("runDaemonized",            "false",                CFGTYPE_BOOL);
   gen.field("databasePath",             CONFIG_DEFAULT_DBPATH,  CFGTYPE_STRING);
   gen.field("overwriteDbFile",          "false",                CFGTYPE_BOOL);
   gen.field("testDatabasePath",         CONFIG_DEFAULT_TESTDBPATH, CFGTYPE_STRING); // only relevant for unit testing, to give the used databasePath
   gen.field("databaseNumMaxConns",      "22",                   CFGTYPE_UINT);
   gen.field("overrideRootMDS",          "",                     CFGTYPE_STRING); // not tested well, should only be used by developers
   gen.override_default("logStdFile", CONFIG_DEFAULT_LOGFILE);
   gen.field("logOutFile",               CONFIG_DEFAULT_OUTFILE, CFGTYPE_STRING); // file for fsck output (not the log messages, but the output, which is also on the console)
   gen.override_default("logNoDate",                "false");
   gen.field("readOnly",                 "false",                CFGTYPE_BOOL);
   gen.field("noFetch",                  "false",                CFGTYPE_BOOL);
   gen.field("automatic",                "false",                CFGTYPE_BOOL);
   gen.field("runOffline",               "false",                CFGTYPE_BOOL);
   gen.field("forceRestart",             "false",                CFGTYPE_BOOL);
   gen.field("quotaEnabled",             "false",                CFGTYPE_BOOL);
   gen.field("ignoreDBDiskSpace",        "false",                CFGTYPE_BOOL);


   // formerly "CheckFsActions"
   gen.field("checkMalformedChunk",             "false", CFGTYPE_BOOL);
   gen.field("checkFilesWithMissingTargets",    "false", CFGTYPE_BOOL);
   gen.field("checkOrphanedDentryByIDFiles",    "false", CFGTYPE_BOOL);
   gen.field("checkDirEntriesWithBrokenIDFile", "false", CFGTYPE_BOOL);
   gen.field("checkOrphanedChunk",              "false", CFGTYPE_BOOL);
   gen.field("checkChunksInWrongPath",          "false", CFGTYPE_BOOL);
   gen.field("checkWrongInodeOwner",            "false", CFGTYPE_BOOL);
   gen.field("checkWrongOwnerInDentry",         "false", CFGTYPE_BOOL);
   gen.field("checkOrphanedContDir",            "false", CFGTYPE_BOOL);
   gen.field("checkOrphanedDirInode",           "false", CFGTYPE_BOOL);
   gen.field("checkOrphanedFileInode",          "false", CFGTYPE_BOOL);
   gen.field("checkDanglingDentry",             "false", CFGTYPE_BOOL);
   gen.field("checkMissingContDir",             "false", CFGTYPE_BOOL);
   gen.field("checkWrongFileAttribs",           "false", CFGTYPE_BOOL);
   gen.field("checkWrongDirAttribs",            "false", CFGTYPE_BOOL);
   gen.field("checkOldStyledHardlinks",         "false", CFGTYPE_BOOL);

   const char *dirpath = "../generated";  // relative to build directory
   const char *classname = "FsckConfigFields";

   generate_config_sources(&gen, dirpath, classname);

   return 0;
}
