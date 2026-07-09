#pragma once

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/StorageErrors.h>

class HandshakeRespMsg : public NetMessageSerdes<HandshakeRespMsg>
{
   public:
      HandshakeRespMsg(FhgfsOpsErr result) :
         BaseType(NETMSGTYPE_HandshakeResp), result(result) {}

      HandshakeRespMsg() : BaseType(NETMSGTYPE_HandshakeResp) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->result;
      }

   private:
      FhgfsOpsErr result{FhgfsOpsErr_INTERNAL};

   public:
      FhgfsOpsErr getResult() const { return result; }
};
