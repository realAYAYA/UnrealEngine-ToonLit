// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineTypes.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"
#include "Evaluation/MovieSceneSectionParameters.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "GameFramework/Actor.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Math/Range.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameTime.h"
#include "Misc/Optional.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequenceID.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneSubSection.generated.h"

class FProperty;
class UMovieScene;
class UMovieSceneEntitySystemLinker;
class UMovieSceneSequence;
class UObject;
namespace UE { namespace MovieScene { struct FEntityImportParams; } }
namespace UE { namespace MovieScene { struct FImportedEntity; } }
struct FFrame;
struct FFrameRate;
struct FMovieSceneEvaluationTemplate;
struct FMovieSceneSectionParameters;
struct FMovieSceneTrackCompilerArgs;
struct FPropertyChangedEvent;
struct FQualifiedFrameTime;

DECLARE_DELEGATE_OneParam(FOnSequenceChanged, UMovieSceneSequence* /*Sequence*/);

struct FSubSequenceInstanceDataParams
{
	/** The ID of the sequence instance that is being generated */
	FMovieSceneSequenceID InstanceSequenceID;

	/** The object binding ID in which the section to be generated resides */
	FMovieSceneEvaluationOperand Operand;
};

/**
 * Implements a section in sub-sequence tracks.
 */
UCLASS(BlueprintType, config = EditorPerProjectUserSettings)
class MOVIESCENE_API UMovieSceneSubSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

public:

	/** Object constructor. */
	UMovieSceneSubSection(const FObjectInitializer& ObjInitializer);

	/**
	 * Get the sequence that is assigned to this section.
	 *
	 * @return The sequence.
	 * @see SetSequence
	 */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Section")
	UMovieSceneSequence* GetSequence() const;

	/**
	 * Get the path name to this sub section from the outer moviescene
	 */
	FString GetPathNameInMovieScene() const;

	/**
	 * Get this sub section's sequence ID
	 */
	FMovieSceneSequenceID GetSequenceID() const;

	/** Generate subsequence data */
	virtual FMovieSceneSubSequenceData GenerateSubSequenceData(const FSubSequenceInstanceDataParams& Params) const;

public:

	/**
	 * Gets the transform that converts time from this section's time-base to its inner sequence's
	 */
	FMovieSceneSequenceTransform OuterToInnerTransform() const;

	/**
	 * Gets the playrange of the inner sequence, in the inner sequence's time space, trimmed with any start/end offsets,
	 * and validated to make sure we get at least a 1-frame long playback range (e.g. in the case where excessive
	 * trimming results in an invalid range).
	 */
	bool GetValidatedInnerPlaybackRange(TRange<FFrameNumber>& OutInnerPlaybackRange) const;

	/**
	 * Helper function used by the above method, but accessible for other uses like track editors.
	 */
	static TRange<FFrameNumber> GetValidatedInnerPlaybackRange(const FMovieSceneSectionParameters& SubSectionParameters, const UMovieScene& InnerMovieScene);

	/**
	 * Sets the sequence played by this section.
	 *
	 * @param Sequence The sequence to play.
	 * @see GetSequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void SetSequence(UMovieSceneSequence* Sequence);

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Delegate to fire when our sequence is changed in the property editor */
	FOnSequenceChanged& OnSequenceChanged() { return OnSequenceChangedDelegate; }
#endif

	FFrameNumber MapTimeToSectionFrame(FFrameTime InPosition) const;

	EMovieSceneServerClientMask GetNetworkMask() const
	{
		return (EMovieSceneServerClientMask)NetworkMask;
	}

	void SetNetworkMask(EMovieSceneServerClientMask InNetworkMask)
	{
		NetworkMask = (uint8)InNetworkMask;
	}

public:

	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual UMovieSceneSection* SplitSection( FQualifiedFrameTime SplitTime, bool bDeleteKeys ) override;
	virtual void TrimSection( FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override { return TOptional<FFrameTime>(FFrameTime(Parameters.StartFrameOffset)); }
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate) override;

protected:

	void BuildDefaultSubSectionComponents(UMovieSceneEntitySystemLinker* EntityLinker, const UE::MovieScene::FEntityImportParams& Params, UE::MovieScene::FImportedEntity* OutImportedEntity) const;

public:

	UPROPERTY(config, BlueprintReadWrite, EditAnywhere, Category="General", meta=(ShowOnlyInnerProperties))
	FMovieSceneSectionParameters Parameters;

private:

	UPROPERTY()
	float StartOffset_DEPRECATED;

	UPROPERTY()
	float TimeScale_DEPRECATED;

	UPROPERTY()
	float PrerollTime_DEPRECATED;

	UPROPERTY(EditAnywhere, Category="Networking", meta=(Bitmask, BitmaskEnum="/Script/MovieScene.EMovieSceneServerClientMask"))
	uint8 NetworkMask;

protected:

	/** Movie scene being played by this section */
	UPROPERTY(EditAnywhere, Category="Sequence")
	TObjectPtr<UMovieSceneSequence> SubSequence;

#if WITH_EDITOR
	/** Delegate to fire when our sequence is changed in the property editor */
	FOnSequenceChanged OnSequenceChangedDelegate;

	/* Previous sub sequence, restored if changed sub sequence is invalid*/
	UMovieSceneSequence* PreviousSubSequence;
#endif
};
