#ifndef GETENTRYINFORESP_H_
#define GETENTRYINFORESP_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RemoteStorageTarget.h>
#include <common/storage/striping/StripePattern.h>

struct GetEntryInfoRespMsg;
typedef struct GetEntryInfoRespMsg GetEntryInfoRespMsg;

static inline void GetEntryInfoRespMsg_init(GetEntryInfoRespMsg* this);

extern bool GetEntryInfoRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

struct GetEntryInfoRespMsg
{
   NetMessage netMessage;

   int result;

   /* Stripe pattern - raw deserialized data, caller creates pattern via
    * StripePattern_createFromBuf(patternStart, patternLength) */
   const char* patternStart;
   uint32_t patternLength;

   /* PathInfo */
   PathInfo pathInfo;

   /* Remote Storage Target */
   RemoteStorageTarget rst;

   /* Legacy field (always 0, no longer in use) */
   uint16_t mirrorNodeID;

   uint32_t numSessionsRead;
   uint32_t numSessionsWrite;
   uint8_t  fileDataState;
};

extern const struct NetMessageOps GetEntryInfoRespMsg_Ops;

void GetEntryInfoRespMsg_init(GetEntryInfoRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetEntryInfoResp, &GetEntryInfoRespMsg_Ops);
   RemoteStorageTarget_init(&this->rst);
}

static inline int GetEntryInfoRespMsg_getResult(GetEntryInfoRespMsg* this)
{
   return this->result;
}

#endif /* GETENTRYINFORESP_H_ */
