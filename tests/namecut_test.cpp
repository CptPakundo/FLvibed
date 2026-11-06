// A pilot's name past the game's cut (core\game\namecut.h).
#include "test.h"

#include "../core/game/namecut.h"

void Test_namecut()
{
    using namecut::Join;
    // the plain cut, and the cut under a prefix the record does not hold
    CHECK(Join(L"Rookie Robert Washingto", L"Rookie Robert Washington") == L"Rookie Robert Washington");
    CHECK(Join(L"Escort 1 Rookie Langsto", L"Rookie Langston") == L"Escort 1 Rookie Langston");
    CHECK(Join(L"Transport DSE Alexandri", L"Alexandria") == L"Transport DSE Alexandria");
    CHECK(Join(L"Escort 1 Rookie Kirkpat", L"Rookie Ray Kirkpatrick") == L"Escort 1 Rookie Kirkpatrick");
    // the template's call sign is no name: seen in game as "Rookie Dennis Schneiders Black 17" and "Escort 2 Ens. Katayanagi 0"
    CHECK(Join(L"Rookie Dennis Schneider", L"Rookie Dennis Schneiders Black 17") == L"Rookie Dennis Schneiders");
    CHECK(Join(L"Escort 2 Ens. Katayanag", L"Ens. Katayanagi 0") == L"Escort 2 Ens. Katayanagi");
    CHECK(Join(L"", L"Rookie Dennis Schneiders Black 17") == L"Rookie Dennis Schneiders");
    CHECK(namecut::Bare(L"Hans Becker") == L"Hans Becker");
    CHECK(namecut::Bare(L"Escort 2") == L"Escort 2"); // (no word before the number but the name's own: Bare is for records; the cache keeps a prefix)
    // an encounter's pilot: the record holds a call sign, the surname comes from the game's list
    {
        const std::vector<std::wstring> sur = {L"Byrd", L"Carmichael", L"Dettweiler", L"Elizabeth", L"Elizabethan", L"Kirkpatrick", L"Miller", L"Millard"};
        std::vector<std::wstring> s = sur;
        std::sort(s.begin(), s.end());
        CHECK(namecut::Complete(L"Rookie Thomas Carmichae", s) == L"Rookie Thomas Carmichael");
        CHECK(namecut::Complete(L"Escort 1 Rookie Kirkpat", s) == L"Escort 1 Rookie Kirkpatrick");
        CHECK(namecut::Complete(L"Escort 1 Rookie Dettwei", s) == L"Escort 1 Rookie Dettweiler");
        CHECK(namecut::Complete(L"Rookie Christopher Byrd", s) == L"Rookie Christopher Byrd");    // whole at 23
        CHECK(namecut::Complete(L"Rookie Daniel Elizabeth", s) == L"Rookie Daniel Elizabeth");    // a surname itself
        CHECK(namecut::Complete(L"Rookie Thomasina Mill", s) == L"Rookie Thomasina Mill");        // under the cut
        CHECK(namecut::Complete(L"Rookie Thomasinaaa Mill", s) == L"Rookie Thomasinaaa Mill");    // two fit
        CHECK(namecut::Complete(L"Rheinland Cruiser Donau", s) == L"Rheinland Cruiser Donau");    // none fits
        CHECK(namecut::Complete(L"Rookie Thomas Carmichae", {}) == L"Rookie Thomas Carmichae");   // no list
    }
    // under the cut the cache stands, whatever the record says
    CHECK(Join(L"Ken Norris", L"Ken Norrison") == L"Ken Norris");
    // exactly 23 and whole: the record adds nothing
    CHECK(Join(L"Rheinland Cruiser Donau", L"Rheinland Cruiser Donau") == L"Rheinland Cruiser Donau");
    // a mission's name with an unrelated record
    CHECK(Join(L"Rheinland Cruiser Donau", L"Hans Becker") == L"Rheinland Cruiser Donau");
    // no cache: the record; no record: the cache
    CHECK(Join(L"", L"Hans Becker") == L"Hans Becker");
    CHECK(Join(L"Rookie Robert Washingto", L"") == L"Rookie Robert Washingto");
}
