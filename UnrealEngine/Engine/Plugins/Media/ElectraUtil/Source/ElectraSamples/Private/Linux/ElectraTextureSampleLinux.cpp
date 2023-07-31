// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if !UE_SERVER

#include "ElectraTextureSample.h"

FElectraTextureSampleLinux::~FElectraTextureSampleLinux()
{
}


FIntPoint FElectraTextureSampleLinux::GetDim() const
{
	if (VideoDecoderOutput.IsValid())
	{
		return VideoDecoderOutput->GetDim();
	}
	return FIntPoint::ZeroValue;
}

FMediaTimeStamp FElectraTextureSampleLinux::GetTime() const
{
	if (VideoDecoderOutput.IsValid())
	{
		const FDecoderTimeStamp TimeStamp = VideoDecoderOutput->GetTime();
		return FMediaTimeStamp(TimeStamp.Time, TimeStamp.SequenceIndex);
	}
	return FMediaTimeStamp();
}

FTimespan FElectraTextureSampleLinux::GetDuration() const
{
	if (VideoDecoderOutput.IsValid())
	{
		return VideoDecoderOutput->GetDuration();
	}
	return FTimespan::Zero();
}

FIntPoint FElectraTextureSampleLinux::GetOutputDim() const
{
	if (VideoDecoderOutput.IsValid())
	{
		return VideoDecoderOutput->GetOutputDim();
	}
	return FIntPoint::ZeroValue;
}

uint32 FElectraTextureSampleLinux::GetStride() const
{
	if (VideoDecoderOutput.IsValid())
	{
		return VideoDecoderOutput->GetStride();
	}
	return 0;
}

const void* FElectraTextureSampleLinux::GetBuffer()
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutput->GetBuffer().GetData();
	}
	return nullptr;
}

EMediaTextureSampleFormat FElectraTextureSampleLinux::GetFormat() const
{
	return EMediaTextureSampleFormat::CharNV12;
}


/**
 * Init code for realloacting an image from the the pool
 */
void FElectraTextureSampleLinux::InitializePoolable()
{
}

/**
 *  Return the object to the pool and inform the renderer about this...
 */
void FElectraTextureSampleLinux::ShutdownPoolable()
{
	VideoDecoderOutput.Reset();
	Texture = nullptr;
}

#endif
