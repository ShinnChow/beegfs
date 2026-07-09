#pragma once

#include <common/net/message/NetMessage.h>
#include "InvalWatch.h"

// Helper function to add watch for watcher associated with this response's connection / socket.

static inline bool add_target_watch_for_connected_watcher(NetMessage::ResponseContext& ctx, EntryInfo *entryInfo)
{
   Watcher *watcher = (Watcher *) ctx.getSocket()->commContext;
   if (! watcher)
   {
      return false;
   }
   return add_target_watch_by_entryid(entryInfo->getEntryID(), watcher);
}
