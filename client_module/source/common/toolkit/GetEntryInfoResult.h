/*
 * GetEntryInfoResult - owns all data returned by FhgfsOpsRemoting_GetEntryInfo().
 *
 * All variable-length fields (pattern, pathInfo, rst) are independently allocated
 * copies that do not reference the network response buffer. Call
 * GetEntryInfoResult_uninit() to free everything.
 *
 * Follows the same ownership pattern as LookupIntentInfoOut.
 */

#ifndef GETENTRYINFORESULT_H_
#define GETENTRYINFORESULT_H_

#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/storage/PathInfo.h>
#include <common/storage/RemoteStorageTarget.h>
#include <common/storage/striping/StripePattern.h>

struct GetEntryInfoResult;
typedef struct GetEntryInfoResult GetEntryInfoResult;

static inline void GetEntryInfoResult_prepare(GetEntryInfoResult* this);
static inline bool GetEntryInfoResult_initFromRespMsg(GetEntryInfoResult* this,
   GetEntryInfoRespMsg* respMsg);
static inline void GetEntryInfoResult_uninit(GetEntryInfoResult* this);

struct GetEntryInfoResult
{
   int result;

   StripePattern* pattern;       /* owned; free with StripePattern_virtualDestruct() */
   PathInfo pathInfo;             /* owned; free with PathInfo_uninit() */
   RemoteStorageTarget rst;       /* owned; free with RemoteStorageTarget_uninit() */

   uint16_t mirrorNodeID;
   uint32_t numSessionsRead;
   uint32_t numSessionsWrite;
   uint8_t  fileDataState;
};

void GetEntryInfoResult_prepare(GetEntryInfoResult* this)
{
   this->result = FhgfsOpsErr_INTERNAL;
   this->pattern = NULL;
   PathInfo_init(&this->pathInfo, 0, NULL, 0);
   RemoteStorageTarget_init(&this->rst);
   this->mirrorNodeID = 0;
   this->numSessionsRead = 0;
   this->numSessionsWrite = 0;
   this->fileDataState = 0;
}

/**
 * Deep-copy all data from the deserialized response message into this result struct.
 * Must be called before freeing the response buffer.
 *
 * - StripePattern: allocated via StripePattern_createFromBuf() (copies raw pattern bytes)
 * - PathInfo: deep-copied via PathInfo_dup() (copies origParentEntryID string)
 * - RST: rstIds ownership transfers from respMsg (was kmalloc'd by deserializer)
 *
 * Returns false on allocation failure. On failure, the caller does not need to call
 * _uninit(), but it is safe to do so regardless.
 */
bool GetEntryInfoResult_initFromRespMsg(GetEntryInfoResult* this, GetEntryInfoRespMsg* respMsg)
{
   this->result = respMsg->result;

   /* Create StripePattern from raw buffer (patternStart points into resp buffer) */
   this->pattern = StripePattern_createFromBuf(respMsg->patternStart, respMsg->patternLength);
   if (!this->pattern)
   {
      /* RST transfer below won't run, so free rstIds now to avoid a leak. */
      RemoteStorageTarget_uninit(&respMsg->rst);
      return false;
   }

   /* Deep copy PathInfo (origParentEntryID points into resp buffer) */
   PathInfo_dup(&respMsg->pathInfo, &this->pathInfo);

   /* Transfer RST ownership: rstIds was kmalloc'd by deserializer */
   this->rst = respMsg->rst;
   RemoteStorageTarget_init(&respMsg->rst); /* zero source to prevent double-free */

   this->mirrorNodeID = respMsg->mirrorNodeID;
   this->numSessionsRead = respMsg->numSessionsRead;
   this->numSessionsWrite = respMsg->numSessionsWrite;
   this->fileDataState = respMsg->fileDataState;

   return true;
}

void GetEntryInfoResult_uninit(GetEntryInfoResult* this)
{
   if (this->pattern)
   {
      StripePattern_virtualDestruct(this->pattern);
      this->pattern = NULL;
   }

   PathInfo_uninit(&this->pathInfo);
   RemoteStorageTarget_uninit(&this->rst);
}

#endif /* GETENTRYINFORESULT_H_ */
