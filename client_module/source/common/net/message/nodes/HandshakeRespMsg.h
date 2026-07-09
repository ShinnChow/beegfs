#ifndef HANDSHAKERESPMSG_H_
#define HANDSHAKERESPMSG_H_

#include <common/net/message/NetMessage.h>


typedef struct HandshakeRespMsg HandshakeRespMsg;
struct HandshakeRespMsg
{
   NetMessage netMessage;
   int result;
};

extern const struct NetMessageOps HandshakeRespMsg_Ops;

static inline void HandshakeRespMsg_init(HandshakeRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_HandshakeResp, &HandshakeRespMsg_Ops);
}

static inline int HandshakeRespMsg_getResult(HandshakeRespMsg* this)
{
   return this->result;
}

#endif /*HANDSHAKERESPMSG_H_*/
