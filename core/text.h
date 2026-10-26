// Freelancer's string resources (shared by every module, compiled into each).
#pragma once
#include "common.h"

#include <string>

namespace text
{
// The text of a string id: dll index = ids >> 16 (0 = resources.dll, then the DLLs of freelancer.ini [Resources] in
// order), LoadStringW(ids & 0xFFFF). Empty for 0 or an unknown id.
std::wstring Ids(u32 ids);
// Ids() as UTF-8.
std::string IdsUtf8(u32 ids);
// UTF-16 -> UTF-8.
std::string Utf8(const std::wstring& w);
} // namespace text
