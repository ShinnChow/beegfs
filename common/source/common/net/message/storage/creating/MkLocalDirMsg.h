#pragma once

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/RemoteStorageTarget.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>

#define MKLOCALDIRMSG_COMPATFLAG_HAS_NFS4_ACL 0x01

class MkLocalDirMsg : public MirroredMessageBase<MkLocalDirMsg>
{
   friend class AbstractNetMessageFactory;
   friend class TestSerialization;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkLocalDirMsg(EntryInfo* entryInfo, unsigned userID, unsigned groupID, int mode,
         StripePattern* pattern, RemoteStorageTarget* rst, NumNodeID parentNodeID,
         const CharVector& defaultACLXAttr, const CharVector& accessACLXAttr,
         const CharVector& nfs4ACLXAttr) :
            BaseType(NETMSGTYPE_MkLocalDir),
            defaultACLXAttr(defaultACLXAttr), accessACLXAttr(accessACLXAttr), nfs4ACLXAttr(nfs4ACLXAttr)
      {
         this->entryInfoPtr = entryInfo;
         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->pattern = pattern;
         this->rstPtr = rst;
         this->parentNodeID = parentNodeID;
      }

      /**
       * For deserialization only!
       */
      MkLocalDirMsg() : BaseType(NETMSGTYPE_MkLocalDir) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->userID
            % obj->groupID
            % obj->mode
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
            % serdes::backedPtr(obj->rstPtr, obj->rst)
            % obj->parentNodeID
            % obj->defaultACLXAttr
            % obj->accessACLXAttr;

         if (obj->isMsgHeaderCompatFeatureFlagSet(MKLOCALDIRMSG_COMPATFLAG_HAS_NFS4_ACL))
            ctx % obj->nfs4ACLXAttr;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->dirTimestamps;
      }

      bool supportsMirroring() const { return true; }

   private:
      uint32_t userID;
      uint32_t groupID;
      int32_t mode;
      NumNodeID parentNodeID;

      // for serialization
      EntryInfo* entryInfoPtr;      // not owned by this object!
      StripePattern* pattern;       // not owned by this object!
      RemoteStorageTarget* rstPtr;  // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
      RemoteStorageTarget rst;
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;

      // ACLs
      CharVector defaultACLXAttr;
      CharVector accessACLXAttr;
      CharVector nfs4ACLXAttr;

   protected:
      MirroredTimestamps dirTimestamps;

   public:
      StripePattern& getPattern()
      {
         return *pattern;
      }

      void setPattern(StripePattern* pattern)
      {
         this->pattern = pattern;
      }

      // getters & setters
      RemoteStorageTarget* getRemoteStorageTarget()
      {
         return &this->rst;
      }

      unsigned getUserID() const
      {
         return userID;
      }

      unsigned getGroupID() const
      {
         return groupID;
      }

      int getMode() const
      {
         return mode;
      }

      NumNodeID getParentNodeID() const
      {
         return this->parentNodeID;
      }

      EntryInfo* getEntryInfo()
      {
         return this->entryInfoPtr;
      }

      const CharVector& getDefaultACLXAttr() const
      {
         return this->defaultACLXAttr;
      }

      const CharVector& getAccessACLXAttr() const
      {
         return this->accessACLXAttr;
      }

      const CharVector& getNfs4ACLXAttr() const
      {
         return this->nfs4ACLXAttr;
      }

      void setDirTimestamps(const MirroredTimestamps& ts) { dirTimestamps = ts; }
};

