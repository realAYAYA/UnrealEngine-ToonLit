// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "Misc/Guid.h"
#include "MovieSceneTrack.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "TrackEditors/PropertyTrackEditors/BoolPropertyTrackEditor.h"
#include "UObject/NameTypes.h"

class FMenuBuilder;
class ISequencer;
class ISequencerTrackEditor;
class SWidget;
class UClass;
class UMovieScene;
class UMovieSceneSequence;
class UMovieSceneTrack;
class UObject;
struct FBuildEditWidgetParams;
struct FGuid;

/**
 * A property track editor for controlling the lifetime of a sapwnable object
 */
class MOVIESCENETOOLS_API FSpawnTrackEditor
	// TODO: Don't derive from bool property track editor here since it doesn't actually edit
	// bool property tracks.
	: public FBoolPropertyTrackEditor
{
public:

	/**
	 * Factory function to create an instance of this class (called by a sequencer).
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FSpawnTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	// ISequencerTrackEditor interface

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override  { return TSharedPtr<SWidget>(); }
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override { return false; }
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
    virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;

private:

	/** Callback for executing the "Add Spawn Track" menu entry. */
	void HandleAddSpawnTrackMenuEntryExecute(TArray<FGuid> ObjectBindings);
	bool CanAddSpawnTrack(FGuid ObjectBinding) const;
};
