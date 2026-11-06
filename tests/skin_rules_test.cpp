// modules\hud\src\skin_rules.h (pure): the skin's sizes, grammar, dialog rows, cursor set, loading and CONTINUE rules (UI_HUD2_SKIN.md 4, 6.2-6.7, 8).
#include "../modules/hud/src/skin_rules.h"

#include "test.h"

void Test_skin_rules()
{
    using namespace skinrules;
    // sizes
    CHECK(ChipHeight(0.010f) == kChipLow && ChipHeight(0.200f) == kChipHigh && ChipHeight(0.050f) == 0.050f);
    CHECK(FlashOk(2.9f) && !FlashOk(3.0f) && !FlashOk(0));
    // pips: anything above zero shows, nothing below it, never past n
    CHECK(PipsLit(0, 10) == 0 && PipsLit(-1, 10) == 0 && PipsLit(0.001f, 10) == 1 && PipsLit(0.5f, 10) == 5 && PipsLit(2, 10) == 10 && PipsLit(1, 0) == 0);
    // the accent edge faces the eye; a dialog keeps its top
    CHECK(AccentEdge(100, 500, false) == Edge::Right && AccentEdge(900, 500, false) == Edge::Left && AccentEdge(100, 500, true) == Edge::Top);
    CHECK(HoldShare(0.5f, 1, true) == 0.5f && HoldShare(2, 1, true) == 1 && HoldShare(0.5f, 1, false) == 0 && HoldShare(1, 0, true) == 0);
    // R9: only what +0x348 reaches, and the skin's own chips
    CHECK(WordTakesInk(WordClass::TextControl) && WordTakesInk(WordClass::SkinChip) && !WordTakesInk(WordClass::BoxButton) && !WordTakesInk(WordClass::RichText));
    char w[40];
    CHECK(FocusWord("PauseButtonBox3", w, sizeof(w)) && !strcmp(w, "PauseButtonText3"));
    CHECK(FocusWord("ItemView02", w, sizeof(w)) && !strcmp(w, "ItemName02"));
    CHECK(FocusWord("ItemButton2", w, sizeof(w)) && !strcmp(w, "ItemName02"));
    CHECK(FocusWord("MissionVendorSummaryBox4", w, sizeof(w)) && !strcmp(w, "MissionVendorSummaryTitle4"));
    CHECK(!FocusWord("Confirm_delBtnYes", w, sizeof(w)) && !FocusWord("ItemView", w, sizeof(w)));
    // motion: FULL and FADES ease, NONE cuts, a bad value is FULL
    CHECK(ClampMotion(7) == MotionFull && ClampMotion(2) == MotionNone);
    CHECK(fabsf(EaseStep(0, true, 0.075f, MotionFull) - 0.5f) < 0.001f && EaseStep(0, true, 0.5f, MotionFades) == 1 && EaseStep(1, false, 0.5f, MotionFull) == 0);
    CHECK(EaseStep(0, true, 0, MotionNone) == 1 && EaseStep(1, false, 0, MotionNone) == 0);
    // skin_native
    CHECK(NativeListed("navmap", "navmap") && NativeListed(" pause , NavMap ", "navmap") && !NativeListed("navmapx", "navmap") && !NativeListed("", "navmap"));
    CHECK(!NativeListed("navmap", "") && !NativeListed(",,", "pause"));
    // dialogs: the verbs of 6.2, the game's words for any other text
    std::wstring name;
    CHECK(ConfirmKind(L"QUIT TO MAIN MENU", nullptr) == DialogKind::Quit);
    CHECK(ConfirmKind(L"ARE YOU SURE YOU WANT TO DELETE Freeport 7?", &name) == DialogKind::Delete && name == L"Freeport 7");
    CHECK(ConfirmKind(L"Do you want to leave this server and choose another?", nullptr) == DialogKind::Other);
    CHECK(ConfirmKind(L"SPIEL BEENDEN", nullptr) == DialogKind::Other && ConfirmKind(nullptr, nullptr) == DialogKind::Other);
    CHECK(ExitKind(L"EXIT GAME?", true) == DialogKind::ExitMenu && ExitKind(L"EXIT GAME?", false) == DialogKind::ExitInGame && ExitKind(L"x", true) == DialogKind::Other);
    CHECK(Row(DialogKind::Quit)->amber && !Row(DialogKind::Quit)->focusAction && !wcscmp(Row(DialogKind::Quit)->safe, L"KEEP PLAYING"));
    CHECK(!Row(DialogKind::ExitMenu)->amber && Row(DialogKind::ExitMenu)->focusAction);
    CHECK(Row(DialogKind::Delete)->amber && !Row(DialogKind::Delete)->cost && Row(DialogKind::Other) == nullptr);
    // the plate holds its chips and words, on the screen, or the dialog stays the game's
    DialogIn in;
    in.W = 1280, in.H = 800, in.chipsX0 = 400, in.chipsX1 = 880, in.chipsY0 = 420, in.chipsY1 = 460, in.titleW = 300, in.costW = 350;
    DialogBox b;
    CHECK(DialogFit(in, b) && b.x0 < 400 && b.x1 > 880 && b.y1 > 460 && b.y0 < b.titleY && b.titleY < b.costY && b.costY < 420 && fabsf((b.x0 + b.x1) / 2 - 640) < 1);
    in.titleW = 1400;
    CHECK(!DialogFit(in, b));
    DialogIn none;
    none.W = 1280, none.H = 800, none.centreY = 400, none.titleW = 400;
    CHECK(DialogFit(none, b) && b.y0 < 400 && b.y1 > 400);
    // loading
    CHECK(!LoadBarShows(1.49f) && LoadBarShows(1.5f));
    CHECK(LoadShare(0.2f, 0.5f) == 0.5f && LoadShare(0.9f, 0.1f) == 0.9f && LoadShare(0.4f, 7) == 0.4f); // a wrap holds, a bad read holds
    // the cursor set: the game's 19 names, a mod's stays the game's
    CHECK(CursorFor("ARROW") && CursorFor("arrow")->shape == CursorShape::Arrow && !CursorFor("my_mod_cursor") && !CursorFor(nullptr));
    CHECK(CursorFor("hostile")->form == FormCorners && CursorFor("fire")->form == FormSpokes && CursorFor("no_fire")->form == FormBar);
    CHECK(!wcscmp(CursorFor("talk_mission")->word, L"JOB") && !wcscmp(CursorFor("talk_equip_dealer")->word, L"GEAR"));
    for (int i = 0; i < kCursorCount; ++i)
    {
        CHECK(CursorRows()[i].shape != CursorShape::Bubble || CursorRows()[i].word);
        for (int j = i + 1; j < kCursorCount; ++j)
            CHECK(strcmp(CursorRows()[i].name, CursorRows()[j].name) != 0);
    }
    CHECK(fabsf(CursorSize(1920) / 1080 - 0.052f) < 0.0001f);
    // CONTINUE
    CHECK(LatestSave(nullptr, 0) == -1);
    SaveStamp s[3] = {{"a.fl", 10, false}, {"b.fl", 30, true}, {"c.fl", 20, false}};
    CHECK(LatestSave(s, 3) == 2);
    s[2].hidden = s[0].hidden = true;
    CHECK(LatestSave(s, 3) == -1);
    CHECK(MenuSlotY(100, 600, 0) == 100 && MenuSlotY(100, 600, 5) == 600 && MenuSlotY(100, 600, 1) == 200);
    // base text: one rung at 800, none at the ship dealer's pitch
    CHECK(NameRungs(800, 0.068f) == 1 && NameRungs(2160, 0.068f) == 0 && NameRungs(800, 0.065f) == 0);
}
