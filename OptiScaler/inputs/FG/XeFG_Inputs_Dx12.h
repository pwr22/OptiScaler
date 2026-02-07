#pragma once

#include <pch.h>
#include <framegen/IFGFeature_Dx12.h>

class XeFG_Inputs_Dx12
{
  private:
    inline static std::mutex _frameBoundaryMutex;
    inline static bool _isFrameFinished = true;

    inline static uint32_t _currentPresentId = 0;
    inline static uint32_t _currentIndex = -1;
    inline static uint32_t _lastPresentId = UINT32_MAX;
    inline static uint32_t _presentIdIndex[BUFFER_COUNT] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };

    static void CheckForFrame(IFGFeature_Dx12* fg, uint32_t presentId);
    static int IndexForPresentId(uint32_t presentId);

  public:
    static bool TagFrameConstants(void* swapChainContext, uint32_t presentId, void* constants);
    static bool TagFrameResource(void* swapChainContext, void* cmdList, uint32_t presentId, void* resourceData);
    static void MarkPresent(uint32_t presentId);
};
