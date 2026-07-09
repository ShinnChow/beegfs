#pragma once

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>

class ReadInvalidationsMsg :  public NetMessageSerdes<ReadInvalidationsMsg>
{
   public:
      uint32_t readPos;
      bool resync; // true if watcher is asking meta to resync (first contact or after desync)

      ReadInvalidationsMsg(uint32_t readPos, bool resync) :
         BaseType(NETMSGTYPE_ReadInvalidations)
      {
         this->readPos = readPos;
         this->resync = resync;
      }

      ReadInvalidationsMsg() : BaseType(NETMSGTYPE_ReadInvalidations)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->readPos;
         ctx % obj->resync;
      }
};

