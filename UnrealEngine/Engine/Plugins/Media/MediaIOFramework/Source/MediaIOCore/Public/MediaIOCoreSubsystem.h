// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreAudioOutput.h"
#include "Subsystems/EngineSubsystem.h"

#include "MediaIOCoreSubsystem.generated.h"

UCLASS()
class MEDIAIOCORE_API UMediaIOCoreSubsystem : public UEngineSubsystem
{
public:
	struct FCreateAudioOutputArgs
	{
		uint32 NumOutputChannels = 0;
		FFrameRate TargetFrameRate; 
		uint32 MaxSampleLatency = 0;
		uint32 OutputSampleRate = 0;
	};

public:
	GENERATED_BODY()

	/**
	 * Create an audio output that allows getting audio that was accumulated during the last frame. 
	 */
	TSharedPtr<FMediaIOAudioOutput> CreateAudioOutput(const FCreateAudioOutputArgs& Args);

private:
	TUniquePtr<FMediaIOAudioCapture> MediaIOCapture;
};
