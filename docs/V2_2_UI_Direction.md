# ArpweaverFm V2.2 UI Direction (Draft)

## Visual Target (Captured)
- Style: dark, warm, analog, VST-like synthesizer panel
- Tone: bronze / brown gradient background, subtle glow, thin separators
- Controls: large rotary knobs, compact labels, grouped control blocks
- Priority: improve aesthetics first while preserving existing DSP behavior

## Mandatory V2.2 UI Constraints (Captured)
- Keep current audio feature scope unchanged unless explicitly requested
- UI should be custom and professional-looking, not default Wwise property list only
- Prefer reusable bitmap/SVG assets that are license-safe for thesis demo
- Keep implementation low-risk for Wwise Authoring plugin (Win32 GUI path)

## Open-Source Asset Candidates (Web-checked)
1. Kenney UI Pack - Adventure (CC0)
   - Link: https://kenney.nl/assets/ui-pack-adventure
   - Use: panel blocks, separators, button base styles, fallback UI parts
   - License: CC0

2. Game-icons "Round knob" (CC BY 3.0)
   - Link: https://game-icons.net/1x1/delapouite/round-knob.html
   - Use: vector/base knob symbol, can be adapted into knob cap overlays
   - License: CC BY 3.0 (attribution required)

3. OpenGameArt "Basic Knob" (OpenGameArt page)
   - Link: https://opengameart.org/content/basic-knob
   - Use: direct knob sprite/base texture candidate
   - Note: verify exact per-asset license on download page before packaging

4. OpenGameArt "Audio knobs, button, sliders etc"
   - Link: https://opengameart.org/content/audio-knobs-button-sliders-etc
   - Use: knob strip / slider alternatives for rapid prototype skinning
   - Note: verify exact per-asset license on download page before packaging

## License Handling Rule (Important)
- Preferred: CC0 / Public Domain assets for frictionless thesis packaging.
- If CC BY is used, include attribution in a `THIRD_PARTY_NOTICES.md` file.
- Before final shipping, each used asset must have: source URL + license line + author line.

## Next Execution Step (for next prompt)
- Build a concrete V2.2 art board:
  - select final knob set
  - select panel/background textures
  - define 1x/2x exported dimensions for Wwise Authoring GUI
  - generate `THIRD_PARTY_NOTICES.md`
