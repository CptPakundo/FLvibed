// The calls that wait for the frame's end (deferred.h).
#include "all.h"

namespace deferred
{
struct DeferredCall
{
    void (*fn)(void*);
    void* ctx;
};
static DeferredCall g_deferred[16];
static int g_deferredCount = 0;

void Drop() { g_deferredCount = 0; }

void Add(void (*fn)(void*), void* ctx)
{
    if (!fn)
        return;
    if (g_deferredCount < 16)
        g_deferred[g_deferredCount++] = {fn, ctx};
    else
        fn(ctx); // full (never seen): at once, as before v29
}

void Run()
{
    // a copy: a deferred call may open a modal loop whose frames defer again
    DeferredCall calls[16];
    const int n = g_deferredCount;
    memcpy(calls, g_deferred, sizeof(DeferredCall) * n);
    g_deferredCount = 0;
    for (int i = 0; i < n; ++i)
        calls[i].fn(calls[i].ctx);
}
} // namespace deferred
