// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackRecorders/MovieSceneTrackRecorder.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Misc/Guid.h"
#include "MovieScene.h"
#include "TrackRecorders/IMovieSceneTrackRecorderFactory.h"
#include "MovieScene/MovieSceneLiveLinkSection.h"

#include "MovieSceneLiveLinkTrackRecorder.generated.h"


class UMovieSceneLiveLinkTrack;
class UMovieSceneLiveLinkSectionBase;
class UMotionControllerComponent;
class ULiveLinkComponent;
class ULiveLinkSubjectProperties;
class UMovieSceneLiveLinkSection;
class UMovieSceneLiveLinkTrack;


UCLASS(BlueprintType)
class LIVELINKSEQUENCER_API UMovieSceneLiveLinkTrackRecorder : public UMovieSceneTrackRecorder
{
	GENERATED_BODY()
public:
	virtual ~UMovieSceneLiveLinkTrackRecorder() = default;

	// UMovieSceneTrackRecorder Interface
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentTime) override;
	virtual void FinalizeTrackImpl() override;
	virtual void SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) override;
	virtual UMovieSceneSection* GetMovieSceneSection() const override { return Cast<UMovieSceneSection>(MovieSceneSection.Get()); }
	virtual void StopRecordingImpl() override;
	virtual void SetSavedRecordingDirectory(const FString& InDirectory)
	{
		Directory = InDirectory;
	}
	virtual bool LoadRecordedFile(const FString& InFileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap, TFunction<void()> InCompletionCallback) override;


public:
	//we don't call UMovieSceneTrackRecorder::CreateTrack or CreateTrackImpl since that expects an  ObjectToRecord and a GUID which isn't needed.
	void CreateTrack(UMovieScene* InMovieScene, const FName& InSubjectName, bool bInSaveSubjectSettings, bool bInAlwaysUseTimecode, bool bDiscardSamplesBeforeStart, UMovieSceneTrackRecorderSettings* InSettingsObject);
	void AddContentsToFolder(UMovieSceneFolder* InFolder);
	void SetReduceKeys(bool bInReduce) { bReduceKeys = bInReduce; }

private:

	UMovieSceneLiveLinkTrack* DoesLiveLinkMasterTrackExist(const FName& MasterTrackName, const TSubclassOf<ULiveLinkRole>& InTrackRole);

	void CreateTracks();

	void OnStaticDataReceived(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InSubjectRole, const FLiveLinkStaticDataStruct& InStaticData);
	void OnFrameDataReceived(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InSubjectRole, const FLiveLinkFrameDataStruct& InFrameData);
private:

	/** Name of Subject To Record */
	FName SubjectName;

	/** Whether we should save subject preset in the the live link section. If not, we'll create one with subject information with no settings */
	bool bSaveSubjectSettings;

	/** Whether or not we use timecode time or world time*/
	bool bUseSourceTimecode;

	/** Whether to discard livelink samples with timecode that occurs before the start of recording*/
	bool bDiscardSamplesBeforeStart;

	/** Role of the subject we will record*/
	TSubclassOf<ULiveLinkRole> SubjectRole;

	/** Cached LiveLink Tracks, section per each maps to SubjectNames */
	TWeakObjectPtr<UMovieSceneLiveLinkTrack> LiveLinkTrack;

	/** Sections to record to on each track*/
	TWeakObjectPtr<UMovieSceneLiveLinkSection> MovieSceneSection;
	
	/** Diff between Engine Time from when starting to record and Platform
	Time which is used by Live Link. Still used if no TimeCode present.*/
	double SecondsDiff; 

	/** The frame at the start of this recording section */
	FFrameNumber RecordStartFrame;

	/** Guid when registered to get LiveLinkData */
	FGuid HandlerGuid;

	/**Cached directory for serializers to save to*/
	FString Directory;

	/** Cached Key Reduction from Live Link Source Properties*/
	bool bReduceKeys;

	/** Whether the Subject is Virtual or not*/
	bool bIsVirtualSubject = false;

	/** Delegates registered during recording to receive live link data as it comes in*/
	FDelegateHandle OnStaticDataReceivedHandle;
	FDelegateHandle OnFrameDataReceivedHandle;

	TArray<FLiveLinkFrameDataStruct> FramesToProcess;
};
