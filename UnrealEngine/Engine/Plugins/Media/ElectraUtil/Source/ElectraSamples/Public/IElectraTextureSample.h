// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMediaTextureSample.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaObjectPool.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "Templates/RefCounting.h"

#include "MediaVideoDecoderOutput.h"

class ELECTRASAMPLES_API IElectraTextureSampleBase
	: public IMediaTextureSample
	, public IMediaPoolable
{
public:
	void Initialize(FVideoDecoderOutput* InVideoDecoderOutput);

	virtual bool IsCacheable() const override
	{
		return true;
	}

#if !UE_SERVER
	virtual void InitializePoolable() override;
	virtual void ShutdownPoolable() override;
#endif

	virtual FIntPoint GetDim() const override;
	virtual FIntPoint GetOutputDim() const override;

	virtual FMediaTimeStamp GetTime() const override;
	virtual FTimespan GetDuration() const override;

	virtual double GetAspectRatio() const override
	{
		return VideoDecoderOutput->GetAspectRatio();
	}

	virtual EMediaOrientation GetOrientation() const override
	{
		return (EMediaOrientation)VideoDecoderOutput->GetOrientation();
	}

	virtual bool IsOutputSrgb() const override;
	virtual const FMatrix& GetYUVToRGBMatrix() const override;
	virtual bool GetFullRange() const override;

	virtual FMatrix44f GetSampleToRGBMatrix() const override;
	virtual FMatrix44d GetGamutToXYZMatrix() const override;
	virtual FVector2d GetWhitePoint() const override;
	virtual FVector2d GetDisplayPrimaryRed() const override;
	virtual FVector2d GetDisplayPrimaryGreen() const override;
	virtual FVector2d GetDisplayPrimaryBlue() const override;
	virtual UE::Color::EEncoding GetEncodingType() const override;
	virtual bool GetDisplayMasteringLuminance(float& OutMin, float& OutMax) const override;
	virtual bool GetMaxLuminanceLevels(uint16& OutCLL, uint16& OutFALL) const override;

protected:
	virtual float GetSampleDataScale(bool b10Bit) const { return 1.0f; }

	/** Output data from video decoder. */
	TSharedPtr<FVideoDecoderOutput, ESPMode::ThreadSafe> VideoDecoderOutput;

	/** Quick access for some HDR related info */
	TWeakPtr<const IVideoDecoderHDRInformation, ESPMode::ThreadSafe> HDRInfo;
	TWeakPtr<const IVideoDecoderColorimetry, ESPMode::ThreadSafe> Colorimetry;

	/** YUV matrix, adjusted to compensate for decoder output specific scale */
	FMatrix44f SampleToRgbMtx;

	/** YUV to RGB matrix without any adjustments for decoder output specifics */
	const FMatrix* YuvToRgbMtx;

	/** Precomputed colorimetric data */
	UE::Color::EEncoding ColorEncoding;
	UE::Color::FColorSpace SampleColorSpace;
	UE::Color::FColorSpace DisplayColorSpace;
	bool bDisplayColorSpaceValid;
	float DisplayMasteringLuminanceMin;
	float DisplayMasteringLuminanceMax;
	uint16 MaxCLL;
	uint16 MaxFALL;
};
