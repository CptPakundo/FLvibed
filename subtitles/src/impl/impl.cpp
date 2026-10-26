// Subtitles logic entry points (FLSubtitlesImpl.dll in the dev build, linked into FLvibed.dll in the release
// build): turns game sound events into subtitles, plus the debug commands used by tools\flsub.ps1.
#include "../api.h"
#include "common.h"
#include "flexe.h"
#include "log.h"
#include "settings_bus.h"
#include "chat.h"
#include "data.h"
#include "display.h"
#include "feed.h"
#include "movie.h"
#include "names.h"
#include "speakers.h"
#include "game/render.h"
#include "game/room.h"
#include "game/space.h"

#include <sstream>
#include <unordered_map>

// Research capture at the game's own frame rate (user, 19 Oct 2026: shield hits "at very high framerates"): `grab
// <frames> <x> <y> <w> <h>` records that back-buffer region on each of the next frames (every Present, out of focus),
// then writes EXE\grab\f000.bmp ... and a times.txt (seconds since the first frame).
static struct
{
    int left = 0, x = 0, y = 0, w = 0, h = 0, step = 1;
    bool armed = false; // `grab ... comm`: waits for the next comm to the player, then records
    std::vector<std::vector<u32>> frames;
    std::vector<double> times;
} g_grab;

const HostApi* g_host = nullptr; // also used by movie.cpp
static bool g_trace = true;

static std::string IniPath() { return std::string(g_host->dllDir) + "FLSubtitles.ini"; }
static std::string DataPath() { return std::string(g_host->dllDir) + "FLSubtitles\\subtitles.tsv"; }

// Every character the font atlases must hold, so no line rebuilds them mid-game (display::SeedGlyphs): the data's speakers
// and texts, the speaker names (the movie lines are seeded where they load: ImplInit), and the punctuation the
// runtime itself produces: the middle dot in "name · faction" (OnComm here, names.cpp BarLabel), the en dash that
// JoinPieces writes for a spoken dash, the em dash and the ellipsis (chat.cpp / display.cpp break on them). A character
// still unseen (a name the game supplies) rebuilds the atlases on its first line, as before.
static void SeedGlyphs()
{
    std::vector<bool> seen(0x10000, false);
    std::wstring distinct;
    auto add = [&](const std::wstring& s)
    {
        for (const wchar_t c : s)
            if (c >= 32 && !seen[c])
                seen[c] = true, distinct += c;
    };
    add(L"\x00B7\x2013\x2014\x2026");
    for (const data::Line& ln : data::Lines())
        add(ln.speaker), add(ln.text);
    add(speakers::Names());
    display::SeedGlyphs(distinct);
    size_t beyond = 0;
    for (const wchar_t c : distinct)
        beyond += c > 126;
    Log("glyphs: %zu distinct characters seeded, %zu beyond ASCII", distinct.size(), beyond);
}

// The subtitle data and the characters' name colours (checked against it).
static bool LoadData()
{
    const bool ok = data::Load(DataPath());
    if (ok)
        data::LoadWordTimes(std::string(g_host->dllDir) + "FLSubtitles\\word_times.tsv");
    speakers::Load(std::string(g_host->dllDir) + "FLSubtitles\\speaker_colors.tsv");
    SeedGlyphs();
    return ok;
}

static u32 PlayerShip() { return Mem<u32>(exe::PlayerShipId); }

// ------------------------------------------------------------------ sentence assembly
static bool Ended(const std::wstring& s)
{
    if (s.empty())
        return false;
    const wchar_t c = s.back();
    return c == L'.' || c == L'!' || c == L'?' || c == L'\x2026' || c == L'\x2013' || c == L'-';
}

// The game plays a chatter message as pieces; join them the way merge.render_sentence does: single
// spaces, each sentence starting with a capital and ending with "." unless it already ends with a mark.
// A piece whose id ends in "-" closes a sentence unless a name piece completes it.
static std::wstring JoinPieces(const std::vector<const data::Line*>& parts)
{
    std::wstring out;
    bool fresh = true;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        std::wstring t = parts[i]->text;
        while (!t.empty() && iswspace(t.back()))
            t.pop_back();
        if (t.empty())
            continue;
        if (fresh)
            t[0] = towupper(t[0]);
        out += (out.empty() ? L"" : L" ") + t;
        const wchar_t last = t.back();
        fresh = last == L'!' || last == L'?' || (last == L'.' && !(t.size() > 1 && t[t.size() - 2] == L'.'));
        const std::string& id = parts[i]->id;
        const bool closes = !id.empty() && id.back() == '-';
        const bool nameNext = i + 1 < parts.size() && _strnicmp(parts[i + 1]->id.c_str(), "gcs_refer_", 10) == 0;
        // "This is [x]. To [y] ..." (user, 22 Oct 2026: "the period after the speaking NPC doesn't make sense when the
        // next word is to"): a piece going on with "to" continues the sentence - no period, "to" stays lower case
        bool toNext = false;
        if (i + 1 < parts.size())
        {
            std::wstring n = parts[i + 1]->text;
            while (!n.empty() && iswspace(n.front()))
                n.erase(0, 1);
            toNext =
                n.size() >= 2 && towlower(n[0]) == L't' && towlower(n[1]) == L'o' && (n.size() == 2 || !iswalpha(n[2]));
        }
        if (toNext)
        {
            if (!out.empty() && out.back() == L'.' && !(out.size() > 1 && out[out.size() - 2] == L'.'))
                out.pop_back();
            fresh = false;
            continue;
        }
        if (i + 1 == parts.size() || (closes && !nameNext))
        {
            if (!Ended(out))
                out += L'.';
            fresh = true;
        }
    }
    // "1 dash 2" -> "1-2"
    for (size_t p; (p = out.find(L" dash ")) != std::wstring::npos;)
    {
        if (p > 0 && iswdigit(out[p - 1]) && p + 6 < out.size() && iswdigit(out[p + 6]))
            out.replace(p, 6, L"-");
        else
            out.replace(p, 6, L" \x2013 ");
    }
    return out;
}

// ------------------------------------------------------------------ events
// Lines shown recently (msg id -> GetTickCount), so a line reported by two hooks shows once.
static std::unordered_map<u32, DWORD> g_recent;

static bool SeenRecently(u32 msg)
{
    const DWORD now = GetTickCount();
    auto it = g_recent.find(msg);
    const bool seen = it != g_recent.end() && now - it->second < 1500;
    g_recent[msg] = now;
    if (g_recent.size() > 256)
        g_recent.clear();
    return seen;
}

// Game frames seen (the frame hook runs from the main menu on; the startup movies run their own loop, and
// this DLL is already loaded then): deploy.ps1 skips movies until it is > 0.
static unsigned g_frames = 0;

// A word of the comm record (+0x08 the costume: head, +0x0C body, ...; docs/DISPLAY_PLAN.md). The host calls OnComm
// inside its exception guard.
static u32 CommWord(const CommInfo& c, int off)
{
    return c.record ? *reinterpret_cast<const u32*>(static_cast<const u8*>(c.record) + off) : 0;
}

static void OnComm(const CommInfo& c)
{
    std::vector<const data::Line*> lines;
    std::ostringstream trace;
    float sec = 0;
    bool anyChunk = false, anyNn = false, anyUnique = false;
    int severity = 0; // Neural Net lines: the highest of the pieces
    for (int i = 0; i < c.count; ++i)
    {
        const data::Line* ln = data::Find(c.voice, c.lines[i]);
        if (g_trace)
        {
            char h[16];
            sprintf_s(h, "%08X", c.lines[i]);
            trace << " [" << (ln ? ln->id : h) << "]";
        }
        if (!ln)
            continue;
        lines.push_back(ln);
        sec += ln->sec;
        anyChunk |= ln->kind == data::Kind::Chunk;
        anyNn |= ln->kind == data::Kind::Nn;
        if (ln->kind == data::Kind::Nn)
            severity = std::max(severity, display::NnSeverity(ln->id));
        anyUnique |= ln->kind == data::Kind::Unique || ln->kind == data::Kind::Story;
    }
    const u32 player = PlayerShip();
    const bool toPlayer = player && (c.to == player);
    if (g_trace)
        Log("comm from=%u to=%u%s voice=%08X head=%08X body=%08X n=%d%s", c.from, c.to, toPlayer ? "(player)" : "",
            c.voice, CommWord(c, 0x08), CommWord(c, 0x0C), c.count, trace.str().c_str());
    if (lines.empty())
        return;
    if (anyNn && display::Hud2ShowsAlert(lines[0]->id))
        return; // HUD 2's alert card shows this event (one place per event)
    if (SeenRecently(c.lines[0]))
        return;

    std::wstring text;
    // word timings of pieces played one after another (their clips back to back); none for joined chatter pieces
    std::vector<float> wordStarts;
    bool timed = !(anyChunk && !anyUnique);
    if (anyChunk && !anyUnique)
        text = JoinPieces(lines);
    else
    {
        float at = 0;
        for (const data::Line* ln : lines)
        {
            text += (text.empty() ? L"" : L" ") + ln->text;
            timed &= !ln->wordStarts.empty();
            for (const float s : ln->wordStarts)
                wordStarts.push_back(s < 0 ? -1.0f : at + s); // (an unplaced word stays unknown)
            at += ln->sec;
        }
    }

    // Speaker: a named character's shipped name (mission and story lines); generic voices (chatter pieces) carry no
    // reliable name in the data: the sender's own name as the game's comm caption gives it - a ship's pilot ("Darcy
    // Kale"), else the object's name ("Fort Bush") - and its faction after it (user, 21 Oct 2026: "the name of the NPC
    // ... and not just their faction ... for both ships and bases/solars"); the faction alone when it has no name.
    const names::Sender who = names::Lookup(c.from);
    std::wstring speaker = anyUnique || anyNn ? lines[0]->speaker : L"";
    if (speaker.empty() && (anyUnique || anyNn))
        speaker = data::VoiceName(c.voice);
    if (speaker.empty())
    {
        const std::wstring name = space::SpeakerName(c.from);
        speaker = name.empty()                                 ? who.faction
                  : who.faction.empty() || who.faction == name ? name
                                                               : name + L" \x00B7 " + who.faction;
    }

    // Relation to Trent: the attitude the HUD uses (hostile <= -0.6, friendly >= 0.6), named story characters as
    // allies, taunts as hostile, the Neural Net as the computer. The name colour is the relation's only for speakers
    // the data does not name (display.cpp SpeakerColor: Trent amber, story characters their own colour).
    display::Relation rel = display::Relation::Neutral;
    if (anyNn)
        rel = display::Relation::Computer;
    else if (who.known && who.attitude <= -0.6f)
        rel = display::Relation::Hostile;
    else if (_strnicmp(lines[0]->id.c_str(), "rms_", 4) == 0)
        rel = display::Relation::Hostile;
    else if ((anyUnique && !speaker.empty()) || (who.known && who.attitude >= 0.6f))
        rel = display::Relation::Ally;
    // the story's stance for this speaker in this mission (the Order ship in M01: its reputation does not say it)
    if (!anyNn && speakers::Hostile(speaker, lines[0]->id))
        rel = display::Relation::Hostile;

    // The game opens its comm window for a comm to the player, not from the player, with a costume (head, body, hands or
    // an accessory; record +0x08..+0x3B: four part ids, eight accessories, their count) - decomp hud_comm.cpp Dispatch
    const bool costume =
        CommWord(c, 0x08) || CommWord(c, 0x0C) || CommWord(c, 0x10) || CommWord(c, 0x14) || CommWord(c, 0x38);
    const bool portrait = toPlayer && c.from != player && costume;

    // Which channel (the owner, 26 Oct 2026: story lines and dialogue take centre stage; NPC lines that do not invoke the
    // comm portrait go to the side chat, so chatter never replaces or covers a story line). Decided from what the game
    // decides for this comm, no name lists: a scripted story line (the data's story / unique kind: the mission's own
    // DX_ / scene ids) or a comm the game opens its portrait for (to the player, from someone else, with a costume - its
    // Dispatch rule; dock replies, a wingman's line to Trent) = the dialogue; joined gcs_ chatter between others, a
    // broadcast, an announcement or a costume-less comm = the chat. (Until 26 Oct 2026 any comm to the player counted as
    // dialogue, and a story character's own chatter voice could replace their story line as "the same speaker".)
    display::Category cat = anyNn                   ? display::Category::Nn
                            : anyUnique || portrait ? display::Category::Comms
                                                    : display::Category::Chatter;
    if (cat == display::Category::Chatter && display::Get().chatter == 0)
    {
        // Every chatter line goes into the game's chat log (nothing lost: a rolling history the chat input shows even
        // under HUD 2, which hides the window); while HUD 2 shows, its comm feed draws it in HUD 2's chat band as well
        // (one comm surface at a time: the feed waits while the top panel shows and drops what went stale - the log keeps it)
        chat::Print(speaker, display::SpeakerRgb(speaker, rel), text);
        display::Hud2Feed(speaker, display::SpeakerRgb(speaker, rel), text, sec);
        return;
    }
    if (toPlayer)
        g_grab.armed = false; // `grab ... comm` starts recording now
    display::Show(cat, speaker, rel, text, sec, severity, portrait, timed ? &wordStarts : nullptr);
}

// The bar character speaking with this voice (user, 22 Oct 2026: bar NPC lines had no names). The base's list
// (mbases.ini) shares voices widely (Manhattan: rvp101 ten times, the officers 220011 and 220012 both rvp111), so the
// room's own characters decide: each holds its mbases individual_name at +0x160 (found 22 Oct 2026: the bartender
// 244609, the officers 220010-220012) and a conversation byte at +0x36C, 1 on the player's conversation partner while
// the conversation runs (byte diff idle / talking / after, 22 Oct 2026; +0x32C, used first, stays set after a talk and
// mixes other bytes into its word). The partner with the line's voice wins; else the one room character with it; else
// the base-wide voice match.
static bool ReadRoomCharacter(const void* c, u32& nameIds, bool& partner)
{
    __try
    {
        nameIds = *reinterpret_cast<const u32*>(static_cast<const u8*>(c) + 0x160);
        partner = *(static_cast<const u8*>(c) + 0x36C) == 1;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The player's conversation partner in the room now (nullptr: none).
static const void* RoomPartner()
{
    if (!g_host->GetRoomCharacters)
        return nullptr;
    void* chars[64];
    const int n = g_host->GetRoomCharacters(chars, 64);
    for (int i = 0; i < n; ++i)
    {
        u32 nameIds = 0;
        bool partner = false;
        if (ReadRoomCharacter(chars[i], nameIds, partner) && partner)
            return chars[i];
    }
    return nullptr;
}

// One bar conversation (user, 22 Oct 2026): its first line names the character with the faction ("Name · Faction"),
// the rest by name alone; a new conversation clears the previous one's lines at once (the bartender's last line stood
// in the next character's scene). Followed every frame by the partner byte (TrackBarConversation).
static struct
{
    const void* partner = nullptr; // the room character talked to
    bool introduced = false;       // its first line was shown with the faction
} g_barConv;

static void TrackBarConversation()
{
    const void* p = RoomPartner();
    if (p == g_barConv.partner)
        return;
    if (g_trace)
        Log("bar conversation partner %p -> %p", g_barConv.partner, p);
    if (p)
        display::EndScene(); // a new conversation: the last one's lines are done
    g_barConv.partner = p, g_barConv.introduced = false;
}

static std::wstring BarSpeakerInRoom(u32 voice)
{
    const std::string dataDir = std::string(g_host->dllDir) + "..\\DATA\\";
    const u32 base = Mem<u32>(exe::PlayerBase);
    if (g_host->GetRoomCharacters)
    {
        void* chars[64];
        const int n = g_host->GetRoomCharacters(chars, 64);
        std::wstring match, partnerMatch;
        int matches = 0;
        bool isPartner = false;
        for (int i = 0; i < n; ++i)
        {
            u32 nameIds = 0;
            bool partner = false;
            if (!ReadRoomCharacter(chars[i], nameIds, partner))
                continue;
            std::wstring label;
            u32 v = 0;
            if (names::BarCharacter(dataDir, base, nameIds, label, v) && v == voice)
            {
                ++matches, match = label;
                if (partner)
                    isPartner = true, partnerMatch = label;
            }
        }
        if (isPartner || matches == 1)
        {
            std::wstring label = isPartner ? partnerMatch : match;
            if (isPartner)
            {
                // after the introduction the name alone ("Name · Faction" -> "Name")
                const size_t dot = label.find(L" \x00B7 ");
                if (g_barConv.introduced && dot != std::wstring::npos)
                    label.erase(dot);
                g_barConv.introduced = true;
            }
            return label;
        }
        if (matches > 1)
            return {};
    }
    return names::BarSpeaker(dataDir, base, voice);
}

static u32 g_unknownStarts = 0;
// Room scenes resolve a voice line with FindVoice + GetSound (caller exe::RetSceneVoice) just before starting it:
// remember which voice each msg came from, since bar lines exist once per voice (different takes).
static std::unordered_map<u32, u32> g_sceneVoice;

static void OnSoundStart(u32 id, const void*, u32 caller)
{
    const bool scene = exe::RetSceneSound.is(caller); // THN SOUND entity start (cutscenes, room and bar scenes)
    u32 voice = 0;
    if (scene)
    {
        auto it = g_sceneVoice.find(id);
        if (it != g_sceneVoice.end())
            voice = it->second;
    }
    const data::Line* ln = data::Find(voice, id);
    if (!ln)
    {
        ++g_unknownStarts; // sound effects, music, ambience
        return;
    }
    if (g_trace)
        Log("sound-start %s (%s) caller=%08X", ln->id.c_str(), ln->voice.c_str(), caller);
    // Bank lines played by the comm player (RetCommFirst first piece, RetCommNext the following pieces) are
    // shown by OnComm as one message; here only story and Neural Net lines.
    if (exe::RetCommFirst.is(caller) || exe::RetCommNext.is(caller))
        return;
    if (!scene && ln->kind != data::Kind::Story && ln->kind != data::Kind::Nn)
        return;
    if (SeenRecently(id))
        return;
    const bool nn = ln->kind == data::Kind::Nn;
    if (nn && display::Hud2ShowsAlert(ln->id))
        return; // HUD 2's alert card shows this event (one place per event)
    const bool bar = _strnicmp(ln->id.c_str(), "gcs_", 4) == 0;
    std::wstring text = ln->text;
    if (bar && ln->kind == data::Kind::Chunk && !Ended(text))
        text += L'.'; // a bar piece played alone is a sentence (joined pieces: see DISPLAY_PLAN, $BGCS)
    if (bar && !text.empty())
        text[0] = towupper(text[0]);
    const display::Category cat = nn ? display::Category::Nn : bar ? display::Category::Bar : display::Category::Story;
    const display::Relation rel = nn                                       ? display::Relation::Computer
                                  : speakers::Hostile(ln->speaker, ln->id) ? display::Relation::Hostile
                                  : ln->speaker == L"Trent"                ? display::Relation::Player
                                  : bar                                    ? display::Relation::Neutral
                                                                           : display::Relation::Ally;
    // a bar character without a name in the data (the generic patrons' speech pieces): named from the base's own list of
    // bar characters by the line's voice (user, 22 Oct 2026: "Bar NPC dialogue scenes don't include their names")
    std::wstring speaker = ln->speaker;
    if (speaker.empty() && bar && !ln->voice.empty())
        speaker = BarSpeakerInRoom(data::CreateID(ln->voice.c_str()));
    display::Show(cat, speaker, rel, text, ln->sec, nn ? display::NnSeverity(ln->id) : 0, false,
                  ln->wordStarts.empty() ? nullptr : &ln->wordStarts);
}

static void OnVoiceLookup(u32 voice, u32 msg, void* sound, u32 caller)
{
    if (exe::RetSceneVoice.is(caller))
    {
        if (g_sceneVoice.size() > 512)
            g_sceneVoice.clear();
        g_sceneVoice[msg] = voice;
    }
    if (!g_trace)
        return;
    // Known callers (traced 17 Oct 2026): RetMissionPreload1/2 preload a mission's lines; RetComm1..3 are the comm
    // path (covered by OnComm). Log only the others.
    if (exe::RetMissionPreload1.is(caller) || exe::RetMissionPreload2.is(caller) || exe::RetComm1.is(caller) ||
        exe::RetComm2.is(caller) || exe::RetComm3.is(caller))
        return;
    const data::Line* ln = data::Find(voice, msg);
    Log("voice-lookup voice=%08X msg=%08X %s sound=%p caller=%08X", voice, msg, ln ? ln->id.c_str() : "?", sound,
        caller);
}

// ------------------------------------------------------------------ debug commands
// Native game interaction (button clicks, room characters, room hotspots) is shared: core/game/room.h.

// Freelancer's flat list of all UI controls: head at [exe::UiControls], node {+0 prev, +4 next, +8 FlControl*}.
static void ListControls(u32 want, char* out, size_t len)
{
    int n = 0, total = 0;
    size_t used = 0;
    out[0] = 0;
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 5000; ++guard, node = Mem<u32>(node + 4))
        {
            const u32 ctl = Mem<u32>(node + 8);
            ++total;
            // want: 0 = all, 1 = visible only, else a vtable address
            if (!ctl || (want > 1 && Mem<u32>(ctl) != want) || (want == 1 && !(Mem<u8>(ctl + 0x6C) & 1)))
                continue;
            char name[33] = {};
            memcpy(name, reinterpret_cast<const void*>(ctl + 0x18), 32);
            name[32] = 0;
            const u32 vt = Mem<u32>(ctl);
            if (++n <= 120 && used + 160 < len)
                used += sprintf_s(out + used, len - used, "%-26s vt=%08X mouse=%08X msg=%08X state=%02X\n", name, vt,
                                  Mem<u32>(vt + 0x4C), Mem<u32>(vt + 0x5C), Mem<u32>(ctl + 0x6C) & 0xFF);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        if (used + 8 < len)
            used += sprintf_s(out + used, len - used, "fault\n");
    }
    if (used + 64 < len)
        sprintf_s(out + used, len - used, "controls %d, matching %d\n", total, n);
}
// `jointest`: JoinPieces on fixture messages whose joined text follows merge.py render_sentence (the review tool's "In
// game:" preview) and docs\chatter_grammar.tsv (a sentence ends after the last piece and after a "-" piece that a new
// sentence follows; "1 dash 2" -> "1-2"; capital at each sentence start; "." unless ? ! ... end it). The ids are the
// grammar's shapes (a trailing "-" closes the sentence, "gcs_refer_" is a name slot), the texts as the data stores them
// (no sentence-ending period). The two joiners are separate code (C++ here, Python there); a FAIL means they drifted.
// The last fixture is the "to" rule (22 Oct 2026, user: "to" continues the sentence), which merge.py lacked until the
// checkup of 24 Oct 2026 ported it (review commit 477cd32): both sides now give "This is Planet Manhattan to Alpha 1."
static std::string JoinTest()
{
    struct Piece
    {
        const char* id;
        const wchar_t* text;
    };
    struct Fixture
    {
        const char* name;
        std::vector<Piece> parts;
        const wchar_t* expect;
    };
    const std::vector<Fixture> fixtures = {
        {"dock request (06ED6DD0: this is @full to ^base- request todock-)",
         {{"gcs_misc_thisis+", L"this is"},
          {"gcs_refer_full_alpha_1_2", L"Liberty Navy Alpha 1 dash 2"},
          {"gcs_misc_to", L"to"},
          {"gcs_refer_base_manhattan-", L"Planet Manhattan"},
          {"gcs_dockrequest_request_03+", L"i'm requesting"},
          {"gcs_dockrequest_todock-", L"to dock"}},
         L"This is Liberty Navy Alpha 1-2 to Planet Manhattan. I'm requesting to dock."},
        {"dock denied (06ED80D0: ack- yourrequest todock denied- nofit-)",
         {{"gcs_misc_ack_02-", L"roger"},
          {"gcs_dockrequest_yourrequest+", L"your request"},
          {"gcs_dockrequest_todock", L"to dock"},
          {"gcs_dockrequest_denied_01-", L"is denied"},
          {"gcs_dockrequest_nofit_01-", L"your ship is too large"}},
         L"Roger. Your request to dock is denied. Your ship is too large."},
        {"dock delayed, ellipsis and question (ack- standby- idreq-)",
         {{"gcs_misc_ack_02-", L"roger"},
          {"gcs_dockrequest_standby_01-", L"stand by..."},
          {"gcs_dockhail_dock_idreq_01-", L"state your business?"}},
         L"Roger. Stand by... State your business?"},
        {"\"to\" continues the sentence (both joiners since 24 Oct 2026)",
         {{"gcs_misc_thisis+", L"this is"},
          {"gcs_refer_base_manhattan-", L"Planet Manhattan"},
          {"gcs_misc_to", L"to"},
          {"gcs_refer_formationdesig_alpha-", L"Alpha 1"}},
         L"This is Planet Manhattan to Alpha 1."},
    };
    std::string out;
    int failed = 0;
    for (const Fixture& f : fixtures)
    {
        std::vector<data::Line> lines(f.parts.size());
        std::vector<const data::Line*> ptrs;
        for (size_t i = 0; i < f.parts.size(); ++i)
        {
            lines[i].id = f.parts[i].id;
            lines[i].kind = data::Kind::Chunk;
            lines[i].sec = 0;
            lines[i].text = f.parts[i].text;
            ptrs.push_back(&lines[i]);
        }
        const std::wstring got = JoinPieces(ptrs);
        const bool pass = got == f.expect;
        failed += !pass;
        out += std::string(pass ? "PASS " : "FAIL ") + f.name + "\n";
        if (!pass)
            out += "  got:      " + data::Narrow(got) + "\n  expected: " + data::Narrow(f.expect) + "\n";
    }
    return out +
           (failed ? std::to_string(failed) + " FAILED\n" : "all " + std::to_string(fixtures.size()) + " passed\n");
}

static std::string Command(const std::string& line)
{
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    if (!rest.empty() && rest[0] == ' ')
        rest.erase(0, 1);

    if (cmd == "state")
    {
        int w = 0, h = 0;
        render::ScreenSize(w, h);
        std::ostringstream o;
        o << "lines " << data::Count() << "\nframes " << g_frames << "\nscreen " << w << "x" << h << "\nplayer_ship "
          << PlayerShip() << "\nbase " << Mem<u32>(exe::PlayerBase) << "\ntrace " << g_trace << "\nactive:\n"
          << display::ActiveText();
        return o.str();
    }
    if (cmd == "moviesample")
        return movie::SetSampling(rest != "off");
    if (cmd == "config")
    {
        if (rest == "save")
        {
            display::SaveSettings(IniPath());
            return "saved " + IniPath();
        }
        if (rest == "load")
        {
            display::LoadSettings(IniPath());
            movie::ApplyIntro();
        }
        return display::Describe();
    }
    if (cmd == "settings") // every setting with its value and help (the registry)
        return display::SettingsReference();
    if (cmd == "set")
    {
        std::istringstream kv(rest);
        std::string k, v;
        kv >> k >> v;
        if (!display::Set(k, v))
            return "unknown setting " + k;
        size_t n = 0;
        const settings::Def* defs = display::Defs(n);
        movie::OnSettingApplied(settings::Find(defs, n, k)); // (display::Set did the display's own reactions)
        return "ok";
    }
    if (cmd == "trace")
    {
        g_trace = rest != "0";
        return g_trace ? "trace on" : "trace off";
    }
    if (cmd == "layout") // the last frame's subtitle geometry (display modes test)
        return display::LayoutReport();
    if (cmd == "pages") // every shown line's pages: their times (word-timed or estimated) and rows
        return display::PagesReport();
    if (cmd == "perf") // the module's CPU time per frame per entry point since the last `perf` (resets)
        return display::PerfReport();
    if (cmd == "hud2sim") // hud2sim on|off: a synthetic HUD 2 layout (the mock-up's) on the HUD bus; `layout` reports it
        return display::Hud2Sim(rest);
    if (cmd ==
        "arrowtrace") // arrowtrace start | dump: the current-target arrow per frame, game vs drawn (EXE\arrowtrace.tsv)
        return display::ArrowTrace(rest, std::string(g_host->dllDir) + "arrowtrace.tsv");
    if (cmd == "test") // test <relation 0-4> <seconds> <speaker>|<text>
    {
        std::istringstream a(rest);
        int rel = 1;
        float sec = 4;
        a >> rel >> sec;
        std::string tail;
        std::getline(a, tail);
        const size_t bar = tail.find('|');
        std::string who = bar == std::string::npos ? "" : tail.substr(0, bar), what = tail.substr(bar + 1);
        while (!who.empty() && who[0] == ' ')
            who.erase(0, 1);
        // relation 3 (Computer) shows as a Neural Net line (its own column in flight)
        const display::Relation r = static_cast<display::Relation>(rel);
        display::Show(r == display::Relation::Computer ? display::Category::Nn : display::Category::Comms,
                      data::Widen(who), r, data::Widen(what), sec);
        return "shown";
    }
    {
        // click, hotspots, goto, talkers, talk: shared with every module (core/game/room.cpp)
        std::string reply;
        if (room::Command(cmd, rest, &reply))
            return reply;
    }
    if (cmd == "nnsev") // nnsev <id> [id...]: the Neural Net severity of each id (0 info, 1 warning, 2 danger)
    {
        std::istringstream a(rest);
        std::string id, out;
        while (a >> id)
            out += id + "=" + std::to_string(display::NnSeverity(id)) + " ";
        return out;
    }
    if (cmd == "nn") // nn <line id>: queue a Neural Net line through the game's own queue (QueueNN cdecl(msg, 0))
    {
        std::istringstream a(rest);
        std::string id;
        a >> id;
        if (id.empty())
            return "usage: nn <line id>";
        if (!exe::QueueNN.ok())
            return "refused: QueueNN unresolved";
        hk::Call<void(__cdecl*)(u32, u32)>(exe::QueueNN, data::CreateID(id.c_str()), 0u);
        return "queued " + id;
    }
    if (cmd == "comm") // comm <from id> <voice> <line id> [head] [body] [caption ids]: a comm to the player through the
    {                  // game (server.dll pub::SpaceObj::SendComm), with any costume ("-" = 0): the comm window's cases
        std::istringstream a(rest);
        u32 from = 0, caption = 0;
        std::string voice, id, head = "-", body = "-";
        a >> from >> voice >> id >> head >> body >> caption;
        if (id.empty())
            return "usage: comm <from id> <voice> <line id> [head] [body] [caption ids]";
        typedef int(__cdecl * SendCommFn)(u32, u32, u32, const void*, u32, u32*, int, u32, float, bool);
        const auto send = reinterpret_cast<SendCommFn>(
            GetProcAddress(GetModuleHandleA("server.dll"), "?SendComm@SpaceObj@pub@@YAHIIIPBUCostume@@IPAIHIM_N@Z"));
        if (!send || !PlayerShip())
            return "refused: no SendComm export or no player ship";
        // a part: "-" none, 0x... an id as logged by the comm trace (a costume the game has loaded), else a nickname
        auto part = [](const std::string& s)
        {
            return s == "-"                ? 0u
                   : s.rfind("0x", 0) == 0 ? static_cast<u32>(strtoul(s.c_str(), nullptr, 16))
                                           : data::CreateID(s.c_str());
        };
        u32 costume[13] = {}; // head, body, left hand, right hand, accessories[8], accessory count (0x34 bytes)
        costume[0] = part(head);
        costume[1] = part(body);
        u32 lineId = data::CreateID(id.c_str());
        const int r = send(from, PlayerShip(), part(voice), costume, caption, &lineId, 1, 0, 0.0f, false);
        return "sent " + id + " r=" + std::to_string(r);
    }
    if (cmd == "say") // say <line id> [voice] [as <speaker>]: show a line from the data as if it played (another name:
    {                 // the fit tests' long speakers; " . " in it stands for the " · " of "name · faction")
        std::string args = rest, as;
        const size_t at = args.find(" as ");
        if (at != std::string::npos)
            as = args.substr(at + 4), args.resize(at);
        std::istringstream a(args);
        std::string id, voice;
        a >> id >> voice;
        const data::Line* ln =
            data::Find(voice.empty() ? 0 : data::CreateID(voice.c_str()), data::CreateID(id.c_str()));
        if (!ln)
            return "unknown line " + id;
        std::wstring speaker = ln->speaker;
        if (!as.empty())
        {
            speaker.assign(as.begin(), as.end());
            for (size_t p; (p = speaker.find(L" . ")) != std::wstring::npos;)
                speaker[p + 1] = 0x00B7;
        }
        display::Show(display::Category::Comms, speaker, display::Relation::Ally, ln->text, ln->sec, 0, false,
                      ln->wordStarts.empty() ? nullptr : &ln->wordStarts);
        return data::Narrow(speaker) + ": " + data::Narrow(ln->text);
    }
    if (cmd == "grab") // grab <frames> <x> <y> <w> <h> [step] [comm]: record a back-buffer region for the next frames
    {                  // (comm: from the next comm to the player on - the comm portrait's opening)
        std::istringstream a(rest);
        int n = 0, x = 0, y = 0, w = 0, h = 0, step = 1;
        a >> n >> x >> y >> w >> h;
        std::string tail;
        if (!(a >> step))
            step = 1, a.clear();
        a >> tail;
        g_grab.armed = tail == "comm";
        // step = keep every step-th pixel per axis (longer captures at ~150 fps). A 32-bit process: at most 400 MB
        // of frames (a 2 GB request unloaded the logic, 19 Oct 2026)
        if (n <= 0 || n > 999 || w <= 0 || h <= 0 || step < 1 || step > 8)
            return "usage: grab <frames 1-999> <x> <y> <w> <h> [step 1-8] (at most 400 MB of frames)";
        // more than 400 MB: as many frames as fit (a refusal here was a recurring test slip, 19 Oct 2026)
        const double frameBytes = static_cast<double>(w / step) * (h / step) * 4;
        std::string capped;
        if (frameBytes * n > 4.0e8)
        {
            n = std::max(1, static_cast<int>(4.0e8 / frameBytes));
            capped = " (capped at 400 MB)";
        }
        g_grab.frames.clear();
        g_grab.times.clear();
        g_grab.x = x, g_grab.y = y, g_grab.w = w, g_grab.h = h, g_grab.left = n, g_grab.step = step;
        return "grabbing " + std::to_string(n) + " frames" + capped;
    }
    if (cmd == "chat") // chat <name>|<text>
    {
        const size_t bar = rest.find('|');
        chat::Print(data::Widen(bar == std::string::npos ? "" : rest.substr(0, bar)), 0xF2E3B3,
                    data::Widen(rest.substr(bar + 1)));
        return "printed";
    }
    if (cmd == "controls") // controls [vtable hex]: walk Freelancer's flat control list (exe::UiControls)
    {
        static char buf[16384];
        ListControls(rest.empty() ? 0 : static_cast<u32>(strtoul(rest.c_str(), nullptr, 16)), buf, sizeof(buf));
        return buf;
    }
    if (cmd == "shown")
        return feed::Since(static_cast<u32>(strtoul(rest.c_str(), nullptr, 10)));
    if (cmd == "clear")
    {
        display::Clear();
        return "cleared";
    }
    if (cmd == "reloaddata") // subtitles.tsv, word_times.tsv, speaker_colors.tsv, and the bar characters of mbases.ini
    {
        names::ResetBarNpcs(); // (read again at the next bar line)
        return LoadData() ? "loaded " + std::to_string(data::Count()) : "load failed";
    }
    if (cmd == "jointest") // JoinPieces on fixture messages against merge.py's rule: PASS / FAIL each
        return JoinTest();
    if (cmd == "colors") // colors [name]: the speaker palette, each character's colour and its closest neighbours
        return speakers::Report(rest);
    return "commands: state, config [save|load], set <key> <value>, trace 0|1, test <rel> <sec> <name>|<text>, "
           "say <id> [voice] [as <speaker>], chat <name>|<text>, clear, reloaddata (data + mbases.ini), jointest, colors [name]";
}

// ------------------------------------------------------------------ the settings bus (the UI build, 25 Oct 2026)
// core\settings_bus.h (HostApi v6): the [Subtitles] registry (display.cpp kDefs, the movie keys included) registered
// before the ini loads, so the bus's default snapshot = the struct initialisers; the options page (controller logic)
// lists, changes and saves it through the bus. A change applied there gets the reactions `set` has (OnSettingApplied).
static int g_busSection = 0;
static SettingsBusSection g_busDesc;
static void BusApplied(void*, const settings::Def* d)
{
    display::OnSettingApplied(d);
    movie::OnSettingApplied(d); // movie_intro -> the host's intro override (a missing file resets the value to 0)
}
static void BusSave(void*) { display::SaveSettings(IniPath()); }

static void RegisterBus()
{
    if (g_host->version < 6 || !g_host->GetSettingsBus)
        return;
    const SettingsBus* bus = g_host->GetSettingsBus();
    if (!bus)
        return;
    size_t n = 0;
    const settings::Def* defs = display::Defs(n);
    g_busDesc = {"subtitles", "Subtitles", defs, n, nullptr, BusApplied, BusSave, nullptr};
    g_busSection = bus->Register(&g_busDesc);
    Log("settings bus: Subtitles %d (%u rows)", g_busSection, static_cast<unsigned>(n));
}

static void UnregisterBus()
{
    if (g_busSection && g_host->version >= 6 && g_host->GetSettingsBus)
        if (const SettingsBus* bus = g_host->GetSettingsBus())
            bus->Unregister(g_busSection);
    g_busSection = 0;
}

// ------------------------------------------------------------------ entry points
static void ImplInit(const HostApi* host)
{
    g_host = host;
    LogSetSink(host->LogWrite, host->dllDir);
    room::SetCharacterSource(host->version >= 2 ? host->GetRoomCharacters : nullptr);
    Log("Logic DLL init (build %s %s)", __DATE__, __TIME__);
    exe::ResolveAll("subtitles logic");
    RegisterBus(); // before the ini loads: the bus's defaults = the struct initialisers
    display::LoadSettings(IniPath());
    movie::ApplyIntro(); // the ini's movie_intro to the host before the startup movies (the logic loads before them)
    LoadData();
    movie::Load(std::string(host->dllDir) + "FLSubtitles\\movies.tsv");
    display::SeedGlyphs(movie::Text());
    render::SetDevice(host->GetD3DDevice());
}

static void ImplShutdown()
{
    UnregisterBus();
    display::RestoreHud(); // the next logic must find the HUD as the game built it
    render::OnDeviceLost();
    Log("Logic DLL shutdown");
}

// The game back in its menu (no player ship, no base: Quit to main menu, a load, the death dialog's load): the lines
// of the game left behind go at once (release acceptance 25 Oct 2026: the quit game's comm lines stood in the main
// menu over the Options screen). The startup (movies, the first menu) starts in the menu: no transition, nothing to
// clear.
static void TrackMenu()
{
    static bool inMenu = true;
    const bool menu = !PlayerShip() && !Mem<u32>(exe::PlayerBase);
    if (menu && !inMenu)
    {
        display::Clear();
        if (g_trace)
            Log("game -> menu: display cleared");
    }
    inMenu = menu;
}

static void ImplOnFrame(double dt)
{
    ++g_frames;
    TrackMenu();
    TrackBarConversation();
    display::Update(dt);
}

static void ImplCommand(const char* line, char* reply, int replyLen)
{
    std::string r;
    try
    {
        r = Command(line);
    }
    catch (...)
    {
        r = "exception";
    }
    strncpy_s(reply, replyLen, r.c_str(), _TRUNCATE);
}

static void ImplOnMovie(int event, void* hwnd, const char* path)
{
    if (event == 0)
    {
        render::SetDevice(g_host->GetD3DDevice()); // the font atlas needs the device (created before movies)
        movie::OnStart(static_cast<HWND>(hwnd), path);
    }
    else if (event == 1)
        movie::OnUpdate();
    else
        movie::OnStop();
}

static bool g_drawnInUi = false;

static void ImplOnRenderUi()
{
    render::SetDevice(g_host->GetD3DDevice());
    display::Draw(false);
    g_drawnInUi = true;
}

static void GrabWrite()
{
    const std::string dir = std::string(g_host->dllDir) + "grab\\";
    CreateDirectoryA(dir.c_str(), nullptr);
    FILE* t = nullptr;
    fopen_s(&t, (dir + "times.txt").c_str(), "w");
    for (size_t i = 0; i < g_grab.frames.size(); ++i)
    {
        char name[32];
        sprintf_s(name, "f%03zu.bmp", i);
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + name).c_str(), "wb") || !f)
            continue;
        const int ow = g_grab.w / g_grab.step, oh = g_grab.h / g_grab.step;
        const u32 size = static_cast<u32>(ow * oh * 4);
        BITMAPFILEHEADER fh = {0x4D42, 54 + size, 0, 0, 54};
        BITMAPINFOHEADER ih = {40, ow, -oh, 1, 32, BI_RGB, size, 0, 0, 0, 0};
        fwrite(&fh, sizeof(fh), 1, f);
        fwrite(&ih, sizeof(ih), 1, f);
        fwrite(g_grab.frames[i].data(), 4, g_grab.frames[i].size(), f);
        fclose(f);
        if (t)
            fprintf(t, "%zu %.4f\n", i, g_grab.times[i] - g_grab.times[0]);
    }
    if (t)
        fclose(t);
    Log("grab: %zu frames written to %s", g_grab.frames.size(), dir.c_str());
    g_grab.frames.clear();
    g_grab.times.clear();
}

static void ImplOnPresent()
{
    render::SetDevice(g_host->GetD3DDevice());
    display::CapturePortrait();
    if (g_grab.left > 0 && !g_grab.armed)
    {
        render::SetDevice(g_host->GetD3DDevice());
        std::vector<u32> px;
        if (render::GrabRegion(g_grab.x, g_grab.y, g_grab.w, g_grab.h, px))
        {
            LARGE_INTEGER c, fq;
            QueryPerformanceCounter(&c);
            QueryPerformanceFrequency(&fq);
            if (const int s = g_grab.step; s > 1)
            {
                const int ow = g_grab.w / s, oh = g_grab.h / s;
                std::vector<u32> sub(static_cast<size_t>(ow) * oh);
                for (int r = 0; r < oh; ++r)
                    for (int col = 0; col < ow; ++col)
                        sub[static_cast<size_t>(r) * ow + col] = px[static_cast<size_t>(r * s) * g_grab.w + col * s];
                px = std::move(sub);
            }
            g_grab.frames.push_back(std::move(px));
            g_grab.times.push_back(static_cast<double>(c.QuadPart) / fq.QuadPart);
        }
        if (--g_grab.left == 0)
            GrabWrite();
    }
    if (!g_drawnInUi)
    {
        render::SetDevice(g_host->GetD3DDevice());
        display::Draw(true);
    }
    g_drawnInUi = false;
}

static void ImplOnDeviceLost()
{
    render::OnDeviceLost();
    display::OnDeviceLost();
}

static void ImplOnDeviceReset()
{
    render::SetDevice(g_host->GetD3DDevice());
    render::OnDeviceReset();
}

static bool ImplOnHudPart(u32 callerEbp, const void* part, const float* pos, float* out)
{
    return display::ConfineHudArrow(callerEbp, part, pos, out);
}

static const ImplApi g_api = {
    kImplApiVersion, ImplInit,      ImplShutdown,     ImplOnFrame,       ImplCommand,
    ImplOnRenderUi,  ImplOnPresent, ImplOnDeviceLost, ImplOnDeviceReset, OnComm,
    OnVoiceLookup,   OnSoundStart,  ImplOnMovie,      ImplOnHudPart,     display::OnManeuverLayout};

#ifdef FLV_STATIC
// Release build: linked into FLvibed.dll; the host takes the API through this (unique, unexported) name.
extern "C" const void* flv_subtitles_impl_api() { return &g_api; }
#else
extern "C" __declspec(dllexport) const ImplApi* __cdecl GetImplApi() { return &g_api; }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
#endif
