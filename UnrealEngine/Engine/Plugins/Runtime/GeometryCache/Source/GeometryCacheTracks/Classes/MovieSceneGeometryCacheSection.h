// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneGeometryCacheSection.generated.h"

class UGeometryCache;
struct FQualifiedFrameTime;

USTRUCT(BlueprintType)
 struct GEOMETRYCACHETRACKS_API FMovieSceneGeometryCacheParams
{
	GENERATED_BODY()

	FMovieSceneGeometryCacheParams();

	/** Gets the animation sequence length, not modified by play rate */
	 float GetSequenceLength() const;
	/** The animation this section plays */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometryCache", DisplayName = "Geometry Cache")
	TObjectPtr<UGeometryCache> GeometryCacheAsset;

	/** The offset for the first loop of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometryCache")
	FFrameNumber FirstLoopStartFrameOffset;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometryCache")
	FFrameNumber StartFrameOffset;

	/** The offset into the end of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometryCache")
	FFrameNumber EndFrameOffset;

	/** The playback rate of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometryCache")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	uint32 bReverse : 1;

	UPROPERTY()
	float StartOffset_DEPRECATED;

	UPROPERTY()
	float EndOffset_DEPRECATED;

	UPROPERTY()
	FSoftObjectPath GeometryCache_DEPRECATED;
};

/**
 * Movie scene section that control geometry cache playback
 */
UCLASS(MinimalAPI)
class UMovieSceneGeometryCacheSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation", meta = (ShowOnlyInnerProperties))
	FMovieSceneGeometryCacheParams Params;

	/** Get Frame Time as Animation Time*/
	virtual float MapTimeToAnimation(float ComponentDuration, FFrameTime InPosition, FFrameRate InFrameRate) const;

protected:
	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
	virtual void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate) override;

	/** ~UObject interface */
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

private:

	//~ UObject interface

#if WITH_EDITOR

	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
public:
	float PreviousPlayRate;

#endif

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Channels/MovieSceneFloatChannel.h"
#include "CoreMinimal.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#endif
