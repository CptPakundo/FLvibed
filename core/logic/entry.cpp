// FLvibed - the logic's entry (core\loader.h): every module's table in one place. Linked into FLvibed.dll in the release
// build, FLvibedLogic.dll's export in the dev build.
#include "loader.h"

extern "C" const void* flv_subtitles_impl_api();
extern "C" const void* flv_controller_impl_api();
extern "C" const void* flv_core_impl_api();
extern "C" const void* flv_movies_impl_api();

static const LogicApi* Api()
{
    static const LogicApi api = {kLogicApiVersion, {flv_subtitles_impl_api(), flv_controller_impl_api(), flv_core_impl_api(), flv_movies_impl_api()}};
    return &api;
}

#ifdef FLV_STATIC
extern "C" const LogicApi* flv_logic_api() { return Api(); }
#else
extern "C" __declspec(dllexport) const LogicApi* __cdecl GetLogicApi() { return Api(); }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
#endif
