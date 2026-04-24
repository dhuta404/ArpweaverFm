#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "../Dx7RuntimeShared.h"

enum ArpweaverFmParamID : AkPluginParamID
{
    PARAM_PITCH_BASE = 0,
    PARAM_BRIGHTNESS = 1,
    PARAM_VOLUME = 2,
    PARAM_ROOT_NOTE = 3,
    PARAM_SCALE_MODE = 4,
    PARAM_ARP_ON = 5,
    PARAM_ARP_RATE = 6,
    PARAM_ARP_MODE = 7,
    PARAM_ARP_STEP_COUNT = 8,

    PARAM_FM_RATIO_CARRIER = 9,
    PARAM_FM_RATIO_MOD = 10,
    PARAM_FM_INDEX = 11,
    PARAM_FM_CARRIER_LEVEL = 12,
    PARAM_FM_MOD_LEVEL = 13,
    PARAM_USE_SYSEX_PATCH = 14,
    PARAM_PATCH_INDEX = 15,

    PARAM_UNISON_VOICES = 16,
    PARAM_UNISON_DETUNE = 17,

    PARAM_CHORUS_ON = 18,
    PARAM_CHORUS_RATE = 19,
    PARAM_CHORUS_DEPTH_MS = 20,
    PARAM_CHORUS_MIX = 21,

    PARAM_DELAY_ON = 22,
    PARAM_DELAY_TIME_MS = 23,
    PARAM_DELAY_FEEDBACK = 24,
    PARAM_DELAY_MIX = 25,

    PARAM_REVERB_ON = 26,
    PARAM_REVERB_MIX = 27,
    PARAM_REVERB_DAMP = 28,

    PARAM_SYSEX_FILE_PATH = 29,
    PARAM_CHORD_ON = 30,
    PARAM_CHORD_MODE = 31,
    PARAM_CHORD_SPREAD = 32,
    PARAM_SEQ_PATTERN = 33,
    PARAM_FM_FEEDBACK = 34,
    PARAM_FM_ALGORITHM_MACRO = 35,
    PARAM_AMP_ATTACK = 36,
    PARAM_AMP_DECAY = 37,
    PARAM_AMP_SUSTAIN = 38,
    PARAM_AMP_RELEASE = 39,
    PARAM_CHORD_INVERSION = 40,
    PARAM_SEQ_GATE = 41,
    PARAM_SEQ_STEP_1 = 42,
    PARAM_SEQ_STEP_2 = 43,
    PARAM_SEQ_STEP_3 = 44,
    PARAM_SEQ_STEP_4 = 45,
    PARAM_SEQ_STEP_5 = 46,
    PARAM_SEQ_STEP_6 = 47,
    PARAM_SEQ_STEP_7 = 48,
    PARAM_SEQ_STEP_8 = 49,
    PARAM_UNISON_ON = 50,
    PARAM_BPM = 51
};

struct ArpweaverFmRTPCParams
{
    AkReal32 PitchBase = 60.0f;
    AkReal32 Brightness = 0.55f;
    AkReal32 Volume = 0.8f;

    AkInt32 RootNote = 0;
    AkInt32 ScaleMode = 0;
    AkReal32 Bpm = 60.0f;

    bool ArpOn = true;
    AkReal32 ArpRate = 6.0f;
    AkInt32 ArpMode = 0;
    AkReal32 ArpStepCount = 8.0f;

    AkReal32 FmRatioCarrier = 1.0f;
    AkReal32 FmRatioMod = 2.0f;
    AkReal32 FmIndex = 2.5f;
    AkReal32 FmCarrierLevel = 0.85f;
    AkReal32 FmModLevel = 0.75f;
    bool UseSysexPatch = false;
    AkInt32 PatchIndex = 0;
    AkReal32 FmFeedback = 0.0f;
    AkInt32 FmAlgorithmMacro = 0;
    AkReal32 FmBrightnessBias = 0.55f;
    AkReal32 AmpAttack = 0.02f;
    AkReal32 AmpDecay = 0.30f;
    AkReal32 AmpSustain = 0.85f;
    AkReal32 AmpRelease = 0.30f;
    AkReal32 ModAttack = 0.01f;
    AkReal32 ModDecay = 0.25f;
    AkReal32 ModSustain = 0.80f;
    AkReal32 ModRelease = 0.30f;

    AkReal32 UnisonVoices = 1.0f;
    AkReal32 UnisonDetune = 0.08f;

    bool ChorusOn = false;
    AkReal32 ChorusRate = 0.35f;
    AkReal32 ChorusDepthMs = 7.5f;
    AkReal32 ChorusMix = 0.25f;

    bool DelayOn = false;
    AkReal32 DelayTimeMs = 280.0f;
    AkReal32 DelayFeedback = 0.35f;
    AkReal32 DelayMix = 0.2f;

    bool ReverbOn = false;
    AkReal32 ReverbMix = 0.2f;
    AkReal32 ReverbDamp = 0.55f;

    bool ChordOn = false;
    AkInt32 ChordMode = 0;
    AkInt32 ChordSpread = 0;
    AkInt32 ChordInversion = 0;
    AkInt32 SeqPattern = 0;
    AkReal32 SeqGate = 0.8f;
    std::array<bool, 8> SeqSteps{ true, false, true, false, true, false, true, false };
    bool UnisonOn = false;

    bool HasDx7PatchData = false;
    ArpweaverFmDx7Runtime::PatchData Dx7PatchData;
};

class ArpweaverFmSourceParams final : public AK::IAkPluginParam
{
public:
    ArpweaverFmSourceParams() = default;
    ArpweaverFmSourceParams(const ArpweaverFmSourceParams& in_other);
    ~ArpweaverFmSourceParams() override = default;

    AK::IAkPluginParam* Clone(AK::IAkPluginMemAlloc* in_pAllocator) override;
    AKRESULT Init(AK::IAkPluginMemAlloc* in_pAllocator, const void* in_pParamsBlock, AkUInt32 in_uBlockSize) override;
    AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;
    AKRESULT SetParamsBlock(const void* in_pParamsBlock, AkUInt32 in_uBlockSize) override;
    AKRESULT SetParam(AkPluginParamID in_paramID, const void* in_pValue, AkUInt32 in_uParamSize) override;

    const ArpweaverFmRTPCParams& GetParams() const { return m_params; }

private:
    static AkReal32 Clamp(AkReal32 in_v, AkReal32 in_lo, AkReal32 in_hi);
    static AkReal32 Clamp01(AkReal32 in_v);
    static AkInt32 ClampInt(AkInt32 in_v, AkInt32 in_lo, AkInt32 in_hi);
    static AkInt32 RoundToInt(AkReal32 in_v);

    ArpweaverFmRTPCParams m_params;
};
