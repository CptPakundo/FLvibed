// FLvibed - the game fixes that belong to no feature (docs\MODULES_PLAN.md: core): the LAN list and the message pump.
#include "all.h"

#include "fixes.h"

namespace fixes
{
// The multiplayer LAN list, IPv4 hosts only (ship it, never over another version of the fix):
// vanilla 1.1 lists a local server by its IPv6 link-local address and the connect fails. HD Edition's exe and FLUF's
// client patch skip an entry whose address is not IPv4 (exe::LanHostFilter, docs\FREELANCER_RE.md "The LAN list and
// IPv6"); FLvibed writes the same 13 bytes only where vanilla 1.1's own bytes are - any other content (that fix, another
// mod's) is left as it is.
static void LanListIpv4()
{
    static const u8 vanilla[13] = {0x51, 0x8D, 0x4C, 0x24, 0x20, 0x51, 0x8D, 0x8C, 0x24, 0xC8, 0x00, 0x00, 0x00};
    static const u8 fixed[13] = {0x40, 0x74, 0x63, 0x48, 0x51, 0x8D, 0x54, 0xE4, 0x20, 0x52, 0x83, 0xE9, 0x08};
    if (!exe::LanHostFilter.ok())
    {
        Log("LAN list: the site is not this game's (signature) - left alone");
        return;
    }
    u8* at = reinterpret_cast<u8*>(exe::LanHostFilter.get());
    if (memcmp(at, fixed, 13) == 0)
    {
        Log("LAN list: IPv4 only already (HD Edition's exe or another mod's fix) - left alone");
        return;
    }
    if (memcmp(at, vanilla, 13) != 0)
    {
        Log("LAN list: another patch at the site - left alone");
        return;
    }
    DWORD old = 0;
    if (!VirtualProtect(at, 13, PAGE_EXECUTE_READWRITE, &old))
        return;
    memcpy(at, fixed, 13);
    VirtualProtect(at, 13, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 13);
    Log("LAN list: IPv4 hosts only (vanilla's list fixed as HD Edition's)");
}

// The message pump's GetMessage (`pump_no_block`, installed with the game hooks): a message that is there is taken at
// once (by the previous pointer when another plugin hooked the import, else PeekMessage PM_REMOVE - never a wait); none
// within 2 ms = WM_NULL (no window: DispatchMessage does nothing), so the pump returns and the frame runs. WM_QUIT
// still ends the loop (GetMessage's 0).
typedef BOOL(WINAPI* GetMessageFn)(LPMSG, HWND, UINT, UINT);
static GetMessageFn g_origGetMessage = nullptr;
static volatile LONG g_pumpEmpty = 0;

static BOOL WINAPI GetMessageHook(LPMSG msg, HWND wnd, UINT lo, UINT hi)
{
    static const GetMessageFn real =
        reinterpret_cast<GetMessageFn>(GetProcAddress(GetModuleHandleA("user32.dll"), "GetMessageA"));
    for (int pass = 0; pass < 2; ++pass)
    {
        if (g_origGetMessage != real) // another plugin's hook first: hand it the call once a message is there
        {
            if (PeekMessageA(msg, wnd, lo, hi, PM_NOREMOVE))
                return g_origGetMessage(msg, wnd, lo, hi);
        }
        else if (PeekMessageA(msg, wnd, lo, hi, PM_REMOVE))
            return msg->message != WM_QUIT;
        if (pass == 0)
            MsgWaitForMultipleObjectsEx(0, nullptr, 2, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    ZeroMemory(msg, sizeof(*msg));
    msg->message = WM_NULL;
    const LONG n = InterlockedIncrement(&g_pumpEmpty);
    if ((n & (n - 1)) == 0) // 1, 2, 4, 8 ...: the pump would have waited here
        Log("message pump: nothing to take after a peek (%ld times) - not waited", n);
    return TRUE;
}

void Install()
{
    LanListIpv4();
    // The game's message pump (0x5B0B60) peeks the queue (PeekMessage, PM_NOREMOVE) and then takes the message with the
    // blocking GetMessage (0x5B0BA2); a peeked message that GetMessage cannot retrieve (one Windows generates on demand
    // and then drops, or one consumed in between) leaves the main thread waiting there until any message comes - no frame
    // meanwhile (gate mpb6, 28 Oct 2026, instance 1 in M07: frozen with the main thread at Freelancer.exe+0x1B0BA4 in
    // win32u, the window not hung; the 4 m 40 s M05 freeze of 21 Oct and the 152 s M02 stall of gate mpb2 resumed by
    // themselves). `pump_no_block` (default 1): GetMessage never waits - nothing retrievable within 2 ms = a WM_NULL
    // (dispatched to no window), the pump returns and the frame runs; the count is logged.
    if (GetPrivateProfileIntA("Controller", "pump_no_block", 1, (DllDir() + "FLController.ini").c_str()) != 0)
    {
        g_origGetMessage = reinterpret_cast<GetMessageFn>(
            hk::PatchImport(GetModuleHandleA(nullptr), "user32.dll", "GetMessageA", GetMessageHook));
        Log("message pump: GetMessageA import %s", g_origGetMessage ? "hooked (never blocks)" : "NOT FOUND");
    }
}
} // namespace fixes
