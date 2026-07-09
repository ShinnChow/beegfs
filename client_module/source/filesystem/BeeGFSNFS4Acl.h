#ifndef BEEGFSNFS4ACL_H_
#define BEEGFSNFS4ACL_H_

#include <linux/fs.h>
#include <linux/cred.h>
#include <linux/xattr.h>
#include <linux/uidgid.h>
#include "BeeGFSNFS4AclXdr.h"

/* xattr name carrying the NFS4 ACL blob. Userspace convention from
 * nfs4-acl-tools; not a kernel-defined name. */
#define BEEGFS_NFS4ACL_XATTR_NAME  "system.nfs4_acl"

struct Config;
typedef struct Config Config;

struct FhgfsInode;
typedef struct FhgfsInode FhgfsInode;

enum BeeGFSNFS4PrincipalKind
{
   // Enum values 0-2 represent special principals OWNER@, GROUP@ and EVERYONE@
   BEEGFS_NFS4_PRINCIPAL_OWNER = 0,
   BEEGFS_NFS4_PRINCIPAL_GROUP,
   BEEGFS_NFS4_PRINCIPAL_EVERYONE,
   BEEGFS_NFS4_PRINCIPAL_UID,
   BEEGFS_NFS4_PRINCIPAL_GID,
   BEEGFS_NFS4_PRINCIPAL_UNRESOLVED,
};

struct BeeGFSNFS4PreparedAce
{
   uint32_t type;
   uint32_t flags;
   uint32_t mask;
   enum BeeGFSNFS4PrincipalKind principalKind;
   kuid_t uid;
   kgid_t gid;
};

struct BeeGFSNFS4PreparedAcl
{
   uint32_t naces;
   struct BeeGFSNFS4PreparedAce *aces;
};

/*
 * Per-FhgfsInode NFS4 ACL cache state:
 * - fhgfsInode->nfs4_acl_cache == NULL: no NFS4 ACL cache entry is present
 * - status == -ENODATA: negative NFS4 ACL cache entry, meaning no ACL xattr was found
 * - status == 0: positive NFS4 ACL cache entry, and @acl points to the prepared ACL
 */
struct BeeGFSNFS4AclCache
{
   struct rcu_head rcu;         /* for RCU deferred free */
   struct BeeGFSNFS4PreparedAcl *acl; /* prepared, immutable NFS4 ACL */
   Time cached_at;              /* when cache was populated */
   int status;                  /* 0 => valid ACL, -ENODATA => known absent */
};

enum BeeGFSNFS4AclCacheLookup
{
   BEEGFS_NFS4ACL_CACHE_MISS,
   BEEGFS_NFS4ACL_CACHE_NEGATIVE_HIT,
   BEEGFS_NFS4ACL_CACHE_POSITIVE_HIT,
};

enum BeeGFSNFS4AclResolveResult
{
   BEEGFS_NFS4ACL_RESOLVE_FALLBACK,
   BEEGFS_NFS4ACL_RESOLVE_NEGATIVE_HIT,
   BEEGFS_NFS4ACL_RESOLVE_POSITIVE_HIT,
   BEEGFS_NFS4ACL_RESOLVE_ERROR,
};

int BeeGFSNFS4Acl_fetch(struct inode *inode, const char *name, void *buffer,
   size_t size);

int BeeGFSNFS4Acl_checkPermission(const struct BeeGFSNFS4PreparedAcl *acl, struct inode *inode,
   const struct cred *cred, int mask, unsigned long ino);

enum BeeGFSNFS4AclResolveResult BeeGFSNFS4Acl_resolveCache(struct inode *inode, Config *cfg,
   const struct BeeGFSNFS4AclCache **out_cache, int *out_error);
void BeeGFSNFS4Acl_invalidateCache(struct inode *inode);
void BeeGFSNFS4Acl_freePrepared(struct BeeGFSNFS4PreparedAcl *acl);
void BeeGFSNFS4Acl_freeCacheRcu(struct rcu_head *rcu);

bool BeeGFSNFS4Acl_initResolvers(void);

#endif /* BEEGFSNFS4ACL_H_ */
