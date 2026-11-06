// The NPC module's entry: its table for the logic's slots (core\game\modslots.h). Read only: it records, it changes nothing in the game.
#include "game/modslots.h"
#include "log.h"
#include "stallrec.h"

static bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op != "stalls")
        return false;
    reply = stallrec::Command(in);
    return true;
}

static const modslots::Module kModule = {nullptr, nullptr, stallrec::Update, nullptr, Command, nullptr, nullptr, nullptr};

extern "C" const modslots::Module* flv_npc_module() { return &kModule; }
