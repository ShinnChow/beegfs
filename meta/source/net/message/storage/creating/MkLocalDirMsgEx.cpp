#include <common/net/message/control/GenericResponseMsg.h>
#include <program/Program.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>
#include <storage/Nfs4ACL.h>

#include "MkLocalDirMsgEx.h"

HashDirLock MkLocalDirMsgEx::lock(EntryLockStore& store)
{
   // we usually need not lock anything here, because the inode ID will be completely unknown to
   // anyone until we finish processing here *and* on the metadata server that sent this message.
   // during resync though we need to lock the hash dir to avoid interefence between bulk resync and
   // mod resync.

   // do not lock the hash dir if we are creating the inode on the same meta node as the dentry,
   // MkDir will have already locked the hash dir.
   if (!rctx->isLocallyGenerated() && resyncJob && resyncJob->isRunning())
      return {&store, MetaStorageTk::getMetaInodeHash(getEntryInfo()->getEntryID())};

   return {};
}

bool MkLocalDirMsgEx::processIncoming(ResponseContext& ctx)
{
   EntryInfo* entryInfo = getEntryInfo();

   LOG_DBG(GENERAL, DEBUG, "", entryInfo->getEntryID(), entryInfo->getFileName());
   (void) entryInfo;

   rctx = &ctx;

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> MkLocalDirMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   StripePattern& pattern = getPattern();

   RemoteStorageTarget* rstInfo = getRemoteStorageTarget();

   EntryInfo *entryInfo = getEntryInfo();
   NumNodeID parentNodeID = getParentNodeID();

   NumNodeID ownerNodeID = entryInfo->getIsBuddyMirrored()
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() )
      : app->getLocalNode().getNumID();

   DirInode newDir(entryInfo->getEntryID(), getMode(), getUserID(),
      getGroupID(), ownerNodeID, pattern, entryInfo->getIsBuddyMirrored());

   newDir.setParentInfoInitial(entryInfo->getParentEntryID(), parentNodeID);

   FhgfsOpsErr mkRes = metaStore->makeDirInode(newDir, getDefaultACLXAttr(), getAccessACLXAttr() );

   // apply inherited NFSv4 ACL (independent of POSIX ACLs)
   if (!getNfs4ACLXAttr().empty() && (mkRes == FhgfsOpsErr_SUCCESS))
   {
      FhgfsOpsErr setNfs4XAttrRes = newDir.setXAttr(nullptr, Nfs4ACL::nfs4ACLXAttrName,
                                                   getNfs4ACLXAttr(), 0);

      if (setNfs4XAttrRes != FhgfsOpsErr_SUCCESS)
      {
         // Note: File/Directory creation continues despite ACL inheritance failures -
         // this follows established NFS server practices and POSIX semantics:
         //
         // RATIONALE:
         // 1. RFC 3530 (NFSv4) uses "SHOULD" not "MUST" for ACL inheritance, indicating
         //    it's a best-effort enhancement, not a blocking requirement.
         // 2. POSIX create() semantics prioritize availability - if a user can create
         //    files in a directory, that operation should succeed regardless of
         //    extended attribute failures.
         // 3. Real NFS implementations (Linux knfsd, FreeBSD, Solaris) follow this
         //    pattern to avoid cascading failures in legitimate workflows.
         LogContext("MkLocalDir").log(Log_WARNING, "Failed to apply inherited NFSv4 ACL to directory " +
            newDir.getID());
      }
   }

   if (!rstInfo->hasInvalidVersion() && (mkRes == FhgfsOpsErr_SUCCESS))
   {
      FhgfsOpsErr setRstRes = newDir.setRemoteStorageTarget(*rstInfo);
      if (setRstRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext("MkLocalDir").log(Log_WARNING, "Failed to set remote storage targets for "
            "dirID: " + newDir.getID() + ". RST might be invalid.");
      }
   }

   if (mkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(newDir, dirTimestamps);

   return boost::make_unique<ResponseState>(mkRes);
}

void MkLocalDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_MkLocalDirResp);
}
