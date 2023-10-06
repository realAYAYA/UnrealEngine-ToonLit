// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IElectraTextureSample.h"
#include "MediaObjectPool.h"
#include "RHI.h"
#include "RHIUtilities.h"

#include "Linux/MediaVideoDecoderOutputLinux.h"

class FRHITexture;

class FElectraTextureSampleLinux
	: public IElectraTextureSampleBase
{
public:
	FElectraTextureSampleLinux() = default;

	virtual	~FElectraTextureSampleLinux();

public:
	void Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
	{
		IElectraTextureSampleBase::Initialize(InVideoDecoderOutput);
		VideoDecoderOutputLinux = static_cast<FVideoDecoderOutputLinux*>(InVideoDecoderOutput);
	}

	const void* GetBuffer() override;
	uint32 GetStride() const override;

	EMediaTextureSampleFormat GetFormat() const override;

	FRHITexture* GetTexture() const override
	{
		return Texture;
	}

    IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override
    {
        return nullptr;
    }

#if !UE_SERVER
	void ShutdownPoolable() override;
#endif

private:
	/** The sample's texture resource. */
	TRefCountPtr<FRHITexture2D> Texture;

	/** Output data from video decoder. */
	FVideoDecoderOutputLinux* VideoDecoderOutputLinux;
};

using FElectraTextureSamplePtr = TSharedPtr<FElectraTextureSampleLinux, ESPMode::ThreadSafe>;
using FElectraTextureSampleRef = TSharedRef<FElectraTextureSampleLinux, ESPMode::ThreadSafe>;

class FElectraTextureSamplePool : public TMediaObjectPool<FElectraTextureSampleLinux>
{
public:
	void PrepareForDecoderShutdown() {}
};
