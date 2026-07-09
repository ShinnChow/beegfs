#include "HandshakeMsg.h"

static const struct NetMessageOps HandshakeMsg_Ops = {
   .serializePayload = HandshakeMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void HandshakeMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   HandshakeMsg* thisCast = (HandshakeMsg*)this;
   NumNodeID_serialize(ctx, &thisCast->clientID);
}

void HandshakeMsg_initFromClientID(HandshakeMsg* this, NumNodeID clientID)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_Handshake, &HandshakeMsg_Ops);
   this->clientID = clientID;
}
