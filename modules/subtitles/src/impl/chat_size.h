// The side chat's text size (pure; tests\chat_size_test.cpp; the owner, 2 Nov 2026, for 0.2.1: "the current side chat
// size should be considered 'Large' ... a new default 'Normal' which is smaller ... still somewhat larger than the
// original side chat text size of the vanilla game"). HUD 2's chat feed (display.cpp DrawFeed) draws its line and name
// fonts scaled to these em heights as a share of the screen height, whatever subtitle size is set. Setting `chat_size`.
//
// The measurement (research\evidence\controller_ui\mockup\img\cap, 1600x900): the game's own chat text (the chat
// window's "Console->" line, x_chat_space_cl.jpg) has a cap height of 14 px = 0.0156 H; the feed at 0.034 H
// (x_chat_space_h2.jpg, the feed's "Never send ...") about 21 px = 0.023 H, i.e. 1.47x vanilla. Normal = 0.027 H, about
// 1.17x vanilla; the name keeps the mock-up's 0.030 / 0.034 proportion.
#pragma once

namespace chatsize
{
struct Em
{
    float line, name; // em heights as a share of the screen height
};

// 0 Normal (default), 1 Large (the size HUD 2's feed shipped with in 0.2.0); out of range = Normal (fails to the default)
constexpr Em ForSetting(int chatSize)
{
    return chatSize == 1 ? Em{0.034f, 0.030f} : Em{0.027f, 0.0238f};
}
} // namespace chatsize
