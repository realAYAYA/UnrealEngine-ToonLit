// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Containers/ArrayView.h"
#include "Templates/SubclassOf.h"
#include "Tickable.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "TakeRecorderSources.generated.h"

class UTakeRecorderSource;
class UMovieSceneSubSection;

DECLARE_LOG_CATEGORY_EXTERN(SubSequenceSerialization, Verbose, All);

struct FTakeRecorderSourcesSettings
{
	bool bStartAtCurrentTimecode;
	bool bRecordSourcesIntoSubSequences;
	bool bRecordToPossessable;
	bool bSaveRecordedAssets;
	bool bAutoLock;
	bool bRemoveRedundantTracks;
};

/**
 * A list of sources to record for any given take. Stored as meta-data on ULevelSequence through ULevelSequence::FindMetaData<UTakeRecorderSources>
 */
UCLASS(BlueprintType, Blueprintable)
class TAKESCORE_API UTakeRecorderSources : public UObject
{
public:
	GENERATED_BODY()

	UTakeRecorderSources(const FObjectInitializer& ObjInit);

	/**
	 * Add a new source to this source list of the templated type
	 *
	 * @return An instance of the templated type
	 */
	template<typename SourceType>
	SourceType* AddSource()
	{
		return static_cast<SourceType*>(AddSource(SourceType::StaticClass()));
	}


	/**
	 * Add a new source to this source list of the templated type
	 *
	 * @param InSourceType    The class type of the source to add
	 * @return An instance of the specified source type
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder", meta = (DeterminesOutputType = "InSourceType"))
	UTakeRecorderSource* AddSource(TSubclassOf<UTakeRecorderSource> InSourceType);

	/**
	 * Remove the specified source from this list
	 *
	 * @param InSource        The source to remove
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	void RemoveSource(UTakeRecorderSource* InSource);


	/**
	 * Access all the sources stored in this list
	 */
	TArrayView<UTakeRecorderSource* const> GetSources() const
	{
		return Sources;
	}

	/**
	* Retrieves a copy of the list of sources that are being recorded. This is intended for Blueprint usages which cannot
	* use TArrayView.
	* DO NOT MODIFY THIS ARRAY, modifications will be lost.
	*/
	UFUNCTION(BlueprintPure, DisplayName = "Get Sources (Copy)", Category = "Take Recorder")
	TArray<UTakeRecorderSource*> GetSourcesCopy() const
	{
		return TArray<UTakeRecorderSource*>(Sources);
	}
	/**
	 * Retrieve the serial number that is incremented when a source is added or removed from this list.
	 * @note: This field is not serialized, and not copied along with UObject duplication.
	 */
	uint32 GetSourcesSerialNumber() const
	{
		return SourcesSerialNumber;
	}

	/** Sources settings from the user and project parameters */
	FTakeRecorderSourcesSettings GetSettings() const { return Settings; }
	void SetSettings(FTakeRecorderSourcesSettings& InSettings) { Settings = InSettings; }

	/** Calls the recording initialization flows on each of the specified sources. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	void StartRecordingSource(TArray<UTakeRecorderSource*> InSources, const FQualifiedFrameTime& CurrentFrameTime);
	
	UE_DEPRECATED(4.27, "StartRecordingSource with FTimecode has been deprecated, please use StartRecordingSource with FQualifiedFrameTime")
	void StartRecordingSource(TArray<UTakeRecorderSource*> InSources, const FTimecode& CurrentTimecode);

public:

	/**
	 * Bind a callback for when this source list changes
	 *
	 * @param Handler         The delegate to call when the list changes
	 * @return A handle to this specific binding that should be passed to UnbindSourcesChanged
	 */
	FDelegateHandle BindSourcesChanged(const FSimpleDelegate& Handler);

	/**
	 * Unbind a previously bound handler for when this source list changes
	 *
	 * @param Handle          The handle returned from BindSourcesChanged for the handler to remove
	 */
	void UnbindSourcesChanged(FDelegateHandle Handle);

public:

	/*
	 * Cache assets needed for sequencer.
	 */
	void SetCachedAssets(class ULevelSequence* InSequence, FManifestSerializer* InManifestSerializer);

	/*
	 * Pre recording pass
	*
	*/
	void PreRecording(class ULevelSequence* InSequence, const FQualifiedFrameTime& InCurrentFrameTime, FManifestSerializer* InManifestSerializer);

	/*
	 * Start recording pass
	 *
	 */
	void StartRecording(class ULevelSequence* InSequence, const FQualifiedFrameTime& InCurrentFrameTime, FManifestSerializer* InManifestSerializer);

	/*
	 * Moves time forward by given DeltaTime
	 *
	 * @return Current Frame Number
	 */
	FFrameTime AdvanceTime(const FQualifiedFrameTime& CurrentFrameTime, float DeltaTime);

	/*
	* Tick recording pass
	* @return Current Frame Number we are recording at.
	*/
	FFrameTime TickRecording(class ULevelSequence* InSequence, const FQualifiedFrameTime& CurrentFrameTime, float DeltaTime);

	/*
	* Stop recording pass
	*
	*/
	void StopRecording(class ULevelSequence* InSequence, const bool bCancelled);

public:
	/*
	*  Static functions used by other parts of the take system
	*/

	/** Creates a sub-sequence asset for the specified sub sequence name based on the given master sequence. */
	static ULevelSequence* CreateSubSequenceForSource(ULevelSequence* InMasterSequence, const FString& SubSequenceTrackName, const FString& SubSequenceAssetName);

	/**
	 * Array of pairs - key time and the corresponding timecode
	 */
	static TArray<TPair<FQualifiedFrameTime, FQualifiedFrameTime> > RecordedTimes;

	FQualifiedFrameTime GetCachedFrameTime() const { return CachedFrameTime; }

private:
	/** Called at the end of each frame in both the Editor and in Game to update all Sources. */
	virtual void Tick(float DeltaTime) {}

	/** Called whenever a property is changed on this class */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** A list of handlers to invoke when the sources list changes */
	DECLARE_MULTICAST_DELEGATE(FOnSourcesChanged);
	FOnSourcesChanged OnSourcesChangedEvent;

	/** Calls PreRecording on sources recursively allowing them to create other sources which properly get PreRecording called on them as well. */
	void PreRecordingRecursive(TArray<UTakeRecorderSource*> InSources, ULevelSequence* InMasterSequence, TArray<UTakeRecorderSource*>& NewSourcesOut, FManifestSerializer* InManifestSerializer);

	/** Finds the folder that the given Source should be created in, creating it if necessary. */
	class UMovieSceneFolder* AddFolderForSource(const UTakeRecorderSource* InSource, class UMovieScene* InMovieScene);

	/** Remove object bindings that don't have any tracks and are not bindings for attach/path tracks */
	void RemoveRedundantTracks();

	void StartRecordingPreRecordedSources(const FQualifiedFrameTime& CurrentFrameTime);

	void PreRecordSources(TArray<UTakeRecorderSource *> InSources);

	void StartRecordingTheseSources(const TArray<UTakeRecorderSource *>& InSources, const FQualifiedFrameTime& CurrentFrameTime);

	void SetSectionStartTimecode(UMovieSceneSubSection* SubSection, const FTimecode& Timecode, const FQualifiedFrameTime& CurrentFrameTime);

private:

	/** The array of all sources contained within this list */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UTakeRecorderSource>> Sources;

	/** Maps each source to the level sequence that was created for that source, or to the master source if a subsequence was not created. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UTakeRecorderSource>, TObjectPtr<ULevelSequence>> SourceSubSequenceMap;

	/** List of sub-sections that we're recording into. Needed to ensure they're all the right size at the end without re-adjusting every sub-section in a sequence. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMovieSceneSubSection>> ActiveSubSections;

	/** Are we currently in a recording pass and should be ticking our Sources? */
	bool bIsRecording;

	/** What Tick Resolution is the target level sequence we're recording into? Used to convert seconds into FrameNumbers. */
	FFrameRate TargetLevelSequenceTickResolution;

	/** What Display Rate is the target level sequence we're recording into? Used to convert seconds into FrameNumbers. */
	FFrameRate TargetLevelSequenceDisplayRate;

	/** Non-serialized serial number that is used for updating UI when the source list changes */
	uint32 SourcesSerialNumber;

	/** Sources settings */
	FTakeRecorderSourcesSettings Settings;

	/** Manifest Serializer that we are recording into. */
	FManifestSerializer* CachedManifestSerializer;

	/** Level Sequence that we are recording into. Cached so that new sources added mid-recording get placed in the right sequence. */
	ULevelSequence* CachedLevelSequence;

	/** Array of Allocated Serializers created for each sub sequence.  Deleted at the end of the recording so memory is freed. */
	TArray<TSharedPtr<FManifestSerializer>> CreatedManifestSerializers;

	/** All sources after PreRecord */
	TArray<UTakeRecorderSource *> PreRecordedSources;

	/** The last frame time during tick recording */
	FQualifiedFrameTime CachedFrameTime;
};
