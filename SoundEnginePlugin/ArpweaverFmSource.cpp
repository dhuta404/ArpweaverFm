#include "ArpweaverFmSource.h"
#include "../ArpweaverFmConfig.h"

#include <AK/AkWwiseSDKVersion.h>
#include <algorithm>
#include <cmath>

AK::IAkPlugin* CreateArpweaverFmSource(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, ArpweaverFmSource());
}

AK::IAkPluginParam* CreateArpweaverFmSourceParams(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, ArpweaverFmSourceParams());
}

AK_IMPLEMENT_PLUGIN_FACTORY(ArpweaverFmSource, AkPluginTypeSource, ArpweaverFmConfig::CompanyID, ArpweaverFmConfig::PluginID)

namespace
{
constexpr AkReal32 kTwoPi = 6.28318530717958647692f;
constexpr AkReal32 kInvQ24 = 1.0f / 16777216.0f;
constexpr AkReal32 kPhaseQ24Scale = 16777216.0f;

constexpr AkInt32 kDx7PitchModSensitivityTable[8] = {0, 10, 20, 33, 55, 92, 153, 255};
constexpr AkUInt32 kDx7AmpModSensitivityTable[4] = {0u, 4342338u, 7171437u, 16777216u};
constexpr AkUInt8 kDx7VelocityData[64] =
{
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163,
    166, 170, 173, 174, 178, 181, 184, 186, 189, 190, 194, 196, 198, 200, 202, 205,
    206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232, 233, 235,
    237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251, 252, 253, 254
};
constexpr AkInt32 kDx7CoarseMul[32] =
{
    -16777216, 0, 16777216, 26591258, 33554432, 38955489, 43368474, 47099600,
    50331648, 53182516, 55732705, 58039632, 60145690, 62083076, 63876816, 65546747,
    67108864, 68576247, 69959732, 71268397, 72509921, 73690858, 74816848, 75892776,
    76922906, 77910978, 78860292, 79773775, 80654032, 81503396, 82323963, 83117622
};
constexpr AkReal64 kDx7LfoRateSource[100] =
{
    0.062541, 0.125031, 0.312393, 0.437120, 0.624610, 0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
    1.560915, 1.752081, 1.875117, 2.062494, 2.247191, 2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
    3.188013, 3.369840, 3.500175, 3.682224, 3.812065, 4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
    4.814636, 4.930480, 5.121901, 5.315191, 5.434783, 5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
    6.431695, 6.564264, 6.749460, 6.868132, 7.052186, 7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
    7.993605, 8.181967, 8.372405, 8.504848, 8.685079, 8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
    9.607994, 9.798158, 9.950249, 10.117361, 11.251125, 11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
    16.366612, 16.638935, 17.869907, 19.193858, 19.425019, 20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
    25.746653, 27.173913, 27.578599, 29.052876, 30.693677, 31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
    38.197097, 38.550501, 40.387722, 40.749796, 42.625746, 44.326241, 44.883303, 46.772685, 48.590865, 49.261084
};

constexpr AkInt32 kDx7FreqLutSamples = 1 << 10;
constexpr AkInt32 kDx7FreqLutShift = 24 - 10;
constexpr AkInt32 kDx7FreqLutMaxLogInt = 20;
static std::array<AkInt32, kDx7FreqLutSamples + 1> g_dx7FreqLut{};
static std::array<AkInt32, (1 << 10) * 2> g_dx7SinLut{};
static std::array<AkInt32, (1 << 10) * 2> g_dx7Exp2Lut{};
static AkUInt32 g_dx7LookupSampleRate = 0u;
static bool g_dx7SinInit = false;
static bool g_dx7ExpInit = false;

struct ScaleDef
{
    const AkInt32* notes;
    AkUInt32 count;
};

struct Dx7AlgorithmDef
{
    AkUInt8 ops[ArpweaverFmDx7Runtime::kOperatorCount];
};

constexpr AkUInt32 kDx7StaticSamples[77] = {
    1764000u, 1764000u, 1411200u, 1411200u, 1190700u, 1014300u, 992250u,
    882000u, 705600u, 705600u, 584325u, 507150u, 502740u, 441000u, 418950u,
    352800u, 308700u, 286650u, 253575u, 220500u, 220500u, 176400u, 145530u,
    145530u, 125685u, 110250u, 110250u, 88200u, 88200u, 74970u, 61740u,
    61740u, 55125u, 48510u, 44100u, 37485u, 31311u, 30870u, 27562u, 27562u,
    22050u, 18522u, 17640u, 15435u, 14112u, 13230u, 11025u, 9261u, 9261u, 7717u,
    6615u, 6615u, 5512u, 5512u, 4410u, 3969u, 3969u, 3439u, 2866u, 2690u, 2249u,
    1984u, 1896u, 1808u, 1411u, 1367u, 1234u, 1146u, 926u, 837u, 837u, 705u,
    573u, 573u, 529u, 441u, 441u
};

constexpr AkUInt8 kDx7PitchEnvRate[100] = {
    1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
    12, 13, 13, 14, 14, 15, 16, 16, 17, 18, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47,
    49, 51, 53, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82,
    85, 88, 91, 94, 98, 102, 106, 110, 115, 120, 125, 130, 135, 141, 147,
    153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255
};

constexpr AkInt8 kDx7PitchEnvTab[100] = {
    -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43,
    -41, -39, -37, -35, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24,
    -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,
    -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73,
    82, 92, 103, 115, 127
};

constexpr AkUInt8 kExpScaleData[33] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
    80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250
};

constexpr AkInt32 kOutLevelLut[20] = {
    0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46
};

constexpr Dx7AlgorithmDef kDx7Algorithms[32] = {
    {{0xC1u, 0x11u, 0x11u, 0x14u, 0x01u, 0x14u}}, {{0x01u, 0x11u, 0x11u, 0x14u, 0xC1u, 0x14u}},
    {{0xC1u, 0x11u, 0x14u, 0x01u, 0x11u, 0x14u}}, {{0xC1u, 0x11u, 0x94u, 0x01u, 0x11u, 0x14u}},
    {{0xC1u, 0x14u, 0x01u, 0x14u, 0x01u, 0x14u}}, {{0xC1u, 0x94u, 0x01u, 0x14u, 0x01u, 0x14u}},
    {{0xC1u, 0x11u, 0x05u, 0x14u, 0x01u, 0x14u}}, {{0x01u, 0x11u, 0xC5u, 0x14u, 0x01u, 0x14u}},
    {{0x01u, 0x11u, 0x05u, 0x14u, 0xC1u, 0x14u}}, {{0x01u, 0x05u, 0x14u, 0xC1u, 0x11u, 0x14u}},
    {{0xC1u, 0x05u, 0x14u, 0x01u, 0x11u, 0x14u}}, {{0x01u, 0x05u, 0x05u, 0x14u, 0xC1u, 0x14u}},
    {{0xC1u, 0x05u, 0x05u, 0x14u, 0x01u, 0x14u}}, {{0xC1u, 0x05u, 0x11u, 0x14u, 0x01u, 0x14u}},
    {{0x01u, 0x05u, 0x11u, 0x14u, 0xC1u, 0x14u}}, {{0xC1u, 0x11u, 0x02u, 0x25u, 0x05u, 0x14u}},
    {{0x01u, 0x11u, 0x02u, 0x25u, 0xC5u, 0x14u}}, {{0x01u, 0x11u, 0x11u, 0xC5u, 0x05u, 0x14u}},
    {{0xC1u, 0x14u, 0x14u, 0x01u, 0x11u, 0x14u}}, {{0x01u, 0x05u, 0x14u, 0xC1u, 0x14u, 0x14u}},
    {{0x01u, 0x14u, 0x14u, 0xC1u, 0x14u, 0x14u}}, {{0xC1u, 0x14u, 0x14u, 0x14u, 0x01u, 0x14u}},
    {{0xC1u, 0x14u, 0x14u, 0x01u, 0x14u, 0x04u}}, {{0xC1u, 0x14u, 0x14u, 0x14u, 0x04u, 0x04u}},
    {{0xC1u, 0x14u, 0x14u, 0x04u, 0x04u, 0x04u}}, {{0xC1u, 0x05u, 0x14u, 0x01u, 0x14u, 0x04u}},
    {{0x01u, 0x05u, 0x14u, 0xC1u, 0x14u, 0x04u}}, {{0x04u, 0xC1u, 0x11u, 0x14u, 0x01u, 0x14u}},
    {{0xC1u, 0x14u, 0x01u, 0x14u, 0x04u, 0x04u}}, {{0x04u, 0xC1u, 0x11u, 0x14u, 0x04u, 0x04u}},
    {{0xC1u, 0x14u, 0x04u, 0x04u, 0x04u, 0x04u}}, {{0xC4u, 0x04u, 0x04u, 0x04u, 0x04u, 0x04u}}
};

AkReal32 ReadCircularLinear(const std::vector<AkReal32>& in_buffer, AkReal32 in_readIndex)
{
    if (in_buffer.empty())
        return 0.0f;
    const AkReal32 sizeF = static_cast<AkReal32>(in_buffer.size());
    while (in_readIndex < 0.0f)
        in_readIndex += sizeF;
    while (in_readIndex >= sizeF)
        in_readIndex -= sizeF;
    const AkUInt32 i0 = static_cast<AkUInt32>(in_readIndex);
    const AkUInt32 i1 = (i0 + 1u) % static_cast<AkUInt32>(in_buffer.size());
    const AkReal32 frac = in_readIndex - static_cast<AkReal32>(i0);
    return in_buffer[i0] + (in_buffer[i1] - in_buffer[i0]) * frac;
}

ScaleDef GetScale(AkInt32 in_mode)
{
    static const AkInt32 kIonian[] = {0, 2, 4, 5, 7, 9, 11};
    static const AkInt32 kAeolian[] = {0, 2, 3, 5, 7, 8, 10};
    static const AkInt32 kDorian[] = {0, 2, 3, 5, 7, 9, 10};
    static const AkInt32 kPhrygian[] = {0, 1, 3, 5, 7, 8, 10};
    static const AkInt32 kLydian[] = {0, 2, 4, 6, 7, 9, 11};
    static const AkInt32 kMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
    static const AkInt32 kLocrian[] = {0, 1, 3, 5, 6, 8, 10};
    static const AkInt32 kHarmonicMinor[] = {0, 2, 3, 5, 7, 8, 11};
    static const AkInt32 kMelodicMinor[] = {0, 2, 3, 5, 7, 9, 11};
    static const AkInt32 kHarmonicMajor[] = {0, 2, 4, 5, 7, 8, 11};
    static const AkInt32 kPentaMajor[] = {0, 2, 4, 7, 9};
    static const AkInt32 kPentaMinor[] = {0, 3, 5, 7, 10};
    static const AkInt32 kBluesMinor[] = {0, 3, 5, 6, 7, 10};
    static const AkInt32 kBluesMajor[] = {0, 2, 3, 4, 7, 9};
    static const AkInt32 kWholeTone[] = {0, 2, 4, 6, 8, 10};
    static const AkInt32 kDimHalfWhole[] = {0, 1, 3, 4, 6, 7, 9, 10};
    static const AkInt32 kDimWholeHalf[] = {0, 2, 3, 5, 6, 8, 9, 11};
    static const AkInt32 kChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    switch (in_mode)
    {
    case 0: return {kIonian, 7}; case 1: return {kAeolian, 7}; case 2: return {kDorian, 7}; case 3: return {kPhrygian, 7};
    case 4: return {kLydian, 7}; case 5: return {kMixolydian, 7}; case 6: return {kLocrian, 7}; case 7: return {kHarmonicMinor, 7};
    case 8: return {kMelodicMinor, 7}; case 9: return {kHarmonicMajor, 7}; case 10: return {kPentaMajor, 5}; case 11: return {kPentaMinor, 5};
    case 12: return {kBluesMinor, 6}; case 13: return {kBluesMajor, 6}; case 14: return {kWholeTone, 6}; case 15: return {kDimHalfWhole, 8};
    case 16: return {kDimWholeHalf, 8}; case 17: return {kChromatic, 12}; default: return {kIonian, 7};
    }
}

bool IsPitchClassInScale(AkInt32 in_pitchClass, AkInt32 in_scaleMode)
{
    const ScaleDef scale = GetScale(in_scaleMode);
    for (AkUInt32 i = 0; i < scale.count; ++i)
    {
        if (scale.notes[i] == in_pitchClass)
            return true;
    }
    return false;
}

AkUInt32 CountDx7Carriers(AkInt32 in_algorithm)
{
    const Dx7AlgorithmDef& algorithm = kDx7Algorithms[static_cast<size_t>((std::max)(0, (std::min)(in_algorithm, 31)))];
    AkUInt32 count = 0u;
    for (AkUInt8 flags : algorithm.ops)
    {
        if ((flags & 0x07u) == 0x04u)
            ++count;
    }
    return (std::max)(static_cast<AkUInt32>(1u), count);
}

AkInt32 GetScaleDegreeOffset(AkInt32 in_scaleMode, AkInt32 in_degreeIndex)
{
    const ScaleDef scale = GetScale(in_scaleMode);
    if (scale.count == 0u)
        return 0;

    const AkInt32 wrappedDegree = (std::max)(0, in_degreeIndex);
    const AkInt32 octave = wrappedDegree / static_cast<AkInt32>(scale.count);
    const AkInt32 degree = wrappedDegree % static_cast<AkInt32>(scale.count);
    return scale.notes[degree] + octave * 12;
}

AkInt32 ResolveChordDegreeRootOffset(AkInt32 in_degreePath, AkUInt32 in_pulseIndex, AkInt32 in_scaleMode)
{
    static const std::array<AkInt32, 1> kStaticTonic = {0};
    static const std::array<AkInt32, 7> kCircleProgression = {0, 3, 6, 2, 5, 1, 4};
    static const std::array<AkInt32, 4> kPopProgression = {0, 5, 1, 4};
    static const std::array<AkInt32, 4> kMinorLift = {0, 5, 2, 6};
    static const std::array<AkInt32, 8> kCadenceWalk = {0, 3, 4, 0, 5, 1, 4, 0};

    const AkInt32 safePath = (std::max)(0, (std::min)(in_degreePath, 4));
    const AkInt32 degreeIndex =
        (safePath == 1) ? kCircleProgression[in_pulseIndex % kCircleProgression.size()] :
        (safePath == 2) ? kPopProgression[in_pulseIndex % kPopProgression.size()] :
        (safePath == 3) ? kMinorLift[in_pulseIndex % kMinorLift.size()] :
        (safePath == 4) ? kCadenceWalk[in_pulseIndex % kCadenceWalk.size()] :
                          kStaticTonic[0];
    return GetScaleDegreeOffset(in_scaleMode, degreeIndex);
}

AkUInt32 BuildChordIntervals(AkInt32 in_mode, AkInt32 in_inversion, std::array<AkInt32, 4>& out_intervals)
{
    out_intervals.fill(0);

    static const std::array<AkInt32, 4> kModes[12] =
    {
        {0, 4, 7, 12},   {0, 3, 7, 12},   {0, 2, 7, 12},   {0, 5, 7, 12},
        {0, 4, 7, 11},   {0, 3, 7, 10},   {0, 4, 7, 10},   {0, 3, 6, 9},
        {0, 4, 8, 12},   {0, 7, 12, 19},  {0, 4, 7, 14},   {0, 3, 10, 14}
    };

    const AkInt32 safeMode = (std::max)(0, (std::min)(in_mode, 11));
    out_intervals = kModes[safeMode];

    AkUInt32 count = 4u;
    while (count > 1u && out_intervals[count - 1u] == out_intervals[count - 2u])
        --count;

    const AkInt32 safeInversion = (std::max)(0, (std::min)(in_inversion, static_cast<AkInt32>(count) - 1));
    for (AkInt32 i = 0; i < safeInversion; ++i)
        out_intervals[static_cast<size_t>(i)] += 12;

    std::sort(out_intervals.begin(), out_intervals.begin() + count);
    return count;
}

AkUInt32 CollectEnabledSequencerSteps(const ArpweaverFmRTPCParams& in_params, AkInt32 in_stepCount, std::array<AkInt32, 16>& out_steps)
{
    out_steps.fill(0);
    AkUInt32 enabledCount = 0u;
    const AkInt32 safeStepCount = (std::max)(1, (std::min)(in_stepCount, 16));
    for (AkInt32 step = 0; step < safeStepCount; ++step)
    {
        const bool enabled = (step >= 8) ? true : in_params.SeqSteps[static_cast<size_t>(step)];
        if (enabled)
            out_steps[enabledCount++] = step;
    }

    if (enabledCount == 0u)
    {
        out_steps[0] = 0;
        enabledCount = 1u;
    }
    return enabledCount;
}

AkInt32 SequenceOrderIndex(AkUInt32 in_step, AkInt32 in_stepCount, AkInt32 in_mode)
{
    if (in_stepCount <= 1)
        return 0;

    const AkInt32 safeMode = (std::max)(0, (std::min)(in_mode, 6));
    const AkInt32 step = static_cast<AkInt32>(in_step);

    switch (safeMode)
    {
    case 0:
        return step % in_stepCount;

    case 1:
        return in_stepCount - 1 - (step % in_stepCount);

    case 2:
        return -1;

    case 3:
    {
        const AkInt32 cycle = in_stepCount * 2 - 2;
        const AkInt32 pos = step % cycle;
        return (pos < in_stepCount) ? pos : (cycle - pos);
    }

    case 4:
        return (step % 2 == 0) ? (step / 2) % in_stepCount : (in_stepCount - 1 - ((step / 2) % in_stepCount));

    case 5:
    {
        const AkInt32 centerLeft = (in_stepCount - 1) / 2;
        const AkInt32 centerRight = in_stepCount / 2;
        const AkInt32 pos = step % in_stepCount;
        if (pos == 0)
            return centerLeft;
        if (pos == 1)
            return centerRight;
        const AkInt32 wing = (pos - 2) / 2 + 1;
        return ((pos & 1) == 0) ? (centerLeft - wing) : (centerRight + wing);
    }

    case 6:
    default:
    {
        static const AkInt32 kOrder[16] = {0, 3, 1, 4, 2, 5, 7, 6, 8, 10, 9, 11, 13, 12, 15, 14};
        return kOrder[step % in_stepCount] % in_stepCount;
    }
    }
}

AkUInt32 GetRhythmPatternLength(AkInt32 in_pattern)
{
    switch ((std::max)(0, (std::min)(in_pattern, 4)))
    {
    case 1: return 2u;
    case 2: return 4u;
    case 3: return 3u;
    case 4: return 4u;
    default: return 1u;
    }
}

AkReal32 GetRhythmPatternMultiplier(AkInt32 in_pattern, AkUInt32 in_pulse)
{
    switch ((std::max)(0, (std::min)(in_pattern, 4)))
    {
    case 1:
    {
        static const AkReal32 kPattern[2] = {0.5f, 1.5f};
        return kPattern[in_pulse % 2u];
    }
    case 2:
    {
        static const AkReal32 kPattern[4] = {0.75f, 0.25f, 0.75f, 0.25f};
        return kPattern[in_pulse % 4u];
    }
    case 3:
    {
        static const AkReal32 kPattern[3] = {1.5f, 1.5f, 1.0f};
        return kPattern[in_pulse % 3u];
    }
    case 4:
    {
        static const AkReal32 kPattern[4] = {1.0f, 0.5f, 1.5f, 1.0f};
        return kPattern[in_pulse % 4u];
    }
    default:
        return 1.0f;
    }
}

AkUInt32 ComputeSequencerStepSamples(AkUInt32 in_sampleRate, AkReal32 in_arpRate, AkReal32 in_bpm, AkInt32 in_seqPattern, AkUInt32 in_pulseIndex)
{
    const AkReal32 safeRate = (std::max)(0.25f, (std::min)(in_arpRate, 24.0f));
    const AkReal32 safeBpm = (std::max)(20.0f, (std::min)(in_bpm, 240.0f));
    const AkUInt32 patternLength = GetRhythmPatternLength(in_seqPattern);
    const AkReal32 effectiveRate = safeRate * (safeBpm / 60.0f);
    AkUInt32 stepSamples = static_cast<AkUInt32>(
        static_cast<AkReal32>(in_sampleRate) * GetRhythmPatternMultiplier(in_seqPattern, in_pulseIndex % patternLength) / effectiveRate);
    return (std::max)(1u, stepSamples);
}
} // namespace

ArpweaverFmSource::ArpweaverFmSource() = default;

AkReal32 ArpweaverFmSource::Clamp(AkReal32 in_v, AkReal32 in_lo, AkReal32 in_hi)
{
    if (in_v < in_lo)
        return in_lo;
    if (in_v > in_hi)
        return in_hi;
    return in_v;
}

AkInt32 ArpweaverFmSource::ClampI(AkInt32 in_v, AkInt32 in_lo, AkInt32 in_hi)
{
    if (in_v < in_lo)
        return in_lo;
    if (in_v > in_hi)
        return in_hi;
    return in_v;
}

AkInt32 ArpweaverFmSource::Mod12(AkInt32 in_x)
{
    AkInt32 m = in_x % 12;
    return (m < 0) ? (m + 12) : m;
}
AKRESULT ArpweaverFmSource::Init(
    AK::IAkPluginMemAlloc* /*in_pAllocator*/,
    AK::IAkSourcePluginContext* /*in_pSourcePluginContext*/,
    AK::IAkPluginParam* in_pParams,
    AkAudioFormat& io_rFormat)
{
    m_params = static_cast<ArpweaverFmSourceParams*>(in_pParams);
    if (io_rFormat.uSampleRate == 0)
        io_rFormat.uSampleRate = 48000;
    io_rFormat.channelConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
    m_sampleRate = io_rFormat.uSampleRate;
    m_lpState = 0.0f;
    m_currentFreq = 261.6256f;
    m_currentMidi = 60;
    m_arpIndex = 0;
    m_seqPulseIndex = 0;
    m_samplesUntilNextStep = 0;
    m_currentStepLengthSamples = 0;
    m_currentGateSamplesRemaining = 0;
    m_rngState = 0xA341316Cu;
    InitializeFxState();
    UpdateCurrentNote(true);
    return AK_Success;
}

AKRESULT ArpweaverFmSource::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
    AK_PLUGIN_DELETE(in_pAllocator, this);
    return AK_Success;
}

AKRESULT ArpweaverFmSource::Reset()
{
    m_lpState = 0.0f;
    m_arpIndex = 0;
    m_seqPulseIndex = 0;
    m_samplesUntilNextStep = 0;
    m_currentStepLengthSamples = 0;
    m_currentGateSamplesRemaining = 0;
    InitializeFxState();
    UpdateCurrentNote(true);
    return AK_Success;
}

AKRESULT ArpweaverFmSource::GetPluginInfo(AkPluginInfo& out_rPluginInfo)
{
    out_rPluginInfo.eType = AkPluginTypeSource;
    out_rPluginInfo.bIsInPlace = true;
    out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
    return AK_Success;
}

AkReal32 ArpweaverFmSource::GetDuration() const
{
#ifdef AK_INFINITE_DURATION
    return static_cast<AkReal32>(AK_INFINITE_DURATION);
#else
    return 2147483647.0f;
#endif
}

AKRESULT ArpweaverFmSource::TimeSkip(AkUInt32& io_uFrames)
{
    if (!m_params)
        return AK_DataReady;
    const ArpweaverFmRTPCParams& p = m_params->GetParams();
    const bool sequencerGateEnabled = p.ArpOn && !p.ChordOn;
    const bool clockEnabled = sequencerGateEnabled || p.ChordOn;
    if (!clockEnabled)
        return AK_DataReady;
    AkUInt32 stepSamples = ComputeSequencerStepSamples(m_sampleRate, p.ArpRate, p.Bpm, p.SeqPattern, m_seqPulseIndex);
    AkUInt32 frames = io_uFrames;
    while (frames > 0)
    {
        if (m_samplesUntilNextStep == 0)
        {
            UpdateCurrentNote(false);
            m_currentStepLengthSamples = stepSamples;
            m_currentGateSamplesRemaining = (std::max)(1u, static_cast<AkUInt32>(static_cast<AkReal32>(stepSamples) * Clamp(p.SeqGate, 0.05f, 1.0f)));
            ++m_seqPulseIndex;
            stepSamples = ComputeSequencerStepSamples(m_sampleRate, p.ArpRate, p.Bpm, p.SeqPattern, m_seqPulseIndex);
            m_samplesUntilNextStep = stepSamples;
        }
        const AkUInt32 consume = (frames < m_samplesUntilNextStep) ? frames : m_samplesUntilNextStep;
        frames -= consume;
        m_samplesUntilNextStep -= consume;
        if (m_currentGateSamplesRemaining > consume)
            m_currentGateSamplesRemaining -= consume;
        else
            m_currentGateSamplesRemaining = 0u;
    }
    return AK_DataReady;
}

void ArpweaverFmSource::Execute(AkAudioBuffer* io_pBuffer)
{
    if (!io_pBuffer || !m_params)
        return;
    const ArpweaverFmRTPCParams& p = m_params->GetParams();
    const bool useDx7Runtime = p.UseSysexPatch && p.HasDx7PatchData;
    io_pBuffer->uValidFrames = static_cast<AkUInt16>(io_pBuffer->MaxFrames());
    const AkUInt32 frameCount = io_pBuffer->uValidFrames;
    const AkUInt32 numChannels = io_pBuffer->NumChannels();
    if (frameCount == 0 || numChannels == 0)
    {
        io_pBuffer->eState = AK_DataReady;
        return;
    }
    const AkReal32 brightness = Clamp(p.Brightness * 0.65f + p.FmBrightnessBias * 0.35f, 0.0f, 1.0f);
    const AkReal32 volume = Clamp(p.Volume, 0.0f, 1.0f);
    const AkReal32 alpha = ComputeLowPassAlpha(brightness, m_sampleRate);
    const bool sequencerGateEnabled = p.ArpOn && !p.ChordOn;
    const bool clockEnabled = sequencerGateEnabled || p.ChordOn;
    AkUInt32 stepSamples = ComputeSequencerStepSamples(m_sampleRate, p.ArpRate, p.Bpm, p.SeqPattern, m_seqPulseIndex);
    if (!clockEnabled)
        UpdateCurrentNote(false);
    AkSampleType* outCh0 = io_pBuffer->GetChannel(0);
    AkSampleType* outCh1 = (numChannels > 1) ? io_pBuffer->GetChannel(1) : nullptr;
    for (AkUInt32 i = 0; i < frameCount; ++i)
    {
        if (clockEnabled)
        {
            if (m_samplesUntilNextStep == 0)
            {
                UpdateCurrentNote(false);
                m_currentStepLengthSamples = stepSamples;
                m_currentGateSamplesRemaining = (std::max)(1u, static_cast<AkUInt32>(static_cast<AkReal32>(stepSamples) * Clamp(p.SeqGate, 0.05f, 1.0f)));
                ++m_seqPulseIndex;
                stepSamples = ComputeSequencerStepSamples(m_sampleRate, p.ArpRate, p.Bpm, p.SeqPattern, m_seqPulseIndex);
                m_samplesUntilNextStep = stepSamples;
            }
            --m_samplesUntilNextStep;
            if (m_currentGateSamplesRemaining > 0u)
                --m_currentGateSamplesRemaining;
        }
        const AkUInt32 voices = (std::max)(1u, m_activeVoiceCount);
        AkReal32 synth = 0.0f;
        for (AkUInt32 voice = 0; voice < voices; ++voice)
            synth += useDx7Runtime ? RenderDx7Voice(voice, p) : RenderManualFmVoice(voice, m_voiceBaseFreq[voice], p);
        synth /= static_cast<AkReal32>(voices);
        if (sequencerGateEnabled && m_currentGateSamplesRemaining == 0u)
            synth = 0.0f;
        m_lpState += alpha * (synth - m_lpState);
        const AkReal32 dry = m_lpState * volume;
        AkReal32 left = dry;
        AkReal32 right = dry;
        ProcessChorus(dry, p, left, right);
        ProcessDelay(left, right, p, left, right);
        ProcessReverb(left, right, p, left, right);
        outCh0[i] = AK_FLOAT_TO_SAMPLETYPE(left);
        if (outCh1)
            outCh1[i] = AK_FLOAT_TO_SAMPLETYPE(right);
        if (numChannels > 2)
        {
            const AkSampleType mid = AK_FLOAT_TO_SAMPLETYPE(0.5f * (left + right));
            for (AkUInt32 ch = 2; ch < numChannels; ++ch)
                io_pBuffer->GetChannel(ch)[i] = mid;
        }
    }
    io_pBuffer->eState = AK_DataReady;
}

void ArpweaverFmSource::InitializeFxState()
{
    m_carrierPhase.fill(0.0f);
    m_modPhase.fill(0.0f);
    m_ampEnvelope.fill(0.0f);
    m_modEnvelope.fill(0.0f);
    m_ampEnvelopeStage.fill(kEnvAttackStage);
    m_modEnvelopeStage.fill(kEnvAttackStage);
    m_feedbackState.fill(0.0f);
    InitializeDx7State();
    const AkUInt32 chorusSize = (std::max)(static_cast<AkUInt32>(128u), static_cast<AkUInt32>(m_sampleRate * 0.06f));
    m_chorusBuffer.assign(static_cast<size_t>(chorusSize), 0.0f);
    m_chorusWrite = 0;
    m_chorusLfoPhase = 0.0f;
    const AkUInt32 delaySize = (std::max)(static_cast<AkUInt32>(512u), static_cast<AkUInt32>(m_sampleRate * 2.5f));
    m_delayBufferL.assign(static_cast<size_t>(delaySize), 0.0f);
    m_delayBufferR.assign(static_cast<size_t>(delaySize), 0.0f);
    m_delayWrite = 0;
    static const AkReal32 kReverbSeconds[kReverbLines] = {0.0297f, 0.0371f, 0.0411f};
    for (AkUInt32 i = 0; i < kReverbLines; ++i)
    {
        const AkUInt32 lenL = (std::max)(static_cast<AkUInt32>(32u), static_cast<AkUInt32>(m_sampleRate * kReverbSeconds[i]) + 1u);
        const AkUInt32 lenR = lenL + (11u * (i + 1u));
        m_reverbBufferL[i].assign(static_cast<size_t>(lenL), 0.0f);
        m_reverbBufferR[i].assign(static_cast<size_t>(lenR), 0.0f);
        m_reverbWriteL[i] = 0;
        m_reverbWriteR[i] = 0;
    }
    m_reverbDampStateL = 0.0f;
    m_reverbDampStateR = 0.0f;
}

void ArpweaverFmSource::InitializeDx7State()
{
    InitDx7LookupTables(m_sampleRate);
    for (auto& phaseSet : m_dx7PhaseQ24)
        phaseSet.fill(0);
    for (auto& freqSet : m_dx7BaseLogFreq)
        freqSet.fill(0);
    for (auto& fbSet : m_dx7FeedbackHistoryA)
        fbSet.fill(0);
    for (auto& fbSet : m_dx7FeedbackHistoryB)
        fbSet.fill(0);
    m_voiceMidi.fill(60);
    m_voiceBaseFreq.fill(261.6256f);
    m_voiceDetuneSemi.fill(0.0f);
    m_activeVoiceCount = 1u;
    for (auto& envSet : m_dx7OpEnv)
    {
        for (auto& env : envSet)
            env = Dx7EnvelopeState{};
    }
    for (auto& env : m_dx7PitchEnv)
        env = Dx7PitchEnvelopeState{};
    for (auto& lfo : m_dx7Lfo)
        lfo = Dx7LfoState{};
}

AkReal32 ArpweaverFmSource::MidiToFrequency(AkReal32 in_midiNote)
{
    return 440.0f * std::pow(2.0f, (in_midiNote - 69.0f) / 12.0f);
}

void ArpweaverFmSource::InitDx7LookupTables(AkUInt32 in_sampleRate)
{
    if (!g_dx7SinInit)
    {
        const double dphase = 2.0 * 3.14159265358979323846 / 1024.0;
        AkInt32 u = 1 << 30;
        AkInt32 v = 0;
        const AkInt32 c = static_cast<AkInt32>(std::floor(std::cos(dphase) * static_cast<double>(1 << 30) + 0.5));
        const AkInt32 s = static_cast<AkInt32>(std::floor(std::sin(dphase) * static_cast<double>(1 << 30) + 0.5));
        const AkInt64 r = 1ll << 29;
        for (int i = 0; i < 512; ++i)
        {
            g_dx7SinLut[(i << 1) + 1] = (v + 32) >> 6;
            g_dx7SinLut[((i + 512) << 1) + 1] = -((v + 32) >> 6);
            const AkInt32 t = static_cast<AkInt32>(((static_cast<AkInt64>(u) * s) + (static_cast<AkInt64>(v) * c) + r) >> 30);
            u = static_cast<AkInt32>(((static_cast<AkInt64>(u) * c) - (static_cast<AkInt64>(v) * s) + r) >> 30);
            v = t;
        }
        for (int i = 0; i < 1023; ++i)
            g_dx7SinLut[i << 1] = g_dx7SinLut[(i << 1) + 3] - g_dx7SinLut[(i << 1) + 1];
        g_dx7SinLut[(1024 << 1) - 2] = -g_dx7SinLut[(1024 << 1) - 1];
        g_dx7SinInit = true;
    }

    if (!g_dx7ExpInit)
    {
        double y = static_cast<double>(1 << 30);
        const double inc = std::pow(2.0, 1.0 / 1024.0);
        for (int i = 0; i < 1024; ++i)
        {
            g_dx7Exp2Lut[(i << 1) + 1] = static_cast<AkInt32>(std::floor(y + 0.5));
            y *= inc;
        }
        for (int i = 0; i < 1023; ++i)
            g_dx7Exp2Lut[i << 1] = g_dx7Exp2Lut[(i << 1) + 3] - g_dx7Exp2Lut[(i << 1) + 1];
        g_dx7Exp2Lut[(1024 << 1) - 2] = static_cast<AkInt32>((1u << 31) - static_cast<AkUInt32>(g_dx7Exp2Lut[(1024 << 1) - 1]));
        g_dx7ExpInit = true;
    }

    if (g_dx7LookupSampleRate == in_sampleRate)
        return;

    double y = std::ldexp(1.0, 24 + kDx7FreqLutMaxLogInt) / static_cast<double>((std::max)(in_sampleRate, 1u));
    const double inc = std::pow(2.0, 1.0 / static_cast<double>(kDx7FreqLutSamples));
    for (int i = 0; i < kDx7FreqLutSamples + 1; ++i)
    {
        g_dx7FreqLut[static_cast<size_t>(i)] = static_cast<AkInt32>(std::floor(y + 0.5));
        y *= inc;
    }
    g_dx7LookupSampleRate = in_sampleRate;
}

AkInt32 ArpweaverFmSource::Dx7SinLookupQ24(AkInt32 in_phaseQ24)
{
    const int shift = 24 - 10;
    const AkInt32 lowbits = in_phaseQ24 & ((1 << shift) - 1);
    const AkInt32 phaseInt = (in_phaseQ24 >> (shift - 1)) & ((1024 - 1) << 1);
    const AkInt32 dy = g_dx7SinLut[static_cast<size_t>(phaseInt)];
    const AkInt32 y0 = g_dx7SinLut[static_cast<size_t>(phaseInt + 1)];
    return y0 + static_cast<AkInt32>((static_cast<AkInt64>(dy) * static_cast<AkInt64>(lowbits)) >> shift);
}

AkInt32 ArpweaverFmSource::Dx7Exp2LookupQ24(AkInt32 in_x)
{
    const int shift = 24 - 10;
    const AkInt32 lowbits = in_x & ((1 << shift) - 1);
    const AkInt32 xInt = (in_x >> (shift - 1)) & ((1024 - 1) << 1);
    const AkInt32 dy = g_dx7Exp2Lut[static_cast<size_t>(xInt)];
    const AkInt32 y0 = g_dx7Exp2Lut[static_cast<size_t>(xInt + 1)];
    const AkInt32 y = y0 + static_cast<AkInt32>((static_cast<AkInt64>(dy) * static_cast<AkInt64>(lowbits)) >> shift);
    return y >> (6 - (in_x >> 24));
}

AkInt32 ArpweaverFmSource::Dx7FreqLookup(AkInt32 in_logFreqQ24)
{
    const AkInt32 ix = (in_logFreqQ24 & 0xFFFFFF) >> kDx7FreqLutShift;
    const AkInt32 y0 = g_dx7FreqLut[static_cast<size_t>(ix)];
    const AkInt32 y1 = g_dx7FreqLut[static_cast<size_t>(ix + 1)];
    const AkInt32 lowbits = in_logFreqQ24 & ((1 << kDx7FreqLutShift) - 1);
    const AkInt32 y = y0 + static_cast<AkInt32>((static_cast<AkInt64>(y1 - y0) * static_cast<AkInt64>(lowbits)) >> kDx7FreqLutShift);
    const AkInt32 hibits = in_logFreqQ24 >> 24;
    return y >> (kDx7FreqLutMaxLogInt - hibits);
}

AkInt32 ArpweaverFmSource::ComputeDx7LogFrequency(AkInt32 in_midiNote, AkUInt8 in_mode, AkUInt8 in_coarse, AkUInt8 in_fine, AkUInt8 in_detune)
{
    if (in_mode == 0u)
    {
        const double base = (std::log(440.0) / std::log(2.0) - 69.0 / 12.0) * static_cast<double>(1 << 24);
        AkInt32 logfreq = static_cast<AkInt32>(std::floor(base + 0.5 + static_cast<double>(in_midiNote) * static_cast<double>(1 << 24) / 12.0));
        const AkInt32 coarse = ClampI(in_coarse, 0, 31);
        const AkInt32 fine = ClampI(in_fine, 0, 99);
        const AkInt32 detune = ClampI(in_detune, 0, 14);
        const double detuneRatio = 0.0209 * std::exp(-0.396 * (static_cast<double>(logfreq) / static_cast<double>(1 << 24))) / 7.0;
        logfreq += static_cast<AkInt32>(detuneRatio * static_cast<double>(logfreq) * static_cast<double>(detune - 7));
        logfreq += kDx7CoarseMul[static_cast<size_t>(coarse)];
        if (fine > 0)
            logfreq += static_cast<AkInt32>(std::floor(24204406.323123 * std::log(1.0 + 0.01 * fine) + 0.5));
        return logfreq;
    }

    AkInt32 logfreq = (4458616 * (((ClampI(in_coarse, 0, 31) & 3) * 100) + ClampI(in_fine, 0, 99))) >> 3;
    if (ClampI(in_detune, 0, 14) > 7)
        logfreq += 13457 * (ClampI(in_detune, 0, 14) - 7);
    return logfreq;
}

AkUInt32 ArpweaverFmSource::NextRandom(AkUInt32& io_state)
{
    AkUInt32 x = io_state;
    x ^= (x << 13);
    x ^= (x >> 17);
    x ^= (x << 5);
    io_state = x;
    return x;
}
AkInt32 ArpweaverFmSource::QuantizeToScale(AkInt32 in_midiNote, AkInt32 in_rootNote, AkInt32 in_scaleMode)
{
    in_rootNote = Mod12(in_rootNote);
    AkInt32 best = in_midiNote;
    AkInt32 bestDist = 9999;
    for (AkInt32 n = in_midiNote - 36; n <= in_midiNote + 36; ++n)
    {
        const AkInt32 pitchClass = Mod12(n - in_rootNote);
        if (!IsPitchClassInScale(pitchClass, in_scaleMode))
            continue;
        const AkInt32 dist = std::abs(n - in_midiNote);
        if (dist < bestDist || (dist == bestDist && n > best))
        {
            bestDist = dist;
            best = n;
        }
    }
    return best;
}

AkReal32 ArpweaverFmSource::ComputeLowPassAlpha(AkReal32 in_brightness, AkUInt32 in_sampleRate)
{
    in_brightness = Clamp(in_brightness, 0.0f, 1.0f);
    const AkReal32 cutoff = 60.0f + in_brightness * (14000.0f - 60.0f);
    const AkReal32 omega = kTwoPi * cutoff / static_cast<AkReal32>(in_sampleRate);
    return Clamp(1.0f - std::exp(-omega), 0.0001f, 1.0f);
}

AkInt32 ArpweaverFmSource::ScaleOutLevel(AkInt32 in_outLevel)
{
    const AkInt32 clamped = ClampI(in_outLevel, 0, 99);
    return (clamped >= 20) ? (28 + clamped) : kOutLevelLut[clamped];
}

AkInt32 ArpweaverFmSource::ScaleVelocity(AkInt32 in_velocity, AkInt32 in_sensitivity)
{
    const AkInt32 clampedVelocity = ClampI(in_velocity, 0, 127);
    const AkInt32 velocityValue = static_cast<AkInt32>(kDx7VelocityData[static_cast<size_t>(clampedVelocity >> 1)]) - 239;
    return (((ClampI(in_sensitivity, 0, 7) * velocityValue) + 7) >> 3) << 4;
}

AkInt32 ArpweaverFmSource::ScaleRate(AkInt32 in_midiNote, AkInt32 in_sensitivity)
{
    const AkInt32 x = (std::min)(static_cast<AkInt32>(31), (std::max)(static_cast<AkInt32>(0), in_midiNote / 3 - 7));
    return (ClampI(in_sensitivity, 0, 7) * x) >> 3;
}

AkInt32 ArpweaverFmSource::ScaleCurve(AkInt32 in_group, AkInt32 in_depth, AkInt32 in_curve)
{
    AkInt32 scale = 0;
    if (in_curve == 0 || in_curve == 3)
        scale = (in_group * in_depth * 329) >> 12;
    else
        scale = (static_cast<AkInt32>(kExpScaleData[static_cast<size_t>((std::min)(in_group, static_cast<AkInt32>(32)))]) * in_depth * 329) >> 15;
    if (in_curve < 2)
        scale = -scale;
    return scale;
}

AkInt32 ArpweaverFmSource::ScaleLevel(AkInt32 in_midiNote, AkInt32 in_breakPoint, AkInt32 in_leftDepth, AkInt32 in_rightDepth, AkInt32 in_leftCurve, AkInt32 in_rightCurve)
{
    const AkInt32 offset = in_midiNote - in_breakPoint - 17;
    if (offset >= 0)
        return ScaleCurve((offset + 1) / 3, in_rightDepth, in_rightCurve);
    return ScaleCurve(-(offset - 1) / 3, in_leftDepth, in_leftCurve);
}

AkReal32 ArpweaverFmSource::ComputeDx7Ratio(AkUInt8 in_mode, AkUInt8 in_coarse, AkUInt8 in_fine)
{
    if (in_mode != 0u)
        return 1.0f;
    const AkInt32 coarse = ClampI(in_coarse, 0, 31);
    const AkInt32 fine = ClampI(in_fine, 0, 99);
    if (coarse == 0)
        return 0.5f + static_cast<AkReal32>(fine) / 200.0f;
    return static_cast<AkReal32>(coarse) + static_cast<AkReal32>(fine) / 100.0f;
}

AkReal32 ArpweaverFmSource::ComputeDx7FixedHz(AkUInt8 in_coarse, AkUInt8 in_fine)
{
    const AkInt32 coarse = ClampI(in_coarse, 0, 31);
    const AkInt32 fine = ClampI(in_fine, 0, 99);
    return std::pow(10.0f, (static_cast<AkReal32>((coarse & 3) * 100 + fine)) * 0.01f);
}

AkReal32 ArpweaverFmSource::ComputeDx7DetuneMultiplier(AkInt32 in_detune, AkReal32 in_referenceHz)
{
    const AkReal32 refHz = (std::max)(in_referenceHz, 1.0f);
    const AkReal32 log2Ref = std::log(refHz) / std::log(2.0f);
    const AkReal32 detuneRatio = 0.0209f * std::exp(-0.396f * log2Ref) / 7.0f;
    const AkReal32 offset = detuneRatio * log2Ref * static_cast<AkReal32>(ClampI(in_detune, 0, 14) - 7);
    return std::pow(2.0f, offset);
}

void ArpweaverFmSource::StartDx7Envelope(Dx7EnvelopeState& io_state, AkUInt32 in_sampleRate, AkUInt8 in_stage)
{
    io_state.stage = in_stage;
    if (in_stage >= ArpweaverFmDx7Runtime::kEnvelopePointCount)
    {
        io_state.active = false;
        io_state.incrementQ24 = 0.0f;
        io_state.holdSamples = 0u;
        return;
    }
    const AkInt32 newLevel = io_state.levels[in_stage];
    AkInt32 actualLevel = ScaleOutLevel(newLevel) >> 1;
    actualLevel = (actualLevel << 6) + io_state.outLevel - 4256;
    if (actualLevel < 16)
        actualLevel = 16;
    io_state.targetQ24 = static_cast<AkReal32>(actualLevel << 16);
    io_state.rising = (io_state.targetQ24 > io_state.levelQ24);
    AkInt32 qRate = (io_state.rates[in_stage] * 41) >> 6;
    qRate += io_state.rateScaling;
    qRate = ClampI(qRate, 0, 63);
    const AkInt32 inc = (4 + (qRate & 3)) << (2 + (qRate >> 2));
    io_state.incrementQ24 = static_cast<AkReal32>(inc) * (44100.0f / static_cast<AkReal32>(in_sampleRate));
    io_state.holdSamples = 0u;
    if (io_state.targetQ24 == io_state.levelQ24 || (in_stage == 0u && newLevel == 0))
    {
        AkInt32 staticRate = ClampI(io_state.rates[in_stage] + io_state.rateScaling, 0, 99);
        AkUInt32 hold = (staticRate < 77) ? kDx7StaticSamples[static_cast<size_t>(staticRate)] : static_cast<AkUInt32>(20 * (99 - staticRate));
        if (staticRate < 77 && in_stage == 0u && newLevel == 0)
            hold /= 20u;
        io_state.holdSamples = static_cast<AkUInt32>(static_cast<AkReal32>(hold) * (static_cast<AkReal32>(in_sampleRate) / 44100.0f));
    }
    io_state.active = true;
}

void ArpweaverFmSource::StartDx7PitchEnvelope(Dx7PitchEnvelopeState& io_state, AkUInt32 in_sampleRate, AkUInt8 in_stage)
{
    io_state.stage = in_stage;
    if (in_stage >= ArpweaverFmDx7Runtime::kEnvelopePointCount)
    {
        io_state.active = false;
        io_state.incrementQ24 = 0.0f;
        return;
    }
    const AkInt32 newLevel = ClampI(io_state.levels[in_stage], 0, 99);
    io_state.targetQ24 = static_cast<AkReal32>(static_cast<AkInt32>(kDx7PitchEnvTab[newLevel]) << 19);
    io_state.rising = (io_state.targetQ24 > io_state.levelQ24);
    const AkReal32 unit = 16777216.0f / (21.3f * static_cast<AkReal32>(in_sampleRate));
    io_state.incrementQ24 = static_cast<AkReal32>(kDx7PitchEnvRate[ClampI(io_state.rates[in_stage], 0, 99)]) * unit;
    io_state.active = true;
}

AkReal32 ArpweaverFmSource::TickDx7Envelope(Dx7EnvelopeState& io_state, AkUInt32 in_sampleRate)
{
    if (!io_state.active)
        return io_state.levelQ24;
    if (io_state.holdSamples > 0u)
    {
        --io_state.holdSamples;
        if (io_state.holdSamples == 0u)
            StartDx7Envelope(io_state, in_sampleRate, static_cast<AkUInt8>(io_state.stage + 1u));
    }
    if (io_state.stage < 3u || (io_state.stage < 4u && !io_state.keyDown))
    {
        if (io_state.holdSamples == 0u)
        {
            if (io_state.rising)
            {
                const AkReal32 jumpTarget = static_cast<AkReal32>(1716 << 16);
                if (io_state.levelQ24 < jumpTarget)
                    io_state.levelQ24 = jumpTarget;
                io_state.levelQ24 += (((17.0f * 16777216.0f) - io_state.levelQ24) * io_state.incrementQ24) * kInvQ24;
                if (io_state.levelQ24 >= io_state.targetQ24)
                {
                    io_state.levelQ24 = io_state.targetQ24;
                    StartDx7Envelope(io_state, in_sampleRate, static_cast<AkUInt8>(io_state.stage + 1u));
                }
            }
            else
            {
                io_state.levelQ24 -= io_state.incrementQ24;
                if (io_state.levelQ24 <= io_state.targetQ24)
                {
                    io_state.levelQ24 = io_state.targetQ24;
                    StartDx7Envelope(io_state, in_sampleRate, static_cast<AkUInt8>(io_state.stage + 1u));
                }
            }
        }
    }
    return io_state.levelQ24;
}

AkReal32 ArpweaverFmSource::TickDx7PitchEnvelope(Dx7PitchEnvelopeState& io_state, AkUInt32 in_sampleRate)
{
    if (!io_state.active)
        return io_state.levelQ24;
    if (io_state.stage < 3u || (io_state.stage < 4u && !io_state.keyDown))
    {
        if (io_state.rising)
        {
            io_state.levelQ24 += io_state.incrementQ24;
            if (io_state.levelQ24 >= io_state.targetQ24)
            {
                io_state.levelQ24 = io_state.targetQ24;
                StartDx7PitchEnvelope(io_state, in_sampleRate, static_cast<AkUInt8>(io_state.stage + 1u));
            }
        }
        else
        {
            io_state.levelQ24 -= io_state.incrementQ24;
            if (io_state.levelQ24 <= io_state.targetQ24)
            {
                io_state.levelQ24 = io_state.targetQ24;
                StartDx7PitchEnvelope(io_state, in_sampleRate, static_cast<AkUInt8>(io_state.stage + 1u));
            }
        }
    }
    return io_state.levelQ24;
}

AkReal32 ArpweaverFmSource::Dx7EnvelopeQ24ToLinear(AkReal32 in_levelQ24)
{
    return Q24ToFloat(Dx7Exp2LookupQ24(static_cast<AkInt32>(in_levelQ24) - (14 << 24)));
}

AkInt32 ArpweaverFmSource::FloatToQ24(AkReal32 in_value)
{
    const AkReal32 scaled = Clamp(in_value, -64.0f, 64.0f) * kPhaseQ24Scale;
    return static_cast<AkInt32>(std::floor(scaled + (scaled >= 0.0f ? 0.5f : -0.5f)));
}

AkReal32 ArpweaverFmSource::Q24ToFloat(AkInt32 in_value)
{
    return static_cast<AkReal32>(in_value) * kInvQ24;
}

void ArpweaverFmSource::ResetDx7Lfo(Dx7LfoState& io_state, const ArpweaverFmDx7Runtime::PatchData& in_patch, AkUInt32 in_sampleRate)
{
    const AkInt32 rate = ClampI(in_patch.lfoSpeed, 0, 99);
    const AkInt32 delay = ClampI(in_patch.lfoDelay, 0, 99);
    const AkReal64 ratio = 4437500000.0 / static_cast<AkReal64>((std::max)(in_sampleRate, 1u));

    io_state = Dx7LfoState{};
    io_state.deltaQ32 = static_cast<AkUInt32>(kDx7LfoRateSource[rate] * ratio);
    io_state.waveform = static_cast<AkUInt8>(ClampI(in_patch.lfoWave, 0, 5));
    io_state.sync = (in_patch.lfoSync != 0u);

    AkInt32 a = 99 - delay;
    if (a == 99)
    {
        io_state.delayInc = 0xFFFFFFFFu;
        io_state.delayInc2 = 0xFFFFFFFFu;
    }
    else
    {
        const AkReal64 unit = 25190424.0 / static_cast<AkReal64>((std::max)(in_sampleRate, 1u));
        a = (16 + (a & 15)) << (1 + (a >> 4));
        io_state.delayInc = static_cast<AkUInt32>(unit * static_cast<AkReal64>(a));
        a &= 0xFF80;
        a = (std::max)(0x80, a);
        io_state.delayInc2 = static_cast<AkUInt32>(unit * static_cast<AkReal64>(a));
    }

    if (io_state.sync)
        io_state.phaseQ32 = 0x7FFFFFFFu;
}

AkInt32 ArpweaverFmSource::TickDx7LfoValue(Dx7LfoState& io_state)
{
    io_state.phaseQ32 += io_state.deltaQ32;
    AkInt32 x = 0;
    switch (io_state.waveform)
    {
    case 0:
        x = static_cast<AkInt32>(io_state.phaseQ32 >> 7);
        x ^= -(static_cast<AkInt32>(io_state.phaseQ32 >> 31));
        x &= (1 << 24) - 1;
        return x;
    case 1:
        return static_cast<AkInt32>((~io_state.phaseQ32 ^ (1U << 31)) >> 8);
    case 2:
        return static_cast<AkInt32>((io_state.phaseQ32 ^ (1U << 31)) >> 8);
    case 3:
        return static_cast<AkInt32>(((~io_state.phaseQ32) >> 7) & (1 << 24));
    case 4:
        return static_cast<AkInt32>((1 << 23) + (Dx7SinLookupQ24(static_cast<AkInt32>((io_state.phaseQ32 >> 8) & 0x00FFFFFFu)) >> 1));
    case 5:
        if (io_state.phaseQ32 < io_state.deltaQ32)
            io_state.randState = static_cast<AkUInt8>((io_state.randState * 179u + 17u) & 0xFFu);
        x = (static_cast<AkInt32>(io_state.randState) ^ 0x80) + 1;
        return x << 16;
    default:
        return 1 << 23;
    }
}

AkInt32 ArpweaverFmSource::TickDx7LfoDelay(Dx7LfoState& io_state)
{
    const AkUInt32 delta = (io_state.delayState < (1U << 31)) ? io_state.delayInc : io_state.delayInc2;
    const AkUInt64 sum = static_cast<AkUInt64>(io_state.delayState) + static_cast<AkUInt64>(delta);
    if (sum > 0xFFFFFFFFull)
        return 1 << 24;
    io_state.delayState = static_cast<AkUInt32>(sum);
    if (sum < (1U << 31))
        return 0;
    return static_cast<AkInt32>((sum >> 7) & ((1U << 24) - 1U));
}

AkInt32 ArpweaverFmSource::SelectArpOffset(const ArpweaverFmRTPCParams& in_params)
{
    static const AkInt32 kPattern[16] = {0, 2, 4, 7, 12, 7, 4, 2, 0, -1, 3, 7, 10, 12, 10, 7};
    const AkInt32 stepCount = ClampI(static_cast<AkInt32>(std::floor(in_params.ArpStepCount + 0.5f)), 1, 16);
    const AkInt32 mode = ClampI(in_params.ArpMode, 0, 6);
    std::array<AkInt32, 16> enabledSteps{};
    const AkUInt32 enabledCount = CollectEnabledSequencerSteps(in_params, stepCount, enabledSteps);
    AkInt32 index = 0;
    if (mode == 2)
        index = enabledSteps[NextRandom(m_rngState) % enabledCount];
    else
        index = enabledSteps[static_cast<size_t>(SequenceOrderIndex(m_arpIndex++, static_cast<AkInt32>(enabledCount), mode))];
    return kPattern[index];
}

void ArpweaverFmSource::TriggerNoteEvent()
{
    for (AkUInt32 i = 0; i < kMaxVoiceLayers; ++i)
    {
        m_carrierPhase[i] = 0.0f;
        m_modPhase[i] = 0.0f;
        m_ampEnvelope[i] = 0.0f;
        m_modEnvelope[i] = 0.0f;
        m_ampEnvelopeStage[i] = kEnvAttackStage;
        m_modEnvelopeStage[i] = kEnvAttackStage;
        m_feedbackState[i] = 0.0f;
    }
    if (!m_params)
        return;
    const ArpweaverFmRTPCParams& p = m_params->GetParams();
    if (!p.HasDx7PatchData)
        return;
    for (AkUInt32 voice = 0; voice < m_activeVoiceCount; ++voice)
        InitializeDx7NoteState(voice, m_voiceMidi[voice], m_voiceDetuneSemi[voice], p);
}

AkReal32 ArpweaverFmSource::AdvanceEnvelope(AkReal32& io_value, AkUInt8& io_stage, AkReal32 in_attackSeconds, AkReal32 in_decaySeconds, AkReal32 in_sustainLevel)
{
    const AkReal32 attackSeconds = Clamp(in_attackSeconds, 0.0025f, 3.0f);
    const AkReal32 decaySeconds = Clamp(in_decaySeconds, 0.01f, 4.0f);
    const AkReal32 sustainLevel = Clamp(in_sustainLevel, 0.0f, 1.0f);
    if (io_stage == kEnvAttackStage)
    {
        const AkReal32 attackStep = 1.0f / (attackSeconds * static_cast<AkReal32>(m_sampleRate));
        io_value += attackStep;
        if (io_value >= 1.0f)
        {
            io_value = 1.0f;
            io_stage = kEnvDecayStage;
        }
    }
    else if (io_stage == kEnvDecayStage)
    {
        const AkReal32 decayStep = 1.0f / (decaySeconds * static_cast<AkReal32>(m_sampleRate));
        io_value += (sustainLevel - io_value) * decayStep * 8.0f;
        if (std::abs(io_value - sustainLevel) <= 0.001f)
        {
            io_value = sustainLevel;
            io_stage = kEnvSustainStage;
        }
    }
    else
    {
        io_value = sustainLevel;
    }
    return Clamp(io_value, 0.0f, 1.0f);
}

void ArpweaverFmSource::UpdateCurrentNote(bool in_forceRetrigger)
{
    if (!m_params)
        return;
    const ArpweaverFmRTPCParams& p = m_params->GetParams();
    const AkInt32 baseMidi = ClampI(static_cast<AkInt32>(std::floor(p.PitchBase + 0.5f)), 24, 96);
    AkInt32 rawMidi = baseMidi;
    const bool sequenceEnabled = p.ArpOn && !p.ChordOn;
    if (sequenceEnabled)
        rawMidi += SelectArpOffset(p);
    rawMidi = ClampI(rawMidi, 12, 120);
    const AkInt32 rootNote = ClampI(p.RootNote, 0, 11);
    const AkInt32 scaleMode = ClampI(p.ScaleMode, 0, 17);
    if (p.ChordOn)
        rawMidi += ResolveChordDegreeRootOffset(p.ChordSpread, m_seqPulseIndex, scaleMode);
    const AkInt32 nextMidi = QuantizeToScale(rawMidi, rootNote, scaleMode);
    const AkUInt32 unisonCount = p.UnisonOn
        ? static_cast<AkUInt32>(ClampI(static_cast<AkInt32>(std::floor(p.UnisonVoices + 0.5f)), 1, static_cast<AkInt32>(kMaxUnisonVoices)))
        : 1u;

    std::array<AkInt32, kMaxChordNotes> chordIntervals = {0, 0, 0, 0};
    AkUInt32 chordCount = 1u;
    if (p.ChordOn)
        chordCount = BuildChordIntervals(p.ChordMode, p.ChordInversion, chordIntervals);

    std::array<AkInt32, kMaxVoiceLayers> nextVoiceMidi{};
    std::array<AkReal32, kMaxVoiceLayers> nextVoiceFreq{};
    std::array<AkReal32, kMaxVoiceLayers> nextVoiceDetune{};
    AkUInt32 nextVoiceCount = 0u;

    for (AkUInt32 chordIndex = 0; chordIndex < chordCount && nextVoiceCount < kMaxVoiceLayers; ++chordIndex)
    {
        const AkInt32 chordMidi = QuantizeToScale(nextMidi + chordIntervals[chordIndex], rootNote, scaleMode);
        for (AkUInt32 unisonIndex = 0; unisonIndex < unisonCount && nextVoiceCount < kMaxVoiceLayers; ++unisonIndex)
        {
            AkReal32 spread = 0.0f;
            if (unisonCount > 1u)
                spread = (2.0f * static_cast<AkReal32>(unisonIndex) / static_cast<AkReal32>(unisonCount - 1u)) - 1.0f;
            const AkReal32 detuneSemi = spread * Clamp(p.UnisonDetune, 0.0f, 0.35f);
            nextVoiceMidi[nextVoiceCount] = chordMidi;
            nextVoiceDetune[nextVoiceCount] = detuneSemi;
            nextVoiceFreq[nextVoiceCount] = MidiToFrequency(static_cast<AkReal32>(chordMidi)) * std::pow(2.0f, detuneSemi / 12.0f);
            ++nextVoiceCount;
        }
    }

    if (nextVoiceCount == 0u)
    {
        nextVoiceCount = 1u;
        nextVoiceMidi[0] = nextMidi;
        nextVoiceDetune[0] = 0.0f;
        nextVoiceFreq[0] = MidiToFrequency(static_cast<AkReal32>(nextMidi));
    }

    bool noteChanged = in_forceRetrigger || nextMidi != m_currentMidi || nextVoiceCount != m_activeVoiceCount;
    for (AkUInt32 voice = 0; !noteChanged && voice < nextVoiceCount; ++voice)
    {
        if (m_voiceMidi[voice] != nextVoiceMidi[voice] || std::abs(m_voiceDetuneSemi[voice] - nextVoiceDetune[voice]) > 0.0001f)
            noteChanged = true;
    }

    m_currentMidi = nextMidi;
    m_currentFreq = MidiToFrequency(static_cast<AkReal32>(m_currentMidi));
    m_activeVoiceCount = nextVoiceCount;
    for (AkUInt32 voice = 0; voice < nextVoiceCount; ++voice)
    {
        m_voiceMidi[voice] = nextVoiceMidi[voice];
        m_voiceDetuneSemi[voice] = nextVoiceDetune[voice];
        m_voiceBaseFreq[voice] = nextVoiceFreq[voice];
    }

    if (noteChanged)
        TriggerNoteEvent();
}

void ArpweaverFmSource::InitializeDx7NoteState(AkUInt32 in_voiceIndex, AkInt32 in_noteMidi, AkReal32 in_detuneSemi, const ArpweaverFmRTPCParams& in_params)
{
    if (!in_params.HasDx7PatchData || in_voiceIndex >= kMaxVoiceLayers)
        return;
    const ArpweaverFmDx7Runtime::PatchData& patch = in_params.Dx7PatchData;
    const AkInt32 transpose = ClampI(static_cast<AkInt32>(patch.transpose) - 24, -24, 24);
    const AkInt32 noteMidi = ClampI(in_noteMidi + transpose, 0, 127);
    InitDx7LookupTables(m_sampleRate);
    if (patch.oscSync != 0u)
        m_dx7PhaseQ24[in_voiceIndex].fill(0);
    ResetDx7Lfo(m_dx7Lfo[in_voiceIndex], patch, m_sampleRate);
    m_dx7FeedbackHistoryA[in_voiceIndex].fill(0);
    m_dx7FeedbackHistoryB[in_voiceIndex].fill(0);
    for (AkUInt32 opIndex = 0; opIndex < kDx7OperatorCount; ++opIndex)
    {
        const ArpweaverFmDx7Runtime::OperatorPatchData& op = patch.ops[opIndex];
        Dx7EnvelopeState& env = m_dx7OpEnv[in_voiceIndex][opIndex];
        env = Dx7EnvelopeState{};
        env.rates = op.egRates;
        env.levels = op.egLevels;
        AkInt32 outLevel = ScaleOutLevel(op.outputLevel);
        outLevel += ScaleLevel(noteMidi, op.breakpoint, op.leftDepth, op.rightDepth, op.leftCurve, op.rightCurve);
        outLevel = ClampI(outLevel, 0, 127);
        outLevel <<= 5;
        outLevel += ScaleVelocity(100, op.velocitySensitivity);
        outLevel = (std::max)(0, outLevel);
        env.outLevel = outLevel;
        env.rateScaling = ScaleRate(noteMidi, op.rateScale);
        env.levelQ24 = 0.0f;
        env.keyDown = true;
        StartDx7Envelope(env, m_sampleRate, 0u);
        m_dx7BaseLogFreq[in_voiceIndex][opIndex] = ComputeDx7LogFrequency(noteMidi, op.mode, op.coarse, op.fine, op.detune) + FloatToQ24(in_detuneSemi / 12.0f);
    }
    Dx7PitchEnvelopeState& pitchEnv = m_dx7PitchEnv[in_voiceIndex];
    pitchEnv = Dx7PitchEnvelopeState{};
    pitchEnv.rates = patch.pitchRates;
    pitchEnv.levels = patch.pitchLevels;
    pitchEnv.levelQ24 = static_cast<AkReal32>(static_cast<AkInt32>(kDx7PitchEnvTab[ClampI(patch.pitchLevels[3], 0, 99)]) << 19);
    pitchEnv.keyDown = true;
    StartDx7PitchEnvelope(pitchEnv, m_sampleRate, 0u);
}

AkReal32 ArpweaverFmSource::RenderManualFmVoice(AkUInt32 in_voiceIndex, AkReal32 in_baseFreq, const ArpweaverFmRTPCParams& in_params)
{
    const AkReal32 ratioCarrier = Clamp(in_params.FmRatioCarrier, 0.125f, 16.0f);
    const AkReal32 ratioMod = Clamp(in_params.FmRatioMod, 0.125f, 16.0f);
    const AkReal32 carrierFreq = in_baseFreq * ratioCarrier;
    const AkReal32 modFreq = in_baseFreq * ratioMod;
    m_carrierPhase[in_voiceIndex] += carrierFreq / static_cast<AkReal32>(m_sampleRate);
    m_modPhase[in_voiceIndex] += modFreq / static_cast<AkReal32>(m_sampleRate);
    if (m_carrierPhase[in_voiceIndex] >= 1.0f)
        m_carrierPhase[in_voiceIndex] -= std::floor(m_carrierPhase[in_voiceIndex]);
    if (m_modPhase[in_voiceIndex] >= 1.0f)
        m_modPhase[in_voiceIndex] -= std::floor(m_modPhase[in_voiceIndex]);
    const AkReal32 ampEnv = AdvanceEnvelope(m_ampEnvelope[in_voiceIndex], m_ampEnvelopeStage[in_voiceIndex], in_params.AmpAttack, in_params.AmpDecay, in_params.AmpSustain);
    const AkReal32 modEnv = AdvanceEnvelope(m_modEnvelope[in_voiceIndex], m_modEnvelopeStage[in_voiceIndex], in_params.ModAttack, in_params.ModDecay, in_params.ModSustain);
    const AkReal32 feedbackAmount = Clamp(in_params.FmFeedback, 0.0f, 1.0f) * 2.5f;
    const AkReal32 carrierPhase = kTwoPi * m_carrierPhase[in_voiceIndex];
    const AkReal32 modPhase = kTwoPi * m_modPhase[in_voiceIndex];
    const AkReal32 modDepth = Clamp(in_params.FmIndex, 0.0f, 20.0f) * Clamp(in_params.FmModLevel, 0.0f, 1.0f) * modEnv;
    const AkReal32 modOsc = std::sin(modPhase + m_feedbackState[in_voiceIndex] * feedbackAmount);
    const AkReal32 modSignal = modOsc * modDepth;
    const AkReal32 carrierLevel = Clamp(in_params.FmCarrierLevel, 0.0f, 1.0f);
    const AkReal32 modLevel = Clamp(in_params.FmModLevel, 0.0f, 1.0f);
    const AkInt32 algorithmMacro = ClampI(in_params.FmAlgorithmMacro, 0, 3);
    AkReal32 output = 0.0f;
    switch (algorithmMacro)
    {
    case 0: output = std::sin(carrierPhase + modSignal) * carrierLevel * ampEnv; break;
    case 1: output = (std::sin(carrierPhase + modSignal * 0.55f) * carrierLevel + std::sin(modPhase) * modLevel * 0.55f) * ampEnv * 0.75f; break;
    case 2: output = std::sin(carrierPhase + modSignal + m_feedbackState[in_voiceIndex] * feedbackAmount * 0.5f) * carrierLevel * ampEnv;
            output += std::sin(modPhase * 0.5f + modSignal * 0.2f) * modLevel * ampEnv * 0.25f; break;
    default: output = (std::sin(carrierPhase + modSignal * 0.7f) + std::sin(carrierPhase * 0.5f + modSignal * 0.25f) * modLevel * 0.65f) * ampEnv * 0.6f; break;
    }
    m_feedbackState[in_voiceIndex] = output;
    return Clamp(output, -1.0f, 1.0f);
}
AkReal32 ArpweaverFmSource::RenderDx7Voice(AkUInt32 in_voiceIndex, const ArpweaverFmRTPCParams& in_params)
{
    const ArpweaverFmDx7Runtime::PatchData& patch = in_params.Dx7PatchData;
    const Dx7AlgorithmDef& algorithm = kDx7Algorithms[static_cast<size_t>(ClampI(patch.algorithm, 0, 31))];
    const AkUInt32 carrierCount = CountDx7Carriers(patch.algorithm);
    const AkInt32 feedbackShift = (patch.feedback != 0u) ? (8 - ClampI(patch.feedback, 0, 7)) : 16;

    const AkInt32 pitchEnvQ24 = static_cast<AkInt32>(TickDx7PitchEnvelope(m_dx7PitchEnv[in_voiceIndex], m_sampleRate));
    const AkInt32 lfoValueQ24 = TickDx7LfoValue(m_dx7Lfo[in_voiceIndex]);
    const AkInt32 lfoDelayQ24 = TickDx7LfoDelay(m_dx7Lfo[in_voiceIndex]);

    const AkUInt32 pitchModDepth = static_cast<AkUInt32>((ClampI(patch.pitchModDepth, 0, 99) * 165) >> 6);
    const AkInt32 sensLfo = kDx7PitchModSensitivityTable[ClampI(patch.pitchModSensitivity, 0, 7)] * (lfoValueQ24 - (1 << 23));
    AkInt32 pitchLfoQ24 = 0;
    if (pitchModDepth != 0u && sensLfo != 0)
    {
        const AkUInt32 pmdQ32 = static_cast<AkUInt32>(pitchModDepth * static_cast<AkUInt32>(lfoDelayQ24));
        pitchLfoQ24 = static_cast<AkInt32>((static_cast<AkInt64>(pmdQ32) * static_cast<AkInt64>(sensLfo)) >> 39);
    }

    const AkInt32 pitchModQ24 = pitchEnvQ24 + pitchLfoQ24;

    AkUInt32 ampModQ24 = static_cast<AkUInt32>((ClampI(patch.ampModDepth, 0, 99) * 165) >> 6);
    ampModQ24 = static_cast<AkUInt32>((static_cast<AkUInt64>(ampModQ24) * static_cast<AkUInt64>(lfoDelayQ24)) >> 8);
    ampModQ24 = static_cast<AkUInt32>((static_cast<AkUInt64>(ampModQ24) * static_cast<AkUInt64>((1 << 24) - ClampI(lfoValueQ24, 0, 1 << 24))) >> 24);

    AkInt32 bus[3] = {0, 0, 0};
    bool hasContents[3] = {true, false, false};
    AkInt64 outputQ24 = 0;

    auto ClampBusQ24 = [](AkInt64 in_value) -> AkInt32
    {
        const AkInt64 limit = static_cast<AkInt64>(16) << 24;
        if (in_value > limit)
            return static_cast<AkInt32>(limit);
        if (in_value < -limit)
            return static_cast<AkInt32>(-limit);
        return static_cast<AkInt32>(in_value);
    };

    for (AkUInt32 opIndex = 0; opIndex < kDx7OperatorCount; ++opIndex)
    {
        const AkUInt8 flags = algorithm.ops[opIndex];
        const AkUInt8 inBus = (flags >> 4) & 0x03u;
        const AkUInt8 outBus = flags & 0x03u;
        const bool add = (flags & 0x04u) != 0u;
        const bool useFeedback = (flags & 0xC0u) == 0xC0u;
        const ArpweaverFmDx7Runtime::OperatorPatchData& op = patch.ops[opIndex];
        AkInt32 envLevelQ24 = static_cast<AkInt32>(TickDx7Envelope(m_dx7OpEnv[in_voiceIndex][opIndex], m_sampleRate));

        if (ClampI(op.ampModSensitivity, 0, 3) != 0)
        {
            const AkUInt32 sensamp = static_cast<AkUInt32>((static_cast<AkUInt64>(ampModQ24) * static_cast<AkUInt64>(kDx7AmpModSensitivityTable[ClampI(op.ampModSensitivity, 0, 3)])) >> 24);
            const AkUInt32 pt = static_cast<AkUInt32>(std::exp((static_cast<AkReal32>(sensamp) / 262144.0f) * 0.07f + 12.2f));
            const AkUInt32 ldiff = static_cast<AkUInt32>((static_cast<AkUInt64>((std::max)(envLevelQ24, 0)) * (static_cast<AkUInt64>(pt) << 4)) >> 28);
            envLevelQ24 = (std::max)(0, envLevelQ24 - static_cast<AkInt32>(ldiff));
        }

        const AkInt32 gainQ24 = Dx7Exp2LookupQ24(envLevelQ24 - (14 << 24));
        if (gainQ24 <= 1)
        {
            if (outBus == 0u)
            {
                if (!add)
                    outputQ24 = 0;
            }
            else if (!add)
            {
                bus[outBus] = 0;
                hasContents[outBus] = false;
            }
            m_dx7FeedbackHistoryA[in_voiceIndex][opIndex] = 0;
            m_dx7FeedbackHistoryB[in_voiceIndex][opIndex] = 0;
            continue;
        }

        const AkInt32 modulation = (inBus == 0u || !hasContents[inBus]) ? 0 : ClampI(bus[inBus], -(4 << 24), (4 << 24));
        AkInt32 feedback = 0;
        if (useFeedback && feedbackShift < 16)
            feedback = ClampI((m_dx7FeedbackHistoryA[in_voiceIndex][opIndex] + m_dx7FeedbackHistoryB[in_voiceIndex][opIndex]) >> (feedbackShift + 1), -(3 << 24), (3 << 24));

        const AkInt32 baseLogFreq = m_dx7BaseLogFreq[in_voiceIndex][opIndex];
        AkInt32 phaseDeltaQ24 = (op.mode == 0u)
            ? Dx7FreqLookup(baseLogFreq + pitchModQ24)
            : Dx7FreqLookup(baseLogFreq + pitchEnvQ24);
        phaseDeltaQ24 = ClampI(phaseDeltaQ24, 0, 0x007FFFFF);
        const AkUInt32 wrappedPhase = (static_cast<AkUInt32>(m_dx7PhaseQ24[in_voiceIndex][opIndex] + phaseDeltaQ24)) & 0x00FFFFFFu;
        m_dx7PhaseQ24[in_voiceIndex][opIndex] = static_cast<AkInt32>(wrappedPhase);

        const AkInt32 phaseInputQ24 = static_cast<AkInt32>(wrappedPhase) + ClampI(modulation + feedback, -(6 << 24), (6 << 24));
        const AkInt32 signalQ24 = static_cast<AkInt32>((static_cast<AkInt64>(Dx7SinLookupQ24(phaseInputQ24 & 0x00FFFFFFu)) * static_cast<AkInt64>(gainQ24)) >> 24);

        m_dx7FeedbackHistoryA[in_voiceIndex][opIndex] = m_dx7FeedbackHistoryB[in_voiceIndex][opIndex];
        m_dx7FeedbackHistoryB[in_voiceIndex][opIndex] = signalQ24;
        if (outBus == 0u)
        {
            if (add)
                outputQ24 += signalQ24;
            else
                outputQ24 = signalQ24;
        }
        else
        {
            if (add && hasContents[outBus])
                bus[outBus] = ClampBusQ24(static_cast<AkInt64>(bus[outBus]) + signalQ24);
            else
                bus[outBus] = signalQ24;
            hasContents[outBus] = (bus[outBus] != 0);
        }
    }
    const AkReal32 outputScale = 0.24f / std::sqrt(static_cast<AkReal32>(carrierCount));
    return Clamp(Q24ToFloat(ClampBusQ24(outputQ24)) * outputScale, -1.0f, 1.0f);
}

void ArpweaverFmSource::ProcessChorus(AkReal32 in_sample, const ArpweaverFmRTPCParams& in_params, AkReal32& out_l, AkReal32& out_r)
{
    if (m_chorusBuffer.empty() || !in_params.ChorusOn)
    {
        out_l = in_sample;
        out_r = in_sample;
        return;
    }
    const AkReal32 mix = Clamp(in_params.ChorusMix, 0.0f, 1.0f);
    if (mix <= 0.0001f)
    {
        out_l = in_sample;
        out_r = in_sample;
        return;
    }
    m_chorusBuffer[m_chorusWrite] = in_sample;
    const AkReal32 rateHz = Clamp(in_params.ChorusRate, 0.05f, 5.0f);
    const AkReal32 depthMs = Clamp(in_params.ChorusDepthMs, 0.0f, 30.0f);
    const AkReal32 baseMs = 8.0f;
    const AkReal32 modL = 0.5f * (std::sin(kTwoPi * m_chorusLfoPhase) + 1.0f);
    const AkReal32 modR = 0.5f * (std::sin(kTwoPi * (m_chorusLfoPhase + 0.25f)) + 1.0f);
    const AkReal32 delaySamplesL = (baseMs + depthMs * modL) * static_cast<AkReal32>(m_sampleRate) / 1000.0f;
    const AkReal32 delaySamplesR = (baseMs + depthMs * modR) * static_cast<AkReal32>(m_sampleRate) / 1000.0f;
    const AkReal32 wetL = ReadCircularLinear(m_chorusBuffer, static_cast<AkReal32>(m_chorusWrite) - delaySamplesL);
    const AkReal32 wetR = ReadCircularLinear(m_chorusBuffer, static_cast<AkReal32>(m_chorusWrite) - delaySamplesR);
    out_l = in_sample * (1.0f - mix) + wetL * mix;
    out_r = in_sample * (1.0f - mix) + wetR * mix;
    m_chorusWrite = (m_chorusWrite + 1u) % static_cast<AkUInt32>(m_chorusBuffer.size());
    m_chorusLfoPhase += rateHz / static_cast<AkReal32>(m_sampleRate);
    if (m_chorusLfoPhase >= 1.0f)
        m_chorusLfoPhase -= std::floor(m_chorusLfoPhase);
}

void ArpweaverFmSource::ProcessDelay(AkReal32 in_l, AkReal32 in_r, const ArpweaverFmRTPCParams& in_params, AkReal32& out_l, AkReal32& out_r)
{
    if (m_delayBufferL.empty() || m_delayBufferR.empty() || !in_params.DelayOn)
    {
        out_l = in_l;
        out_r = in_r;
        return;
    }
    const AkReal32 mix = Clamp(in_params.DelayMix, 0.0f, 1.0f);
    if (mix <= 0.0001f)
    {
        out_l = in_l;
        out_r = in_r;
        return;
    }
    const AkUInt32 bufferSize = static_cast<AkUInt32>(m_delayBufferL.size());
    const AkReal32 delayMs = Clamp(in_params.DelayTimeMs, 5.0f, 2000.0f);
    AkUInt32 delaySamples = static_cast<AkUInt32>(delayMs * static_cast<AkReal32>(m_sampleRate) / 1000.0f);
    delaySamples = (std::max)(static_cast<AkUInt32>(1u), (std::min)(delaySamples, bufferSize - 1u));
    const AkUInt32 readIndex = (m_delayWrite + bufferSize - delaySamples) % bufferSize;
    const AkReal32 delayedL = m_delayBufferL[readIndex];
    const AkReal32 delayedR = m_delayBufferR[readIndex];
    const AkReal32 feedback = Clamp(in_params.DelayFeedback, 0.0f, 0.95f);
    m_delayBufferL[m_delayWrite] = in_l + delayedL * feedback;
    m_delayBufferR[m_delayWrite] = in_r + delayedR * feedback;
    out_l = in_l * (1.0f - mix) + delayedL * mix;
    out_r = in_r * (1.0f - mix) + delayedR * mix;
    m_delayWrite = (m_delayWrite + 1u) % bufferSize;
}

void ArpweaverFmSource::ProcessReverb(AkReal32 in_l, AkReal32 in_r, const ArpweaverFmRTPCParams& in_params, AkReal32& out_l, AkReal32& out_r)
{
    if (!in_params.ReverbOn)
    {
        out_l = in_l;
        out_r = in_r;
        return;
    }
    const AkReal32 mix = Clamp(in_params.ReverbMix, 0.0f, 1.0f);
    if (mix <= 0.0001f)
    {
        out_l = in_l;
        out_r = in_r;
        return;
    }
    const AkReal32 damp = Clamp(in_params.ReverbDamp, 0.05f, 1.0f);
    auto processChannel = [&](AkReal32 in_sample, std::array<std::vector<AkReal32>, kReverbLines>& in_buffers, std::array<AkUInt32, kReverbLines>& in_writes, AkReal32& io_dampState) -> AkReal32
    {
        AkReal32 y = in_sample;
        for (AkUInt32 i = 0; i < kReverbLines; ++i)
        {
            auto& buffer = in_buffers[i];
            if (buffer.empty())
                continue;
            AkUInt32 write = in_writes[i];
            if (write >= buffer.size())
                write = 0;
            const AkReal32 tap = buffer[write];
            const AkReal32 feedback = 0.72f - 0.08f * static_cast<AkReal32>(i);
            buffer[write] = y + tap * feedback;
            y = tap - y * 0.45f;
            write = (write + 1u) % static_cast<AkUInt32>(buffer.size());
            in_writes[i] = write;
        }
        io_dampState += damp * (y - io_dampState);
        return io_dampState;
    };
    const AkReal32 wetL = processChannel(in_l, m_reverbBufferL, m_reverbWriteL, m_reverbDampStateL);
    const AkReal32 wetR = processChannel(in_r + 0.12f * wetL, m_reverbBufferR, m_reverbWriteR, m_reverbDampStateR);
    out_l = in_l * (1.0f - mix) + wetL * mix;
    out_r = in_r * (1.0f - mix) + wetR * mix;
}
