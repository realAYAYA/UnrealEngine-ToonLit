// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "HAL/ThreadSafeBool.h"
#include "MoviePipelineBase.h"
#include "MovieGraphDataTypes.h"
#include "MovieGraphConfig.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieGraphPipeline.generated.h"

// Forward Declares
class UMoviePipelineExecutorJob;
class UMoviePipelineExecutorShot;
class UMovieGraphFileOutputNode;
class IImageWriteQueue;

namespace UE::MovieGraph
{
	typedef TTuple<TFuture<bool>, UE::MovieGraph::FMovieGraphOutputFutureData> FMovieGraphOutputFuture;
}

UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphPipeline : public UMoviePipelineBase
{
	GENERATED_BODY()

public:
	UMovieGraphPipeline();

	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	void Initialize(UMoviePipelineExecutorJob* InJob, const FMovieGraphInitConfig& InitConfig);

	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	UMoviePipelineExecutorJob* GetCurrentJob() const { return CurrentJob; }

	/**
	* Returns the time this movie pipeline was initialized at.
	*/
	UFUNCTION(BlueprintPure, Category = "Movie Graph")
	FDateTime GetInitializationTime() const { return GraphInitializationTime; }

	/**
	* Override the time this movie pipeline was initialized at. This can be used for render farms
	* to ensure that jobs on all machines use the same date/time instead of each calculating it locally.
	*
	* Needs to be called after ::Initialize(...)
	*
	* @param InDateTime - The DateTime object to return for GetInitializationTime.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	void SetInitializationTime(const FDateTime& InDateTime) { GraphInitializationTime = InDateTime; }

public:
	UMovieGraphConfig* GetRootGraphForShot(UMoviePipelineExecutorShot* InShot) const;
	FMovieGraphTraversalContext GetCurrentTraversalContext() const;

	/** Get the Active Shot list, which is the full shot list generated from the external data source, with disabled shots removed. */
	const TArray<TObjectPtr<UMoviePipelineExecutorShot>>& GetActiveShotList() const { return ActiveShotList; }
	/** Which index of the Active Shot List are we currently on */
	int32 GetCurrentShotIndex() const { return CurrentShotIndex; }
	/** Called by the TimeStepInstance when it's time to set up for another shot. Don't call this unless you know what you're doing. */
	void SetupShot(UMoviePipelineExecutorShot* InShot);
	/** Called by the TimeStepInstance when it's time to tear down the current shot. Don't call this unless you know what you're doing. */
	void TeardownShot(UMoviePipelineExecutorShot* InShot);
	/** Used occasionally to cross-reference other components. Don't call this unless you know what you're doing. */
	UMovieGraphTimeStepBase* GetTimeStepInstance() const { return GraphTimeStepInstance; }
	/** Used occasionally to cross-reference other components. Don't call this unless you know what you're doing. */
	UMovieGraphRendererBase* GetRendererInstance() const { return GraphRendererInstance; }
	/** Used occasionally to cross-reference other components. Don't call this unless you know what you're doing. */
	UMovieGraphDataSourceBase* GetDataSourceInstance() const { return GraphDataSourceInstance; }
	/** Gets the Output Merger for this Movie Pipeline which is responsible for gathering all of the data coming in for a given output frame, before making it available to the MovieGraphPipeline. */
	TSharedPtr<UE::MovieGraph::IMovieGraphOutputMerger> GetOutputMerger() const { return OutputMerger; }
	/** Writing images to disk is an async process. When you start writing, declare a future with the filename you will eventually write to, and complete the future once it is on disk. */
	void AddOutputFuture(TFuture<bool>&& InOutputFuture, const UE::MovieGraph::FMovieGraphOutputFutureData& InData);

protected:
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	virtual void OnMoviePipelineFinishedImpl();

protected:
	virtual void OnEngineTickBeginFrame();
	virtual void OnEngineTickEndFrame();
	virtual void RenderFrame();
	virtual void BuildShotListFromDataSource();
	virtual void ProcessOutstandingFinishedFrames();
	virtual void ProcessOutstandingFutures();

	virtual void TickProducingFrames();
	virtual void TickPostFinalizeExport(const bool bInForceFinish);
	virtual void TickFinalizeOutputContainers(const bool bInForceFinish);
	virtual void TransitionToState(const EMovieRenderPipelineState InNewState);
	virtual const TSet<TObjectPtr<UMovieGraphFileOutputNode>> GetOutputNodesUsed() const;
	virtual void BeginFinalize();

	// UMoviePipelineBase Interface
	virtual void RequestShutdownImpl(bool bIsError) override;
	virtual void ShutdownImpl(bool bIsError) override;
	virtual bool IsShutdownRequestedImpl() const override { return bShutdownRequested; }
	virtual EMovieRenderPipelineState GetPipelineStateImpl() const override { return PipelineState; }
	// ~UMoviePipelineBase Interface


protected:
	UPROPERTY(Transient)
	TObjectPtr<UMovieGraphTimeStepBase> GraphTimeStepInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMovieGraphRendererBase> GraphRendererInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMovieGraphDataSourceBase> GraphDataSourceInstance;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineExecutorJob> CurrentJob;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMoviePipelineExecutorShot>> ActiveShotList;

	/**
	* An array of Node CDOs that we sent data through to write data to disk.
	* This is the list of output nodes that will have OnAllFramesSubmitted/IsFinishedWritingToDisk
	* on them.
	*/
	UPROPERTY(Transient)
	TSet<TObjectPtr<UMovieGraphFileOutputNode>> OutputNodesDataSentTo;

	int32 CurrentShotIndex;

	/** True if we're in a TransitionToState call. Used to prevent reentrancy. */
	bool bIsTransitioningState;

	/** True if RequestShutdown() was called. At the start of the next frame we will stop producing frames and start shutting down. */
	FThreadSafeBool bShutdownRequested;

	/** Set to true during Shutdown/RequestShutdown if we are shutting down due to an error. */
	FThreadSafeBool bShutdownSetErrorFlag;

	/** Which step of the rendering process is the graph currently in. */
	EMovieRenderPipelineState PipelineState;

	/** What time (in UTC) was Initialization called? Used internally for tracking total job duration. */
	FDateTime GraphInitializationTime;

	/** This gathers all of the produced data for an output frame (which may come in async, many frames later) before passing them onto the Output Containers. */
	TSharedPtr<UE::MovieGraph::IMovieGraphOutputMerger> OutputMerger;

	/** A debug image sequence writer in the event they want to dump every sample generated on its own. */
	IImageWriteQueue* Debug_ImageWriteQueue;

	/** 
	* An array of Output Futures for files that have started writing to disk, but have not finished. 
	* Each frame we take each future that finishes writing to disk and push it into the GeneratedOutputData array.
	*/
	TArray<UE::MovieGraph::FMovieGraphOutputFuture> OutstandingOutputFutures;

	/**
	* An array of output data, one per shot. This only keeps track of the filenames generated during
	* rendering (and is only pushed to once the file is actually on disk), and does not store pixel data.
	*/
	TArray<FMovieGraphRenderOutputData> GeneratedOutputData;
};