#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "ArpweaverFmSourceParams.h"
#include "../Dx7RuntimeShared.h"
#include <array>
#include <vector>

class ArpweaverFmSource final : public AK::IAkSourcePlugin
{
public:
    ArpweaverFmSource();
    ~ArpweaverFmSource() override = default;

    AKRESULT Init(
        AK::IAkPluginMemAlloc* in_pAllocator,
        AK::IAkSourcePluginContext* in_pSourcePluginContext,
        AK::IAkPluginParam* in_pParams,
        AkAudioFormat& io_rFormat) override;

    AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;
    AKRESULT Reset() override;
    AKRESULT GetPluginInfo(AkPluginInfo& out_rPluginInfo) override;
    AkReal32 GetDuration() const override;
    AKRESULT TimeSkip(AkUInt32& io_uFrames) override;
    void Execute(AkAudioBuffer* io_pBuffer) override;

private:
    static constexpr AkUInt32 kMaxUnisonVoices = 4;
    static constexpr AkUInt32 kMaxChordNotes = 4;
    static constexpr AkUInt32 kMaxVoiceLayers = 16;
    static constexpr AkUInt32 kReverbLines = 3;
    static constexpr AkUInt32 kDx7OperatorCount = ArpweaverFmDx7Runtime::kOperatorCount;
    static constexpr AkUInt8 kEnvAttackStage = 0;
    static constexpr AkUInt8 kEnvDecayStage = 1;
    static constexpr AkUInt8 kEnvSustainStage = 2;
    static constexpr AkUInt8 kEnvReleaseStage = 3;

    struct Dx7EnvelopeState
    {
        std::array<AkUInt8, ArpweaverFmDx7Runtime::kEnvelopePointCount> rates{};
        std::array<AkUInt8, ArpweaverFmDx7Runtime::kEnvelopePointCount> levels{};
        AkInt32 outLevel = 0;
        AkInt32 rateScaling = 0;
        AkReal32 levelQ24 = 0.0f;
        AkReal32 targetQ24 = 0.0f;
        AkReal32 incrementQ24 = 0.0f;
        AkUInt32 holdSamples = 0u;
        AkUInt8 stage = 0u;
        bool rising = true;
        bool keyDown = true;
        bool active = false;
    };

    struct Dx7PitchEnvelopeState
    {
        std::array<AkUInt8, ArpweaverFmDx7Runtime::kEnvelopePointCount> rates{};
        std::array<AkUInt8, ArpweaverFmDx7Runtime::kEnvelopePointCount> levels{};
        AkReal32 levelQ24 = 0.0f;
        AkReal32 targetQ24 = 0.0f;
        AkReal32 incrementQ24 = 0.0f;
        AkUInt8 stage = 0u;
        bool rising = true;
        bool keyDown = true;
        bool active = false;
    };

    struct Dx7LfoState
    {
        AkUInt32 phaseQ32 = 0u;
        AkUInt32 deltaQ32 = 0u;
        AkUInt8 waveform = 0u;
        AkUInt8 randState = 0u;
        bool sync = false;
        AkUInt32 delayState = 0u;
        AkUInt32 delayInc = 0u;
        AkUInt32 delayInc2 = 0u;
    };

    static AkReal32 Clamp(AkReal32 in_v, AkReal32 in_lo, AkReal32 in_hi);
    static AkInt32 ClampI(AkInt32 in_v, AkInt32 in_lo, AkInt32 in_hi);
    static AkInt32 Mod12(AkInt32 in_x);
    static AkReal32 MidiToFrequency(AkReal32 in_midiNote);
    static AkInt32 ComputeDx7LogFrequency(AkInt32 in_midiNote, AkUInt8 in_mode, AkUInt8 in_coarse, AkUInt8 in_fine, AkUInt8 in_detune);
    static void InitDx7LookupTables(AkUInt32 in_sampleRate);
    static AkInt32 Dx7SinLookupQ24(AkInt32 in_phaseQ24);
    static AkInt32 Dx7Exp2LookupQ24(AkInt32 in_x);
    static AkInt32 Dx7FreqLookup(AkInt32 in_logFreqQ24);
    static AkInt32 QuantizeToScale(AkInt32 in_midiNote, AkInt32 in_rootNote, AkInt32 in_scaleMode);
    static AkReal32 ComputeLowPassAlpha(AkReal32 in_brightness, AkUInt32 in_sampleRate);
    static AkUInt32 NextRandom(AkUInt32& io_state);
    static AkInt32 ScaleOutLevel(AkInt32 in_outLevel);
    static AkInt32 ScaleVelocity(AkInt32 in_velocity, AkInt32 in_sensitivity);
    static AkInt32 ScaleRate(AkInt32 in_midiNote, AkInt32 in_sensitivity);
    static AkInt32 ScaleCurve(AkInt32 in_group, AkInt32 in_depth, AkInt32 in_curve);
    static AkInt32 ScaleLevel(AkInt32 in_midiNote, AkInt32 in_breakPoint, AkInt32 in_leftDepth, AkInt32 in_rightDepth, AkInt32 in_leftCurve, AkInt32 in_rightCurve);
    static AkReal32 ComputeDx7Ratio(AkUInt8 in_mode, AkUInt8 in_coarse, AkUInt8 in_fine);
    static AkReal32 ComputeDx7FixedHz(AkUInt8 in_coarse, AkUInt8 in_fine);
    static AkReal32 ComputeDx7DetuneMultiplier(AkInt32 in_detune, AkReal32 in_referenceHz);
    static void StartDx7Envelope(Dx7EnvelopeState& io_state, AkUInt32 in_sampleRate, AkUInt8 in_stage);
    static void StartDx7PitchEnvelope(Dx7PitchEnvelopeState& io_state, AkUInt32 in_sampleRate, AkUInt8 in_stage);
    static AkReal32 TickDx7Envelope(Dx7EnvelopeState& io_state, AkUInt32 in_sampleRate);
    static AkReal32 TickDx7PitchEnvelope(Dx7PitchEnvelopeState& io_state, AkUInt32 in_sampleRate);
    static AkReal32 Dx7EnvelopeQ24ToLinear(AkReal32 in_levelQ24);
    static void ResetDx7Lfo(Dx7LfoState& io_state, const ArpweaverFmDx7Runtime::PatchData& in_patch, AkUInt32 in_sampleRate);
    static AkInt32 TickDx7LfoValue(Dx7LfoState& io_state);
    static AkInt32 TickDx7LfoDelay(Dx7LfoState& io_state);
    static AkInt32 FloatToQ24(AkReal32 in_value);
    static AkReal32 Q24ToFloat(AkInt32 in_value);

    void InitializeFxState();
    void InitializeDx7State();
    void UpdateCurrentNote(bool in_forceRetrigger);
    void TriggerNoteEvent();
    AkReal32 AdvanceEnvelope(AkReal32& io_value, AkUInt8& io_stage, AkReal32 in_attackSeconds, AkReal32 in_decaySeconds, AkReal32 in_sustainLevel);
    AkInt32 SelectArpOffset(const ArpweaverFmRTPCParams& in_params);
    void InitializeDx7NoteState(AkUInt32 in_voiceIndex, AkInt32 in_noteMidi, AkReal32 in_detuneSemi, const ArpweaverFmRTPCParams& in_params);

    AkReal32 RenderManualFmVoice(AkUInt32 in_voiceIndex, AkReal32 in_baseFreq, const ArpweaverFmRTPCParams& in_params);
    AkReal32 RenderDx7Voice(AkUInt32 in_voiceIndex, const ArpweaverFmRTPCParams& in_params);
    void ProcessChorus(AkReal32 in_sample, const ArpweaverFmRTPCParams& in_params, AkReal32& out_l, AkReal32& out_r);
    void ProcessDelay(AkReal32 in_l, AkReal32 in_r, const ArpweaverFmRTPCParams& in_params, AkReal32& out_l, AkReal32& out_r);
    void ProcessReverb(AkReal32 in_l, AkReal32 in_r, const ArpweaverFmRTPCParams& in_params, AkReal32& out_l, AkReal32& out_r);

    ArpweaverFmSourceParams* m_params = nullptr;
    AkUInt32 m_sampleRate = 48000;

    AkReal32 m_lpState = 0.0f;
    AkReal32 m_currentFreq = 261.6256f;
    AkInt32 m_currentMidi = 60;

    AkUInt32 m_arpIndex = 0;
    AkUInt32 m_seqPulseIndex = 0;
    AkUInt32 m_samplesUntilNextStep = 0;
    AkUInt32 m_currentStepLengthSamples = 0;
    AkUInt32 m_currentGateSamplesRemaining = 0;
    AkUInt32 m_rngState = 0xA341316Cu;

    std::array<AkReal32, kMaxVoiceLayers> m_carrierPhase{};
    std::array<AkReal32, kMaxVoiceLayers> m_modPhase{};
    std::array<AkReal32, kMaxVoiceLayers> m_ampEnvelope{};
    std::array<AkReal32, kMaxVoiceLayers> m_modEnvelope{};
    std::array<AkUInt8, kMaxVoiceLayers> m_ampEnvelopeStage{};
    std::array<AkUInt8, kMaxVoiceLayers> m_modEnvelopeStage{};
    std::array<AkReal32, kMaxVoiceLayers> m_feedbackState{};

    std::array<std::array<AkInt32, kDx7OperatorCount>, kMaxVoiceLayers> m_dx7PhaseQ24{};
    std::array<AkInt32, kMaxVoiceLayers> m_voiceMidi{};
    std::array<AkReal32, kMaxVoiceLayers> m_voiceBaseFreq{};
    std::array<AkReal32, kMaxVoiceLayers> m_voiceDetuneSemi{};
    AkUInt32 m_activeVoiceCount = 1u;

    std::array<std::array<AkInt32, kDx7OperatorCount>, kMaxVoiceLayers> m_dx7BaseLogFreq{};
    std::array<std::array<Dx7EnvelopeState, kDx7OperatorCount>, kMaxVoiceLayers> m_dx7OpEnv{};
    std::array<Dx7PitchEnvelopeState, kMaxVoiceLayers> m_dx7PitchEnv{};
    std::array<Dx7LfoState, kMaxVoiceLayers> m_dx7Lfo{};
    std::array<std::array<AkInt32, kDx7OperatorCount>, kMaxVoiceLayers> m_dx7FeedbackHistoryA{};
    std::array<std::array<AkInt32, kDx7OperatorCount>, kMaxVoiceLayers> m_dx7FeedbackHistoryB{};

    std::vector<AkReal32> m_chorusBuffer;
    AkUInt32 m_chorusWrite = 0;
    AkReal32 m_chorusLfoPhase = 0.0f;

    std::vector<AkReal32> m_delayBufferL;
    std::vector<AkReal32> m_delayBufferR;
    AkUInt32 m_delayWrite = 0;

    std::array<std::vector<AkReal32>, kReverbLines> m_reverbBufferL;
    std::array<std::vector<AkReal32>, kReverbLines> m_reverbBufferR;
    std::array<AkUInt32, kReverbLines> m_reverbWriteL{};
    std::array<AkUInt32, kReverbLines> m_reverbWriteR{};
    AkReal32 m_reverbDampStateL = 0.0f;
    AkReal32 m_reverbDampStateR = 0.0f;
};
