#pragma once

#include <cstdint>
#include <string> // not a dependency I like...

#include <common/net/message/mon/RequestMetaDataRespMsg.h>

// Fixed-size, more compact representation compared to the variable-size
// EntryID strings used elsewhere in the codebase. This is a serialisation of
// counter (32-bit), timestamp (32-bit), node-id (32-bit).

struct InodeId
{
   uint8_t buffer[12];
};

// We maintain InvalTarget objects (beside FileInode and DirInode objects which live
// elsewhere in the system) that are tracked for cache invalidation.
struct InvalTarget;

// A Watcher represents a connected client that has registered to receive
// target invalidations.
struct Watcher;

// Create an InodeId from a (zero-terminated) EntryID string.
// Returns: whether successful. This function is successful on all valid EntryID strings.
bool read_inodeid(std::string const& entryID, InodeId *out);

// lookup_watcher(): Lookup watcher given 32-bit session Id. This will
//   currently implicitly create a new Watcher if none is registered for
//   that session id.
//   Watchers are reference counted and the returned watcher will have its
//   reference count incremented.
//   When you're done using the watcher, call put_watcher().
//   Note: If all watcher slots are taken and none can be freed up,
//   this function can fail and return NULL.
//   Note: this function can also fail in case of concurrent shutdown.
// get_watcher(): increment watcher refcount given existing watcher reference.
// put_watcher(): decrement watcher refcount (dropping watcher if refcount goes to 0)
Watcher *lookup_watcher(uint32_t sessionId);
uint32_t get_watcher(Watcher *watcher);
void put_watcher(Watcher *watcher);

// Get an InvalTarget from the cache. If no InvalTarget exists for the given InodeId
// it will be allocated. The object lives in
// memory only (no disk persistence).
// Returns: NULL on failure. Pointer to InvalTarget on success. The target is
// ref-counted. You have to call put_invaltarget() when you are done using it.

InvalTarget *get_invaltarget(InodeId const& inodeId);
void put_invaltarget(InvalTarget *target);

// Add a watcher to the watch list of a target.
// Returns: whether successful. The operation can fail if not enough memory is
// available.

void add_target_watch(InvalTarget *target, Watcher *watcher);

// Trigger an invalidation on an InvalTarget. This will move the current
// Watcher-list of the target to an update queue, which causes all watching
// watchers to be notified asynchronously. A new empty watch list will be set on
// the target.

void invalidate_target(InvalTarget *target);

// Invalidation buffer reader interface.
//
// This is an interface to consume cache invalidations resulting from
// earlier watcher subscriptions. For any watcher-target association that was made
// using add_target_watch(), the next (only the next) invalidation of that
// target will cause an invalidation entry to be enqueued to that watcher's
// invalidation buffer.
//
// This buffer reader interface allows reading the buffer using a cursor API.
// A cursor is represented as uint32_t, which can wrap around to 0 (virtual
// ringbuffer).
//
// The initial reading position is 0. Reading from the invalidation buffer
// will return as many invalidations as available in the specified time.
// Note that there are two times specified, time-to-first-invalidation and
// time-to-fill. The former can be a longer time, on the order of seconds,
// to avoid fast spinning (repeated requests) if no invalidations are available.
// The latter should be a very short time, just a grace period so invalidations
// can be transferred in batches, but short enough to not unnecessarily delay
// the request completion.
//
// A read request with readpos=N doubles as an acknowledgement of receipt of
// all previous invalidations up to N in the invalidation buffer. Those
// invalidations will be dropped from the buffer.

struct InvalidationReadRequest
{
   Watcher *watcher;
   void *buffer;  // buffer of size max_bytes
   uint32_t bufferSize;  // max number of bytes to read
   uint32_t read_pos;  // where read should begin
   bool resync;  // true if watcher is asking meta to resync (first contact or after desync)
   uint32_t waitMicrosecondsNonEmpty;  // max time to wait for something to be in the queue.
   uint32_t waitMicrosecondsFillBuffer;  // max time to wait for the output buffer to be completely filled.
};

struct InvalidationReadResult
{
   uint32_t bytes_read;
   uint32_t end_pos;

   // Error conditions -- some of them can happen in the same call.

   // requested input position was not in valid data range.
   bool invalid_read_pos;

   // Invalidation queue overflow (some bytes might have been read but
   // synchronisation was lost anyway).
   bool queue_overflow; 

   // Caching disabled by server config
   bool disabled;

   // Caching service unavailable, probably metadata service shutting down.
   bool unavailable;

   // The buffer was completely filled before the wait time to fill buffer
   // elapsed. More invalidations are likely available. If this happens
   // repeatedly, it means that the reader wasn't able to keep up with the rate
   // of incoming invalidations, and the queue is likely to overflow.
   bool buffer_full;

   // The watcher has not yet synced with us. This usually means a meta restart
   // happened (watcher must resync), or the watcher's state is stale.
   bool resync_needed;
};

// Describes the valid range of data in an invalidation queue.
// The sequence numbers are counted in bytes!
//
// `read_pos` is the sequence number of the next byte that can be read.
// `write_pos` is the sequence number of the next byte that can be written.
//
// Note that `read_pos == write_pos` means that there is currently no data
// available for reading.
//
// Note that sequence numbers may wrap around. It can happen that `read_pos <
// write_pos`. Direct comparisons are not meaningful for that reason.
// To know the number of bytes in the queue, use modular arithmetic:
// `num_bytes = write_pos - read_pos`.

struct IvqValidDataRange
{
   uint32_t read_pos;
   uint32_t write_pos;
};

static inline bool IvqValidDataRange_contains(IvqValidDataRange range, uint32_t x)
{
   uint32_t a = range.read_pos;
   uint32_t b = range.write_pos;
   return x - a <= b - a;
}

// This function does three things:
// 1) Acknowledge that the final destination received all data up to `begin_pos`.
// This will drop all data up that position from the invalidation queue.
// If begin_pos is outside the valid range, the function sets invalid_read_pos=true and returns.
//
// 2) Wait for a certain amount of time for anything to be in the invalidation
// queue following position `begin_pos`.
//
// Returns false when the given time to wait for first byte has elapsed.
// Returns true when one or more of the following conditions were true before the time elapsed:
//  - the invalidation queue is not empty
//  - the invalidation queue was overflowed (overflow bit is set).
//  - the service is unavailable or request processing was interrupted (e.g. system shutting down).
//
// 3) Read new invalidations from the queue. Try fill the given buffer but
// don't take longer than the given time.

void read_invalidations(InvalidationReadRequest const *request,
      InvalidationReadResult *out_result);


// helper doing read_inodeid(), get_invaltarget().
InvalTarget *get_invaltarget_by_entryid(std::string const& entryID);

// helper doing read_inodeid(), get_invaltarget(). add_target_watch(), put_invaltarget().
bool add_target_watch_by_entryid(std::string const& entryID, Watcher *watcher);

// helper doing read_inodeid(), get_invaltarget(). invalidate_target(), put_invaltarget().
bool invalidate_target_by_entryid(std::string const& entryID);


// Collect all InvalWatch monitoring data in a single call.
// Fills aggregate stats and per-watcher data into 'out'.
// Takes watcher_store.mutex once to gather both consistently.
void get_invalwatch_mon_data(InvalWatchStat* out);


// These functions should be called to initialize/uninitialize this entire subsystem.
void initialize_invalwatch(uint64_t maxCachedObjects, uint32_t maxQueueBytes);
void uninitialize_invalwatch();
