// The settings registry's parsing (core\settings.h Apply): numbers, clamping, and failing closed on what does not parse
// (a hand-edited ini: "nan", "inf", "1e10" into an unclamped int, a word) - the setting then keeps its value.
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "settings.h"

#include "test.h"

void Test_settings()
{
    bool b = false;
    int i = 7, u = 3;
    float f = 0.5f;
    const settings::Def defs[] = {
        {"b", settings::Bool, &b, 0, 1, ""},
        {"i", settings::Int, &i, 0, 10, ""},
        {"u", settings::Int, &u, 0, 0, ""}, // unclamped
        {"f", settings::Float, &f, 0.1f, 2, ""},
    };
    const settings::Def &db = defs[0], &di = defs[1], &du = defs[2], &df = defs[3];
    // numbers as before: "1", "0.5", truncation, clamping
    CHECK(settings::Apply(db, "1") && b);
    CHECK(settings::Apply(db, "0") && !b);
    CHECK(settings::Apply(di, "4.9") && i == 4);
    CHECK(settings::Apply(di, "99") && i == 10);
    CHECK(settings::Apply(df, "5") && f == 2.0f);
    CHECK(settings::Apply(df, " 1.5 ") && f == 1.5f);
    CHECK(settings::Apply(di, "3 ; a note") && i == 3); // a hand-edited ini line
    // words for bools
    CHECK(settings::Apply(db, "true") && b);
    CHECK(settings::Apply(db, "OFF") && !b);
    // fails closed: the value stays
    CHECK(!settings::Apply(df, "nan") && f == 1.5f);
    CHECK(!settings::Apply(df, "inf") && f == 1.5f);
    CHECK(!settings::Apply(di, "abc") && i == 3);
    CHECK(!settings::Apply(di, "") && i == 3);
    CHECK(!settings::Apply(di, "2x") && i == 3);
    CHECK(!settings::Apply(db, "maybe") && !b);
    // an unclamped int never goes through an undefined float -> int conversion
    CHECK(settings::Apply(du, "1e10") && u == 2147483647);
    CHECK(settings::Apply(du, "-1e10") && u == -2147483647);
}
