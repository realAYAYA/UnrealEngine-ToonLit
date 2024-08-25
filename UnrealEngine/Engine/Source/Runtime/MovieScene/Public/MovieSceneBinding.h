// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "MovieSceneTrack.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "MovieSceneBinding.generated.h"

class UMovieScene;
class UMovieSceneTrack;

/**
 * A set of tracks bound to runtime objects
 */
USTRUCT()
struct FMovieSceneBinding
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor. */
	FMovieSceneBinding()
#if WITH_EDITORONLY_DATA
		: SortingOrder(-1)
#endif
	{ }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InObjectGuid
	 * @param InBindingName
	 * @param InTracks
	 */
	FMovieSceneBinding(const FGuid& InObjectGuid, const FString& InBindingName, const TArray<UMovieSceneTrack*>& InTracks)
		: ObjectGuid(InObjectGuid)
		, BindingName(InBindingName)
		, Tracks(InTracks)
#if WITH_EDITORONLY_DATA
		, SortingOrder(-1)
#endif
	{ }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InObjectGuid
	 * @param InBindingName
	 */
	FMovieSceneBinding(const FGuid& InObjectGuid, const FString& InBindingName)
		: ObjectGuid(InObjectGuid)
		, BindingName(InBindingName)
#if WITH_EDITORONLY_DATA
		, SortingOrder(-1)
#endif
	{ }

	/**
	* Set the object guid
	*
	* @param InObjectGuid
	*/
	void SetObjectGuid(const FGuid& InObjectGuid)
	{
		ObjectGuid = InObjectGuid;
	}

	/**
	 * @return The guid of runtime objects in this binding
	 */
	const FGuid& GetObjectGuid() const
	{
		return ObjectGuid;
	}

	/**
	 * Set display name of the binding
	 */
	void SetName(const FString& InBindingName)
	{
		BindingName = InBindingName;
	} 

	/**
	 * @return The display name of the binding
	 */
	const FString& GetName() const
	{
		return BindingName;
	}

	/**
	 * Adds a new track to this binding
	 *
	 * @param NewTrack	The track to add
	 */
	MOVIESCENE_API void AddTrack(UMovieSceneTrack& NewTrack, UMovieScene* Owner);

	/**
	 * Removes a track from this binding
	 * 
	 * @param Track	The track to remove
	 * @return true if the track was successfully removed, false if the track could not be found
	 */
	MOVIESCENE_API bool RemoveTrack(UMovieSceneTrack& Track, UMovieScene* Owner);

	/**
	 * Removes all null tracks from this binding
	 */
	MOVIESCENE_API void RemoveNullTracks();

	/**
	 * @return All tracks in this binding
	 */
	const TArray<UMovieSceneTrack*>& GetTracks() const
	{
		return Tracks;
	}

	/**
	 * Reset all tracks in this binding, returning the previous array of tracks
	 */
	MOVIESCENE_API TArray<UMovieSceneTrack*> StealTracks(UMovieScene* Owner);

	/**
	 * Assign all tracks in this binding
	 */
	MOVIESCENE_API void SetTracks(TArray<UMovieSceneTrack*>&& InTracks, UMovieScene* Owner);

	/* For sorts so we can search quickly by Guid */
	FORCEINLINE bool operator<(const FMovieSceneBinding& RHS) const { return ObjectGuid < RHS.ObjectGuid; }
	FORCEINLINE bool operator<(const FGuid& InGuid) const { return ObjectGuid < InGuid; }
	FORCEINLINE friend bool operator<(const FGuid& InGuid, const FMovieSceneBinding& RHS) { return InGuid < RHS.GetObjectGuid(); }

#if WITH_EDITORONLY_DATA
	/**
	* Get this folder's desired sorting order
	*/
	int32 GetSortingOrder() const
	{
		return SortingOrder;
	}

	/**
	* Set this folder's desired sorting order.
	*
	* @param InSortingOrder The higher the value the further down the list the folder will be.
	*/
	void SetSortingOrder(const int32 InSortingOrder)
	{
		SortingOrder = InSortingOrder;
	}
#endif

private:

	/** Object binding guid for runtime objects */
	UPROPERTY()
	FGuid ObjectGuid;
	
	/** Display name */
	UPROPERTY()
	FString BindingName;

	/** All tracks in this binding */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMovieSceneTrack>> Tracks;

#if WITH_EDITORONLY_DATA
	/** The desired sorting order for this binding in Sequencer */
	UPROPERTY()
	int32 SortingOrder;
#endif
};
