// FLvibed - the DualSense grips' synth (modules\controller\docs\HAPTICS.md 8): what the logic asks for (WaveFrame), the signal drawn from it, the energy limiter last, and the
// rules of the stream (the endpoint's format, the host's refusals). Pure and header only, free of system headers, as pad_backend.h: the host's stream
// (host\hapticwave.cpp) and the logic both include it, and tests\wave_synth_test.cpp renders into buffers, so nothing is ever played to test it.
#pragma once

namespace wavesynth
{
// Waveform by default on Automatic only once the owner's card has passed its probe (decision 2, HAPTICS.md 2). Until then Automatic reads motors and Waveform is the opt-in.
constexpr bool kWaveformProven = true;

// `haptics_mode`
constexpr int kModeAuto = 0, kModeMotors = 1, kModeWave = 2;
inline bool WantWave(int mode) { return mode == kModeWave || (mode == kModeAuto && kWaveformProven); }

// One frame of the logic's mix: left and right grip, each band's level 0..1, the bands' frequencies. A zero frame is silence.
struct WaveFrame
{
    float bed[2], mass[2], tex[2];
    float bedHz, massHz, texHz;
    float noise;   // the texture band's share of noise
    int clicks[2]; // bursts to draw from this frame
};

constexpr int kRate = 48000;
constexpr float kPi2 = 6.2831853f;

class Synth
{
  public:
    static constexpr float kSmooth = 0.0015f;   // seconds: how fast a level reaches its target; short, so that an attack stays an attack
    static constexpr float kHighPass = 20.0f;   // Hz: nothing below it reaches the coil, and no DC
    static constexpr float kEnergyCap = 0.18f;  // root of the energy over about kEnergyWindow, of full scale: hsll233's unverified limit, a sustained signal is what heats a coil
    static constexpr float kEnergyWindow = 1.0f;
    static constexpr float kClickSec = 0.003f;

    void Set(const WaveFrame& f)
    {
        want_ = f;
        for (int side = 0; side < 2; ++side)
        {
            clickLeft_[side] += f.clicks[side] > 0 ? static_cast<int>(kClickSec * kRate) : 0;
        }
    }

    // `frames` samples of both grips, interleaved left, right, -1..1.
    void Render(float* out, int frames)
    {
        const float k = 1.0f - expf(-1.0f / (kSmooth * kRate));
        const float hp = expf(-kPi2 * kHighPass / kRate);
        const float energyK = 1.0f / (kEnergyWindow * kRate);
        for (int i = 0; i < frames; ++i)
        {
            Step(bedPhase_, want_.bedHz), Step(massPhase_, want_.massHz), Step(texPhase_, want_.texHz);
            const float bedS = sinf(bedPhase_), massS = sinf(massPhase_), texS = sinf(texPhase_);
            for (int side = 0; side < 2; ++side)
            {
                bed_[side] += (want_.bed[side] - bed_[side]) * k;
                mass_[side] += (want_.mass[side] - mass_[side]) * k;
                tex_[side] += (want_.tex[side] - tex_[side]) * k;
                const float noise = Rand() * 2 - 1;
                const float texture = texS * (1 - want_.noise) + noise * want_.noise;
                float x = bed_[side] * bedS + mass_[side] * massS + tex_[side] * texture;
                if (clickLeft_[side] > 0)
                {
                    --clickLeft_[side];
                    x += 0.6f * noise;
                }
                // the high-pass
                const float y = hp * (hpOut_[side] + x - hpIn_[side]);
                hpIn_[side] = x, hpOut_[side] = y;
                // the energy limiter, last: the gain falls as soon as the window's energy passes the cap, and rises slowly
                energy_[side] += (y * y - energy_[side]) * energyK;
                const float root = sqrtf(energy_[side]);
                const float target = root > kEnergyCap ? kEnergyCap / root : 1.0f;
                gain_[side] += (target - gain_[side]) * (target < gain_[side] ? 0.01f : 0.0001f);
                float v = y * gain_[side];
                v = v > 1 ? 1 : v < -1 ? -1 : v;
                out[i * 2 + side] = v;
            }
        }
    }

    float Gain(int side) const { return gain_[side]; }

  private:
    static void Step(float& phase, float hz)
    {
        phase += kPi2 * hz / kRate;
        if (phase > kPi2)
            phase -= kPi2;
    }
    float Rand()
    {
        rng_ ^= rng_ << 13, rng_ ^= rng_ >> 17, rng_ ^= rng_ << 5;
        return static_cast<float>(rng_ >> 8) / 16777216.0f;
    }

    WaveFrame want_ = {};
    float bed_[2] = {0, 0}, mass_[2] = {0, 0}, tex_[2] = {0, 0};
    float bedPhase_ = 0, massPhase_ = 0, texPhase_ = 0;
    float hpIn_[2] = {0, 0}, hpOut_[2] = {0, 0}, energy_[2] = {0, 0}, gain_[2] = {1, 1};
    int clickLeft_[2] = {0, 0};
    unsigned rng_ = 0x9E3779B9u;
};

// ---------------------------------------------------------------- the stream's rules
constexpr unsigned kSpeakerBackLeft = 0x10, kSpeakerBackRight = 0x20; // KSAUDIO_SPEAKER: the grips

// The endpoint's mix format as the stream needs it: four channels (front pair, back pair) at 48 kHz. Null: fit; else why not, for the log.
// A Stereo layout in the Sound panel has no back pair in shared mode (hsll233): the player gets motors and the reason.
inline const char* WaveFormatRefusal(int channels, int rate, unsigned mask)
{
    if (channels < 4)
        return "the pad's speaker has fewer than four channels (its layout in the Sound panel)";
    if (rate != kRate)
        return "the pad's speaker is not at 48 kHz";
    if (mask && (mask & (kSpeakerBackLeft | kSpeakerBackRight)) != (kSpeakerBackLeft | kSpeakerBackRight))
        return "the pad's speaker has no back pair";
    return nullptr;
}

// The channel of the back pair's left grip in the frame's order (the mask's bits, lowest first): 2 for the pad's 0x33. -1 unknown.
inline int BackChannel(unsigned mask)
{
    if (!mask)
        return 2;
    int n = 0;
    for (unsigned bit = 1; bit < kSpeakerBackLeft; bit <<= 1)
        n += (mask & bit) != 0;
    return (mask & kSpeakerBackLeft) ? n : -1;
}

// May the stream open at all? No test, no background game, no Wine: the coils can be heard, and a ContainerId match fails there anyway. Null: yes; else why not.
inline const char* WaveOpenRefusal(bool device, bool background, bool wine, bool wired)
{
    if (!device)
        return "haptics_device 0";
    if (background)
        return "a background game";
    if (wine)
        return "Wine";
    if (!wired)
        return "not on USB";
    return nullptr;
}

// A device interface path ("\\?\HID#VID_054C&PID_0DF2&MI_03#7&abc&0&0000#{guid}") into its instance id ("HID\VID_054C&PID_0DF2&MI_03\7&abc&0&0000"). False if it is none.
inline bool InstanceFromPath(const char* path, char* out, int cap)
{
    if (!path || !out || cap <= 0)
        return false;
    const char* p = path;
    if (p[0] == '\\' && p[1] == '\\' && (p[2] == '?' || p[2] == '.') && p[3] == '\\')
        p += 4;
    int n = 0, hashes = 0;
    for (; *p && *p != '{' && n + 1 < cap; ++p)
    {
        char c = *p;
        if (c == '#')
        {
            ++hashes;
            c = '\\';
        }
        out[n++] = c;
    }
    if (n > 0 && out[n - 1] == '\\')
        --n;
    out[n] = 0;
    return hashes >= 2;
}
} // namespace wavesynth
