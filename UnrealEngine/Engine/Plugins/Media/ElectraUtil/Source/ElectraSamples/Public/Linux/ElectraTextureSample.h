// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"
#include "MediaObjectPool.h"
#include "RHI.h"
#include "RHIUtilities.h"

#include "Linux/MediaVideoDecoderOutputLinux.h"

class FRHITexture;

class FElectraTextureSampleLinux
	: public IMediaTextureSample
	, public IMediaPoolable
{
public:
	FElectraTextureSampleLinux() = default;

	virtual	~FElectraTextureSampleLinux();

public:
	void Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
	{
		VideoDecoderOutput = StaticCastSharedPtr<FVideoDecoderOutputLinux, IDecoderOutputPoolable, ESPMode::ThreadSafe>(InVideoDecoderOutput->AsShared());
	}
/*
	void CreateTexture()
	{
		check(IsInRenderingThread());

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("DummyTexture2D"))
			.SetExtent(TotalSize)
			.SetFormat(PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::SRGB | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
			.SetInitialState(ERHIAccess::SRVMask);

		Texture = RHICreateTexture(Desc);
	}
*/

	const void* GetBuffer() override;

	FIntPoint GetDim() const override;

	FTimespan GetDuration() const override;

	double GetAspectRatio() const override
	{
		return VideoDecoderOutput->GetAspectRatio();
	}

	EMediaOrientation GetOrientation() const override
	{
		return (EMediaOrientation)VideoDecoderOutput->GetOrientation();
	}

	EMediaTextureSampleFormat GetFormat() const override;

	FIntPoint GetOutputDim() const override;
	uint32 GetStride() const override;

	FRHITexture* GetTexture() const override
	{
		return Texture;
	}

	FMediaTimeStamp GetTime() const override;

	bool IsCacheable() const override
	{
		return true;
	}

	bool IsOutputSrgb() const override
	{
		return true;
	}

    IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override
    {
        return nullptr;
    }

#if !UE_SERVER
	void InitializePoolable() override;
	void ShutdownPoolable() override;
#endif
/*
	TRefCountPtr<FRHITexture2D> GetTextureRef() const
	{
		return Texture;
	}
*/
private:
	/** The sample's texture resource. */
	TRefCountPtr<FRHITexture2D> Texture;

	/** Output data from video decoder. */
	TSharedPtr<FVideoDecoderOutputLinux, ESPMode::ThreadSafe> VideoDecoderOutput;
};

using FElectraTextureSamplePtr = TSharedPtr<FElectraTextureSampleLinux, ESPMode::ThreadSafe>;
using FElectraTextureSampleRef = TSharedRef<FElectraTextureSampleLinux, ESPMode::ThreadSafe>;

class FElectraTextureSamplePool : public TMediaObjectPool<FElectraTextureSampleLinux>
{
public:
	void PrepareForDecoderShutdown() {}
};
