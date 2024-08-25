// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/MovieGraphDataTypes.h"
#include "Misc/FrameTime.h"
#include "MovieSceneTimeController.h"

#include "MovieGraphSequenceDataSource.generated.h"

// Forward Declares
class ULevelSequence;
class ALevelSequenceActor;
class UMovieSceneSequencePlayer;

namespace UE::MovieGraph
{
	struct FMovieGraphSequenceTimeController : public FMovieSceneTimeController
	{
		virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;
		void SetCachedFrameTiming(const FQualifiedFrameTime& InTimeCache) { TimeCache = InTimeCache; }

	private:
		/** Simply store the number calculated and return it when requested. */
		FQualifiedFrameTime TimeCache;
	};
}

/**
* The UMovieGraphSequenceDataSource allows using a ULevelSequence as the external datasource for the Movie Graph.
* It will build the range of shots based on the contents of the level sequence (one shot per camera cut found inside
* the sequence hierarchy, not allowing overlapping Camera Cut sections), and then it will evaluate the level sequence
* for the given time when rendering.
*/
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphSequenceDataSource : public UMovieGraphDataSourceBase
{
	GENERATED_BODY()

public:
	UMovieGraphSequenceDataSource();
	virtual void CacheDataPreJob(const FMovieGraphInitConfig& InInitConfig) override;
	virtual void RestoreCachedDataPostJob() override;
	virtual void UpdateShotList() override;
	virtual FFrameRate GetTickResolution() const override;
	virtual FFrameRate GetDisplayRate() const override;
	virtual void SyncDataSourceTime(const FFrameTime& InTime) override;
	virtual void PlayDataSource() override;
	virtual void PauseDataSource() override;
	virtual void JumpDataSource(const FFrameTime& InTimeToJumpTo) override;
	virtual void CacheHierarchyForShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) override;
	virtual void RestoreHierarchyForShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) override;
	virtual void MuteShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) override;
	virtual void UnmuteShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) override;
	virtual void ExpandShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot, const int32 InLeftDeltaFrames, const int32 InLeftDeltaFramesUserPoV,
		const int32 InRightDeltaFrames, const bool bInPrepass) override;
protected:
	void OverrideSequencePlaybackRangeFromGlobalOutputSettings(ULevelSequence* InSequence);
	void CacheLevelSequenceData(ULevelSequence* InSequence);
	void OnSequenceEvaluated(const UMovieSceneSequencePlayer& Player, FFrameTime CurrentTime, FFrameTime PreviousTime);

protected:
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> LevelSequenceActor;

	/** Custom Time Controller for the Sequence Player, used to match Custom TimeStep without any floating point accumulation errors. */
	TSharedPtr<UE::MovieGraph::FMovieGraphSequenceTimeController> CustomSequenceTimeController;

	TSharedPtr<MoviePipeline::FCameraCutSubSectionHierarchyNode> CachedSequenceHierarchyRoot;
};