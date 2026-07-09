/*
 * RemoteStorageTarget - RST information for a file entry.
 *
 * Mirrors the C++ common/storage/RemoteStorageTarget.h class.
 * The rstIds array is dynamically allocated on deserialization and must be
 * freed with RemoteStorageTarget_uninit().
 */

#ifndef REMOTESTORAGETARGET_H_
#define REMOTESTORAGETARGET_H_

#include <common/toolkit/SerializationTypes.h>

struct RemoteStorageTarget;
typedef struct RemoteStorageTarget RemoteStorageTarget;

static inline void RemoteStorageTarget_init(RemoteStorageTarget* this);
static inline void RemoteStorageTarget_uninit(RemoteStorageTarget* this);

extern void RemoteStorageTarget_serialize(SerializeCtx* ctx, const RemoteStorageTarget* this);
extern bool RemoteStorageTarget_deserialize(DeserializeCtx* ctx, RemoteStorageTarget* outThis);

struct RemoteStorageTarget
{
   uint8_t  majorVersion;
   uint8_t  minorVersion;
   uint16_t coolDownPeriod;
   uint16_t reserved;
   uint16_t filePolicies;

   uint32_t numRSTIds;
   uint32_t* rstIds;      /* dynamically allocated array, NULL if numRSTIds == 0 */
};

void RemoteStorageTarget_init(RemoteStorageTarget* this)
{
   this->majorVersion = 0;
   this->minorVersion = 0;
   this->coolDownPeriod = 0;
   this->reserved = 0;
   this->filePolicies = 0;
   this->numRSTIds = 0;
   this->rstIds = NULL;
}

void RemoteStorageTarget_uninit(RemoteStorageTarget* this)
{
   kfree(this->rstIds);
   this->rstIds = NULL;
   this->numRSTIds = 0;
}

#endif /* REMOTESTORAGETARGET_H_ */
