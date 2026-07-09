#pragma once

#include <common/net/message/storage/attribs/ReadInvalidationsMsg.h>


class ReadInvalidationsMsgEx : public ReadInvalidationsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

