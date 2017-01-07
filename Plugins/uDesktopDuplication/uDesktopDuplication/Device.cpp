#pragma once

#include <queue>
#include <d3d11.h>

#include "IUnityInterface.h"
#include "IUnityGraphicsD3D11.h"
#include "Device.h"
#include "Debug.h"
#include "Common.h"

#pragma comment(lib, "d3d11.lib")

using namespace Microsoft::WRL;



const Microsoft::WRL::ComPtr<ID3D11Texture2D>& SharedTextureWrapper::Get() const
{
    return pointer_;
}


Microsoft::WRL::ComPtr<ID3D11Texture2D> SharedTextureWrapper::Lock()
{
    if (locked_) return nullptr;
    locked_ = true;
    return pointer_;
}


void SharedTextureWrapper::Unlock()
{
    locked_ = false;
}


bool SharedTextureWrapper::IsLocked() const
{
    return locked_;
}



IsolatedD3D11Device::IsolatedD3D11Device(UINT cachedTextureNum)
{
    for (UINT i = 0; i < cachedTextureNum; ++i) 
    {
        cachedSharedTextures_.push_back(std::make_shared<SharedTextureWrapper>());
    }
}


IsolatedD3D11Device::~IsolatedD3D11Device()
{
}


HRESULT IsolatedD3D11Device::Create(const ComPtr<IDXGIAdapter>& adapter)
{
    const auto driverType = adapter ? 
        D3D_DRIVER_TYPE_UNKNOWN : 
        D3D_DRIVER_TYPE_HARDWARE;
    const auto flags =
        D3D11_CREATE_DEVICE_BGRA_SUPPORT;       // D2D Compatible
        // | D3D11_CREATE_DEVICE_VIDEO_SUPPORT  // MediaFoundation
    const D3D_FEATURE_LEVEL featureLevelsRequested[] = 
    {
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, 
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2, 
        D3D_FEATURE_LEVEL_9_1 
    };
    const UINT numLevelsRequested = sizeof(featureLevelsRequested) / sizeof(D3D_FEATURE_LEVEL);
    D3D_FEATURE_LEVEL featureLevelsSupported;

    return D3D11CreateDevice(
        adapter.Get(),
        driverType,
        nullptr,
        flags,
        featureLevelsRequested,
        numLevelsRequested,
        D3D11_SDK_VERSION,
        &device_,
        &featureLevelsSupported,
        nullptr);
}


ComPtr<ID3D11Device> IsolatedD3D11Device::GetDevice()
{ 
    return device_;
}


std::shared_ptr<SharedTextureWrapper> IsolatedD3D11Device::GetCompatibleSharedTexture(
    const ComPtr<ID3D11Texture2D>& src,
    UINT index)
{
    if (index < 0 || index >= cachedSharedTextures_.size())
    {
        Debug::Error("IsolatedD3D11Device::GetCompatibleSharedTexture() => ", index, " is out of cachedTextures range.");
        return nullptr;
    }

    auto& sharedTextureWrapper = cachedSharedTextures_.at(index);
    auto& texture = sharedTextureWrapper->pointer_; // raw pointer

    // check if the format and size of the current texture are same as the source one
    D3D11_TEXTURE2D_DESC srcDesc;
    src->GetDesc(&srcDesc);
    if (texture) 
    {
        D3D11_TEXTURE2D_DESC targetDesc;
        texture->GetDesc(&targetDesc);
        if (targetDesc.Format == srcDesc.Format && 
            targetDesc.Width  == srcDesc.Width  && 
            targetDesc.Height == srcDesc.Height)
        {
            return sharedTextureWrapper;
        }
    }

    // for sharing this texture with unity device
    srcDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    if (FAILED(device_->CreateTexture2D(&srcDesc, nullptr, &texture)))
    {
        Debug::Error("IsolatedD3D11Device::GetCompatibleSharedTexture() => Creating shared texture failed.");
        return nullptr;
    }

    return sharedTextureWrapper;
}