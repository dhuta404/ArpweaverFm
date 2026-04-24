#include "ArpweaverFmSourceParams.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include <cmath>

namespace
{
bool ReadParamAsFloat(const void* in_pValue, AkUInt32 in_uParamSize, AkReal32& out_value)
{
    if (!in_pValue)
        return false;

    if (in_uParamSize == sizeof(AkReal32))
    {
        out_value = *reinterpret_cast<const AkReal32*>(in_pValue);
        return true;
    }
    if (in_uParamSize == sizeof(AkInt32))
    {
        out_value = static_cast<AkReal32>(*reinterpret_cast<const AkInt32*>(in_pValue));
        return true;
    }
    if (in_uParamSize == sizeof(AkUInt32))
    {
        out_value = static_cast<AkReal32>(*reinterpret_cast<const AkUInt32*>(in_pValue));
        return true;
    }
    if (in_uParamSize == sizeof(bool))
    {
        out_value = (*reinterpret_cast<const bool*>(in_pValue)) ? 1.0f : 0.0f;
        return true;
    }

    return false;
}

bool TryReadUInt32(AkUInt8*& io_pBlock, AkUInt32& io_uBlockSize, AkUInt32& out_value)
{
    if (io_uBlockSize < sizeof(AkUInt32))
        return false;

    out_value = READBANKDATA(AkUInt32, io_pBlock, io_uBlockSize);
    return true;
}

bool TryReadUInt8(AkUInt8*& io_pBlock, AkUInt32& io_uBlockSize, AkUInt8& out_value)
{
    if (io_uBlockSize < sizeof(AkUInt8))
        return false;

    out_value = READBANKDATA(AkUInt8, io_pBlock, io_uBlockSize);
    return true;
}
} // namespace

ArpweaverFmSourceParams::ArpweaverFmSourceParams(const ArpweaverFmSourceParams& in_other) = default;

AkReal32 ArpweaverFmSourceParams::Clamp(AkReal32 in_v, AkReal32 in_lo, AkReal32 in_hi)
{
    if (in_v < in_lo)
        return in_lo;
    if (in_v > in_hi)
        return in_hi;
    return in_v;
}

AkReal32 ArpweaverFmSourceParams::Clamp01(AkReal32 in_v)
{
    return Clamp(in_v, 0.0f, 1.0f);
}

AkInt32 ArpweaverFmSourceParams::ClampInt(AkInt32 in_v, AkInt32 in_lo, AkInt32 in_hi)
{
    if (in_v < in_lo)
        return in_lo;
    if (in_v > in_hi)
        return in_hi;
    return in_v;
}

AkInt32 ArpweaverFmSourceParams::RoundToInt(AkReal32 in_v)
{
    return static_cast<AkInt32>(std::floor(in_v + (in_v >= 0.0f ? 0.5f : -0.5f)));
}

AK::IAkPluginParam* ArpweaverFmSourceParams::Clone(AK::IAkPluginMemAlloc* in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, ArpweaverFmSourceParams(*this));
}

AKRESULT ArpweaverFmSourceParams::Init(AK::IAkPluginMemAlloc* /*in_pAllocator*/, const void* in_pParamsBlock, AkUInt32 in_uBlockSize)
{
    if (in_pParamsBlock && in_uBlockSize > 0)
        return SetParamsBlock(in_pParamsBlock, in_uBlockSize);

    return AK_Success;
}

AKRESULT ArpweaverFmSourceParams::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
    AK_PLUGIN_DELETE(in_pAllocator, this);
    return AK_Success;
}

AKRESULT ArpweaverFmSourceParams::SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_uBlockSize)
{
    if (!in_pParamsBlock || in_uBlockSize == 0)
        return AK_Success;

    AKRESULT eResult = AK_Success;
    AkUInt8* pParamsBlock = (AkUInt8*)in_pParamsBlock;

    m_params.PitchBase = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.Brightness = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.Volume = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.RootNote = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.ScaleMode = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.Bpm = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.ArpOn = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.ArpRate = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ArpMode = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.ArpStepCount = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.FmRatioCarrier = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.FmRatioMod = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.FmIndex = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.FmCarrierLevel = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.FmModLevel = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.UseSysexPatch = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.PatchIndex = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.FmFeedback = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.FmAlgorithmMacro = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.FmBrightnessBias = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.AmpAttack = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.AmpDecay = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.AmpSustain = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.AmpRelease = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ModAttack = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ModDecay = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ModSustain = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ModRelease = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.UnisonVoices = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.UnisonDetune = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.ChorusOn = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.ChorusRate = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ChorusDepthMs = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ChorusMix = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.DelayOn = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.DelayTimeMs = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.DelayFeedback = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.DelayMix = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.ReverbOn = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.ReverbMix = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.ReverbDamp = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);

    m_params.ChordOn = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.ChordMode = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.ChordSpread = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.SeqPattern = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.ChordInversion = READBANKDATA(AkInt32, pParamsBlock, in_uBlockSize);
    m_params.SeqGate = READBANKDATA(AkReal32, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[0] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[1] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[2] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[3] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[4] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[5] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[6] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.SeqSteps[7] = READBANKDATA(bool, pParamsBlock, in_uBlockSize);
    m_params.UnisonOn = READBANKDATA(bool, pParamsBlock, in_uBlockSize);

    m_params.HasDx7PatchData = false;
    m_params.Dx7PatchData = ArpweaverFmDx7Runtime::PatchData{};

    AkUInt32 magic = 0u;
    AkUInt32 version = 0u;
    if (TryReadUInt32(pParamsBlock, in_uBlockSize, magic) &&
        TryReadUInt32(pParamsBlock, in_uBlockSize, version) &&
        magic == ArpweaverFmDx7Runtime::kPatchBankMagic &&
        version == ArpweaverFmDx7Runtime::kPatchBankVersion)
    {
        m_params.Dx7PatchData.magic = magic;
        m_params.Dx7PatchData.version = version;

        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.hasPatch);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.algorithm);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.feedback);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.oscSync);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.lfoSpeed);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.lfoDelay);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.pitchModDepth);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.ampModDepth);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.lfoSync);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.lfoWave);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.pitchModSensitivity);
        TryReadUInt8(pParamsBlock, in_uBlockSize, m_params.Dx7PatchData.transpose);

        for (AkUInt8& value : m_params.Dx7PatchData.pitchRates)
            TryReadUInt8(pParamsBlock, in_uBlockSize, value);
        for (AkUInt8& value : m_params.Dx7PatchData.pitchLevels)
            TryReadUInt8(pParamsBlock, in_uBlockSize, value);

        for (auto& op : m_params.Dx7PatchData.ops)
        {
            for (AkUInt8& value : op.egRates)
                TryReadUInt8(pParamsBlock, in_uBlockSize, value);
            for (AkUInt8& value : op.egLevels)
                TryReadUInt8(pParamsBlock, in_uBlockSize, value);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.breakpoint);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.leftDepth);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.rightDepth);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.leftCurve);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.rightCurve);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.rateScale);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.ampModSensitivity);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.velocitySensitivity);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.outputLevel);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.mode);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.coarse);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.fine);
            TryReadUInt8(pParamsBlock, in_uBlockSize, op.detune);
        }

        m_params.HasDx7PatchData = (m_params.Dx7PatchData.hasPatch != 0u);
    }

    CHECKBANKDATASIZE(in_uBlockSize, eResult);

    m_params.PitchBase = Clamp(m_params.PitchBase, 24.0f, 96.0f);
    m_params.Brightness = Clamp01(m_params.Brightness);
    m_params.Volume = Clamp01(m_params.Volume);

    m_params.RootNote = ClampInt(m_params.RootNote, 0, 11);
    m_params.ScaleMode = ClampInt(m_params.ScaleMode, 0, 17);
    m_params.Bpm = Clamp(m_params.Bpm, 20.0f, 240.0f);

    m_params.ArpRate = Clamp(m_params.ArpRate, 0.25f, 24.0f);
    m_params.ArpMode = ClampInt(m_params.ArpMode, 0, 6);
    m_params.ArpStepCount = Clamp(m_params.ArpStepCount, 1.0f, 16.0f);

    m_params.FmRatioCarrier = Clamp(m_params.FmRatioCarrier, 0.125f, 16.0f);
    m_params.FmRatioMod = Clamp(m_params.FmRatioMod, 0.125f, 16.0f);
    m_params.FmIndex = Clamp(m_params.FmIndex, 0.0f, 20.0f);
    m_params.FmCarrierLevel = Clamp01(m_params.FmCarrierLevel);
    m_params.FmModLevel = Clamp01(m_params.FmModLevel);
    m_params.PatchIndex = ClampInt(m_params.PatchIndex, 0, 127);
    m_params.FmFeedback = Clamp01(m_params.FmFeedback);
    m_params.FmAlgorithmMacro = ClampInt(m_params.FmAlgorithmMacro, 0, 31);
    m_params.FmBrightnessBias = Clamp01(m_params.FmBrightnessBias);
    m_params.AmpAttack = Clamp(m_params.AmpAttack, 0.0025f, 3.0f);
    m_params.AmpDecay = Clamp(m_params.AmpDecay, 0.01f, 4.0f);
    m_params.AmpSustain = Clamp01(m_params.AmpSustain);
    m_params.AmpRelease = Clamp(m_params.AmpRelease, 0.02f, 5.0f);
    m_params.ModAttack = Clamp(m_params.ModAttack, 0.0025f, 3.0f);
    m_params.ModDecay = Clamp(m_params.ModDecay, 0.01f, 4.0f);
    m_params.ModSustain = Clamp01(m_params.ModSustain);
    m_params.ModRelease = Clamp(m_params.ModRelease, 0.02f, 5.0f);

    m_params.UnisonVoices = Clamp(m_params.UnisonVoices, 1.0f, 4.0f);
    m_params.UnisonDetune = Clamp(m_params.UnisonDetune, 0.0f, 0.35f);

    m_params.ChorusRate = Clamp(m_params.ChorusRate, 0.05f, 5.0f);
    m_params.ChorusDepthMs = Clamp(m_params.ChorusDepthMs, 0.0f, 30.0f);
    m_params.ChorusMix = Clamp01(m_params.ChorusMix);

    m_params.DelayTimeMs = Clamp(m_params.DelayTimeMs, 5.0f, 2000.0f);
    m_params.DelayFeedback = Clamp(m_params.DelayFeedback, 0.0f, 0.95f);
    m_params.DelayMix = Clamp01(m_params.DelayMix);

    m_params.ReverbMix = Clamp01(m_params.ReverbMix);
    m_params.ReverbDamp = Clamp01(m_params.ReverbDamp);
    m_params.ChordMode = ClampInt(m_params.ChordMode, 0, 11);
    m_params.ChordSpread = ClampInt(m_params.ChordSpread, 0, 4);
    m_params.ChordInversion = ClampInt(m_params.ChordInversion, 0, 3);
    m_params.SeqPattern = ClampInt(m_params.SeqPattern, 0, 4);
    m_params.SeqGate = Clamp(m_params.SeqGate, 0.05f, 1.0f);

    if (m_params.HasDx7PatchData)
    {
        m_params.Dx7PatchData.algorithm = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.algorithm, 0, 31));
        m_params.Dx7PatchData.feedback = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.feedback, 0, 7));
        m_params.Dx7PatchData.oscSync = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.oscSync, 0, 1));
        m_params.Dx7PatchData.lfoSpeed = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.lfoSpeed, 0, 99));
        m_params.Dx7PatchData.lfoDelay = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.lfoDelay, 0, 99));
        m_params.Dx7PatchData.pitchModDepth = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.pitchModDepth, 0, 99));
        m_params.Dx7PatchData.ampModDepth = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.ampModDepth, 0, 99));
        m_params.Dx7PatchData.lfoSync = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.lfoSync, 0, 1));
        m_params.Dx7PatchData.lfoWave = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.lfoWave, 0, 5));
        m_params.Dx7PatchData.pitchModSensitivity = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.pitchModSensitivity, 0, 7));
        m_params.Dx7PatchData.transpose = static_cast<AkUInt8>(ClampInt(m_params.Dx7PatchData.transpose, 0, 48));
        for (auto& op : m_params.Dx7PatchData.ops)
            op.velocitySensitivity = static_cast<AkUInt8>(ClampInt(op.velocitySensitivity, 0, 7));
    }

    return eResult;
}

AKRESULT ArpweaverFmSourceParams::SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_uParamSize)
{
    AkReal32 value = 0.0f;
    if (!ReadParamAsFloat(in_pValue, in_uParamSize, value))
        return AK_InvalidParameter;

    switch (in_paramID)
    {
    case PARAM_PITCH_BASE:
        m_params.PitchBase = Clamp(value, 24.0f, 96.0f);
        return AK_Success;

    case PARAM_BRIGHTNESS:
        m_params.Brightness = Clamp01(value);
        return AK_Success;

    case PARAM_VOLUME:
        m_params.Volume = Clamp01(value);
        return AK_Success;

    case PARAM_ROOT_NOTE:
        m_params.RootNote = ClampInt(RoundToInt(value), 0, 11);
        return AK_Success;

    case PARAM_SCALE_MODE:
        m_params.ScaleMode = ClampInt(RoundToInt(value), 0, 17);
        return AK_Success;

    case PARAM_BPM:
        m_params.Bpm = Clamp(value, 20.0f, 240.0f);
        return AK_Success;

    case PARAM_ARP_ON:
        m_params.ArpOn = (value >= 0.5f);
        return AK_Success;

    case PARAM_ARP_RATE:
        m_params.ArpRate = Clamp(value, 0.25f, 24.0f);
        return AK_Success;

    case PARAM_ARP_MODE:
        m_params.ArpMode = ClampInt(RoundToInt(value), 0, 6);
        return AK_Success;

    case PARAM_ARP_STEP_COUNT:
        m_params.ArpStepCount = Clamp(value, 1.0f, 16.0f);
        return AK_Success;

    case PARAM_FM_RATIO_CARRIER:
        m_params.FmRatioCarrier = Clamp(value, 0.125f, 16.0f);
        return AK_Success;

    case PARAM_FM_RATIO_MOD:
        m_params.FmRatioMod = Clamp(value, 0.125f, 16.0f);
        return AK_Success;

    case PARAM_FM_INDEX:
        m_params.FmIndex = Clamp(value, 0.0f, 20.0f);
        return AK_Success;

    case PARAM_FM_CARRIER_LEVEL:
        m_params.FmCarrierLevel = Clamp01(value);
        return AK_Success;

    case PARAM_FM_MOD_LEVEL:
        m_params.FmModLevel = Clamp01(value);
        return AK_Success;

    case PARAM_USE_SYSEX_PATCH:
        m_params.UseSysexPatch = (value >= 0.5f);
        return AK_Success;

    case PARAM_PATCH_INDEX:
        m_params.PatchIndex = ClampInt(RoundToInt(value), 0, 127);
        return AK_Success;

    case PARAM_FM_FEEDBACK:
        m_params.FmFeedback = Clamp01(value);
        return AK_Success;

    case PARAM_FM_ALGORITHM_MACRO:
        m_params.FmAlgorithmMacro = ClampInt(RoundToInt(value), 0, 31);
        return AK_Success;

    case PARAM_AMP_ATTACK:
        m_params.AmpAttack = Clamp(value, 0.0025f, 3.0f);
        return AK_Success;

    case PARAM_AMP_DECAY:
        m_params.AmpDecay = Clamp(value, 0.01f, 4.0f);
        return AK_Success;

    case PARAM_AMP_SUSTAIN:
        m_params.AmpSustain = Clamp01(value);
        return AK_Success;

    case PARAM_AMP_RELEASE:
        m_params.AmpRelease = Clamp(value, 0.02f, 5.0f);
        return AK_Success;

    case PARAM_UNISON_VOICES:
        m_params.UnisonVoices = Clamp(value, 1.0f, 4.0f);
        return AK_Success;

    case PARAM_UNISON_DETUNE:
        m_params.UnisonDetune = Clamp(value, 0.0f, 0.35f);
        return AK_Success;

    case PARAM_CHORUS_ON:
        m_params.ChorusOn = (value >= 0.5f);
        return AK_Success;

    case PARAM_CHORUS_RATE:
        m_params.ChorusRate = Clamp(value, 0.05f, 5.0f);
        return AK_Success;

    case PARAM_CHORUS_DEPTH_MS:
        m_params.ChorusDepthMs = Clamp(value, 0.0f, 30.0f);
        return AK_Success;

    case PARAM_CHORUS_MIX:
        m_params.ChorusMix = Clamp01(value);
        return AK_Success;

    case PARAM_DELAY_ON:
        m_params.DelayOn = (value >= 0.5f);
        return AK_Success;

    case PARAM_DELAY_TIME_MS:
        m_params.DelayTimeMs = Clamp(value, 5.0f, 2000.0f);
        return AK_Success;

    case PARAM_DELAY_FEEDBACK:
        m_params.DelayFeedback = Clamp(value, 0.0f, 0.95f);
        return AK_Success;

    case PARAM_DELAY_MIX:
        m_params.DelayMix = Clamp01(value);
        return AK_Success;

    case PARAM_REVERB_ON:
        m_params.ReverbOn = (value >= 0.5f);
        return AK_Success;

    case PARAM_REVERB_MIX:
        m_params.ReverbMix = Clamp01(value);
        return AK_Success;

    case PARAM_REVERB_DAMP:
        m_params.ReverbDamp = Clamp01(value);
        return AK_Success;

    case PARAM_CHORD_ON:
        m_params.ChordOn = (value >= 0.5f);
        return AK_Success;

    case PARAM_CHORD_MODE:
        m_params.ChordMode = ClampInt(RoundToInt(value), 0, 11);
        return AK_Success;

    case PARAM_CHORD_SPREAD:
        m_params.ChordSpread = ClampInt(RoundToInt(value), 0, 4);
        return AK_Success;

    case PARAM_SEQ_PATTERN:
        m_params.SeqPattern = ClampInt(RoundToInt(value), 0, 4);
        return AK_Success;

    case PARAM_CHORD_INVERSION:
        m_params.ChordInversion = ClampInt(RoundToInt(value), 0, 3);
        return AK_Success;

    case PARAM_SEQ_GATE:
        m_params.SeqGate = Clamp(value, 0.05f, 1.0f);
        return AK_Success;

    case PARAM_SEQ_STEP_1:
    case PARAM_SEQ_STEP_2:
    case PARAM_SEQ_STEP_3:
    case PARAM_SEQ_STEP_4:
    case PARAM_SEQ_STEP_5:
    case PARAM_SEQ_STEP_6:
    case PARAM_SEQ_STEP_7:
    case PARAM_SEQ_STEP_8:
    {
        const AkInt32 stepIndex = static_cast<AkInt32>(in_paramID) - static_cast<AkInt32>(PARAM_SEQ_STEP_1);
        m_params.SeqSteps[static_cast<size_t>(stepIndex)] = (value >= 0.5f);
        return AK_Success;
    }

    case PARAM_UNISON_ON:
        m_params.UnisonOn = (value >= 0.5f);
        return AK_Success;

    default:
        return AK_InvalidParameter;
    }
}
