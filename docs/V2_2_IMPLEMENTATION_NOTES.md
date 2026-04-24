# ArpweaverFm v2 Implementation Notes

## Folder
- This v2 implementation is isolated in `ArpweaverFm_v2`.

## M1: Extended Arpeggiator
- 12 root notes (C through B).
- 18 scale modes:
  1. Major (Ionian)
  2. Natural Minor (Aeolian)
  3. Dorian
  4. Phrygian
  5. Lydian
  6. Mixolydian
  7. Locrian
  8. Harmonic Minor
  9. Melodic Minor
  10. Harmonic Major
  11. Pentatonic Major
  12. Pentatonic Minor
  13. Blues Minor
  14. Blues Major
  15. Whole Tone
  16. Diminished (Half-Whole)
  17. Diminished (Whole-Half)
  18. Chromatic
- Added `ArpStepCount` (1..16).
- Existing speed mapping remains via `ArpRate`.

## M2: Minimal FM + SysEx
- Replaced the core voice with a 2-operator FM voice.
- Added FM parameters:
  - `FmRatioCarrier`
  - `FmRatioMod`
  - `FmIndex`
  - `FmCarrierLevel`
  - `FmModLevel`
- Added SysEx controls:
  - `UseSysexPatch`
  - `PatchIndex`
  - `SysexFilePath`
- Authoring side (`GetBankParameters`) parses selected `.syx` file and resolves FM values during bank serialization.

## M3: Effects
- Added simple unison:
  - `UnisonVoices` (1..4)
  - `UnisonDetune`
- Added chorus:
  - `ChorusOn`
  - `ChorusRate`
  - `ChorusDepthMs`
  - `ChorusMix`
- Added delay:
  - `DelayOn`
  - `DelayTimeMs`
  - `DelayFeedback`
  - `DelayMix`
- Added reverb:
  - `ReverbOn`
  - `ReverbMix`
  - `ReverbDamp`

## M4: V2.2 UI Pass
- Added a custom Contents panel in Authoring (`ArpweaverFm.rc`).
- Added a direct `Load .syx` button in plugin UI.
- Added dark warm panel styling for the V2.2 visual direction.

## Important Runtime Rule
- Runtime audio generation does not load files from disk.
- SysEx is interpreted in Authoring while preparing bank parameters.
