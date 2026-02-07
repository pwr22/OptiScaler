#pragma once

#include <pch.h>
#include <framegen/IFGFeature_Dx12.h>

class XeFG_Inputs_Dx12
{
  private:
    inline static std::mutex _frameBoundaryMutex;
    inline static bool _isFrameFinished = true;

    inline static uint32_t _currentFrameId = 0;
    inline static uint32_t _currentIndex = -1;
    inline static uint32_t _lastFrameId = UINT32_MAX;
    inline static uint32_t _frameIdIndex[BUFFER_COUNT] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };

    static void CheckForFrame(IFGFeature_Dx12* fg, uint32_t frameId);
    static int IndexForFrameId(uint32_t frameId);

  public:
    static bool TagFrameConstants(void* swapChainContext, uint64_t frameId, void* constants);
    static bool TagFrameResource(void* swapChainContext, void* cmdList, uint64_t frameId, void* resourceData);
    static void MarkPresent(uint64_t frameId);
};
