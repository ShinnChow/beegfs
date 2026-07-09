#include <app/App.h>
#include <app/log/Logger.h>
#include <os/OsCompat.h>
#include <linux/rcupdate.h>
#include "BeeGFSNFS4Acl.h"
#include "BeeGFSNFS4AclResolvers.h"
#include "FhgfsOpsInode.h"
#include "common/Common.h"

static int BeeGFSNFS4Acl_cachePopulate(struct inode *inode);
static int BeeGFSNFS4Acl_eval(const struct BeeGFSNFS4PreparedAcl *acl, const struct inode *inode,
   const struct cred *cred, uint32_t reqmask);
static uint32_t BeeGFSNFS4Acl_vfsToAceReqmask(const struct inode *inode, int mask);
static int BeeGFSNFS4Acl_prepareAce(const struct BeeGFSNFS4Ace *ace,
   struct BeeGFSNFS4PreparedAce *prepared_ace);
static int BeeGFSNFS4Acl_prepareAcl(const struct BeeGFSNFS4Acl *acl,
   struct BeeGFSNFS4PreparedAcl **prepared_acl);
static struct BeeGFSNFS4AclPrincipalResolver* BeeGFSNFS4Acl_selectResolver(
   const char *who, uint32_t len);
static kuid_t BeeGFSNFS4Acl_resolveUid(
   struct BeeGFSNFS4AclPrincipalResolver *resolv, const char *who, uint32_t len);
static kgid_t BeeGFSNFS4Acl_resolveGid(
   struct BeeGFSNFS4AclPrincipalResolver *resolv, const char *who, uint32_t len);

// global resolver for uid/gid style principals
struct BeeGFSNFS4AclPrincipalResolver trivialResolver = {
   .state = RESOLVER_UNINITIALIZED,
   .init = BeeGFSNFS4Acl_initTrivialResolver,
};

struct BeeGFSNFS4AclPrincipalResolver sidResolver = {
   .state = RESOLVER_UNINITIALIZED,
   .init = BeeGFSNFS4Acl_initSidResolver,
};

// lists the resolvers to be initialized by BeeGFSNFS4Acl_initResolvers(). Don't forget to list
// new resolvers here, because they will not be usable otherwise.
static struct BeeGFSNFS4AclPrincipalResolver * supported_resolvers[] = {
   &trivialResolver,
   &sidResolver
};

bool BeeGFSNFS4Acl_initResolvers(void) {
   struct BeeGFSNFS4AclPrincipalResolver *resolv;
   bool ret = true;

   for (size_t i = 0; i < ARRAY_SIZE(supported_resolvers); i++) {
      resolv = supported_resolvers[i];
      if (resolv->state == RESOLVER_UNINITIALIZED) {
         resolv->init(resolv);
         if (resolv->state == RESOLVER_INIT_FAILED) {
            resolv->state = RESOLVER_IGNORE;
            ret = false;
         }
      }
   }
   return ret;
}

static struct BeeGFSNFS4AclPrincipalResolver* BeeGFSNFS4Acl_selectResolver(
   const char *who, uint32_t len)
{
   /*
    * @who is a length-delimited principal string extracted from
    * the ACL payload and is not guaranteed to be NUL-terminated.
    */
   if (len >= 3 && !memcmp(who, "S-1", 3))
      return &sidResolver;

   return &trivialResolver;
}

static kuid_t BeeGFSNFS4Acl_resolveUid(
   struct BeeGFSNFS4AclPrincipalResolver *resolv, const char *who, uint32_t len)
{
   return resolv->resolve_user(&init_user_ns, who, len);
}

static kgid_t BeeGFSNFS4Acl_resolveGid(
   struct BeeGFSNFS4AclPrincipalResolver *resolv, const char *who, uint32_t len)
{
   return resolv->resolve_group(&init_user_ns, who, len);
}

static int BeeGFSNFS4Acl_prepareAce(const struct BeeGFSNFS4Ace *ace,
   struct BeeGFSNFS4PreparedAce *prepared_ace)
{
   const char *who = ace->who;
   uint32_t len = ace->who_len;
   struct BeeGFSNFS4AclPrincipalResolver *resolv;

   memset(prepared_ace, 0, sizeof(*prepared_ace));
   prepared_ace->type = ace->type;
   prepared_ace->flags = ace->flags;
   prepared_ace->mask = ace->mask;
   prepared_ace->principalKind = BEEGFS_NFS4_PRINCIPAL_UNRESOLVED;

   if (who_is_owner(who, len))
   {
      prepared_ace->principalKind = BEEGFS_NFS4_PRINCIPAL_OWNER;
      return 0;
   }

   if (who_is_group(who, len))
   {
      prepared_ace->principalKind = BEEGFS_NFS4_PRINCIPAL_GROUP;
      return 0;
   }

   if (who_is_everyone(who, len))
   {
      prepared_ace->principalKind = BEEGFS_NFS4_PRINCIPAL_EVERYONE;
      return 0;
   }

   resolv = BeeGFSNFS4Acl_selectResolver(who, len);
   switch (resolv->state) {
      case RESOLVER_READY:
         break;
      case RESOLVER_IGNORE:
      case RESOLVER_UNINITIALIZED:
      case RESOLVER_INIT_FAILED:
      default:
         printk_fhgfs_debug(KERN_DEBUG,
            "NFS4 ACL prepare: who=%.*s resolver unavailable state=%d",
            len, who, resolv->state);
         return 0;
   }

   if (ace->flags & ACE4_IDENTIFIER_GROUP)
   {
      prepared_ace->gid = BeeGFSNFS4Acl_resolveGid(resolv, who, len);
      if (gid_valid(prepared_ace->gid))
         prepared_ace->principalKind = BEEGFS_NFS4_PRINCIPAL_GID;

      printk_fhgfs_debug(KERN_DEBUG,
         "NFS4 ACL prepare: who=%.*s kind=%s gid=%u flags=0x%x mask=0x%x",
         len, who,
         prepared_ace->principalKind == BEEGFS_NFS4_PRINCIPAL_GID ? "GID" : "UNRESOLVED",
         __kgid_val(prepared_ace->gid), ace->flags, ace->mask);
   }
   else
   {
      prepared_ace->uid = BeeGFSNFS4Acl_resolveUid(resolv, who, len);
      if (uid_valid(prepared_ace->uid))
         prepared_ace->principalKind = BEEGFS_NFS4_PRINCIPAL_UID;

      printk_fhgfs_debug(KERN_DEBUG,
         "NFS4 ACL prepare: who=%.*s kind=%s uid=%u flags=0x%x mask=0x%x",
         len, who,
         prepared_ace->principalKind == BEEGFS_NFS4_PRINCIPAL_UID ? "UID" : "UNRESOLVED",
         __kuid_val(prepared_ace->uid), ace->flags, ace->mask);
   }

   return 0;
}

static int BeeGFSNFS4Acl_prepareAcl(const struct BeeGFSNFS4Acl *acl,
   struct BeeGFSNFS4PreparedAcl **prepared_acl)
{
   struct BeeGFSNFS4PreparedAcl *prepared;
   uint32_t i;

   *prepared_acl = NULL;

   prepared = kzalloc(sizeof(*prepared), GFP_KERNEL);
   if (!prepared)
      return -ENOMEM;

   prepared->naces = acl->naces;
   if (acl->naces)
   {
      prepared->aces = kcalloc(acl->naces, sizeof(*prepared->aces), GFP_KERNEL);
      if (!prepared->aces)
      {
         kfree(prepared);
         return -ENOMEM;
      }
   }

   for (i = 0; i < acl->naces; i++)
   {
      int rc = BeeGFSNFS4Acl_prepareAce(&acl->aces[i], &prepared->aces[i]);
      if (rc)
      {
         BeeGFSNFS4Acl_freePrepared(prepared);
         return rc;
      }
   }

   printk_fhgfs_debug(KERN_DEBUG, "NFS4 ACL prepare: prepared %u ACEs", prepared->naces);
   *prepared_acl = prepared;
   return 0;
}

static bool BeeGFSNFS4Acl_principalMatchesUserOrGroup(const struct BeeGFSNFS4PreparedAce *ace,
   const struct cred *cred)
{
   if (ace->principalKind == BEEGFS_NFS4_PRINCIPAL_GID)
   {
      if (gid_eq(ace->gid, cred->gid))
      {
         printk_fhgfs_debug(KERN_DEBUG,
            "NFS4 ACL eval: ACE matched primary gid=%u", __kgid_val(ace->gid));
         return true;
      }

      if (in_group_p(ace->gid))
      {
         printk_fhgfs_debug(KERN_DEBUG,
            "NFS4 ACL eval: ACE matched supplementary gid=%u", __kgid_val(ace->gid));
         return true;
      }

      return false;
   }

   if (ace->principalKind == BEEGFS_NFS4_PRINCIPAL_UID)
   {
      printk_fhgfs_debug(KERN_DEBUG,
         "NFS4 ACL eval: checking uid=%u against cred uid=%u",
         __kuid_val(ace->uid), __kuid_val(cred->uid));
      return uid_eq(ace->uid, cred->uid);
   }

   return false;
}

static bool BeeGFSNFS4Acl_aceAppliesToSubject(const struct BeeGFSNFS4PreparedAce *ace,
   const struct inode *inode,
   const struct cred *cred)
{
   if (ace->principalKind == BEEGFS_NFS4_PRINCIPAL_OWNER)
      return uid_eq(inode->i_uid, cred->uid);

   if (ace->principalKind == BEEGFS_NFS4_PRINCIPAL_GROUP)
   {
      if (gid_eq(inode->i_gid, cred->gid))
         return true;
      return in_group_p(inode->i_gid);
   }

   if (ace->principalKind == BEEGFS_NFS4_PRINCIPAL_EVERYONE)
      return true;

   return BeeGFSNFS4Acl_principalMatchesUserOrGroup(ace, cred);
}

/* RCU cache helpers */
void BeeGFSNFS4Acl_freeCacheRcu(struct rcu_head *rcu)
{
   struct BeeGFSNFS4AclCache *c = container_of(rcu, struct BeeGFSNFS4AclCache, rcu);
   if (c->acl)
      BeeGFSNFS4Acl_freePrepared(c->acl);
   kfree(c);
}

/**
 * BeeGFSNFS4Acl_checkPermission - Check permission using a prepared NFS4 ACL
 *
 * Converts the VFS permission mask into an NFS4 reqmask and delegates to
 * BeeGFSNFS4Acl_eval().
 *
 * Return:
 *    0  access allowed by the ACL
 *  -EACCES  access denied by the ACL
 *    1  undecided — caller falls back to POSIX mode-bit checks
 */
int BeeGFSNFS4Acl_checkPermission(const struct BeeGFSNFS4PreparedAcl *acl, struct inode *inode,
   const struct cred *cred, int mask, unsigned long ino)
{
   App* app = FhgfsOps_getApp(inode->i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   uint32_t req;
   int rc;

   if (!acl)
   {
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "NFS4 ACL cache: no ACL present, permission fallback required (ino: %lu)", ino);
      return 1;
   }

   req = BeeGFSNFS4Acl_vfsToAceReqmask(inode, mask);
   rc = BeeGFSNFS4Acl_eval(acl, inode, cred, req);

   if (rc == 0)
      return 0;
   if (rc < 0)
      return rc;

   return 1;
}

/*
 * BeeGFSNFS4Acl_cacheLookup() assumes the caller already holds
 * rcu_read_lock(); it does not acquire or release RCU itself.
 *
 * On positive hit, @out_cache points to the cached entry and is valid only
 * while that RCU read-side section is held.
 * Negative hit means a valid cached -ENODATA result. Miss means no usable
 * cache entry exists.
 */
static enum BeeGFSNFS4AclCacheLookup BeeGFSNFS4Acl_cacheLookup(FhgfsInode *fhgfsInode,
   struct inode *inode, Config *cfg, const struct BeeGFSNFS4AclCache **out_cache)
{
   App* app = FhgfsOps_getApp(inode->i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   const struct BeeGFSNFS4AclCache *cached_acl;
   unsigned timeout;
   unsigned elapsed;

   *out_cache = NULL;

   cached_acl = rcu_dereference(fhgfsInode->nfs4_acl_cache);
   if (!cached_acl)
      return BEEGFS_NFS4ACL_CACHE_MISS;

   timeout = FhgfsInode_getCacheValidityMS(inode->i_mode, cfg);
   if (!timeout)
      return BEEGFS_NFS4ACL_CACHE_MISS;

   elapsed = Time_elapsedMS((Time*)&cached_acl->cached_at);
   if (elapsed > timeout)
   {
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "NFS4 ACL cache expired (ino: %lu, elapsed: %u ms, timeout: %u ms)",
         inode->i_ino, elapsed, timeout);
      return BEEGFS_NFS4ACL_CACHE_MISS;
   }

   *out_cache = cached_acl;

   if (cached_acl->status == -ENODATA)
   {
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "NFS4 ACL ENODATA cache hit (ino: %lu)", inode->i_ino);
      return BEEGFS_NFS4ACL_CACHE_NEGATIVE_HIT;
   }

   Logger_logFormatted(log, Log_DEBUG, logContext,
      "NFS4 ACL positive cache hit (ino: %lu)", inode->i_ino);
   return BEEGFS_NFS4ACL_CACHE_POSITIVE_HIT;
}

/*
 * Interpret a cache entry while rcu_read_lock() is already held.
 *
 * The caller retains responsibility for releasing the RCU read lock. A
 * BEEGFS_NFS4ACL_RESOLVE_POSITIVE_HIT result means @out_cache points to the
 * cached ACL entry that will be evaluated under the same lock.
 */
static enum BeeGFSNFS4AclResolveResult BeeGFSNFS4Acl_resolveLockedCache(
   const struct BeeGFSNFS4AclCache *cached_acl, const struct BeeGFSNFS4AclCache **out_cache)
{
   if (!cached_acl)
      return BEEGFS_NFS4ACL_RESOLVE_FALLBACK;

   *out_cache = cached_acl;

   if (cached_acl->status == -ENODATA)
      return BEEGFS_NFS4ACL_RESOLVE_NEGATIVE_HIT;

   return BEEGFS_NFS4ACL_RESOLVE_POSITIVE_HIT;
}

/*
 * Resolve the inode's NFS4 ACL cache state.
 *
 * The function first checks the current cache state under RCU. On cache miss,
 * it populates the cache and then re-checks the published cache entry under a
 * fresh RCU read-side critical section.
 *
 * On BEEGFS_NFS4ACL_RESOLVE_POSITIVE_HIT, @out_cache points to a valid cached
 * ACL entry and rcu_read_lock() remains held for the caller. The caller must
 * release the lock after evaluating the cached ACL.
 *
 * On all other return values, no RCU read lock is held on return.
 */
enum BeeGFSNFS4AclResolveResult BeeGFSNFS4Acl_resolveCache(struct inode *inode, Config *cfg,
   const struct BeeGFSNFS4AclCache **out_cache, int *out_error)
{
   FhgfsInode *fhgfsInode = BEEGFS_INODE(inode);
   enum BeeGFSNFS4AclCacheLookup lookup;
   const struct BeeGFSNFS4AclCache *cached_acl;
   enum BeeGFSNFS4AclResolveResult resolve;
   int rc;

   *out_cache = NULL;
   *out_error = 0;

   rcu_read_lock();
   lookup = BeeGFSNFS4Acl_cacheLookup(fhgfsInode, inode, cfg, out_cache);
   switch (lookup)
   {
      case BEEGFS_NFS4ACL_CACHE_POSITIVE_HIT:
         return BEEGFS_NFS4ACL_RESOLVE_POSITIVE_HIT;

      case BEEGFS_NFS4ACL_CACHE_NEGATIVE_HIT:
         rcu_read_unlock();
         return BEEGFS_NFS4ACL_RESOLVE_NEGATIVE_HIT;

      case BEEGFS_NFS4ACL_CACHE_MISS:
      default:
         rcu_read_unlock();
         break;
   }

   rc = BeeGFSNFS4Acl_cachePopulate(inode);
   if (rc == -ENODATA)
      return BEEGFS_NFS4ACL_RESOLVE_NEGATIVE_HIT;

   if (rc < 0)
   {
      *out_error = rc;
      return BEEGFS_NFS4ACL_RESOLVE_ERROR;
   }

   rcu_read_lock();
   cached_acl = rcu_dereference(fhgfsInode->nfs4_acl_cache);
   resolve = BeeGFSNFS4Acl_resolveLockedCache(cached_acl, out_cache);
   if (resolve != BEEGFS_NFS4ACL_RESOLVE_POSITIVE_HIT)
      rcu_read_unlock();
   return resolve;
}

/**
 * BeeGFSNFS4Acl_cachePopulate - Fetch and cache the NFS4 ACL for an inode
 * @inode: inode whose ACL cache should be populated or refreshed
 *
 * Retrieves the serialized "system.nfs4_acl" blob from the metadata server,
 * decodes it from XDR format, prepares its ACEs, and stores the prepared ACL in the inode's
 * RCU-protected cache. If the server reports that the "system.nfs4_acl" xattr
 * does not exist, a negative cache entry is stored so future lookups do not
 * trigger additional fetches. If the xattr exists but contains a valid empty
 * ACL payload, a positive cache entry with zero ACEs is stored.
 *
 * The routine allocates the necessary cache structures, performs the XDR
 * decoding, prepares the ACL, replaces the cache entry under RCU, and schedules
 * the previous cache (if any) for deferred reclamation.
 *
 * Returns:
 *  0        on success (ACL cached successfully)
 *  -ENODATA if metadata reports that no ACL exists for the inode
 *  -ENOMEM  if memory allocation fails
 *  other negative errno codes from XDR decode or metadata fetch errors
 */
static int BeeGFSNFS4Acl_cachePopulate(struct inode *inode)
{
   App* app = FhgfsOps_getApp(inode->i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   FhgfsInode *fhgfsInode = BEEGFS_INODE(inode);
   struct BeeGFSNFS4AclCache *newc, *oldc;
   struct BeeGFSNFS4Acl *acl = NULL;
   struct BeeGFSNFS4PreparedAcl *prepared_acl = NULL;
   ssize_t len = BEEGFS_NFS4ACL_MAX_XDR;
   void *buf = NULL;
   size_t xdr_size = 0;
   int rc;

   buf = kmalloc(len, GFP_KERNEL);
   if (!buf)
      return -ENOMEM;

   rc = BeeGFSNFS4Acl_fetch(inode, BEEGFS_NFS4ACL_XATTR_NAME, buf, len);
   if (rc == -ENODATA)
   {
      kfree(buf);

      newc = kzalloc(sizeof(*newc), GFP_KERNEL);
      if (!newc)
         return -ENOMEM;

      newc->acl = NULL;
      newc->status = -ENODATA;
      Time_setToNow(&newc->cached_at);

      oldc = xchg((struct BeeGFSNFS4AclCache **)&fhgfsInode->nfs4_acl_cache, newc);
      if (oldc)
         call_rcu(&oldc->rcu, BeeGFSNFS4Acl_freeCacheRcu);

      Logger_logFormatted(log, Log_DEBUG, logContext,
         "Cached NFS4 ACL ENODATA result (ino: %lu)", inode->i_ino);
      return -ENODATA;
   }
   else if (rc < 0)
   {
      kfree(buf);
      return rc;
   }

   if (rc == 0)
   {
      kfree(buf);

      /* Metadata returned an empty ACL; remember that explicitly. */
      acl = kzalloc(sizeof(*acl), GFP_KERNEL);
      if (!acl)
         return -ENOMEM;

      acl->naces = 0;
      acl->aces  = NULL;
   }
   else if (rc > 0)
   {
      xdr_size = rc;
      rc = BeeGFSNFS4AclXdr_decode(buf, xdr_size, &acl);
      kfree(buf);
      if (rc)
      {
         Logger_logFormatted(log, Log_WARNING, logContext,
            "Failed to decode NFS4 ACL from meta (ino: %lu, xdr_size: %zu, rc: %d)",
            inode->i_ino, xdr_size, rc);
         return rc;
      }
   }
   rc = BeeGFSNFS4Acl_prepareAcl(acl, &prepared_acl);
   if (rc)
   {
      BeeGFSNFS4Acl_freePrepared(prepared_acl);
      BeeGFSNFS4AclXdr_free(acl);
      return rc;
   }

   newc = kzalloc(sizeof(*newc), GFP_KERNEL);
   if (!newc)
   {
      BeeGFSNFS4Acl_freePrepared(prepared_acl);
      BeeGFSNFS4AclXdr_free(acl);
      return -ENOMEM;
   }

   BeeGFSNFS4AclXdr_free(acl);
   newc->acl = prepared_acl;
   Time_setToNow(&newc->cached_at);
   newc->status = 0;

   /* Publish new cache, free old lazily */
   oldc = xchg((struct BeeGFSNFS4AclCache **)&fhgfsInode->nfs4_acl_cache, newc);
   if (oldc)
      call_rcu(&oldc->rcu, BeeGFSNFS4Acl_freeCacheRcu);
   return 0;
}

void BeeGFSNFS4Acl_invalidateCache(struct inode *inode)
{
   App* app = FhgfsOps_getApp(inode->i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   FhgfsInode *fhgfsInode = BEEGFS_INODE(inode);
   struct BeeGFSNFS4AclCache *oldc;

   /* Atomically swap cache pointer to NULL. */
   oldc = xchg((struct BeeGFSNFS4AclCache **)&fhgfsInode->nfs4_acl_cache, NULL);

   Logger_logFormatted(log, Log_DEBUG, logContext,
      "Cleared NFS4 ACL cache (ino: %lu, had entry: %d)", inode->i_ino, !!oldc);
   if (oldc)
      call_rcu(&oldc->rcu, BeeGFSNFS4Acl_freeCacheRcu);
}

void BeeGFSNFS4Acl_freePrepared(struct BeeGFSNFS4PreparedAcl *acl)
{
   if (!acl)
      return;

   kfree(acl->aces);
   kfree(acl);
}

/**
 * BeeGFSNFS4Acl_fetch - Fetch raw NFS4 ACL xattr from BeeGFS meta node
 *
 * @inode:  Inode whose ACL should be retrieved
 * @name:   xattr name ("system.nfs4_acl")
 * @buffer: Caller-provided buffer
 * @size:   Size of buffer
 *
 * Issues a getxattr() call to the BeeGFS metadata server to retrieve the
 * serialized NFS4 ACL XDR blob associated with the inode. The ACL is stored
 * on the meta node under the "system.nfs4_acl" extended attribute.
 *
 * The raw buffer returned here can be decoded into a parsed in-kernel ACL
 * structure using BeeGFSNFS4AclXdr_decode(), which is then used by the ACL
 * evaluation logic.
 *
 * ----------------------------------------------------------------------------
 * Return values
 * ----------------------------------------------------------------------------
 *   >=0 → number of bytes copied into @buffer
 *   <0  → negative errno (e.g. -ENODATA if no ACL present,
 *                        -ERANGE if buffer too small,
 *                        -EIO on metadata fetch error)
 */
int BeeGFSNFS4Acl_fetch(struct inode *inode, const char* name, void *buffer, size_t size)
{
   return FhgfsOps_getxattr(inode, name, buffer, size);
}

/**
 * BeeGFSNFS4Acl_eval - Evaluate NFS4 ACLs against a requested access mask
 *
 * @acl:     Prepared ACL structure (list of ACEs) for the inode
 * @inode:   Inode being accessed
 * @cred:    Credentials of the requesting subject
 * @reqmask: Bitmask of requested access rights (already mapped from VFS MAY_*
 *           to NFS4 ACE4_* bits by BeeGFSNFS4Acl_vfsToAceReqmask()).
 *
 * ----------------------------------------------------------------------------
 * Evaluation rules
 * ----------------------------------------------------------------------------
 *   • ACEs are processed in order.
 *   • DENY ACEs take precedence: if any requested bit is denied → -EACCES.
 *   • ALLOW ACEs clear bits from "remaining"; all must be allowed to succeed.
 *   • AUDIT/ALARM ACE types are ignored.
 *   • INHERIT_ONLY ACEs are ignored for direct access checks.
 *   • If after processing, some bits remain undecided → default undecided and fallback to POSIX mode.
 *
 * ----------------------------------------------------------------------------
 * Return values
 * ----------------------------------------------------------------------------
 *   0        → Access granted (all bits allowed, none denied)
 *  -EACCES   → Access denied
 *   1        → Undecided (fallback to POSIX mode)
 */
static int BeeGFSNFS4Acl_eval(const struct BeeGFSNFS4PreparedAcl *acl, const struct inode *inode,
   const struct cred *cred, uint32_t reqmask)
{
   uint32_t remaining = reqmask;
   uint32_t deny_seen = 0;
   uint32_t allow_seen = 0;
   uint32_t i;

   /* No ACL or an empty ACL does not decide access; let the caller fall back. */
   if (!acl || acl->naces == 0)
      return 1;

   for (i = 0; i < acl->naces; i++)
   {
      const struct BeeGFSNFS4PreparedAce *ace = &acl->aces[i];
      uint32_t bits;

      /* Ignore ACE if the inherit only flag is set */
      if (ace->flags & ACE4_INHERIT_ONLY_ACE)
         continue;

      /* Ignore AUDIT/ALARM for access checks */
      if (ace->type != ACE4_ACCESS_ALLOWED_ACE_TYPE && ace->type != ACE4_ACCESS_DENIED_ACE_TYPE)
         continue;

      /* Skip ACEs that don't apply to this subject. An ACE whose named principal
       * cannot be resolved on this client is also skipped here (it matches no
       * subject). On a correctly configured cluster every principal resolves and
       * enforcement -- including named DENY entries -- works as written; skipping
       * only takes effect when resolution fails (e.g. a client lacking the SID
       * mapping). The skip is the fail-safe choice for the recommended whitelist
       * pattern (named ALLOWs + catch-all DENY EVERYONE@): an unresolved ALLOW
       * grants nothing and falls through to the catch-all DENY, which is anchored
       * to EVERYONE@/OWNER@/GROUP@ (never needing resolution) and so stays
       * consistent even on clients with incomplete identity info. A DENY of a
       * *named* principal instead relies on that client resolving it, so it is
       * less robust than the whitelist form -- see user-doc advanced_topics/acl.rst
       * (NFSv4 ACLs). */
      if (!BeeGFSNFS4Acl_aceAppliesToSubject(ace, inode, cred))
         continue;

      /* Only the bits we still care about matter */
      bits = ace->mask & remaining;
      if (!bits)
         continue;

      printk_fhgfs_debug(KERN_DEBUG,
         "NFS4 ACL eval: ace=%u type=%u kind=%u mask=0x%x remaining=0x%x bits=0x%x",
         i, ace->type, ace->principalKind, ace->mask, remaining, bits);

      if (ace->type == ACE4_ACCESS_DENIED_ACE_TYPE)
      {
         deny_seen |= bits;
         /* Any outstanding bit denied → deny immediately */
         printk_fhgfs_debug(KERN_DEBUG,
            "NFS4 ACL eval: deny ace=%u bits=0x%x", i, bits);
         return -EACCES;
      } else { /* ALLOW */
         printk_fhgfs_debug(KERN_DEBUG, "NFS4 ACL: ACE bits: %d", bits);
         allow_seen |= bits;
         remaining &= ~bits;
         printk_fhgfs_debug(KERN_DEBUG, "NFS4 ACL: remaining bits: %d", remaining);
         if (!remaining)
         {
            printk_fhgfs_debug(KERN_DEBUG,
               "NFS4 ACL eval: allow ace=%u completed request", i);
            return 0; /* all requested bits allowed */
         }
      }
   }

   /* ACL didn't fully decide. Return "undecided" so caller can fallback. */
   printk_fhgfs_debug(KERN_DEBUG,
      "NFS4 ACL eval: undecided reqmask=0x%x remaining=0x%x deny=0x%x allow=0x%x",
      reqmask, remaining, deny_seen, allow_seen);
   return 1; /* >0 means not decided; 0 allowed; <0 denied */
}

/**
 * BeeGFSNFS4Acl_vfsToAceReqmask - Translate Linux VFS mask to NFS4 ACE mask bits
 *
 * @inode: Inode being accessed (to distinguish file vs directory)
 * @mask:  VFS mask from inode_permission() (MAY_READ, MAY_WRITE, etc.)
 *
 * ----------------------------------------------------------------------------
 * Mapping of VFS requests to NFS4 ACE bits
 * ----------------------------------------------------------------------------
 * BeegFS translates Linux VFS permission checks into reqmask bits that correspond
 * to NFSv4 ACE mask constants (see RFC 7530 §6.2.1.3):
 * https://datatracker.ietf.org/doc/html/rfc7530#section-6.2.1.3
 *
 * ----------------------------------------------------------------------------
 * Mapping
 * ----------------------------------------------------------------------------
 *   MAY_ACCESS → ACE4_READ_ATTRIBUTES
 *   MAY_READ   → ACE4_READ_DATA | ACE4_READ_ATTRIBUTES | ACE4_READ_ACL |
 *                ACE4_READ_NAMED_ATTRS
 *   MAY_WRITE  → (file) ACE4_WRITE_DATA | ACE4_APPEND_DATA
 *                (dir)  ACE4_WRITE_DATA | ACE4_APPEND_DATA | ACE4_DELETE_CHILD
 *                (both) + ACE4_WRITE_ATTRIBUTES | ACE4_WRITE_NAMED_ATTRS
 *   MAY_APPEND → ACE4_APPEND_DATA
 *   MAY_EXEC   → ACE4_EXECUTE
 *   MAY_CHDIR  → ACE4_EXECUTE
 *   MAY_OPEN   → ACE4_EXECUTE (dir) or ACE4_READ_ATTRIBUTES (file),
 *                applied only when no other MAY_* flag is set
 *
 * ----------------------------------------------------------------------------
 * Notes
 * ----------------------------------------------------------------------------
 * • This mapping is specific to BeeGFS's direct ACL evaluator.
 * • In Linux nfsd, similar mapping is implicit because ACLs are first
 *   converted into POSIX ACLs/mode bits and evaluated via generic VFS checks.
 *
 * ----------------------------------------------------------------------------
 * Return values
 * ----------------------------------------------------------------------------
 *   Bitmask of ACE4_* constants suitable for BeeGFSNFS4Acl_eval().
 */
static uint32_t BeeGFSNFS4Acl_vfsToAceReqmask(const struct inode *inode, int mask)
{
   uint32_t req = 0;
   bool is_dir = S_ISDIR(inode->i_mode);

   /* access(F_OK) → need only attributes */
   if (mask & MAY_ACCESS) {
      req |= ACE4_READ_ATTRIBUTES;
   }

   /* READ */
   if (mask & MAY_READ) {
      req |= ACE4_READ_DATA;          // file: read(), dir: list entries
      req |= ACE4_READ_ATTRIBUTES;    // stat()
      req |= ACE4_READ_ACL;           // getfacl
      req |= ACE4_READ_NAMED_ATTRS;   // getxattr
   }

   /* WRITE */
   if (mask & MAY_WRITE) {
      if (is_dir) {
         req |= ACE4_WRITE_DATA;     // add file to dir
         req |= ACE4_APPEND_DATA;    // add subdir
         req |= ACE4_DELETE_CHILD;   // unlink entry
      } else {
         req |= ACE4_WRITE_DATA;     // overwrite
         req |= ACE4_APPEND_DATA;    // append
      }
      req |= ACE4_WRITE_ATTRIBUTES;   // chmod, utimes
      req |= ACE4_WRITE_NAMED_ATTRS;  // setxattr
   }

   /* APPEND */
   if (mask & MAY_APPEND) {
      req |= ACE4_APPEND_DATA;
   }

   /* EXECUTE */
   if (mask & MAY_EXEC) {
      req |= ACE4_EXECUTE;            // exec file or traverse dir
   }

   /* CHDIR (dir traversal) */
   if (mask & MAY_CHDIR) {
      req |= ACE4_EXECUTE;
   }

   /* Only map OPEN if there are no other flags */
   if (req == 0 && (mask & MAY_OPEN)) {
      if (is_dir)
         req |= ACE4_EXECUTE;        // need exec to open dir FD
      else
         req |= ACE4_READ_ATTRIBUTES; // need attrs to open file FD
   }

   return req;
}
