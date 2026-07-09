#include <common/toolkit/Serialization.h>
#include <common/storage/RemoteStorageTarget.h>
#include <os/OsDeps.h>

/**
 * Serialize RemoteStorageTarget.
 *
 * Wire format (matching C++ RemoteStorageTarget::serialize):
 *   uint8_t  majorVersion
 *   uint8_t  minorVersion
 *   uint16_t coolDownPeriod
 *   uint16_t reserved
 *   uint16_t filePolicies
 *   vector<uint32_t> rstIdVec:
 *     uint32_t totalLengthInBytes (sizeof(uint32_t count) + count * sizeof(uint32_t))
 *     uint32_t count
 *     uint32_t[count] ids
 */
void RemoteStorageTarget_serialize(SerializeCtx* ctx, const RemoteStorageTarget* this)
{
   uint32_t i;
   uint32_t vecByteLen = sizeof(uint32_t) + this->numRSTIds * sizeof(uint32_t);

   Serialization_serializeUInt8(ctx, this->majorVersion);
   Serialization_serializeUInt8(ctx, this->minorVersion);
   Serialization_serializeUShort(ctx, this->coolDownPeriod);
   Serialization_serializeUShort(ctx, this->reserved);
   Serialization_serializeUShort(ctx, this->filePolicies);

   /* vector<uint32_t> wire format */
   Serialization_serializeUInt(ctx, vecByteLen);        /* totalLengthInBytes */
   Serialization_serializeUInt(ctx, this->numRSTIds);   /* element count */
   for (i = 0; i < this->numRSTIds; i++)
      Serialization_serializeUInt(ctx, this->rstIds[i]);
}

/**
 * Deserialize RemoteStorageTarget from network buffer.
 *
 * Allocates rstIds array with kmalloc if numRSTIds > 0.
 * Caller must call RemoteStorageTarget_uninit() to free.
 */
bool RemoteStorageTarget_deserialize(DeserializeCtx* ctx, RemoteStorageTarget* outThis)
{
   DeserializeCtx innerCtx;
   unsigned rstVecCount;
   unsigned i;

   RemoteStorageTarget_init(outThis);

   if (!Serialization_deserializeUInt8(ctx, &outThis->majorVersion))
      return false;
   if (!Serialization_deserializeUInt8(ctx, &outThis->minorVersion))
      return false;
   if (!Serialization_deserializeUShort(ctx, &outThis->coolDownPeriod))
      return false;
   if (!Serialization_deserializeUShort(ctx, &outThis->reserved))
      return false;
   if (!Serialization_deserializeUShort(ctx, &outThis->filePolicies))
      return false;

   /* vector<uint32_t> wire format: totalLen, count, elements.
    * Use __Serialization_deserializeNestedField so that the outer ctx is advanced
    * past the entire block immediately; future server additions inside the block
    * are silently skipped rather than corrupting subsequent field reads. */
   if (!__Serialization_deserializeNestedField(ctx, &innerCtx))
      return false;
   if (!Serialization_deserializeUInt(&innerCtx, &rstVecCount))
      return false;

   outThis->numRSTIds = rstVecCount;

   if (rstVecCount > 0)
   {
      outThis->rstIds = kmalloc(rstVecCount * sizeof(uint32_t), GFP_NOFS);
      if (!outThis->rstIds)
         return false;

      for (i = 0; i < rstVecCount; i++)
      {
         if (!Serialization_deserializeUInt(&innerCtx, &outThis->rstIds[i]))
         {
            kfree(outThis->rstIds);
            outThis->rstIds = NULL;
            outThis->numRSTIds = 0;
            return false;
         }
      }
   }

   return true;
}
