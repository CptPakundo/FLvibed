// FLvibed - calls that wait for the frame's end, with no logic call on the stack: the front end's clicks. A click may open a dialog with its own modal loop, whose
// frames the host then serves (router.cpp ModalFrame). One queue: only one entry drives the logic's frame.
#pragma once

namespace deferred
{
void Add(void (*fn)(void*), void* ctx); // at once when the queue is full
void Run();                             // at the frame's end
void Drop();                            // the logic stopped: what it queued is its own code
} // namespace deferred
