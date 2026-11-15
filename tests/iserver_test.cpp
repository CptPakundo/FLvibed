// core\game\iserver.h (types only): the server interface's slots by name, checked against the test install's own
// Server.dll: mapped without running it, each slot of the exported IServer vtable must hold the method's export.
#include "test.h"

#include "game/iserver.h"

#include <set>

void Test_iserver()
{
    // The table itself: every offset a slot (a multiple of 4), none twice, each named slot in the export table.
    std::set<u32> offsets;
    for (const iserver::SlotExport& e : iserver::kExports)
    {
        CHECK(e.offset % 4 == 0);
        CHECK(offsets.insert(e.offset).second);
        CHECK(strstr(e.mangled, e.method) == e.mangled + 1);
    }
    CHECK(offsets.size() == 27);
    CHECK(offsets.count(iserver::kMissionResponse) && offsets.count(iserver::kSaveGame) && offsets.count(iserver::kStopTradeRequest));

    if (!NeedInstall("the install's Server.dll"))
        return;
    const std::string path = g_test.install + "\\EXE\\Server.dll";
    // Mapped as an image, relocated, no imports resolved and no DllMain: the vtable's pointers and the exports agree.
    HMODULE srv = LoadLibraryExA(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    CHECK(srv != nullptr);
    if (!srv)
        return;
    void** vt = reinterpret_cast<void**>(GetProcAddress(srv, "??_7IServerImpl@@6BIServer@@@"));
    void** chat = reinterpret_cast<void**>(GetProcAddress(srv, "??_7IServerImpl@@6BIChatServer@@@"));
    CHECK(vt != nullptr && chat != nullptr);
    if (vt && chat)
    {
        for (const iserver::SlotExport& e : iserver::kExports)
        {
            void* fn = reinterpret_cast<void*>(GetProcAddress(srv, e.mangled));
            CHECK(fn != nullptr);
            if (fn && vt[e.offset / 4] != fn)
            {
                CHECK(vt[e.offset / 4] == fn);
                fprintf(stderr, "  iserver: %s is not at slot 0x%X\n", e.method, e.offset);
            }
        }
        // Connect shares its body with ReqCargo: both slots hold the one export.
        CHECK(vt[iserver::kConnect / 4] == vt[0x74 / 4]);
        CHECK(chat[iserver::kSubmitChat / 4] == reinterpret_cast<void*>(GetProcAddress(srv, "?SubmitChat@IServerImpl@@UAEXUCHAT_ID@@KPBX0H@Z")));
    }
    FreeLibrary(srv);
}
