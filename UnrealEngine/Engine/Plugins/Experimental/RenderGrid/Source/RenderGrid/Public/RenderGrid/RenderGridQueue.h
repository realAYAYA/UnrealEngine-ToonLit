// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGridManager.h"
#include "RenderGridUtils.h"
#include "RenderGridQueue.generated.h"


class UMoviePipelineSetting;
class UMoviePipelineOutputBase;
struct FMoviePipelineOutputData;
class URenderGrid;
class URenderGridJob;
class UMoviePipelineExecutorBase;
class UMoviePipelinePIEExecutor;
class UMoviePipelineQueue;
class UMoviePipelineExecutorJob;
class URenderGridQueue;

namespace UE::RenderGrid::Private
{
	class FRenderGridGenericExecutionQueue;
}


namespace UE::RenderGrid
{
	/** A delegate for when a rendering queue is about to start. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRenderGridQueueStarted, URenderGridQueue* /*Queue*/);

	/** A delegate for when a rendering queue has finished. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRenderGridQueueFinished, URenderGridQueue* /*Queue*/, bool /*bSuccess*/);

	/**
	 * The arguments for the URenderGridQueue::Create function.
	 */
	struct RENDERGRID_API FRenderGridQueueCreateArgs
	{
	public:
		/** The render grid of the given render grid jobs that will be rendered. */
		TObjectPtr<URenderGrid> RenderGrid = nullptr;

		/** The specific render grid jobs that will be rendered. */
		TArray<TObjectPtr<URenderGridJob>> RenderGridJobs;

		/** If not null, it will override the movie pipeline executor class with this class. */
		TSubclassOf<UMoviePipelinePIEExecutor> PipelineExecutorClass = nullptr;

		/** The movie pipeline settings classes to disable (things like Anti-Aliasing, High-Res, etc). */
		TArray<TSubclassOf<UMoviePipelineSetting>> DisablePipelineSettingsClasses;

		/** Whether it should run the Begin and End Batch Render events or not. */
		bool bIsBatchRender = false;

		/** Whether it should run invisibly (so without any UI elements popping up during rendering) or not. */
		bool bHeadless = false;

		/** Whether it should make sure it will output an image or not (if this bool is true, it will test if JPG/PNG/etc output is enabled, if none are, it will enable PNG output). */
		bool bForceOutputImage = false;

		/** Whether it should make sure it will only output in a single format (if this bool is true, if for example JPG and PNG output are enabled, one will be disabled, so that there will only be 1 output that's enabled). */
		bool bForceOnlySingleOutput = false;

		/** Whether it should use the sequence's framerate rather than any manually set framerate (if this bool is true, it will make sure bUseCustomFrameRate is set to false). */
		bool bForceUseSequenceFrameRate = false;

		/** Whether it should make sure it will output files named 0000000001, 0000000002, etc (if this bool is true, it will override the FileNameFormat to simply output the frame number, and it will add 1000000000 to that frame number to hopefully ensure that any negative frame numbers will not result in filenames starting with a minus character). */
		bool bEnsureSequentialFilenames = false;
	};
}


/**
 * This class is responsible for the movie pipeline part of the rendering of the given render grid job.
 */
UCLASS()
class RENDERGRID_API URenderGridMoviePipelineRenderJob : public UObject
{
	GENERATED_BODY()

public:
	/** Creates a new render job, it won't be started right away. */
	static URenderGridMoviePipelineRenderJob* Create(URenderGridQueue* Queue, URenderGridJob* Job, const UE::RenderGrid::FRenderGridQueueCreateArgs& Args);

	/** The destructor, cleans up the TPromise (if it's set). */
	virtual void BeginDestroy() override;

	/** Starts this render job. */
	TSharedFuture<void> Execute();

	/** Cancels this render job. Relies on the internal movie pipeline implementation of job canceling on whether this will do anything or not. */
	void Cancel();

	/** Retrieves the rendering status of the given render grid job. */
	FString GetStatus() const;

	/** Retrieves the "Engine Warm Up Count" value from the AntiAliasingSettings from the render preset that this render grid job uses. */
	int32 GetEngineWarmUpCount() const;

public:
	/** Returns true if this render job was canceled (which for example can be caused by calling Cancel(), or by closing the render popup). */
	bool IsCanceled() const { return bCanceled; }

private:
	void ComputePlaybackContext(bool& bOutAllowBinding);
	void ExecuteFinished(UMoviePipelineExecutorBase* InPipelineExecutor, const bool bSuccess);

protected:
	/** The render grid job that will be rendered. */
	UPROPERTY(Transient)
	TObjectPtr<URenderGridJob> RenderGridJob;

	/** The render grid that the render grid job (that will be rendered) belongs to. */
	UPROPERTY(Transient)
	TObjectPtr<URenderGrid> RenderGrid;

	/** The movie pipeline queue. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineQueue> PipelineQueue;

	/** The movie pipeline executor. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineExecutorBase> PipelineExecutor;

	/** The movie pipeline executor job of the given render grid job. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineExecutorJob> PipelineExecutorJob;

	/** The TPromise of the rendering process. */
	TSharedPtr<TPromise<void>> Promise;

	/** The TFuture of the rendering process. */
	TSharedFuture<void> PromiseFuture;

	/** The rendering status of the given render grid job. */
	UPROPERTY(Transient)
	FString Status;

	/** Whether the entry can execute, or whether it should just skip execution. */
	UPROPERTY(Transient)
	bool bCanExecute;

	/** Whether the entry was canceled (like by calling Cancel(), or by closing the render popup). */
	UPROPERTY(Transient)
	bool bCanceled;
};


/**
 * This class is responsible for rendering the given render grid jobs.
 */
UCLASS()
class RENDERGRID_API URenderGridQueue : public UObject
{
	GENERATED_BODY()

public:
	/** Creates a new render queue, it won't be started right away. */
	static URenderGridQueue* Create(const UE::RenderGrid::FRenderGridQueueCreateArgs& Args);

	/** Starts this render queue. */
	void Execute();

public:
	/** Queues the given job. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Queue", Meta=(Keywords="render append"))
	void AddJob(URenderGridJob* Job);

	/** Pauses the queue. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Queue", Meta=(Keywords="wait"))
	void Pause();

	/** Resumes the queue. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Queue", Meta=(Keywords="unwait unpause"))
	void Resume();

	/** Cancels the current and the remaining queued jobs. Relies on the internal movie pipeline implementation of job canceling on whether this will stop the current render grid job from rendering or not. Will always prevent new render grid jobs from rendering. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Queue", Meta=(Keywords="stop quit exit kill terminate end"))
	void Cancel();

	/** Returns true if this queue has been canceled. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Queue", Meta=(Keywords="stopped quited exited killed terminated ended"))
	bool IsCanceled() const { return bCanceled; }

	/** Retrieves the rendering status of the given render grid job. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Queue", Meta=(Keywords="render progress"))
	FString GetJobStatus(URenderGridJob* Job) const;

protected:
	void OnStart();
	void OnProcessJob(URenderGridJob* Job);
	void OnFinish();

protected:
	/** The queue containing the render actions. */
	UE::RenderGrid::FRenderGridQueueCreateArgs Args;

	/** The queue containing the render actions. */
	TSharedPtr<UE::RenderGrid::Private::FRenderGridGenericExecutionQueue> Queue;

	/** The render grid jobs that are to be rendered, removed when one is grabbed from it and added to the execution queue. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URenderGridJob>> RemainingJobs;

	/** The render grid jobs that are to be rendered, mapped to the movie pipeline render job of each specific render grid job. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<const URenderGridJob>, TObjectPtr<URenderGridMoviePipelineRenderJob>> Entries;

	/** The render grid of the given render grid job that will be rendered. */
	UPROPERTY(Transient)
	TObjectPtr<URenderGrid> RenderGrid;

	/** Whether the remaining render grid jobs should be prevented from rendering. */
	UPROPERTY(Transient)
	bool bCanceled;

	/** The property values that have been overwritten by the currently applied render grid job property values. */
	UPROPERTY(Transient)
	FRenderGridManagerPreviousPropValues PreviousProps;

	/** The engine framerate settings values that have been overwritten by the currently applied engine framerate settings values. */
	UPROPERTY(Transient)
	FRenderGridPreviousEngineFpsSettings PreviousFrameLimitSettings;

public:
	/** A delegate for when the queue is about to start. */
	UE::RenderGrid::FOnRenderGridQueueStarted& OnExecuteStarted() { return OnExecuteStartedDelegate; }

	/** A delegate for when the queue has finished. */
	UE::RenderGrid::FOnRenderGridQueueFinished& OnExecuteFinished() { return OnExecuteFinishedDelegate; }

private:
	UE::RenderGrid::FOnRenderGridQueueStarted OnExecuteStartedDelegate;
	UE::RenderGrid::FOnRenderGridQueueFinished OnExecuteFinishedDelegate;
};
