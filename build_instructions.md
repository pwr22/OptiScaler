# Build Instructions

## Option 1: GitHub Actions (Recommended)

Since gh CLI requires authentication, please manually trigger the build:

1. Go to: https://github.com/pwr22/OptiScaler/actions/workflows/just_build_no_signature.yml
2. Click "Run workflow"
3. Select branch: `copilot/add-xess-frame-generation-support`
4. Click "Run workflow" button
5. Wait ~10 minutes for build to complete
6. Download artifact from the workflow run

## Option 2: Local Windows Build

If you have Windows with Visual Studio:

```bash
git clone https://github.com/pwr22/OptiScaler
cd OptiScaler
git checkout copilot/add-xess-frame-generation-support  
git submodule update --init --recursive
# Open OptiScaler.sln in Visual Studio
# Build → Build Solution (Release)
# Output in x64/Release/a/
```

## What Gets Built

The build produces:
- nvngx.dll (main DLL)
- dxgi.dll (DXGI proxy mode)
- winmm.dll (WinMM proxy mode)
- Other support DLLs

These DLLs contain the XeFG input interception implementation.

