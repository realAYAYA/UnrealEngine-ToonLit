// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderFactory.h"
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
// H.264
#include "Decoders/VideoDecoderH264_Impl.h"

// VP8
#include "Decoders/VideoDecoderVP8.h"

// MPEG4
#include "Decoders/VideoDecoderMpeg4.h"

// Dummy test
#include "Decoders/VideoDecoderH264_Dummy.h"
#endif

namespace AVEncoder
{

FThreadSafeCounter			FVideoDecoderFactory::NextID = 1;
bool						FVideoDecoderFactory::bDebugDontRegisterDefaultCodecs = false;

FVideoDecoderFactory& FVideoDecoderFactory::Get()
{
	static FVideoDecoderFactory Singleton;
	return Singleton;
}

FVideoDecoderFactory::FVideoDecoderFactory()
{
	if (!bDebugDontRegisterDefaultCodecs)
	{
		RegisterDefaultCodecs();
	}
}

FVideoDecoderFactory::~FVideoDecoderFactory()
{
	AvailableDecoders.Empty();
	CreateDecoders.Empty();
}


void FVideoDecoderFactory::Debug_SetDontRegisterDefaultCodecs()
{
	bDebugDontRegisterDefaultCodecs = true;
}


void FVideoDecoderFactory::Register(const FVideoDecoderInfo& InInfo, const CreateDecoderCallback& InCreateEncoder)
{
	AvailableDecoders.Push(InInfo);
	AvailableDecoders.Last().ID = NextID.Increment();
	CreateDecoders.Push(InCreateEncoder);
}


void FVideoDecoderFactory::RegisterDefaultCodecs()
{
	// Add video decoders supported on this platform to the registry. Decoders that wish to add themselves
	// will call back into our FVideoDecoderFactory::Register() above with capability information and a
	// factory method through which a new instance can be created.
#if PLATFORM_WINDOWS
	FVideoDecoderH264_Impl::Register(*this);
	FVideoDecoderVP8::Register(*this);
	FVideoDecoderMPEG4::Register(*this);
#endif

	// For testing purposes add a dummy decoder last.
#if defined(AVENCODER_VIDEO_DECODER_AVAILABLE_H264_DUMMY)
	FVideoDecoderH264_Dummy::Register(*this);
#endif
}

bool FVideoDecoderFactory::GetInfo(uint32 InID, FVideoDecoderInfo& OutInfo) const
{
	for (int32 Index = 0; Index < AvailableDecoders.Num(); ++Index)
	{
		if (AvailableDecoders[Index].ID == InID)
		{
			OutInfo = AvailableDecoders[Index];
			return true;
		}
	}
	return false;
}


FVideoDecoder* FVideoDecoderFactory::Create(uint32 InID, const FVideoDecoder::FInit& InInit)
{
	FVideoDecoder* Result = nullptr;
	for (int32 Index = 0; Index < AvailableDecoders.Num(); ++Index)
	{
		if (AvailableDecoders[Index].ID == InID)
		{
			Result = CreateDecoders[Index]();
			if (Result && !Result->Setup(InInit))
			{
				Result->Shutdown();
				Result = nullptr;
			}
			break;
		}
	}
	return Result;
}

} /* namespace AVEncoder */
