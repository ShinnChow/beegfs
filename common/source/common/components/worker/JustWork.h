#pragma once

// Simplification of the existing Work structs
// Hide dependencies in the implementation to speed up the build a lot.

// Unfortunately, the implementation of this currently needs to live in the
// individual service, since the queues are stored in a service independent
// way.

typedef void JustWorkFunc(void *data);

void just_enqueue_work(JustWorkFunc *func, void *data);
