#ifndef GETENTRYINFOMSG_H_
#define GETENTRYINFOMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

struct GetEntryInfoMsg;
typedef struct GetEntryInfoMsg GetEntryInfoMsg;

static inline void GetEntryInfoMsg_init(GetEntryInfoMsg* this);
static inline void GetEntryInfoMsg_initFromEntryInfo(GetEntryInfoMsg* this,
      const EntryInfo* entryInfo);

extern void GetEntryInfoMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);

struct GetEntryInfoMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfoPtr; // not owned by this object
};

extern const struct NetMessageOps GetEntryInfoMsg_Ops;

void GetEntryInfoMsg_init(GetEntryInfoMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetEntryInfo, &GetEntryInfoMsg_Ops);
}

void GetEntryInfoMsg_initFromEntryInfo(GetEntryInfoMsg* this, const EntryInfo* entryInfo)
{
   GetEntryInfoMsg_init(this);
   this->entryInfoPtr = entryInfo;
}

#endif // GETENTRYINFOMSG_H_
