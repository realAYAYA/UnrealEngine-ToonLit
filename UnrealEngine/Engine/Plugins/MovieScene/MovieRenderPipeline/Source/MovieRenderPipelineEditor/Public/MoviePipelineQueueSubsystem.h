// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EditorSubsystem.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineQueueSubsystem.generated.h"

UCLASS(BlueprintType)
class MOVIERENDERPIPELINEEDITOR_API UMoviePipelineQueueSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	
public:

	UMoviePipelineQueueSubsystem()
	{
		CurrentQueue = CreateDefaultSubobject<UMoviePipelineQueue>("EditorMoviePipelineQueue");
	}
	
	/** Returns the queue of Movie Pipelines that need to be rendered. */
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	UMoviePipelineQueue* GetQueue() const
	{
		return CurrentQueue;
	}

	/** Returns the active executor (if there is one). This can be used to subscribe to events on an already in-progress render. May be null. */
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	UMoviePipelineExecutorBase* GetActiveExecutor() const
	{
		return ActiveExecutor;
	}

	/** 
	* Starts processing the current queue with the supplied executor class. This starts an async process which
	* may or may not run in a separate process (or on separate machines), determined by the executor implementation.
	* The executor should report progress for jobs depending on the implementation.
	*
	* @param InExecutorType	A subclass of UMoviePipelineExecutorBase. An instance of this class is created and started.
	* @return A pointer to the instance of the class created. This instance will be kept alive by the Queue Subsystem
			  until it has finished (or been canceled). Register for progress reports and various callbacks on this instance.
	*/
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InExecutorType"), Category = "Movie Render Pipeline|Rendering")
	UMoviePipelineExecutorBase* RenderQueueWithExecutor(TSubclassOf<UMoviePipelineExecutorBase> InExecutorType);

	/**
	* Starts processing the current queue with the supplied executor. This starts an async process which
	* may or may not run in a separate process (or on separate machines), determined by the executor implementation.
	* The executor should report progress for jobs depending on the implementation.
	*
	* @param InExecutor	Instance of a subclass of UMoviePipelineExecutorBase.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline|Rendering")
	void RenderQueueWithExecutorInstance(UMoviePipelineExecutorBase* InExecutor);

	/**
	* Returns true if there is an active executor working on producing a movie. This could be actively rendering frames,
	* or working on post processing (finalizing file writes, etc.). Use GetActiveExecutor() and query it directly for
	* more information, progress updates, etc.
	*/
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline|Rendering")
	bool IsRendering() const
	{
		return ActiveExecutor ? ActiveExecutor->IsRendering() : false;
	}

private:
	/** Called when the executor is finished so that we can release it and stop reporting IsRendering() == true. */
	void OnExecutorFinished(UMoviePipelineExecutorBase* InPipelineExecutor, bool bSuccess);
	void OnSequencerContextBinding(bool& bAllowBinding);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineExecutorBase> ActiveExecutor;

	UPROPERTY(Transient, Instanced)
	TObjectPtr<UMoviePipelineQueue> CurrentQueue;
};