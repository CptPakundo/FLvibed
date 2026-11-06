// modules\subtitles\src\impl\chat_size.h (pure): the side chat's em heights per `chat_size` - 1 Large = the 0.2.0 size (0.034 /
// 0.030 H), 0 Normal smaller but still above the vanilla chat text (cap 0.0156 H; Agency FB's cap ~0.64 em), the name in
// the same proportion to the line; out of range = Normal.
#include "test.h"

#include "../modules/subtitles/src/impl/chat_size.h"

void Test_chat_size()
{
    using chatsize::ForSetting;
    CHECK(ForSetting(1).line == 0.034f && ForSetting(1).name == 0.030f);
    const chatsize::Em n = ForSetting(0);
    CHECK(n.line < ForSetting(1).line && n.name < ForSetting(1).name);
    CHECK(n.line * 0.64f > 0.0156f * 1.1f && n.line * 0.64f < 0.0156f * 1.25f); // vanilla x ~1.1-1.25 (cap)
    CHECK(n.name / n.line > 0.87f && n.name / n.line < 0.89f);                    // the mock-up's 0.030 / 0.034
    CHECK(ForSetting(-1).line == n.line && ForSetting(2).line == n.line);
}
