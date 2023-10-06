// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTextTrack.h"
#include "PropertyTrackEditor.h"

class UMovieSceneKeyStructType;
class FSequencerKeyStructGenerator;
struct FMovieSceneTextChannel;

/** Track Editor for Text Property */
class FTextPropertyTrackEditor : public FPropertyTrackEditor<UMovieSceneTextTrack>
{
public:
	FTextPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor(InSequencer, GetAnimatedPropertyTypes())
	{
	}

	/** Retrieve a list of all property types that this track editor animates  */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromPropertyTypeName(NAME_TextProperty) });
	}

	/**
	 * Creates an instance of this class (called by a sequencer).
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:
	//~ Begin FPropertyTrackEditor
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys) override;
	//~ End FPropertyTrackEditor
};

bool CanCreateKeyEditor(const FMovieSceneTextChannel* Channel);

TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneTextChannel>& Channel
	, UMovieSceneSection* Section
	, const FGuid& InObjectBindingID
	, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings
	, TWeakPtr<ISequencer> InSequencer);

UMovieSceneKeyStructType* InstanceGeneratedStruct(FMovieSceneTextChannel* Channel, FSequencerKeyStructGenerator* Generator);

void PostConstructKeyInstance(const TMovieSceneChannelHandle<FMovieSceneTextChannel>& ChannelHandle
	, FKeyHandle InHandle
	, FStructOnScope* Struct);
