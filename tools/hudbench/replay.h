// FLvibed design bench - a recorded frame of HUD 2's flight HUD run again with no game (audit 71b B5a): the record
// (`hud2 bench record`, modules\hud\src\hud2_record.cpp: flstate/0, modules\hud\src\hud2_state_io.h StateDoc) gives
// Step and Draw (modules\hud\src\hud2_frame.h) their inputs and the state they took, a bench-side FrameHost answers
// every host call from the record's tape (strictly in call order per kind: a tape that runs out or is left over fails
// the replay), the kit's list back end (kitlist.h) catches the under and over passes, and fldl::ListJson writes them as
// the game wrote its list. Level 1 (the design's section 5) then compares the bench's list with the game's list beside
// the record, by content (vertex counts per layer and mark, texture names, colours, positions within 0.01 px) and by the
// record's fingerprints (each layer's kitdump hash, each font set's print), and Step's results with what the game's
// Draw took. What the bench does not draw (the holes) is named, never counted as a difference.
//
// The frame, in the game's order: Step from the state Step took (the record's `frame_state`, after the host's model
// refresh) with Update's tape; the under pass from the record's `under` (the view backings); Draw from the state the
// game's Draw took (`render.st`: after Render's relayout and the target's projection, both host work) with Draw's tape,
// into a pass opened as the game opened it (`render`: the back buffer's size, the text size asked). The comm caption
// the game draws after the mark `parts.end` is the game's window text: a hole.
#pragma once
#include "../../modules/hud/src/hud2_state_io.h"
#include "kitlist.h"

#include <string>
#include <vector>

namespace benchreplay
{
// What one replay of one record made.
struct Run
{
    bool ok = false;           // Step and Draw ran and each used its tape exactly
    std::string error;         // the first reason it did not (a tape that ran out or was left over, a pass not opened)
    hud2::StepOut stepOut;     // what Step asked of its host
    hud2::FrameState stepped;  // the state after Step (the game's Draw took this one, unless Render re-laid out)
    hud2::FrameState drawn;    // the state after Draw (started from the record's render.st)
    bool underDrawn = false;
    kitlist::Pass under, over; // the passes copied before their End, as the game's recorder copies them (an empty pass too)
    std::vector<std::unique_ptr<fldl::Texture>> textures; // their textures' descriptions
    int hotOn = -1;            // the hotspot sink's start (as the record's render.hotOn)
    std::vector<hud2state::HotRect> hot;
    int drawPx = 0;            // the kit's text size after Begin's floor
};
// Replays a record that is read (the kit's source opened: kitlist::Open). `run` must stay where it is while used: the
// parts' binding points into it until the next replay.
void Replay(const hud2state::StateDoc& doc, Run& run);
// Unbinds the kit's marquee table and clock from the last replay's state and empties the kit's own table, so the next
// replay starts as a game's first frame would (Replay calls it first; call it before `run` goes away).
void Unbind();

// A display list as level 1 reads it: per pass and layer the vertices (x, y, colour, u, v; u v relative to the box
// sampled) and each vertex's texture by name, a glyph's set named by what makes it (face, px, weight, fingerprint), so a
// game list's `AgencyFB-21-700` and the bench's `AgencyFB-21-700.2` of the same raster read alike.
struct ListLayer
{
    std::vector<float> x, y, u, v;
    std::vector<unsigned> color;
    std::vector<std::string> tex;
};
struct ListPass
{
    bool writeAlpha = false;
    ListLayer layers[hud2kit::LCount];
    std::vector<std::pair<std::string, std::vector<unsigned>>> marks; // name, six counts
};
struct ListRead
{
    int w = 0, h = 0;
    ListPass under, over;
    std::vector<std::pair<std::string, std::string>> sets; // name, "face px weight print"
};
bool ReadList(const std::string& text, ListRead& out, std::string& error);

// Level 1's verdict on one record.
struct Verdict
{
    int differences = 0;              // list content, hotspots, Step's results (each counted once per item)
    int fontDifferences = 0;          // a font set's fingerprint not the game's (the raster differs: exit 3)
    std::vector<std::string> lines;   // the differences, the first few of each kind
    std::vector<std::string> holes;   // what the bench does not draw, named with its reason
    std::vector<std::string> notes;   // what was compared and how (exact, within tolerance)
    bool exactOver = false, exactUnder = false; // the layers' fingerprints equal the record's (bit for bit)
    float maxMove = 0;                // the largest position difference met (px)
};
// Compares a replay with the record and the game's list (its text). `benchList` the bench's list of the same frame;
// `retired` the members the reader read past in the record (hud2state::Read), written back for its inputs' fingerprint.
void Compare(const hud2state::StateDoc& doc, const std::string& gameListText, const Run& run, const std::string& benchList,
             Verdict& out, const std::vector<hud2state::RetiredMember>* retired = nullptr);

// ---------------------------------------------------------------- a variant (B5a's rest): the recorded state drawn
// with settings, preset numbers or the back buffer's size changed. The tape is then answered leniently (the changed code
// may ask the clocks more or less often: past its end the last answer, the rest left), and the list is a design picture,
// not a replay: level 1 does not judge it. With no override the replay above runs, unchanged.

// One number of a layout preset's table (core\ui_meta.h kNums_<preset>, from UI_SPEC.json "hud2_presets") changed for
// the run: named "<preset>.<context>.<element>.<key>", e.g. "standard.calm.card.w".
struct LayoutNum
{
    std::string name;
    int preset = 0, index = 0; // the preset (hud_preset) and the number's place in its table
    float value = 0, was = 0;
};
struct Overrides
{
    std::vector<std::pair<std::string, std::string>> set; // --set name=value, in the order given
    std::vector<LayoutNum> layout;                        // --layout <file>
    int w = 0, h = 0;                                     // --size WxH (0: the record's)
    bool Any() const { return !set.empty() || !layout.empty() || w > 0; }
};
// "name=value" into `o.set` (the name checked when it is applied). False: not of that form.
bool ParseSet(const std::string& arg, Overrides& o, std::string& why);
// A layout file: one JSON object, "<preset>.<context>.<element>.<key>": number. Every name must be a number of the
// preset's table (a name it lacks fails: the code would never read it). False with the reason.
bool ReadLayout(const std::string& text, std::vector<LayoutNum>& out, std::string& why);
// Every preset number, one "name = value" per line (`flbench layout-names`).
std::string LayoutNames();
// The settings `set` written into a frame's copies (hud2_frame.h PartsIn, names as a record writes them:
// hud2_state_io.h). A name is a member's path or its end ("hud_scale" sets opt.hud_scale and rows.hud_scale; "rows.shield.on"
// one); `hud_size` = small / normal / large (1-3) resolves hud_size as the game's frame does (core\game\hudsize.h) at the
// height `H`. `lines` says what changed (and a value outside the options page's range). False: an unknown name, a bad value.
bool ApplySet(const std::vector<std::pair<std::string, std::string>>& set, float H, hud2::PartsIn& p,
              std::vector<std::string>& lines, std::string& why);

// Every setting a frame's copies carry that ApplySet can change (the design bench's window lists them: `flbench serve`):
// `name` the shortest name that sets them (a name several copies share, e.g. "hud_scale" for opt.hud_scale and
// rows.hud_scale, when they hold one value; else the full path), its value as text, its kind (bool, int, float, enum,
// text), and from the options page's row of that name (core\ui_meta.h) its label and range or choices.
struct SettingInfo
{
    std::string name, value, kind, label;
    bool range = false;                                     // min / max / step from the options page (the member's units)
    double min = 0, max = 0, step = 0;
    std::vector<std::pair<std::string, std::string>> options; // a picker's: the value to set, its word
    int copies = 1;
};
void SettingList(const hud2::PartsIn& p, std::vector<SettingInfo>& out);

// What a variant replay made beside Run's: its size, what it changed, what cannot be true of it, the 3D views' places.
struct VariantOut
{
    int w = 0, h = 0;
    bool relayout = false;          // drawn at another size than recorded
    std::vector<std::string> lines; // what was changed
    std::vector<std::string> notes; // what cannot be true of the picture (named, never hidden)
    hud2::StepOut views;            // the 3D views asked for (holes in the picture)
};
// Replays `doc` as a variant (Overrides::Any). `run.ok` false and `run.error` on an override that cannot apply.
void ReplayVariant(const hud2state::StateDoc& doc, const Overrides& o, Run& run, VariantOut& out);

// Two lists of the same frame compared as sets of vertices (place to 0.01 px, colour, texture): per pass and layer how
// many are only in one of them, and the box they lie in. Lines empty: the same picture.
void DiffLists(const ListRead& before, const ListRead& after, std::vector<std::string>& lines, size_t& changed);
} // namespace benchreplay
