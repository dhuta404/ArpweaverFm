#pragma once

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <array>

namespace ArpweaverFmDx7Runtime
{
static constexpr AkUInt32 kPatchBankMagic = 0x44583750u;
static constexpr AkUInt32 kPatchBankVersion = 2u;
static constexpr AkUInt32 kOperatorCount = 6u;
static constexpr AkUInt32 kEnvelopePointCount = 4u;

struct OperatorPatchData
{
    std::array<AkUInt8, kEnvelopePointCount> egRates{};
    std::array<AkUInt8, kEnvelopePointCount> egLevels{};
    AkUInt8 breakpoint = 0u;
    AkUInt8 leftDepth = 0u;
    AkUInt8 rightDepth = 0u;
    AkUInt8 leftCurve = 0u;
    AkUInt8 rightCurve = 0u;
    AkUInt8 rateScale = 0u;
    AkUInt8 ampModSensitivity = 0u;
    AkUInt8 velocitySensitivity = 0u;
    AkUInt8 outputLevel = 0u;
    AkUInt8 mode = 0u;
    AkUInt8 coarse = 1u;
    AkUInt8 fine = 0u;
    AkUInt8 detune = 7u;
};

struct PatchData
{
    AkUInt32 magic = kPatchBankMagic;
    AkUInt32 version = kPatchBankVersion;
    AkUInt8 hasPatch = 0u;
    AkUInt8 algorithm = 0u;
    AkUInt8 feedback = 0u;
    AkUInt8 oscSync = 0u;
    AkUInt8 lfoSpeed = 0u;
    AkUInt8 lfoDelay = 0u;
    AkUInt8 pitchModDepth = 0u;
    AkUInt8 ampModDepth = 0u;
    AkUInt8 lfoSync = 0u;
    AkUInt8 lfoWave = 0u;
    AkUInt8 pitchModSensitivity = 0u;
    AkUInt8 transpose = 24u;
    std::array<AkUInt8, kEnvelopePointCount> pitchRates{};
    std::array<AkUInt8, kEnvelopePointCount> pitchLevels{};
    std::array<OperatorPatchData, kOperatorCount> ops{};
};
} // namespace ArpweaverFmDx7Runtime
