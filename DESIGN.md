---
name: schnellerTyp-e
description: A compact, keyboard-first multilingual typing utility with an Omarchy-inspired terminal grammar.
colors:
  ink-dark: "#e4e4df"
  ink-light: "#20201e"
  secondary-dark: "#b7b7af"
  secondary-light: "#4c4c46"
  muted-dark: "#999991"
  muted-light: "#62625a"
  background-dark: "#101010"
  background-light: "#f2f2ee"
  surface-dark: "#171717"
  surface-light: "#fafaf7"
  raised-dark: "#292929"
  raised-light: "#e2e2dc"
  border-dark: "#686868"
  border-light: "#777770"
  subtle-border-dark: "#383838"
  subtle-border-light: "#cdcdc5"
  running-dark: "#ffcf36"
  running-light: "#795900"
  warning-dark: "#e9ba68"
  warning-light: "#785112"
  error-dark: "#f29b91"
  error-light: "#a33025"
typography:
  display:
    fontFamily: "Consolas, monospace"
    fontSize: "21px"
    fontWeight: 600
  body:
    fontFamily: "Consolas, monospace"
    fontSize: "14px"
    fontWeight: 400
  label:
    fontFamily: "Consolas, monospace"
    fontSize: "12px"
    fontWeight: 600
rounded:
  none: "0px"
spacing:
  xs: "6px"
  sm: "8px"
  md: "16px"
  lg: "32px"
components:
  button:
    backgroundColor: "{colors.background-dark}"
    textColor: "{colors.ink-dark}"
    rounded: "{rounded.none}"
    height: "34px"
  input:
    backgroundColor: "{colors.surface-dark}"
    textColor: "{colors.ink-dark}"
    rounded: "{rounded.none}"
    height: "104px"
---

# Design System: schnellerTyp-e

## Overview

**Creative North Star: “The Omarchy Writing Utility”**

A multilingual typing tool expressed through a compact, monochrome terminal grammar: clear labels, thin rules, flat surfaces, and direct controls. It adapts the restrained panel grammar of [Cyberghost Omarchy](https://github.com/peteVo/cyberghost-omarchy-plugin) at source revision `d7fa8a8f269679cbb2e43cab71e024e61a1952f6`, while keeping its own visual identity and real Qt behavior. The source reference informs grouping and thin native controls; this app uses its own system-selected neutral palettes. Consolas on Windows and the platform monospace fallback elsewhere give the interface a consistent typographic voice.

The visual language supports one short path: choose a typing mode, enter raw keystrokes, inspect the actual engine preview, then move to preferences or setup when needed. Color remains quiet; the warm amber is reserved for a running state in dark mode. The system palette selects a neutral light or dark set.

**Key Characteristics:**
- Monospace, compact, and utility-led.
- Native Qt controls with square corners and thin borders.
- Real input and engine output are the primary work surface.

## Colors

Theme.qml provides paired neutral palettes selected from the operating system palette. Frontmatter names each mode-specific color; the actual semantic mapping is in `Theme.qml`.

### Primary
- **Running Amber:** Reserved for the connected/running state in dark mode; its light counterpart is a darker ochre for legibility.

### Neutral
- **Ink:** Primary text and selected-tab text.
- **Secondary and Muted Ink:** Supporting descriptions, labels, and quiet footer text.
- **Background, Surface, and Raised Surface:** Window canvas, control surfaces, and hover/raised states.
- **Borders:** Strong borders outline controls; subtle borders divide regions and distinguish output.

### Named Rules
**The State-Only Amber Rule.** Amber marks the connected/running status in dark mode; it is not a general action color. Primary actions use the neutral ink treatment.

## Typography

**Display Font:** Consolas (with monospace fallback)
**Body Font:** Consolas on Windows; platform monospace elsewhere
**Label/Mono Font:** Same family as body.

**Character:** Compact, legible, and technical without simulating a command prompt. Labels and values share the same family; size and weight establish hierarchy.

### Hierarchy
- **Display** (DemiBold, 21px): Product name.
- **Headline** (DemiBold, 18px): Page headings such as Typing preview, Preferences, and Setup.
- **Title** (DemiBold, 14px): Field names and section headings.
- **Body** (regular, 13–15px): Explanations and ordinary control text.
- **Label** (DemiBold, 10–12px): Compact metadata and uppercase field markers.

### Named Rules
**The Type-By-Size Rule.** Keep the monospace family consistent; create hierarchy through size, weight, and spacing.

## Layout

The desktop window opens at 820 × 740 px and remains usable down to 640 × 560 px. A compact status and enable row sits above three equal-width tabs—Typing, Preferences, Setup—and a vertically scrollable page. The main content column is centered and capped at 640 px. The typing page prioritizes mode selection, raw input, then live result. A quiet footer carries tray and quit affordances. Use 16 px as the main gap/padding rhythm, 8 px for tight group spacing, and 32 px outer content inset.

## Elevation & Depth

The interface is flat. It uses no shadows; thin borders, subtle divider lines, and a restrained raised neutral surface communicate grouping and hover state.

### Named Rules
**The Flat Surface Rule.** Do not introduce shadow-based depth; separate regions with tonal changes and one-pixel rules.

## Shapes

All controls and containers use square corners (0 px radius). A one-pixel border defines resting controls and the window frame; keyboard focus increases the control border to 2 px. Keep geometry simple and native to desktop controls.

## Components

### Buttons and Tabs
Compact, square Qt buttons keep familiar keyboard and accessibility semantics. Resting actions are transparent or surface-toned with a one-pixel border. Hover and press use the raised surface. The selected tab uses primary ink as its fill and the background color for its text. Focus uses a 2 px outline/border.

### Inputs / Fields
Editable input uses a surface fill, thin border, and 14 px inset. The raw-key practice field is 104 px tall; the read-only engine result is 128 px tall and uses the output surface. Both use wrapping monospace text. Focus changes the border to 2 px. Preserve text selection and normal keyboard navigation.

### Navigation
Three equal-width, checkable tabs stay visible above the scrollable page content. Keep their labels short and their selected state explicit in both fill and text color.

### Status and Switch
The top row pairs product identity with actual service readiness and the enable switch. Status includes words; color supplements the message. The switch remains a real Qt switch with a square indicator, visible focus, and a text label.

## Do's and Don'ts

### Do:
- **Do** use the system-selected neutral light or dark palette from `Theme.qml`.
- **Do** use Consolas on Windows and the platform monospace fallback on Linux.
- **Do** keep the typing flow centered on mode, raw keystrokes, and actual transformed output.
- **Do** use native Qt controls and preserve keyboard focus, accessibility names, disabled states, and text selection.
- **Do** express service state in words as well as color.

### Don't:
- **Don't** use amber as a general action color.
- **Don't** add fake terminal commands, CRT effects, glow, or imagery.
- **Don't** replace real Qt controls with decorative mock controls.
