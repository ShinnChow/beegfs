#pragma once

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>

class HandshakeMsg :  public NetMessageSerdes<HandshakeMsg>
{
   public:
      HandshakeMsg(uint32_t clientID) :
         BaseType(NETMSGTYPE_Handshake)
      {
         this->clientID = clientID;
      }

      HandshakeMsg() : BaseType(NETMSGTYPE_Handshake)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->clientID;
      }

   private:
      uint32_t clientID;

   public:
      uint32_t getClientID() const
      {
         return clientID;
      }

};

