// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeKernelShared.h"
#include "ShaderCompiler.h"

/** Information tracked for each shader compile worker process instance. */
struct FComputeKernelShaderCompileWorkerInfo
{
	/** Process handle of the worker app once launched.  Invalid handle means no process. */
	FProcHandle WorkerProcess;

	/** Tracks whether tasks have been issued to the worker. */
	bool bIssuedTasksToWorker;

	/** Whether the worker has been launched for this set of tasks. */
	bool bLaunchedWorker;

	/** Tracks whether all tasks issued to the worker have been received. */
	bool bComplete;

	/** Time at which the worker started the most recent batch of tasks. */
	double StartTime;

	/** Jobs that this worker is responsible for compiling. */
	TArray<FShaderCommonCompileJobPtr> QueuedJobs;

	FComputeKernelShaderCompileWorkerInfo() :
		bIssuedTasksToWorker(false),
		bLaunchedWorker(false),
		bComplete(false),
		StartTime(0)
	{
	}

	// warning: not virtual
	~FComputeKernelShaderCompileWorkerInfo()
	{
		if (WorkerProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(WorkerProcess);
			FPlatformProcess::CloseProc(WorkerProcess);
		}
	}
};

/** Results for a single compiled shader map. */
struct FComputeKernelShaderMapCompileResults
{
	FComputeKernelShaderMapCompileResults() :
		NumJobsQueued(0),
		bAllJobsSucceeded(true),
		bRecreateComponentRenderStateOnCompletion(false)
	{}

	int32 NumJobsQueued;
	bool bAllJobsSucceeded;
	bool bRecreateComponentRenderStateOnCompletion;
	TArray<FShaderCommonCompileJobPtr> FinishedJobs;
};


/** Results for a single compiled and finalized shader map. */
struct FComputeKernelShaderMapFinalizeResults : public FComputeKernelShaderMapCompileResults
{
	/** Tracks finalization progress on this shader map. */
	int32 FinalizeJobIndex;

	FComputeKernelShaderMapFinalizeResults(const FComputeKernelShaderMapCompileResults& InCompileResults) :
		FComputeKernelShaderMapCompileResults(InCompileResults),
		FinalizeJobIndex(0)
	{}
};

#if WITH_EDITOR

/** Handles finished shader compile jobs, applying of the shaders to their config asset, and some error handling. */
class FComputeKernelShaderCompilationManager
{
public:
	FComputeKernelShaderCompilationManager();
	~FComputeKernelShaderCompilationManager();

	void Tick(float DeltaSeconds);
	void AddJobs(TArray<FShaderCommonCompileJobPtr> InNewJobs);
	void ProcessAsyncResults();

	void FinishCompilation(const TCHAR* InKernelName, const TArray<int32>& ShaderMapIdsToFinishCompiling);

private:
	void ProcessCompiledComputeKernelShaderMaps(TMap<int32, FComputeKernelShaderMapFinalizeResults>& CompiledShaderMaps, float TimeBudget);
	void RunCompileJobs();

	void InitWorkerInfo();

	TArray<FShaderCommonCompileJobPtr> JobQueue;

	/** Map from shader map Id to the compile results for that map, used to gather compiled results. */
	TMap<int32, FComputeKernelShaderMapCompileResults> ComputeKernelShaderMapJobs;

	/** Map from shader map id to results being finalized.  Used to track shader finalizations over multiple frames. */
	TMap<int32, FComputeKernelShaderMapFinalizeResults> PendingFinalizeComputeKernelShaderMaps;

	TArray<struct FComputeKernelShaderCompileWorkerInfo*> WorkerInfos;
};

extern FComputeKernelShaderCompilationManager GComputeKernelShaderCompilationManager;

#endif // WITH_EDITOR
