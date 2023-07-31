// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HLMediaPrivate.h"

#include "IMediaTextureSample.h"
#include "MediaUtils/Public/MediaObjectPool.h"

#include "ID3D11DynamicRHI.h"
#include "ID3D12DynamicRHI.h"

class FHLMediaTextureSample
    : public IMediaTextureSample
    , public IMediaPoolable
{
public:
	FHLMediaTextureSample(ID3D11Texture2D* InTexture, ID3D11ShaderResourceView* InShaderResourceView, HANDLE InSharedTextureHandle)
        : Duration(FTimespan::Zero())
        , Time(FTimespan::Zero())
    {
        D3D11_TEXTURE2D_DESC Desc;
        InTexture->GetDesc(&Desc);

		if (RHIGetInterfaceType() == ERHIInterfaceType::D3D12)
		{
			// MediaFoundation creates a DX11 texture.  To use this texture with DX12, open the shared handle with the DX12 RHI.
			TComPtr<ID3D12Resource> sharedMediaTexture;
			if (FAILED(GetID3D12DynamicRHI()->RHIGetDevice(0)->OpenSharedHandle(InSharedTextureHandle, IID_PPV_ARGS(&sharedMediaTexture))))
			{
				UE_LOG(LogHLMediaPlayer, Log, TEXT("ID3D12Device::OpenSharedHandle failed in FHLMediaTextureSample"));
				return;
			}

			Texture = GetID3D12DynamicRHI()->RHICreateTexture2DFromResource(PF_B8G8R8A8, TexCreate_Dynamic, FClearValueBinding::None, sharedMediaTexture.Get());
		}
		else
		{
			Texture = GetID3D11DynamicRHI()->RHICreateTexture2DFromResource(PF_B8G8R8A8, TexCreate_None, FClearValueBinding::Transparent, InTexture);
		}
    }

    virtual ~FHLMediaTextureSample()
    { 
    }

    bool Update(
        FTimespan InTime,
        FTimespan InDuration)
    {
        if (!Texture.IsValid())
        {
            return false;
        }

        Time = InTime;
        Duration = InDuration;

        return true;
    }

    // IMediaTextureSample
    virtual const void* GetBuffer() override
    {
        return nullptr;
    }

    virtual FIntPoint GetDim() const override
    {
        return Texture.IsValid() ? Texture->GetTexture2D()->GetSizeXY() : FIntPoint::ZeroValue;
    }

    virtual FTimespan GetDuration() const override
    {
        return Duration;
    }

    virtual EMediaTextureSampleFormat GetFormat() const override
    {
        return EMediaTextureSampleFormat::CharBGRA;
    }

    virtual FIntPoint GetOutputDim() const override
    {
        return Texture.IsValid() ? Texture->GetTexture2D()->GetSizeXY() : FIntPoint::ZeroValue;
    }

    virtual uint32 GetStride() const override
    {
        return Texture.IsValid() ? Texture->GetTexture2D()->GetSizeX() * 4 : 0;
    }

    virtual FRHITexture* GetTexture() const override
    {
        return Texture;
    }

    virtual FMediaTimeStamp GetTime() const override
    {
        return FMediaTimeStamp(Time);
    }

    virtual bool IsCacheable() const override
    {
        return true;
    }

    virtual bool IsOutputSrgb() const override
    {
        return true;
    }

    virtual void Reset() override
    {
        Time = FTimespan::Zero();
        Duration = FTimespan::Zero();
        Texture = nullptr;
    }

protected:
    /** The sample's texture resource. */
    TRefCountPtr<FRHITexture2D> Texture;

    /** Duration for which the sample is valid. */
    FTimespan Duration;

    /** Sample time. */
    FTimespan Time;
};
