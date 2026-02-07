#include "XeFG_Inputs_Dx12.h"

#include <Config.h>
#include <State.h>
#include <resource_tracking/ResTrack_dx12.h>

#include <xefg_swapchain.h>
#include <xefg_swapchain_d3d12.h>

#include <magic_enum.hpp>

void XeFG_Inputs_Dx12::CheckForFrame(IFGFeature_Dx12* fg, uint32_t frameId)
{
    std::scoped_lock lock(_frameBoundaryMutex);

    if (_isFrameFinished && _lastFrameId == _currentFrameId && frameId == 0 && frameId != _currentFrameId)
    {
        LOG_DEBUG("1> CheckForFrame: frameId={}, currentFrameId={}, lastFrameId={}, isFrameFinished={}", frameId,
                  _currentFrameId, _lastFrameId, _isFrameFinished);

        _isFrameFinished = false;

        fg->StartNewFrame();
        _currentIndex = fg->GetIndex();

        if (frameId != 0)
            _currentFrameId = frameId;
        else
            _currentFrameId = _lastFrameId + 1;

        _frameIdIndex[_currentIndex] = _currentFrameId;
    }
    else if (frameId != 0 && frameId > _currentFrameId)
    {
        LOG_DEBUG("2> CheckForFrame: frameId={}, currentFrameId={}, lastFrameId={}, isFrameFinished={}", frameId,
                  _currentFrameId, _lastFrameId, _isFrameFinished);

        _isFrameFinished = false;
        _lastFrameId = frameId - 1;

        fg->StartNewFrame();
        _currentIndex = fg->GetIndex();
        _currentFrameId = frameId;
        _frameIdIndex[_currentIndex] = _currentFrameId;
    }
}

int XeFG_Inputs_Dx12::IndexForFrameId(uint32_t frameId)
{
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        if (_frameIdIndex[i] == frameId)
            return i;
    }

    return -1;
}

bool XeFG_Inputs_Dx12::TagFrameConstants(void* swapChainContext, uint64_t frameId, void* constants)
{
    LOG_FUNC();

    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);

    if (fgOutput == nullptr)
        return true;

    CheckForFrame(fgOutput, frameId);

    auto constData = static_cast<xefg_swapchain_frame_constants_t*>(constants);
    if (constData == nullptr)
        return true;

    auto config = Config::Instance();

    // Extract and save XeFG configuration flags
    bool depthInverted = (constData->flags & XEFG_SWAPCHAIN_FRAME_FLAG_DEPTH_INVERTED) != 0;
    bool jitteredMV = (constData->flags & XEFG_SWAPCHAIN_FRAME_FLAG_JITTERED_MOTION_VECTORS) != 0;
    bool highResMV = (constData->flags & XEFG_SWAPCHAIN_FRAME_FLAG_HIGH_RES_MOTION_VECTORS) != 0;

    if (config->FGXeFGDepthInverted.value_or_default() != depthInverted ||
        config->FGXeFGJitteredMV.value_or_default() != jitteredMV ||
        config->FGXeFGHighResMV.value_or_default() != highResMV)
    {
        config->FGXeFGDepthInverted = depthInverted;
        config->FGXeFGJitteredMV = jitteredMV;
        config->FGXeFGHighResMV = highResMV;
        LOG_DEBUG("XeFG DepthInverted: {}", config->FGXeFGDepthInverted.value_or_default());
        LOG_DEBUG("XeFG JitteredMV: {}", config->FGXeFGJitteredMV.value_or_default());
        LOG_DEBUG("XeFG HighResMV: {}", config->FGXeFGHighResMV.value_or_default());
        config->SaveXeFG();
    }

    // Build FG constants
    FG_Constants fgConstants {};
    fgConstants.displayWidth = constData->output_width;
    fgConstants.displayHeight = constData->output_height;

    fgConstants.flags.reset();

    if (depthInverted)
        fgConstants.flags |= FG_Flags::InvertedDepth;

    if (jitteredMV)
        fgConstants.flags |= FG_Flags::JitteredMVs;

    if (highResMV)
        fgConstants.flags |= FG_Flags::DisplayResolutionMVs;

    if (config->FGAsync.value_or_default())
        fgConstants.flags |= FG_Flags::Async;

    // Camera parameters
    fgConstants.cameraNear = constData->camera_near;
    fgConstants.cameraFar = constData->camera_far;
    fgConstants.cameraFovAngleVertical = constData->vertical_fov;

    // Jitter
    fgConstants.jitterX = constData->jitter_offset_x;
    fgConstants.jitterY = constData->jitter_offset_y;

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

bool XeFG_Inputs_Dx12::TagFrameResource(void* swapChainContext, void* cmdList, uint64_t frameId, void* resourceData)
{
    LOG_FUNC();

    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(State::Instance().currentFG);

    if (fgOutput == nullptr || !fgOutput->IsActive())
        return true;

    auto resData = static_cast<xefg_swapchain_d3d12_resource_data_t*>(resourceData);
    if (resData == nullptr || resData->resource == nullptr)
        return true;

    auto index = IndexForFrameId(frameId);
    if (index < 0)
    {
        LOG_WARN("Invalid frame index for frameId: {}", frameId);
        return true;
    }

    // Map XeFG resource types to OptiScaler FG resource types
    FG_ResourceType optiType = FG_ResourceType::Color;
    
    switch (resData->type)
    {
    case XEFG_SWAPCHAIN_RESOURCE_TYPE_COLOR:
        optiType = FG_ResourceType::Color;
        break;
    case XEFG_SWAPCHAIN_RESOURCE_TYPE_DEPTH:
        optiType = FG_ResourceType::Depth;
        break;
    case XEFG_SWAPCHAIN_RESOURCE_TYPE_MOTION_VECTORS:
        optiType = FG_ResourceType::Velocity;
        break;
    case XEFG_SWAPCHAIN_RESOURCE_TYPE_UI:
        optiType = FG_ResourceType::UIColor;
        break;
    case XEFG_SWAPCHAIN_RESOURCE_TYPE_HUD_LESS:
        optiType = FG_ResourceType::HudlessColor;
        break;
    default:
        LOG_WARN("Unknown XeFG resource type: {}", resData->type);
        return true;
    }

    // Create Dx12Resource wrapper
    Dx12Resource dx12Res {};
    dx12Res.resource = resData->resource;
    dx12Res.state = resData->state;
    dx12Res.cmdList = static_cast<ID3D12GraphicsCommandList*>(cmdList);
    dx12Res.type = optiType;
    dx12Res.validity = FG_ResourceValidity::ValidNow;
    dx12Res.frameIndex = index;

    LOG_DEBUG("TagFrameResource: frameId={}, type={}, index={}", frameId, magic_enum::enum_name(optiType), index);

    // Set the resource on the FG output
    fgOutput->SetResource(&dx12Res);

    return true;
}

void XeFG_Inputs_Dx12::MarkPresent(uint64_t frameId)
{
    std::scoped_lock lock(_frameBoundaryMutex);
    
    _isFrameFinished = true;
    _lastFrameId = frameId;
}
