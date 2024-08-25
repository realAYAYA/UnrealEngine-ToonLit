// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "Misc/Guid.h"
#include "MovieSceneTrack.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "MovieSceneTrackEditor.h"
#include "UObject/NameTypes.h"

class FMenuBuilder;
class ISequencer;
class SWidget;
class UClass;
class UMovieScene;
class UMovieSceneSequence;
class UMovieSceneTrack;
class UObject;
struct FBuildEditWidgetParams;
struct FGuid;

/**
 * A track editor for controlling the lifetime of an object binding
 */
class MOVIESCENETOOLS_API FBindingLifetimeTrackEditor
	: public FMovieSceneTrackEditor
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
	FBindingLifetimeTrackEditor(TSharedRef<ISequencer> InSequencer);

	void CreateNewSection(UMovieSceneTrack* Track, bool bSelect);

public:

	// ISequencerTrackEditor interface

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override { return false; }
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

private:

	/** Callback for executing the "Add Binding Lifetime Track" menu entry. */
	void HandleAddBindingLifetimeTrackMenuEntryExecute(TArray<FGuid> ObjectBindings);
	bool CanAddBindingLifetimeTrack(FGuid ObjectBinding) const;
};
