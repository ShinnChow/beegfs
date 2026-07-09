#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <common/Common.h>
#include <linux/byteorder/generic.h>
#ifdef KERNEL_HAS_LINUX_UNALIGNED_H
#include <linux/unaligned.h>
#else
#include <asm/unaligned.h>
#endif
#include "BeeGFSNFS4AclXdr.h"

/**
 * BeeGFSNFS4AclXdr_decode - Decode NFS4 ACL from XDR format into kernel structures
 * @buf: Pointer to the XDR-encoded ACL data buffer
 * @len: Length of the XDR buffer in bytes
 * @out: Pointer to store the allocated and parsed ACL structure
 *
 * Decodes an NFS4 ACL from XDR (External Data Representation) format into
 * kernel memory structures. The function allocates memory for the ACL and
 * its access control entries (ACEs). Ownership of the returned structure is
 * transferred to the caller, who must release it with BeeGFSNFS4AclXdr_free().
 *
 * Returns:
 *  0 on success (ACL successfully parsed and stored in @out)
 *  -EINVAL if the XDR data is malformed or contains invalid values
 *  -ENOMEM if memory allocation fails
 *  -EOVERFLOW if ACL size exceeds sanity limits
 */
int BeeGFSNFS4AclXdr_decode(const void *buf, size_t len, struct BeeGFSNFS4Acl **out)
{
   const void *p = buf;
   size_t rem = len;
   struct BeeGFSNFS4Acl *acl;
   uint32_t n, i;
   int ret;

   if (len < 4)
      return -EINVAL;

   if (len > BEEGFS_NFS4ACL_MAX_XDR)
      return -EOVERFLOW;

   n = get_unaligned_be32(p);
   p = (char *)p + 4; rem -= 4;

   if (n > BEEGFS_NFS4ACL_MAX_ACES)
      return -EOVERFLOW;

   acl = kzalloc(sizeof(*acl), GFP_KERNEL);
   if (!acl)
      return -ENOMEM;
   acl->naces = n;
   acl->aces = kcalloc(n, sizeof(*acl->aces), GFP_KERNEL);
   if (!acl->aces)
   {
      kfree(acl);
      return -ENOMEM;
   }

   for (i = 0; i < n; i++)
   {
      struct BeeGFSNFS4Ace *a = &acl->aces[i];
      uint32_t who_len;
      size_t who_pad;

      if (rem < 16) /* 4 * uint32_t */
         goto einval;
      a->type  = get_unaligned_be32(p); p = (char *)p + 4; rem -= 4;
      a->flags = get_unaligned_be32(p); p = (char *)p + 4; rem -= 4;
      a->mask  = get_unaligned_be32(p); p = (char *)p + 4; rem -= 4;
      who_len  = get_unaligned_be32(p); p = (char *)p + 4; rem -= 4;

      if (who_len > BEEGFS_NFS4ACL_MAX_WHO_LEN)
         goto eoverflow;
      if (rem < who_len)
         goto einval;

      a->who = kmemdup(p, who_len, GFP_KERNEL);

      if (!a->who)
         goto enomem;
      a->who_len = who_len;

      who_pad = ALIGN(who_len, 4) - who_len;
      if (rem < who_len + who_pad)
         goto einval;
      p = (char *)p + who_len + who_pad;
      rem -= who_len + who_pad;

      /* Optional: further sanity on type/flags/mask */
      if (a->type > ACE4_SYSTEM_ALARM_ACE_TYPE)
         goto einval;
   }

   *out = acl;
   return 0;
enomem:
   ret = -ENOMEM;
   goto err_free;
eoverflow:
   ret = -EOVERFLOW;
   goto err_free;
einval:
   ret = -EINVAL;
   goto err_free;
err_free:
   if (acl)
   {
      if (acl->aces)
      {
         for (i = 0; i < acl->naces; i++)
            kfree(acl->aces[i].who);

         kfree(acl->aces);
      }
      kfree(acl);
   }
   return ret;
}

void BeeGFSNFS4AclXdr_free(struct BeeGFSNFS4Acl *acl)
{
   uint32_t i;
   if (!acl)
      return;
   for (i = 0; i < acl->naces; i++)
      kfree(acl->aces[i].who);

   kfree(acl->aces);
   kfree(acl);
}

#ifdef BEEGFS_DEBUG
static const char *ace_type_str(uint32_t t)
{
   switch (t)
   {
      case ACE4_ACCESS_ALLOWED_ACE_TYPE: return "ALLOW";
      case ACE4_ACCESS_DENIED_ACE_TYPE:  return "DENY";
      default:                           return "OTHER";
   }
}

/*
 * Dump a decoded ACL to the kernel log. Debug helper, compiled only in
 * debug builds (make BEEGFS_DEBUG=1). Currently has no caller; retained
 * for interactive debugging of decoded ACLs.
 */
void BeeGFSNFS4AclXdr_print(const struct BeeGFSNFS4Acl *acl)
{
   uint32_t i;

   if (!acl)
   {
      printk_fhgfs(KERN_INFO, "nfs4_acl: (null)\n");
      return;
   }
   printk_fhgfs(KERN_INFO, "nfs4_acl: naces=%u\n", acl->naces);
   for (i = 0; i < acl->naces; i++)
   {
      const struct BeeGFSNFS4Ace *ace = &acl->aces[i];
      const char *t = ace_type_str(ace->type);
      const char *who_kind = NULL;
      char flags_buf[128] = {0};
      char perms_buf[256] = {0};
      struct {
         const struct BeeGFSNFS4AceLabel *tbl; size_t n; uint32_t val; char *out; size_t outlen;
      } parts[2] = {
         { aceFlagNames, ARRAY_SIZE(aceFlagNames), ace->flags, flags_buf, sizeof(flags_buf) },
         { acePermNames, ARRAY_SIZE(acePermNames), ace->mask,  perms_buf, sizeof(perms_buf) },
      };
      /* Very small local printer to into buffers */
      for (int p = 0; p < 2; p++)
      {
         size_t j; bool first = true; size_t off = 0;
         for (j = 0; j < parts[p].n; j++)
         {
            if (parts[p].val & parts[p].tbl[j].bit)
            {
               off += scnprintf(parts[p].out + off, parts[p].outlen - off,
                  "%s%s", first ? "" : "|", parts[p].tbl[j].name);
               first = false;
               parts[p].val &= ~parts[p].tbl[j].bit;
            }
         }
         if (!first && parts[p].val)
            scnprintf(parts[p].out + off, parts[p].outlen - off, "|0x%x", parts[p].val);
         else if (first)
            scnprintf(parts[p].out + off, parts[p].outlen - off, "0");
      }

      if (who_is_owner(ace->who, ace->who_len))
         who_kind = "OWNER@";
      else if (who_is_group(ace->who, ace->who_len))
         who_kind = "GROUP@";
      else if (who_is_everyone(ace->who, ace->who_len))
         who_kind = "EVERYONE@";

      if (who_kind)
         printk_fhgfs(KERN_INFO, "[%u] type=%s flags=0x%x [%s] mask=0x%x [%s] who=%s\n",
            i, t, ace->flags, flags_buf, ace->mask, perms_buf, who_kind);
      else
         printk_fhgfs(KERN_INFO, "[%u] type=%s flags=0x%x [%s] mask=0x%x [%s] who=%.*s\n",
            i, t, ace->flags, flags_buf, ace->mask, perms_buf, ace->who_len, ace->who);
   }
}
#endif //BEEGFS_DEBUG
