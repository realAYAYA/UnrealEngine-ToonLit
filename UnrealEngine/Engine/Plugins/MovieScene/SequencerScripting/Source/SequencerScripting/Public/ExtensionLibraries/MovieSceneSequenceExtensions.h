// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Containers/ArrayView.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovieSceneBindingProxy.h"
#include "SequencerScriptingRange.h"
#include "Templates/SubclassOf.h"
#include "MovieSceneTrack.h"
#include "MovieSceneObjectBindingID.h" // for EMovieSceneObjectBindingSpace
#include "MovieScene.h" // only for FMovieSceneMarkedFrame in the .gen.cpp

#include "MovieSceneSequenceExtensions.generated.h"

/**
 * Function library containing methods that should be hoisted onto UMovieSceneSequences for scripting purposes
 */
UCLASS()
class UMovieSceneSequenceExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Get this sequence's movie scene data
	 *
	 * @param Sequence        The sequence to use
	 * @return This sequence's movie scene data object
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static UMovieScene* GetMovieScene(UMovieSceneSequence* Sequence);

	/**
	 * Get all master tracks
	 *
	 * @param Sequence        The sequence to use
	 * @return An array containing all master tracks in this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static TArray<UMovieSceneTrack*> GetMasterTracks(UMovieSceneSequence* Sequence);

	/**
	 * Find all master tracks of the specified type
	 *
	 * @param Sequence        The sequence to use
	 * @param TrackType     A UMovieSceneTrack class type specifying which types of track to return
	 * @return An array containing any tracks that match the type specified
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod, DeterminesOutputType="TrackType"))
	static TArray<UMovieSceneTrack*> FindMasterTracksByType(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Find all master tracks of the specified type, not allowing sub-classed types
	 *
	 * @param Sequence        The sequence to use
	 * @param TrackType     A UMovieSceneTrack class type specifying the exact types of track to return
	 * @return An array containing any tracks that are exactly the same as the type specified
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod, DeterminesOutputType="TrackType"))
	static TArray<UMovieSceneTrack*> FindMasterTracksByExactType(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Add a new master track of the specified type
	 *
	 * @param Sequence        The sequence to use
	 * @param TrackType     A UMovieSceneTrack class type to create
	 * @return The newly created track, if successful
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod, DeterminesOutputType="TrackType"))
	static UMovieSceneTrack* AddMasterTrack(UMovieSceneSequence* Sequence, TSubclassOf<UMovieSceneTrack> TrackType);

	/**
	 * Removes a master track
	 *
	 * @param Sequence        The sequence to use
	 * @param MasterTrack     The master track to remove
	 * @return Whether the master track was successfully removed
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static bool RemoveMasterTrack(UMovieSceneSequence* Sequence, UMovieSceneTrack* MasterTrack);

	/**
	 * Gets this sequence's display rate
	 *
	 * @param Sequence        The sequence to use
	 * @return The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FFrameRate GetDisplayRate(UMovieSceneSequence* Sequence);

	/**
	 * Sets this sequence's display rate
	 *
	 * @param Sequence        The sequence to use
	 * @param DisplayRate The display rate that this sequence is displayed as
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static void SetDisplayRate(UMovieSceneSequence* Sequence, FFrameRate DisplayRate);

	/**
	 * Gets this sequence's tick resolution
	 *
	 * @param Sequence        The sequence to use
	 * @return The tick resolution of the sequence, defining the smallest unit of time representable on this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FFrameRate GetTickResolution(UMovieSceneSequence* Sequence);

	/**
	 * Sets this sequence's tick resolution and migrates frame times
	 *
	 * @param Sequence        The sequence to use
	 * @param TickResolution The tick resolution of the sequence, defining the smallest unit of time representable on this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static void SetTickResolution(UMovieSceneSequence* Sequence, FFrameRate TickResolution);

	/**
	 * Sets this sequence's tick resolution directly without migrating frame times
	 *
	 * @param Sequence        The sequence to use
	 * @param TickResolution The tick resolution of the sequence, defining the smallest unit of time representable on this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static void SetTickResolutionDirectly(UMovieSceneSequence* Sequence, FFrameRate TickResolution);

	/**
	 * Make a new range for this sequence in its display rate
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param StartFrame      The frame at which to start the range
	 * @param Duration        The length of the range
	 * @return Specified sequencer range
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FSequencerScriptingRange MakeRange(UMovieSceneSequence* Sequence, int32 StartFrame, int32 Duration);

	/**
	 * Make a new range for this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param StartTime       The time in seconds at which to start the range
	 * @param Duration        The length of the range in seconds
	 * @return Specified sequencer range
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FSequencerScriptingRange MakeRangeSeconds(UMovieSceneSequence* Sequence, float StartTime, float Duration);

	/**
	 * Get playback range of this sequence in display rate resolution
	 *
	 * @param Sequence        The sequence within which to get the playback range
	 * @return Playback range of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static FSequencerScriptingRange GetPlaybackRange(UMovieSceneSequence* Sequence);

	/**
	 * Get playback start of this sequence in display rate resolution
	 *
	 * @param Sequence        The sequence within which to get the playback start
	 * @return Playback start of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static int32 GetPlaybackStart(UMovieSceneSequence* Sequence);

	/**
	 * Get playback start of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to get the playback start
	 * @return Playback start of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static float GetPlaybackStartSeconds(UMovieSceneSequence* Sequence);

	/**
	 * Get playback end of this sequence in display rate resolution
	 *
	 * @param Sequence        The sequence within which to get the playback end
	 * @return Playback end of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static int32 GetPlaybackEnd(UMovieSceneSequence* Sequence);

	/**
	 * Get playback end of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to get the playback end
	 * @return Playback end of this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static float GetPlaybackEndSeconds(UMovieSceneSequence* Sequence);

	/**
	 * Set playback start of this sequence
	 *
	 * @param Sequence        The sequence within which to set the playback start
	 * @param StartFrame      The desired start frame for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetPlaybackStart(UMovieSceneSequence* Sequence, int32 StartFrame);

	/**
	 * Set playback start of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to set the playback start
	 * @param StartTime       The desired start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetPlaybackStartSeconds(UMovieSceneSequence* Sequence, float StartTime);

	/**
	 * Set playback end of this sequence
	 *
	 * @param Sequence        The sequence within which to set the playback end
	 * @param EndFrame        The desired end frame for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetPlaybackEnd(UMovieSceneSequence* Sequence, int32 EndFrame);

	/**
	 * Set playback end of this sequence in seconds
	 *
	 * @param Sequence        The sequence within which to set the playback end
	 * @param EndTime         The desired end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetPlaybackEndSeconds(UMovieSceneSequence* Sequence, float EndTime);

	/**
	 * Set the sequence view range start in seconds
	 *
	 * @param Sequence The sequence within which to set the view range start
	 * @param StartTimeInSeconds The desired view range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetViewRangeStart(UMovieSceneSequence* InSequence, float StartTimeInSeconds);

	/**
	 * Get the sequence view range start in seconds
	 *
	 * @param Sequence The sequence within which to get the view range start
	 * @return The view range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetViewRangeStart(UMovieSceneSequence* InSequence);

	/**
	 * Set the sequence view range end in seconds
	 *
	 * @param Sequence The sequence within which to set the view range end
	 * @param StartTimeInSeconds The desired view range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetViewRangeEnd(UMovieSceneSequence* InSequence, float EndTimeInSeconds);

	/**
	 * Get the sequence view range end in seconds
	 *
	 * @param Sequence The sequence within which to get the view range end
	 * @return The view range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetViewRangeEnd(UMovieSceneSequence* InSequence);

	/**
	 * Set the sequence work range start in seconds
	 *
	 * @param Sequence The sequence within which to set the work range start
	 * @param StartTimeInSeconds The desired work range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetWorkRangeStart(UMovieSceneSequence* InSequence, float StartTimeInSeconds);

	/**
	 * Get the sequence work range start in seconds
	 *
	 * @param Sequence The sequence within which to get the work range start
	 * @return The work range start time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetWorkRangeStart(UMovieSceneSequence* InSequence);

	/**
	 * Set the sequence work range end in seconds
	 *
	 * @param Sequence The sequence within which to set the work range end
	 * @param StartTimeInSeconds The desired work range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static void SetWorkRangeEnd(UMovieSceneSequence* InSequence, float EndTimeInSeconds);

	/**
	 * Get the sequence work range end in seconds
	 *
	 * @param Sequence The sequence within which to get the work range end
	 * @return The work range end time in seconds for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Sequence", meta = (ScriptMethod, DevelopmentOnly))
	static float GetWorkRangeEnd(UMovieSceneSequence* InSequence);

	/**
	 * Set the evaluation type for this sequence
	 *
	 * @param Sequence The sequence within which to set the evaluation type
	 * @param InEvaluationType The evaluation type to set for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetEvaluationType(UMovieSceneSequence* InSequence, EMovieSceneEvaluationType InEvaluationType);

	/**
	 * Get the evaluation type for this sequence
	 *
	 * @param Sequence The sequence within which to get the evaluation type
	 * @return The evaluation type for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static EMovieSceneEvaluationType GetEvaluationType(UMovieSceneSequence* InSequence);

	/**
	 * Set the clock source for this sequence
	 *
	 * @param Sequence The sequence within which to set the clock source
	 * @param InClockSource The clock source to set for this sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetClockSource(UMovieSceneSequence* InSequence, EUpdateClockSource InClockSource);

	/**
	 * Get the clock source for this sequence
	 *
	 * @param Sequence The sequence within which to get the clock source
	 * @return The clock source for this sequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static EUpdateClockSource GetClockSource(UMovieSceneSequence* InSequence);

	/**
	 * Get the timecode source of this sequence
	 *
	 * @param Sequence        The sequence within which to get the timecode source
	 * @return Timecode source of this sequence
	 */
	UE_DEPRECATED(5.0, "GetTimecodeSource() is no longer supported for movie scene sequences. Please use GetEarliestTimecodeSource() instead.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod, DeprecatedFunction, DeprecationMessage = "GetTimecodeSource() is no longer supported for movie scene sequences. Please use GetEarliestTimecodeSource() instead."))
	static FTimecode GetTimecodeSource(UMovieSceneSequence* Sequence);

	/**
	 * Attempt to locate a binding in this sequence by its name
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param Name            The display name of the binding to look up
	 * @return A unique identifier for the binding, or invalid
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FMovieSceneBindingProxy FindBindingByName(UMovieSceneSequence* Sequence, FString Name);

	/**
	 * Attempt to locate a binding in this sequence by its Id
	 *
	 * @param Sequence        The sequence within which to find the binding
	 * @param BindingId       The binding Id to look up
	 * @return A unique identifier for the binding, or invalid
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FMovieSceneBindingProxy FindBindingById(UMovieSceneSequence* Sequence, FGuid BindingId);

	/**
	 * Get all the bindings in this sequence
	 *
	 * @param Sequence        The sequence to get bindings for
	 * @return An array of unique identifiers for all the bindings in this sequence
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static TArray<FMovieSceneBindingProxy> GetBindings(UMovieSceneSequence* Sequence);

	/**
	* Get all the spawnables in this sequence
	*
	* @param Sequence        The sequence to get spawnables for
	* @return Spawnables in this sequence
	*/
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static TArray<FMovieSceneBindingProxy> GetSpawnables(UMovieSceneSequence* Sequence);

	/**
	* Get all the possessables in this sequence
	*
	* @param Sequence        The sequence to get possessables for
	* @return Possessables in this sequence
	*/
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static TArray<FMovieSceneBindingProxy> GetPossessables(UMovieSceneSequence* Sequence);

	/**
	 * Add a new binding to this sequence that will possess the specified object
	 *
	 * @param Sequence        The sequence to add a possessable to
	 * @param ObjectToPossess The object that this sequence should possess when evaluating
	 * @return A unique identifier for the new binding
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FMovieSceneBindingProxy AddPossessable(UMovieSceneSequence* Sequence, UObject* ObjectToPossess);

	/**
	 * Add a new binding to this sequence that will spawn the specified object
	 *
	 * @param Sequence        The sequence to add to
	 * @param ObjectToSpawn   An object instance to use as a template for spawning
	 * @return A unique identifier for the new binding
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FMovieSceneBindingProxy AddSpawnableFromInstance(UMovieSceneSequence* Sequence, UObject* ObjectToSpawn);

	/**
	 * Add a new binding to this sequence that will spawn the specified object
	 *
	 * @param Sequence        The sequence to add to
	 * @param ClassToSpawn    A class or blueprint type to spawn for this binding
	 * @return A unique identifier for the new binding
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static FMovieSceneBindingProxy AddSpawnableFromClass(UMovieSceneSequence* Sequence, UClass* ClassToSpawn);

	/**
	 * Locate all the objects that correspond to the specified object ID, using the specified context
	 *
	 * @param Sequence   The sequence to locate bound objects for
	 * @param InBinding  The object binding
	 * @param Context    Optional context to use to find the required object
	 * @return An array of all bound objects
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static TArray<UObject*> LocateBoundObjects(UMovieSceneSequence* Sequence, const FMovieSceneBindingProxy& InBinding, UObject* Context);


	/**
	 * Make a binding id for the given binding in this sequence
	 *
	 * @param MasterSequence  The master sequence that contains the sequence
	 * @param Binding The binding proxy to generate the binding id from
	 * @param Space The object binding space to resolve from (Root or Local)
	 * @return The new object binding id
	 */
	UE_DEPRECATED(5.0, "Please migrate to GetBindingID or GetPortableBindingID depending on use-case.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod, DeprecatedFunction, DeprecationMessage="Please migrate to GetBindingID or GetPortableBindingID depending on use-case."))
	static FMovieSceneObjectBindingID MakeBindingID(UMovieSceneSequence* MasterSequence, const FMovieSceneBindingProxy& InBinding, EMovieSceneObjectBindingSpace Space = EMovieSceneObjectBindingSpace::Root);


	/**
	 * Get the binding ID for a binding within a sequence.
	 * @note: The resulting binding is only valid when applied to properties within the same sequence as this binding. Use GetPortableBindingID for bindings which live in different sub-sequences.
	 *
	 * @param Binding The binding proxy to generate the binding id from
	 * @return The binding's id
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static FMovieSceneObjectBindingID GetBindingID(const FMovieSceneBindingProxy& InBinding);


	/**
	 * Get a portable binding ID for a binding that resides in a different sequence to the one where this binding will be resolved.
	 * @note: This function must be used over GetBindingID when the target binding resides in different shots or sub-sequences.
	 * @note: Only unique instances of sequences within a master sequences are supported
	 *
	 * @param MasterSequence The master sequence that contains both the destination sequence (that will resolve the binding ID) and the target sequence that contains the actual binding
	 * @param DestinationSequence The sequence that will own or resolve the resulting binding ID. For example, if the binding ID will be applied to a camera cut section pass the sequence that contains the camera cut track to this parameter.
	 * @param Binding The target binding to create the ID from
	 * @return The binding's id
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static FMovieSceneObjectBindingID GetPortableBindingID(UMovieSceneSequence* MasterSequence, UMovieSceneSequence* DestinationSequence, const FMovieSceneBindingProxy& InBinding);

	/**
	 * Make a binding for the given binding ID
	 *
	 * @param MasterSequence  The master sequence that contains the sequence
	 * @param ObjectBindingID The object binding id that has the guid and the sequence id
	 * @return The new binding proxy
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static FMovieSceneBindingProxy ResolveBindingID(UMovieSceneSequence* MasterSequence, FMovieSceneObjectBindingID InObjectBindingID);


	/**
	 * Get the root folders in the provided sequence
	 *
	 * @param Sequence	The sequence to retrieve folders from
	 * @return The folders contained within the given sequence
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static TArray<UMovieSceneFolder*> GetRootFoldersInSequence(UMovieSceneSequence* Sequence);

	/**
	 * Add a root folder to the given sequence
	 *
	 * @param Sequence			The sequence to add a folder to
	 * @param NewFolderName		The name to give the added folder
	 * @return The newly created folder
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static UMovieSceneFolder* AddRootFolderToSequence(UMovieSceneSequence* Sequence, FString NewFolderName);

	/**
	 * Remove a root folder from the given sequence. Will throw an exception if the specified folder is not valid or not a root folder.
	 *
	 * @param Sequence			The sequence That the folder belongs to
	 * @param Folder			The folder to remove
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Sequence", meta=(ScriptMethod))
	static void RemoveRootFolderFromSequence(UMovieSceneSequence* Sequence, UMovieSceneFolder* Folder);

public:

	/**
	 * Filter the specified array of tracks by class, optionally only matching exact classes
	 *
	 * @param InTracks       The array of tracks to filter
	 * @param DesiredClass   The class to match against
	 * @param bExactMatch    Whether to match sub classes or not
	 * @return A filtered array of tracks
	 */
	static TArray<UMovieSceneTrack*> FilterTracks(TArrayView<UMovieSceneTrack* const> InTracks, UClass* DesiredClass, bool bExactMatch);

public:

	/*
	 * @return Return the user marked frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static TArray<FMovieSceneMarkedFrame> GetMarkedFrames(UMovieSceneSequence* Sequence);

	/*
	 * Add a given user marked frame.
	 * A unique label will be generated if the marked frame label is empty
	 *
	 * @InMarkedFrame The given user marked frame to add
	 * @return The index to the newly added marked frame
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static int32 AddMarkedFrame(UMovieSceneSequence* Sequence, const FMovieSceneMarkedFrame& InMarkedFrame);

	/*
	 * Sets the frame number for the given marked frame index. Does not maintain sort. Call SortMarkedFrames
	 *
	 * @InMarkIndex The given user marked frame index to edit
	 * @InFrameNumber The frame number to set
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetMarkedFrame(UMovieSceneSequence* Sequence, int32 InMarkIndex, FFrameNumber InFrameNumber);

	/*
	 * Delete the user marked frame by index.
	 *
	 * @DeleteIndex The index to the user marked frame to delete
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void DeleteMarkedFrame(UMovieSceneSequence* Sequence, int32 DeleteIndex);

	/*
	 * Delete all user marked frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void DeleteMarkedFrames(UMovieSceneSequence* Sequence);

	/*
	 * Sort the marked frames in chronological order
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SortMarkedFrames(UMovieSceneSequence* Sequence);

	/*
	 * Find the user marked frame by label
	 *
	 * @InLabel The label to the user marked frame to find
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static int32 FindMarkedFrameByLabel(UMovieSceneSequence* Sequence, const FString& InLabel);

	/*
	 * Find the user marked frame by frame number
	 *
	 * @InFrameNumber The frame number of the user marked frame to find
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static int32 FindMarkedFrameByFrameNumber(UMovieSceneSequence* Sequence, FFrameNumber InFrameNumber);

	/*
	 * Find the next/previous user marked frame from the given frame number
	 *
	 * @InFrameNumber The frame number to find the next/previous user marked frame from
	 * @bForward Find forward from the given frame number.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static int32 FindNextMarkedFrame(UMovieSceneSequence* Sequence, FFrameNumber InFrameNumber, bool bForward);

	/*
	 * Set read only
	 *
	 * @bInReadOnly Whether the movie scene should be read only or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static void SetReadOnly(UMovieSceneSequence* Sequence, bool bInReadOnly);

	/*
	 * Is read only
	 *
	 * @return Whether the movie scene is read only or not
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Sequence", meta = (ScriptMethod))
	static bool IsReadOnly(UMovieSceneSequence* Sequence);
};