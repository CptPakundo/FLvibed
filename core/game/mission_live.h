// The story mission's LIVE trigger table (Content.dll), read-only (IDEAS A39, 29 Oct 2026; docs\FREELANCER_RE.md "The
// live trigger table"). mission.h derives which triggers are armed from the script data; this reads the list the game
// itself keeps - the one a save writes as [TriggerSave] - so a mid-mission load ("Awaiting Mission Objective") knows
// exactly what the script waits for.
//
// Root: the Content.dll global that holds the story mission's controller id (written by pub::Controller::Create in the
// story's start / load paths, never cleared) -> Server.dll's controller list (that id, Content.dll's module) -> the
// mission object (its own id at +4 must match) -> its active list (std::list of trigger-map nodes: id, state,
// Condition*). Every offset the reader needs is taken from the game's code by signature (Content.dll's [TriggerSave]
// writer, the story path's store, Server.dll's controller lookup), resolved once per Content.dll / Server.dll load
// (base + TimeDateStamp), a missing signature cached as missing (no rescans). Single player only (a client of a remote
// server has no story controller): fails closed - no Content.dll, no story id, a signature missing, the id not in
// Server.dll's list (stale: the mission ended), a node unreadable - with the reason, never a guess. Read on the game's
// thread (the debug command and the controller's frame).
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace mission
{
struct LiveTrigger
{
    u32 id = 0;           // CreateID of the [Trigger] nickname (node +0xC)
    int state = -1;       // node +0x10: 0 armed (ACTIVE), 1 inactive, 2 complete (InitState; Cnd_WatchTrigger's values)
    u32 condRva = 0;      // the Condition's vtable as an RVA in Content.dll (0: unreadable / outside the module)
    const char* cond = nullptr; // its Cnd_ name from the vtable map of this Content.dll build (verified rows only), else null
    u32 condField[2] = {};      // Condition +0xC / +0x10: Cnd_Timer elapsed / duration (floats), Cnd_Destroyed count,
                                // Cnd_WatchTrigger the watched trigger id (other types: raw)
    std::string nickname;       // the mission ini's [Trigger] nickname (lower case), "" when no script knows the id
    std::string iniCond;        // that trigger's condition key in the ini ("cnd_timer"), "" unknown
};

struct LiveTable
{
    bool ok = false;
    std::string reason;  // why not ok (logged once per change)
    u32 controller = 0;  // the story mission's controller id
    std::string script;  // the mission ini the ids name in ("" none)
    std::vector<LiveTrigger> triggers; // in the live list's order (activation order, not the save file's)
};

// The live table now (cheap: a few hundred guarded reads plus map lookups over the scripts mission.cpp read once).
LiveTable LiveTriggers();
const char* TriggerStateName(int state);
// Debug: `mission live` (the table, one line per trigger), `mission live raw` (also the layout and addresses).
bool Command(const std::string& cmd, const std::string& rest, std::string* reply);

// ---- the pure pieces (tests\mission_live_test.cpp drives them over a fake memory) ----
namespace live
{
typedef bool (*ReadFn)(uintptr_t addr, void* out, size_t len, void* ctx);

struct Layout
{
    uintptr_t content = 0, contentEnd = 0; // Content.dll's image (HMODULE .. + SizeOfImage)
    uintptr_t storyIdAt = 0;               // the story controller id global
    uintptr_t ctlHeadAt = 0;               // Server.dll: the controller list's head pointer
    u32 ctlIdOff = 0x18;                   // list node: controller id (the lookup's cmp [ecx+18h])
    u32 ctlModOff = 0x0C, ctlInstOff = 0x08; // list node: HMODULE, IDLL instance (pub::Controller::Create's node)
    u32 ownIdOff = 4;                      // instance: its own controller id (the IDLL base constructor)
    u32 listHeadOff = 0;                   // mission: the active list's head pointer (the writer's 0x1C8; size at +4)
    u32 listValOff = 8;                    // list node: the value (a trigger-map node pointer; the writer's [edi+8])
    u32 idOff = 0, recOff = 0, condOff = 0; // trigger-map node: id (0xC), record = state (0x10), Condition* in it (4)
    u32 condBackOff = 4;                   // Condition: its mission (the base constructor's back pointer)
};

struct Raw
{
    u32 id = 0;
    int state = -1;
    uintptr_t cond = 0, condVt = 0;
    u32 f[2] = {};
};

// Walks root -> controller -> instance -> active list. nullptr on success (`controller`, `out` filled), else the reason
// (`out` then holds no answer; `controller` the story id read, 0 if none).
const char* Walk(const Layout& l, ReadFn read, void* ctx, u32& controller, std::vector<Raw>& out);

// The Cnd_ name of a Condition vtable RVA in the Content.dll build (TimeDateStamp, SizeOfImage) the map was made for;
// null for another build or an unknown RVA. `status` (optional): 0 scripted only (null returned), 1 verified statically,
// 2 verified in game.
const char* CondName(u32 timeDateStamp, u32 sizeOfImage, u32 vtRva, int* status = nullptr);
} // namespace live
} // namespace mission
