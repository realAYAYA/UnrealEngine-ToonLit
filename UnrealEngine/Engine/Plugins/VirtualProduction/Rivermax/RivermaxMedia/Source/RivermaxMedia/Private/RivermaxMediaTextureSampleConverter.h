// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTextureSampleConverter.h"

#include "RivermaxMediaSource.h"


class FRivermaxMediaTextureSample;

/**
 * 
 */
class FRivermaxMediaTextureSampleConverter : public IMediaTextureSampleConverter
{
public:

	/** Configures settings to convert incoming sample */
	void Setup(ERivermaxMediaSourcePixelFormat InPixelFormat, TWeakPtr<FRivermaxMediaTextureSample> InSample, bool bInDoSRGBToLinear);

	//~ Begin IMediaTextureSampleConverter interface
	virtual bool Convert(FTexture2DRHIRef& InDstTexture, const FConversionHints& Hints) override;
	virtual uint32 GetConverterInfoFlags() const override;
	//~ End IMediaTextureSampleConverter interface

private:

	/** Incoming pixel format used to know how to handle the input buffer */
	ERivermaxMediaSourcePixelFormat InputPixelFormat = ERivermaxMediaSourcePixelFormat::RGB_10bit;

	/** Sample holding the buffer to convert */
	TWeakPtr<FRivermaxMediaTextureSample> Sample;

	/** Whether shader should do a sRGB to linear conversion before writing out to the texture */
	bool bDoSRGBToLinear = false;
};
