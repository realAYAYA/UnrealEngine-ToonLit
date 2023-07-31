// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "KeyPropertyParams.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "PropertyTrackEditor.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "Misc/LargeWorldCoordinates.h"

/**
 * A property track editor for float vectors.
 */
class FFloatVectorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneFloatVectorTrack>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FFloatVectorPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor(InSequencer, GetAnimatedPropertyTypes())
	{
	}

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<3>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<3>>({
			FAnimatedPropertyKey::FromStructType(NAME_Vector2f),
			FAnimatedPropertyKey::FromStructType(NAME_Vector3f),
			FAnimatedPropertyKey::FromStructType(NAME_Vector4f)
		});
	}

	/**
	 * Creates an instance of this class (called by a sequence).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:
	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys) override;
	virtual void InitializeNewTrack(UMovieSceneFloatVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams) override;
	virtual bool ModifyGeneratedKeysByCurrentAndWeight(UObject* Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber Time, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const override;

private:

	static FName XName;
	static FName YName;
	static FName ZName;
	static FName WName;
};

/**
 * A property track editor for double vectors.
 */
class FDoubleVectorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneDoubleVectorTrack>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FDoubleVectorPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor(InSequencer, GetAnimatedPropertyTypes())
	{
	}

	/**
	 * Retrieve a list of all property types that this track editor animates
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({
			FAnimatedPropertyKey::FromStructType(NAME_Vector),
			FAnimatedPropertyKey::FromStructType(NAME_Vector4),
			FAnimatedPropertyKey::FromStructType(NAME_Vector2D),
			FAnimatedPropertyKey::FromStructType(NAME_Vector3d),
			FAnimatedPropertyKey::FromStructType(NAME_Vector4d)
		});
	}

	/**
	 * Creates an instance of this class (called by a sequence).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:
	//~ FPropertyTrackEditor interface

	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys) override;
	virtual void InitializeNewTrack(UMovieSceneDoubleVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams) override;
	virtual bool ModifyGeneratedKeysByCurrentAndWeight(UObject* Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber Time, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const override;

private:

	static FName XName;
	static FName YName;
	static FName ZName;
	static FName WName;
};
