#include "GetEntryInfoRespMsg.h"

const struct NetMessageOps GetEntryInfoRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = GetEntryInfoRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

/*
 * Deserialize GetEntryInfoResp from meta server.
 *
 * Wire format (must match C++ GetEntryInfoRespMsg::serialize order):
 *   int32_t  result
 *   StripePattern (length-prefixed blob)
 *   PathInfo
 *   RemoteStorageTarget
 *   uint16_t mirrorNodeID (legacy, always 0)
 *   uint32_t numSessionsRead
 *   uint32_t numSessionsWrite
 *   uint8_t  fileDataState
 */
bool GetEntryInfoRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetEntryInfoRespMsg* thisCast = (GetEntryInfoRespMsg*)this;

   if (!Serialization_deserializeInt(ctx, &thisCast->result))
      return false;

   if (!StripePattern_deserializePatternPreprocess(ctx,
         &thisCast->patternStart, &thisCast->patternLength))
      return false;

   if (!PathInfo_deserialize(ctx, &thisCast->pathInfo))
      return false;

   if (!RemoteStorageTarget_deserialize(ctx, &thisCast->rst))
      return false;

   /* RST rstIds was kmalloc'd above; if any field below fails we must free it */

   if (!Serialization_deserializeUShort(ctx, &thisCast->mirrorNodeID))
      goto fail_cleanup_rst;

   if (!Serialization_deserializeUInt(ctx, &thisCast->numSessionsRead))
      goto fail_cleanup_rst;

   if (!Serialization_deserializeUInt(ctx, &thisCast->numSessionsWrite))
      goto fail_cleanup_rst;

   if (!Serialization_deserializeUInt8(ctx, &thisCast->fileDataState))
      goto fail_cleanup_rst;

   return true;

fail_cleanup_rst:
   RemoteStorageTarget_uninit(&thisCast->rst);
   return false;
}
