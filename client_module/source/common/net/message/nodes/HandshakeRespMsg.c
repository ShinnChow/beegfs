#include "HandshakeRespMsg.h"

static bool HandshakeRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   HandshakeRespMsg* thisCast = (HandshakeRespMsg*)this;
   if(!Serialization_deserializeInt(ctx, &thisCast->result))
      return false;
   return true;
}

const struct NetMessageOps HandshakeRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy, // serialization not implemented
   .deserializePayload = HandshakeRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};
