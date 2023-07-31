// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/QualifiedFrameTime.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneBindingProxy.h"
#include "LevelSequenceEditorBlueprintLibrary.generated.h"

class ISequencer;
class ULevelSequence;
class UMovieSceneFolder;
class UMovieSceneSection;
class UMovieSceneSubSection;
class UMovieSceneTrack;

USTRUCT(BlueprintType)
struct FSequencerChannelProxy
{
	GENERATED_BODY()

	FSequencerChannelProxy()
		: Section(nullptr)
	{}

	FSequencerChannelProxy(const FName& InChannelName, UMovieSceneSection* InSection)
		: ChannelName(InChannelName)
		, Section(InSection)
	{}

	UPROPERTY(BlueprintReadWrite, Category=Channel)
	FName ChannelName;

	UPROPERTY(BlueprintReadWrite, Category=Channel)
	TObjectPtr<UMovieSceneSection> Section;
};


UCLASS()
class LEVELSEQUENCEEDITOR_API ULevelSequenceEditorBlueprintLibrary : public UBlueprintFunctionLibrary
{
public:

	GENERATED_BODY()

	/*
	 * Open a level sequence asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static bool OpenLevelSequence(ULevelSequence* LevelSequence);

	/*
	 * Get the currently opened root/master level sequence asset
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static ULevelSequence* GetCurrentLevelSequence();

	/*
	 * Get the currently focused/viewed level sequence asset if there is a hierarchy of sequences.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static ULevelSequence* GetFocusedLevelSequence();

	/*
	 * Focus/view the sequence associated to the given sub sequence section.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void FocusLevelSequence(UMovieSceneSubSection* SubSection);

	/*
	 * Focus/view the parent sequence, popping out of the current sub sequence section.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void FocusParentSequence();

	/*
	 * Get the current sub section hierarchy from the current sequence to the section associated with the focused sequence.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static TArray<UMovieSceneSubSection*> GetSubSequenceHierarchy();
	
	/*
	 * Close
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void CloseLevelSequence();

	/**
	 * Play the current level sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void Play();

	/**
	 * Pause the current level sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void Pause();

public:

	/**
	 * Set global playback position for the current level sequence in frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetCurrentTime(int32 NewFrame);

	/**
	 * Get the current global playback position in frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static int32 GetCurrentTime();

	/**
	 * Set local playback position for the current level sequence in frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetCurrentLocalTime(int32 NewFrame);

	/**
	 * Get the current local playback position in frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static int32 GetCurrentLocalTime();

	/**
	 * Play from the current time to the requested time in frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void PlayTo(FMovieSceneSequencePlaybackParams PlaybackParams);

public:

	/** Check whether the sequence is actively playing. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static bool IsPlaying();

public:

	/** Gets the currently selected tracks. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static TArray<UMovieSceneTrack*> GetSelectedTracks();

	/** Gets the currently selected sections. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static TArray<UMovieSceneSection*> GetSelectedSections();

	/** Gets the currently selected channels. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static TArray<FSequencerChannelProxy> GetSelectedChannels();

	/** Gets the currently selected folders. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static TArray<UMovieSceneFolder*> GetSelectedFolders();

	/** Gets the currently selected Object Guids*/
	UE_DEPRECATED(5.1, "GetSelectedObjects is deprecated, please use GetSelectedBindings which returns an array of FMovieSceneBindingProxy")
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor", meta = (DeprecatedFunction, DeprecationMessage="GetSelectedObjects is deprecated, please use GetSelectedBindings which returns an array of FMovieSceneBindingProxy"))
	static TArray<FGuid> GetSelectedObjects();

	/** Gets the currently selected object bindings */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static TArray<FMovieSceneBindingProxy> GetSelectedBindings();

	/** Select tracks */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SelectTracks(const TArray<UMovieSceneTrack*>& Tracks);

	/** Select sections */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SelectSections(const TArray<UMovieSceneSection*>& Sections);

	/** Select channels */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SelectChannels(const TArray<FSequencerChannelProxy>& Channels);

	/** Select folders */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SelectFolders(const TArray<UMovieSceneFolder*>& Folders);

	/** Select objects by GUID */
	UE_DEPRECATED(5.1, "SelectObjects is deprecated, please use SelectBindings which takes an FMovieSceneBindingProxy")
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor", meta=(DeprecatedFunction, DeprecationMessage="SelectObjects is deprecated, please use SelectBindings which takes an FMovieSceneBindingProxy"))
	static void SelectObjects(TArray<FGuid> ObjectBinding);

	/** Select bindings */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SelectBindings(const TArray<FMovieSceneBindingProxy>& ObjectBindings);

	/** Empties the current selection. */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void EmptySelection();

	/** Set the selection range start frame. */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetSelectionRangeStart(int32 NewFrame);

	/** Set the selection range end frame. */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetSelectionRangeEnd(int32 NewFrame);

	/** Get the selection range start frame. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static int32 GetSelectionRangeStart();

	/** Get the selection range end frame. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static int32 GetSelectionRangeEnd();

public:

	/** Refresh Sequencer UI. */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void RefreshCurrentLevelSequence();

	/** Get the object bound to the given binding ID with the current level sequence editor */
	UFUNCTION(BlueprintPure, Category="Level Sequence Editor")
	static TArray<UObject*> GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding);

	/** Check whether the current level sequence and its descendants are locked for editing. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static bool IsLevelSequenceLocked();

	/** Sets the lock for the current level sequence and its descendants for editing. */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetLockLevelSequence(bool bLock);

public:

	/** Check whether the lock for the viewport to the camera cuts is enabled. */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static bool IsCameraCutLockedToViewport();

	/** Sets the lock for the viewport to the camera cuts. */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetLockCameraCutToViewport(bool bLock);

public:

	/** Gets whether the specified track filter is on/off */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static bool IsTrackFilterEnabled(const FText& TrackFilterName);

	/** Sets the specified track filter to be on or off */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetTrackFilterEnabled(const FText& TrackFilterName, bool bEnabled);

	/** Gets all the available track filter names */
	UFUNCTION(BlueprintPure, Category = "Level Sequence Editor")
	static TArray<FText> GetTrackFilterNames();

public:

	/** Get if a custom color for specified channel idendified by it's class and identifier exists */
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static bool HasCustomColorForChannel(UClass* Class, const FString& Identifier);
	
	/** Get custom color for specified channel idendified by it's class and identifier,if none exists will return white*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static FLinearColor GetCustomColorForChannel(UClass* Class, const FString& Identifier);
	
	/** Set Custom Color for specified channel idendified by it's class and identifier. This will be stored in editor user preferences.*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetCustomColorForChannel(UClass* Class, const FString& Identifier, const FLinearColor& NewColor);
	
	/** Set Custom Color for specified channels idendified by it's class and identifiers. This will be stored in editor user preferences.*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetCustomColorForChannels(UClass* Class, const TArray<FString>& Identifiers, const TArray<FLinearColor>& NewColors);
	
	/** Set Random Colors for specified channels idendified by it's class and identifiers. This will be stored in editor user preferences.*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void SetRandomColorForChannels(UClass* Class, const TArray<FString>& Identifiers);
	
	/** Delete for specified channel idendified by it's class and identifier.*/
	UFUNCTION(BlueprintCallable, Category = "Level Sequence Editor")
	static void DeleteColorForChannels(UClass* Class, FString& Identifier);

public:

	/*
	 * Callbacks
	 */

public:

	/**
	 * Internal function to assign a sequencer singleton.
	 * NOTE: Only to be called by LevelSequenceEditor::Construct.
	 */
	static void SetSequencer(TSharedRef<ISequencer> InSequencer);
};