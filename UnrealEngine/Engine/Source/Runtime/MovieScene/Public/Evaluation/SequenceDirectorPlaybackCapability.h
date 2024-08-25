// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Evaluation/MovieScenePlaybackCapabilities.h"
#include "MovieSceneSequenceID.h"
#include "UObject/GCObject.h"

namespace UE::MovieScene
{

struct FSharedPlaybackState;

/**
 * Playback capability for sequences that have a director blueprint.
 */
struct MOVIESCENE_API FSequenceDirectorPlaybackCapability
{
	/** Playback capability ID */
	static TPlaybackCapabilityID<FSequenceDirectorPlaybackCapability> ID;

	/** Remove all director blueprint instances */
	void ResetDirectorInstances();

	/** Gets a new or existing director blueprint instance for the given root or sub sequence */
	UObject* GetOrCreateDirectorInstance(TSharedRef<const FSharedPlaybackState> SharedPlaybackState, FMovieSceneSequenceIDRef SequenceID);

private:

	// The actual cache of BP instances is stored in a heap-allocated object
	// because it neeeds to be an FGCObject to keep those instances alive, and
	// an FGCObject isn't relocatable so we can't put it in a playback 
	// capabailities container.
	struct FDirectorInstanceCache : FGCObject
	{
		virtual FString GetReferencerName() const override;
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		TSortedMap<FMovieSceneSequenceID, TObjectPtr<UObject>> DirectorInstances;
	};
	TUniquePtr<FDirectorInstanceCache> Cache;
};

}  // namespace UE::MovieScene

