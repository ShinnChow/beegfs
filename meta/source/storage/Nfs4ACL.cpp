#include <program/Program.h>
#include <sstream>
#include <cstring>
#include <common/toolkit/serialization/BigEndianSerDes.h>

#include "Nfs4ACL.h"

const std::string Nfs4ACL::nfs4ACLXAttrName = "system.nfs4_acl";

/**
 * Deserialize NFSv4 ACL data
 *
 * @param xattr serialized form of the NFSv4 ACL in XDR format.
 * @return true on success, false on error.
 *
 * XDR Format:
 * - uint32_t naces (number of ACEs)
 * - For each ACE:
 *   - uint32_t type
 *   - uint32_t flags
 *   - uint32_t mask
 *   - uint32_t who_len
 *   - char who[who_len] (not null-terminated)
 */
bool Nfs4ACL::deserializeXAttr(const CharVector& xattr)
{
   if (xattr.size() < sizeof(uint32_t))
      return false;

   if (xattr.size() > NFS4_ACL_MAX_XDR)
      return false;

   // Use unified big-endian deserializer for XDR format
   const char* constData = &xattr[0];
   BigEndianSerDes des(constData, xattr.size());

   uint32_t naces = 0;
   des % naces;

   if (!des.good())
      return false;

   if (naces > NFS4_ACL_MAX_ACES)
      return false;

   aces.clear();

   for (uint32_t i = 0; i < naces && des.good(); i++)
   {
      Nfs4ACE newAce;

      des % newAce.type;
      des % newAce.flags;
      des % newAce.mask;
      des % newAce.who_len;

      if (!des.good())
         return false;

      if (newAce.who_len > NFS4_ACL_MAX_WHO_LEN)
         return false;

      if (newAce.who_len > 0)
      {
         des.processString(newAce.who, newAce.who_len);
      }

      if (!des.good())
         return false;

      aces.push_back(std::move(newAce));
   }

   return des.good() && aces.size() == naces;
}

/**
 * Serialize the NFSv4 ACL into XDR format for storage as an extended attribute.
 * @param xattr output vector to store the serialized ACL
 */
void Nfs4ACL::serializeXAttr(CharVector& xattr) const
{
   xattr.clear();

   // Calculate size first, then serialize
   for (int pass = 0; pass < 2; pass++)
   {
      BigEndianSerDes ser(pass == 0 ? nullptr : &xattr[0], pass == 0 ? SIZE_MAX : xattr.size());

      uint32_t naces = static_cast<uint32_t>(aces.size());
      ser % naces;

      for (Nfs4ACEVecCIter aceIt = aces.begin(); aceIt != aces.end(); ++aceIt)
      {
         uint32_t type = aceIt->type;
         uint32_t flags = aceIt->flags;
         uint32_t mask = aceIt->mask;
         uint32_t who_len = aceIt->who_len;

         ser % type;
         ser % flags;
         ser % mask;
         ser % who_len;

         if (aceIt->who_len > 0)
         {
            std::string who = aceIt->who; // Copy for non-const reference
            ser.processString(who, aceIt->who_len);
         }
      }

      if (pass == 0)
         xattr.resize(ser.size());
   }
}

/**
 * Create an inherited ACL for a new file or directory based on this ACL's inheritable ACEs.
 * This method implements the complete NFSv4 inheritance logic:
 * - FILE_INHERIT_ACE: Inherits to files and directories
 * - DIRECTORY_INHERIT_ACE: Inherits only to directories
 * - NO_PROPAGATE_INHERIT_ACE: Inheritance stops after one level
 * - INHERIT_ONLY_ACE: ACE only applies to children, not the directory itself
 * - INHERITED_ACE: Marks ACEs that were inherited from parent
 *
 * For files: ALL inheritance flags are removed since files cannot have children
 * For directories: Inheritance flags are preserved to allow further propagation
 *
 * @param isDirectory true if the new entry is a directory
 * @return new ACL with properly processed inherited ACEs
 */
Nfs4ACL Nfs4ACL::createInheritedACL(bool isDirectory) const
{
   Nfs4ACL inheritedACL;

   for (const Nfs4ACE& ace : aces)
   {
      // Skip ACEs that have no inheritance flags
      if (!ace.hasInheritance())
         continue;

      bool shouldInherit = false;

      if (isDirectory)
      {
         // Directories inherit either flavor: DIRECTORY_INHERIT applies to the new
         // dir; FILE_INHERIT is carried forward (INHERIT_ONLY) for files within.
         shouldInherit = ace.hasDirectoryInheritance() || ace.hasFileInheritance();
      }
      else
      {
         // Files inherit ACEs with FILE_INHERIT_ACE flag (regardless of other flags)
         shouldInherit = ace.hasFileInheritance();
      }

      if (shouldInherit)
      {
         Nfs4ACE inheritedAce = ace;

         // Mark as inherited (required by NFSv4 spec)
         inheritedAce.flags |= Nfs4ACE::INHERITED_ACE;

         // Handle NO_PROPAGATE_INHERIT_ACE flag
         if (ace.flags & Nfs4ACE::NO_PROPAGATE_INHERIT_ACE)
         {
            // A FILE_INHERIT-only ACE on a new directory only targets files one level
            // down; NO_PROPAGATE stops that, so it is not carried onto the directory.
            if (isDirectory && !ace.hasDirectoryInheritance())
               continue;

            // Strip all inheritance flags so it doesn't propagate further
            inheritedAce.flags &= ~(Nfs4ACE::FILE_INHERIT_ACE |
                                    Nfs4ACE::DIRECTORY_INHERIT_ACE |
                                    Nfs4ACE::NO_PROPAGATE_INHERIT_ACE |
                                    Nfs4ACE::INHERIT_ONLY_ACE);
         }
         else
         {
            // Normal inheritance - preserve inheritance flags for further propagation
            // But for files, strip ALL inheritance flags (as per RFC 3530) since files
            // can't have children
            if (!isDirectory)
            {
               inheritedAce.flags &= ~(Nfs4ACE::FILE_INHERIT_ACE |
                                      Nfs4ACE::DIRECTORY_INHERIT_ACE |
                                      Nfs4ACE::NO_PROPAGATE_INHERIT_ACE |
                                      Nfs4ACE::INHERIT_ONLY_ACE);
            }
         }

         if (isDirectory && !ace.hasDirectoryInheritance() &&
             !(ace.flags & Nfs4ACE::NO_PROPAGATE_INHERIT_ACE))
         {
            // FILE_INHERIT-only ACE on a new directory: must NOT apply to the directory
            // itself, but must keep propagating to files created within it.
            inheritedAce.flags |= Nfs4ACE::INHERIT_ONLY_ACE;
         }
         else
         {
            // Inherit-only ACEs on the parent become effective on the child (RFC 5661).
            inheritedAce.flags &= ~Nfs4ACE::INHERIT_ONLY_ACE;
         }

         inheritedACL.addACE(inheritedAce);
      }
   }

   return inheritedACL;
}

/**
 * Convert ACL to string representation for debugging.
 * @return string representation of the ACL
 */
std::string Nfs4ACL::toString() const
{
   std::ostringstream ostr;
   ostr << "NFSv4 ACL Size: " << aces.size() << std::endl;
   for (Nfs4ACEVecCIter it = aces.begin(); it != aces.end(); ++it)
   {
      ostr << "ACE[ ";
      ostr << "type: 0x" << std::hex << it->type << std::dec;
      ostr << " flags: 0x" << std::hex << it->flags << std::dec;
      ostr << " mask: 0x" << std::hex << it->mask << std::dec;
      ostr << " who_len: " << it->who_len;
      ostr << " who: \"" << it->who << "\"";
      ostr << " ]" << std::endl;
   }
   return ostr.str();
}