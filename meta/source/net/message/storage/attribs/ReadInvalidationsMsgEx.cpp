#include <common/net/message/storage/attribs/ReadInvalidationsRespMsg.h>
#include <common/app/log/LogContext.h>
#include <components/InvalWatch.h>

#include "ReadInvalidationsMsgEx.h"

bool ReadInvalidationsMsgEx::processIncoming(ResponseContext& ctx)
{
   // NOTE: watcher may be NULL if handshake failed or client didn't do it.
   Watcher* watcher = (Watcher*)ctx.getSocket()->commContext;
   ctx.sendResponse(ReadInvalidationsRespMsg(watcher, this->readPos,
         this->resync));
   return true;
}
