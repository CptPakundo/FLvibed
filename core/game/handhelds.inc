// FLvibed - which machines are handhelds: data for core\game\handheld.h, one row per model. Written from the kernel's and
// InputPlumber's tables (modules\hud\docs\UI_HUD2_INPUTS.md 7; evidence inputs_handheld_detection.md); nothing here ran on the hardware.
// Row: where the name is read (System = SMBIOS type 1 product, Board = type 2 product, Either), how it is matched
// (Exact, Prefix), the name, the maker it must come from ("" = any: a name no other maker uses; a prefix, either case),
// and what it decides (NotHandheld, Handheld; Deck = a Steam Deck, whose docked rule differs).
// Rows that name a machine that is no handheld come first: the first match wins.
{Either, Exact, "Fremont", "", NotHandheld},   // the Steam Machine
{System, Exact, "G1622-01", "", NotHandheld},  // GPD Duo
{System, Exact, "G1628-04", "", NotHandheld},  // GPD Pocket 4
{System, Exact, "G1688-08", "", NotHandheld},  // GPD MicroPC 2
{System, Exact, "MicroPC", "GPD", NotHandheld},
{Either, Exact, "Jupiter", "Valve", Deck},
{Either, Exact, "Galileo", "Valve", Deck},
{Board, Exact, "RC71L", "", Handheld},   // ROG Ally
{Board, Exact, "RC72LA", "", Handheld},  // ROG Ally X
{Board, Exact, "RC73YA", "", Handheld},  // ROG Xbox Ally
{Board, Exact, "RC73XA", "", Handheld},  // ROG Xbox Ally X
{Board, Exact, "RC74XA", "", Handheld},  // ROG Xbox Ally X20
{System, Exact, "83E1", "LENOVO", Handheld}, // Legion Go
{System, Exact, "83N0", "LENOVO", Handheld}, // Legion Go 2
{System, Exact, "83N1", "LENOVO", Handheld},
{System, Exact, "83L3", "LENOVO", Handheld}, // Legion Go S
{System, Exact, "83N6", "LENOVO", Handheld},
{System, Exact, "83Q2", "LENOVO", Handheld},
{System, Exact, "83Q3", "LENOVO", Handheld},
{Board, Exact, "MS-1T41", "", Handheld}, // MSI Claw A1M
{Board, Exact, "MS-1T42", "", Handheld}, // Claw 7 AI+
{Board, Exact, "MS-1T52", "", Handheld}, // Claw 8 AI+
{Board, Exact, "MS-1T8K", "", Handheld}, // Claw A8
{Board, Exact, "MS-1T91", "", Handheld}, // Claw 8 EX AI+
{Board, Exact, "G0A1W", "", Handheld},   // Zotac Zone
{Board, Exact, "G1A1W", "", Handheld},
{Either, Prefix, "G1618-", "", Handheld}, // GPD Win 3, 4, 5
{Either, Prefix, "G1617-", "", Handheld}, // GPD Win Mini
{Either, Prefix, "G1619-", "", Handheld}, // GPD Win Max 2
{System, Exact, "WIN2", "GPD", Handheld},
{Either, Prefix, "AIR", "AYA", Handheld},  // AYANEO: bare names, so the maker decides (AYANEO, AYADEVICE, AYA NEO, AYA)
{Either, Prefix, "AB05", "AYA", Handheld},
{Either, Prefix, "AYANEO", "AYA", Handheld},
{Either, Prefix, "AYA NEO", "AYA", Handheld},
{Either, Exact, "AB07", "AYA", Handheld},
{Either, Exact, "AB09", "AYA", Handheld},
{Either, Exact, "AB10", "AYA", Handheld},
{Either, Prefix, "GEEK", "AYA", Handheld},
{Either, Prefix, "NEXT", "AYA", Handheld},
{Either, Exact, "KUN", "AYA", Handheld},
{Either, Exact, "SLIDE", "AYA", Handheld},
{Either, Exact, "AS01", "AYA", Handheld},
{Either, Prefix, "FLIP", "AYA", Handheld},
{Either, Prefix, "ONEXPLAYER", "", Handheld},
{Either, Prefix, "ONE XPLAYER", "", Handheld},
{Either, Prefix, "AOKZOE", "", Handheld},
{Either, Prefix, "Loki", "AYN", Handheld},
