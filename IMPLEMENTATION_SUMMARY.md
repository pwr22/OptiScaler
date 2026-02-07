# XeFG Input Implementation Summary

## What Was Implemented

### 1. XeFG Input Handler (XeFG_Inputs_Dx12.cpp)
Intercepts and processes XeFG API calls from games:

- **TagFrameConstants**: Extracts camera parameters, jitter, flags (depth inverted, jittered MVs, high-res MVs)
- **TagFrameResource**: Captures frame resources (color, depth, motion vectors, UI, hudless)
- **MarkPresent**: Tracks frame synchronization

### 2. XeFG API Hooks (XeFG_Hooks.cpp)
Uses Microsoft Detours to hook libxess_fg.dll functions:

- **hk_xefgSwapChainTagFrameConstants**: Intercepts frame constant calls
- **hk_xefgSwapChainD3D12TagFrameResource**: Intercepts resource tagging calls  
- **hk_xefgSwapChainSetPresentId**: Intercepts present notifications
- **InstallHooks()**: Initializes all hooks

### 3. Integration (dllmain.cpp)
- Calls `XeFGHooks::InstallHooks()` when `FGInput = XeFG`
- Hooks are installed during OptiScaler initialization

### 4. Build System (OptiScaler.vcxproj)
Added to Visual Studio project:
- XeFG_Inputs_Dx12.h
- XeFG_Inputs_Dx12.cpp
- XeFG_Hooks.cpp

## How It Works

### Scenario: Game with XeSS FG → Redirect to FSR3 FG

1. **Game calls**: `xefgSwapChainTagFrameConstants(context, frameId, constants)`
2. **Hook intercepts**: `hk_xefgSwapChainTagFrameConstants()` is called
3. **Extract data**: Frame constants (camera, jitter, flags) extracted
4. **Pass to OptiScaler**: `XeFG_Inputs_Dx12::TagFrameConstants()` processes
5. **Build FG constants**: Convert XeFG format → OptiScaler FG_Constants
6. **Send to output**: `fgOutput->EvaluateState()` sends to any FG output
7. **FSR3 generates frames**: Selected FG output (FSR3, XeFG, etc.) generates frames

### Resource Flow

```
Game → xefgSwapChainD3D12TagFrameResource()
     ↓
OptiScaler Hook (hk_xefgSwapChainD3D12TagFrameResource)
     ↓
XeFG_Inputs_Dx12::TagFrameResource()
     ↓
Map XeFG resource types:
  - XEFG_SWAPCHAIN_RESOURCE_TYPE_COLOR → FG_ResourceType::Color
  - XEFG_SWAPCHAIN_RESOURCE_TYPE_DEPTH → FG_ResourceType::Depth
  - XEFG_SWAPCHAIN_RESOURCE_TYPE_MOTION_VECTORS → FG_ResourceType::Velocity
  - XEFG_SWAPCHAIN_RESOURCE_TYPE_UI → FG_ResourceType::UIColor
  - XEFG_SWAPCHAIN_RESOURCE_TYPE_HUD_LESS → FG_ResourceType::HudlessColor
     ↓
fgOutput->SetResource() (FSR3/XeFG/etc.)
```

## Configuration

User can now set in OptiScaler.ini:
```ini
[FrameGen]
FGInput=XeFG      # Intercept XeFG API calls from game
FGOutput=FSRFG    # Use FSR3 FG to generate frames
```

Or any other combination:
- `FGInput=XeFG` + `FGOutput=XeFG` - Passthrough (game XeFG → OptiScaler XeFG)
- `FGInput=XeFG` + `FGOutput=FSRFG` - Redirect (game XeFG → FSR3 FG)
- `FGInput=XeFG` + `FGOutput=Nukems` - Redirect (game XeFG → Nukem's FSR3-FG)

## Files Created/Modified

### Created:
1. OptiScaler/inputs/FG/XeFG_Inputs_Dx12.h (25 lines)
2. OptiScaler/inputs/FG/XeFG_Inputs_Dx12.cpp (220 lines)  
3. OptiScaler/inputs/FG/XeFG_Hooks.cpp (120 lines)

### Modified:
1. OptiScaler/dllmain.cpp - Added XeFG hook initialization
2. OptiScaler/proxies/XeFG_Proxy.h - Added hook declarations
3. OptiScaler/OptiScaler.vcxproj - Added files to build
4. OptiScaler/Config.cpp - Config serialization (previous commit)
5. OptiScaler/menu/menu_common.cpp - UI integration (previous commit)

## Testing Requirements

To test this implementation:

1. **Build** the project using Visual Studio or GitHub Actions
2. **Get a game** that uses native XeSS Frame Generation (when available)
3. **Configure** OptiScaler.ini with `FGInput=XeFG` and `FGOutput=FSRFG`
4. **Run game** and verify:
   - OptiScaler intercepts XeFG calls
   - FSR3 FG generates frames instead of XeFG
   - Performance and quality as expected

## Current Status

✅ Implementation complete
✅ Code committed to branch
⏳ Build pending (requires Windows build environment)
⏳ Testing pending (requires game with XeFG support)

## Notes

- XeFG is Intel's frame generation technology (very new)
- Currently, no known games use native XeFG API
- When games adopt XeFG, this implementation will enable:
  - Using FSR3 FG instead of XeFG
  - Using other FG outputs with XeFG input
  - Full compatibility with OptiScaler's FG system
