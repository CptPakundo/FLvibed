#include "../events.h"

#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "text.h"

#include <cstdarg>
#include <deque>
#include <vector>

namespace events
{
// ------------------------------------------------------------------ event ring
struct Entry
{
    u32 seq;
    std::string line;
};
static CRITICAL_SECTION g_lock;
static bool g_lockInit = false;
static std::deque<Entry> g_ring;
static u32 g_seq = 0;
// A whole long mission (M02: ~2000 events, mostly comms) must stay readable: the harness maps mission ships from the
// session's spawn events (Get-FLMissionShips); 512 dropped King's launch spawn (18 Oct 2026). ~100 bytes per line.
static const size_t kMaxEvents = 20000;

static void EnsureLock()
{
    if (!g_lockInit)
    {
        InitializeCriticalSection(&g_lock);
        g_lockInit = true;
    }
}

void Push(const char* fmt, ...)
{
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf_s(text, _TRUNCATE, fmt, ap);
    va_end(ap);
    EnsureLock();
    EnterCriticalSection(&g_lock);
    const u32 seq = ++g_seq;
    char line[600];
    sprintf_s(line, "%u %u %s", seq, GetTickCount(), text);
    g_ring.push_back({seq, line});
    if (g_ring.size() > kMaxEvents)
        g_ring.pop_front();
    LeaveCriticalSection(&g_lock);
    Log("event %s", text);
}

std::string Since(u32 after, int maxLen)
{
    EnsureLock();
    EnterCriticalSection(&g_lock);
    std::string out;
    for (const Entry& e : g_ring)
    {
        if (e.seq <= after)
            continue;
        if (static_cast<int>(out.size() + e.line.size() + 1) >= maxLen)
            break;
        out += e.line;
        out += '\n';
    }
    LeaveCriticalSection(&g_lock);
    return out;
}

u32 LastSeq() { return g_seq; }

// ------------------------------------------------------------------ string resources
std::string IdsText(u32 ids)
{
    // One line per event: line breaks of the resource text become spaces.
    std::string s = text::IdsUtf8(ids);
    for (char& c : s)
        if (c == '\n' || c == '\r')
            c = ' ';
    return s;
}

static u32 Word(const void* base, int offset)
{
    u32 v = 0;
    __try
    {
        v = *reinterpret_cast<const u32*>(static_cast<const u8*>(base) + offset);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return v;
}

static u32 StrId(const void* fmtStr)
{
    // FmtStr: +0 ?, +4 strid (resource id of the text)
    u32 id = 0;
    __try
    {
        id = static_cast<const u32*>(fmtStr)[1];
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return id;
}

// A hook argument's fields copied out under a guard (a null or short struct from a mod faults here, not in the hook):
// false = no data, and the hook goes on as if none had been passed.
static bool CopyBytes(void* dst, const void* src, size_t len)
{
    __try
    {
        memcpy(dst, src, len);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string Quote(u32 ids)
{
    std::string t = IdsText(ids);
    if (t.size() > 160)
        t = t.substr(0, 157) + "...";
    return "\"" + t + "\"";
}

// ------------------------------------------------------------------ Content.dll hooks (server.dll pub:: API)
typedef int(__cdecl* SetObjectivesFn)(const u32&, const u32&, const void*, u32, const void*, u8, const void*);
typedef int(__cdecl* SetObjectiveStateFn)(const u32&, const u32&, int, u32);
typedef int(__cdecl* PopUpFn)(u32, const void*, const void*, u32);
typedef int(__cdecl* MissionMsgFn)(const u32&, const void*, int, bool);
typedef int(__cdecl* NNMessageFn)(u32, u32);
typedef int(__cdecl* SendCommFn)(u32, u32, u32, const void*, u32, u32*, int, u32, float, bool);

static SetObjectivesFn g_origSetObjectives;
static SetObjectiveStateFn g_origSetObjectiveState;
static PopUpFn g_origPopUp;
static MissionMsgFn g_origMissionMsg;
static NNMessageFn g_origNNMessage;
static SendCommFn g_origSendComm;
typedef int(__cdecl* AdjustCashFn)(const u32&, int);
typedef int(__cdecl* SetRankFn)(u32, int);
static AdjustCashFn g_origAdjustCash;
static SetRankFn g_origSetRank;
// pub::Save(client, ids): a mission's Act_Save checkpoint (the game writes a save file described by `ids`).
typedef int(__cdecl* SaveFn)(u32, u32);
static SaveFn g_origSave;

static int __cdecl SaveHook(u32 client, u32 ids)
{
    const int r = g_origSave(client, ids);
    Push("mission-save ids=%u result=%d %s", ids, r, Quote(ids).c_str());
    return r;
}

typedef u32(__cdecl* OfferCreateFn)(const void*);
static OfferCreateFn g_origOfferCreate;

// Job board offers (pub::GF::MissionVendorOfferCreate, from Content.dll, when a base loads). MissionVendorOfferDescription
// as mapped against the board (18 Oct 2026, Manhattan, 12 offers; the board lists them in key order): +0 base hash, +4
// index, +8 ?, +0xC job type (1 Kill Ships, 3 portrait icon, 6 cargo icon; others unconfirmed), +0x10.. text
// arguments, +0x58 reward in credits. Difficulty is not a plain field (it is in the text arguments).
static u32 __cdecl OfferCreateHook(const void* d)
{
    const u32 key = g_origOfferCreate(d);
    Push("offer key=%u base=%u type=%u reward=%u", key, Word(d, 0), Word(d, 0xC), Word(d, 0x58));
    return key;
}

// The story's money target (Content.dll's story object, when a mission ends and free time starts: net worth then +
// the free time's amount - StoryLvl-style mods change the amounts; the call carries the game's own number).
typedef int(__cdecl* MoneyNeededFn)(u32, int);
static MoneyNeededFn g_origMoneyNeeded;
static int __cdecl MoneyNeededHook(u32 client, int amount)
{
    Push("worth-target %d", amount);
    return g_origMoneyNeeded(client, amount);
}

typedef void(__cdecl* OfferDestroyFn)(u32*);
static OfferDestroyFn g_origOfferDestroy;

// An offer leaves the board (accepted, or the base's list renewed).
static void __cdecl OfferDestroyHook(u32* key)
{
    Push("offer-gone key=%u", key ? Word(key, 0) : 0);
    g_origOfferDestroy(key);
}

// Money a mission pays and the rank it sets (marks: HookClientMark below).

static int __cdecl AdjustCashHook(const u32& client, int amount)
{
    Push("cash %d", amount);
    return g_origAdjustCash(client, amount);
}

static int __cdecl SetRankHook(u32 client, int rank)
{
    Push("rank %d", rank);
    return g_origSetRank(client, rank);
}

// The objects the mission points the player at (host API v10 MissionObjects): marked ones (newest last) and the current
// objective's 'R' targets. Written by the hooks below, read by the logic (any thread: a lock).
static SRWLOCK g_missionLock = SRWLOCK_INIT;
static std::map<u32, ULONGLONG> g_commHeard; // comm line id -> when last sent (CommHeardAt; under g_missionLock)
static std::deque<SpawnRec> g_spawned; // ships spawned this session, the last 2048 (SpawnedShips; under g_missionLock)
static std::vector<u32> g_marked, g_objectiveTargets;

int MissionObjects(u32* out, int max)
{
    AcquireSRWLockShared(&g_missionLock);
    int n = 0;
    for (auto it = g_marked.rbegin(); it != g_marked.rend() && n < max; ++it)
        out[n++] = *it;
    for (u32 id : g_objectiveTargets)
        if (n < max && std::find(out, out + n, id) == out + n)
            out[n++] = id;
    ReleaseSRWLockShared(&g_missionLock);
    return n;
}

static int CopyOut(const std::vector<u32>& v, u32* out, int max, bool newestFirst)
{
    AcquireSRWLockShared(&g_missionLock);
    int n = 0;
    for (size_t i = 0; i < v.size() && n < max; ++i)
        out[n++] = newestFirst ? v[v.size() - 1 - i] : v[i];
    ReleaseSRWLockShared(&g_missionLock);
    return n;
}

int MissionMarks(u32* out, int max) { return CopyOut(g_marked, out, max, true); }
int ObjectiveRefs(u32* out, int max) { return CopyOut(g_objectiveTargets, out, max, false); }

// The current step (entry 0 of the last SetObjectives): its text id, its 'R' values, its navmarker.
static u32 g_objSeq = 0, g_objText = 0, g_objNavSystem = 0;
static u32 g_objTitle = 0; // the mission's title string id with the last SetObjectives (22602 "ESCAPE FROM LIBERTY")
static u32 g_resultSeq = 0, g_resultType = 0; // v20 MissionResult (MissionMsgHook below)
static float g_objNav[3] = {};
static bool g_objHasNav = false;
static std::vector<u32> g_objRefs0;
static std::vector<ObjectiveStep> g_objSteps; // every entry (v15 ObjectiveSteps)
static std::wstring g_objFormatted;           // entry 0 as the game formats it (v24 ObjectiveText)

// An objective entry's text as the game shows it: the entry's FmtStr at +4 ({+0 the Val chain, +4 the string id}: all
// exe::FmtStrFormat reads, disassembly 25 Oct 2026) through the game's own formatter - the values filled in, a code
// without a handler (vanilla 1.1's "Destroy the Hostile Ships%M") dropped as the game drops it. Read inside the hook:
// the chain is freed after the call. "" when the formatter is unavailable (signature) or faults.
static void FormatEntryRaw(const void* fmtStr, wchar_t* buf, int max)
{
    buf[0] = 0;
    __try
    {
        hk::Call<int(__fastcall*)(const void*, void*, wchar_t*, int)>(exe::FmtStrFormat, fmtStr, nullptr, buf, max);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        buf[0] = 0;
    }
    buf[max - 1] = 0;
}

static bool Blank(const wchar_t* s)
{
    for (; s && *s; ++s)
        if (!iswspace(*s))
            return false;
    return true;
}

// The objective line the game's banner shows: the first entry whose formatted text is not blank. A job board mission's
// entry 0 is a blank step (type 9, text 1 " ") before "Find your target, %s0 %s1, by the mission waypoint." (type 58),
// and the banner shows the second (Assassinate, HDE, 26 Oct 2026); a story step's entry 0 is its text, unchanged.
// entryAt(i): entry i's FmtStr ({+0 the Val chain, +4 the string id}) or null past the last.
template <class EntryAt> static std::wstring FormatFirstShown(EntryAt entryAt, int count)
{
    if (!exe::FmtStrFormat.ok())
        return {};
    wchar_t buf[512];
    for (int i = 0; i < count && i < 8; ++i)
    {
        const void* fmt = entryAt(i);
        if (!fmt)
            break;
        FormatEntryRaw(fmt, buf, 512);
        if (!Blank(buf))
            return buf;
    }
    return {};
}

// A FmtStr argument as the game formats it (a job's description 327681 is "%!0%!1...%!23", its parts filled in by the
// formatter); the plain resource text when the formatter is unavailable or gives nothing.
static std::string QuoteFmt(const void* fmtStr, u32 ids)
{
    if (fmtStr && ids && exe::FmtStrFormat.ok())
    {
        wchar_t buf[512];
        FormatEntryRaw(fmtStr, buf, 512);
        if (!Blank(buf))
        {
            std::string t = text::Utf8(buf);
            for (char& c : t)
                if (c == '\n' || c == '\r' || c == '"')
                    c = c == '"' ? '\'' : ' ';
            if (t.size() > 160)
                t = t.substr(0, 157) + "...";
            return "\"" + t + "\"";
        }
    }
    return Quote(ids);
}

u32 ObjectiveText(wchar_t* out, int max)
{
    AcquireSRWLockShared(&g_missionLock);
    if (out && max > 0)
        wcsncpy_s(out, max, g_objFormatted.c_str(), _TRUNCATE);
    const u32 seq = g_objSeq;
    ReleaseSRWLockShared(&g_missionLock);
    return seq;
}

int ObjectiveSteps(ObjectiveStep* out, int max)
{
    AcquireSRWLockShared(&g_missionLock);
    int n = 0;
    for (; out && n < max && n < static_cast<int>(g_objSteps.size()); ++n)
        out[n] = g_objSteps[n];
    if (!out)
        n = static_cast<int>(g_objSteps.size());
    ReleaseSRWLockShared(&g_missionLock);
    return n;
}

u32 ObjectiveNow(ObjectiveStep* out)
{
    AcquireSRWLockShared(&g_missionLock);
    if (out)
    {
        *out = ObjectiveStep{};
        out->textId = g_objText;
        out->hasNav = g_objHasNav;
        memcpy(out->nav, g_objNav, sizeof(out->nav));
        out->navSystem = g_objNavSystem;
        for (size_t i = 0; i < g_objRefs0.size() && i < 4; ++i)
            out->refs[out->refCount++] = g_objRefs0[i];
    }
    const u32 seq = g_objSeq;
    ReleaseSRWLockShared(&g_missionLock);
    return seq;
}

u32 ObjectiveTitle()
{
    AcquireSRWLockShared(&g_missionLock);
    const u32 t = g_objTitle;
    ReleaseSRWLockShared(&g_missionLock);
    return t;
}

// v25: the objective the game itself holds - its client copy (exe::ClientObjective, what the NeuroNet and the banner
// show). A save load restores a mission's objective there without any SetMissionObjectives call (a mission save loaded
// after a free-roam start, 25 Oct 2026: no objective event, HUD 2's headline stayed empty). Entry i: +0 the FmtStr
// {+0 the Val chain, +4 the string id}. Taken over only when it differs from the hooked copy (then a new sequence);
// the navmarker is not read from the client copy (its 'N' value there is unverified): hasNav stays as it was cleared.
static const void* ClientEntry(int i)
{
    const void* e = nullptr;
    __try
    {
        e = hk::Call<const void*(__cdecl*)(int)>(exe::ClientObjective, i);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        e = nullptr;
    }
    return e;
}

u32 ObjectiveReread()
{
    if (!exe::ClientObjective.ok())
        return ObjectiveNow(nullptr);
    std::vector<u32> targets, refs0;
    std::vector<ObjectiveStep> steps;
    for (int i = 0; i < 8; ++i)
    {
        const void* e = ClientEntry(i);
        if (!e)
            break;
        steps.push_back(ObjectiveStep{});
        ObjectiveStep& es = steps.back();
        es.textId = Word(e, 4);
        const void* p = reinterpret_cast<const void*>(Word(e, 0));
        for (int k = 0; p && k < 16; ++k, p = reinterpret_cast<const void*>(Word(p, 0)))
        {
            const u32 v = Word(p, 4), kind = Word(p, 8) & 0xFFFF;
            if (kind == 'R' && v)
            {
                targets.push_back(v);
                if (i == 0)
                    refs0.push_back(v);
                if (es.refCount < 4)
                    es.refs[es.refCount++] = v;
            }
        }
    }
    const u32 text = steps.empty() ? 0 : steps[0].textId;
    std::wstring formatted;
    if (text)
        formatted = FormatFirstShown([](int i) { return ClientEntry(i); }, static_cast<int>(steps.size()));
    AcquireSRWLockExclusive(&g_missionLock);
    const bool changed = text != g_objText || (text && formatted != g_objFormatted);
    if (changed)
    {
        g_objFormatted = formatted;
        g_objectiveTargets = targets;
        g_objRefs0 = refs0;
        g_objSteps = steps;
        g_objText = text;
        g_objHasNav = false;
        g_objNavSystem = 0;
        memset(g_objNav, 0, sizeof(g_objNav));
        ++g_objSeq;
    }
    const u32 seq = g_objSeq;
    ReleaseSRWLockExclusive(&g_missionLock);
    if (changed)
        Push("objective-reread count=%u obj=%u %s", static_cast<u32>(steps.size()), text, Quote(text).c_str());
    return seq;
}

// A navmarker value with a position: M01a's "Dock with the Trade Lane" (21875) carries an 'N' at 0,0,0 beside its
// object (TL146) - no marker; read as one it put the picker's MISSION WAYPOINT on the system's origin, the sun (the
// owner, 26 Oct 2026).
static bool NavSet(u32 v)
{
    for (int i = 0; i < 3; ++i)
        if (Word(reinterpret_cast<const void*>(v), 8 + i * 4) & 0x7FFFFFFF) // (+-0.0f is no position)
            return true;
    return false;
}

static int __cdecl SetObjectivesHook(const u32& client, const u32& a, const void* objs, u32 count, const void* title,
                                     u8 b, const void* text)
{
    {
        std::vector<u32> targets, refs0;
        std::vector<ObjectiveStep> steps;
        bool hasNav = false;
        float nav[3] = {};
        u32 navSystem = 0;
        for (u32 e = 0; objs && e < count && e < 8; ++e)
        {
            steps.push_back(ObjectiveStep{});
            ObjectiveStep& es = steps.back();
            es.textId = Word(objs, e * 28 + 8);
            for (const void* p = reinterpret_cast<const void*>(Word(objs, e * 28 + 4)); p;
                 p = reinterpret_cast<const void*>(Word(p, 0)))
            {
                const u32 v = Word(p, 4), kind = Word(p, 8) & 0xFFFF;
                if (kind == 'R' && v && es.refCount < 4)
                    es.refs[es.refCount++] = v;
                else if (kind == 'N' && v && !es.hasNav && NavSet(v))
                {
                    es.hasNav = true;
                    es.navSystem = Word(reinterpret_cast<const void*>(v), 0);
                    for (int i = 0; i < 3; ++i)
                    {
                        const u32 w = Word(reinterpret_cast<const void*>(v), 8 + i * 4);
                        memcpy(&es.nav[i], &w, 4);
                    }
                }
            }
        }
        for (u32 e = 0; objs && e < count && e < 8; ++e)
            for (const void* p = reinterpret_cast<const void*>(Word(objs, e * 28 + 4)); p;
                 p = reinterpret_cast<const void*>(Word(p, 0)))
            {
                const u32 v = Word(p, 4), kind = Word(p, 8) & 0xFFFF;
                if (kind == 'R' && v)
                {
                    targets.push_back(v);
                    if (e == 0)
                        refs0.push_back(v);
                }
                else if (kind == 'N' && v && e == 0 && !hasNav && NavSet(v))
                {
                    hasNav = true;
                    navSystem = Word(reinterpret_cast<const void*>(v), 0);
                    for (int i = 0; i < 3; ++i)
                    {
                        const u32 w = Word(reinterpret_cast<const void*>(v), 8 + i * 4);
                        memcpy(&nav[i], &w, 4);
                    }
                }
            }
        const std::wstring formatted =
            (objs && count) ? FormatFirstShown([objs](int i) -> const void* { return static_cast<const u8*>(objs) + i * 28 + 4; },
                                               static_cast<int>(count))
                            : std::wstring();
        AcquireSRWLockExclusive(&g_missionLock);
        g_objFormatted = formatted;
        g_objectiveTargets = targets;
        g_objRefs0 = refs0;
        g_objSteps = steps;
        g_objText = (objs && count) ? Word(objs, 8) : 0;
        g_objTitle = StrId(title);
        g_objHasNav = hasNav;
        memcpy(g_objNav, nav, sizeof(nav));
        g_objNavSystem = navSystem;
        ++g_objSeq;
        ReleaseSRWLockExclusive(&g_missionLock);
    }
    // title / text: the mission's (the NN log header, e.g. 22000 "MISSION 1"); obj: the step objective - the string
    // id at +8 of the first pub::Player::MissionObjective entry (found in game 18 Oct 2026, M01b: 22065, 21660, 22010
    // "Enter Formation with Epsilon 1", 22020; the struct is not documented).
    // Entries are 28 bytes: +0 type, +4 the argument chain, +8 the text id. The chain's nodes are {+0 next, +4 value,
    // +8 u16 kind}: 'S' system hash, 'R' a space object (the HUD marks it: a job's trade lane ring, a story's
    // rep_inst), 'N' a navmarker -> {+0 system, +4 ?, +8 x, +0xC y, +0x10 z} (the "Mission Waypoint"; a job sets no
    // waypoint list, only these), 'F' a number. Read at the call: the chain is freed after it (found 18 Oct 2026, job
    // board Kill Ships: entry 0 = R the lane ring + N beside it; the last entry = N where the enemies spawn).
    const u32 t = StrId(title), x = StrId(text), o = (objs && count) ? Word(objs, 8) : 0;
    char where[8][96] = {};
    for (u32 e = 0; objs && e < count && e < 8; ++e)
    {
        int n = 0;
        const void* p = reinterpret_cast<const void*>(Word(objs, e * 28 + 4));
        for (int k = 0; p && k < 8; ++k)
        {
            const u32 v = Word(p, 4), kind = Word(p, 8) & 0xFFFF;
            if (kind == 'R' && v)
                n += sprintf_s(where[e] + n, sizeof(where[e]) - n, " target=%u", v);
            else if (kind == 'N' && v)
            {
                float f[3];
                for (int i = 0; i < 3; ++i)
                {
                    const u32 w = Word(reinterpret_cast<const void*>(v), 8 + i * 4);
                    memcpy(&f[i], &w, 4);
                }
                n += sprintf_s(where[e] + n, sizeof(where[e]) - n, " nav=%.0f,%.0f,%.0f system=%u", f[0], f[1], f[2],
                               Word(reinterpret_cast<const void*>(v), 0));
            }
            p = reinterpret_cast<const void*>(Word(p, 0));
        }
    }
    Push("objective count=%u title=%u %s text=%u %s obj=%u %s%s", count, t, Quote(t).c_str(), x, QuoteFmt(text, x).c_str(), o,
         Quote(o).c_str(), where[0]);
    for (u32 e = 1; objs && e < count && e < 8; ++e)
    {
        const u32 s = Word(objs, e * 28 + 8);
        Push("objective-step %u type=%u obj=%u %s%s", e, Word(objs, e * 28), s, Quote(s).c_str(), where[e]);
    }
    return g_origSetObjectives(client, a, objs, count, title, b, text);
}

static int __cdecl SetObjectiveStateHook(const u32& client, const u32& id, int state, u32 d)
{
    Push("objective-state id=%u state=%d", id, state);
    return g_origSetObjectiveState(client, id, state, d);
}

static int __cdecl PopUpHook(u32 client, const void* caption, const void* msg, u32 buttons)
{
    const u32 c = StrId(caption), m = StrId(msg);
    Push("popup caption=%u %s text=%u buttons=0x%X", c, Quote(c).c_str(), m, buttons);
    return g_origPopUp(client, caption, msg, buttons);
}

static int __cdecl MissionMsgHook(const u32& client, const void* text, int type, bool b)
{
    // pub::Player::MissionMessageType, from game data (18 Oct 2026, M01b): 0 = failure ("Mission Failed. ..."), 1 =
    // success (1231 "Mission Result: Success"); others (e.g. "GAME SAVED", personal log entries) by number until known.
    // Job board missions report their result with other types (type 2 = 13093 "Mission Result: Failure - You ran from
    // the battle", 18 Oct 2026): a "Mission Result" text is named by its own words.
    const u32 t = StrId(text);
    char name[16];
    const std::string words = IdsText(t);
    // A job's success is type 3 ("You have made <faction> very pleased.", 18 Oct 2026).
    if (type == 0 || type == 1 || type == 3)
        strcpy_s(name, type == 0 ? "FAILURE" : "SUCCESS");
    else if (words.find("Mission Result") != std::string::npos && words.find("Failure") != std::string::npos)
        strcpy_s(name, "FAILURE");
    else if (words.find("Mission Result") != std::string::npos && words.find("Success") != std::string::npos)
        strcpy_s(name, "SUCCESS");
    else
        sprintf_s(name, "%d", type);
    Push("mission-message type=%s text=%u %s", name, t, Quote(t).c_str());
    if (name[0] == 'S' || name[0] == 'F') // v20 MissionResult: the same naming, as a sequence for the logic
    {
        AcquireSRWLockExclusive(&g_missionLock);
        ++g_resultSeq;
        g_resultType = name[0] == 'S' ? 1 : 2;
        ReleaseSRWLockExclusive(&g_missionLock);
    }
    return g_origMissionMsg(client, text, type, b);
}

u32 MissionResult(u32* type)
{
    AcquireSRWLockShared(&g_missionLock);
    const u32 seq = g_resultSeq;
    if (type)
        *type = g_resultType;
    ReleaseSRWLockShared(&g_missionLock);
    return seq;
}

static int __cdecl NNMessageHook(u32 client, u32 ids)
{
    Push("nn-message %u %s", ids, Quote(ids).c_str());
    return g_origNNMessage(client, ids);
}

static int __cdecl SendCommHook(u32 from, u32 to, u32 voice, const void* costume, u32 infocard, u32* lines, int count,
                                u32 infocard2, float delay, bool global)
{
    // Every line id (the harness times the dialogue from the clips' lengths) and the send delay. The first 16 ids are
    // copied once under a guard: a faulting `lines` reads as none.
    u32 line[16];
    int n = lines && count > 0 ? std::min(count, 16) : 0;
    if (n > 0 && !CopyBytes(line, lines, static_cast<size_t>(n) * sizeof(u32)))
        n = 0;
    char ids[256] = {};
    for (int i = 0, k = 0; i < n && k < static_cast<int>(sizeof(ids)) - 12; ++i)
        k += sprintf_s(ids + k, sizeof(ids) - k, i ? ",%08X" : "%08X", line[i]);
    Push("comm from=%u to=%u lines=%d first=0x%08X ids=%s delay=%.2f", from, to, count, n > 0 ? line[0] : 0, ids,
         delay);
    {
        const ULONGLONG now = GetTickCount64();
        AcquireSRWLockExclusive(&g_missionLock);
        for (int i = 0; i < n; ++i)
            if (g_commHeard.size() < 8192 || g_commHeard.count(line[i]))
                g_commHeard[line[i]] = now;
        ReleaseSRWLockExclusive(&g_missionLock);
    }
    return g_origSendComm(from, to, voice, costume, infocard, lines, count, infocard2, delay, global);
}

int SpawnedShips(SpawnRec* out, int max)
{
    if (!out || max <= 0)
        return 0;
    AcquireSRWLockShared(&g_missionLock);
    const size_t n = std::min(g_spawned.size(), static_cast<size_t>(max));
    const size_t from = g_spawned.size() - n;
    for (size_t i = 0; i < n; ++i)
        out[i] = g_spawned[from + i];
    ReleaseSRWLockShared(&g_missionLock);
    return static_cast<int>(n);
}

unsigned long long CommHeardAt(u32 line)
{
    AcquireSRWLockShared(&g_missionLock);
    const auto it = g_commHeard.find(line);
    const ULONGLONG at = it == g_commHeard.end() ? 0 : it->second;
    ReleaseSRWLockShared(&g_missionLock);
    return at;
}

// Objects Content.dll spawns, as `spawn-<kind>` events: ships (pub::SpaceObj::Create; ShipInfo archetype +8,
// position +0xC), loot, solars. Content.dll spawns every NPC in single player, the random encounters as well as the
// mission script's ships, through one spawn helper and one caller of it (verified 17 Oct 2026: M01a's King, Donau and
// Order formation came the same way as the traffic): nothing in the call tells a mission ship apart. The mission
// script's ships spawn exactly at the mission ini's [MsnShip] / [MsnFormation] position, so the harness maps the
// events' ids to the script's nicknames and labels (Get-FLMissionShips).
typedef int(__cdecl* CreateObjFn)(u32& id, const void* info);
static CreateObjFn g_origCreateShip = nullptr, g_origCreateLoot = nullptr, g_origCreateSolar = nullptr;

static void NoteSpawn(const char* kind, int rc, u32 id, const void* info, bool ship)
{
    if (rc != 0 || !id)
        return;
    u8 raw[0x18]; // ShipInfo: archetype +8, position +0xC (copied under a guard: a short struct reads as no info)
    if (ship && info && CopyBytes(raw, info, sizeof(raw)))
    {
        u32 arch;
        float pos[3];
        memcpy(&arch, raw + 8, sizeof(arch));
        memcpy(pos, raw + 0xC, sizeof(pos));
        Push("spawn-%s id=%u arch=0x%08X pos=%.0f,%.0f,%.0f", kind, id, arch, pos[0], pos[1], pos[2]);
        const SpawnRec r = {id, arch, {pos[0], pos[1], pos[2]}};
        AcquireSRWLockExclusive(&g_missionLock);
        g_spawned.push_back(r);
        if (g_spawned.size() > 2048)
            g_spawned.pop_front();
        ReleaseSRWLockExclusive(&g_missionLock);
    }
    else
        Push("spawn-%s id=%u", kind, id);
}

static int __cdecl CreateShipHook(u32& id, const void* info)
{
    const int rc = g_origCreateShip(id, info);
    NoteSpawn("ship", rc, id, info, true);
    return rc;
}

static int __cdecl CreateLootHook(u32& id, const void* info)
{
    const int rc = g_origCreateLoot(id, info);
    NoteSpawn("loot", rc, id, info, false);
    return rc;
}

static int __cdecl CreateSolarHook(u32& id, const void* info)
{
    const int rc = g_origCreateSolar(id, info);
    NoteSpawn("solar", rc, id, info, false);
    return rc;
}

// Every object the server marks for the player, whoever marks it: a job board mission marks its trade lane / targets
// without calling pub::Player::MarkObj through Content.dll's imports (18 Oct 2026). Server.dll's MarkObj forwards to
// the client interface (FLHookSDK IClient: slot 75 Send_FLPACKET_SERVER_MARKOBJ(client, obj, state)); the client object
// is Server.dll's global read by that call (`mov ecx,[client]; ...; call [edx+12Ch]`, unique in HDE 0.7.1 and the
// current HDE). Its vtable slot is patched (chained) once the object exists.
typedef bool(__fastcall* ClientMarkFn)(void* self, void* edx, u32 client, u32 obj, u32 state);
static ClientMarkFn g_origClientMark = nullptr;

static bool __fastcall ClientMarkHook(void* self, void* edx, u32 client, u32 obj, u32 state)
{
    Push("mark id=%u state=%u", obj, state);
    AcquireSRWLockExclusive(&g_missionLock);
    g_marked.erase(std::remove(g_marked.begin(), g_marked.end(), obj), g_marked.end());
    if (state && g_marked.size() < 64)
        g_marked.push_back(obj);
    ReleaseSRWLockExclusive(&g_missionLock);
    return g_origClientMark(self, edx, client, obj, state);
}

// The server's answer to a player request (FLHookSDK IClient slot 83 Send_FLPACKET_SERVER_REQUEST_RETURNED(client,
// ship, kind, response, x); in the exe the slot leads to the ship's reply handler 0x546D00, which voices it: kind 0
// response 3 = "dock in use - entering queue" (waiting_to_dock), 4 granted; docs\FREELANCER_RE.md "Dock replies").
// Same client object and patch as the mark hook.
typedef bool(__fastcall* ClientReplyFn)(void* self, void* edx, u32 client, u32 ship, u32 kind, u32 response, u32 x);
static ClientReplyFn g_origClientReply = nullptr;
static u32 g_replySeq = 0, g_replyShip = 0, g_replyKind = 0, g_replyResponse = 0;

static bool __fastcall ClientReplyHook(void* self, void* edx, u32 client, u32 ship, u32 kind, u32 response, u32 x)
{
    Push("request-reply ship=%u kind=%u response=%u x=%u", ship, kind, response, x);
    AcquireSRWLockExclusive(&g_missionLock);
    g_replyShip = ship, g_replyKind = kind, g_replyResponse = response;
    ++g_replySeq;
    ReleaseSRWLockExclusive(&g_missionLock);
    return g_origClientReply(self, edx, client, ship, kind, response, x);
}

u32 DockReply(u32* ship, u32* kind, u32* response)
{
    AcquireSRWLockShared(&g_missionLock);
    if (ship)
        *ship = g_replyShip;
    if (kind)
        *kind = g_replyKind;
    if (response)
        *response = g_replyResponse;
    const u32 seq = g_replySeq;
    ReleaseSRWLockShared(&g_missionLock);
    return seq;
}

// The client interface's vtable lives in a module the game reloads with every save load started inside a flight
// (rpclocal.dll in single player: the hooks were installed again after each such load, and the load's own marks - M03's
// convoy - went out before that: HUD 2's protected-ship bar missing after SaveC705 -> SaveS008, 26 Oct 2026). The
// module and the vtable's RVA are kept at the first patch; a DLL load notification patches the vtable again the moment
// that module is mapped again, before the game runs it.
static wchar_t g_vtModule[64] = {};
static uintptr_t g_vtRva = 0;
static uintptr_t g_clientGlobal = 0;
static bool g_clientSearched = false;

static void PatchClientVtable(void** vt)
{
    if (vt[83] != reinterpret_cast<void*>(ClientReplyHook))
    {
        g_origClientReply =
            reinterpret_cast<ClientReplyFn>(hk::PatchVtable(vt, 83, reinterpret_cast<void*>(ClientReplyHook)));
        Log("events: client request-reply hook installed");
    }
    if (vt[75] == reinterpret_cast<void*>(ClientMarkHook))
        return;
    g_origClientMark = reinterpret_cast<ClientMarkFn>(hk::PatchVtable(vt, 75, reinterpret_cast<void*>(ClientMarkHook)));
    Log("events: client mark hook installed");
}

struct LdrUStr
{
    USHORT length, maxLength;
    const wchar_t* buffer;
};
struct LdrNotifyData
{
    ULONG flags;
    const LdrUStr* fullName;
    const LdrUStr* baseName;
    void* base;
    ULONG size;
};

static VOID CALLBACK ClientModuleLoaded(ULONG reason, const LdrNotifyData* d, void*)
{
    if (reason != 1 || !d || !d->baseName || !d->baseName->buffer || !g_vtRva || !g_vtModule[0]) // 1 = loaded
        return;
    const size_t n = d->baseName->length / sizeof(wchar_t);
    if (n != wcslen(g_vtModule) || _wcsnicmp(d->baseName->buffer, g_vtModule, n) != 0)
        return;
    __try
    {
        const uintptr_t base = reinterpret_cast<uintptr_t>(d->base), end = base + d->size;
        void** vt = reinterpret_cast<void**>(base + g_vtRva);
        // a vtable as the first patch saw it: both slots point into the module's own code (else a different build)
        const auto inside = [&](void* p) { return reinterpret_cast<uintptr_t>(p) >= base && reinterpret_cast<uintptr_t>(p) < end; };
        if (g_vtRva + 84 * sizeof(void*) <= d->size && inside(vt[75]) && inside(vt[83]))
        {
            PatchClientVtable(vt);
            Log("events: %ls loaded again - client hooks re-applied at once", g_vtModule);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void WatchClientModule(void** vt)
{
    HMODULE mod = nullptr;
    wchar_t path[MAX_PATH] = {};
    if (g_vtRva || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       reinterpret_cast<LPCWSTR>(vt), &mod) ||
        !GetModuleFileNameW(mod, path, MAX_PATH))
        return;
    const wchar_t* slash = wcsrchr(path, L'\\');
    wcsncpy_s(g_vtModule, slash ? slash + 1 : path, _TRUNCATE);
    g_vtRva = reinterpret_cast<uintptr_t>(vt) - reinterpret_cast<uintptr_t>(mod);
    typedef LONG(NTAPI * RegisterFn)(ULONG, void*, void*, void**);
    static void* cookie = nullptr;
    if (auto reg = reinterpret_cast<RegisterFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification")))
        Log("events: client vtable in %ls +0x%X; reload watch %s", g_vtModule, static_cast<unsigned>(g_vtRva),
            reg(0, reinterpret_cast<void*>(ClientModuleLoaded), nullptr, &cookie) == 0 ? "on" : "FAILED");
}

static void HookClientMark()
{
    uintptr_t& global = g_clientGlobal;
    bool& searched = g_clientSearched;
    if (!searched)
    {
        searched = true;
        global = hk::FindModuleData("server.dll", "8B 0D ?? ?? ?? ?? 8B 11 56 50 8D 44 24 18 50 FF 92 2C 01 00 00", 2);
        if (!global)
            Log("events: server client-interface call not found (mark events off)");
    }
    if (!global)
        return;
    void* client = nullptr;
    __try
    {
        client = *reinterpret_cast<void**>(global);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    if (!client)
        return;
    void** vt = *static_cast<void***>(client);
    // Fixed slot numbers from the FLHookSDK layout, not from a signature: research\repos\FLHookSDK\include\FLCore\
    // FLCoreRemoteClient.h, class IClient - the 76th / 84th virtual (0-based 75 Send_FLPACKET_SERVER_MARKOBJ, 83
    // Send_FLPACKET_SERVER_REQUEST_RETURNED; recounted 24 Oct 2026). A Server.dll whose IClient differs would get a
    // random method patched; only the "already ours" compares guard the re-patch.
    PatchClientVtable(vt);
    WatchClientModule(vt);
}

// Player actions as the server receives them (single player: the client calls Server.dll's IServerImpl through its
// IServer vtable). The slot is the one holding the method's own export - no slot numbers, any build. Patched once
// (chained: the previous pointer is called). Returns the original, null when not found.
static void* HookServerMethod(const char* methodExport, void* hook)
{
    HMODULE srv = GetModuleHandleA("server.dll");
    if (!srv)
        return nullptr;
    void** vt = reinterpret_cast<void**>(GetProcAddress(srv, "??_7IServerImpl@@6BIServer@@@"));
    void* fn = reinterpret_cast<void*>(GetProcAddress(srv, methodExport));
    if (!vt || !fn)
        return nullptr;
    for (int i = 0; i < 400; ++i)
    {
        void* slot = nullptr;
        __try
        {
            slot = vt[i];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
        if (slot == hook)
            return nullptr; // already ours
        if (slot == fn)
            return hk::PatchVtable(vt, i, hook);
    }
    // Another mod swapped the slot (FLUF's IServerHook, FLUF Source\Server\FlufServer.cpp 891-903: 36 slots, each
    // keeping the pointer it replaced and calling it): the slot is where server.dll's file holds the export, and our
    // hook chains over whatever is there now - the other mod's, which calls the game's.
    const int i = hk::VtableSlot(srv, vt, fn);
    if (i < 0)
        return nullptr;
    void* now = nullptr;
    __try
    {
        now = vt[i];
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
    if (now == hook || !hk::IsRestoreStyle(reinterpret_cast<uintptr_t>(now)))
        return nullptr; // ours already, or a pointer of unknown origin (fails closed)
    Log("events: %s - the slot holds a FLUF hook (chained over it)", methodExport);
    return hk::PatchVtable(vt, i, hook);
}

// A cargo scan the server accepted from the player (IServer::SPScanCargo(scanner, target, client)): what a mission's
// Cnd_CargoScanned sees (M02's practice scan, 18 Oct 2026: the harness needs the game's own confirmation).
typedef void(__fastcall* SPScanCargoFn)(void* self, void* edx, const u32& scanner, const u32& target, u32 client);
static SPScanCargoFn g_origScan = nullptr;
static void __fastcall SPScanCargoHook(void* self, void* edx, const u32& scanner, const u32& target, u32 client)
{
    Push("scan scanner=%u target=%u", scanner, target);
    g_origScan(self, edx, scanner, target, client);
}

// The base economy as the server receives it (repair, dealers, cash): which client code sends what, so the autopilot's
// base stop can call the game's own repair / buy routines (research 20 Oct 2026; `from` = the caller in Freelancer.exe).
#define SERVER_HOOK(name, params, args, fmt, ...)                                                                      \
    typedef void(__fastcall * name##Fn) params;                                                                        \
    static name##Fn g_orig##name = nullptr;                                                                            \
    static void __fastcall name##Hook params                                                                           \
    {                                                                                                                  \
        Push("server " #name " " fmt " from=%p", __VA_ARGS__, _ReturnAddress());                                       \
        g_orig##name args;                                                                                             \
    }
SERVER_HOOK(ReqHullStatus, (void* self, void* edx, float status, u32 client), (self, edx, status, client),
            "status=%.3f", status)
SERVER_HOOK(ReqChangeCash, (void* self, void* edx, int delta, u32 client), (self, edx, delta, client), "delta=%d",
            delta)
SERVER_HOOK(ReqSetCash, (void* self, void* edx, int cash, u32 client), (self, edx, cash, client), "cash=%d", cash)
SERVER_HOOK(ReqEquipment, (void* self, void* edx, const void* list, u32 client), (self, edx, list, client), "list=%p",
            list)
SERVER_HOOK(ReqCollisionGroups, (void* self, void* edx, const void* list, u32 client), (self, edx, list, client),
            "list=%p", list)
SERVER_HOOK(ReqAddItem,
            (void* self, void* edx, u32 good, const char* hp, int count, float status, bool mounted, u32 client),
            (self, edx, good, hp, count, status, mounted, client), "good=%u hp=%s count=%d status=%.2f mounted=%d",
            good, hp ? hp : "-", count, status, mounted ? 1 : 0)
SERVER_HOOK(ReqRemoveItem, (void* self, void* edx, unsigned short id, int count, u32 client),
            (self, edx, id, count, client), "id=%u count=%d", id, count)
SERVER_HOOK(ReqShipArch, (void* self, void* edx, u32 arch, u32 client), (self, edx, arch, client), "arch=%u", arch)
SERVER_HOOK(GFGoodBuy, (void* self, void* edx, const u32* info, u32 client), (self, edx, info, client),
            "info=%08X %08X %08X %08X", Word(info, 0), Word(info, 4), Word(info, 8), Word(info, 12))
SERVER_HOOK(GFGoodSell, (void* self, void* edx, const u32* info, u32 client), (self, edx, info, client),
            "info=%08X %08X %08X %08X", Word(info, 0), Word(info, 4), Word(info, 8), Word(info, 12))
#undef SERVER_HOOK

static void HookServerCalls()
{
    static bool done = false;
    if (done || !GetModuleHandleA("server.dll"))
        return;
    done = true;
    if (void* o = HookServerMethod("?SPScanCargo@IServerImpl@@UAEXABI0I@Z", reinterpret_cast<void*>(SPScanCargoHook)))
        g_origScan = reinterpret_cast<SPScanCargoFn>(o);
    Log("events: server scan hook %s", g_origScan ? "installed" : "not found");
    int economy = 0;
#define HOOK_ECONOMY(name, exportName)                                                                                 \
    if (void* o = HookServerMethod(exportName, reinterpret_cast<void*>(name##Hook)))                                   \
    {                                                                                                                  \
        g_orig##name = reinterpret_cast<name##Fn>(o);                                                                  \
        ++economy;                                                                                                     \
    }
    HOOK_ECONOMY(ReqHullStatus, "?ReqHullStatus@IServerImpl@@UAEXMI@Z")
    HOOK_ECONOMY(ReqChangeCash, "?ReqChangeCash@IServerImpl@@UAEXHI@Z")
    HOOK_ECONOMY(ReqSetCash, "?ReqSetCash@IServerImpl@@UAEXHI@Z")
    HOOK_ECONOMY(ReqEquipment, "?ReqEquipment@IServerImpl@@UAEXABVEquipDescList@@I@Z")
    HOOK_ECONOMY(ReqCollisionGroups, "?ReqCollisionGroups@IServerImpl@@UAEXABV?$list@UCollisionGroupDesc@@V?$allocator@"
                                     "UCollisionGroupDesc@@@std@@@std@@I@Z")
    HOOK_ECONOMY(ReqAddItem, "?ReqAddItem@IServerImpl@@UAEXIPBDHM_NI@Z")
    HOOK_ECONOMY(ReqRemoveItem, "?ReqRemoveItem@IServerImpl@@UAEXGHI@Z")
    HOOK_ECONOMY(ReqShipArch, "?ReqShipArch@IServerImpl@@UAEXII@Z")
    HOOK_ECONOMY(GFGoodBuy, "?GFGoodBuy@IServerImpl@@UAEXABUSGFGoodBuyInfo@@I@Z")
    HOOK_ECONOMY(GFGoodSell, "?GFGoodSell@IServerImpl@@UAEXABUSGFGoodSellInfo@@I@Z")
#undef HOOK_ECONOMY
    Log("events: server economy hooks %d of 10", economy);
}

// Installs the hooks, and installs them again whenever Content.dll is a fresh copy: the game unloads and reloads it
// (e.g. loading a game after a death), which drops IAT patches; a reloaded copy can sit at the same address, so the
// check reads the import slot itself.
void Update()
{
    static u32 nextCheck = 0;
    if (GetTickCount() < nextCheck)
        return;
    nextCheck = GetTickCount() + 1000;
    HookClientMark();
    HookServerCalls();
    HMODULE content = GetModuleHandleA("content.dll");
    void** probe = hk::ImportSlot(content, "server.dll", "?SendComm@SpaceObj@pub@@YAHIIIPBUCostume@@IPAIHIM_N@Z");
    if (!probe || *probe == reinterpret_cast<void*>(SendCommHook))
        return;
    struct H
    {
        const char* name;
        void* hook;
        void** orig;
    };
    const H hooks[] = {
        {"?SetMissionObjectives@Player@pub@@YAHABI0PBUMissionObjective@12@IABUFmtStr@@E2@Z", SetObjectivesHook,
         reinterpret_cast<void**>(&g_origSetObjectives)},
        {"?SetMissionObjectiveState@Player@pub@@YAHABI0HI@Z", SetObjectiveStateHook,
         reinterpret_cast<void**>(&g_origSetObjectiveState)},
        {"?PopUpDialog@Player@pub@@YAHIABUFmtStr@@0I@Z", PopUpHook, reinterpret_cast<void**>(&g_origPopUp)},
        {"?DisplayMissionMessage@Player@pub@@YAHABIABUFmtStr@@W4MissionMessageType@@_N@Z", MissionMsgHook,
         reinterpret_cast<void**>(&g_origMissionMsg)},
        {"?SendNNMessage@Player@pub@@YAHII@Z", NNMessageHook, reinterpret_cast<void**>(&g_origNNMessage)},
        {"?SendComm@SpaceObj@pub@@YAHIIIPBUCostume@@IPAIHIM_N@Z", SendCommHook,
         reinterpret_cast<void**>(&g_origSendComm)},
        {"?MissionVendorOfferCreate@GF@pub@@YAKABUMissionVendorOfferDescription@@@Z", OfferCreateHook,
         reinterpret_cast<void**>(&g_origOfferCreate)},
        {"?MissionVendorOfferDestroy@GF@pub@@YAXPAK@Z", OfferDestroyHook,
         reinterpret_cast<void**>(&g_origOfferDestroy)},
        {"?SetMoneyNeededToNextRank@Player@pub@@YAHIH@Z", MoneyNeededHook,
         reinterpret_cast<void**>(&g_origMoneyNeeded)},
        {"?AdjustCash@Player@pub@@YAHABIH@Z", AdjustCashHook, reinterpret_cast<void**>(&g_origAdjustCash)},
        {"?SetRank@Player@pub@@YAHIH@Z", SetRankHook, reinterpret_cast<void**>(&g_origSetRank)},
        {"?Save@pub@@YAHII@Z", SaveHook, reinterpret_cast<void**>(&g_origSave)},
        {"?Create@SpaceObj@pub@@YAHAAIABUShipInfo@12@@Z", CreateShipHook, reinterpret_cast<void**>(&g_origCreateShip)},
        {"?CreateLoot@SpaceObj@pub@@YAHAAIABULootInfo@12@@Z", CreateLootHook,
         reinterpret_cast<void**>(&g_origCreateLoot)},
        {"?CreateSolar@SpaceObj@pub@@YAHAAIABUSolarInfo@12@@Z", CreateSolarHook,
         reinterpret_cast<void**>(&g_origCreateSolar)},
    };
    int ok = 0;
    for (const H& h : hooks)
    {
        void** slot = hk::ImportSlot(content, "server.dll", h.name);
        if (slot && *slot == h.hook)
        {
            ++ok; // already ours (never chain a hook to itself)
            continue;
        }
        *h.orig = hk::PatchImport(content, "server.dll", h.name, h.hook);
        if (*h.orig)
            ++ok;
        else
            Log("events: import %s not found", h.name);
    }
    Log("events: Content.dll hooks installed (%d/%d)", ok, static_cast<int>(sizeof(hooks) / sizeof(hooks[0])));
    // Content.dll (re)loaded = a game loaded (every save load reloads it, 25 Oct 2026): the mission's objective was set
    // while its imports were not yet ours (no objective event: HUD 2's headline stayed empty after a mission save loaded
    // in a running game), so take it from the game's own copy once (v25 ObjectiveReread; a free-roam load clears it).
    const u32 seq = ObjectiveReread();
    ObjectiveStep st = {};
    ObjectiveNow(&st);
    Log("events: objective after the load: seq %u obj %u", seq, st.textId);
}
} // namespace events
