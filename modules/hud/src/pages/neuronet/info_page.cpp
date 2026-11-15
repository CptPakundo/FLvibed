// FLvibed - the INFO page's description (audit 70b F2a; the behaviour of nnpage.cpp's INFO as it was): it claims the
// game's NN_Help window and parks it off the screen by its own setter for as long as the page stands; its model is the text
// the window would show, read on the open and when the ship takes a new target (core\game\screens\nnhelp_screen.*); Y shows
// the map; its text scrolls 15 lines a second on a held D-pad or right stick (pageinput.h kInfoTextRepeat).
#include "info_page.h"

#include "../pagelist.h"
#include "game/screens/nnhelp_screen.h"

namespace infopage
{
static void Init(void* m) { new (m) infobody::Model(); }
static void Done(void* m) { static_cast<infobody::Model*>(m)->~Model(); }
static bool Read(void* m, u32) { return infobody::Read(*static_cast<infobody::Model*>(m)); }
static void Visit(const void* m, page::FieldVisitor& v)
{
    const infobody::Body& b = static_cast<const infobody::Model*>(m)->body;
    v.Field("case", static_cast<double>(b.kase));
    v.Field("target", b.target);
    v.Text("title", b.title.c_str());
    v.Field("paras", static_cast<double>(b.paras.size()));
}
static void Game(u32 root, std::wostream& out)
{
    infobody::Body b;
    if (infobody::ReadWindow(root, b))
        out << infobody::Dump(b);
}

static const pageinput::Binding kPad[] = {
    {XINPUT_GAMEPAD_Y, 0, infobody::kActShowOnMap, L"SHOW ON MAP", false, pageinput::Role::Action, 0},
};

const page::PageDesc kInfo = {
    {kId, "neuronet", page::Kind::Page, {infobody::kWindowClass, infobody::kWindowName, nullptr, nullptr}, page::Park::Root,
     page::Pause::World, kPad, static_cast<int>(sizeof(kPad) / sizeof(kPad[0])), nullptr, 0, infobody::kActions,
     infobody::kActionCount, nullptr, &pageinput::kInfoTextRepeat,
     // (the D-pad's sides, A and X do nothing on INFO and do not reach the game, as before)
     XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X},
    {sizeof(infobody::Model), Init, Read, infobody::Stamp, Visit, Game, 0, Done},
    View,
};
PAGE_REGISTER(kInfo);
} // namespace infopage
