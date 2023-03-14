// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreTextureSampleBase.h"

#include "MediaShaders.h"
#include "RivermaxMediaSource.h"
#include "Templates/SharedPointer.h"

class FRivermaxMediaTextureSampleConverter;

/**
 * Implements a media texture sample for RivermaxMedia.
 */
class FRivermaxMediaTextureSample : public FMediaIOCoreTextureSampleBase, public TSharedFromThis<FRivermaxMediaTextureSample>
{
	using Super = FMediaIOCoreTextureSampleBase;

public:

	FRivermaxMediaTextureSample();


	//~ Begin IMediaTextureSample interface
	virtual const FMatrix& GetYUVToRGBMatrix() const override;
	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override;
	virtual bool IsOutputSrgb() const override;
	//~ End IMediaTextureSample interface

	bool ConfigureSample(uint32 InWidth, uint32 InHeight, uint32 InStride, ERivermaxMediaSourcePixelFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode, bool bInIsSRGBInput);

private:

	/** Texture converted used to handle incoming 2110 formats and convert them to RGB textures the engine handles */
	TUniquePtr<FRivermaxMediaTextureSampleConverter> TextureConverter;
};

/*
 * Implements a pool for Rivermax texture sample objects.
 */
class FRivermaxMediaTextureSamplePool : public TMediaObjectPool<FRivermaxMediaTextureSample> { };
