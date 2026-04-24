# Requirement 2 - Feasibility and Execution Spec (V2.2)

## Conclusion
The requested upgrade is feasible, but should be implemented in staged form to keep the existing working plugin stable.

## Scope A - Full Arpeggiator (Ableton-like direction)
### Target
- Root note selectable (C, C#, D ... B).
- More scales/modes than current 6.
- Adjustable step count.
- Keep current rate control and RTPC compatibility.

### Recommended parameter additions
- `ArpStepCount` (int, 1..16)
- `ArpOctaveRange` (int, 1..4)
- `ArpGate` (float, 0.05..1.0)
- `ArpSwing` (float, 0..0.5)
- `PatternStep01..PatternStep16` (int semitone offsets, small range)

### Recommended additional scale modes
- Ionian (Major)
- Natural Minor
- Harmonic Minor
- Melodic Minor
- Dorian
- Phrygian
- Lydian
- Mixolydian
- Locrian
- Pentatonic Major
- Pentatonic Minor
- Blues

### Implementation note
- Keep quantization pipeline: raw note -> scale quantize -> output pitch.
- For stability, first keep fixed internal pattern buffer and expose only `ArpStepCount`.

## Scope B - Minimal FM Synth (Dexed-inspired, no Dexed dependency)
### Target
- Minimal FM engine, not a full Dexed clone.
- Patch/preset switching.
- Sysex support for external DX-style patch files.

### Practical architecture in Wwise plugin
- Runtime audio path should avoid network operations.
- Use offline import flow:
  1) User downloads `.syx` patch banks manually.
  2) A local converter script parses `.syx` -> normalized patch JSON/bin.
  3) Authoring-side plugin loads converted patch list and exposes `PatchIndex`.
  4) Bank build stores selected patch parameters for runtime playback.

### Minimal FM spec (safe V2.2)
- 2-operator FM core (Op1 carrier, Op2 modulator).
- Optional algorithm switch:
  - Algo 0: Op2 -> Op1
  - Algo 1: parallel mix
- Basic parameters:
  - `FmRatio1`, `FmRatio2`
  - `FmIndex`
  - `FmOp1Level`, `FmOp2Level`
  - `PatchIndex`

### Why not full Dexed-equivalent now
- 6-op DX7 behavior + exact sysex compatibility is large and high-risk for thesis timeline.
- A 2-op FM plus sysex-derived lightweight mapping is enough to demonstrate technical novelty.

## Scope C - Built-in effects
### Requested
- Unison
- Chorus
- Delay
- Reverb

### Recommended MVP-safe implementations
- `UnisonVoices` (1..4), `UnisonDetune` (0..0.2 semitone)
- Chorus: single modulated short delay line
- Delay: stereo feedback delay with simple HP/LP damping
- Reverb: small Schroeder-style comb/allpass block (lightweight)

### Risk control
- Implement in this order:
  1) Unison
  2) Chorus
  3) Delay
  4) Reverb
- Add bypass flags for each effect to simplify debugging.

## RTPC mapping extension (keep existing mappings)
- Speed -> ArpRate (keep)
- Health -> Volume (keep)
- Danger -> Brightness (keep)
- Mood -> ScaleMode (keep, expanded enum range)
- Optional new:
  - Combo/Intensity -> FmIndex
  - Environment -> ReverbMix

## UI implications (V2.2 visual target)
- Wwise custom Authoring GUI can be made VST-like with bitmap knobs and grouped panels.
- The plugin already has Win32 GUI entry files; these can be upgraded without changing DSP architecture.
- Parameter metadata in XML remains authoritative for automation/RTPC.

## Recommended execution milestones
1. Milestone M1: Arp v2 (scales + step count) and keep current synth path.
2. Milestone M2: 2-op FM core + patch index + local sysex converter tool.
3. Milestone M3: Unison + chorus + delay.
4. Milestone M4: Lightweight reverb + VST-like GUI skin.

## Acceptance criteria for Requirement 2
- Arp has selectable root, extended mode list, and adjustable step count.
- FM timbre is audibly different from saw baseline and patch switch works.
- At least one downloaded sysex bank can be converted and switched by patch index.
- Unison/chorus/delay/reverb can be enabled and heard independently.
- Existing UE5 RTPC flow still works.
