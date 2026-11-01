// The random missions' (jobs') objective texts (IDEAS A40, A37_PLAN S1): DATA\RANDOMMISSIONS\vignetteparams.ini's
// `objective_text = <kind>, <text id>[, <arg> ...]` rows in its [DataNode]s (HDE 0.7.1: 33 rows). A job's objective
// step names its kind by the text id the game shows, so a step's kind comes from the game's own data instead of the
// objective mode's English keywords - when the id belongs to one kind only: an id several kinds share (327757 "RM
// bad_strid!", the default node's placeholder) is ambiguous and the caller falls back to the words.
// Pure over ini sections (tests\jobs_test.cpp); Install() reads the install's file once.
#pragma once
#include "common.h"
#include "ini.h"

#include <string>
#include <vector>

namespace jobs
{
struct ObjectiveText
{
    std::string kind;              // lower case: "guide_to_main_battle"
    u32 textId = 0;                // the string id the step shows
    std::vector<std::string> args; // what the text's %-codes are filled from ("Hostile_group", "TARGET_FULL_NAME")
    int node = -1;                 // the [DataNode]'s node_id (-1 none)
};

struct Table
{
    bool ok = false; // the file was read and has rows
    std::vector<ObjectiveText> rows;
};

// The rows of vignetteparams.ini's sections (every section's objective_text keys, in order).
Table FromSections(const std::vector<ini::Section>& sections);
// The install's DATA\RANDOMMISSIONS\vignetteparams.ini, read once (empty table when missing).
const Table& Install();

// What a text id names: "" when no row uses it, "ambiguous" when rows of more than one kind use it, else the kind.
// `rows` (optional): how many rows use the id; `kinds` (optional): the distinct kinds using it, in first-seen order.
std::string KindOf(const Table& t, u32 textId, int* rows = nullptr, std::vector<std::string>* kinds = nullptr);
// The distinct kinds of the table, in first-seen order.
std::vector<std::string> Kinds(const Table& t);
} // namespace jobs
