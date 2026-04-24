#include "ArpweaverFmPlugin.h"
#include "Dx7SysexSupport.h"
#include "../Dx7RuntimeShared.h"
#include "../SoundEnginePlugin/ArpweaverFmSourceFactory.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
float Clamp(float in_v, float in_lo, float in_hi)
{
    if (in_v < in_lo)
        return in_lo;
    if (in_v > in_hi)
        return in_hi;
    return in_v;
}

int ClampInt(int in_v, int in_lo, int in_hi)
{
    if (in_v < in_lo)
        return in_lo;
    if (in_v > in_hi)
        return in_hi;
    return in_v;
}

void TraceSysex(const std::string& in_line)
{
    const char* temp = std::getenv("TEMP");
    if (!temp || !temp[0])
        return;

    std::error_code ec;
    std::filesystem::path path = std::filesystem::u8path(temp);
    path /= "ArpweaverFm_sysex_trace.log";

    std::ofstream f(path, std::ios::app);
    if (!f)
        return;

    f << in_line << "\n";
}

bool LoadFileBytes(const std::string& in_pathUtf8, std::vector<uint8_t>& out_bytes)
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

ArpweaverFmDx7::MacroPatch ConvertGenericBytesToPatch(const uint8_t* in_data, size_t in_size, int in_patchIndex)
{
    ArpweaverFmDx7::MacroPatch patch;
    if (!in_data || in_size == 0)
        return patch;

    const size_t stride = 64;
    const size_t start = (static_cast<size_t>((std::max)(in_patchIndex, 0)) * stride) % in_size;

    auto getByte = [&](size_t in_offset) -> uint8_t
    {
        return in_data[(start + in_offset) % in_size];
    };

    const float b0 = static_cast<float>(getByte(0));
    const float b1 = static_cast<float>(getByte(1));
    const float b2 = static_cast<float>(getByte(2));
    const float b3 = static_cast<float>(getByte(3));
    const float b4 = static_cast<float>(getByte(4));

    patch.ratioCarrier = Clamp(0.25f + (b0 / 255.0f) * 7.75f, 0.125f, 16.0f);
    patch.ratioMod = Clamp(0.25f + (b1 / 255.0f) * 15.75f, 0.125f, 16.0f);
    patch.index = Clamp((b2 / 255.0f) * 20.0f, 0.0f, 20.0f);
    patch.carrierLevel = Clamp(0.15f + (b3 / 255.0f) * 0.85f, 0.0f, 1.0f);
    patch.modLevel = Clamp((b4 / 255.0f), 0.0f, 1.0f);
    patch.feedback = Clamp((b0 + b4) / 510.0f, 0.0f, 1.0f);
    patch.algorithmMacro = (in_patchIndex % 4 + 4) % 4;
    patch.brightnessBias = Clamp(0.35f + (b2 / 255.0f) * 0.5f, 0.0f, 1.0f);
    patch.ampAttack = Clamp(0.01f + (255.0f - b0) / 255.0f * 0.75f, 0.01f, 1.2f);
    patch.ampDecay = Clamp(0.05f + (255.0f - b1) / 255.0f * 1.25f, 0.05f, 2.0f);
    patch.ampSustain = Clamp(0.2f + (b3 / 255.0f) * 0.8f, 0.05f, 1.0f);
    patch.ampRelease = Clamp(0.05f + (255.0f - b4) / 255.0f * 1.5f, 0.05f, 2.5f);
    patch.modAttack = patch.ampAttack * 0.5f;
    patch.modDecay = patch.ampDecay * 0.75f;
    patch.modSustain = Clamp(0.1f + (b4 / 255.0f) * 0.9f, 0.0f, 1.0f);
    patch.modRelease = patch.ampRelease * 0.8f;
    patch.name = "Generic Binary Patch";

    return patch;
}

bool ResolvePatchFromFile(const std::string& in_pathUtf8, int in_patchIndex, ArpweaverFmDx7::MacroPatch& out_patch, std::string& out_label)
{
    std::vector<ArpweaverFmDx7::PatchData> patches;
    std::string formatName;
    if (ArpweaverFmDx7::LoadPatchesFromFile(in_pathUtf8, patches, formatName) && !patches.empty())
    {
        const int safePatchIndex = ClampInt(in_patchIndex, 0, static_cast<int>(patches.size()) - 1);
        out_patch = ArpweaverFmDx7::ReducePatchToMacro(patches[static_cast<size_t>(safePatchIndex)]);
        out_label = formatName + " :: " + out_patch.name;
        TraceSysex("DX parse ok. format=" + formatName +
                   " patches=" + std::to_string(patches.size()) +
                   " selected=" + std::to_string(safePatchIndex) +
                   " name=" + out_patch.name);
        return true;
    }

    std::vector<uint8_t> bytes;
    if (!LoadFileBytes(in_pathUtf8, bytes) || bytes.empty())
    {
        out_label = "No readable SysEx data";
        TraceSysex("Generic parse failed (empty bytes): " + in_pathUtf8);
        return false;
    }

    out_patch = ConvertGenericBytesToPatch(bytes.data(), bytes.size(), in_patchIndex);
    out_label = "Generic binary fallback";
    TraceSysex("Generic patch fallback used. bytes=" + std::to_string(bytes.size()) + " patchIndex=" + std::to_string(in_patchIndex));
    return true;
}

ArpweaverFmDx7Runtime::PatchData ConvertToRuntimePatch(const ArpweaverFmDx7::PatchData& in_patch)
{
    ArpweaverFmDx7Runtime::PatchData out_patch;
    out_patch.hasPatch = 1u;
    out_patch.algorithm = static_cast<AkUInt8>(ClampInt(in_patch.algorithm, 0, 31));
    out_patch.feedback = static_cast<AkUInt8>(ClampInt(in_patch.feedback, 0, 7));
    out_patch.oscSync = static_cast<AkUInt8>(ClampInt(in_patch.oscSync, 0, 1));
    out_patch.lfoSpeed = static_cast<AkUInt8>(ClampInt(in_patch.lfoSpeed, 0, 99));
    out_patch.lfoDelay = static_cast<AkUInt8>(ClampInt(in_patch.lfoDelay, 0, 99));
    out_patch.pitchModDepth = static_cast<AkUInt8>(ClampInt(in_patch.pitchModDepth, 0, 99));
    out_patch.ampModDepth = static_cast<AkUInt8>(ClampInt(in_patch.ampModDepth, 0, 99));
    out_patch.lfoSync = static_cast<AkUInt8>(ClampInt(in_patch.lfoSync, 0, 1));
    out_patch.lfoWave = static_cast<AkUInt8>(ClampInt(in_patch.lfoWave, 0, 5));
    out_patch.pitchModSensitivity = static_cast<AkUInt8>(ClampInt(in_patch.pitchModSensitivity, 0, 7));
    out_patch.transpose = static_cast<AkUInt8>(ClampInt(in_patch.transpose, 0, 48));

    for (size_t i = 0; i < ArpweaverFmDx7Runtime::kEnvelopePointCount; ++i)
    {
        out_patch.pitchRates[i] = static_cast<AkUInt8>(ClampInt(in_patch.pitchRates[i], 0, 99));
        out_patch.pitchLevels[i] = static_cast<AkUInt8>(ClampInt(in_patch.pitchLevels[i], 0, 99));
    }

    for (size_t opIndex = 0; opIndex < ArpweaverFmDx7Runtime::kOperatorCount; ++opIndex)
    {
        const ArpweaverFmDx7::OperatorData& src = in_patch.ops[opIndex];
        auto& dst = out_patch.ops[opIndex];

        for (size_t i = 0; i < ArpweaverFmDx7Runtime::kEnvelopePointCount; ++i)
        {
            dst.egRates[i] = static_cast<AkUInt8>(ClampInt(src.egRates[i], 0, 99));
            dst.egLevels[i] = static_cast<AkUInt8>(ClampInt(src.egLevels[i], 0, 99));
        }

        dst.breakpoint = static_cast<AkUInt8>(ClampInt(src.breakpoint, 0, 99));
        dst.leftDepth = static_cast<AkUInt8>(ClampInt(src.leftDepth, 0, 99));
        dst.rightDepth = static_cast<AkUInt8>(ClampInt(src.rightDepth, 0, 99));
        dst.leftCurve = static_cast<AkUInt8>(ClampInt(src.leftCurve, 0, 3));
        dst.rightCurve = static_cast<AkUInt8>(ClampInt(src.rightCurve, 0, 3));
        dst.rateScale = static_cast<AkUInt8>(ClampInt(src.rateScale, 0, 7));
        dst.ampModSensitivity = static_cast<AkUInt8>(ClampInt(src.ampModSensitivity, 0, 3));
        dst.velocitySensitivity = static_cast<AkUInt8>(ClampInt(src.velocitySensitivity, 0, 7));
        dst.outputLevel = static_cast<AkUInt8>(ClampInt(src.outputLevel, 0, 99));
        dst.mode = static_cast<AkUInt8>(ClampInt(src.mode, 0, 1));
        dst.coarse = static_cast<AkUInt8>(ClampInt(src.coarse, 0, 31));
        dst.fine = static_cast<AkUInt8>(ClampInt(src.fine, 0, 99));
        dst.detune = static_cast<AkUInt8>(ClampInt(src.detune, 0, 14));
    }

    return out_patch;
}

bool WriteRuntimePatch(AK::Wwise::Plugin::DataWriter& in_dataWriter, const ArpweaverFmDx7Runtime::PatchData& in_patch)
{
    bool result = true;
    result &= in_dataWriter.WriteUInt32(in_patch.magic);
    result &= in_dataWriter.WriteUInt32(in_patch.version);
    result &= in_dataWriter.WriteUInt8(in_patch.hasPatch);
    result &= in_dataWriter.WriteUInt8(in_patch.algorithm);
    result &= in_dataWriter.WriteUInt8(in_patch.feedback);
    result &= in_dataWriter.WriteUInt8(in_patch.oscSync);
    result &= in_dataWriter.WriteUInt8(in_patch.lfoSpeed);
    result &= in_dataWriter.WriteUInt8(in_patch.lfoDelay);
    result &= in_dataWriter.WriteUInt8(in_patch.pitchModDepth);
    result &= in_dataWriter.WriteUInt8(in_patch.ampModDepth);
    result &= in_dataWriter.WriteUInt8(in_patch.lfoSync);
    result &= in_dataWriter.WriteUInt8(in_patch.lfoWave);
    result &= in_dataWriter.WriteUInt8(in_patch.pitchModSensitivity);
    result &= in_dataWriter.WriteUInt8(in_patch.transpose);

    for (const AkUInt8 value : in_patch.pitchRates)
        result &= in_dataWriter.WriteUInt8(value);
    for (const AkUInt8 value : in_patch.pitchLevels)
        result &= in_dataWriter.WriteUInt8(value);

    for (const auto& op : in_patch.ops)
    {
        for (const AkUInt8 value : op.egRates)
            result &= in_dataWriter.WriteUInt8(value);
        for (const AkUInt8 value : op.egLevels)
            result &= in_dataWriter.WriteUInt8(value);
        result &= in_dataWriter.WriteUInt8(op.breakpoint);
        result &= in_dataWriter.WriteUInt8(op.leftDepth);
        result &= in_dataWriter.WriteUInt8(op.rightDepth);
        result &= in_dataWriter.WriteUInt8(op.leftCurve);
        result &= in_dataWriter.WriteUInt8(op.rightCurve);
        result &= in_dataWriter.WriteUInt8(op.rateScale);
        result &= in_dataWriter.WriteUInt8(op.ampModSensitivity);
        result &= in_dataWriter.WriteUInt8(op.velocitySensitivity);
        result &= in_dataWriter.WriteUInt8(op.outputLevel);
        result &= in_dataWriter.WriteUInt8(op.mode);
        result &= in_dataWriter.WriteUInt8(op.coarse);
        result &= in_dataWriter.WriteUInt8(op.fine);
        result &= in_dataWriter.WriteUInt8(op.detune);
    }

    return result;
}
} // namespace

ArpweaverFmPlugin::ArpweaverFmPlugin()
{
}

ArpweaverFmPlugin::~ArpweaverFmPlugin()
{
}

bool ArpweaverFmPlugin::GetBankParameters(const GUID& in_guidPlatform, AK::Wwise::Plugin::DataWriter& in_dataWriter) const
{
    bool result = true;

    const float pitchBase = m_propertySet.GetReal32(in_guidPlatform, "PitchBase");
    const float brightness = m_propertySet.GetReal32(in_guidPlatform, "Brightness");
    const float volume = m_propertySet.GetReal32(in_guidPlatform, "Volume");

    const int rootNote = m_propertySet.GetInt32(in_guidPlatform, "RootNote");
    const int scaleMode = m_propertySet.GetInt32(in_guidPlatform, "ScaleMode");

    const bool arpOn = m_propertySet.GetBool(in_guidPlatform, "ArpOn");
    const float arpRate = m_propertySet.GetReal32(in_guidPlatform, "ArpRate");
    const int arpMode = m_propertySet.GetInt32(in_guidPlatform, "ArpMode");
    const float arpStepCount = m_propertySet.GetReal32(in_guidPlatform, "ArpStepCount");

    float fmRatioCarrier = m_propertySet.GetReal32(in_guidPlatform, "FmRatioCarrier");
    float fmRatioMod = m_propertySet.GetReal32(in_guidPlatform, "FmRatioMod");
    float fmIndex = m_propertySet.GetReal32(in_guidPlatform, "FmIndex");
    float fmCarrierLevel = m_propertySet.GetReal32(in_guidPlatform, "FmCarrierLevel");
    float fmModLevel = m_propertySet.GetReal32(in_guidPlatform, "FmModLevel");
    float fmFeedback = m_propertySet.GetReal32(in_guidPlatform, "FmFeedback");
    int fmAlgorithmMacro = m_propertySet.GetInt32(in_guidPlatform, "FmAlgorithmMacro");
    float fmBrightnessBias = brightness;
    float ampAttack = m_propertySet.GetReal32(in_guidPlatform, "AmpAttack");
    float ampDecay = m_propertySet.GetReal32(in_guidPlatform, "AmpDecay");
    float ampSustain = m_propertySet.GetReal32(in_guidPlatform, "AmpSustain");
    float ampRelease = m_propertySet.GetReal32(in_guidPlatform, "AmpRelease");
    float modAttack = 0.01f;
    float modDecay = 0.25f;
    float modSustain = 0.80f;
    float modRelease = 0.30f;

    const bool useSysexPatch = m_propertySet.GetBool(in_guidPlatform, "UseSysexPatch");
    const int patchIndex = m_propertySet.GetInt32(in_guidPlatform, "PatchIndex");
    const char* sysexPath = m_propertySet.GetString(in_guidPlatform, "SysexFilePath");
    const bool hasPath = (sysexPath != nullptr && sysexPath[0] != '\0');
    ArpweaverFmDx7Runtime::PatchData runtimePatch;

    TraceSysex(std::string("GetBankParameters path=") + (hasPath ? sysexPath : "<empty>") +
               " useSysex=" + (useSysexPatch ? "1" : "0") +
               " patchIndex=" + std::to_string(patchIndex));

    if (hasPath && useSysexPatch)
    {
        ArpweaverFmDx7::MacroPatch patchFromFile;
        std::string patchLabel;
        if (ResolvePatchFromFile(sysexPath, patchIndex, patchFromFile, patchLabel))
        {
            TraceSysex("Applied patch => " + patchLabel +
                       " carrier=" + std::to_string(fmRatioCarrier) +
                       " mod=" + std::to_string(fmRatioMod) +
                       " index=" + std::to_string(fmIndex) +
                       " feedback=" + std::to_string(fmFeedback));
        }
        else
        {
            TraceSysex("Patch apply skipped (parse failed or unreadable file)");
        }

        std::vector<ArpweaverFmDx7::PatchData> patches;
        std::string formatName;
        if (ArpweaverFmDx7::LoadPatchesFromFile(sysexPath, patches, formatName) && !patches.empty())
        {
            const int safePatchIndex = ClampInt(patchIndex, 0, static_cast<int>(patches.size()) - 1);
            runtimePatch = ConvertToRuntimePatch(patches[static_cast<size_t>(safePatchIndex)]);
        }
    }

    const float unisonVoices = m_propertySet.GetReal32(in_guidPlatform, "UnisonVoices");
    const float unisonDetune = m_propertySet.GetReal32(in_guidPlatform, "UnisonDetune");

    const bool chorusOn = m_propertySet.GetBool(in_guidPlatform, "ChorusOn");
    const float chorusRate = m_propertySet.GetReal32(in_guidPlatform, "ChorusRate");
    const float chorusDepthMs = m_propertySet.GetReal32(in_guidPlatform, "ChorusDepthMs");
    const float chorusMix = m_propertySet.GetReal32(in_guidPlatform, "ChorusMix");

    const bool delayOn = m_propertySet.GetBool(in_guidPlatform, "DelayOn");
    const float delayTimeMs = m_propertySet.GetReal32(in_guidPlatform, "DelayTimeMs");
    const float delayFeedback = m_propertySet.GetReal32(in_guidPlatform, "DelayFeedback");
    const float delayMix = m_propertySet.GetReal32(in_guidPlatform, "DelayMix");

    const bool reverbOn = m_propertySet.GetBool(in_guidPlatform, "ReverbOn");
    const float reverbMix = m_propertySet.GetReal32(in_guidPlatform, "ReverbMix");
    const float reverbDamp = m_propertySet.GetReal32(in_guidPlatform, "ReverbDamp");

    const bool chordOn = m_propertySet.GetBool(in_guidPlatform, "ChordOn");
    const int chordMode = m_propertySet.GetInt32(in_guidPlatform, "ChordMode");
    const int chordSpread = m_propertySet.GetInt32(in_guidPlatform, "ChordSpread");
    const int chordInversion = m_propertySet.GetInt32(in_guidPlatform, "ChordInversion");
    const int seqPattern = m_propertySet.GetInt32(in_guidPlatform, "SeqPattern");
    const float seqGate = m_propertySet.GetReal32(in_guidPlatform, "SeqGate");
    const bool seqStep1 = m_propertySet.GetBool(in_guidPlatform, "SeqStep1");
    const bool seqStep2 = m_propertySet.GetBool(in_guidPlatform, "SeqStep2");
    const bool seqStep3 = m_propertySet.GetBool(in_guidPlatform, "SeqStep3");
    const bool seqStep4 = m_propertySet.GetBool(in_guidPlatform, "SeqStep4");
    const bool seqStep5 = m_propertySet.GetBool(in_guidPlatform, "SeqStep5");
    const bool seqStep6 = m_propertySet.GetBool(in_guidPlatform, "SeqStep6");
    const bool seqStep7 = m_propertySet.GetBool(in_guidPlatform, "SeqStep7");
    const bool seqStep8 = m_propertySet.GetBool(in_guidPlatform, "SeqStep8");
    const bool unisonOn = m_propertySet.GetBool(in_guidPlatform, "UnisonOn");

    result &= in_dataWriter.WriteReal32(pitchBase);
    result &= in_dataWriter.WriteReal32(brightness);
    result &= in_dataWriter.WriteReal32(volume);

    result &= in_dataWriter.WriteInt32(rootNote);
    result &= in_dataWriter.WriteInt32(scaleMode);

    result &= in_dataWriter.WriteBool(arpOn);
    result &= in_dataWriter.WriteReal32(arpRate);
    result &= in_dataWriter.WriteInt32(arpMode);
    result &= in_dataWriter.WriteReal32(arpStepCount);

    result &= in_dataWriter.WriteReal32(fmRatioCarrier);
    result &= in_dataWriter.WriteReal32(fmRatioMod);
    result &= in_dataWriter.WriteReal32(fmIndex);
    result &= in_dataWriter.WriteReal32(fmCarrierLevel);
    result &= in_dataWriter.WriteReal32(fmModLevel);
    result &= in_dataWriter.WriteBool(useSysexPatch);
    result &= in_dataWriter.WriteInt32(patchIndex);
    result &= in_dataWriter.WriteReal32(fmFeedback);
    result &= in_dataWriter.WriteInt32(fmAlgorithmMacro);
    result &= in_dataWriter.WriteReal32(fmBrightnessBias);
    result &= in_dataWriter.WriteReal32(ampAttack);
    result &= in_dataWriter.WriteReal32(ampDecay);
    result &= in_dataWriter.WriteReal32(ampSustain);
    result &= in_dataWriter.WriteReal32(ampRelease);
    result &= in_dataWriter.WriteReal32(modAttack);
    result &= in_dataWriter.WriteReal32(modDecay);
    result &= in_dataWriter.WriteReal32(modSustain);
    result &= in_dataWriter.WriteReal32(modRelease);

    result &= in_dataWriter.WriteReal32(unisonVoices);
    result &= in_dataWriter.WriteReal32(unisonDetune);

    result &= in_dataWriter.WriteBool(chorusOn);
    result &= in_dataWriter.WriteReal32(chorusRate);
    result &= in_dataWriter.WriteReal32(chorusDepthMs);
    result &= in_dataWriter.WriteReal32(chorusMix);

    result &= in_dataWriter.WriteBool(delayOn);
    result &= in_dataWriter.WriteReal32(delayTimeMs);
    result &= in_dataWriter.WriteReal32(delayFeedback);
    result &= in_dataWriter.WriteReal32(delayMix);

    result &= in_dataWriter.WriteBool(reverbOn);
    result &= in_dataWriter.WriteReal32(reverbMix);
    result &= in_dataWriter.WriteReal32(reverbDamp);

    result &= in_dataWriter.WriteBool(chordOn);
    result &= in_dataWriter.WriteInt32(chordMode);
    result &= in_dataWriter.WriteInt32(chordSpread);
    result &= in_dataWriter.WriteInt32(seqPattern);
    result &= in_dataWriter.WriteInt32(chordInversion);
    result &= in_dataWriter.WriteReal32(seqGate);
    result &= in_dataWriter.WriteBool(seqStep1);
    result &= in_dataWriter.WriteBool(seqStep2);
    result &= in_dataWriter.WriteBool(seqStep3);
    result &= in_dataWriter.WriteBool(seqStep4);
    result &= in_dataWriter.WriteBool(seqStep5);
    result &= in_dataWriter.WriteBool(seqStep6);
    result &= in_dataWriter.WriteBool(seqStep7);
    result &= in_dataWriter.WriteBool(seqStep8);
    result &= in_dataWriter.WriteBool(unisonOn);
    result &= WriteRuntimePatch(in_dataWriter, runtimePatch);

    return result;
}

AK_DEFINE_PLUGIN_CONTAINER(ArpweaverFm);
AK_EXPORT_PLUGIN_CONTAINER(ArpweaverFm);
AK_ADD_PLUGIN_CLASS_TO_CONTAINER(
    ArpweaverFm,
    ArpweaverFmPlugin,
    ArpweaverFmSource
);
DEFINE_PLUGIN_REGISTER_HOOK
DEFINE_PLUGIN_ASSERT_HOOK;
