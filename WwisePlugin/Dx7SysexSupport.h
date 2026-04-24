#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace ArpweaverFmDx7
{
struct OperatorData
{
    std::array<int, 4> egRates{};
    std::array<int, 4> egLevels{};
    int breakpoint = 0;
    int leftDepth = 0;
    int rightDepth = 0;
    int leftCurve = 0;
    int rightCurve = 0;
    int rateScale = 0;
    int ampModSensitivity = 0;
    int velocitySensitivity = 0;
    int outputLevel = 0;
    int mode = 0;
    int coarse = 1;
    int fine = 0;
    int detune = 7;
};

struct PatchData
{
    std::array<OperatorData, 6> ops{};
    std::array<int, 4> pitchRates{};
    std::array<int, 4> pitchLevels{};
    int algorithm = 0;
    int feedback = 0;
    int oscSync = 0;
    int lfoSpeed = 0;
    int lfoDelay = 0;
    int pitchModDepth = 0;
    int ampModDepth = 0;
    int lfoSync = 0;
    int lfoWave = 0;
    int pitchModSensitivity = 0;
    int transpose = 24;
    std::string name = "INIT VOICE";
};

struct MacroPatch
{
    float ratioCarrier = 1.0f;
    float ratioMod = 2.0f;
    float index = 2.5f;
    float carrierLevel = 0.85f;
    float modLevel = 0.75f;
    float feedback = 0.0f;
    int algorithmMacro = 0;
    float brightnessBias = 0.55f;
    float ampAttack = 0.02f;
    float ampDecay = 0.30f;
    float ampSustain = 0.85f;
    float ampRelease = 0.30f;
    float modAttack = 0.01f;
    float modDecay = 0.25f;
    float modSustain = 0.80f;
    float modRelease = 0.30f;
    std::string name = "INIT VOICE";
};

inline int ClampInt(int in_value, int in_minValue, int in_maxValue)
{
    return (std::max)(in_minValue, (std::min)(in_value, in_maxValue));
}

inline float Clamp(float in_value, float in_minValue, float in_maxValue)
{
    return (std::max)(in_minValue, (std::min)(in_value, in_maxValue));
}

inline std::string SanitizeName(const uint8_t* in_data, size_t in_size)
{
    std::string out;
    out.reserve(in_size);

    for (size_t i = 0; i < in_size; ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(in_data[i] & 0x7F);
        if (ch >= 32u && ch <= 126u)
            out.push_back(static_cast<char>(ch));
        else
            out.push_back(' ');
    }

    while (!out.empty() && out.back() == ' ')
        out.pop_back();

    if (out.empty())
        out = "Unnamed Patch";

    return out;
}

inline float OutputLevelToUnit(int in_outputLevel)
{
    const float normalized = Clamp(static_cast<float>(ClampInt(in_outputLevel, 0, 99)) / 99.0f, 0.0f, 1.0f);
    return std::pow(normalized, 1.25f);
}

inline float DxRatioFromModeCoarseFine(int in_mode, int in_coarse, int in_fine)
{
    const int coarse = ClampInt(in_coarse, 0, 31);
    const int fine = ClampInt(in_fine, 0, 99);

    if (in_mode == 0)
    {
        if (coarse == 0)
            return 0.5f + static_cast<float>(fine) / 200.0f;

        return static_cast<float>(coarse) + static_cast<float>(fine) / 100.0f;
    }

    const float fixedHz = std::pow(10.0f, (static_cast<float>((coarse & 3) * 100 + fine)) * 0.01f);
    const float ratioToMiddleC = fixedHz / 261.6255653f;
    return Clamp(ratioToMiddleC, 0.125f, 16.0f);
}

inline void UnpackBulkVoice128To156(const uint8_t* in_bulkVoice, uint8_t* out_voice156)
{
    if (!in_bulkVoice || !out_voice156)
        return;

    std::fill(out_voice156, out_voice156 + 156, 0u);

    for (int op = 0; op < 6; ++op)
    {
        const uint8_t* src = in_bulkVoice + op * 17;
        uint8_t* dst = out_voice156 + op * 21;

        for (int i = 0; i < 11; ++i)
            dst[i] = src[i] & 0x7F;

        const uint8_t leftRightCurves = static_cast<uint8_t>(src[11] & 0x0F);
        dst[11] = leftRightCurves & 0x03;
        dst[12] = (leftRightCurves >> 2) & 0x03;

        const uint8_t detuneRateScale = static_cast<uint8_t>(src[12] & 0x7F);
        dst[13] = detuneRateScale & 0x07;

        const uint8_t kvsAms = static_cast<uint8_t>(src[13] & 0x1F);
        dst[14] = kvsAms & 0x03;
        dst[15] = (kvsAms >> 2) & 0x07;

        dst[16] = src[14] & 0x7F;

        const uint8_t coarseMode = static_cast<uint8_t>(src[15] & 0x3F);
        dst[17] = coarseMode & 0x01;
        dst[18] = (coarseMode >> 1) & 0x1F;

        dst[19] = src[16] & 0x7F;
        dst[20] = (detuneRateScale >> 3) & 0x0F;
    }

    for (int i = 0; i < 8; ++i)
        out_voice156[126 + i] = in_bulkVoice[102 + i] & 0x7F;

    out_voice156[134] = in_bulkVoice[110] & 0x1F;

    const uint8_t feedbackSync = static_cast<uint8_t>(in_bulkVoice[111] & 0x0F);
    out_voice156[135] = feedbackSync & 0x07;
    out_voice156[136] = (feedbackSync >> 3) & 0x01;
    out_voice156[137] = in_bulkVoice[112] & 0x7F;
    out_voice156[138] = in_bulkVoice[113] & 0x7F;
    out_voice156[139] = in_bulkVoice[114] & 0x7F;
    out_voice156[140] = in_bulkVoice[115] & 0x7F;

    const uint8_t lfoPacked = in_bulkVoice[116] & 0x7F;
    out_voice156[141] = lfoPacked & 0x01;
    out_voice156[142] = (lfoPacked >> 1) & 0x07;
    out_voice156[143] = (lfoPacked >> 4) & 0x07;
    out_voice156[144] = in_bulkVoice[117] & 0x7F;

    for (int i = 0; i < 10; ++i)
        out_voice156[145 + i] = in_bulkVoice[118 + i] & 0x7F;
}

inline PatchData ParseVoice156(const uint8_t* in_voice156)
{
    PatchData patch;
    if (!in_voice156)
        return patch;

    for (int op = 0; op < 6; ++op)
    {
        const uint8_t* src = in_voice156 + op * 21;
        // DX7 SysEx stores operators in the order 6,5,4,3,2,1.
        // The runtime algorithm tables in this plug-in address operators as 1,2,3,4,5,6.
        // Reverse them here so algorithm routing and feedback mapping stay stable.
        OperatorData& dst = patch.ops[static_cast<size_t>(5 - op)];

        for (int i = 0; i < 4; ++i)
        {
            dst.egRates[static_cast<size_t>(i)] = ClampInt(src[i], 0, 99);
            dst.egLevels[static_cast<size_t>(i)] = ClampInt(src[4 + i], 0, 99);
        }

        dst.breakpoint = ClampInt(src[8], 0, 99);
        dst.leftDepth = ClampInt(src[9], 0, 99);
        dst.rightDepth = ClampInt(src[10], 0, 99);
        dst.leftCurve = ClampInt(src[11], 0, 3);
        dst.rightCurve = ClampInt(src[12], 0, 3);
        dst.rateScale = ClampInt(src[13], 0, 7);
        dst.ampModSensitivity = ClampInt(src[14], 0, 3);
        dst.velocitySensitivity = ClampInt(src[15], 0, 7);
        dst.outputLevel = ClampInt(src[16], 0, 99);
        dst.mode = ClampInt(src[17], 0, 1);
        dst.coarse = ClampInt(src[18], 0, 31);
        dst.fine = ClampInt(src[19], 0, 99);
        dst.detune = ClampInt(src[20], 0, 14);
    }

    for (int i = 0; i < 4; ++i)
    {
        patch.pitchRates[static_cast<size_t>(i)] = ClampInt(in_voice156[126 + i], 0, 99);
        patch.pitchLevels[static_cast<size_t>(i)] = ClampInt(in_voice156[130 + i], 0, 99);
    }

    patch.algorithm = ClampInt(in_voice156[134], 0, 31);
    patch.feedback = ClampInt(in_voice156[135], 0, 7);
    patch.oscSync = ClampInt(in_voice156[136], 0, 1);
    patch.lfoSpeed = ClampInt(in_voice156[137], 0, 99);
    patch.lfoDelay = ClampInt(in_voice156[138], 0, 99);
    patch.pitchModDepth = ClampInt(in_voice156[139], 0, 99);
    patch.ampModDepth = ClampInt(in_voice156[140], 0, 99);
    patch.lfoSync = ClampInt(in_voice156[141], 0, 1);
    patch.pitchModSensitivity = ClampInt(in_voice156[142], 0, 7);
    patch.lfoWave = ClampInt(in_voice156[143], 0, 5);
    patch.transpose = ClampInt(in_voice156[144], 0, 48);
    patch.name = SanitizeName(in_voice156 + 145, 10);

    return patch;
}

inline bool LoadFileBytes(const std::string& in_pathUtf8, std::vector<uint8_t>& out_bytes)
{
    out_bytes.clear();

    std::error_code ec;
    const std::filesystem::path path = std::filesystem::u8path(in_pathUtf8);
    if (!std::filesystem::exists(path, ec))
        return false;

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0)
        return false;

    file.seekg(0, std::ios::beg);
    out_bytes.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(out_bytes.data()), size);

    return file.good() || file.eof();
}

inline bool AppendSingleVoice(const uint8_t* in_voice155, size_t in_size, std::vector<PatchData>& io_patches)
{
    if (!in_voice155 || in_size < 155)
        return false;

    io_patches.push_back(ParseVoice156(in_voice155));
    return true;
}

inline bool AppendBulkBank(const uint8_t* in_bulk4096, size_t in_size, std::vector<PatchData>& io_patches)
{
    if (!in_bulk4096 || in_size < 4096)
        return false;

    uint8_t unpacked[156] = {};
    for (int patchIndex = 0; patchIndex < 32; ++patchIndex)
    {
        UnpackBulkVoice128To156(in_bulk4096 + patchIndex * 128, unpacked);
        io_patches.push_back(ParseVoice156(unpacked));
    }

    return true;
}

inline bool LoadPatchesFromFile(const std::string& in_pathUtf8, std::vector<PatchData>& out_patches, std::string& out_formatName)
{
    out_patches.clear();
    out_formatName.clear();

    std::vector<uint8_t> bytes;
    if (!LoadFileBytes(in_pathUtf8, bytes))
        return false;

    const auto appendAndTag = [&](bool in_result, const char* in_name) -> bool
    {
        if (in_result && !out_patches.empty())
        {
            out_formatName = in_name;
            return true;
        }
        return false;
    };

    if (bytes.size() == 4096 && appendAndTag(AppendBulkBank(bytes.data(), bytes.size(), out_patches), "DX7 32-Voice Bulk Raw"))
        return true;

    if (bytes.size() == 4104 && bytes.front() == 0xF0 && bytes.back() == 0xF7 &&
        appendAndTag(AppendBulkBank(bytes.data() + 6, 4096, out_patches), "DX7 32-Voice Bulk"))
        return true;

    if (bytes.size() == 155 && appendAndTag(AppendSingleVoice(bytes.data(), bytes.size(), out_patches), "DX7 Single Voice Raw"))
        return true;

    if (bytes.size() == 163 && bytes.front() == 0xF0 && bytes.back() == 0xF7 &&
        appendAndTag(AppendSingleVoice(bytes.data() + 6, 155, out_patches), "DX7 Single Voice"))
        return true;

    size_t offset = 0;
    while (offset < bytes.size())
    {
        if (bytes[offset] != 0xF0)
        {
            ++offset;
            continue;
        }

        size_t end = offset + 1;
        while (end < bytes.size() && bytes[end] != 0xF7)
            ++end;

        if (end >= bytes.size())
            break;

        const size_t messageLength = end - offset + 1;
        if (messageLength >= 8 && bytes[offset + 1] == 0x43)
        {
            const uint8_t format = bytes[offset + 3];
            const int byteCount = (static_cast<int>(bytes[offset + 4]) << 7) | static_cast<int>(bytes[offset + 5]);
            const size_t dataStart = offset + 6;

            if (format == 0x09 && byteCount == 4096 && dataStart + 4096 <= end)
            {
                AppendBulkBank(bytes.data() + dataStart, 4096, out_patches);
                out_formatName = "DX7 32-Voice Bulk Stream";
            }
            else if (byteCount == 155 && dataStart + 155 <= end)
            {
                AppendSingleVoice(bytes.data() + dataStart, 155, out_patches);
                out_formatName = "DX7 Single Voice Stream";
            }
            else if (byteCount == 128 && dataStart + 128 <= end)
            {
                uint8_t unpacked[156] = {};
                UnpackBulkVoice128To156(bytes.data() + dataStart, unpacked);
                out_patches.push_back(ParseVoice156(unpacked));
                out_formatName = "DX7 Packed Voice Stream";
            }
        }

        offset = end + 1;
    }

    return !out_patches.empty();
}

inline float RateToAttackSeconds(int in_rate)
{
    const float t = 1.0f - Clamp(static_cast<float>(in_rate) / 99.0f, 0.0f, 1.0f);
    return 0.0025f + t * t * 1.25f;
}

inline float RateToSegmentSeconds(int in_rate, float in_minSeconds, float in_maxSeconds)
{
    const float t = 1.0f - Clamp(static_cast<float>(in_rate) / 99.0f, 0.0f, 1.0f);
    return in_minSeconds + t * t * (in_maxSeconds - in_minSeconds);
}

inline int MapAlgorithmToMacro(int in_algorithm)
{
    const int algorithm = ClampInt(in_algorithm, 0, 31);
    if (algorithm <= 7)
        return 0;
    if (algorithm <= 15)
        return 1;
    if (algorithm <= 23)
        return 2;
    return 3;
}

inline MacroPatch ReducePatchToMacro(const PatchData& in_patch)
{
    struct RankedOp
    {
        int index = 0;
        float weight = 0.0f;
        float ratio = 1.0f;
        float sustain = 1.0f;
    };

    std::array<RankedOp, 6> ranked{};
    for (int i = 0; i < 6; ++i)
    {
        const OperatorData& op = in_patch.ops[static_cast<size_t>(i)];
        ranked[static_cast<size_t>(i)].index = i;
        ranked[static_cast<size_t>(i)].weight = OutputLevelToUnit(op.outputLevel);
        ranked[static_cast<size_t>(i)].ratio = DxRatioFromModeCoarseFine(op.mode, op.coarse, op.fine);
        ranked[static_cast<size_t>(i)].sustain = Clamp(static_cast<float>(op.egLevels[2]) / 99.0f, 0.0f, 1.0f);
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedOp& in_left, const RankedOp& in_right)
    {
        if (in_left.weight != in_right.weight)
            return in_left.weight > in_right.weight;
        return in_left.index < in_right.index;
    });

    const RankedOp& carrier = ranked[0];
    const RankedOp& mod = ranked[1];
    const OperatorData& carrierOp = in_patch.ops[static_cast<size_t>(carrier.index)];
    const OperatorData& modOp = in_patch.ops[static_cast<size_t>(mod.index)];

    float otherWeight = 0.0f;
    float weightedRatioSum = 0.0f;
    for (size_t i = 1; i < ranked.size(); ++i)
    {
        otherWeight += ranked[i].weight;
        weightedRatioSum += ranked[i].ratio * ranked[i].weight;
    }

    MacroPatch patch;
    patch.name = in_patch.name;
    patch.ratioCarrier = Clamp(carrier.ratio, 0.125f, 16.0f);
    patch.ratioMod = Clamp(otherWeight > 0.0001f ? (weightedRatioSum / otherWeight) : mod.ratio, 0.125f, 16.0f);
    patch.carrierLevel = Clamp(0.08f + carrier.weight * 0.92f, 0.05f, 1.0f);
    patch.modLevel = Clamp(0.04f + mod.weight * 0.96f, 0.0f, 1.0f);

    const float algorithmContribution = static_cast<float>(in_patch.algorithm) / 31.0f;
    const float feedbackContribution = static_cast<float>(in_patch.feedback) / 7.0f;
    const float modulationPool = Clamp(otherWeight - carrier.weight * 0.15f, 0.0f, 3.5f);
    patch.index = Clamp(0.35f + modulationPool * 3.0f + algorithmContribution * 4.0f + feedbackContribution * 2.5f, 0.0f, 20.0f);
    patch.feedback = Clamp(feedbackContribution, 0.0f, 1.0f);
    patch.algorithmMacro = MapAlgorithmToMacro(in_patch.algorithm);
    patch.brightnessBias = Clamp(0.25f +
                                 static_cast<float>(in_patch.ampModDepth) / 99.0f * 0.20f +
                                 static_cast<float>(in_patch.lfoSpeed) / 99.0f * 0.15f +
                                 static_cast<float>(in_patch.transpose) / 48.0f * 0.20f +
                                 carrier.weight * 0.20f,
                                 0.0f, 1.0f);

    patch.ampAttack = Clamp(RateToAttackSeconds(carrierOp.egRates[0]), 0.0025f, 2.0f);
    patch.ampDecay = Clamp(RateToSegmentSeconds(carrierOp.egRates[1], 0.01f, 2.5f), 0.01f, 3.0f);
    patch.ampSustain = Clamp(static_cast<float>(carrierOp.egLevels[2]) / 99.0f, 0.05f, 1.0f);
    patch.ampRelease = Clamp(RateToSegmentSeconds(carrierOp.egRates[3], 0.02f, 3.5f), 0.02f, 4.0f);

    patch.modAttack = Clamp(RateToAttackSeconds(modOp.egRates[0]), 0.0025f, 2.0f);
    patch.modDecay = Clamp(RateToSegmentSeconds(modOp.egRates[1], 0.01f, 2.5f), 0.01f, 3.0f);
    patch.modSustain = Clamp(static_cast<float>(modOp.egLevels[2]) / 99.0f, 0.0f, 1.0f);
    patch.modRelease = Clamp(RateToSegmentSeconds(modOp.egRates[3], 0.02f, 3.5f), 0.02f, 4.0f);

    return patch;
}
} // namespace ArpweaverFmDx7
