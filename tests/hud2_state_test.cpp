// HUD 2's state as text (modules\hud\src\hud2_state_io.h, audit 71b B4a): every struct's Fields list is whole (each
// member listed once: its count is the compiler's, and no two listed members share a byte), a sentinel round trip (every
// member set to a value its default does not have, written, read back into a state that held other values, written again:
// the two texts equal), and the reader refuses what is not a whole state; a frame the game recorded (tests\bench\, B4b)
// reads back and writes again byte for byte. FLVIBED_STATE_PINS=1 prints the pins.
#include "test.h"

#include "../modules/hud/src/hud2_state_io.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

namespace
{
// Sets every listed member to a value its default does not have (a flag flipped, a number moved, text appended with the
// characters the writer must escape and encode).
struct Filler
{
    static constexpr bool kSets = true, kFills = true;
    unsigned n = 0;
    bool failed = false;

    template <class T> void Int(T& x) { x = static_cast<T>(x + static_cast<T>(1 + ++n % 50)); }
    template <class T> void Real(T& x)
    {
        T y = -x - static_cast<T>(1.5) - static_cast<T>(++n % 97);
        x = y == x ? y + 1 : y;
    }
    void operator()(const char*, bool& x) { x = !x; }
    void operator()(const char*, int& x) { Int(x); }
    void operator()(const char*, unsigned& x) { Int(x); }
    void operator()(const char*, long long& x) { Int(x); }
    void operator()(const char*, unsigned long long& x) { Int(x); }
    void operator()(const char*, wchar_t& x) { Int(x); }
    void operator()(const char*, float& x) { Real(x); }
    void operator()(const char*, double& x) { Real(x); }
    void operator()(const char*, std::string& x) { x += "\"\\\x01/\xC3\xA9" + std::to_string(++n); }
    void operator()(const char*, std::wstring& x) { x += L"\"\\\x01" L"\x00E9" L"\x4E2D" L"\xD83D\xDE80" + std::to_wstring(++n); } // (e-acute, a CJK sign, a rocket)
    template <class T> std::enable_if_t<std::is_enum_v<T>> operator()(const char*, T& x)
    {
        x = static_cast<T>(static_cast<std::underlying_type_t<T>>(x) + 1);
    }
    template <class T> std::enable_if_t<std::is_class_v<T> && !hud2state::kIsText<T>> operator()(const char*, T& x)
    {
        hud2state::Fields(*this, x);
    }
    template <class T, size_t N> void operator()(const char*, T (&a)[N])
    {
        for (T& e : a)
            (*this)(nullptr, e);
    }
    template <class T> void operator()(const char*, std::vector<T>& a)
    {
        if (a.empty())
            a.resize(2);
        for (T& e : a)
            (*this)(nullptr, e);
    }
    template <class T, class F> void group(const char*, T& x, F f) { f(*this, x); }
    template <class T> void note(T&) {}
    void fail(const char*, const char*) { failed = true; }
};

// The lists against the structs: per struct, the members listed (a fixed array counts its elements, as the compiler's
// count does) equal its member count, and no two listed members share a byte. Each struct type is checked once, from the
// first object of it met.
struct Coverage
{
    static constexpr bool kSets = false, kFills = false;
    struct Span
    {
        size_t at, size, align;
        std::string name;
    };
    struct Frame
    {
        std::string type;
        const char* base;
        size_t size, align, members, listed = 0; // members 0: not an aggregate (no count: the gaps are checked instead)
        std::vector<Span> spans;
    };
    std::vector<Frame> stack;
    std::set<std::string> checked;
    std::vector<std::string> problems;

    // a member of the open struct (a stand-in local of a Fields function lies outside it: not counted)
    void Member(const char* k, const void* p, size_t size, size_t align, size_t count)
    {
        if (stack.empty())
            return;
        Frame& f = stack.back();
        const char* c = static_cast<const char*>(p);
        if (c < f.base || c + size > f.base + f.size)
            return;
        f.spans.push_back({static_cast<size_t>(c - f.base), size, align, k ? k : "[]"});
        f.listed += count;
    }
    template <class T> void Open(const std::string& type, T& x)
    {
        stack.push_back({type, reinterpret_cast<const char*>(&x), sizeof(T), alignof(T), hud2state::MemberCount<T>()});
        if (!std::is_aggregate_v<T>)
            ++nonAggregates;
    }
    template <class T> void Check(T& x)
    {
        if (!checked.insert(typeid(T).name()).second)
            return;
        Open(typeid(T).name(), x);
        hud2state::Fields(*this, x);
        Close();
    }
    void Close()
    {
        Frame f = stack.back();
        stack.pop_back();
        if (f.members && f.listed != f.members)
            problems.push_back(f.type + ": " + std::to_string(f.listed) + " listed, " + std::to_string(f.members) + " members");
        std::sort(f.spans.begin(), f.spans.end(), [](const Span& a, const Span& b) { return a.at < b.at; });
        size_t end = 0; // the listed bytes so far
        for (size_t i = 0; i < f.spans.size(); ++i)
        {
            const Span& s = f.spans[i];
            if (s.at < end)
                problems.push_back(f.type + ": " + f.spans[i - 1].name + " and " + s.name + " share bytes");
            else if (!f.members && s.at > end && !(s.at - end < s.align && s.at % s.align == 0))
                problems.push_back(f.type + ": bytes " + std::to_string(end) + ".." + std::to_string(s.at) + " before " + s.name + " not listed");
            end = std::max(end, s.at + s.size);
        }
        if (!f.members && f.size - end >= f.align)
            problems.push_back(f.type + ": its last " + std::to_string(f.size - end) + " bytes not listed");
    }
    int nonAggregates = 0;
    template <class T> void Leaf(const char* k, T& x) { Member(k, &x, sizeof(T), alignof(T), 1); }
    void operator()(const char* k, bool& x) { Leaf(k, x); }
    void operator()(const char* k, int& x) { Leaf(k, x); }
    void operator()(const char* k, unsigned& x) { Leaf(k, x); }
    void operator()(const char* k, long long& x) { Leaf(k, x); }
    void operator()(const char* k, unsigned long long& x) { Leaf(k, x); }
    void operator()(const char* k, wchar_t& x) { Leaf(k, x); }
    void operator()(const char* k, float& x) { Leaf(k, x); }
    void operator()(const char* k, double& x) { Leaf(k, x); }
    void operator()(const char* k, std::string& x) { Leaf(k, x); }
    void operator()(const char* k, std::wstring& x) { Leaf(k, x); }
    template <class T> std::enable_if_t<std::is_enum_v<T>> operator()(const char* k, T& x) { Leaf(k, x); }
    template <class T> std::enable_if_t<std::is_class_v<T> && !hud2state::kIsText<T>> operator()(const char* k, T& x)
    {
        Leaf(k, x);
        Check(x);
    }
    template <class T, size_t N> void operator()(const char* k, T (&a)[N])
    {
        Member(k, &a, sizeof(a), alignof(T), N);
        if constexpr (std::is_class_v<T> && !hud2state::kIsText<T>)
            Check(a[0]);
    }
    template <class T> void operator()(const char* k, std::vector<T>& a) // (a member, or a stand-in outside the struct; its element type is checked)
    {
        Leaf(k, a);
        if constexpr (std::is_class_v<T> && !hud2state::kIsText<T>)
        {
            T e = a.empty() ? T() : a[0];
            Check(e);
        }
    }
    template <class T, class F> void group(const char* k, T& x, F f)
    {
        Leaf(k, x);
        if (!checked.insert(typeid(T).name()).second)
            return;
        Open(std::string(k) + " (unnamed)", x);
        f(*this, x);
        Close();
    }
    template <class T> void note(T& x) { Leaf("(by meaning)", x); }
    void fail(const char*, const char*) {}
};

// The leaves of two parsed texts of the same shape that are equal (arrays of other lengths skipped), with their paths.
void SameLeaves(const hud2state::Json& a, const hud2state::Json& b, const std::string& path, std::vector<std::string>& out)
{
    if (a.kind != b.kind)
        return out.push_back(path + " (kind)");
    if (a.kind == hud2state::Json::Object)
    {
        if (a.members.size() != b.members.size())
            return out.push_back(path + " (members)");
        for (size_t i = 0; i < a.members.size(); ++i)
            SameLeaves(a.members[i].second, b.members[i].second, path + "." + a.members[i].first, out);
    }
    else if (a.kind == hud2state::Json::Array)
    {
        if (a.items.size() == b.items.size())
            for (size_t i = 0; i < a.items.size(); ++i)
                SameLeaves(a.items[i], b.items[i], path + "[" + std::to_string(i) + "]", out);
    }
    else if (a.text == b.text)
        out.push_back(path);
}

bool Parse(const std::string& text, hud2state::Json& j)
{
    hud2state::JsonParser p{text};
    return p.Value(j);
}

// `text` with the member line holding `key` taken out / changed (the writer puts one member on a line, the comma last).
std::string Without(std::string text, const std::string& key)
{
    const size_t p = text.find("\"" + key + "\": ");
    if (p == std::string::npos)
        return "";
    return text.erase(p, text.find('\n', p) + 1 - p);
}
std::string Replaced(std::string text, const std::string& from, const std::string& to)
{
    const size_t p = text.find(from);
    return p == std::string::npos ? "" : text.replace(p, from.size(), to);
}

size_t Count(const std::string& s, const std::string& what)
{
    size_t n = 0;
    for (size_t p = s.find(what); p != std::string::npos; p = s.find(what, p + 1))
        ++n;
    return n;
}
} // namespace

#define FLV_STATE_PRINT(T) printf("FLV_STATE_PIN(%s, %u, %u);\n", #T, static_cast<unsigned>(sizeof(T)), static_cast<unsigned>(hud2state::MemberCount<T>()));

void Test_hud2_state()
{
    const char* pins = getenv("FLVIBED_STATE_PINS");
    if (pins && *pins == '1')
    {
        FLV_STATE_TYPES(FLV_STATE_PRINT)
    }

    // the lists are whole: every struct reached from a state file and from the frame's answers
    {
        auto doc = std::make_unique<hud2state::StateDoc>();
        hud2::StartOut so;
        hud2::StepOut out;
        Coverage c;
        c.Check(*doc), c.Check(so), c.Check(out);
        for (const std::string& p : c.problems)
            fprintf(stderr, "hud2_state: %s\n", p.c_str());
        CHECK(c.problems.empty());
        CHECK(c.checked.size() == 65); // FLV_STATE_TYPES' structs (+ the two unnamed eases and tMark among them)
        CHECK(c.nonAggregates == 1);   // hud2target::State alone is held by its gaps, not its count
    }

    // the sentinel round trip
    auto def = std::make_unique<hud2state::StateDoc>();
    auto a = std::make_unique<hud2state::StateDoc>();
    Filler fa;
    hud2state::Fields(fa, *a);
    CHECK(!fa.failed && fa.n > 400);
    const std::string tDef = hud2state::Write(*def), tA = hud2state::Write(*a);
    CHECK(tA.find(hud2state::PrivateMarker()) == std::string::npos && tDef.find(hud2state::PrivateMarker()) != std::string::npos);
    {
        // every member written moved from its default (the schema is a constant)
        hud2state::Json jd, ja;
        CHECK(Parse(tDef, jd) && Parse(tA, ja));
        std::vector<std::string> same;
        SameLeaves(jd, ja, "", same);
        for (const std::string& s : same)
            if (s != ".schema")
                fprintf(stderr, "hud2_state: the filler left %s at its default\n", s.c_str());
        CHECK(same.size() == 1 && same[0] == ".schema");
    }
    auto b = std::make_unique<hud2state::StateDoc>();
    std::string err;
    CHECK(hud2state::Read(tA, *b, &err));
    if (!err.empty())
        fprintf(stderr, "hud2_state: %s\n", err.c_str());
    const std::string tB = hud2state::Write(*b);
    CHECK(tB == tA);
    CHECK(!b->priv && b->in.accept == a->in.accept && b->st.m.tName == a->st.m.tName && b->st.m.presetData == a->st.m.presetData);
    CHECK(b->st.parts.marquees.clocks.size() == 2 && b->st.m.adapt.compat.note == a->st.m.adapt.compat.note);
    // a state that held other values takes every member from the text
    {
        auto c = std::make_unique<hud2state::StateDoc>();
        Filler f1, f2;
        f2.n = 1000;
        hud2state::Fields(f1, *c), hud2state::Fields(f2, *c);
        CHECK(hud2state::Read(tA, *c) && hud2state::Write(*c) == tA);
        CHECK(hud2state::Read(tDef, *c) && hud2state::Write(*c) == tDef);
    }
    // a float that is not finite, and floats bit for bit
    {
        auto c = std::make_unique<hud2state::StateDoc>();
        c->st.t = std::numeric_limits<float>::quiet_NaN(), c->st.viewZoom = std::numeric_limits<float>::infinity();
        c->st.formDt = -std::numeric_limits<float>::infinity(), c->st.noticeX = 0.1f, c->st.noticeY = -0.0f;
        c->st.navEase.at = 0xFFFFFFFFFFFFFFFFull, c->st.loadoutSig = -0x7FFFFFFFFFFFFFFFll - 1;
        auto d = std::make_unique<hud2state::StateDoc>();
        CHECK(hud2state::Read(hud2state::Write(*c), *d));
        CHECK(std::isnan(d->st.t) && d->st.viewZoom > 1e38f && d->st.formDt < -1e38f && d->st.noticeX == 0.1f);
        CHECK(std::signbit(d->st.noticeY) && d->st.navEase.at == c->st.navEase.at && d->st.loadoutSig == c->st.loadoutSig);
    }

    // the reader refuses what is not a whole state, and leaves its target as it was
    const auto refused = [&](const std::string& text, const char* what, const char* reason)
    {
        auto c = std::make_unique<hud2state::StateDoc>();
        std::string why;
        const bool ok = !text.empty() && !hud2state::Read(text, *c, &why) && hud2state::Write(*c) == tDef &&
                        why.find(reason) != std::string::npos;
        if (!ok)
            fprintf(stderr, "hud2_state: %s: not refused as expected (%s)\n", what, why.c_str());
        return ok;
    };
    // (Step's state and Render's: each member twice, the edits below change Step's, the first)
    CHECK(Count(tA, "\"view_zoom\": ") == 2 && Count(tA, "\"menu_over\": ") == 2 && Count(tA, "\"measured_px\": ") == 2);
    CHECK(tA.find("\"view_zoom\": ") < tA.find("\"render\": {"));
    CHECK(refused(Without(tA, "view_zoom"), "a member missing", "frame_state.view_zoom: expected here, found \"sim\""));
    CHECK(refused(Replaced(tA, "\"view_zoom\": ", "\"zoom_view\": 1,\n\"view_zoom\": "), "an unknown member", "zoom_view"));
    CHECK(refused(Replaced(tA, "\"menu_over\": ", "\"visible\": true,\n\"menu_over\": "), "a member twice", "menu_over: expected"));
    CHECK(refused(Replaced(tA, "\"view_zoom\": ", "\"view_zoom\": \"x\",\n\"sim_was\": "), "a wrong kind", "view_zoom: not a number"));
    CHECK(refused(Replaced(tA, "\"measured_px\": ", "\"measured_px\": 99999999999,\n\"was\": "), "out of range", "out of range"));
    CHECK(refused(Replaced(tA, "\"measured_px\": ", "\"measured_px\": 1.5,\n\"was\": "), "a fraction", "not an integer"));
    CHECK(refused(Replaced(tA, "\"rad_art\": [", "\"rad_art\": [1,"), "an array longer", "an array of 3 for 2"));
    CHECK(refused(Replaced(tA, "\"view_zoom\": ", "\"view_zoom\": 1e39,\n\"was\": "), "a float past its range", "view_zoom: a number out of range"));
    CHECK(refused(Replaced(tA, "\"flstate/0\"", "\"flstate/9\""), "another schema", "not flstate/0"));
    CHECK(refused(Replaced(tA, "\"public\"", "\"open\""), "another privacy", "privacy"));
    CHECK(refused(Replaced(tA, "\"preset_data\": ", "\"preset_data\": 9,\n\"was\": "), "a preset past the table", "preset_data"));
    CHECK(refused(Replaced(tA, "\"compat_cell\": ", "\"compat_cell\": -1,\n\"was\": "), "a copy of no cell", "compat_cell"));
    // (cut at a line's end past the middle: the plain middle fell inside a string once the model grew - GitHub 84 / 85)
    CHECK(refused(tA.substr(0, tA.find('\n', tA.size() / 2)), "a cut text", "missing"));
    CHECK(refused(tA + "{}", "text after the object", "text after the object"));
    CHECK(refused(Replaced(tA, "\"accept\": \"", "\"accept\": \"\xC3("), "bytes that are not UTF-8", "not UTF-8"));
    CHECK(refused("[]", "not an object", "not an object"));
    // the retired members (740aa4d9: base_pages, ui_style, skin_native of "rows") are read past where a state recorded
    // before then has them, and nowhere else; a member the list does not name is still refused there
    {
        // ("nn_pause" and "skin_motion" stand only in "rows", skin_motion its last member: one before, one between, one after)
        CHECK(Count(tA, "\"nn_pause\": ") == 2 && Count(tA, "\"skin_motion\": ") == 2);
        std::string old = Replaced(tA, "\"nn_pause\": ", "\"base_pages\": 1,\n\"nn_pause\": ");
        old = Replaced(old, "\"skin_motion\": ", "\"ui_style\": 1,\n\"skin_motion\": ");
        old.insert(old.find('\n', old.find("\"skin_motion\": ")), ",\n\"skin_native\": \"\"");
        auto c = std::make_unique<hud2state::StateDoc>();
        std::string why;
        CHECK(old != tA && hud2state::Read(old, *c, &why) && hud2state::Write(*c) == tA);
        if (!why.empty())
            fprintf(stderr, "hud2_state: the retired members: %s\n", why.c_str());
        // listed where they stood, and written back there by a writer given them (the bench's fingerprint check)
        std::vector<hud2state::RetiredMember> list;
        CHECK(hud2state::Read(old, *c, nullptr, &list) && list.size() == 3);
        CHECK(list.size() == 3 && list[0].before == "nn_pause" && list[1].before == "skin_motion" && list[2].before.empty());
        CHECK(list.size() == 3 && list[0].path == list[2].path && list[0].path.size() > 5 &&
              list[0].path.compare(list[0].path.size() - 5, 5, ".rows") == 0 && list[2].text == "\"\"");
        hud2state::Writer w;
        w.retired = &list;
        w.Open(nullptr, '{');
        hud2state::Fields(w, *c);
        w.Close('}', true);
        const size_t pb = w.out.find("\"base_pages\": 1,"), pn = w.out.find("\"nn_pause\": "), pu = w.out.find("\"ui_style\": 1,");
        const size_t pm = w.out.find("\"skin_motion\": "), ps = w.out.find("\"skin_native\": \"\"\n");
        CHECK(pb != std::string::npos && pb < pn && pn < pu && pu < pm && pm < ps && ps != std::string::npos);
        CHECK(Count(w.out, "\"base_pages\": ") == 1 && Count(w.out, "\"skin_native\": ") == 1);
        std::vector<hud2state::RetiredMember> again;
        CHECK(hud2state::Read(w.out + "\n", *c, nullptr, &again) && again.size() == 3 && hud2state::Write(*c) == tA);
        CHECK(refused(Replaced(tA, "\"nn_pause\": ", "\"base_pages\": {},\n\"nn_pause\": "), "a retired member as an object", "not a value"));
        CHECK(refused(Replaced(tA, "\"nn_pause\": ", "\"base_page\": 1,\n\"nn_pause\": "), "an unknown member in rows", "base_page"));
        CHECK(refused(Replaced(tA, "\"view_zoom\": ", "\"base_pages\": 1,\n\"view_zoom\": "), "a retired name elsewhere", "base_pages"));
    }

    // a frame the game recorded (`hud2 bench record`, tests\bench_record.ps1; B4b): read back and written again, the same
    // bytes; what makes it a recorded frame (both halves, the tape, the list's fingerprints) is there
    {
        const std::string dir = g_test.fixtures + "\\..\\bench";
        int files = 0;
        WIN32_FIND_DATAA fd;
        const HANDLE h = FindFirstFileA((dir + "\\*.flstate.json").c_str(), &fd);
        for (BOOL more = h != INVALID_HANDLE_VALUE; more; more = FindNextFileA(h, &fd))
        {
            ++files;
            std::string text;
            if (FILE* f = fopen((dir + "\\" + fd.cFileName).c_str(), "rb"))
            {
                char buf[65536];
                for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;)
                    text.append(buf, n);
                fclose(f);
            }
            auto r = std::make_unique<hud2state::StateDoc>();
            std::string why;
            const bool read = hud2state::Read(text, *r, &why);
            if (!read)
                fprintf(stderr, "hud2_state: %s: %s\n", fd.cFileName, why.c_str());
            CHECK(read && hud2state::Write(*r) == text);
            CHECK(r->priv && !r->build.empty() && r->render.drawn && r->update.phase == hud2::Phase::Live);
            // (the layout's eases read tickMs64 in Step; the parts' pulses tickMs in Draw; both read typing)
            CHECK(!r->update.tape.tickMs64.empty() && !r->update.tape.typing.empty() && !r->render.tape.tickMs.empty());
            CHECK(r->render.st.L.H == static_cast<float>(r->render.sh) && r->render.drawPx > 0 && r->render.iconsReady);
            CHECK(r->list.over.size() == 6 && !r->list.sets.empty() && r->list.hash.size() == 16 && r->list.inputs.size() == 16);
        }
        if (h != INVALID_HANDLE_VALUE)
            FindClose(h);
        CHECK(files >= 1);
    }
}
