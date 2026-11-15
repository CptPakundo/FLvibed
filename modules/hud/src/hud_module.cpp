// The HUD module's table for the logic's slots (core\game\modslots.h). It holds two entries: the extras' (extras\extras.h: the cues, game_text_size,
// the idle cursor's hide, kept on both interfaces) and HUD 2's (hud2_entry.h). Each publishes its own places in core's frame at Attach (core\ui\views.h
// ExtrasView, HudView, SkinView); off, every place is empty: the game's own HUD, the classic look, and what the pad's prompts need from core\ui\winplates.*.
#include "extras/extras.h"
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "hud2_entry.h"
#include "ui/hud_door.h"

#include <sstream>

static void Attach()
{
    LogJoin(hostcalls::g.LogWrite);
    extras::Attach();
    hud2entry::Attach();
}

static void Detach()
{
    hud2entry::Detach();
    extras::Detach();
}

// Render.cpp's D3D textures go with the logic: every owner frees its own, and the game's panels, windows and colours go back.
static void Shutdown()
{
    extras::Shutdown();
    hud2entry::Shutdown();
}

// A fault: HUD 2's give-back first (the game's panels, its cursor, the click box; its windows' art in phase 1), then the extras' (the idle cursor shown).
static void Fault(int phase)
{
    hud2entry::Fault(phase);
    extras::Fault(phase);
}

// Its debug verbs: the extras' (cue, gametext) and HUD 2's. `gametext` is both: its second word picks the entry.
static bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op == "gametext") // gametext [rich <percent>] | window | frame 0|1 | texts | bar | fit
    {
        std::string what;
        int pct = 100;
        in >> what >> pct;
        if (!hud2entry::GameTextVerb(what, pct, reply))
            reply = extras::GameTextVerb(what, pct);
        return true;
    }
    return extras::Command(op, in, reply) || hud2entry::Command(op, in, reply);
}

// A raw key for HUD 2's dialogs only while HUD 2 is selected (core\ui\hud_door.h): on the classic HUD every key is the game's, the key-up of an Enter held
// across the switch too. Counted as a call into the module, as its views are.
static bool Key(int vk, bool down)
{
    if (huddoor::Classic())
        return false;
    ++modslots::g_viewCalls[modslots::kHud];
    return hud2entry::Key(vk, down);
}

static const modslots::Module kModule = {nullptr, Shutdown, nullptr, nullptr, Command, nullptr, Attach, Detach, Fault, Key};

extern "C" const modslots::Module* flv_hud_module() { return &kModule; }
