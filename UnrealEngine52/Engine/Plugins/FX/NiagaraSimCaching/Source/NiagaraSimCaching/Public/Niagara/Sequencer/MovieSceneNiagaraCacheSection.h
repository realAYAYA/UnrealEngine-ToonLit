// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSimCache.h"
#include "Sections/MovieSceneBaseCacheSection.h"
#include "MovieSceneNiagaraCacheSection.generated.h"

UENUM(BlueprintType)
enum class ENiagaraSimCacheSectionPlayMode : uint8
{
	/**
	When the sequence has no cached data to display, the Niagara component runs the simulation normally
	*/
	SimWithoutCache,

	/**
	When the sequence has no cached data to display, the Niagara component is disabled
	*/
	DisplayCacheOnly,
};

UENUM(BlueprintType)
enum class ENiagaraSimCacheSectionStretchMode : uint8
{
	/**
	When the cache section is stretched in the track it will repeat the cached data 
	*/
	Repeat,

	/**
	When the cache section is stretched in the track it will dilate the input time so the cached data is stretched once over the full section
	*/
	TimeDilate,
};

USTRUCT()
struct NIAGARASIMCACHING_API FMovieSceneNiagaraCacheParams : public FMovieSceneBaseCacheParams
{
	GENERATED_BODY()

	FMovieSceneNiagaraCacheParams();
	virtual ~FMovieSceneNiagaraCacheParams() override {}

	/** Gets the animation sequence length, not modified by play rate */
	virtual float GetSequenceLength() const override;

	UPROPERTY(EditAnywhere, Category = "NiagaraCache")
	FNiagaraSimCacheCreateParameters CacheParameters;
	
	/** The sim cache this section plays and records into */
	UPROPERTY(EditAnywhere, Category = "NiagaraCache")
	TObjectPtr<UNiagaraSimCache> SimCache;

	/** What should the effect do when the track has no cache data to display */
	UPROPERTY(EditAnywhere, Category="SimCache")
	ENiagaraSimCacheSectionPlayMode CacheReplayPlayMode = ENiagaraSimCacheSectionPlayMode::DisplayCacheOnly;

	/** What should the effect do when the cache section is stretched? */
	UPROPERTY(EditAnywhere, Category="SimCache")
	ENiagaraSimCacheSectionStretchMode SectionStretchMode = ENiagaraSimCacheSectionStretchMode::TimeDilate;
};

/**
 * Movie scene section that control NiagaraCache playback
 */
UCLASS(MinimalAPI)
class UMovieSceneNiagaraCacheSection : public UMovieSceneBaseCacheSection
{
	GENERATED_UCLASS_BODY()

public:

	UMovieSceneNiagaraCacheSection();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, Category = "NiagaraCache", meta = (ShowOnlyInnerProperties))
	FMovieSceneNiagaraCacheParams Params;

	UPROPERTY()
	bool bCacheOutOfDate = false;
};
