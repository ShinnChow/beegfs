#ifndef HANDSHAKEMSG_H_
#define HANDSHAKEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>

typedef struct HandshakeMsg HandshakeMsg;
struct HandshakeMsg
{
   NetMessage netMessage;
   NumNodeID clientID; // clientID to identify ourselves
};

void HandshakeMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
void HandshakeMsg_initFromClientID(HandshakeMsg* this, NumNodeID clientID);

#endif /*HANDSHAKEMSG_H_*/
