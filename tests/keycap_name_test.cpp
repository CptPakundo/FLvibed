// The K&M keycaps' punctuation names (modules\hud\src\keycap_name.h; audit 133_ item 8): an OEM key is named by the
// layout passed, so the default layout gives the same name whatever layout the thread holds. Only layouts already loaded
// in this session are used (no LoadKeyboardLayout / ActivateKeyboardLayout: either would touch the user's input list).
#include "../modules/hud/src/keycap_name.h"

#include "test.h"

void Test_keycap_name()
{
    using namespace keycapname;
    // the marks' words
    CHECK(std::wstring(MarkWord(L'`')) == L"~" && std::wstring(MarkWord(L'~')) == L"~");
    CHECK(std::wstring(MarkWord(0x00B4)) == L"ACUTE" && std::wstring(MarkWord(L',')) == L"COMMA");
    CHECK(MarkWord(L';') == nullptr && MarkWord(L'A') == nullptr);
    // only the OEM keys take the character path
    CHECK(IsOemKey(VK_OEM_3) && IsOemKey(VK_OEM_1) && IsOemKey(VK_OEM_102) && IsOemKey(VK_OEM_PERIOD));
    CHECK(!IsOemKey('A') && !IsOemKey(VK_NUMPAD0) && !IsOemKey(VK_F1) && !IsOemKey(VK_SPACE));
    CHECK(OemName('A', DefaultLayout()).empty() && OemName(VK_OEM_3, nullptr).empty());
    // the default layout answers, and twice the same (no thread state in it)
    const HKL def = DefaultLayout();
    CHECK(def != nullptr);
    CHECK(OemName(VK_OEM_3, def) == OemName(VK_OEM_3, def));
    // the loaded layouts: a US layout (low word's language 0x0409 or the layout id 0409) names the grave key "~";
    // a Hebrew one (0x040D) ";" - the two names one key took start to start before the fix (one prints per layout)
    HKL list[16] = {};
    const int n = GetKeyboardLayoutList(16, list);
    for (int i = 0; i < n; ++i)
    {
        const unsigned lang = LOWORD(reinterpret_cast<UINT_PTR>(list[i])), dev = HIWORD(reinterpret_cast<UINT_PTR>(list[i]));
        const std::wstring g = OemName(VK_OEM_3, list[i]);
        CHECK(!g.empty());
        if (dev == 0x0409 || (lang == 0x0409 && (dev & 0xF000) != 0xF000 && dev == lang))
            CHECK(g == L"~");
        if (lang == 0x040D && dev == 0x040D)
            CHECK(g == L";");
    }
}
