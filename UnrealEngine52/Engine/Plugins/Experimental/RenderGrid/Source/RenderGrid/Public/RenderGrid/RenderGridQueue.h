// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGridManager.h"
#include "RenderGridUtils.h"
#include "Tickable.h"
#include "Containers/Queue.h"
#include "UObject/ObjectPtr.h"
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
		TStrongObjectPtr<URenderGrid> RenderGrid = nullptr;

		/** The specific render grid jobs that will be rendered. */
		TArray<TStrongObjectPtr<URenderGridJob>> RenderGridJobs;

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

	/** Returns true if this render job can render (otherwise it will be skipped). */
	bool CanExecute() const { return bCanExecute; }

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
UCLASS(BlueprintType)
class RENDERGRID_API URenderGridQueue : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Returns true if it's currently executing any rendering queues. */
	static bool IsExecutingAny();

	DECLARE_MULTICAST_DELEGATE(FOnExecutionQueueChanged);
	/** The event that will be fired when the currently executing rendering queue changes (like for example when the previous one has completed its execution). */
	static FOnExecutionQueueChanged& OnExecutionQueueChanged() { return OnExecutionQueueChangedDelegate; }

	/** Call this function to make it so that the editor will be closed when all rendering queues finish execution. This function has to be only called once. */
	UFUNCTION(BlueprintCallable, Category="Render Grid Queue", Meta=(Keywords="execution execute finish close stop end done quit"))
	static void CloseEditorOnCompletion();

private:
	static void RequestAppExitIfSetToExitOnCompletion();

private:
	/** Returns the currently executing queue, or NULL if there isn't any currently executing. */
	static URenderGridQueue* GetCurrentlyExecutingQueue();

	/** Adds the given queue. Will automatically execute it if it's the only one currently. */
	static void AddExecutingQueue(URenderGridQueue* Queue);

	/** Executes the next queue, if any. Nothing will happen until the next tick. */
	static void DoNextExecutingQueue();

private:
	/** The queue of executing queues. This contains the currently rendering queue, as well as the ones that should be rendering after it. */
	static TQueue<TObjectPtr<URenderGridQueue>> ExecutingQueues;

	/** The delegate for when data in the ExecutingQueues has changed. */
	static FOnExecutionQueueChanged OnExecutionQueueChangedDelegate;

	/** Whether the editor should be closed when all rendering queues finish their execution. */
	static bool bExitOnCompletion;

public:
	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickable() const override { return true; }
	virtual bool IsAllowedToTick() const override { return true; }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderGridQueue, STATGROUP_Tickables);
	}
	//~ End FTickableGameObject interface

	/** Creates a new render queue, it won't be started right away. */
	static URenderGridQueue* Create(const UE::RenderGrid::FRenderGridQueueCreateArgs& Args);

	/** Starts this render queue. */
	void Execute();

public:
	/** Queues the given job. */
	UFUNCTION(BlueprintCallable, Category="Render Grid Queue", Meta=(Keywords="render append"))
	void AddJob(URenderGridJob* Job);

	/** Pauses the queue. */
	UFUNCTION(BlueprintCallable, Category="Render Grid Queue", Meta=(Keywords="wait"))
	void Pause();

	/** Resumes the queue. */
	UFUNCTION(BlueprintCallable, Category="Render Grid Queue", Meta=(Keywords="unwait unpause"))
	void Resume();

	/** Cancels the current and the remaining queued jobs. Relies on the internal movie pipeline implementation of job canceling on whether this will stop the current render grid job from rendering or not. Will always prevent new render grid jobs from rendering. */
	UFUNCTION(BlueprintCallable, Category="Render Grid Queue", Meta=(Keywords="stop quit exit kill terminate end"))
	void Cancel();

	/** Returns true if this queue has been started. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="stopped quited exited killed terminated ended succeeded completed finished canceled cancelled"))
	bool IsStarted() const { return bStarted; }

	/** Returns true if this queue is currently paused. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="stopped quited exited killed terminated ended succeeded completed finished canceled cancelled"))
	bool IsPaused() const { return (bPaused && !bCanceled && !bFinished); }

	/** Returns true if this queue has been canceled. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="stopped quited exited killed terminated ended succeeded completed finished canceled cancelled"))
	bool IsCanceled() const { return bCanceled; }

	/** Returns true if this queue has been canceled. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="stopped quited exited killed terminated ended succeeded completed finished canceled cancelled"))
	bool IsFinished() const { return bFinished; }

	/** Returns true if this queue is the one that's currently rendering, returns false if it hasn't started yet, or if it's waiting in the queue, or if it has finished. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="stopped quited exited killed terminated ended succeeded completed finished canceled cancelled"))
	bool IsCurrentlyRendering() const { return (GetCurrentlyExecutingQueue() == this); }

	/** Retrieves the rendering status of the given render grid job. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering obtain text"))
	URenderGrid* GetRenderGrid() const { return RenderGrid; }

	/** Retrieves the rendering status of the given render grid job. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain text"))
	FString GetJobStatus(URenderGridJob* Job) const;

	/** Returns all the jobs that have been and will be rendered. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain"))
	TArray<URenderGridJob*> GetJobs() const;

	/** Returns the number of jobs that have been and will be rendered. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain number amount"))
	int32 GetJobsCount() const;

	/** Returns the number of jobs that have finished rendering. Basically just returns [Get Jobs Count] minus [Get Jobs Remaining Count]. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain number amount finished"))
	int32 GetJobsCompletedCount() const;

	/** Returns the percentage of jobs finished, this includes the progression of the job that is currently rendering. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain number amount finished"))
	float GetStatusPercentage() const;

	/** Returns the number of jobs that are still left to render, includes the job that is currently rendering. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain number amount finished"))
	int32 GetJobsRemainingCount() const;

	/** Returns the status of the rendering process. */
	UFUNCTION(BlueprintPure, Category="Render Grid Queue", Meta=(Keywords="rendering progression obtain text"))
	FString GetStatus() const;

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
	TMap<TObjectPtr<URenderGridJob>, TObjectPtr<URenderGridMoviePipelineRenderJob>> Entries;

	/** The render grid of the given render grid job that will be rendered. */
	UPROPERTY(Transient)
	TObjectPtr<URenderGrid> RenderGrid;

	/** Whether the queue has been started yet. */
	UPROPERTY(Transient)
	bool bStarted;

	/** Whether the queue has been paused. */
	UPROPERTY(Transient)
	bool bPaused;

	/** Whether the queue has been canceled. */
	UPROPERTY(Transient)
	bool bCanceled;

	/** Whether the queue has been finished. */
	UPROPERTY(Transient)
	bool bFinished;

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
