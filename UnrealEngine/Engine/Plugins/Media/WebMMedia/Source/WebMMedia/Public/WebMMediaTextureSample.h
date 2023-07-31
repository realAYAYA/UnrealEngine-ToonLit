// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"
#include "MediaObjectPool.h"
#include "RHI.h"
#include "RHIUtilities.h"

class FRHITexture;

class WEBMMEDIA_API FWebMMediaTextureSample
	: public IMediaTextureSample
	, public IMediaPoolable
{
public:
	FWebMMediaTextureSample();

public:
	void Initialize(FIntPoint InDisplaySize, FIntPoint InTotalSize, FTimespan InTime, FTimespan InDuration);
	void CreateTexture();

public:
	//~ IMediaTextureSample interface
	virtual const void* GetBuffer() override;
	virtual FIntPoint GetDim() const override;
	virtual FTimespan GetDuration() const override;
	virtual EMediaTextureSampleFormat GetFormat() const override;
	virtual FIntPoint GetOutputDim() const override;
	virtual uint32 GetStride() const override;
	virtual FRHITexture* GetTexture() const override;
	virtual FMediaTimeStamp GetTime() const override;
	virtual bool IsCacheable() const override;
	virtual bool IsOutputSrgb() const override;

public:
	//~ IMediaPoolable interface
	virtual void ShutdownPoolable() override;

public:
	TRefCountPtr<FRHITexture2D> GetTextureRef() const;

private:
	TRefCountPtr<FRHITexture2D> Texture;
	FTimespan Time;
	FTimespan Duration;
	FIntPoint TotalSize;
	FIntPoint DisplaySize;
};

class FWebMMediaTextureSamplePool : public TMediaObjectPool<FWebMMediaTextureSample> { };
