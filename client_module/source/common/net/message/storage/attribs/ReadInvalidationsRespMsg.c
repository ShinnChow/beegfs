#include <common/toolkit/Serialization.h>
#include <common/Common.h>
#include "ReadInvalidationsRespMsg.h"

static void ReadInvalidationsRespMsg_release(NetMessage* this)
{
   ReadInvalidationsRespMsg* thisCast = (ReadInvalidationsRespMsg*)this;

   kfree(thisCast->inodeIds);
   thisCast->inodeIds = NULL;
}

static const struct NetMessageOps ReadInvalidationsRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = ReadInvalidationsRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .release = ReadInvalidationsRespMsg_release,
};

void ReadInvalidationsRespMsg_init(ReadInvalidationsRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ReadInvalidationsResp, &ReadInvalidationsRespMsg_Ops);
}

bool ReadInvalidationsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   ReadInvalidationsRespMsg* thisCast = (ReadInvalidationsRespMsg*)this;

   // returnCode
   if (!Serialization_deserializeUInt(ctx, &thisCast->errorFlags) )
      return false;

   // numUpdates
   if (!Serialization_deserializeUInt(ctx, &thisCast->numUpdates) )
      return false;

   // Reject an implausible count before using it to size the allocation below. The meta
   // never sends more than READINVALIDATIONS_MAX_IDS, so anything larger is a corrupt or
   // hostile message; bailing here avoids an attacker-controlled kmalloc.
   if (thisCast->numUpdates > READINVALIDATIONS_MAX_IDS)
      return false;

   // readPos
   if (!Serialization_deserializeUInt(ctx, &thisCast->readPos) )
      return false;
   
   { // inode entryID list
      uint32_t i;

      thisCast->inodeIds = beegfs_kmalloc(InodeId, thisCast->numUpdates, GFP_NOFS);
      if (thisCast->inodeIds == NULL) {
          return false; // Memory allocation failed
      }
      for (i = 0; i < thisCast->numUpdates; i++)
      {
         InodeId *id = &thisCast->inodeIds[i];
         bool a = Serialization_deserializeUInt(ctx, &id->a);
         bool b = Serialization_deserializeUInt(ctx, &id->b);
         bool c = Serialization_deserializeUInt(ctx, &id->c);
         if (! a || ! b || ! c)
         {
            kfree(thisCast->inodeIds);
            thisCast->inodeIds = NULL;
            return false; // Return false if deserialization fails
         }
      }
   }

   return true;
}
