# dinput8 Wrapper — DCS FFB Filter

**Author:** Valmantas Palikša  
**License:** MIT — see [LICENSE](LICENSE)

A DirectInput8 proxy DLL that wraps the system `dinput8.dll` to intercept and
control Force Feedback (FFB) behaviour on a per-device basis. Primary use case:
disable or scale FFB for specific joystick types (e.g. vJoy) in DCS World.

## Features

- **Per-device FFB blocking** — completely disable FFB for devices matched by
  product name substring (e.g. vJoy)
- **Per-device FFB scaling** — scale force magnitudes to a percentage (0-100%)
- **FFB auto-restart after reconnect** — automatically restores running FFB
  effects (spring centering, trim forces, etc.) when a device is disconnected
  and reconnected mid-session, without requiring a mission restart
- **FFB effect logging** — log all FFB operations (CreateEffect, Start, Stop,
  SetParameters, SendForceFeedbackCommand) to a log file for debugging
- **INI-based configuration** — simple `dinput8.ini` config file, no registry
  or external dependencies
- **Full COM proxy** — wraps both `IDirectInput8A` and `IDirectInput8W`,
  `IDirectInputDevice8A/W`, and `IDirectInputEffect`
- **Null-effect fallback** — when FFB is blocked for a device that doesn't
  support it, returns a silent stub so the game never sees errors

## Building

### Requirements
- CMake 3.20+
- Clang/LLVM (clang-cl) — tested with Clang 20
- Windows SDK 10

### Build Steps
```powershell
# Debug build
cmake --preset clang-debug
cmake --build build-debug

# Release build
cmake --preset clang-release
cmake --build build
```

The output `dinput8.dll` is placed in the build directory.

## Installation

1. Copy `dinput8.dll` to the game directory (next to the game executable).
   For DCS World: `C:\Program Files\Eagle Dynamics\DCS World\bin\`
2. Copy `dinput8.ini` to the same directory.
3. Edit `dinput8.ini` to configure FFB rules for your devices.
4. Launch the game. A log file `dinput8_wrapper.log` will be created in the
   same directory.

## Configuration

Edit `dinput8.ini`:

```ini
[General]
Enabled=true        ; Master switch (false = pure pass-through)
LogLevel=3          ; 0=none, 1=error, 2=warn, 3=info, 4=debug

[FFB]
Enabled=true        ; Global FFB enable (false = block ALL devices)
LogEffects=true     ; Log every FFB operation to the log file
DefaultScale=100    ; Default force scale for all devices (0-100)
AutoRestart=true    ; Auto-restart FFB effects after device reconnection

[FFBDevices]
; Per-device rules — first substring match wins.
; Actions: block, allow, or 0-100 (scale percentage)
vJoy=block
; MSFFB 2=50        ; Example: scale to 50%
```

### Device Matching

Rules in `[FFBDevices]` match against the device's DirectInput **product name**
using case-insensitive substring search. The first matching rule wins.

## Architecture

```
Game (DCS)
  │
  ├── DirectInput8Create()
  │       │
  │   ┌───▼──────────────┐
  │   │ WrapperDInput8    │  ← intercepts CreateDevice
  │   └───┬──────────────┘
  │       │
  │   ┌───▼──────────────┐
  │   │ WrapperDevice8    │  ← intercepts CreateEffect, SendFFBCommand
  │   └───┬──────────────┘
  │       │
  │   ┌───▼──────────────┐
  │   │ WrapperEffect     │  ← intercepts Start/Stop/SetParams/Download
  │   └───┬──────────────┘
  │       │
  └───────▼───────────────
      Real dinput8.dll (System32)
```

## File Structure

```
├── CMakeLists.txt           # Build configuration
├── CMakePresets.json        # Clang presets (debug/release)
├── dinput8.def              # DLL export definitions
├── dinput8.ini              # Default configuration
├── README.md
├── docs/
│   └── PLAN-device-reconnect.md  # Design document for auto-restart feature
└── src/
    ├── dllmain.cpp              # DLL entry point + DirectInput8Create export
    ├── proxy.h/cpp              # Loads real system dinput8.dll
    ├── logger.h/cpp             # File-based logging
    ├── config.h/cpp             # INI parser + device policy resolution
    ├── ffb_filter.h/cpp         # FFB policy enforcement + effect logging
    ├── ffb_state_registry.h/cpp # Global FFB state tracking for auto-restart
    ├── wrapper_dinput8.h/cpp    # IDirectInput8 A/W wrapper
    ├── wrapper_device8.h/cpp    # IDirectInputDevice8 A/W wrapper
    └── wrapper_effect.h/cpp     # IDirectInputEffect wrapper
```

## License

MIT License — Copyright (c) 2026 Valmantas Palikša. See [LICENSE](LICENSE) for full text.
