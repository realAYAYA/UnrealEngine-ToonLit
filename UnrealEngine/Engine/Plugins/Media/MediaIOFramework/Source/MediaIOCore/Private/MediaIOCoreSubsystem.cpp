// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaIOCoreSubsystem)

TSharedPtr<FMediaIOAudioOutput> UMediaIOCoreSubsystem::CreateAudioOutput(const FCreateAudioOutputArgs& Args)
{
	if (!MediaIOCapture)
	{
		MediaIOCapture = MakeUnique<FMediaIOAudioCapture>();
	}

	return MediaIOCapture->CreateAudioOutput(Args.NumOutputChannels, Args.TargetFrameRate, Args.MaxSampleLatency, Args.OutputSampleRate);
}

