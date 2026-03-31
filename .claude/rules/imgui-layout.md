# ImGui Layout Rules

## Panel layout constants
```cpp
constexpr float toolbar_height  = 40.f;
constexpr float captions_height = 80.f;
const float top_panels_height = io.DisplaySize.y - toolbar_height - captions_height;
```

Desktop widths (≥ 640 px):
- **Toolbar:** y=0, h=40, full width
- **Camera Feed:** 35% of window width, h=top_panels_height — shows live webcam when capturing, dark placeholder when idle
- **ASL Avatar:** 40% of window width, h=top_panels_height
- **Controls:** 25% of window width, h=`io.DisplaySize.y - toolbar_height` (extends to bottom)
- **Captions:** 75% width, y=`DisplaySize.y - 80`, h=80

Mobile mode (< 640 px): Avatar fills top, Captions strip 60 px at bottom. Both panels use `NoTitleBar | NoScrollbar`.

## Window flags
- Locked panels: `NoMove | NoResize | NoCollapse`
- Toolbar: `NoTitleBar | NoResize | NoMove | NoBringToFrontOnFocus`
- Always use `ImGuiCond_Always` for programmatically driven panel positions/sizes

## Styling rules
- Use `ImGui::PushStyleColor` / `PopStyleColor` for per-widget overrides — never modify `style.Colors` inside the render loop
- `apply_custom_style()` in `src/main.cpp` is the only place for global style overrides
- Button normal color uses `accent_color` (Windows DWM) via `PushStyleColor`; hover/active remain teal
- `ACCENT_TEAL_TR = IM_COL32(26, 188, 156, 200)` — use for placeholders and highlights

## Push/Pop discipline
- Every `ImGui::PushStyleColor` must have a matching `PopStyleColor` on all code paths
- Every `ImGui::Begin` must have a matching `ImGui::End` on all code paths — even early-exit branches
- `GetWindowDrawList()` calls must happen between `Begin` and `End` of that window
- `PushFont` / `PopFont`, `PushID` / `PopID` — same rule

## Fonts
- Calibri 16 px (primary, OversampleH=3, OversampleV=2, PixelSnapH=true)
- Segoe UI 16 px merged for symbol ranges `0x2190–0x23FF` and `0x25A0–0x26FF` (► ■ ● ⚙)
- Static glyph range arrays must be declared `static` — ImGui holds a pointer to them across frames
- Never add or resize fonts after `ImGui_ImplOpenGL3_Init` — the font atlas is already built

## Content region
- Use `GetContentRegionAvail()` for sizing widgets to remaining space in a panel
- Use `GetWindowSize()` only when you need the full window dimensions including title bar
- Do not confuse the two — `GetContentRegionAvail()` excludes padding and scrollbar
