// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Recording/LiveLinkRecorder.h"

#include "LiveLinkTypes.h"
#include "Misc/CoreMiscDefines.h"
#include "Templates/PimplPtr.h"

struct FLiveLinkRecordingBaseDataContainer;
struct FLiveLinkUAssetRecordingData;
struct FInstancedStruct;

/** UAsset implementation for serializing recorded livelink data. */
class FLiveLinkUAssetRecorder : public ILiveLinkRecorder
{
public:

	//~ Begin ILiveLinkRecorder
	virtual void StartRecording() override;
	virtual void StopRecording() override;
	virtual void RecordStaticData(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, const FLiveLinkStaticDataStruct& StaticData) override;
	virtual void RecordFrameData(const FLiveLinkSubjectKey& SubjectKey, const FLiveLinkFrameDataStruct& FrameData) override;
	virtual bool IsRecording() const override;
	//~ End ILiveLinkRecorder

private:
	/** Prompt the user for a destination path for the recording. */
	bool OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName);
	/** Creates a unique asset name and prompts the user for the recording name. */
	bool GetSavePresetPackageName(FString& OutName);
	/** Create a recording package and save it. */
	void SaveRecording();
	/** Record data to a ULiveLinkRecording object. */
	void RecordBaseData(FLiveLinkRecordingBaseDataContainer& StaticDataContainer, FInstancedStruct&& DataToRecord);
	/** Record initial data for all livelink subjects. (Useful when static data was sent before the recording started). */
	void RecordInitialStaticData();

private:
	/** Holds metadata and recording data. */
	TPimplPtr<FLiveLinkUAssetRecordingData> CurrentRecording;
	/** Whether we're currently recording livelink data. */
	bool bIsRecording = false;
	/** Timestamp in seconds of when the recording was started. */
	double TimeRecordingStarted = 0.0;
	/** Timestamp in seconds of when the recording ended. */
	double TimeRecordingEnded = 0.0;
};
