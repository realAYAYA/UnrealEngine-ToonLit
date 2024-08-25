// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceSpawnRegister.h"
#include "UObject/ObjectKey.h"

struct FNewSpawnable;

class IMovieScenePlayer;
class ISequencer;
class UMovieScene;
class UMovieSceneSequence;
class FObjectPreSaveContext;

/**
 * Spawn register used in the editor to add some usability features like maintaining selection states, and projecting spawned state onto spawnable defaults
 */
class LEVELSEQUENCEEDITOR_API FLevelSequenceEditorSpawnRegister
	: public FLevelSequenceSpawnRegister
{
public:

	/** Constructor */
	FLevelSequenceEditorSpawnRegister();

	/** Destructor. */
	~FLevelSequenceEditorSpawnRegister();

public:

	void SetSequencer(const TSharedPtr<ISequencer>& Sequencer);

public:

	// FLevelSequenceSpawnRegister interface

	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) override;
	virtual void PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID) override;
	virtual void SaveDefaultSpawnableState(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, TSharedRef<const FSharedPlaybackState> SharedPlaybackState) override;
#if WITH_EDITOR
	virtual TValueOrError<FNewSpawnable, FText> CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene, UActorFactory* ActorFactory = nullptr) override;
	virtual void SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const TOptional<FTransformData>& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings) override;
	virtual void HandleConvertPossessableToSpawnable(UObject* OldObject, TSharedRef<const FSharedPlaybackState> SharedPlaybackState, TOptional<FTransformData>& OutTransformData) override;
	virtual bool CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const override;
#endif

private:

	/** Called when the editor selection has changed. */
	void HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	/** Saves the default state for the specified spawnable, if an instance for it currently exists */
	void SaveDefaultSpawnableState(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID);
	void SaveDefaultSpawnableStateImpl(FMovieSceneSpawnable& Spawnable, UMovieSceneSequence* Sequence, UObject* SpawnedObject, TSharedRef<const FSharedPlaybackState> SharedPlaybackState);

	/** Called from the editor when a blueprint object replacement has occurred */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/** Called whenever an object is modified in the editor */
	void OnObjectModified(UObject* ModifiedObject);

	/** Called before an object is saved in the editor */
	void OnPreObjectSaved(UObject* Object, FObjectPreSaveContext SaveContext);

	/** Called on pre/post GC */
	void UpdateIsEngineCollectingGarbage(bool bIsCollectingGarbage);

private:

	struct FTrackedObjectState
	{
		FTrackedObjectState(FMovieSceneSequenceIDRef InTemplateID, const FGuid& InObjectBindingID) : TemplateID(InTemplateID), ObjectBindingID(InObjectBindingID), bHasBeenModified(false) {}

		/** The sequence ID that spawned this object */
		FMovieSceneSequenceID TemplateID;

		/** The object binding ID of the object in the template */
		FGuid ObjectBindingID;

		/** true if this object has been modified since it was spawned and is different from the current object template */
		bool bHasBeenModified;
	};

private:

	/** Handles for delegates that we've bound to. */
	FDelegateHandle OnActorSelectionChangedHandle;

	/** Set of spawn register keys for objects that should be selected if they are spawned. */
	TSet<FMovieSceneSpawnRegisterKey> SelectedSpawnedObjects;

	/** Map from a sequenceID to an array of objects that have been tracked */
	TMap<FObjectKey, FTrackedObjectState> TrackedObjects;

	/** Set of UMovieSceneSequences that this register has spawned objects for that are modified */
	TSet<FObjectKey> SequencesWithModifiedObjects;

	/** True if we should clear the above selection cache when the editor selection has been changed. */
	bool bShouldClearSelectionCache;

	/** Weak pointer to the active sequencer. */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Handle to a delegate that is bound to FCoreUObjectDelegates::OnObjectModified to harvest changes to spawned objects. */
	FDelegateHandle OnObjectModifiedHandle;

	/** Handle to a delegate that is bound to FCoreUObjectDelegates::OnObjectPreSave to harvest changes to spawned objects. */
	FDelegateHandle OnObjectSavedHandle;

	/** True when GCing */
	bool bIsEngineCollectingGarbage;

	/** Handle to a delegate that is bound to FCoreUObjectDelegates::OnPre/PostGarbageCollectHandle to disable saving changes while GCing. */
	FDelegateHandle OnPreGarbageCollectHandle;
	FDelegateHandle OnPostGarbageCollectHandle;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
