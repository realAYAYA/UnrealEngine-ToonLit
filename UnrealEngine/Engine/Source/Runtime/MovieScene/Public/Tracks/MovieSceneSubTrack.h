// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Internationalization/Text.h"
#include "Misc/FrameNumber.h"
#include "Misc/InlineValue.h"
#include "MovieSceneNameableTrack.h"
#include "MovieSceneSection.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneSubTrack.generated.h"

class UMovieSceneSequence;
class UMovieSceneSubSection;
class UObject;
struct FMovieSceneSegmentCompilerRules;

/**
 * A track that holds sub-sequences within a larger sequence.
 */
UCLASS()
class MOVIESCENE_API UMovieSceneSubTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_BODY()

public:

	UMovieSceneSubTrack( const FObjectInitializer& ObjectInitializer );

	/**
	 * Adds a movie scene section at the requested time.
	 *
	 * @param Sequence The sequence to add
	 * @param StartTime The time to add the section at
	 * @param Duration The duration of the section in frames
	 * @return The newly created sub section
	 */
	virtual UMovieSceneSubSection* AddSequence(UMovieSceneSequence* Sequence, FFrameNumber StartTime, int32 Duration) { return AddSequenceOnRow(Sequence, StartTime, Duration, INDEX_NONE); }

	/**
	 * Adds a movie scene section at the requested time.
	 *
	 * @param Sequence The sequence to add
	 * @param StartTime The time to add the section at
	 * @param Duration The duration of the section in frames
	 * @param bInsertSequence Whether or not to insert the sequence and push existing sequences out
	 * @return The newly created sub section
	 */
	virtual UMovieSceneSubSection* AddSequenceOnRow(UMovieSceneSequence* Sequence, FFrameNumber StartTime, int32 Duration, int32 RowIndex);

	/**
	 * Check whether this track contains the given sequence.
	 *
	 * @param Sequence The sequence to find.
	 * @param Recursively Whether to search for the sequence in sub-sequences.
	 * @param SectionToSkip Skip this section when searching the track (ie. the section is already set to this sequence). 
	 * @return true if the sequence is in this track, false otherwise.
	 */
	bool ContainsSequence(const UMovieSceneSequence& Sequence, bool Recursively = false, const UMovieSceneSection* SectionToSkip = nullptr) const;

public:

	// UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveAllAnimationData() override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool SupportsMultipleRows() const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

protected:

	/** All movie scene sections. */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> Sections;
};
