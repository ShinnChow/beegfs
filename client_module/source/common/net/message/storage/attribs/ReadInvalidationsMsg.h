#ifndef READINVALIDATIONSMSG_H_
#define READINVALIDATIONSMSG_H_

#include <common/net/message/NetMessage.h>

typedef struct ReadInvalidationsMsg ReadInvalidationsMsg;
struct ReadInvalidationsMsg
{
   NetMessage netMessage;
   uint32_t readPos; // position in the invalidation queue to read from
   bool resync; // true if watcher is asking meta to resync (first contact or after desync)
};

void ReadInvalidationsMsg_init(ReadInvalidationsMsg* this);
void ReadInvalidationsMsg_initFromReadPos(ReadInvalidationsMsg* this, uint32_t readPos,
   bool resync);
void ReadInvalidationsMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

#endif /*READINVALIDATIONSMSG_H_*/
