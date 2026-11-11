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
    // the hover help's place: where the game put it when nothing stands there; else the nearest free place, inside the area
    {
        const TipRect area = {64, 40, 1216, 760}, want = {110, 0, 400, 110}; // the room bar's long hint at 1280x800, under the strip's card
        TipRect out;
        CHECK(TipPlace(want, area, nullptr, 0, 8, out) && out.x0 == 110 && out.y0 == 40 && out.y1 == 150);
        const TipRect card = {0, 0, 350, 160};
        CHECK(TipPlace(want, area, &card, 1, 8, out) && out.y0 == 168 && out.x0 == 110); // under the card: nearer than right of it
        const TipRect two[2] = {{0, 0, 350, 160}, {80, 160, 500, 400}}; // pushed under the card onto a second block: past both
        CHECK(TipPlace(want, area, two, 2, 8, out) && !TipOverlaps(out, two[0]) && !TipOverlaps(out, two[1]) && out.x0 >= area.x0 && out.x1 <= area.x1);
        const TipRect all = {0, 0, 1280, 800}; // nowhere free: the wanted place inside the area
        CHECK(!TipPlace(want, area, &all, 1, 8, out) && out.x0 == 110 && out.y0 == 40);
        const TipRect lane = {0, 720, 1280, 800}, low = {600, 700, 700, 740};
        CHECK(TipPlace(low, area, &lane, 1, 8, out) && out.y1 <= 712 && out.x0 == 600); // above the prompt lane
    }
    // a HUD 2 word's help (issue 71): the short tip only where it says more than the word
    {
        CHECK(SameWords(L"BAR", L"Bar") && SameWords(L"SHIP DEALER", L"Ship Dealer") && SameWords(L"SHIP DEALER", L"ShipDealer"));
        CHECK(!SameWords(L"TRADER", L"Commodity Trader") && !SameWords(L"LOG", L"Neural Net Log (F5)") && !SameWords(L"BAR", L"") && SameWords(L"", L" "));
        CHECK(WordHelpText(L"BAR", L"Bar", L"The Bar is the place ...") == L"The Bar is the place ...");
        CHECK(WordHelpText(L"LOG", L"Neural Net Log (F5)", L"Mission objectives.") == L"Neural Net Log (F5)\nMission objectives.");
        CHECK(WordHelpText(L"TRADER", L"Commodity Trader", L"") == L"Commodity Trader");
        CHECK(WordHelpText(L"SHIP DEALER", L"Ship Dealer", L"").empty() && WordHelpText(L"NEWS", L"", L"").empty());
    }
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
