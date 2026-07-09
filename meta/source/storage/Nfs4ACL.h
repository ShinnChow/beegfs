#pragma once

#include <common/Common.h>

// Sanity caps on untrusted client ACL blobs. Keep in sync with the kernel client
// decoder (BeeGFSNFS4AclXdr.h: BEEGFS_NFS4ACL_MAX_*). who_len matches the 256-byte
// resolver buffer there — longer principals are unresolvable anyway.
static constexpr uint32_t NFS4_ACL_MAX_ACES    = 4096;
static constexpr uint32_t NFS4_ACL_MAX_WHO_LEN = 256;
static constexpr size_t   NFS4_ACL_MAX_XDR     = 32 * 1024; // total serialized blob

struct Nfs4ACE
{
   // NFSv4 ACE Types (RFC 3530)
   enum AceType
   {
      ACCESS_ALLOWED_ACE_TYPE = 0x00000000,
      ACCESS_DENIED_ACE_TYPE  = 0x00000001,
      SYSTEM_AUDIT_ACE_TYPE   = 0x00000002,
      SYSTEM_ALARM_ACE_TYPE   = 0x00000003
   };

   // NFSv4 ACE Flags (RFC 3530)
   enum AceFlags
   {
      FILE_INHERIT_ACE        = 0x00000001,
      DIRECTORY_INHERIT_ACE   = 0x00000002,
      NO_PROPAGATE_INHERIT_ACE= 0x00000004,
      INHERIT_ONLY_ACE        = 0x00000008,
      SUCCESSFUL_ACCESS_ACE   = 0x00000010,
      FAILED_ACCESS_ACE       = 0x00000020,
      IDENTIFIER_GROUP        = 0x00000040,
      INHERITED_ACE           = 0x00000080
   };

   // NFSv4 Access Mask bits (RFC 3530)
   enum AccessMask
   {
      // Basic file/directory operations
      READ_DATA         = 0x00000001,
      LIST_DIRECTORY    = 0x00000001,  // Same as READ_DATA for directories
      WRITE_DATA        = 0x00000002,
      ADD_FILE          = 0x00000002,  // Same as WRITE_DATA for directories
      APPEND_DATA       = 0x00000004,
      ADD_SUBDIRECTORY  = 0x00000004,  // Same as APPEND_DATA for directories

      // Named attributes
      READ_NAMED_ATTRS  = 0x00000008,
      WRITE_NAMED_ATTRS = 0x00000010,

      // Execute and directory operations
      EXECUTE           = 0x00000020,
      DELETE_CHILD      = 0x00000040,

      // File attributes
      READ_ATTRIBUTES   = 0x00000080,
      WRITE_ATTRIBUTES  = 0x00000100,

      // 0x00000200 to 0x00008000 reserved in RFC 3530

      // Object control
      DELETE            = 0x00010000,
      READ_ACL          = 0x00020000,
      WRITE_ACL         = 0x00040000,
      WRITE_OWNER       = 0x00080000,
      SYNCHRONIZE       = 0x00100000
   };

   uint32_t type;        // ACE type
   uint32_t flags;       // ACE flags
   uint32_t mask;        // Access mask
   uint32_t who_len;     // Length of who string
   std::string who;      // Principal (user/group name)

   Nfs4ACE() : type(0), flags(0), mask(0), who_len(0) {}
   Nfs4ACE(uint32_t aceType, uint32_t aceFlags, uint32_t accessMask, const std::string& principal)
      : type(aceType), flags(aceFlags), mask(accessMask), who_len(principal.length()), who(principal) {}

   void setWho(const std::string& principal)
   {
      who = principal;
      who_len = who.length();
   }

   // Check if this ACE has inheritance flags going to be used for new files/directories
   bool hasFileInheritance() const { return flags & FILE_INHERIT_ACE; }
   bool hasDirectoryInheritance() const { return flags & DIRECTORY_INHERIT_ACE; }
   bool hasInheritance() const { return hasFileInheritance() || hasDirectoryInheritance(); }
   bool isInheritOnly() const { return flags & INHERIT_ONLY_ACE; }
   bool isInherited() const { return flags & INHERITED_ACE; }
};

typedef std::vector<Nfs4ACE> Nfs4ACEVec;
typedef Nfs4ACEVec::const_iterator Nfs4ACEVecCIter;
typedef Nfs4ACEVec::iterator Nfs4ACEVecIter;

class Nfs4ACL
{
   public:
      bool deserializeXAttr(const CharVector& xattr);
      void serializeXAttr(CharVector& xattr) const;

      std::string toString() const;
      bool empty() const { return aces.empty(); }
      size_t size() const { return aces.size(); }

      // Access to ACEs
      const Nfs4ACEVec& getACEs() const { return aces; }
      void addACE(const Nfs4ACE& ace) { aces.push_back(ace); }
      void clear() { aces.clear(); }

      // Inheritance processing for new files/directories
      Nfs4ACL createInheritedACL(bool isDirectory) const;
      static const std::string nfs4ACLXAttrName;

   private:
      Nfs4ACEVec aces;
};
