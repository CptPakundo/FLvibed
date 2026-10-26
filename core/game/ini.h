// Freelancer's plain-text .ini files (DATA\...): sections in order, repeated keys kept, comments (;) stripped; a binary
// (BINI) file reads as empty. Paths are relative to DATA (beside EXE\Freelancer.exe), resolved the way the game's own
// files are laid out. The string helpers (Trim, Lower, StripComment) live in core\str.h.
#pragma once
#include "common.h"

#include <string>
#include <utility>
#include <vector>

namespace ini
{
struct Section
{
    std::string type;                                    // lower case, without brackets: "shieldgenerator"
    std::vector<std::pair<std::string, std::string>> kv; // keys lower case, values trimmed

    const std::string* Get(const char* key) const; // first value, nullptr if absent
    std::vector<std::string> All(const char* key) const;
};

// DATA\<relative> (e.g. "EQUIPMENT\\st_equip.ini"), full path.
std::string DataPath(const std::string& relative);
// The files the game loads for a [Data] key of EXE\freelancer.ini ("ships", "fuses", ...), full paths in its order.
std::vector<std::string> DataFiles(const char* key);
// The file's sections (empty when missing, or a binary BINI file).
std::vector<Section> Read(const std::string& fullPath);
// Splits "a, b, c" into trimmed fields.
std::vector<std::string> Fields(const std::string& value);
} // namespace ini
