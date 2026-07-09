/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2009 Fraunhofer ITWM, 2022 ThinkParQ GmbH
 *
 * BeeGFS client userspace API (ioctl ABI).
 *
 * This header is licensed GPL-2.0 WITH Linux-syscall-note. The syscall-note
 * exception means userspace programs that use this header to issue ioctls to
 * the BeeGFS client kernel module are not derivative works of the module and
 * may carry any license. See https://spdx.org/licenses/Linux-syscall-note.html
 */
#ifndef _BEEGFS_CLIENT_H_INCLUDED
#define _BEEGFS_CLIENT_H_INCLUDED

#define BEEGFS_IOCTL_CFG_MAX_PATH 4096 // just an arbitrary value, has to be identical in user space
#define BEEGFS_IOCTL_TEST_STRING  "_FhGFS_" /* copied to user space by BEEGFS_IOC_TEST_IS_FHGFS to
                                 to confirm an FhGFS mount */
#define BEEGFS_IOCTL_TEST_BUFLEN  6 /* note: char[6] is actually the wrong size for the
                                 BEEGFS_IOCTL_TEST_STRING that is exchanged, but that is no problem
                                 in this particular case and so we keep it for compatibility */
#define BEEGFS_IOCTL_MOUNTID_BUFLEN     256
#define BEEGFS_IOCTL_NODEALIAS_BUFLEN   256 // The alias (formerly string ID) buffer length.
#define BEEGFS_IOCTL_NODETYPE_BUFLEN    16
#define BEEGFS_IOCTL_FILENAME_MAXLEN    256 // max supported filename len (incl terminating zero)

// entryID string is made of three 32 bit values in hexadecimal form plus two dashes
// (see common/toolkit/StorageTk.h)
#define BEEGFS_IOCTL_ENTRYID_MAXLEN     26

// Stripe targets: matches DIRINODE_MAX_STRIPE_TARGETS enforced by the meta server.
// RST IDs: no hard server-side limit; 256 is chosen to match stripe targets.
#define BEEGFS_IOCTL_MAX_STRIPE_TARGETS 256
#define BEEGFS_IOCTL_MAX_RST_IDS        256

// stripe pattern types
#define BEEGFS_STRIPEPATTERN_INVALID      0
#define BEEGFS_STRIPEPATTERN_RAID0        1
#define BEEGFS_STRIPEPATTERN_RAID10       2
#define BEEGFS_STRIPEPATTERN_BUDDYMIRROR  3

#define BEEGFS_IOCTL_PING_MAX_COUNT       10
#define BEEGFS_IOCTL_PING_MAX_INTERVAL    2000
#define BEEGFS_IOCTL_PING_NODE_BUFLEN     64
#define BEEGFS_IOCTL_PING_SOCKTYPE_BUFLEN 8

/*
 * General notes:
 * - the _IOR() macro is for ioctls that read information, _IOW refers to ioctls that write or make
 *    modifications (e.g. file creation).
 *
 * - _IOR(type, number, data_type) meanings:
 *    - note: _IOR() encodes all three values (type, number, data_type size) into the request number
 *    - type: 8 bit driver-specific number to identify the driver if there are multiple drivers
 *       listening to the same fd (e.g. such as the TCP and IP layers).
 *    - number: 8 bit integer command number, so different numbers for different routines.
 *    - data_type: the data type (size) to be exchanged with the driver (though this number can
 *       also rather be seen as a command number subversion, because the actual number given here is
 *       not really exchanged unless the drivers' ioctl handler explicity does the exchange).
 */

#define BEEGFS_IOCTYPE_ID                    'f'

#define BEEGFS_IOCNUM_GETVERSION_OLD         1 // value from FS_IOC_GETVERSION in linux/fs.h
#define BEEGFS_IOCNUM_GETVERSION             3
#define BEEGFS_IOCNUM_GET_CFG_FILE           20
#define BEEGFS_IOCNUM_CREATE_FILE            21
#define BEEGFS_IOCNUM_TEST_IS_FHGFS          22
#define BEEGFS_IOCNUM_TEST_IS_BEEGFS         22
#define BEEGFS_IOCNUM_GET_RUNTIME_CFG_FILE   23
#define BEEGFS_IOCNUM_GET_MOUNTID            24
#define BEEGFS_IOCNUM_GET_STRIPEINFO         25
#define BEEGFS_IOCNUM_GET_STRIPETARGET       26
#define BEEGFS_IOCNUM_MKFILE_STRIPEHINTS     27
#define BEEGFS_IOCNUM_CREATE_FILE_V2         28
#define BEEGFS_IOCNUM_CREATE_FILE_V3         29
#define BEEGFS_IOCNUM_GETINODEID             30
#define BEEGFS_IOCNUM_GETENTRYINFO           31
#define BEEGFS_IOCNUM_PINGNODE               32
#define BEEGFS_IOCNUM_SET_FILE_STATE         33 // set accessFlags and dataState on a file
#define BEEGFS_IOCNUM_GETENTRYINFO_V2        34 // get full entry info via RPC to meta

#define BEEGFS_IOC_GETVERSION     _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETVERSION, long)
#define BEEGFS_IOC32_GETVERSION   _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETVERSION, int)
#define BEEGFS_IOC_GET_CFG_FILE   _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_CFG_FILE, struct BeegfsIoctl_GetCfgFile_Arg)
#define BEEGFS_IOC_CREATE_FILE    _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_CREATE_FILE, struct BeegfsIoctl_MkFile_Arg)
#define BEEGFS_IOC_CREATE_FILE_V2  _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_CREATE_FILE_V2, struct BeegfsIoctl_MkFileV2_Arg)
#define BEEGFS_IOC_CREATE_FILE_V3  _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_CREATE_FILE_V3, struct BeegfsIoctl_MkFileV3_Arg)
#define BEEGFS_IOC_TEST_IS_FHGFS  _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_TEST_IS_FHGFS, char[BEEGFS_IOCTL_TEST_BUFLEN])
#define BEEGFS_IOC_TEST_IS_BEEGFS  _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_TEST_IS_BEEGFS, char[BEEGFS_IOCTL_TEST_BUFLEN])
#define BEEGFS_IOC_GET_RUNTIME_CFG_FILE    _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_RUNTIME_CFG_FILE, struct BeegfsIoctl_GetCfgFile_Arg)
#define BEEGFS_IOC_GET_MOUNTID    _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_MOUNTID, char[BEEGFS_IOCTL_MOUNTID_BUFLEN])
#define BEEGFS_IOC_GET_STRIPEINFO          _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_STRIPEINFO, struct BeegfsIoctl_GetStripeInfo_Arg)
#define BEEGFS_IOC_GET_STRIPETARGET        _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_STRIPETARGET, struct BeegfsIoctl_GetStripeTarget_Arg)
#define BEEGFS_IOC_GET_STRIPETARGET_V2     _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GET_STRIPETARGET, struct BeegfsIoctl_GetStripeTargetV2_Arg)
#define BEEGFS_IOC_MKFILE_STRIPEHINTS      _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_MKFILE_STRIPEHINTS, struct BeegfsIoctl_MkFileWithStripeHints_Arg)
#define BEEGFS_IOC_GETINODEID             _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETINODEID, struct BeegfsIoctl_GetInodeID_Arg)
#define BEEGFS_IOC_GETENTRYINFO             _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETENTRYINFO, struct BeegfsIoctl_GetEntryInfo_Arg)
#define BEEGFS_IOC_PINGNODE                 _IOR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_PINGNODE, struct BeegfsIoctl_PingNode_Arg)
#define BEEGFS_IOC_SET_FILE_STATE _IOW( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_SET_FILE_STATE, struct BeegfsIoctl_SetFileState_Arg)
#define BEEGFS_IOC_GETENTRYINFO_V2 _IOWR( \
   BEEGFS_IOCTYPE_ID, BEEGFS_IOCNUM_GETENTRYINFO_V2, struct BeegfsIoctl_GetEntryInfoV2_Arg)

/* used to return the client config file path using an IOCTL */
struct BeegfsIoctl_GetCfgFile_Arg
{
   char path[BEEGFS_IOCTL_CFG_MAX_PATH]; // (out-value) where the result path will be stored
   int length;                          /* (in-value) length of path buffer (unused, because it's
                                           after a fixed-size path buffer anyways) */
};

/* used to pass information for file creation */
struct BeegfsIoctl_MkFile_Arg
{
   uint16_t ownerNodeID; // owner node of the parent dir

   const char* parentParentEntryID; // entryID of the parent of the parent (=> the grandparentID)
   int parentParentEntryIDLen;

   const char* parentEntryID; // entryID of the parent
   int parentEntryIDLen;

   const char* parentName;   // name of parent dir
   int parentNameLen;

   // file information
   const char* entryName; // file name we want to create
   int entryNameLen;
   int fileType; // see linux/fs.h or man 3 readdir, DT_UNKNOWN, DT_FIFO, ...

   const char* symlinkTo;  // Only must be set for symlinks. The name a symlink is supposed to point to
   int symlinkToLen; // Length of the symlink name

   int mode; // mode (permission) of the new file

   // user ID and group only will be used, if the current user is root
   uid_t uid; // user ID
   gid_t gid; // group ID

   int numTargets;     // number of targets in prefTargets array (without final 0 element)
   uint16_t* prefTargets;  // array of preferred targets (additional last element must be 0)
   int prefTargetsLen; // raw byte length of prefTargets array (including final 0 element)
};

struct BeegfsIoctl_MkFileV2_Arg
{
   uint32_t ownerNodeID; // owner node/group of the parent dir

   const char* parentParentEntryID; // entryID of the parent of the parent (=> the grandparentID)
   int parentParentEntryIDLen;

   const char* parentEntryID; // entryID of the parent
   int parentEntryIDLen;
   int parentIsBuddyMirrored;

   const char* parentName;   // name of parent dir
   int parentNameLen;

   // file information
   const char* entryName; // file name we want to create
   int entryNameLen;
   int fileType; // see linux/fs.h or man 3 readdir, DT_UNKNOWN, DT_FIFO, ...

   char* symlinkTo;  // Only must be set for symlinks. The name a symlink is supposed to point to
   int symlinkToLen; // Length of the symlink name

   int mode; // mode (permission) of the new file

   // user ID and group only will be used, if the current user is root
   uid_t uid; // user ID
   gid_t gid; // group ID

   int numTargets;     // number of targets in prefTargets array (without final 0 element)
   uint16_t* prefTargets;  // array of preferred targets (additional last element must be 0)
   int prefTargetsLen; // raw byte length of prefTargets array (including final 0 element)
};

struct BeegfsIoctl_MkFileV3_Arg
{
   uint32_t ownerNodeID; // owner node/group of the parent dir

   const char* parentParentEntryID; // entryID of the parent of the parent (=> the grandparentID)
   int parentParentEntryIDLen;

   const char* parentEntryID; // entryID of the parent
   int parentEntryIDLen;
   int parentIsBuddyMirrored;

   const char* parentName;   // name of parent dir
   int parentNameLen;

   // file information
   const char* entryName; // file name we want to create
   int entryNameLen;
   int fileType; // see linux/fs.h or man 3 readdir, DT_UNKNOWN, DT_FIFO, ...

   const char* symlinkTo;  // Only must be set for symlinks. The name a symlink is supposed to point to
   int symlinkToLen; // Length of the symlink name

   int mode; // mode (permission) of the new file

   // user ID and group only will be used, if the current user is root
   uid_t uid; // user ID
   gid_t gid; // group ID

   int numTargets;     // number of targets in prefTargets array (without final 0 element)
   uint16_t* prefTargets;  // array of preferred targets (additional last element must be 0)
   int prefTargetsLen; // raw byte length of prefTargets array (including final 0 element)

   uint16_t storagePoolId; // if set, this is used to override the pool id of the parent dir
};

/* used to get the stripe info of a file */
struct BeegfsIoctl_GetStripeInfo_Arg
{
   unsigned outPatternType; // (out-value) stripe pattern type (STRIPEPATTERN_...)
   unsigned outChunkSize; // (out-value) chunksize for striping
   uint16_t outNumTargets; // (out-value) number of stripe targets of given file
};

/* used to get the stripe target of a file */
struct BeegfsIoctl_GetStripeTarget_Arg
{
   uint16_t targetIndex; // index of the target that should be queried (0-based)

   uint16_t outTargetNumID; // (out-value) numeric ID of target with given index
   uint16_t outNodeNumID;   // (out-value) numeric ID of node to which this target is mapped
   char outNodeAlias[BEEGFS_IOCTL_NODEALIAS_BUFLEN]; /* (out-value) alias (formerly string ID) of the node
                                                       to which this target is mapped */
};

struct BeegfsIoctl_GetStripeTargetV2_Arg
{
   /* inputs */
   uint32_t targetIndex;

   /* outputs */
   uint32_t targetOrGroup; // target ID if the file is not buddy mirrored, otherwise mirror group ID

   uint32_t primaryTarget; // target ID != 0 if buddy mirrored
   uint32_t secondaryTarget; // target ID != 0 if buddy mirrored

   uint32_t primaryNodeID; // node ID of target (if unmirrored) or primary target (if mirrored)
   uint32_t secondaryNodeID; // node ID of secondary target, or 0 if unmirrored

   char primaryNodeAlias[BEEGFS_IOCTL_NODEALIAS_BUFLEN];
   char secondaryNodeAlias[BEEGFS_IOCTL_NODEALIAS_BUFLEN];
};

/* used to pass information for file creation with stripe hints */
struct BeegfsIoctl_MkFileWithStripeHints_Arg
{
   const char* filename; // file name we want to create
   unsigned mode; // mode (access permission) of the new file

   unsigned numtargets; // number of desired targets, 0 for directory default
   unsigned chunksize; // in bytes, must be 2^n >= 64Ki, 0 for directory default
};

struct BeegfsIoctl_GetInodeID_Arg
{
   // input
   char entryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];

   // output
   uint64_t inodeID;

};

struct BeegfsIoctl_GetEntryInfo_Arg
{
   uint32_t ownerID;
   char parentEntryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];
   char entryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];
   int entryType;
   int featureFlags;
};

struct BeegfsIoctl_PingNode_Arg_Params
{
   uint32_t nodeId;
   char nodeType[BEEGFS_IOCTL_NODETYPE_BUFLEN];
   unsigned count;
   unsigned interval;
};

struct BeegfsIoctl_PingNode_Arg_Results
{
   char outNode[BEEGFS_IOCTL_PING_NODE_BUFLEN];
   unsigned outSuccess;
   unsigned outErrors;
   unsigned outTotalTime;
   unsigned outPingTime[BEEGFS_IOCTL_PING_MAX_COUNT];
   char outPingType[BEEGFS_IOCTL_PING_MAX_COUNT][BEEGFS_IOCTL_PING_SOCKTYPE_BUFLEN];
};

struct BeegfsIoctl_PingNode_Arg
{
   struct BeegfsIoctl_PingNode_Arg_Params params;
   struct BeegfsIoctl_PingNode_Arg_Results results;
};

/* used to set access flags and data state on a file */
struct BeegfsIoctl_SetFileState_Arg
{
   char filename[BEEGFS_IOCTL_FILENAME_MAXLEN];
   uint8_t fileState; // combined AccessFlags and DataState
};

/*
 * V2 entry info: returns comprehensive entry information including stripe pattern,
 * path info, RST, session counts, and file state in a single ioctl call.
 *
 * The fd passed to this ioctl MUST be an open directory.  If 'filename' is
 * non-empty it names a direct child (file or sub-directory) of that directory;
 * the kernel sends a LookupIntent RPC to meta to resolve the child and uses its
 * EntryInfo for the subsequent GetEntryInfo RPC.  If 'filename' is empty ("") the
 * ioctl queries the directory fd itself (using the fd's cached EntryInfo).
 *
 * Design note on variable-length data:
 *   BeeGFS entries (files and directories) may have a variable number of stripe targets and RST IDs.
 *   Since ioctl arguments must be a fixed size known at compile time, both
 *   stripeTargetIDs[] and rstIds[] are pre-allocated to BEEGFS_IOCTL_MAX_STRIPE_TARGETS
 *   and BEEGFS_IOCTL_MAX_RST_IDS (256 each) entries respectively. Only the first
 *   numTargets / numRSTIds entries are valid. If the actual count exceeds the cap,
 *   the kernel logs a warning and truncates to the maximum.
 *
 * Note: storagePoolId reflects the storage pool ID stored in the kernel StripePattern,
 *   which is populated during deserialization via StripePattern_createFromBuf().
 */
struct BeegfsIoctl_GetEntryInfoV2_Arg
{
   /* Input/output: name of a direct child to query, relative to the fd directory.
    * Leave empty (filename[0] == '\0') to query the directory fd itself.
    * Must not contain '/' or exceed NAME_MAX characters.
    * On success the kernel writes back the resolved entryInfo->fileName:
    *   - if non-empty on input: overwritten with the canonical name from meta
    *   - if empty on input: filled with the directory's own name */
   char filename[BEEGFS_IOCTL_FILENAME_MAXLEN];

   /* Basic entry info (same as V1) */
   uint32_t ownerID;
   char parentEntryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];
   char entryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];
   int entryType;
   int featureFlags;

   /* Stripe pattern */
   uint32_t patternType;
   uint32_t chunkSize;
   uint32_t storagePoolId;
   uint32_t defaultNumTargets;
   uint16_t numTargets;
   uint16_t stripeTargetIDs[BEEGFS_IOCTL_MAX_STRIPE_TARGETS];

   /* PathInfo */
   uint32_t pathInfoFlags;
   uint32_t origParentUID;
   char origParentEntryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];

   /* Remote Storage Target (RST) */
   uint8_t  rstMajorVersion;
   uint8_t  rstMinorVersion;
   uint16_t rstCoolDownPeriod;
   uint16_t rstFilePolicies;
   uint32_t numRSTIds;
   uint32_t rstIds[BEEGFS_IOCTL_MAX_RST_IDS];

   /* Session and state info */
   uint32_t numSessionsRead;
   uint32_t numSessionsWrite;
   uint8_t  fileDataState;
   /* Result of the GetEntryInfo RPC. Zero (FhgfsOpsErr_SUCCESS) means full
    * data is populated. A non-zero error value indicates a partial result;
    * only the basic entry info fields above are valid. Non-zero values are
    * FhgfsOpsErr codes */
   int32_t  getEntryInfoResult;
};

#endif
