#include "GetEntryInfoMsg.h"

const struct NetMessageOps GetEntryInfoMsg_Ops = {
   .serializePayload = GetEntryInfoMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void GetEntryInfoMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   GetEntryInfoMsg* thisCast = (GetEntryInfoMsg*)this;

   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
}