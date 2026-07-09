#include "ReadInvalidationsMsg.h"

static const struct NetMessageOps ReadInvalidationsMsg_Ops = {
   .serializePayload = ReadInvalidationsMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};


void ReadInvalidationsMsg_init(ReadInvalidationsMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_ReadInvalidations, &ReadInvalidationsMsg_Ops);
}

void ReadInvalidationsMsg_initFromReadPos(ReadInvalidationsMsg* this, uint32_t readPos,
   bool resync)
{
   ReadInvalidationsMsg_init(this);
   this->readPos = readPos;
   this->resync = resync;
}

void ReadInvalidationsMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   ReadInvalidationsMsg* thisCast = (ReadInvalidationsMsg*)this;
   Serialization_serializeUInt(ctx, thisCast->readPos);
   Serialization_serializeBool(ctx, thisCast->resync);
}
