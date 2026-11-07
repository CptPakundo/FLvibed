// The DualSense grips' synth (modules\controller\src\wave_synth.h), rendered into buffers: nothing here plays, and nothing here says how it feels.
#include "test.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "../modules/controller/src/wave_synth.h"

using namespace wavesynth;

namespace
{
constexpr unsigned kGoldenWave = 0x556E4F52u; // the fixed scene's checksum, below

// A band's strength in a channel of the buffer: Goertzel.
float Tone(const std::vector<float>& lr, int side, float hz, int from)
{
    float s1 = 0, s2 = 0;
    const float c = 2 * cosf(kPi2 * hz / kRate);
    int n = 0;
    for (size_t i = from * 2 + side; i < lr.size(); i += 2, ++n)
    {
        const float s0 = lr[i] + c * s1 - s2;
        s2 = s1, s1 = s0;
    }
    const float power = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2 * sqrtf(power > 0 ? power : 0) / n;
}
float Rms(const std::vector<float>& lr, int side, int from)
{
    double sum = 0;
    int n = 0;
    for (size_t i = from * 2 + side; i < lr.size(); i += 2, ++n)
        sum += lr[i] * lr[i];
    return n ? static_cast<float>(sqrt(sum / n)) : 0;
}
float Mean(const std::vector<float>& lr, int side, int from)
{
    double sum = 0;
    int n = 0;
    for (size_t i = from * 2 + side; i < lr.size(); i += 2, ++n)
        sum += lr[i];
    return n ? static_cast<float>(sum / n) : 0;
}
std::vector<float> Draw(Synth& s, const WaveFrame& f, float seconds)
{
    s.Set(f);
    std::vector<float> out(static_cast<size_t>(seconds * kRate) * 2);
    s.Render(out.data(), static_cast<int>(out.size() / 2));
    return out;
}
} // namespace

void Test_wave_synth()
{
    // ---- a zero frame is silence, exactly
    {
        Synth s;
        const std::vector<float> o = Draw(s, WaveFrame{}, 0.2f);
        bool zero = true;
        for (float v : o)
            zero = zero && v == 0;
        CHECK(zero);
    }
    // ---- each band where it was asked for, on its grip
    {
        Synth s;
        WaveFrame f = {};
        f.bedHz = 30, f.massHz = 80, f.texHz = 220;
        f.mass[1] = 0.3f;
        const std::vector<float> o = Draw(s, f, 0.5f);
        CHECK(Tone(o, 1, 80, 4800) > 0.2f);
        CHECK(Tone(o, 0, 80, 4800) < 0.001f); // the left grip silent
        CHECK(Tone(o, 1, 220, 4800) < 0.02f);
    }
    {
        Synth s;
        WaveFrame f = {};
        f.bedHz = 30, f.massHz = 80, f.texHz = 220;
        f.tex[0] = f.tex[1] = 0.3f;
        const std::vector<float> o = Draw(s, f, 0.5f);
        CHECK(Tone(o, 0, 220, 4800) > 0.2f && Tone(o, 0, 80, 4800) < 0.02f);
    }
    {
        Synth s;
        WaveFrame f = {};
        f.bedHz = 30, f.massHz = 80, f.texHz = 220;
        f.bed[0] = f.bed[1] = 0.2f;
        const std::vector<float> o = Draw(s, f, 0.5f);
        CHECK(Tone(o, 0, 30, 9600) > 0.1f); // under 40 Hz, through the 20 Hz high-pass
    }
    // ---- an attack stays an attack: full level inside a few milliseconds
    {
        Synth s;
        WaveFrame f = {};
        f.massHz = 80, f.texHz = 220;
        f.tex[0] = 0.2f;
        std::vector<float> o = Draw(s, f, 0.02f);
        float peak = 0;
        for (int i = 0; i < 480; ++i) // the first 10 ms
            peak = std::max(peak, fabsf(o[i * 2]));
        CHECK(peak > 0.12f);
    }
    // ---- no DC, and nothing below the high-pass survives
    {
        Synth s;
        WaveFrame f = {};
        f.bedHz = 5, f.massHz = 80, f.texHz = 220;
        f.bed[0] = 0.5f;
        const std::vector<float> o = Draw(s, f, 2.0f);
        CHECK(fabsf(Mean(o, 0, 48000)) < 0.002f);
        CHECK(Tone(o, 0, 5, 48000) < 0.5f * 0.35f); // about -10 dB at a quarter of the corner
    }
    // ---- the energy limit: a sustained full signal is held at the cap, a short peak is not touched
    {
        Synth s;
        WaveFrame f = {};
        f.massHz = 80, f.texHz = 220;
        f.mass[0] = f.mass[1] = 1;
        const std::vector<float> o = Draw(s, f, 6.0f);
        CHECK(Rms(o, 0, 48000 * 4) <= Synth::kEnergyCap * 1.1f);
        CHECK(Rms(o, 0, 48000 * 4) >= Synth::kEnergyCap * 0.8f);
        CHECK(s.Gain(0) < 0.5f);
        Synth t;
        std::vector<float> p = Draw(t, f, 0.03f); // a hit: 30 ms
        float peak = 0;
        for (float v : p)
            peak = std::max(peak, fabsf(v));
        CHECK(peak > 0.8f);
        // and it recovers
        WaveFrame quiet = {};
        quiet.massHz = 80, quiet.texHz = 220;
        Draw(s, quiet, 8.0f);
        CHECK(s.Gain(0) > 0.9f);
    }
    // ---- never past full scale, whatever is asked
    {
        Synth s;
        WaveFrame f = {};
        f.bedHz = 30, f.massHz = 80, f.texHz = 220, f.noise = 1;
        for (int side = 0; side < 2; ++side)
            f.bed[side] = f.mass[side] = f.tex[side] = 1, f.clicks[side] = 3;
        const std::vector<float> o = Draw(s, f, 0.5f);
        bool ok = true;
        for (float v : o)
            ok = ok && v >= -1 && v <= 1 && v == v;
        CHECK(ok);
    }
    // ---- a click is a burst on its grip
    {
        Synth s;
        WaveFrame f = {};
        f.massHz = 80, f.texHz = 220;
        f.clicks[1] = 1;
        const std::vector<float> o = Draw(s, f, 0.05f);
        CHECK(Rms(o, 1, 0) > 0.01f && Rms(o, 0, 0) == 0);
    }
    // ---- the same frames give the same samples: a golden checksum of a fixed scene (changes only with the synth's authoring)
    {
        Synth s;
        WaveFrame f = {};
        f.bedHz = 30, f.massHz = 72, f.texHz = 240, f.noise = 0.25f;
        f.bed[0] = 0.1f, f.mass[0] = 0.4f, f.mass[1] = 0.1f, f.tex[1] = 0.3f;
        const std::vector<float> a = Draw(s, f, 0.25f);
        Synth t;
        const std::vector<float> b = Draw(t, f, 0.25f);
        CHECK(a == b);
        unsigned hash = 2166136261u;
        for (float v : a)
        {
            const int q = static_cast<int>(v * 32767);
            hash = (hash ^ static_cast<unsigned>(q & 0xFFFF)) * 16777619u;
        }
        if (hash != kGoldenWave)
            fprintf(stderr, "wave_synth: the scene's checksum is now 0x%08Xu\n", hash);
        CHECK(hash == kGoldenWave);
    }

    // ---- the stream's rules
    CHECK(kWaveformProven); // decision 2: on by default, the owner's word of 6 Nov 2026: opt out, not opt in
    CHECK(WantWave(kModeAuto) && !WantWave(kModeMotors) && WantWave(kModeWave));
    CHECK(WaveFormatRefusal(4, 48000, 0x33) == nullptr); // this Edge, read 6 Nov 2026
    CHECK(WaveFormatRefusal(2, 48000, 0x3) != nullptr);  // a Stereo layout
    CHECK(WaveFormatRefusal(4, 44100, 0x33) != nullptr);
    CHECK(WaveFormatRefusal(4, 48000, 0x603) != nullptr); // side speakers, no back pair
    CHECK(WaveFormatRefusal(6, 48000, 0x3F) == nullptr);
    CHECK(BackChannel(0x33) == 2 && BackChannel(0x3F) == 4 && BackChannel(0x3) == -1 && BackChannel(0) == 2);
    CHECK(WaveOpenRefusal(true, false, true) == nullptr);
    CHECK(WaveOpenRefusal(false, false, true) && WaveOpenRefusal(true, true, true) && WaveOpenRefusal(true, false, false));
    // the speaker by name (Wine, 7 Nov 2026): Windows' and PipeWire's names of a DualSense's endpoint, never another device's
    CHECK(EndpointNameMatches(L"Speakers (2- DualSense Edge Wireless Controller)", "DualSense Edge Wireless Controller"));
    CHECK(EndpointNameMatches(L"DualSense Wireless Controller Analog Surround 4.0", "PS5 Controller"));
    CHECK(EndpointNameMatches(L"dualsense wireless controller", nullptr));
    CHECK(!EndpointNameMatches(L"Speakers (Realtek(R) Audio)", "DualSense Edge Wireless Controller"));
    CHECK(!EndpointNameMatches(L"Headphones (Xbox Wireless Controller)", "PS5 Controller"));
    CHECK(!EndpointNameMatches(nullptr, "DualSense") && !EndpointNameMatches(L"", "DualSense"));
    char id[128];
    CHECK(InstanceFromPath("\\\\?\\HID#VID_054C&PID_0DF2&MI_03#7&abc&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}", id, sizeof(id)));
    CHECK(!strcmp(id, "HID\\VID_054C&PID_0DF2&MI_03\\7&abc&0&0000"));
    CHECK(!InstanceFromPath("nothing", id, sizeof(id)) && !InstanceFromPath(nullptr, id, sizeof(id)));
}
