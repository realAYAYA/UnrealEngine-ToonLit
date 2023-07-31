// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneSection.h"
#include "CinePrestreamingData.h"
#include "MovieSceneCinePrestreamingSection.generated.h"

class UCinePrestreamingData;

/** Movie Scene Section representing a Prestreaming asset. */
UCLASS(MinimalAPI)
class UMovieSceneCinePrestreamingSection
	: public UMovieSceneSection
	, public IMovieSceneEntityProvider
{
	GENERATED_BODY()

public:
	UMovieSceneCinePrestreamingSection(const FObjectInitializer& ObjInit);

	/** Get the prestreaming asset soft pointer. Use for async loading. */
	UFUNCTION(BlueprintPure, Category = "Cinematic Prestreaming")
	TSoftObjectPtr<UCinePrestreamingData> GetPrestreamingAsset() const { return PrestreamingAsset; }

	UFUNCTION(BlueprintCallable, Category = "Cinematic Prestreaming")
	void SetPrestreamingAsset(const UCinePrestreamingData* InData) { PrestreamingAsset = TSoftObjectPtr<UCinePrestreamingData>(InData); }

	/** If MovieScene.PreStream.QualityLevel is less than this then discard this section at runtime. */
	UFUNCTION(BlueprintPure, Category = "Cinematic Prestreaming")
	int32 GetQualityLevel() const { return QualityLevel; };

	UFUNCTION(BlueprintCallable, Category = "Cinematic Prestreaming")
	void SetQualityLevel(const int32 InLevel) { QualityLevel = InLevel; }

	UFUNCTION(BlueprintCallable, Category = "Cinematic Prestreaming")
	void SetStartFrameOffset(const int32 InOffset) { StartFrameOffset = InOffset; }

	/** UMovieSceneSection interface */
	TOptional<FFrameTime> GetOffsetTime() const override { return TOptional<FFrameTime>(StartFrameOffset); }
	

protected:
	/** IMovieSceneEntityProvider interface */
	void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;

protected:
	/** The asset containing cinematic prestreaming data to use for this section. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prestreaming)
	TSoftObjectPtr<UCinePrestreamingData> PrestreamingAsset;

	/** Number of frames by which to offset the evaluation. Larger values cause prestreaming to happen earlier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prestreaming)
	int32 StartFrameOffset = 0;

	/** If If MovieScene.PreStream.QualityLevel is less than this then discard this section at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prestreaming, AdvancedDisplay)
	int32 QualityLevel = 0;
};


/** Component data for the cinematic prestreaming system. */
USTRUCT()
struct FMovieSceneCinePrestreamingComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UMovieSceneCinePrestreamingSection> Section = nullptr;
};
