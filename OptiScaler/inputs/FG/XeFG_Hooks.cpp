#include <pch.h>
#include <Config.h>
#include <State.h>
#include <proxies/XeFG_Proxy.h>
#include <inputs/FG/XeFG_Inputs_Dx12.h>

#include <detours/detours.h>

// Original function pointers
static PFN_xefgSwapChainTagFrameConstants o_xefgSwapChainTagFrameConstants = nullptr;
static PFN_xefgSwapChainD3D12TagFrameResource o_xefgSwapChainD3D12TagFrameResource = nullptr;
static PFN_xefgSwapChainSetPresentId o_xefgSwapChainSetPresentId = nullptr;

xess_result_t hk_xefgSwapChainTagFrameConstants(xefg_swapchain_handle_t swapChainContext, uint64_t frameId,
                                                  const xefg_swapchain_frame_constants_t* constants)
{
    LOG_FUNC();

    // If XeFG is the active input, intercept and redirect
    if (State::Instance().activeFgInput == FGInput::XeFG && constants != nullptr)
    {
        // Make a mutable copy since we need to pass non-const pointer to our handler
        xefg_swapchain_frame_constants_t constCopy = *constants;
        XeFG_Inputs_Dx12::TagFrameConstants(swapChainContext, frameId, &constCopy);
    }

    // Call original if it exists (for passthrough or when XeFG is also the output)
    if (o_xefgSwapChainTagFrameConstants != nullptr)
        return o_xefgSwapChainTagFrameConstants(swapChainContext, frameId, constants);

    return XESS_RESULT_SUCCESS;
}

xess_result_t hk_xefgSwapChainD3D12TagFrameResource(xefg_swapchain_handle_t swapChainContext, ID3D12CommandList* pCommandList,
                                                      uint64_t frameId, const xefg_swapchain_d3d12_resource_data_t* resourceData)
{
    LOG_FUNC();

    // If XeFG is the active input, intercept and redirect
    if (State::Instance().activeFgInput == FGInput::XeFG && resourceData != nullptr)
    {
        // Make a mutable copy
        xefg_swapchain_d3d12_resource_data_t resCopy = *resourceData;
        XeFG_Inputs_Dx12::TagFrameResource(swapChainContext, pCommandList, frameId, &resCopy);
    }

    // Call original if it exists
    if (o_xefgSwapChainD3D12TagFrameResource != nullptr)
        return o_xefgSwapChainD3D12TagFrameResource(swapChainContext, pCommandList, frameId, resourceData);

    return XESS_RESULT_SUCCESS;
}

xess_result_t hk_xefgSwapChainSetPresentId(xefg_swapchain_handle_t swapChainContext, uint64_t presentId)
{
    LOG_FUNC();

    // Mark present for frame synchronization
    if (State::Instance().activeFgInput == FGInput::XeFG)
    {
        XeFG_Inputs_Dx12::MarkPresent(presentId);
    }

    // Call original if it exists
    if (o_xefgSwapChainSetPresentId != nullptr)
        return o_xefgSwapChainSetPresentId(swapChainContext, presentId);

    return XESS_RESULT_SUCCESS;
}

namespace XeFGHooks
{
bool InstallHooks()
{
    if (!XeFGProxy::InitXeFG())
    {
        LOG_INFO("XeFG library not available, skipping hooks");
        return false;
    }

    LOG_INFO("Installing XeFG input hooks...");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Hook TagFrameConstants
    o_xefgSwapChainTagFrameConstants = XeFGProxy::TagFrameConstants();
    if (o_xefgSwapChainTagFrameConstants != nullptr)
    {
        DetourAttach(&(PVOID&) o_xefgSwapChainTagFrameConstants, hk_xefgSwapChainTagFrameConstants);
        LOG_INFO("Hooked xefgSwapChainTagFrameConstants");
    }

    // Hook D3D12TagFrameResource
    o_xefgSwapChainD3D12TagFrameResource = XeFGProxy::D3D12TagFrameResource();
    if (o_xefgSwapChainD3D12TagFrameResource != nullptr)
    {
        DetourAttach(&(PVOID&) o_xefgSwapChainD3D12TagFrameResource, hk_xefgSwapChainD3D12TagFrameResource);
        LOG_INFO("Hooked xefgSwapChainD3D12TagFrameResource");
    }

    // Hook SetPresentId
    o_xefgSwapChainSetPresentId = XeFGProxy::SetPresentId();
    if (o_xefgSwapChainSetPresentId != nullptr)
    {
        DetourAttach(&(PVOID&) o_xefgSwapChainSetPresentId, hk_xefgSwapChainSetPresentId);
        LOG_INFO("Hooked xefgSwapChainSetPresentId");
    }

    auto result = DetourTransactionCommit();
    if (result != NO_ERROR)
    {
        LOG_ERROR("Failed to install XeFG hooks: {}", result);
        return false;
    }

    LOG_INFO("XeFG input hooks installed successfully");
    return true;
}
} // namespace XeFGHooks
