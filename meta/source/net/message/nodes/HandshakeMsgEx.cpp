#include <common/net/message/nodes/HandshakeRespMsg.h>
#include <common/app/log/LogContext.h>
#include <program/Program.h>
#include <components/InvalWatch.h>
#include "HandshakeMsgEx.h"

static void cleanup_watcher(void *commContext)
{
   Watcher *watcher = (Watcher *) commContext;
   if (watcher)
   {
      put_watcher(watcher);
   }
}

bool HandshakeMsgEx::processIncoming(ResponseContext& ctx)
{
    const char* logContext = "HandshakeMsg";
    LogContext(logContext).log(Log_DEBUG, "ClientID: " + StringTk::uintToStr(this->getClientID()));
    FhgfsOpsErr handshakeRes = FhgfsOpsErr_INTERNAL;
    Socket* sock = ctx.getSocket();
    if (sock->commContext)
    {
        // Already registered. Client shouldn't do that
        handshakeRes = FhgfsOpsErr_INVAL;
    }
    else if (! Program::getApp()->getConfig()->getSysRemoteInvalEnabled())
    {
         handshakeRes = FhgfsOpsErr_NOTSUPP;
    }
    else
    {
        uint32_t clientID = this->getClientID();
        Watcher* watcher = lookup_watcher(clientID);
        if (watcher)
        {
           sock->commContext = watcher;
           sock->commContextCleanupFunc = &cleanup_watcher;
           handshakeRes = FhgfsOpsErr_SUCCESS;
        }
        else
        {
            handshakeRes = FhgfsOpsErr_INTERNAL;
        }
    }
    ctx.sendResponse(HandshakeRespMsg(handshakeRes) );
    return true;
}
