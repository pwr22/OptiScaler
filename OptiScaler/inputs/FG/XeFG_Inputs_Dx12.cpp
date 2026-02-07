#include "XeFG_Inputs_Dx12.h"

#include <Config.h>
#include <State.h>
#include <resource_tracking/ResTrack_dx12.h>

#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>

#include <magic_enum.hpp>

void XeFG_Inputs_Dx12::CheckForFrame(IFGFeature_Dx12* fg, uint32_t presentId)
{
    std::scoped_lock lock(_frameBoundaryMutex);

    if (_isFrameFinished && _lastPresentId == _currentPresentId && presentId == 0 && presentId != _currentPresentId)
    {
        LOG_DEBUG("1> CheckForFrame: presentId={}, currentPresentId={}, lastPresentId={}, isFrameFinished={}", presentId,
                  _currentPresentId, _lastPresentId, _isFrameFinished);

        _isFrameFinished = false;

        fg->StartNewFrame();
        _currentIndex = fg->GetIndex();

        if (presentId != 0)
            _currentPresentId = presentId;
        else
            _currentPresentId = _lastPresentId + 1;

        _presentIdIndex[_currentIndex] = _currentPresentId;
    }
    else if (presentId != 0 && presentId > _currentPresentId)
    {
        LOG_DEBUG("2> CheckForFrame: presentId={}, currentPresentId={}, lastPresentId={}, isFrameFinished={}", presentId,
                  _currentPresentId, _lastPresentId, _isFrameFinished);

        _isFrameFinished = false;
        _lastPresentId = presentId - 1;

        fg->StartNewFrame();
        _currentIndex = fg->GetIndex();
        _currentPresentId = presentId;
        _presentIdIndex[_currentIndex] = _currentPresentId;
    }
}

int XeFG_Inputs_Dx12::IndexForPresentId(uint32_t presentId)
{
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        if (_presentIdIndex[i] == presentId)
            return i;
    }

    return -1;
}

bool XeFG_Inputs_Dx12::TagFrameConstants(void* swapChainContext, uint32_t presentId, void* constants)
{
    LOG_FUNC();

    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);

    if (fgOutput == nullptr)
        return true;

    CheckForFrame(fgOutput, presentId);

    auto constData = static_cast<const xefg_swapchain_frame_constant_data_t*>(constants);
    if (constData == nullptr)
        return true;

    auto config = Config::Instance();

    // Build FG constants
    FG_Constants fgConstants {};
    fgConstants.displayWidth = 0;  // Will be set by output
    fgConstants.displayHeight = 0; // Will be set by output

    fgConstants.flags.reset();

    // Note: XeFG API doesn't provide per-frame flags like depth inverted, jittered MVs, etc.
    // Those are set during swapchain initialization via XEFG_SWAPCHAIN_INIT_FLAG_*
    // We'll use the saved config values that were set during initialization
    if (config->FGXeFGDepthInverted.value_or_default())
        fgConstants.flags |= FG_Flags::InvertedDepth;

    if (config->FGXeFGJitteredMV.value_or_default())
        fgConstants.flags |= FG_Flags::JitteredMVs;

    if (config->FGXeFGHighResMV.value_or_default())
        fgConstants.flags |= FG_Flags::DisplayResolutionMVs;

    if (config->FGAsync.value_or_default())
        fgConstants.flags |= FG_Flags::Async;

    // XeFG provides matrices instead of camera parameters
    // We could extract near/far/FOV from the projection matrix, but for simplicity
    // we'll leave them as defaults (0) and let the FG output handle it
    fgConstants.cameraNear = 0.0f;
    fgConstants.cameraFar = 0.0f;
    fgConstants.cameraFovAngleVertical = 0.0f;

    // Jitter offsets (note: XeFG uses jitterOffsetX/Y, not jitterX/Y)
    fgConstants.jitterX = constData->jitterOffsetX;
    fgConstants.jitterY = constData->jitterOffsetY;

    // Motion vector scale
    fgConstants.mvScaleX = constData->motionVectorScaleX;
    fgConstants.mvScaleY = constData->motionVectorScaleY;

    fgOutput->EvaluateState(State::Instance().currentD3D12Device, fgConstants);

    if (!config->FGEnabled.value_or_default())
    {
        LOG_TRACE("FG not active or paused");
        return true;
    }
    else
    {
        if (!fgOutput->IsActive() && !fgOutput->IsPaused())
        {
            fgOutput->Activate();
        }
        else if (!fgOutput->IsActive() || fgOutput->IsPaused())
        {
            LOG_TRACE("FG not active or paused");
            return true;
        }
    }

    return true;
}

bool XeFG_Inputs_Dx12::TagFrameResource(void* swapChainContext, void* cmdList, uint32_t presentId, void* resourceData)
{
    LOG_FUNC();

    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);

    if (fgOutput == nullptr || !fgOutput->IsActive())
        return true;

    auto resData = static_cast<const xefg_swapchain_d3d12_resource_data_t*>(resourceData);
    if (resData == nullptr || resData->pResource == nullptr)
        return true;

    auto index = IndexForPresentId(presentId);
    if (index < 0)
    {
        LOG_WARN("Invalid frame index for presentId: {}", presentId);
        return true;
    }

    // Map XeFG resource types to OptiScaler FG resource types
    FG_ResourceType optiType = FG_ResourceType::Color;
    
    switch (resData->type)
    {
    case XEFG_SWAPCHAIN_RES_HUDLESS_COLOR:
        optiType = FG_ResourceType::HudlessColor;
        break;
    case XEFG_SWAPCHAIN_RES_DEPTH:
        optiType = FG_ResourceType::Depth;
        break;
    case XEFG_SWAPCHAIN_RES_MOTION_VECTOR:
        optiType = FG_ResourceType::Velocity;
        break;
    case XEFG_SWAPCHAIN_RES_UI:
        optiType = FG_ResourceType::UIColor;
        break;
    case XEFG_SWAPCHAIN_RES_BACKBUFFER:
        optiType = FG_ResourceType::Color;
        break;
    default:
        LOG_WARN("Unknown XeFG resource type: {}", resData->type);
        return true;
    }

    // Create Dx12Resource wrapper
    Dx12Resource dx12Res {};
    dx12Res.resource = resData->pResource;
    dx12Res.state = resData->incomingState;
    dx12Res.cmdList = static_cast<ID3D12GraphicsCommandList*>(cmdList);
    dx12Res.type = optiType;
    dx12Res.validity = FG_ResourceValidity::ValidNow;
    dx12Res.frameIndex = index;

    LOG_DEBUG("TagFrameResource: presentId={}, type={}, index={}", presentId, magic_enum::enum_name(optiType), index);

    // Set the resource on the FG output
    fgOutput->SetResource(&dx12Res);

    return true;
}

void XeFG_Inputs_Dx12::MarkPresent(uint32_t presentId)
{
    std::scoped_lock lock(_frameBoundaryMutex);
    
    _isFrameFinished = true;
    _lastPresentId = presentId;
}
