#include "uiopt.h"

namespace uiopt
{
static UiOptions g_ui;
UiOptions& Get() { return g_ui; }

// The registry rows live in controller.cpp kOptions since the PAGE track's commit 001d858 (25 Oct 2026): this table
// is empty; controller.cpp copies the Options fields into Get() every frame (OnFrame "the controller UI's settings").
static const settings::Def* kDefs = nullptr;

const settings::Def* Defs(size_t& count)
{
    count = 0;
    return kDefs;
}
} // namespace uiopt
