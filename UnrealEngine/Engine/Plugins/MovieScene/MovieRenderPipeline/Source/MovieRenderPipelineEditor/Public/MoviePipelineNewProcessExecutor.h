// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineLinearExecutor.h"
#include "MoviePipelineNewProcessExecutor.generated.h"

/**
* This is the implementation responsible for executing the rendering of
* multiple movie pipelines on the local machine in an external process.
* This simply handles launching and managing the external processes and 
* acts as a proxy to them where possible. This internally uses the
* UMoviePipelineInProcessExecutor on the launched instances.
*/
UCLASS(Blueprintable)
class MOVIERENDERPIPELINEEDITOR_API UMoviePipelineNewProcessExecutor : public UMoviePipelineExecutorBase
{
	GENERATED_BODY()

	// UMoviePipelineExecutorBase Interface
	virtual void Execute_Implementation(UMoviePipelineQueue* InPipelineQueue) override;
	virtual bool IsRendering_Implementation() const override { return ProcessHandle.IsValid(); }

	// Canceling current job is equivalent to canceling all jobs for this executor
	virtual void CancelCurrentJob_Implementation() override { CancelAllJobs_Implementation(); }
	virtual void CancelAllJobs_Implementation() override;
	// ~UMoviePipelineExecutorBase Interface

protected:
	void CheckForProcessFinished();

protected:
	/** A handle to the currently running process (if any) for the active job. */
	FProcHandle ProcessHandle;

};