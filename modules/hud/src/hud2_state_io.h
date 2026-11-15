// HUD 2's state as text (audit 71b B4a): every struct the frame takes in or carries (hud2_frame.h FrameIn, DrawIn,
// FrameState and all they hold) lists its fields once, in a `Fields(v, x)` function below, and one list serves every
// direction - the writer, the reader, the sentinel filler and the layout check of tests\hud2_state_test.cpp - so they
// cannot drift apart. A recorded frame (`hud2 bench record`, hud2_record.cpp, B4b: both halves' inputs and states, the
// host's answers on a tape, the fingerprints of its display list) and the design bench's input (B5a) are `StateDoc`s
// written by `Write` and read back by `Read`.
//
// The guards that keep the lists whole: a pin per struct (FLV_STATE_PIN at the end: its sizeof and its member count as
// the compiler reads it, so a member added or removed anywhere fails the build - the unit build, and the DLL's through
// the recorder - until the pin is set again, even one that
// fits in old padding), and the unit test's layout check (per struct, the members listed equal that count and no two of
// them share a byte; hud2target::State, a class with private members and so no count, lists its own members and is held
// by its sizeof and by having no unlisted bytes beyond padding - a missing member of 1-3 bytes before padding there is
// the one gap). The unit test also prints every pin (FLVIBED_STATE_PINS=1).
//
// The format (schema "flstate/0"): UTF-8 JSON, one object per file, the members of an object in the order of its Fields
// list. Numbers: integers in decimal, floats "%.9g" and doubles "%.17g" (each reads back bit for bit), a float that is not
// finite as the string "nan", "inf" or "-inf"; booleans true / false; wide strings as UTF-8; enums as their integer; a
// fixed array, and a vector, as a JSON array (a fixed one of its full length). Three members are written by meaning, not by memory: the model's
// preset data (a pointer into ui_meta.h's kPresets: its index), the preset adapter's compatibility cell (a copy of a
// kCompat cell: its index, row-major), and the draw kit's marquee table (an unordered map: its entries sorted by key).
// Reading fails closed: an unknown, missing, repeated or misordered member, a value of the wrong kind or out of range,
// an array of another length, text after the object -> false and the reason; a partial state is never taken for whole.
//
// Part of HUD 2's game-free core (tools\module_lint.py BENCH_CORE): no game header, no mutable static.
#pragma once
#include "hud2_frame.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace hud2state
{
namespace hp = uimeta::hud2p;

// ---- a recorded frame (B4b: `hud2 bench record`, hud2_record.cpp; the bench replays it, B5a)
// The host's answers one half of the frame used (hud2_frame.h FrameHost), in call order per kind: a program with no game
// answers each call with the next value of its kind, and a count that differs fails the replay.
struct Tape
{
    std::vector<unsigned> tickMs;            // tickMs()
    std::vector<unsigned long long> tickMs64; // tickMs64()
    std::vector<double> seconds;             // seconds() (the parts' and the kit's marquee clock)
    std::vector<int> typing;                 // typing(): 0 / 1
    std::vector<int> hovered;                // hotHovered()
};
// A clickable rectangle the parts gave the hotspot sink while they drew (FrameHost hotRect): Draw's output.
struct HotRect
{
    int slot = 0;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
// Update's half: what StepStart decided and asked, what Step asked, the host's answers Step used.
struct UpdateRec
{
    hud2::Phase phase = hud2::Phase::Away;
    hud2::StartOut start;
    hud2::StepOut step;
    Tape tape; // during Step
};
// Render's half: the kit's pass as the host opened it, the state Draw took (after the host's relayout to the back
// buffer and the target's projection on the screen), the host's answers Draw used and the hotspots it gave.
struct RenderRec
{
    bool drawn = false;      // HUD 2's pass drew in this frame (false: the rest is empty)
    int sw = 0, sh = 0;      // the back buffer, the pass's size (audit 71b row 21; the layout re-runs when it is not the layout's)
    int askPx = 0;           // the text size the host asked the kit's Begin for (KitTextPx of the layout)
    int drawPx = 0;          // the kit's size after its legibility floor (hud2kit::TextPx), the pass drew with it
    bool iconsReady = false; // the icon atlas stood (flicons::Ready, row 27): without it the parts draw shapes
    std::wstring kitExtra;   // the characters the kit's atlases gained on demand (hud2kit::AddedChars, row 31): seeded
                             // before a replay's first pass, or its text draws stand-ins
    hud2::FrameState st;     // the state Draw took
    Tape tape;               // during Draw
    int hotOn = -1;          // the hotspot sink's start (FrameHost hotStart: 1 recording, 0 not; -1 not called)
    std::vector<HotRect> hot;
};
// The under pass (the backings under the game's 3D views): what DrawUnder took.
struct UnderRec
{
    bool drawn = false;
    int sw = 0, sh = 0, px = 0;
    bool back[2] = {};
    hud2::ViewBack viewBack[2] = {};
};
// One layer of a pass as the kit submitted it: its vertex count and the FNV-1a 64 of its vertices (`hud2 kitdump`'s).
struct LayerPrint
{
    unsigned count = 0;
    std::string hash;
};
// A font set the list draws with and the FNV-1a 64 of its fingerprint block (the list's `sets` carries the same).
struct SetPrint
{
    std::string name, print;
};
// What ties the frame's display list to this record, and the record to the game's frame.
struct ListRef
{
    std::string file;   // the list (fldl/0) beside this file
    std::string hash;   // FNV-1a 64 of the list file's bytes
    std::string inputs; // FNV-1a 64 of draw_in and render as written here: the list carries it as record.inputs
    std::vector<SetPrint> sets;
    std::vector<LayerPrint> under, over; // six each (an under pass not drawn: none)
};

// What one state file holds: the frame's inputs and its carried state (B4a), and for a recorded frame (B4b) both halves'
// records, the host's answers and the fingerprints. Update's state `st` is the one Step took (after StepStart and the
// host's model refresh); Render's its own (render.st).
struct StateDoc
{
    bool priv = true;   // the private marker (tools\private_check.py): game names, the player's bindings; false = "public"
    std::string build;  // the build that recorded it (build_info.txt's first word)
    std::string state;  // the state's name: a sim's (hud2_sim.h) or "live"
    unsigned frame = 0; // the frame's number in its recording (0 first)
    hud2::FrameIn in;   // Update's input
    hud2::DrawIn draw;  // Render's input
    hud2::FrameState st; // the state carried into the frame (Step's)
    UpdateRec update;
    RenderRec render;
    UnderRec under;
    ListRef list;
};

// The private marker, assembled so that this source never holds it whole (tools\private_check.py would flag the file).
inline std::string PrivateMarker() { return std::string("flbench") + "-" + "private"; }

// ---- the fields of every struct (one list each; a visitor `v` is called as v("name", member))
// v.group(name, member, f): an unnamed struct member, its fields listed by f(v, member). v.note(member): a member written by
// meaning through a stand-in (the layout check counts its bytes as listed). V::kSets: the visitor writes into the object
// (the reader, the filler); V::kFills: the sentinel filler.

template <class V> void Fields(V& v, HudRect& x)
{
    v("x0", x.x0), v("y0", x.y0), v("x1", x.x1), v("y1", x.y1);
}
template <class V> void Fields(V& v, HudSubtitles& x)
{
    v("frame", x.frame), v("dialogue", x.dialogue), v("chat", x.chat);
}
template <class V> void Fields(V& v, presets::Binding& x)
{
    v("tap", x.tap), v("hold", x.hold), v("dbl", x.dbl);
}
template <class V> void Fields(V& v, presets::Table& x)
{
    v("b", x.b);
}
template <class V> void Fields(V& v, views::HudContext& x)
{
    v("space", x.space), v("space_ui", x.spaceUi), v("cutscene", x.cutscene), v("pad", x.pad), v("bot", x.bot);
    v("table", x.table), v("autopilot", x.autopilot), v("turret", x.turret), v("weapon_group", x.weaponGroup);
    v("ap_known", x.apKnown), v("modal", x.modal), v("steer", x.steer), v("firing", x.firing);
}
template <class V> void Fields(V& v, uiopt::UiOptions& x)
{
    v("input_mode", x.inputMode), v("glyph_style", x.glyphStyle), v("glyph_look", x.glyphLook);
    v("confirm_button", x.confirmButton), v("pause_on_disconnect", x.pauseOnDisconnect), v("pad_preset", x.padPreset);
    v("weapon_groups", x.weaponGroups), v("y_swap", x.swapY), v("holds_as_toggles", x.holdsAsToggles), v("rumble", x.rumble);
    v("trigger_effects", x.triggerEffects), v("trigger_strength", x.triggerStrength), v("haptics_mode", x.hapticsMode);
    v("haptics_class", x.hapticsClass), v("haptics_device", x.hapticsDevice), v("pad_triggers", x.padTriggers);
    v("pad_cursor", x.padCursor), v("cursor_magnet", x.cursorMagnet), v("map_touchpad", x.mapTouchpad);
    v("prompt_bar", x.promptBar), v("world_prompts", x.worldPrompts), v("ap_hints", x.apHints), v("text_size", x.textSize);
    v("hud_glyphs", x.hudGlyphs), v("button_legend", x.buttonLegend), v("ap_status_line", x.apStatusLine);
    v("hud_style", x.hudStyle), v("hud_fade", x.hudFade), v("hud_opacity", x.hudOpacity), v("hud_scale", x.hudScale);
    v("hud_numbers", x.hudNumbers), v("hud_threats", x.hudThreats), v("hud_context", x.hudContext);
    v("hud_prompts", x.hudPrompts), v("hud_alerts", x.hudAlerts), v("hud_info", x.hudInfo);
    v("hud_wireframes", x.hudWireframes), v("hud_transition", x.hudTransition), v("hud_preset", x.hudPreset);
}
template <class V> void Fields(V& v, hudrows::ShieldCue& x)
{
    v("on", x.on), v("intensity", x.intensity), v("hull", x.hull), v("pattern", x.pattern);
}
template <class V> void Fields(V& v, hudrows::ReticleCue& x)
{
    v("on", x.on);
}
template <class V> void Fields(V& v, hudrows::Values& x)
{
    v("shield", x.shield), v("reticle", x.reticle), v("game_text_size", x.gameTextSize), v("window_text", x.windowText);
    v("nn_pause", x.nnPause), v("hud_style", x.hudStyle), v("hud_fade", x.hudFade);
    v("hud_opacity", x.hudOpacity), v("hud_scale", x.hudScale), v("hud_numbers", x.hudNumbers), v("hud_context", x.hudContext);
    v("hud_prompts", x.hudPrompts), v("hud_threats", x.hudThreats), v("hud_alerts", x.hudAlerts), v("hud_info", x.hudInfo);
    v("hud_wireframes", x.hudWireframes), v("group_cue", x.groupCue), v("hud_transition", x.hudTransition);
    v("hud_preset", x.hudPreset), v("skin_motion", x.skinMotion);
}
template <class V> void Fields(V& v, hud2::SizeIn& x)
{
    v("deck", x.deck), v("base_k", x.baseK), v("floor_h", x.floorH);
}
template <class V> void Fields(V& v, hud2::PartsIn& x)
{
    v("opt", x.opt), v("rows", x.rows), v("family", x.family), v("size", x.size);
}
template <class V> void Fields(V& v, hud2::SubsIn& x)
{
    v("read", x.read), v("rec", x.rec), v("frame", x.frame);
}
template <class V> void Fields(V& v, hud2::FrameIn& x)
{
    v("dt", x.dt), v("ctx", x.ctx), v("parts", x.parts), v("want", x.want), v("game_loads", x.gameLoads);
    v("built", x.built), v("combat", x.combat), v("target_id", x.targetId), v("box_up", x.boxUp), v("box", x.box);
    v("accept", x.accept), v("cancel", x.cancel), v("letterbox", x.letterbox), v("mouse_x", x.mouseX), v("mouse_y", x.mouseY);
    v("screen_w", x.screenW), v("screen_h", x.screenH), v("mouse_flight", x.mouseFlight), v("fire_key", x.fireKey);
    v("subs", x.subs), v("view_obj", x.viewObj);
}
template <class V> void Fields(V& v, hud2::DrawIn& x)
{
    v("parts", x.parts), v("combat", x.combat), v("dim", x.dim), v("view_shown", x.viewShown), v("form_on", x.formOn);
    v("prompt_visible", x.promptVisible);
}

// the model (hud2_impl.h) and what it holds
template <class V> void Fields(V& v, hud2preset::Setup& x)
{
    v("pad_preset", x.padPreset), v("weapon_groups", x.weaponGroups), v("holds_as_toggles", x.holdsAsToggles), v("kbm", x.kbm);
}
template <class V> void Fields(V& v, hud2preset::Needs& x)
{
    v("throttle_word", x.throttleWord), v("kill_word", x.killWord), v("throttle_readout", x.throttleReadout);
    v("ap_line", x.apLine), v("take_control_pinned", x.takeControlPinned), v("reticle_always", x.reticleAlways);
    v("group_rows", x.groupRows), v("latched_words", x.latchedWords), v("keycaps", x.keycaps);
}
// A copy of a kCompat cell, as the cell's index (row-major); a copy of no cell writes -1, which no reader takes.
template <class V> void Fields(V& v, hud2preset::Adapt& x)
{
    v("setup", x.setup), v("column", x.column);
    constexpr int kRows = static_cast<int>(sizeof(hp::kCompat) / sizeof(hp::kCompat[0]));
    constexpr int kCols = static_cast<int>(sizeof(hp::kCompat[0]) / sizeof(hp::kCompat[0][0]));
    int cell = -1;
    for (int i = 0; i < kRows * kCols && cell < 0; ++i)
    {
        const hp::Compat& c = hp::kCompat[i / kCols][i % kCols];
        if (c.fit == x.compat.fit && c.suggested == x.compat.suggested && x.compat.note && strcmp(c.note, x.compat.note) == 0)
            cell = i;
    }
    v.note(x.compat);
    if constexpr (V::kFills)
        cell = (cell + 1) % (kRows * kCols); // (the filler: another cell)
    else
        v("compat_cell", cell);
    if constexpr (V::kSets)
    {
        if (cell >= 0 && cell < kRows * kCols)
            x.compat = hp::kCompat[cell / kCols][cell % kCols];
        else
            v.fail("compat_cell", "no cell of the compatibility table");
    }
    v("needs", x.needs), v("suggested", x.suggested);
}
template <class V> void Fields(V& v, hud2::Prompt& x)
{
    v("btn", x.btn), v("hold", x.hold), v("key", x.key), v("label", x.label), v("side", x.side);
}
template <class V> void Fields(V& v, hud2::AlertCard& x)
{
    v("text", x.text), v("none", x.none), v("col", x.col), v("prompts", x.prompts), v("n", x.n), v("left", x.left);
    v("side", x.side), v("y_now", x.yNow);
}
template <class V> void Fields(V& v, hud2bars::BloomOut& x)
{
    v("on", x.on), v("light", x.light), v("reach", x.reach), v("pips", x.pips);
}
template <class V> void Fields(V& v, hud2bars::Breath& x)
{
    v("on", x.on), v("phase", x.phase), v("rate", x.rate), v("depth", x.depth), v("quiet", x.quiet), v("last", x.last);
}
template <class V> void Fields(V& v, hud2bars::Bloom& x)
{
    v("age", x.age), v("since", x.since), v("wait", x.wait);
}
template <class V> void Fields(V& v, hud2::BarFx& x)
{
    v("glow_shield", x.glowShield), v("glow_hull", x.glowHull), v("glow_boost", x.glowBoost);
    v("glow_t_shield", x.glowTShield), v("glow_t_hull", x.glowTHull), v("glow_escort", x.glowEscort);
    v("bloom_shield", x.bloomShield), v("bloom_hull", x.bloomHull), v("bloom_boost", x.bloomBoost);
    v("bloom_cruise", x.bloomCruise), v("bloom_t_shield", x.bloomTShield), v("bloom_t_hull", x.bloomTHull);
    v("cruise_fill", x.cruiseFill), v("beat", x.beat), v("shield_down", x.shieldDown), v("down_mix", x.downMix);
}
template <class V> void Fields(V& v, hud2::Model::Mark& x)
{
    v("x", x.x), v("y", x.y), v("hull", x.hull), v("name", x.name), v("dist", x.dist), v("col", x.col);
}
template <class V> void Fields(V& v, hud2::Model::CueMark& x)
{
    v("ch", x.ch), v("selected", x.selected), v("amber", x.amber), v("locked", x.locked), v("want", x.want);
    v("lit_a", x.litA), v("reload", x.reload);
}
template <class V> void Fields(V& v, hud2::Model::Threat& x)
{
    v("name", x.name), v("dist", x.dist), v("col", x.col), v("id", x.id), v("icon", x.icon);
}
template <class V> void Fields(V& v, hud2::Model::Escort& x)
{
    v("name", x.name), v("hull", x.hull);
}
template <class V> void Fields(V& v, hud2notice::Item& x)
{
    v("key", x.key), v("name", x.name), v("count", x.count), v("full", x.full);
}
template <class V> void Fields(V& v, hud2notice::Notice& x)
{
    v("credits", x.credits), v("cash", x.cash), v("gain", x.gain), v("items", x.items), v("n_items", x.nItems);
    v("extra", x.extra), v("n_extra", x.nExtra), v("left", x.left), v("alpha", x.alpha);
}
template <class V> void Fields(V& v, hud2::Model& x)
{
    v("state", x.state), v("kbm", x.kbm), v("preset", x.preset), v("preset_drawn", x.presetDrawn);
    // the preset's data: its index in kPresets (a pointer elsewhere writes -1, which no reader takes)
    int pd = -1;
    for (int i = 0; i < hp::kPresetCount; ++i)
        if (x.presetData == &hp::kPresets[i])
            pd = i;
    v.note(x.presetData);
    if constexpr (V::kFills)
        pd = (pd + 1) % hp::kPresetCount; // (the filler: another preset)
    else
        v("preset_data", pd);
    if constexpr (V::kSets)
    {
        if (pd >= 0 && pd < hp::kPresetCount)
            x.presetData = &hp::kPresets[pd];
        else
            v.fail("preset_data", "not an index of the presets");
    }
    v("adapt", x.adapt);
    // the ring
    v("hull", x.hull), v("shield", x.shield), v("power", x.power), v("speed", x.speed), v("cruise", x.cruise);
    v("charge", x.charge), v("charge_left", x.chargeLeft), v("burner", x.burner), v("kill", x.kill), v("reverse", x.reverse);
    v("throttle", x.throttle), v("tail", x.tail), v("tail_ang", x.tailAng), v("missiles", x.missiles);
    v("missile_ang", x.missileAng), v("hit_ang", x.hitAng), v("hit_age", x.hitAge);
    // nav
    v("objective", x.objective), v("protect", x.protect), v("man", x.man), v("man_name", x.manName), v("man_dest", x.manDest);
    v("dest_on_card", x.destOnCard), v("man_icon", x.manIcon), v("man_col", x.manCol), v("progress", x.progress);
    v("eta", x.eta), v("peek", x.peek), v("ap", x.ap), v("ap_mode", x.apMode), v("wait", x.wait), v("turret", x.turret);
    v("info", x.info), v("timer", x.timer), v("event", x.event), v("comm", x.comm), v("cine", x.cine), v("fight", x.fight);
    v("place", x.place), v("place_left", x.placeLeft), v("where", x.where), v("radiation", x.radiation), v("lag", x.lag);
    v("news", x.news), v("neuro", x.neuro), v("tractor_btn", x.tractorBtn), v("picker_btn", x.pickerBtn);
    v("wheel_btn", x.wheelBtn);
    // target
    v("target", x.target), v("target_id", x.targetId), v("t_name", x.tName), v("t_faction", x.tFaction);
    v("t_class", x.tClass), v("t_att", x.tAtt), v("t_dist", x.tDist), v("t_level", x.tLevel), v("t_col", x.tCol);
    v("t_bars", x.tBars), v("t_shield", x.tShield), v("t_hull", x.tHull), v("t_icon", x.tIcon), v("t_waypoint", x.tWaypoint);
    v.group("t_mark", x.tMark, [](auto& w, auto& t) { w("on_screen", t.onScreen), w("x", t.x), w("y", t.y), w("r", t.r); });
    v("marks", x.marks), v("n_marks", x.nMarks), v("fx", x.fx), v("shield_up", x.shieldUp);
    v("cue", x.cue), v("n_cue", x.nCue), v("cue_digits", x.cueDigits), v("cue_show", x.cueShow);
    v("cue_any_lit", x.cueAnyLit), v("boost", x.boost), v("cruise_wait", x.cruiseWait), v("t_view_ok", x.tViewOk);
    v("t_mission", x.tMission), v("t_view", x.tView), v("ship_view", x.shipView);
    // the threat list / the contact list
    v("threats", x.threats), v("n_threats", x.nThreats), v("filter", x.filter), v("threats_btn", x.threatsBtn);
    v("next_btn", x.nextBtn), v("list", x.list), v("list_filter", x.listFilter), v("list_first", x.listFirst);
    v("list_total", x.listTotal), v("list_max", x.listMax);
    // keyboard and mouse
    v("man_btn", x.manBtn), v("man_run", x.manRun), v("clear_btn", x.clearBtn), v("next_enemy_btn", x.nextEnemyBtn);
    v("group_btn", x.groupBtn);
    // ship panel
    v("escorts", x.escorts), v("escort_ids", x.escortIds), v("n_escorts", x.nEscorts), v("hold_used", x.holdUsed);
    v("hold_cap", x.holdCap), v("credits", x.credits), v("notice", x.notice), v("ship_name", x.shipName);
    v("ship_class", x.shipClass);
    // loadout
    v("nano", x.nano), v("batt", x.batt), v("cm", x.cm), v("mines", x.mines), v("hot", x.hot), v("hot_col", x.hotCol);
    v("slot_btn", x.slotBtn), v("groups", x.groups), v("group_num", x.groupNum), v("n_groups", x.nGroups);
    v("active_group", x.activeGroup), v("real_groups", x.realGroups), v("ungrouped", x.ungrouped), v("fire_btn", x.fireBtn);
    v("launch_btn", x.launchBtn), v("torpedo_btn", x.torpedoBtn), v("fire_btn2", x.fireBtn2), v("launcher", x.launcher);
    v("ammo", x.ammo), v("torpedo", x.torpedo), v("torpedo_ammo", x.torpedoAmmo), v("locked", x.locked);
    // context line
    v("ctx", x.ctx), v("n_ctx", x.nCtx), v("loot", x.loot), v("loot_name", x.lootName), v("loot_dist", x.lootDist);
    v("t_loot_reach", x.tLootReach), v("enemies", x.enemies), v("nearest_btn", x.nearestBtn);
    // alert
    v("alert", x.alert), v("alert_col", x.alertCol), v("alert_prompts", x.alertPrompts);
    v("n_alert_prompts", x.nAlertPrompts), v("alert_none", x.alertNone), v("alert_left", x.alertLeft);
    v("alert_side", x.alertSide), v("alert_y_now", x.alertYNow), v("held", x.held), v("alert_slots", x.alertSlots);
}

// the layout (hud2_impl.h Layout, hud2_layout.h Out) and what it holds
template <class V> void Fields(V& v, promptfit::HoldPlan& x)
{
    v("step", x.step), v("at", x.at);
}
template <class V> void Fields(V& v, hud2layout::Rect& x)
{
    v("x0", x.x0), v("y0", x.y0), v("x1", x.x1), v("y1", x.y1);
}
template <class V> void Fields(V& v, hud2layout::Reserved& x)
{
    v("at", x.at), v("quiet", x.quiet);
}
template <class V> void Fields(V& v, hud2::Layout& x)
{
    v("w", x.W), v("h", x.H), v("k", x.k), v("aspect", x.aspect), v("mx", x.mx), v("my", x.my), v("cx", x.cx), v("cy", x.cy);
    v("r", x.R), v("nav_x0", x.navX0), v("nav_y0", x.navY0), v("nav_x1", x.navX1), v("nav_y1", x.navY1);
    v("nav_max_x", x.navMaxX), v("tgt_x0", x.tgtX0), v("tgt_y0", x.tgtY0), v("tgt_x1", x.tgtX1), v("tgt_y1", x.tgtY1);
    v("thr_y0", x.thrY0), v("thr_y1", x.thrY1), v("ship_x0", x.shipX0), v("ship_y0", x.shipY0), v("ship_x1", x.shipX1);
    v("ship_y1", x.shipY1), v("lo_x0", x.loX0), v("lo_y0", x.loY0), v("lo_x1", x.loX1), v("lo_y1", x.loY1);
    v("cons_x0", x.consX0), v("cons_y0", x.consY0), v("cons_x1", x.consX1), v("cons_y1", x.consY1);
    v("ctx_y", x.ctxY), v("ctx_x0", x.ctxX0), v("ctx_x1", x.ctxX1), v("ctx_plan", x.ctxPlan), v("ctx_w", x.ctxW);
    v("ctx_x", x.ctxX), v("ctx_py", x.ctxPY), v("ctx_wrap", x.ctxWrap), v("ctx_tall", x.ctxTall), v("ctx_top", x.ctxTop);
    v("dlg_x0", x.dlgX0), v("dlg_y0", x.dlgY0), v("dlg_x1", x.dlgX1), v("dlg_y1", x.dlgY1);
    v("alert_y", x.alertY), v("alert_x0", x.alertX0), v("alert_x1", x.alertX1), v("alert_y1", x.alertY1);
    v("held_x", x.heldX), v("held_y", x.heldY), v("held_show", x.heldShow), v("list_in_card", x.listInCard);
    v("form", x.form), v("band_y", x.bandY), v("hold_y", x.holdY), v("clear_y0", x.clearY0);
    v("nt_x0", x.ntX0), v("nt_y0", x.ntY0), v("nt_x1", x.ntX1), v("nt_y1", x.ntY1), v("nt_show", x.ntShow);
    v("nt_wrap_w", x.ntWrapW), v("res", x.res);
}
template <class V> void Fields(V& v, hud2layout::Out& x)
{
    v("res", x.res), v("w", x.W), v("h", x.H), v("k", x.k), v("aspect", x.aspect), v("mx", x.mx), v("my", x.my);
    v("cx", x.cx), v("cy", x.cy), v("r", x.R), v("nav", x.nav), v("nav_max_x", x.navMaxX), v("card", x.card);
    v("thr_y0", x.thrY0), v("thr_y1", x.thrY1), v("ship", x.ship), v("loadout", x.loadout), v("cons", x.cons);
    v("cons_cells", x.consCells), v("n_ctx", x.nCtx), v("ctx_y", x.ctxY), v("ctx_x0", x.ctxX0), v("ctx_x1", x.ctxX1);
    v("ctx_top", x.ctxTop), v("ctx_plan", x.ctxPlan), v("ctx_w", x.ctxW), v("ctx_x", x.ctxX), v("ctx_py", x.ctxPY);
    v("ctx_side", x.ctxSide), v("ctx_wrap", x.ctxWrap), v("ctx_tall", x.ctxTall), v("ctx_fallback", x.ctxFallback);
    v("alert_y", x.alertY), v("alert_x0", x.alertX0), v("alert_x1", x.alertX1), v("alert_y1", x.alertY1);
    v("held_x", x.heldX), v("held_y", x.heldY), v("held_show", x.heldShow), v("ring_bus", x.ringBus);
    v("top_free", x.topFree), v("chat_free", x.chatFree), v("suppressed", x.suppressed), v("budget_off", x.budgetOff);
    v("boxed", x.boxed), v("budget", x.budget), v("held", x.held), v("unplated", x.unplated);
    v("list_in_card", x.listInCard), v("form", x.form), v("band_y", x.bandY), v("hold_y", x.holdY);
    v("ship_top", x.shipTop), v("clear_y0", x.clearY0), v("notice", x.notice), v("notice_show", x.noticeShow);
    v("notice_wrap_at", x.noticeWrapAt);
}
template <class V> void Fields(V& v, hud2layout::RevealRules& x)
{
    v("level", x.level), v("auto_peek", x.autoPeek), v("wake_s", x.wakeS), v("peek_s", x.peekS);
    v("fight_linger_s", x.fightLingerS), v("form_linger_s", x.formLingerS), v("form_tau", x.formTau);
    v("hull_below_full", x.hullBelowFull), v("shield_recharge", x.shieldRecharge), v("speed_show_s", x.speedShowS);
    v("speed_change_frac", x.speedChangeFrac), v("speed_change_hold_s", x.speedChangeHoldS), v("speed_floor", x.speedFloor);
}
template <class V> void Fields(V& v, hud2layout::RevealOut& x)
{
    v("held", x.held), v("escorts", x.escorts), v("nav_full", x.navFull), v("arc_shield", x.arcShield);
    v("arc_hull", x.arcHull), v("arc_power", x.arcPower), v("speed", x.speed), v("peek", x.peek), v("fight", x.fight);
    v("form", x.form);
}
template <class V> void Fields(V& v, hud2layout::Reveal& x)
{
    v("peek_left", x.peekLeft), v("pinned", x.pinned), v("wake", x.wake), v("sig", x.sig), v("primed", x.primed);
    v("fight_left", x.fightLeft), v("form_left", x.formLeft), v("form", x.form), v("speed_left", x.speedLeft);
    v("speed_held", x.speedHeld), v("speed_ref", x.speedRef), v("thr_was", x.thrWas);
}
template <class V> void Fields(V& v, hud2::Fades& x)
{
    v("nav", x.nav), v("ring", x.ring), v("target", x.target), v("ship", x.ship), v("loadout", x.loadout);
    v("context", x.context), v("alert", x.alert), v("ring_on", x.ringOn), v("enemy_cue", x.enemyCue);
    v("wake_target", x.wakeTarget), v("wake_hints", x.wakeHints), v("slide", x.slide), v("arc_shield", x.arcShield);
    v("arc_hull", x.arcHull), v("arc_power", x.arcPower), v("band", x.band), v("speed", x.speed), v("escorts", x.escorts);
}
template <class V> void Fields(V& v, hud2::ViewBack& x)
{
    v("on", x.on), v("x0", x.x0), v("y0", x.y0), v("x1", x.x1), v("y1", x.y1), v("ch", x.ch), v("corners", x.corners);
    v("alpha", x.alpha);
}
template <class V> void Fields(V& v, promptshow::State& x)
{
    v("seen", x.seen), v("sig", x.sig), v("left", x.left);
}
template <class V> void Fields(V& v, hudwake::State& x)
{
    v("left", x.left);
}
template <class V> void Fields(V& v, hud2notice::LoadWatch& x)
{
    v("seen", x.seen), v("primed", x.primed);
}

// the parts' state
template <class V> void Fields(V& v, hud2target::Out& x)
{
    v("show", x.show), v("settle", x.settle), v("class_a", x.classA), v("plate_a", x.plateA), v("words_a", x.wordsA);
    v("seat", x.seat), v("side", x.side), v("mark", x.mark), v("caret_x", x.caretX), v("caret_y", x.caretY);
    v("block_x", x.blockX), v("block_y", x.blockY), v("card_x0", x.cardX0), v("card_y0", x.cardY0);
    v("hit_shield_a", x.hitShieldA), v("hit_hull_a", x.hitHullA), v("lost_shield", x.lostShield), v("lost_hull", x.lostHull);
}
template <class V> void Fields(V& v, hud2target::State& x) // (private members: the class lists them itself)
{
    x.Fields(v);
}
// One marquee clock with its key (the table is an unordered map: written as its entries sorted by key).
struct MarqueeEntry
{
    unsigned long long key = 0;
    hud2kit::MarqueeClock clock;
};
template <class V> void Fields(V& v, hud2kit::MarqueeClock& x)
{
    v("t0", x.t0), v("seen", x.seen);
}
template <class V> void Fields(V& v, MarqueeEntry& x)
{
    v("key", x.key), v("clock", x.clock);
}
template <class V> void Fields(V& v, hud2kit::Marquees& x)
{
    std::vector<MarqueeEntry> list;
    for (const auto& e : x.clocks)
        list.push_back({e.first, e.second});
    std::sort(list.begin(), list.end(), [](const MarqueeEntry& a, const MarqueeEntry& b) { return a.key < b.key; });
    v.note(x.clocks);
    v("clocks", list);
    if constexpr (V::kSets)
    {
        x.clocks.clear();
        for (const MarqueeEntry& e : list)
            if (!x.clocks.emplace(e.key, e.clock).second)
                v.fail("clocks", "a key twice");
    }
}
template <class V> void Fields(V& v, hud2::PartsState& x)
{
    v("br_shield", x.brShield), v("br_hull", x.brHull), v("br_boost", x.brBoost), v("br_t_shield", x.brTShield);
    v("br_t_hull", x.brTHull), v("br_escort", x.brEscort), v("bl_shield", x.blShield), v("bl_hull", x.blHull);
    v("bl_boost", x.blBoost), v("bl_cruise", x.blCruise), v("bl_t_shield", x.blTShield), v("bl_t_hull", x.blTHull);
    v("beat_own", x.beatOwn), v("beat", x.beat), v("up_was", x.upWas), v("hull_was", x.hullWas), v("boost_was", x.boostWas);
    v("t_shield_was", x.tShieldWas), v("t_hull_was", x.tHullWas), v("target_was", x.targetWas);
    v("cruise_was", x.cruiseWas), v("cruise_fill", x.cruiseFill), v("down_phase", x.downPhase);
    v("word_phase", x.wordPhase), v("lo_gun_end", x.loGunEnd), v("lo_pad_v", x.loPadV), v("lo_row_p", x.loRowP);
    v("form", x.form), v("form_out", x.formOut), v("form_seat", x.formSeat), v("form_side", x.formSide);
    v("form_id", x.formId), v("marquees", x.marquees);
}
template <class V> void Fields(V& v, hud2::FrameState& x)
{
    v("ctx", x.ctx), v("m", x.m), v("l", x.L), v("core", x.core), v("reveal", x.reveal), v("rv", x.rv);
    v("entry_seen", x.entrySeen), v("focus_readout", x.focusReadout), v("reveal_rules_of", x.revealRulesOf);
    v("reveal_rules", x.revealRules), v("nav_bottom", x.navBottom), v("nav_right", x.navRight), v("f", x.f);
    v("view_back", x.viewBack), v("view_zoom", x.viewZoom), v("sim", x.sim), v("sim_comm", x.simComm), v("active", x.active);
    v("menu_over", x.menuOver), v("visible", x.visible), v("was_in_space", x.wasInSpace), v("t", x.t);
    v("switches", x.switches), v("form_dt", x.formDt), v("notice_dt", x.noticeDt), v("notice_x", x.noticeX);
    v("notice_y", x.noticeY), v("measured_px", x.measuredPx), v("rad_art", x.radArt), v("nav_scrim", x.navScrim);
    v("letterbox", x.letterbox), v("prompt_show", x.promptShow), v("ctx_sig", x.ctxSig), v("prompt_action", x.promptAction);
    v("prompt_target", x.promptTarget), v("wake", x.wake), v("wake_trig", x.wakeTrig), v("wake_shown", x.wakeShown);
    v("loadout_sig", x.loadoutSig), v("objective_sig", x.objectiveSig), v("maneuver_sig", x.maneuverSig);
    v("cruise_was", x.cruiseWas), v("kill_was", x.killWas), v("turret_was", x.turretWas), v("mouse_x_was", x.mouseXWas);
    v("mouse_y_was", x.mouseYWas), v("cursor_eligible", x.cursorEligible), v("alerts_off", x.alertsOff);
    v("box_up", x.boxUp), v("box_answers", x.boxAnswers), v("box", x.box), v("peek_left", x.peekLeft), v("loads", x.loads);
    v.group("nav_ease", x.navEase, [](auto& w, auto& e)
            { w("right", e.right), w("bottom", e.bottom), w("scrim_right", e.scrimRight), w("scrim_bottom", e.scrimBottom),
              w("h_was", e.hWas), w("at", e.at); });
    v.group("panel_ease", x.panelEase, [](auto& w, auto& e)
            { w("card_w", e.cardW), w("ship_w", e.shipW), w("load_w", e.loadW), w("k_was", e.kWas), w("h_was", e.hWas),
              w("at", e.at), w("ship_h", e.shipH), w("load_h", e.loadH), w("card_h", e.cardH); });
    v("subs_was", x.subsWas), v("subs_seen_at", x.subsSeenAt), v("parts", x.parts);
}
// The frame's answers (not carried: a recording compares them)
template <class V> void Fields(V& v, hud2::ViewRect& x)
{
    v("cx", x.cx), v("cy", x.cy), v("hw", x.hw), v("hh", x.hh), v("show", x.show);
}
template <class V> void Fields(V& v, hud2::StartOut& x)
{
    v("game_load", x.gameLoad), v("left_space", x.leftSpace), v("forget", x.forget), v("feed", x.feed);
    v("entered_space", x.enteredSpace), v("hot_on", x.hotOn);
}
template <class V> void Fields(V& v, hud2::StepOut& x)
{
    v("pulse", x.pulse), v("view", x.view);
}
// a recorded frame's (B4b)
template <class V> void Fields(V& v, Tape& x)
{
    v("tick_ms", x.tickMs), v("tick_ms64", x.tickMs64), v("seconds", x.seconds), v("typing", x.typing), v("hovered", x.hovered);
}
template <class V> void Fields(V& v, HotRect& x)
{
    v("slot", x.slot), v("x0", x.x0), v("y0", x.y0), v("x1", x.x1), v("y1", x.y1);
}
template <class V> void Fields(V& v, UpdateRec& x)
{
    v("phase", x.phase), v("start", x.start), v("step", x.step), v("tape", x.tape);
}
template <class V> void Fields(V& v, RenderRec& x)
{
    v("drawn", x.drawn), v("sw", x.sw), v("sh", x.sh), v("ask_px", x.askPx), v("draw_px", x.drawPx);
    v("icons_ready", x.iconsReady), v("kit_extra", x.kitExtra), v("st", x.st), v("tape", x.tape), v("hot_on", x.hotOn);
    v("hot", x.hot);
}
template <class V> void Fields(V& v, UnderRec& x)
{
    v("drawn", x.drawn), v("sw", x.sw), v("sh", x.sh), v("px", x.px), v("back", x.back), v("view_back", x.viewBack);
}
template <class V> void Fields(V& v, LayerPrint& x)
{
    v("count", x.count), v("hash", x.hash);
}
template <class V> void Fields(V& v, SetPrint& x)
{
    v("name", x.name), v("print", x.print);
}
template <class V> void Fields(V& v, ListRef& x)
{
    v("file", x.file), v("hash", x.hash), v("inputs", x.inputs), v("sets", x.sets), v("under", x.under), v("over", x.over);
}
// The file: the schema and the privacy first.
template <class V> void Fields(V& v, StateDoc& x)
{
    std::string schema = "flstate/0", privacy = x.priv ? PrivateMarker() : "public";
    v.note(x.priv);
    if constexpr (V::kFills)
        x.priv = !x.priv; // (the filler: the other privacy)
    else
    {
        v("schema", schema), v("privacy", privacy);
        if constexpr (V::kSets)
        {
            if (schema != "flstate/0")
                v.fail("schema", "not flstate/0");
            if (privacy != "public" && privacy != PrivateMarker())
                v.fail("privacy", "neither public nor the private marker");
            x.priv = privacy != "public";
        }
    }
    v("build", x.build), v("state", x.state), v("frame", x.frame), v("frame_in", x.in), v("draw_in", x.draw);
    v("frame_state", x.st), v("update", x.update), v("render", x.render), v("under", x.under), v("list", x.list);
}

// ---- UTF-16 <-> UTF-8 (the wide strings; a lone surrogate is written as U+FFFD, as Windows' converter does)
inline std::string Utf8(const std::wstring& w)
{
    std::string o;
    for (size_t i = 0; i < w.size(); ++i)
    {
        unsigned c = static_cast<unsigned>(w[i]) & 0xFFFFu;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < w.size() && (static_cast<unsigned>(w[i + 1]) & 0xFC00u) == 0xDC00)
            c = 0x10000 + ((c - 0xD800) << 10) + ((static_cast<unsigned>(w[++i]) & 0xFFFFu) - 0xDC00);
        else if (c >= 0xD800 && c < 0xE000)
            c = 0xFFFD;
        if (c < 0x80)
            o += static_cast<char>(c);
        else if (c < 0x800)
            o += static_cast<char>(0xC0 | (c >> 6)), o += static_cast<char>(0x80 | (c & 0x3F));
        else if (c < 0x10000)
            o += static_cast<char>(0xE0 | (c >> 12)), o += static_cast<char>(0x80 | ((c >> 6) & 0x3F)),
                o += static_cast<char>(0x80 | (c & 0x3F));
        else
            o += static_cast<char>(0xF0 | (c >> 18)), o += static_cast<char>(0x80 | ((c >> 12) & 0x3F)),
                o += static_cast<char>(0x80 | ((c >> 6) & 0x3F)), o += static_cast<char>(0x80 | (c & 0x3F));
    }
    return o;
}
// False on bytes that are not UTF-8 (an overlong form, a surrogate, past U+10FFFF, a cut sequence).
inline bool Wide(const std::string& s, std::wstring& out)
{
    out.clear();
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        const int n = b < 0x80 ? 0 : (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : -1;
        if (n < 0 || i + n >= s.size() + (n ? 0 : 1))
            return false;
        unsigned c = n == 0 ? b : n == 1 ? b & 0x1Fu : n == 2 ? b & 0x0Fu : b & 0x07u;
        for (int j = 1; j <= n; ++j)
        {
            const unsigned char t = static_cast<unsigned char>(s[i + j]);
            if ((t & 0xC0) != 0x80)
                return false;
            c = (c << 6) | (t & 0x3Fu);
        }
        if ((n == 1 && c < 0x80) || (n == 2 && c < 0x800) || (n == 3 && c < 0x10000) || c > 0x10FFFF || (c >= 0xD800 && c < 0xE000))
            return false;
        if (c >= 0x10000)
            out += static_cast<wchar_t>(0xD800 + ((c - 0x10000) >> 10)), out += static_cast<wchar_t>(0xDC00 + ((c - 0x10000) & 0x3FF));
        else
            out += static_cast<wchar_t>(c);
        i += n + 1;
    }
    return true;
}

template <class T> constexpr bool kIsText = std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>;

// "frame_state.m.marks[]": the path of member `k` (null: an array's item) inside the object or array at `up`
inline std::string PathJoin(const std::string& up, const char* k)
{
    const std::string name = k ? k : "[]";
    return up.empty() || name.empty() ? up + name : up + (k ? "." : "") + name;
}
// A retired member the reader read past (Reader::Retired): the object it stood in, the member it stood before ("" the
// object's last), its name and its value as the writer writes it. The design bench writes them back in their places
// (Writer::retired) to check an old record's fingerprint of its own inputs, which was taken over the text with them.
struct RetiredMember
{
    std::string path, before, name, text;
};

// ---- the writer
struct Writer
{
    static constexpr bool kSets = false, kFills = false;
    std::string out;
    int depth = 0;
    bool first = true; // the open object or array has no member yet
    // The retired members to write back in their places (null in the game: none); `at` the open objects' paths for it.
    const std::vector<RetiredMember>* retired = nullptr;
    std::vector<std::string> at;

    static std::string Quote(const std::string& s)
    {
        std::string o = "\"";
        for (unsigned char c : s)
        {
            if (c == '"' || c == '\\')
                o += '\\', o += static_cast<char>(c);
            else if (c < 0x20)
            {
                char b[8];
                snprintf(b, sizeof(b), "\\u%04x", c);
                o += b;
            }
            else
                o += static_cast<char>(c);
        }
        return o + "\"";
    }
    static std::string Num(float f)
    {
        if (std::isnan(f))
            return "\"nan\"";
        if (std::isinf(f))
            return f > 0 ? "\"inf\"" : "\"-inf\"";
        char b[32];
        snprintf(b, sizeof(b), "%.9g", static_cast<double>(f));
        return b;
    }
    static std::string Num(double d)
    {
        if (std::isnan(d))
            return "\"nan\"";
        if (std::isinf(d))
            return d > 0 ? "\"inf\"" : "\"-inf\"";
        char b[40];
        snprintf(b, sizeof(b), "%.17g", d);
        return b;
    }
    void Name(const char* k)
    {
        out += first ? "" : ",";
        first = false;
        if (k)
            out += "\n" + std::string(static_cast<size_t>(depth), ' ') + Quote(k) + ": ";
    }
    // the retired members that stood before `k` ("": at the end) in the open object
    void Retired(const char* k)
    {
        if (retired && !at.empty())
            for (const RetiredMember& r : *retired)
                if (r.before == k && r.path == at.back())
                    Name(r.name.c_str()), out += r.text;
    }
    void Key(const char* k) // a member's name (k null: an array's element)
    {
        if (k)
            Retired(k);
        Name(k);
    }
    void Open(const char* k, char c)
    {
        Key(k), out += c, ++depth, first = true;
        if (retired) // (the outermost object's path is "", as the reader names it)
            at.push_back(at.empty() ? std::string() : PathJoin(at.back(), k));
    }
    void Close(char c, bool lines)
    {
        if (retired && !at.empty())
            Retired(""), at.pop_back();
        --depth;
        if (lines && !first)
            out += "\n" + std::string(static_cast<size_t>(depth), ' ');
        out += c, first = false;
    }

    void operator()(const char* k, bool& x) { Key(k), out += x ? "true" : "false"; }
    void operator()(const char* k, int& x) { Key(k), out += std::to_string(x); }
    void operator()(const char* k, unsigned& x) { Key(k), out += std::to_string(x); }
    void operator()(const char* k, long long& x) { Key(k), out += std::to_string(x); }
    void operator()(const char* k, unsigned long long& x) { Key(k), out += std::to_string(x); }
    void operator()(const char* k, wchar_t& x) { Key(k), out += std::to_string(static_cast<unsigned>(x)); }
    void operator()(const char* k, float& x) { Key(k), out += Num(x); }
    void operator()(const char* k, double& x) { Key(k), out += Num(x); }
    void operator()(const char* k, std::string& x) { Key(k), out += Quote(x); }
    void operator()(const char* k, std::wstring& x) { Key(k), out += Quote(Utf8(x)); }
    template <class T> std::enable_if_t<std::is_enum_v<T>> operator()(const char* k, T& x)
    {
        Key(k), out += std::to_string(static_cast<long long>(x));
    }
    template <class T> std::enable_if_t<std::is_class_v<T> && !kIsText<T>> operator()(const char* k, T& x)
    {
        Open(k, '{');
        Fields(*this, x);
        Close('}', true);
    }
    template <class T, size_t N> void operator()(const char* k, T (&a)[N])
    {
        Open(k, '[');
        for (T& e : a)
            (*this)(nullptr, e);
        Close(']', std::is_class_v<T> && !kIsText<T>);
    }
    template <class T> void operator()(const char* k, std::vector<T>& a)
    {
        Open(k, '[');
        for (T& e : a)
            (*this)(nullptr, e);
        Close(']', std::is_class_v<T> && !kIsText<T>);
    }
    template <class T, class F> void group(const char* k, T& x, F f)
    {
        Open(k, '{');
        f(*this, x);
        Close('}', true);
    }
    template <class T> void note(T&) {}
    void fail(const char*, const char*) {}
};

// ---- the reader: the text parsed into a tree, then read member by member in the lists' order
struct Json
{
    enum Kind
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    } kind = Null;
    std::string text;                                  // a number's text as written, a string's bytes (UTF-8), "true" / "false"
    std::vector<std::pair<std::string, Json>> members; // an object's, in the text's order
    std::vector<Json> items;                           // an array's
};

struct JsonParser
{
    const std::string& s;
    size_t at = 0;
    std::string err;
    int depth = 0;

    bool Fail(const char* why)
    {
        if (err.empty())
            err = std::string(why) + " at byte " + std::to_string(at);
        return false;
    }
    void Space()
    {
        while (at < s.size() && (s[at] == ' ' || s[at] == '\t' || s[at] == '\n' || s[at] == '\r'))
            ++at;
    }
    bool Hex4(unsigned& c)
    {
        c = 0;
        for (int i = 0; i < 4; ++i, ++at)
        {
            if (at >= s.size())
                return false;
            const char h = s[at];
            const int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
            if (d < 0)
                return false;
            c = c * 16 + static_cast<unsigned>(d);
        }
        return true;
    }
    bool Str(std::string& o)
    {
        ++at; // the opening quote
        o.clear();
        while (at < s.size() && s[at] != '"')
        {
            const unsigned char c = static_cast<unsigned char>(s[at]);
            if (c < 0x20)
                return Fail("a control character in a string");
            if (c != '\\')
            {
                o += static_cast<char>(c), ++at;
                continue;
            }
            if (++at >= s.size())
                break;
            const char e = s[at++];
            if (e == '"' || e == '\\' || e == '/')
                o += e;
            else if (e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't')
                o += e == 'b' ? '\b' : e == 'f' ? '\f' : e == 'n' ? '\n' : e == 'r' ? '\r' : '\t';
            else if (e == 'u')
            {
                unsigned c1 = 0, c2 = 0;
                if (!Hex4(c1))
                    return Fail("a bad \\u escape");
                if (c1 >= 0xD800 && c1 < 0xDC00)
                {
                    if (at + 1 >= s.size() || s[at] != '\\' || s[at + 1] != 'u' || (at += 2, !Hex4(c2)) || c2 < 0xDC00 || c2 >= 0xE000)
                        return Fail("a lone surrogate");
                    c1 = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00);
                }
                else if (c1 >= 0xDC00 && c1 < 0xE000)
                    return Fail("a lone surrogate");
                std::wstring w;
                if (c1 >= 0x10000)
                    w += static_cast<wchar_t>(0xD800 + ((c1 - 0x10000) >> 10)), w += static_cast<wchar_t>(0xDC00 + ((c1 - 0x10000) & 0x3FF));
                else
                    w += static_cast<wchar_t>(c1);
                o += Utf8(w);
            }
            else
                return Fail("a bad escape");
        }
        if (at >= s.size())
            return Fail("a string not closed");
        ++at;
        return true;
    }
    bool Value(Json& j)
    {
        if (++depth > 64)
            return Fail("nested too deep");
        Space();
        if (at >= s.size())
            return Fail("a value missing");
        const char c = s[at];
        bool ok = true;
        if (c == '{')
        {
            j.kind = Json::Object, ++at, Space();
            if (at < s.size() && s[at] == '}')
                ++at;
            else
                for (;;)
                {
                    Space();
                    if (at >= s.size() || s[at] != '"')
                        return Fail("a member's name missing");
                    std::string k;
                    if (!Str(k))
                        return false;
                    Space();
                    if (at >= s.size() || s[at] != ':')
                        return Fail("':' missing");
                    ++at;
                    j.members.emplace_back(k, Json());
                    if (!Value(j.members.back().second))
                        return false;
                    Space();
                    if (at < s.size() && s[at] == ',')
                    {
                        ++at;
                        continue;
                    }
                    if (at < s.size() && s[at] == '}')
                    {
                        ++at;
                        break;
                    }
                    return Fail("',' or '}' missing");
                }
        }
        else if (c == '[')
        {
            j.kind = Json::Array, ++at, Space();
            if (at < s.size() && s[at] == ']')
                ++at;
            else
                for (;;)
                {
                    j.items.emplace_back();
                    if (!Value(j.items.back()))
                        return false;
                    Space();
                    if (at < s.size() && s[at] == ',')
                    {
                        ++at;
                        continue;
                    }
                    if (at < s.size() && s[at] == ']')
                    {
                        ++at;
                        break;
                    }
                    return Fail("',' or ']' missing");
                }
        }
        else if (c == '"')
            j.kind = Json::String, ok = Str(j.text);
        else if (s.compare(at, 4, "true") == 0 || s.compare(at, 5, "false") == 0)
            j.kind = Json::Bool, j.text = s[at] == 't' ? "true" : "false", at += j.text.size();
        else if (s.compare(at, 4, "null") == 0)
            j.kind = Json::Null, at += 4;
        else if (c == '-' || (c >= '0' && c <= '9'))
        {
            const size_t b = at;
            while (at < s.size() && (strchr("+-.eE", s[at]) || (s[at] >= '0' && s[at] <= '9')) && s[at])
                ++at;
            j.kind = Json::Number, j.text = s.substr(b, at - b);
        }
        else
            return Fail("not a value");
        --depth;
        return ok;
    }
};

struct Reader
{
    static constexpr bool kSets = true, kFills = false;
    struct Open
    {
        const Json* j;
        size_t next; // the next member / item to read
        std::string path;
    };
    std::vector<Open> stack;
    bool ok = true;
    std::string err;

    // "frame_state.m.marks[]": the path of member `k` (null: an array's item) of the open object
    std::string PathOf(const char* k) const { return PathJoin(stack.empty() ? std::string() : stack.back().path, k); }
    std::vector<RetiredMember>* retired = nullptr; // where the retired members read past are listed (null: nowhere)
    void fail(const char* k, const char* why)
    {
        if (ok)
            ok = false, err = PathOf(k) + ": " + why;
    }
    // Members a struct no longer has, read past where they stand so a state recorded before they went still reads (the
    // design bench's pinned records): each named with the object it stood in. Any other member the list does not know is
    // still refused. hudrows::Values ("rows") lost base_pages, ui_style and skin_native in 740aa4d9 (audit 113_ B6).
    static bool Retired(const std::string& in, const std::string& name)
    {
        static const char* const kRows[] = {"base_pages", "ui_style", "skin_native"};
        const size_t dot = in.rfind('.');
        if (in.compare(dot == std::string::npos ? 0 : dot + 1, std::string::npos, "rows") != 0)
            return false;
        for (const char* r : kRows)
            if (name == r)
                return true;
        return false;
    }
    // Steps the open object past the retired members at its next place (`k` the member expected there, null: its end),
    // listing each with its value as the writer writes it (a scalar: they all were).
    void SkipRetired(Open& o, const char* k)
    {
        while (o.next < o.j->members.size() && Retired(o.path, o.j->members[o.next].first))
        {
            const auto& m = o.j->members[o.next++];
            const Json& v = m.second;
            if (v.kind == Json::Object || v.kind == Json::Array)
                return fail(m.first.c_str(), "a retired member that is not a value");
            if (retired)
                retired->push_back({o.path, k ? k : "", m.first, v.kind == Json::String ? Writer::Quote(v.text) : v.kind == Json::Null ? "null" : v.text});
        }
    }
    // The value of member `k` (k null: the open array's next item), or null after a failure.
    const Json* Take(const char* k)
    {
        if (!ok || stack.empty())
            return nullptr;
        Open& o = stack.back();
        if (o.j->kind == Json::Array)
        {
            if (o.next >= o.j->items.size())
                return fail(k, "the array is shorter"), nullptr;
            return &o.j->items[o.next++];
        }
        SkipRetired(o, k);
        if (!ok)
            return nullptr;
        if (o.next >= o.j->members.size())
            return fail(k, "missing"), nullptr;
        const auto& m = o.j->members[o.next];
        if (m.first != k)
            return fail(k, ("expected here, found \"" + m.first.substr(0, 40) + "\"").c_str()), nullptr;
        ++o.next;
        return &m.second;
    }
    // An object or array begun / ended: every member or item read, none more.
    bool Enter(const char* k, Json::Kind kind)
    {
        const Json* j = Take(k);
        if (!j)
            return false;
        if (j->kind != kind)
            return fail(k, kind == Json::Object ? "not an object" : "not an array"), false;
        stack.push_back({j, 0, PathOf(k)});
        return true;
    }
    void Leave()
    {
        if (stack.back().j->kind == Json::Object)
            SkipRetired(stack.back(), nullptr);
        const Open o = stack.back();
        stack.pop_back();
        const size_t n = o.j->kind == Json::Object ? o.j->members.size() : o.j->items.size();
        if (ok && o.next != n)
        {
            ok = false;
            err = o.path + (o.j->kind == Json::Object ? ": an unknown member \"" + o.j->members[o.next].first.substr(0, 40) + "\""
                                                      : ": the array is longer");
        }
    }
    template <class T> bool Int(const char* k, T& x)
    {
        const Json* j = Take(k);
        if (!j)
            return false;
        if (j->kind != Json::Number || j->text.find_first_of(".eE") != std::string::npos)
            return fail(k, "not an integer"), false;
        errno = 0;
        char* e = nullptr;
        if constexpr (std::is_signed_v<T>)
        {
            const long long v = strtoll(j->text.c_str(), &e, 10);
            if (errno || !e || *e || v < static_cast<long long>(std::numeric_limits<T>::min()) ||
                v > static_cast<long long>(std::numeric_limits<T>::max()))
                return fail(k, "an integer out of range"), false;
            x = static_cast<T>(v);
        }
        else
        {
            const unsigned long long v = strtoull(j->text.c_str(), &e, 10);
            if (errno || !e || *e || j->text[0] == '-' || v > static_cast<unsigned long long>(std::numeric_limits<T>::max()))
                return fail(k, "an integer out of range"), false;
            x = static_cast<T>(v);
        }
        return true;
    }
    template <class T> void Real(const char* k, T& x)
    {
        const Json* j = Take(k);
        if (!j)
            return;
        if (j->kind == Json::String && (j->text == "nan" || j->text == "inf" || j->text == "-inf"))
        {
            x = j->text == "nan" ? std::numeric_limits<T>::quiet_NaN()
                                 : j->text == "inf" ? std::numeric_limits<T>::infinity() : -std::numeric_limits<T>::infinity();
            return;
        }
        if (j->kind != Json::Number)
            return fail(k, "not a number");
        char* e = nullptr;
        if constexpr (std::is_same_v<T, float>)
            x = strtof(j->text.c_str(), &e);
        else
            x = strtod(j->text.c_str(), &e);
        if (!e || *e)
            fail(k, "not a number");
        else if (std::isinf(x)) // (a finite text past the type's range: the writer never makes one)
            fail(k, "a number out of range");
    }

    void operator()(const char* k, bool& x)
    {
        const Json* j = Take(k);
        if (j && j->kind != Json::Bool)
            return fail(k, "not true or false");
        if (j)
            x = j->text == "true";
    }
    void operator()(const char* k, int& x) { Int(k, x); }
    void operator()(const char* k, unsigned& x) { Int(k, x); }
    void operator()(const char* k, long long& x) { Int(k, x); }
    void operator()(const char* k, unsigned long long& x) { Int(k, x); }
    void operator()(const char* k, wchar_t& x)
    {
        unsigned short c = 0;
        if (Int(k, c))
            x = static_cast<wchar_t>(c);
    }
    void operator()(const char* k, float& x) { Real(k, x); }
    void operator()(const char* k, double& x) { Real(k, x); }
    void operator()(const char* k, std::string& x)
    {
        const Json* j = Take(k);
        if (j && j->kind != Json::String)
            return fail(k, "not a string");
        if (j)
            x = j->text;
    }
    void operator()(const char* k, std::wstring& x)
    {
        const Json* j = Take(k);
        if (j && j->kind != Json::String)
            return fail(k, "not a string");
        if (j && !Wide(j->text, x))
            fail(k, "not UTF-8");
    }
    template <class T> std::enable_if_t<std::is_enum_v<T>> operator()(const char* k, T& x)
    {
        std::underlying_type_t<T> u = 0;
        if (Int(k, u))
            x = static_cast<T>(u);
    }
    template <class T> std::enable_if_t<std::is_class_v<T> && !kIsText<T>> operator()(const char* k, T& x)
    {
        if (!Enter(k, Json::Object))
            return;
        Fields(*this, x);
        Leave();
    }
    template <class T, size_t N> void operator()(const char* k, T (&a)[N])
    {
        if (!Enter(k, Json::Array))
            return;
        if (stack.back().j->items.size() != N)
            fail(nullptr, ("an array of " + std::to_string(stack.back().j->items.size()) + " for " + std::to_string(N)).c_str());
        for (size_t i = 0; i < N && ok; ++i)
            (*this)(nullptr, a[i]);
        Leave();
    }
    template <class T> void operator()(const char* k, std::vector<T>& a)
    {
        if (!Enter(k, Json::Array))
            return;
        a.assign(stack.back().j->items.size(), T());
        for (size_t i = 0; i < a.size() && ok; ++i)
            (*this)(nullptr, a[i]);
        Leave();
    }
    template <class T, class F> void group(const char* k, T& x, F f)
    {
        if (!Enter(k, Json::Object))
            return;
        f(*this, x);
        Leave();
    }
    template <class T> void note(T&) {}
};

// ---- the file
inline std::string Write(const StateDoc& doc)
{
    Writer w;
    w.Open(nullptr, '{');
    Fields(w, const_cast<StateDoc&>(doc)); // (the writer only reads: one list serves both directions)
    w.Close('}', true);
    return w.out + "\n";
}
// Reads Write's text into `out` (left as it was on a failure, with the reason in `err`); the retired members read past
// are listed in `retired` when one is given.
inline bool Read(const std::string& text, StateDoc& out, std::string* err = nullptr, std::vector<RetiredMember>* retired = nullptr)
{
    const auto fail = [&](const std::string& why)
    {
        if (err)
            *err = why;
        return false;
    };
    Json root;
    JsonParser p{text};
    if (!p.Value(root))
        return fail(p.err);
    p.Space();
    if (p.at != text.size())
        return fail("text after the object at byte " + std::to_string(p.at));
    if (root.kind != Json::Object)
        return fail("not an object");
    Json top; // the root as the one member of an outer object, so the reader enters it as any other
    top.kind = Json::Object;
    top.members.emplace_back("", std::move(root));
    Reader r;
    std::vector<RetiredMember> list;
    r.retired = retired ? &list : nullptr;
    r.stack.push_back({&top, 0, ""});
    auto doc = std::make_unique<StateDoc>();
    if (r.Enter("", Json::Object))
    {
        Fields(r, *doc);
        r.Leave();
    }
    if (!r.ok)
        return fail(r.err);
    out = std::move(*doc);
    if (retired)
        *retired = std::move(list);
    return true;
}
} // namespace hud2state

// ---- the pins
namespace hud2state
{
// A struct's member count, read by the compiler: the most values `T{...}` takes (each fixed array counts its elements,
// as the lists do: a member struct counts one). 0 for a class that is not an aggregate (hud2target::State, whose members
// are private): its sizeof pin and the unit test's layout check (no unlisted bytes beyond padding) hold it.
struct AnyField
{
    template <class U> operator U() const; // (declared only: used in unevaluated braces)
};
template <class T, class Seq, class = void> struct BracesTake : std::false_type
{
};
template <class T, size_t... I> struct BracesTake<T, std::index_sequence<I...>, std::void_t<decltype(T{(void(I), AnyField{})...})>> : std::true_type
{
};
template <class T, size_t N = 0> constexpr size_t MemberCount()
{
    if constexpr (!std::is_aggregate_v<T>)
        return 0;
    else if constexpr (BracesTake<T, std::make_index_sequence<N + 1>>::value)
        return MemberCount<T, N + 1>();
    else
        return N;
}
} // namespace hud2state

// Every struct with a Fields list (the unit test prints their pins with FLVIBED_STATE_PINS=1).
#define FLV_STATE_TYPES(X)                                                                                                     \
    X(HudRect) X(HudSubtitles) X(presets::Binding) X(presets::Table) X(views::HudContext) X(uiopt::UiOptions)                  \
    X(hudrows::ShieldCue) X(hudrows::ReticleCue) X(hudrows::Values) X(hud2::SizeIn) X(hud2::PartsIn) X(hud2::SubsIn)           \
    X(hud2::FrameIn) X(hud2::DrawIn) X(hud2preset::Setup) X(hud2preset::Needs) X(hud2preset::Adapt) X(hud2::Prompt)            \
    X(hud2::AlertCard) X(hud2bars::BloomOut) X(hud2bars::Breath) X(hud2bars::Bloom) X(hud2::BarFx) X(hud2::Model::Mark)        \
    X(hud2::Model::CueMark) X(hud2::Model::Threat) X(hud2::Model::Escort) X(decltype(hud2::Model::tMark)) X(hud2notice::Item)  \
    X(hud2notice::Notice) X(hud2::Model) X(promptfit::HoldPlan) X(hud2layout::Rect) X(hud2layout::Reserved) X(hud2::Layout)    \
    X(hud2layout::Out) X(hud2layout::RevealRules) X(hud2layout::RevealOut) X(hud2layout::Reveal) X(hud2::Fades)                \
    X(hud2::ViewBack) X(promptshow::State) X(hudwake::State) X(hud2notice::LoadWatch) X(hud2target::Out) X(hud2target::State) \
    X(hud2kit::MarqueeClock) X(hud2state::MarqueeEntry) X(hud2kit::Marquees) X(hud2::PartsState)                               \
    X(decltype(hud2::FrameState::navEase)) X(decltype(hud2::FrameState::panelEase)) X(hud2::FrameState) X(hud2::ViewRect)      \
    X(hud2::StartOut) X(hud2::StepOut) X(hud2state::Tape) X(hud2state::HotRect) X(hud2state::UpdateRec)                    \
    X(hud2state::RenderRec) X(hud2state::UnderRec) X(hud2state::LayerPrint) X(hud2state::SetPrint) X(hud2state::ListRef)      \
    X(hud2state::StateDoc)

// x86, MSVC /O2 /MT (every build of this project). A pin that fails: a member was added, removed or changed in that
// struct - list it in its Fields function above (or take it out), then set the pin (FLVIBED_STATE_PINS=1 fltest hud2_state
// prints them all).
#define FLV_STATE_PIN(T, size, members)                                                                                        \
    static_assert(sizeof(T) == (size) && hud2state::MemberCount<T>() == (members),                                           \
                  "hud2_state_io.h: " #T " changed: list its members in its Fields, then set its pin")
FLV_STATE_PIN(HudRect, 16, 4);
FLV_STATE_PIN(HudSubtitles, 36, 3);
FLV_STATE_PIN(presets::Binding, 3, 3);
FLV_STATE_PIN(presets::Table, 54, 18);
FLV_STATE_PIN(views::HudContext, 80, 13);
FLV_STATE_PIN(uiopt::UiOptions, 160, 43);
FLV_STATE_PIN(hudrows::ShieldCue, 16, 4);
FLV_STATE_PIN(hudrows::ReticleCue, 1, 1);
FLV_STATE_PIN(hudrows::Values, 80, 20);
FLV_STATE_PIN(hud2::SizeIn, 12, 3);
FLV_STATE_PIN(hud2::PartsIn, 256, 4);
FLV_STATE_PIN(hud2::SubsIn, 44, 3);
FLV_STATE_PIN(hud2::FrameIn, 496, 24);
FLV_STATE_PIN(hud2::DrawIn, 292, 13);
FLV_STATE_PIN(hud2preset::Setup, 12, 4);
FLV_STATE_PIN(hud2preset::Needs, 9, 9);
FLV_STATE_PIN(hud2preset::Adapt, 40, 5);
FLV_STATE_PIN(hud2::Prompt, 60, 5);
FLV_STATE_PIN(hud2::AlertCard, 188, 9);
FLV_STATE_PIN(hud2bars::BloomOut, 16, 4);
FLV_STATE_PIN(hud2bars::Breath, 24, 6);
FLV_STATE_PIN(hud2bars::Bloom, 12, 3);
FLV_STATE_PIN(hud2::BarFx, 144, 18);
FLV_STATE_PIN(hud2::Model::Mark, 64, 6);
FLV_STATE_PIN(hud2::Model::CueMark, 16, 7);
FLV_STATE_PIN(hud2::Model::Threat, 60, 5);
FLV_STATE_PIN(hud2::Model::Escort, 28, 2);
FLV_STATE_PIN(decltype(hud2::Model::tMark), 16, 4);
FLV_STATE_PIN(hud2notice::Item, 36, 4);
FLV_STATE_PIN(hud2notice::Notice, 312, 43);
FLV_STATE_PIN(hud2::Model, 4776, 193);
FLV_STATE_PIN(promptfit::HoldPlan, 12, 9);
FLV_STATE_PIN(hud2layout::Rect, 16, 4);
FLV_STATE_PIN(hud2layout::Reserved, 260, 17);
FLV_STATE_PIN(hud2::Layout, 552, 76);
FLV_STATE_PIN(hud2layout::Out, 640, 70);
FLV_STATE_PIN(hud2layout::RevealRules, 52, 13);
FLV_STATE_PIN(hud2layout::RevealOut, 28, 10);
FLV_STATE_PIN(hud2layout::Reveal, 64, 14);
FLV_STATE_PIN(hud2::Fades, 72, 18);
FLV_STATE_PIN(hud2::ViewBack, 32, 8);
FLV_STATE_PIN(promptshow::State, 12, 3);
FLV_STATE_PIN(hudwake::State, 20, 5);
FLV_STATE_PIN(hud2notice::LoadWatch, 8, 2);
FLV_STATE_PIN(hud2target::Out, 72, 18);
FLV_STATE_PIN(hud2target::State, 120, 0);
FLV_STATE_PIN(hud2kit::MarqueeClock, 16, 2);
FLV_STATE_PIN(hud2state::MarqueeEntry, 24, 2);
FLV_STATE_PIN(hud2kit::Marquees, 32, 1);
FLV_STATE_PIN(hud2::PartsState, 580, 35);
FLV_STATE_PIN(decltype(hud2::FrameState::navEase), 32, 6);
FLV_STATE_PIN(decltype(hud2::FrameState::panelEase), 48, 9);
FLV_STATE_PIN(hud2::FrameState, 7224, 66);
FLV_STATE_PIN(hud2::ViewRect, 20, 5);
FLV_STATE_PIN(hud2::StartOut, 6, 6);
FLV_STATE_PIN(hud2::StepOut, 44, 3);
FLV_STATE_PIN(hud2state::Tape, 60, 5);
FLV_STATE_PIN(hud2state::HotRect, 20, 5);
FLV_STATE_PIN(hud2state::UpdateRec, 116, 4);
FLV_STATE_PIN(hud2state::RenderRec, 7352, 11);
FLV_STATE_PIN(hud2state::UnderRec, 84, 8);
FLV_STATE_PIN(hud2state::LayerPrint, 28, 2);
FLV_STATE_PIN(hud2state::SetPrint, 48, 2);
FLV_STATE_PIN(hud2state::ListRef, 108, 6);
FLV_STATE_PIN(hud2state::StateDoc, 15736, 11);
