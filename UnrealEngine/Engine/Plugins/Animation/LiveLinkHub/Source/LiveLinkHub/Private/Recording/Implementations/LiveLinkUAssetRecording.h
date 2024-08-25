// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStruct.h"
#include "LiveLinkPreset.h"
#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "Misc/DateTime.h"
#include "Recording/LiveLinkRecording.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "LiveLinkUAssetRecording.generated.h"

/** Base data container for a recording track. */
USTRUCT()
struct FLiveLinkRecordingBaseDataContainer
{
	GENERATED_BODY()

	/** Timestamps for the recorded data. Each entry matches an entry in the RecordedData array. */
	UPROPERTY()
	TArray<double> Timestamps;

	/** Array of either static or frame recorded for a given timestamp.*/
	UPROPERTY()
	TArray<FInstancedStruct> RecordedData;
};

/** Container for static data. */
USTRUCT()
struct FLiveLinkRecordingStaticDataContainer : public FLiveLinkRecordingBaseDataContainer
{
	GENERATED_BODY()

	/** The role of the static data being recorded. */
	UPROPERTY()
	TSubclassOf<ULiveLinkRole> Role = nullptr;
};

USTRUCT()
struct FLiveLinkUAssetRecordingData
{
	GENERATED_BODY()

	/** Length of the recording in seconds. */
	UPROPERTY()
	double LengthInSeconds = 0;

	/** Static data encountered while recording. */
	UPROPERTY()
	TMap<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer> StaticData;
	
	/** Frame data encountered while recording. */
	UPROPERTY()
	TMap<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer> FrameData;
};

UCLASS()
class ULiveLinkUAssetRecording : public ULiveLinkRecording
{
public:
	GENERATED_BODY()

	/** Recorded Static and frame data. */
	UPROPERTY()
	FLiveLinkUAssetRecordingData RecordingData;
};
