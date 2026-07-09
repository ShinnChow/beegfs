#pragma once

#include <common/net/message/nodes/HandshakeMsg.h>
#include <common/net/sock/Socket.h>

class HandshakeMsgEx : public HandshakeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

