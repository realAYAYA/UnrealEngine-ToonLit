// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkPreset.h"
#include "UObject/Object.h"

#include "LiveLinkRecording.generated.h"

UCLASS(Abstract)
class ULiveLinkRecording : public UObject
{
public:
	GENERATED_BODY()

	ULiveLinkRecording()
	{
		RecordingPreset = CreateDefaultSubobject<ULiveLinkPreset>(TEXT("RecordingPreset"));
	}

	/** LiveLink Preset used to save the initial state of the sources and subjects at the time of recording. */
	UPROPERTY(Instanced)
	TObjectPtr<ULiveLinkPreset> RecordingPreset = nullptr;

	/** Length of the recording. */
	UPROPERTY()
	double LengthInSeconds = 0.0;
};