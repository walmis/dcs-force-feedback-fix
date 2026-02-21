# Implementation Plan: FFB Effect Auto-Restart After Device Reconnection

## Problem Statement

DCS World does not properly handle joystick disconnection/reconnection with
respect to Force Feedback (FFB) effects. The observed behaviour:

1. Device is connected, DCS creates FFB effects (Spring, ConstantForce, Square)
2. `Effect.Start()` is called when the mission loads — FFB works
3. Device is disconnected mid-session (USB unplug, firmware update, etc.)
4. Device is reconnected
5. DCS detects reconnection (via `WM_DEVICECHANGE` or similar)
6. DCS calls `DirectInput8Create` again and re-enumerates all devices
7. DCS calls `CreateDevice` for the reconnected joystick — **new device object**
8. DCS calls `CreateEffect` for Spring, ConstantForce, Square — **new effects**
9. **DCS does NOT call `Effect.Start()` on the new effects** — bug

Result: Spring centering, trim forces, and vibration effects are dead until
the mission is fully restarted.

### Evidence from wrapper log

```
09:07:09 — Initial startup:
  CreateDevice: [VPforce Rhino FFB Joystick]
  CreateEffect: type=Spring           ← created
  CreateEffect: type=ConstantForce    ← created
  CreateEffect: type=Square           ← created

09:09:45 — Mission start:
  Effect.Start: iterations=1  flags=0x0   ← FFB active, working

09:09:56 — Device reconnect detected by DCS:
  DirectInput8Create (version=0x00000800)   ← new DI8 instance
  CreateDevice: [VPforce Rhino FFB Joystick]  ← new device
  CreateEffect: type=Spring           ← recreated
  CreateEffect: type=ConstantForce    ← recreated
  CreateEffect: type=Square           ← recreated
  (NO Effect.Start calls!)            ← BUG: effects dead
```

DCS re-creates everything but forgets to `Start()` the new effects.

---

## Solution: FFB Effect Auto-Start

### Core Idea

The wrapper maintains a **global per-device-name registry** of previously
running FFB effects. When DCS creates a new effect (via `CreateEffect`) on
a device whose previous incarnation had effects running, the wrapper
automatically calls `Start()` on the real effect after creation.

```
┌──────────────────────────────────────────────────────────────────┐
│  Global FFB State Registry (keyed by device product name)        │
│                                                                  │
│  "VPforce Rhino FFB Joystick" → {                                │
│      Spring:        { running=true, iterations=1, flags=0x0,  }  │
│      ConstantForce: { running=true, iterations=1, flags=0x0,  }  │
│      Square:        { running=true, iterations=1, flags=0x0,  }  │
│  }                                                               │
└──────────────────────────────────────────────────────────────────┘
```

When `CreateEffect(GUID_Spring)` is called on a newly created
"VPforce Rhino FFB Joystick" device, the wrapper checks the registry,
sees that Spring was previously running, and auto-calls `Start(1, 0)`.

---

## Detailed Design

### Component 1: EffectStateRecord

Captures the last-known state of a single effect.

```cpp
struct EffectStateRecord {
    GUID   guid;              // GUID_Spring, GUID_ConstantForce, etc.
    bool   wasRunning;        // was Start() called without matching Stop()?
    DWORD  lastIterations;    // from last Start() call
    DWORD  lastStartFlags;    // from last Start() call

    // Last known parameters (deep-copied) for optional replay
    bool           hasParams;
    DIEFFECT       params;
    std::vector<DWORD> axes;
    std::vector<LONG>  directions;
    std::vector<BYTE>  typeSpecific;
    DIENVELOPE     envelope;
    bool           hasEnvelope;
};
```

### Component 2: FFBStateRegistry (global singleton)

```cpp
class FFBStateRegistry {
public:
    static FFBStateRegistry& instance();

    // Record that an effect was started/stopped/parameterised
    void recordStart(const std::wstring& deviceName, REFGUID effectGuid,
                     DWORD iterations, DWORD flags);
    void recordStop(const std::wstring& deviceName, REFGUID effectGuid);
    void recordParams(const std::wstring& deviceName, REFGUID effectGuid,
                      const DIEFFECT* params);

    // Query: was this effect type previously running on this device?
    bool wasRunning(const std::wstring& deviceName, REFGUID effectGuid,
                    DWORD& outIterations, DWORD& outFlags) const;

    // Query: get last known params for replay
    const EffectStateRecord* getRecord(const std::wstring& deviceName,
                                       REFGUID effectGuid) const;

    // Clear all records for a device (e.g. on clean shutdown)
    void clearDevice(const std::wstring& deviceName);

private:
    // Map: device name → (effect GUID → record)
    std::map<std::wstring, std::map<GUID, EffectStateRecord>> m_records;
    mutable std::mutex m_mutex;
};
```

### Component 3: WrapperEffect modifications

**Recording:** Each `WrapperEffect` reports state changes to the registry.

```cpp
// In WrapperEffect::Start():
HRESULT WrapperEffect::Start(DWORD dwIterations, DWORD dwFlags) {
    // Record to global registry BEFORE forwarding
    FFBStateRegistry::instance().recordStart(
        m_filter->deviceName(), m_guid, dwIterations, dwFlags);
    // ... existing Start logic ...
}

// In WrapperEffect::Stop():
HRESULT WrapperEffect::Stop() {
    FFBStateRegistry::instance().recordStop(
        m_filter->deviceName(), m_guid);
    // ... existing Stop logic ...
}

// In WrapperEffect::SetParameters():
HRESULT WrapperEffect::SetParameters(LPCDIEFFECT peff, DWORD dwFlags) {
    FFBStateRegistry::instance().recordParams(
        m_filter->deviceName(), m_guid, peff);
    // ... existing SetParameters logic ...
}
```

### Component 4: WrapperDevice8::CreateEffect modifications

**Auto-start:** When creating an effect, check the registry and auto-start.

```cpp
HRESULT WrapperDevice8::CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff,
                                     LPDIRECTINPUTEFFECT* ppdeff,
                                     LPUNKNOWN punkOuter)
{
    // ... existing creation logic (create real + wrap) ...

    // After successful creation, check if this effect type was
    // previously running on a device with the same name
    auto& registry = FFBStateRegistry::instance();
    DWORD iterations, startFlags;
    if (registry.wasRunning(m_filter->deviceName(), rguid,
                            iterations, startFlags))
    {
        LOG_INFO("FFB [%ls] Auto-starting %s (was running before reconnect)"
                 " iterations=%lu flags=0x%lx",
                 m_filter->deviceName().c_str(),
                 FFBFilter::effectGuidToString(rguid),
                 iterations, startFlags);

        // Optionally replay last known parameters first
        const auto* record = registry.getRecord(
            m_filter->deviceName(), rguid);
        if (record && record->hasParams) {
            (*ppdeff)->SetParameters(&record->params,
                DIEP_GAIN | DIEP_TYPESPECIFICPARAMS |
                DIEP_AXES | DIEP_DIRECTION | DIEP_ENVELOPE |
                DIEP_DURATION | DIEP_SAMPLEPERIOD);
        }

        // Auto-start the effect
        (*ppdeff)->Start(iterations, startFlags);
    }

    return hr;
}
```

---

## Edge Cases

### 1. Multiple devices with the same product name

DCS log shows the VPforce Rhino appears twice (possibly joystick + pedals,
both named similarly). The registry must handle this. Options:

**Option A (recommended):** Key by product name + instance GUID.
Problem: instance GUID may change across reconnection if vJoy reassigns IDs.

**Option B:** Key by product name only, store effects per-name.
Works because typically only one device of a given name has FFB.
If there are two identical FFB devices, effects for both are merged into one
set — acceptable since they'd have the same effect types.

**Option C:** Key by product name + effect-type set signature.
Match based on the combination of device name and which effect types were
created together.

**Recommendation:** Start with Option B (product name only). If testing shows
two identical FFB devices interfering, upgrade to Option A with fallback.

### 2. Effect parameters change between Start calls

DCS may call `SetParameters()` many times (e.g. updating ConstantForce
magnitude for trim). The registry stores the **last** params before
disconnection. On reconnect, those params are replayed. This is correct
because the trim state is captured.

### 3. Multiple reconnect cycles

Each reconnection creates new wrapper objects. The registry persists across
all of them since it's a global singleton. It's updated in-place — no
accumulation or growth issues.

### 4. Effect was stopped before disconnect

If the game stopped an effect before the device disconnected, the registry
records `wasRunning = false` and auto-start is NOT triggered. Correct.

### 5. Race: DCS calls Start() after our auto-start

If DCS does eventually call `Start()` on the new effects (e.g. in a future
DCS patch), calling `Start()` twice is harmless — DirectInput just restarts
the effect. No harm done.

### 6. DCS calls CreateEffect during initial startup (not reconnect)

On first startup the registry is empty, so `wasRunning()` returns false and
no auto-start happens. The normal flow proceeds. Correct.

---

## State Lifecycle

```
First run:
  CreateDevice("Rhino") → new WrapperDevice8
  CreateEffect(Spring)  → new WrapperEffect [registry: empty, no auto-start]
  CreateEffect(CF)      → new WrapperEffect [registry: empty, no auto-start]
  Start(Spring)          → registry: {Rhino → {Spring: running=true}}
  Start(CF)              → registry: {Rhino → {CF: running=true}}
  SetParams(CF, mag=500) → registry: {Rhino → {CF: params={mag=500}}}
  ...
  <<< DEVICE DISCONNECT >>>
  <<< DEVICE RECONNECT  >>>
  ...
  CreateDevice("Rhino") → new WrapperDevice8
  CreateEffect(Spring)  → new WrapperEffect
                           registry.wasRunning("Rhino", Spring) → YES
                           → AUTO: replay params + Start(1, 0)  ← FIXED!
  CreateEffect(CF)      → new WrapperEffect
                           registry.wasRunning("Rhino", CF) → YES
                           → AUTO: replay params (mag=500) + Start(1, 0)  ← FIXED!
```

---

## Configuration

Add to `[FFB]` section:

```ini
[FFB]
; Automatically restart FFB effects after device reconnection.
; The wrapper remembers which effects were running and auto-starts them
; when DCS recreates them after a reconnect.
AutoRestart=true
```

---

## Implementation Steps

| # | Task | Files | Risk | Size |
|---|------|-------|------|------|
| 1 | Create `EffectStateRecord` struct | `ffb_state_registry.h` | Low | S |
| 2 | Create `FFBStateRegistry` singleton | `ffb_state_registry.h/cpp` | Low | M |
| 3 | Hook `WrapperEffect::Start/Stop/SetParams` to record state | `wrapper_effect.cpp` | Low | S |
| 4 | Deep-copy `DIEFFECT` params in `recordParams` | `ffb_state_registry.cpp` | Med | M |
| 5 | Add auto-start check in `WrapperDevice8::CreateEffect` | `wrapper_device8.cpp` | Med | S |
| 6 | Add `AutoRestart` config option | `config.h/cpp`, `dinput8.ini` | Low | S |
| 7 | Add `ffb_state_registry.cpp` to CMakeLists.txt | `CMakeLists.txt` | Low | S |
| 8 | Build and unit-test deep-copy logic | — | Low | S |
| 9 | Integration test: VPforce disconnect/reconnect | — | Med | Manual |
| 10 | Integration test: multiple rapid reconnects | — | Med | Manual |

Estimated total effort: ~2-3 hours of implementation.

---

## Comparison with Previous Plan (Transparent Reconnection)

The original plan assumed DCS keeps old device handles and the wrapper must
silently reconnect underneath. The log evidence shows this is **not** the case:
DCS detects disconnection independently and creates entirely new device/effect
objects. This makes the solution much simpler:

| Aspect | Transparent Reconnection (old) | Auto-Start (new) |
|--------|-------------------------------|-------------------|
| Complexity | High (device state machine, deep-copy data format, re-acquire, SEH) | Low (global registry + auto-start) |
| Risk | High (driver crash, stale COM objects) | Low (just one extra Start() call) |
| DCS compatibility | Fights DCS's reconnection logic | Works *with* DCS's reconnection logic |
| Effect on non-FFB | Requires disconnected-mode stubs for all methods | No change to non-FFB methods |
| Code changes | 8+ files, 500+ lines | 3-4 files, ~200 lines |

The auto-start approach is strictly better for the actual DCS behavior.

---

## Open Questions

1. **VPforce Rhino appears twice in the log** — one for joystick and one for pedals?
   Both get Spring/ConstantForce/Square. Need to verify whether the product
   names are identical or subtly different (the log shows "VPforce Rhino FFB
   Joystick" for one, need to check if the second is "VPforce Rhino FFB Pedals").

2. **Should auto-start replay SetParameters too?** The last trim magnitude
   might be important. If DCS sends a `SetParameters` after `CreateEffect`
   (before the missing `Start`), our auto-replayed params would be
   immediately overwritten — which is fine. If DCS does NOT send SetParameters
   on reconnect, then replaying the last known params restores the correct
   force state.

3. **Timing:** Does DCS call `CreateEffect` synchronously during the
   reconnect sequence, or could there be a delay? The log shows it's all
   synchronous within the same millisecond. Safe to auto-start immediately
   in `CreateEffect`.
