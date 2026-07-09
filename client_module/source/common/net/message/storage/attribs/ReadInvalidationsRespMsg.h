#ifndef READINVALIDATIONSRESPMSG_H_
#define READINVALIDATIONSRESPMSG_H_

#include <common/net/message/NetMessage.h>

// Budget for the *entire* serialized response: the 40-byte message header, the three
// fixed uint32 fields (errorFlags, numUpdates, readPos) and the inode-id payload must all
// fit within this. Must stay within the messaging limits on BOTH sides:
//   - NETMSG_MAX_MSG_SIZE (1 MB), and
//   - the client recv buffer = tuneMsgBufSize (default 1 MB, pooled msgBufStore), and
//   - the meta send buffer  = tuneWorkerBufSize (default 1 MB).
// MUST be kept in sync with the meta-side common/.../ReadInvalidationsRespMsg.h.
#define READINVALIDATIONS_MAX_RESP_BYTES    (512 * 1024)
#define READINVALIDATIONS_PAYLOAD_OVERHEAD  (NETMSG_HEADER_LENGTH + 3 * sizeof(uint32_t))
#define READINVALIDATIONS_MAX_IDS \
   ((READINVALIDATIONS_MAX_RESP_BYTES - READINVALIDATIONS_PAYLOAD_OVERHEAD) / sizeof(InodeId))

enum
{
   ReadInvalidations_InvalidReadPos        = 1 << 0,
   ReadInvalidations_QueueOverflow         = 1 << 1,
   ReadInvalidations_Disabled              = 1 << 2,
   ReadInvalidations_Unavailable           = 1 << 3,
   ReadInvalidations_ResyncNeeded          = 1 << 4,
};


typedef struct InodeId InodeId;
struct InodeId  // new struct, smaller than EntryID and fixed-size
{
   uint32_t a;  // timestamp collision resolution counter
   uint32_t b;  // timestamp
   uint32_t c;  // (initial?) node num
};

typedef struct ReadInvalidationsRespMsg ReadInvalidationsRespMsg;
struct ReadInvalidationsRespMsg
{
   NetMessage netMessage;
   uint32_t errorFlags;
   uint32_t numUpdates;
   InodeId* inodeIds;
   uint32_t readPos;
};

typedef struct ReadInvalidationsData ReadInvalidationsData;
struct ReadInvalidationsData
{
   uint32_t errorFlags;
   uint32_t numUpdates;
   InodeId* inodeIds; // caller provided buffer
   uint32_t outReadPos;
};

void ReadInvalidationsRespMsg_init(ReadInvalidationsRespMsg* this);
bool ReadInvalidationsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

#endif /*READINVALIDATIONSRESPMSG_H_*/
