// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSimCache.h"
#include "Sections/MovieSceneBaseCacheSection.h"
#include "MovieSceneNiagaraCacheSection.generated.h"

USTRUCT()
struct NIAGARASIMCACHING_API FMovieSceneNiagaraCacheParams : public FMovieSceneBaseCacheParams
{
	GENERATED_BODY()

	FMovieSceneNiagaraCacheParams();
	virtual ~FMovieSceneNiagaraCacheParams() override {}

	/** Gets the animation sequence length, not modified by play rate */
	virtual float GetSequenceLength() const override;

	UPROPERTY(VisibleAnywhere, Category = "NiagaraCache")
	FNiagaraSimCacheCreateParameters CacheParameters;
	
	/** The sim cache this section plays and records into */
	UPROPERTY()
	TObjectPtr<UNiagaraSimCache> SimCache;
};

/**
 * Movie scene section that control NiagaraCache playback
 */
UCLASS(MinimalAPI)
class UMovieSceneNiagaraCacheSection : public UMovieSceneBaseCacheSection
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "NiagaraCache", meta = (ShowOnlyInnerProperties))
	FMovieSceneNiagaraCacheParams Params;
};
