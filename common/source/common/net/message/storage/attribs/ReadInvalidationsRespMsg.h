#pragma once

#include <common/net/message/NetMessage.h>
#include <common/Common.h>
#include <common/app/log/LogContext.h>

#include <components/InvalWatch.h>

enum
{
   ReadInvalidations_InvalidReadPos        = 1 << 0,
   ReadInvalidations_QueueOverflow         = 1 << 1,
   ReadInvalidations_Disabled              = 1 << 2,
   ReadInvalidations_Unavailable           = 1 << 3,
   ReadInvalidations_ResyncNeeded          = 1 << 4,
};

// Budget for the *entire* serialized response: the 40-byte message header, the three
// fixed uint32 fields (error_flags, num_invalidations, end_pos) and the inode-id payload
// must all fit within this. Must stay within the messaging limits on BOTH sides:
//   - NETMSG_MAX_MSG_SIZE (1 MB), and
//   - the meta send buffer  = tuneWorkerBufSize (default 1 MB), and
//   - the client recv buffer = tuneMsgBufSize (default 1 MB, pooled msgBufStore).
// MUST be kept in sync with the client-side ReadInvalidationsRespMsg.h.
#define READINVALIDATIONS_MAX_RESP_BYTES    (512 * 1024)
#define READINVALIDATIONS_PAYLOAD_OVERHEAD  (NETMSG_HEADER_LENGTH + 3 * sizeof(uint32_t))
#define READINVALIDATIONS_MAX_PAYLOAD_BYTES \
   (READINVALIDATIONS_MAX_RESP_BYTES - READINVALIDATIONS_PAYLOAD_OVERHEAD)



struct RawBlockDeser
{
   void *ptr;
   size_t size;

   RawBlockDeser(void *ptr, size_t size) : ptr(ptr), size(size) {}

   friend Serializer& operator%(Serializer& ser, RawBlockDeser const& obj)
   {
      ser.putBlock(obj.ptr, obj.size);
      return ser;
   }
   friend Deserializer& operator%(Deserializer& deser, RawBlockDeser const& obj)
   {
      deser.getBlock(obj.ptr, obj.size);
      return deser;
   }
};


class ReadInvalidationsRespMsg : public NetMessageSerdes<ReadInvalidationsRespMsg>
{
   public:
      Watcher *watcher;
      uint32_t read_pos;
      bool resync;

      ReadInvalidationsRespMsg(Watcher *watcher, uint32_t read_pos,
            bool resync) :
         BaseType(NETMSGTYPE_ReadInvalidationsResp),
         watcher(watcher), read_pos(read_pos),
         resync(resync)
      {
         // note, due to current code organization we accept NULL watcher and handle that later.
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         // Size the payload so that the full message (header + fixed fields + payload)
         // stays within READINVALIDATIONS_MAX_RESP_BYTES (512 KB) and can never overflow the
         // 1 MB network buffer. we could avoid heap allocations but let's save some lines
         // of code for now.
         std::vector<uint8_t> buffer;
         buffer.resize(READINVALIDATIONS_MAX_PAYLOAD_BYTES);
         uint32_t error_flags = 0;

         InvalidationReadRequest request = {};
         request.watcher = obj->watcher;
         request.buffer = buffer.data();
         request.bufferSize = buffer.size();
         request.read_pos = obj->read_pos;
         request.resync = obj->resync;
         request.waitMicrosecondsNonEmpty = 1000000ul;  // 1s
         request.waitMicrosecondsFillBuffer = 5000;  // 5ms
                                                        //
         InvalidationReadResult result = {};
         // quickfix: prime with pos of read begin, in case function fails and field doesn't get written.
         result.end_pos = obj->read_pos;

         read_invalidations(&request, &result);

         if (result.invalid_read_pos)
         {
            LogContext("ReadInvalidations").log(Log_ERR, "Invalid read pos in invalidation read request");
            error_flags |= ReadInvalidations_InvalidReadPos;
         }
         if (result.queue_overflow)
         {
            LogContext("ReadInvalidations").log(Log_WARNING, "Client lost sync: Invalidation queue overflowed");
            error_flags |= ReadInvalidations_QueueOverflow;
         }
         if (result.disabled)
         {
            error_flags |= ReadInvalidations_Disabled;
         }
         if (result.unavailable)
         {
            error_flags |= ReadInvalidations_Unavailable;
         }
         if (result.resync_needed)
         {
            LogContext("ReadInvalidations").log(Log_NOTICE, "Watcher lost sync");
            error_flags |= ReadInvalidations_ResyncNeeded;
         }

         uint32_t end_pos = result.end_pos;
         uint32_t num_invalidations = result.bytes_read / sizeof (InodeId);

         ctx % error_flags;
         ctx % num_invalidations;
         ctx % end_pos;
         ctx % RawBlockDeser(buffer.data(), result.bytes_read);
      }
};
