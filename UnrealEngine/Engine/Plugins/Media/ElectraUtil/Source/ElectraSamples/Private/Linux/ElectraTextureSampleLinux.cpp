// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if !UE_SERVER

#include "ElectraTextureSample.h"

FElectraTextureSampleLinux::~FElectraTextureSampleLinux()
{
}

uint32 FElectraTextureSampleLinux::GetStride() const
{
	if (VideoDecoderOutput.IsValid())
	{
		return VideoDecoderOutputLinux->GetStride();
	}
	return 0;
}

const void* FElectraTextureSampleLinux::GetBuffer()
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutputLinux->GetBuffer().GetData();
	}
	return nullptr;
}

EMediaTextureSampleFormat FElectraTextureSampleLinux::GetFormat() const
{
	if (VideoDecoderOutputLinux)
	{
		return (VideoDecoderOutputLinux->GetFormat() == PF_NV12) ? EMediaTextureSampleFormat::CharNV12 : EMediaTextureSampleFormat::P010;
	}
	return EMediaTextureSampleFormat::Undefined;
}


/**
 *  Return the object to the pool and inform the renderer about this...
 */
void FElectraTextureSampleLinux::ShutdownPoolable()
{
	VideoDecoderOutputLinux = nullptr;
	Texture = nullptr;
	IElectraTextureSampleBase::ShutdownPoolable();
}

#endif
