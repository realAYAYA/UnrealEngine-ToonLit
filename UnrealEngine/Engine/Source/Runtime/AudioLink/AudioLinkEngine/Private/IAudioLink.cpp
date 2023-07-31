// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAudioLink.h"

#include "AudioAnalytics.h"

IAudioLink::IAudioLink()
{
	Audio::Analytics::RecordEvent_Usage(TEXT("AudioLink.InstanceCreated"));
}
