#ifndef BEEGFSNFS4ACLXDR_H_
#define BEEGFSNFS4ACLXDR_H_

#include <linux/types.h>
#include <linux/string.h>

/* Max size of a serialized NFS4 ACL blob. 32 KB fits any realistic ACL. */
#define BEEGFS_NFS4ACL_MAX_XDR     (32 * 1024)

/* Max ACE count accepted when decoding. Guards against malformed blobs
 * that would otherwise drive a huge allocation. */
#define BEEGFS_NFS4ACL_MAX_ACES    4096

/* Max length of a single 'who' principal. Matches the 256-byte resolver desc/
 * payload buffers; longer principals cannot be resolved. */
#define BEEGFS_NFS4ACL_MAX_WHO_LEN 256

enum BeeGFSNFS4AceType
{
    ACE4_ACCESS_ALLOWED_ACE_TYPE = 0,
    ACE4_ACCESS_DENIED_ACE_TYPE  = 1,
    ACE4_SYSTEM_AUDIT_ACE_TYPE   = 2,
    ACE4_SYSTEM_ALARM_ACE_TYPE   = 3,
};

enum BeeGFSNFS4AceFlag
{
    ACE4_FILE_INHERIT_ACE        = 0x00000001,
    ACE4_DIRECTORY_INHERIT_ACE   = 0x00000002,
    ACE4_NO_PROPAGATE_INHERIT_ACE= 0x00000004,
    ACE4_INHERIT_ONLY_ACE        = 0x00000008,
    ACE4_SUCCESSFUL_ACCESS_ACE_FLAG = 0x00000010,
    ACE4_FAILED_ACCESS_ACE_FLAG     = 0x00000020,
    ACE4_IDENTIFIER_GROUP        = 0x00000040,
    /*
     * ACE4_OWNER/GROUP/EVERYONE are NOT RFC 7530 wire flags. Special
     * principals are matched by string instead (who_is_owner/group/
     * everyone()). These are currently unused */
    ACE4_OWNER                   = 0x00000100,
    ACE4_GROUP                   = 0x00000200,
    ACE4_EVERYONE                = 0x00000400,
};

enum BeeGFSNFS4AceMask
{
    ACE4_READ_DATA               = 0x00000001,
    ACE4_WRITE_DATA              = 0x00000002,
    ACE4_APPEND_DATA             = 0x00000004,
    ACE4_READ_NAMED_ATTRS        = 0x00000008,
    ACE4_WRITE_NAMED_ATTRS       = 0x00000010,
    ACE4_EXECUTE                 = 0x00000020,
    ACE4_DELETE_CHILD            = 0x00000040,
    ACE4_READ_ATTRIBUTES         = 0x00000080,
    ACE4_WRITE_ATTRIBUTES        = 0x00000100,
    ACE4_DELETE                  = 0x00010000,
    ACE4_READ_ACL                = 0x00020000,
    ACE4_WRITE_ACL               = 0x00040000,
    ACE4_WRITE_OWNER             = 0x00080000,
    ACE4_SYNCHRONIZE             = 0x00100000,
};

struct BeeGFSNFS4AceLabel
{
   uint32_t bit;
   const char *name;
};

static const struct BeeGFSNFS4AceLabel aceFlagNames[] =
{
    { ACE4_FILE_INHERIT_ACE,        "FILE_INHERIT" },
    { ACE4_DIRECTORY_INHERIT_ACE,   "DIR_INHERIT" },
    { ACE4_NO_PROPAGATE_INHERIT_ACE,"NO_PROPAGATE" },
    { ACE4_INHERIT_ONLY_ACE,        "INHERIT_ONLY" },
    { ACE4_IDENTIFIER_GROUP,        "IDENTIFIER_GROUP" },
    // { ACE4_INHERITED_ACE,           "INHERITED" },
};

static const struct BeeGFSNFS4AceLabel acePermNames[] =
{
    { ACE4_READ_DATA,         "READ_DATA" },
    { ACE4_WRITE_DATA,        "WRITE_DATA" },
    { ACE4_APPEND_DATA,       "APPEND_DATA" },
    { ACE4_READ_NAMED_ATTRS,  "READ_NAMED_ATTRS" },
    { ACE4_WRITE_NAMED_ATTRS, "WRITE_NAMED_ATTRS" },
    { ACE4_EXECUTE,           "EXECUTE" },
    { ACE4_DELETE_CHILD,      "DELETE_CHILD" },
    { ACE4_READ_ATTRIBUTES,   "READ_ATTRS" },
    { ACE4_WRITE_ATTRIBUTES,  "WRITE_ATTRS" },
    { ACE4_DELETE,            "DELETE" },
    { ACE4_READ_ACL,          "READ_ACL" },
    { ACE4_WRITE_ACL,         "WRITE_ACL" },
    { ACE4_WRITE_OWNER,       "WRITE_OWNER" },
    { ACE4_SYNCHRONIZE,       "SYNCHRONIZE" },
};

struct BeeGFSNFS4Ace
{
   uint32_t type;
   uint32_t flags;
   uint32_t mask;
   /* NFS4 'who' principal in kernel memory (not XDR): */
   uint32_t who_len;
   char *who; /* kmalloc'd, not NUL-terminated necessarily */
};

struct BeeGFSNFS4Acl
{
    uint32_t naces;
    struct BeeGFSNFS4Ace *aces; /* naces elements */
};

int BeeGFSNFS4AclXdr_decode(const void *buf, size_t len, struct BeeGFSNFS4Acl **out);
void BeeGFSNFS4AclXdr_free(struct BeeGFSNFS4Acl *acl);
#ifdef BEEGFS_DEBUG
void BeeGFSNFS4AclXdr_print(const struct BeeGFSNFS4Acl *acl);
#endif

static inline bool who_is_everyone(const char *s, uint32_t n)
{ return n == 9 && !memcmp(s, "EVERYONE@", 9); }

static inline bool who_is_owner(const char *s, uint32_t n)
{ return n == 6 && !memcmp(s, "OWNER@", 6); }

static inline bool who_is_group(const char *s, uint32_t n)
{ return n == 6 && !memcmp(s, "GROUP@", 6); }

#endif /* BEEGFSNFS4ACLXDR_H_ */
