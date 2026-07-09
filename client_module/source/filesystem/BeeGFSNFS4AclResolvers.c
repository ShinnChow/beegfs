#include <linux/key.h>
#include <linux/keyctl.h>
#ifdef KERNEL_HAS_LINUX_UNALIGNED_H
  #include <linux/unaligned.h>
#else
  #include <asm/unaligned.h>
#endif

#include <keys/user-type.h>

#include "common/Common.h"
#include "BeeGFSNFS4AclResolvers.h"

/*
 * Weak references to the kernel NFS4 idmapper. These resolve name@domain
 * principals to kuid/kgid. The nfsidmap-based resolver that used them was
 * removed, so they are currently unreferenced. Kept as the integration
 * point should a name@domain resolver be reintroduced.
 */
__attribute__((weak)) extern kuid_t nfs_map_name_to_uid(struct user_namespace *user_ns,
                                                        const char *name, size_t namelen);
__attribute__((weak)) extern kgid_t nfs_map_name_to_gid(struct user_namespace *user_ns,
                                                        const char *name, size_t namelen);

/* Initialize a trivial resolver.
 *
 * Wires up the user and group resolvers and sets the resolver to a ready state.
 */
int BeeGFSNFS4Acl_initTrivialResolver(struct BeeGFSNFS4AclPrincipalResolver *r) {
   r->resolve_user = BeeGFSNFS4Acl_resolveUserTrivial;
   r->resolve_group = BeeGFSNFS4Acl_resolveGroupTrivial;
   r->state = RESOLVER_READY;

   return 0;
}

/* Translate a string principal that only contains a numeric ID into a kernel UID.
 *
 * Handles principals like "1000" or "1234" that do not have the GROUP_IDENTIFIER flag set.
 * Returns either a valid kuid_t or INVALID_UID.
 */
kuid_t BeeGFSNFS4Acl_resolveUserTrivial(struct user_namespace *user_ns,
                                                  const char *who, size_t len) {
   unsigned long id;
   char who_num[32]; /* big enough for "18446744073709551615" + NUL */
   size_t copy_len = min(len, sizeof(who_num) - 1);
   memcpy(who_num, who, copy_len);
   who_num[copy_len] = '\0';
   printk_fhgfs_debug(KERN_DEBUG, "principal: numeric id string: %s", who_num);

   if (!kstrtoul(who_num, 10, &id)) {
      return make_kuid(user_ns, id);
   } else {
      return INVALID_UID;
   }
}

/* Translate a string principal that only contains a numeric ID into a kernel GID.
 *
 * Handles principals like "1000" or "1234" that have the GROUP_IDENTIFIER flag set.
 * Returns either a valid kgid_t or INVALID_GID.
 */
kgid_t BeeGFSNFS4Acl_resolveGroupTrivial(struct user_namespace *user_ns,
                                                  const char *who, size_t len) {
   unsigned long id;
   char who_num[32]; /* big enough for "18446744073709551615" + NUL */
   size_t copy_len = min(len, sizeof(who_num) - 1);
   memcpy(who_num, who, copy_len);
   who_num[copy_len] = '\0';
   printk_fhgfs_debug(KERN_DEBUG, "principal: numeric id string: %s", who_num);

   if (!kstrtoul(who_num, 10, &id)) {
      return make_kgid(user_ns, id);
   } else {
      return INVALID_GID;
   }
}

/* Initialize a SID resolver.
 *
 * Wires up the user and group resolvers and sets the resolver to a ready state.
 */
int BeeGFSNFS4Acl_initSidResolver(struct BeeGFSNFS4AclPrincipalResolver *r) {
   r->resolve_user = BeeGFSNFS4Acl_resolveUserSid;
   r->resolve_group = BeeGFSNFS4Acl_resolveGroupSid;
   r->state = RESOLVER_READY;

   return 0;
}

/* Translate an AD SID string principal (e.g. "S-1-5-21-...") into a kernel UID.
 *
 * Called for user principals, i.e. ACEs whose ACE4_IDENTIFIER_GROUP flag is
 * NOT set. Returns either a valid kuid_t or INVALID_UID.
 */
kuid_t BeeGFSNFS4Acl_resolveUserSid(struct user_namespace *user_ns,
                                         const char *who, size_t len) {
    struct key *key;
    char desc[256];
    long payload_len;
    char payload[256];
    uid_t uid;
    kuid_t out_kuid;
#ifdef BEEGFS_DEBUG
    char *data;
#endif

    /*
     * request_key() expects a NUL-terminated description string, while @who is
     * length-delimited ACL data and is not guaranteed to be NUL-terminated.
     */
    if (scnprintf(desc, sizeof(desc), "os:%.*s", (int)len, who) >= sizeof(desc))
        return INVALID_UID;

    key = request_key(&key_type_logon, desc, "");
    if (IS_ERR(key))
        return INVALID_UID;

    rcu_read_lock();
    payload_len = user_read(key, payload, 256);
    rcu_read_unlock();

#ifdef BEEGFS_DEBUG
    data = kzalloc(1024 * sizeof(char), GFP_KERNEL);
    if (data != NULL) {
      if (payload_len > 0 && payload_len <= (long) sizeof(payload)) {
        for (size_t i = 0; i < (size_t) payload_len; i++)
          sprintf(data + i * 2, "%02x", (uint8_t)payload[i]);
        data[payload_len*2] = 0;
        printk_fhgfs_debug(KERN_DEBUG, "principal: SID: got payload: %s, payload_len: %ld", data, payload_len);
      }
      kfree(data);
    }
#endif
    if (payload_len == 4) {
      uid = get_unaligned_le32(payload);
      printk_fhgfs_debug(KERN_DEBUG, "principal: SID: converted UID payload to UID: %d", uid);
    } else {
      printk_fhgfs(KERN_WARNING, "principal: SID: resolver returned UID that is not four bytes long");
      key_put(key);
      return INVALID_UID;
    }

    /* Map the raw uid_t into a kuid_t relative to passed in user namespace */
    out_kuid = make_kuid(user_ns, uid);
    if (!uid_valid(out_kuid)) {
        key_put(key);
        return INVALID_UID;
    }

    key_put(key);
    return out_kuid;
}

/* Translate an AD SID string principal (e.g. "S-1-5-21-...") into a kernel GID.
 *
 * Called for group principals, i.e. ACEs whose ACE4_IDENTIFIER_GROUP flag IS
 * set. Returns either a valid kgid_t or INVALID_GID.
 */
kgid_t BeeGFSNFS4Acl_resolveGroupSid(struct user_namespace *user_ns,
                                         const char *who, size_t len) {
    struct key *key;
    char desc[256];
    long payload_len;
    char payload[256];
    gid_t gid;
    kgid_t out_kgid;
#ifdef BEEGFS_DEBUG
    char *data;
#endif

    /*
     * request_key() expects a NUL-terminated description string, while @who is
     * length-delimited ACL data and is not guaranteed to be NUL-terminated.
     */
    if (scnprintf(desc, sizeof(desc), "gs:%.*s", (int)len, who) >= sizeof(desc))
        return INVALID_GID;

    key = request_key(&key_type_logon, desc, "");
    if (IS_ERR(key))
        return INVALID_GID;

    rcu_read_lock();
    payload_len = user_read(key, payload, 256);
    rcu_read_unlock();

#ifdef BEEGFS_DEBUG
    data = kzalloc(1024 * sizeof(char), GFP_KERNEL);
    if (data != NULL) {
      if (payload_len > 0 && payload_len <= (long) sizeof(payload)) {
        for (size_t i = 0; i < (size_t) payload_len; i++)
          sprintf(data + i * 2, "%02x", (uint8_t)payload[i]);
        data[payload_len*2] = 0;
        printk_fhgfs_debug(KERN_DEBUG, "principal: SID: got payload: %s, payload_len: %ld", data, payload_len);
      }
      kfree(data);
    }
#endif
    if (payload_len == 4) {
      gid = get_unaligned_le32(payload);
      printk_fhgfs_debug(KERN_DEBUG, "principal: SID: converted GID payload to GID: %d", gid);
    } else {
      printk_fhgfs(KERN_WARNING, "principal: SID: resolver returned GID that is not four bytes long");
      key_put(key);
      return INVALID_GID;
    }

    /* Map the raw gid_t into a kgid_t relative to passed in user namespace */
    out_kgid = make_kgid(user_ns, gid);
    if (!gid_valid(out_kgid)) {
        key_put(key);
        return INVALID_GID;
    }

    key_put(key);
    return out_kgid;
}
