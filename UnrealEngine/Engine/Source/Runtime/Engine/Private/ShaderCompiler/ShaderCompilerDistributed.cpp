// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistributedBuildControllerInterface.h"
#include "Async/Future.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "ShaderCompiler.h"

namespace DistributedShaderCompilerVariables
{
	//TODO: Remove the XGE doublet
	int32 MinBatchSize = 50;
	FAutoConsoleVariableRef CVarXGEShaderCompileMinBatchSize(
        TEXT("r.XGEShaderCompile.MinBatchSize"),
        MinBatchSize,
        TEXT("This CVar is deprecated, please use r.ShaderCompiler.DistributedMinBatchSize"),
        ECVF_Default);

	FAutoConsoleVariableRef CVarDistributedMinBatchSize(
		TEXT("r.ShaderCompiler.DistributedMinBatchSize"),
		MinBatchSize,
		TEXT("Minimum number of shaders to compile with a distributed controller.\n")
		TEXT("Smaller number of shaders will compile locally."),
		ECVF_Default);

	static int32 GDistributedControllerTimeout = 15 * 60;
	static FAutoConsoleVariableRef CVarDistributedControllerTimeout(
		TEXT("r.ShaderCompiler.DistributedControllerTimeout"),
		GDistributedControllerTimeout,
		TEXT("Maximum number of seconds we expect to pass between getting distributed controller complete a task (this is used to detect problems with the distribution controllers).")
	);

}

bool FShaderCompileDistributedThreadRunnable_Interface::IsSupported()
{
	//TODO Handle Generic response
	return true;
}

class FDistributedShaderCompilerTask
{
public:
	TFuture<FDistributedBuildTaskResult> Future;
	TArray<FShaderCommonCompileJobPtr> ShaderJobs;
	FString InputFilePath;
	FString OutputFilePath;

	FDistributedShaderCompilerTask(TFuture<FDistributedBuildTaskResult>&& Future,TArray<FShaderCommonCompileJobPtr>&& ShaderJobs, FString&& InputFilePath, FString&& OutputFilePath)
		: Future(MoveTemp(Future))
		, ShaderJobs(MoveTemp(ShaderJobs))
		, InputFilePath(MoveTemp(InputFilePath))
		, OutputFilePath(MoveTemp(OutputFilePath))
	{}
};

/** Initialization constructor. */
FShaderCompileDistributedThreadRunnable_Interface::FShaderCompileDistributedThreadRunnable_Interface(class FShaderCompilingManager* InManager, IDistributedBuildController& InController)
	: FShaderCompileThreadRunnableBase(InManager)
	, NumDispatchedJobs(0)
	, LastTimeTaskCompleted(FPlatformTime::Seconds())
	, bIsHung(false)
	, CachedController(InController)
{
}

FShaderCompileDistributedThreadRunnable_Interface::~FShaderCompileDistributedThreadRunnable_Interface()
{
}

void FShaderCompileDistributedThreadRunnable_Interface::DispatchShaderCompileJobsBatch(TArray<FShaderCommonCompileJobPtr>& JobsToSerialize)
{
	const FString BaseFilePath = CachedController.CreateUniqueFilePath();
	FString InputFilePath = BaseFilePath + TEXT(".in");
	FString OutputFilePath = BaseFilePath + TEXT(".out");

	const FString WorkingDirectory = FPaths::GetPath(InputFilePath);

	// Serialize the jobs to the input file
	GShaderCompilerStats->RegisterJobBatch(JobsToSerialize.Num(), FShaderCompilerStats::EExecutionType::Distributed);
	FArchive* InputFileAr = IFileManager::Get().CreateFileWriter(*InputFilePath, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
	FShaderCompileUtilities::DoWriteTasks(JobsToSerialize, *InputFileAr, &CachedController, CachedController.RequiresRelativePaths());
	delete InputFileAr;

	// Kick off the job
	NumDispatchedJobs += JobsToSerialize.Num();

	FTaskCommandData TaskCommandData;
	TaskCommandData.Command = Manager->ShaderCompileWorkerName;
	TaskCommandData.WorkingDirectory = WorkingDirectory;
	TaskCommandData.DispatcherPID = Manager->ProcessId;
	TaskCommandData.InputFileName = InputFilePath;
	TaskCommandData.OutputFileName = OutputFilePath;
	TaskCommandData.ExtraCommandArgs = FString::Printf(TEXT("%s%s"), *FCommandLine::GetSubprocessCommandline(), GIsBuildMachine ? TEXT(" -buildmachine") : TEXT(""));
	TaskCommandData.Dependencies = GetDependencyFilesForJobs(JobsToSerialize);
	
	DispatchedTasks.Add(
		new FDistributedShaderCompilerTask(
			CachedController.EnqueueTask(TaskCommandData),
			MoveTemp(JobsToSerialize),
			MoveTemp(InputFilePath),
			MoveTemp(OutputFilePath)
		)
	);
}

TArray<FString> FShaderCompileDistributedThreadRunnable_Interface::GetDependencyFilesForJobs(
	TArray<FShaderCommonCompileJobPtr>& Jobs)
{
	TArray<FString> Dependencies;
	TBitArray<> ShaderPlatformMask;
	ShaderPlatformMask.Init(false, EShaderPlatform::SP_NumPlatforms);
	for (const FShaderCommonCompileJobPtr& Job : Jobs)
	{
		EShaderPlatform ShaderPlatform = EShaderPlatform::SP_PCD3D_SM5;
		const FShaderCompileJob* ShaderJob = Job->GetSingleShaderJob();
		if (ShaderJob)
		{
			ShaderPlatform = ShaderJob->Input.Target.GetPlatform();
			// Add the source shader file and its dependencies.
			AddShaderSourceFileEntry(Dependencies, ShaderJob->Input.VirtualSourceFilePath, ShaderPlatform);
		}
		else
		{
			const FShaderPipelineCompileJob* PipelineJob = Job->GetShaderPipelineJob();
			if (PipelineJob)
			{
				for (const TRefCountPtr<FShaderCompileJob>& CommonCompileJob : PipelineJob->StageJobs)
				{
					if (const FShaderCompileJob* SingleShaderJob = CommonCompileJob->GetSingleShaderJob())
					{
						ShaderPlatform = SingleShaderJob->Input.Target.GetPlatform();
						// Add the source shader file and its dependencies.
						AddShaderSourceFileEntry(Dependencies, SingleShaderJob->Input.VirtualSourceFilePath, ShaderPlatform);
					}
				}
			}
			else
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Unknown shader compilation job type."));
			}
		}
		// Add base dependencies for the platform only once.
		if (!(ShaderPlatformMask[(int)ShaderPlatform]))
		{
			ShaderPlatformMask[(int)ShaderPlatform] = true;
			TArray<FString>& ShaderPlatformCacheEntry = PlatformShaderInputFilesCache.FindOrAdd(ShaderPlatform);
			if (!ShaderPlatformCacheEntry.Num())
			{
				GetAllVirtualShaderSourcePaths(ShaderPlatformCacheEntry, ShaderPlatform);
			}
			if (Dependencies.Num())
			{
				for (const FString& Filename : ShaderPlatformCacheEntry)
				{
					Dependencies.AddUnique(Filename);
				}
			}
			else
			{
				Dependencies = ShaderPlatformCacheEntry;
			}
		}
	}

	return Dependencies;
}

int32 FShaderCompileDistributedThreadRunnable_Interface::CompilingLoop()
{
	TArray<FShaderCommonCompileJobPtr> PendingJobs;
	//if (LIKELY(!bIsHung))	// stop accepting jobs if we're hung - TODO: re-enable this after lockup detection logic is proved reliable and/or we have job resubmission in place
	{
		for (int32 PriorityIndex = MaxPriorityIndex; PriorityIndex >= MinPriorityIndex; --PriorityIndex)
		{
			// Grab as many jobs from the job queue as we can
			const EShaderCompileJobPriority Priority = (EShaderCompileJobPriority)PriorityIndex;
			const int32 MinBatchSize = (Priority == EShaderCompileJobPriority::Low) ? 1 : DistributedShaderCompilerVariables::MinBatchSize;
			const int32 NumJobs = Manager->AllJobs.GetPendingJobs(EShaderCompilerWorkerType::Distributed, Priority, MinBatchSize, INT32_MAX, PendingJobs);
			if (NumJobs > 0)
			{
				UE_LOG(LogShaderCompilers, Verbose, TEXT("Started %d 'Distributed' shader compile jobs with '%s' priority"),
					NumJobs,
					ShaderCompileJobPriorityToString((EShaderCompileJobPriority)PriorityIndex));
			}
			if (PendingJobs.Num() >= DistributedShaderCompilerVariables::MinBatchSize)
			{
				break;
			}
		}
	}

	// if we don't have any dispatched jobs, reset the completion timer
	if (DispatchedTasks.Num() == 0)
	{
		LastTimeTaskCompleted = FPlatformTime::Seconds();
	}

	if (PendingJobs.Num() > 0)
	{
		// Increase the batch size when more jobs are queued/in flight.

		// Build farm is much more prone to pool oversubscription, so make sure the jobs are submitted in batches of at least MinBatchSize
		int MinJobsPerBatch = GIsBuildMachine ? DistributedShaderCompilerVariables::MinBatchSize : 1;

		// Just to provide typical numbers: the number of total jobs is usually in tens of thousands at most, oftentimes in low thousands. Thus JobsPerBatch when calculated as a log2 rarely reaches the value of 16,
		// and that seems to be a sweet spot: lowering it does not result in faster completion, while increasing the number of jobs per batch slows it down.
		const uint32 JobsPerBatch = FMath::Max(MinJobsPerBatch, FMath::FloorToInt(FMath::LogX(2.f, PendingJobs.Num() + NumDispatchedJobs)));
		UE_LOG(LogShaderCompilers, Log, TEXT("Current jobs: %d, Batch size: %d, Num Already Dispatched: %d"), PendingJobs.Num(), JobsPerBatch, NumDispatchedJobs);


		struct FJobBatch
		{
			TArray<FShaderCommonCompileJobPtr> Jobs;
			TSet<const FShaderType*> UniquePointers;

			bool operator == (const FJobBatch& B) const
			{
				return Jobs == B.Jobs;
			}
		};


		// Different batches.
		TArray<FJobBatch> JobBatches;


		for (int32 i = 0; i < PendingJobs.Num(); i++)
		{
			// Randomize the shader compile jobs a little.
			{
				int32 PickedUpIndex = FMath::RandRange(i, PendingJobs.Num() - 1);
				if (i != PickedUpIndex)
				{
					Swap(PendingJobs[i], PendingJobs[PickedUpIndex]);
				}
			}

			// Avoid to have multiple of permutation of same global shader in same batch, to avoid pending on long shader compilation
			// of batches that tries to compile permutation of a global shader type that is giving a hard time to the shader compiler.
			const FShaderType* OptionalUniqueShaderType = nullptr;
			if (FShaderCompileJob* ShaderCompileJob = PendingJobs[i]->GetSingleShaderJob())
			{
				if (ShaderCompileJob->Key.ShaderType->GetGlobalShaderType())
				{
					OptionalUniqueShaderType = ShaderCompileJob->Key.ShaderType;
				}
			}

			// Find a batch this compile job can be packed with.
			FJobBatch* SelectedJobBatch = nullptr;
			{
				if (JobBatches.Num() == 0)
				{
					SelectedJobBatch = &JobBatches[JobBatches.Emplace()];
				}
				else if (OptionalUniqueShaderType)
				{
					for (FJobBatch& PendingJobBatch : JobBatches)
					{
						if (!PendingJobBatch.UniquePointers.Contains(OptionalUniqueShaderType))
						{
							SelectedJobBatch = &PendingJobBatch;
							break;
						}
					}

					if (!SelectedJobBatch)
					{
						SelectedJobBatch = &JobBatches[JobBatches.Emplace()];
					}
				}
				else
				{
					SelectedJobBatch = &JobBatches[0];
				}
			}

			// Assign compile job to job batch.
			{
				SelectedJobBatch->Jobs.Add(PendingJobs[i]);
				if (OptionalUniqueShaderType)
				{
					SelectedJobBatch->UniquePointers.Add(OptionalUniqueShaderType);
				}
			}

			// Kick off compile job batch.
			if (SelectedJobBatch->Jobs.Num() == JobsPerBatch)
			{
				DispatchShaderCompileJobsBatch(SelectedJobBatch->Jobs);
				JobBatches.RemoveSingleSwap(*SelectedJobBatch);
			}
		}

		// Kick off remaining compile job batches.
		for (FJobBatch& PendingJobBatch : JobBatches)
		{
			DispatchShaderCompileJobsBatch(PendingJobBatch.Jobs);
		}
	}

	for (auto Iter = DispatchedTasks.CreateIterator(); Iter; ++Iter)
	{
		bool bOutputFileReadFailed = true;

		FDistributedShaderCompilerTask* Task = *Iter;
		if (!Task->Future.IsReady())
		{
			continue;
		}

		FDistributedBuildTaskResult Result = Task->Future.Get();
		NumDispatchedJobs -= Task->ShaderJobs.Num();
		LastTimeTaskCompleted = FPlatformTime::Seconds();

		if (Result.ReturnCode != 0)
		{
			UE_LOG(LogShaderCompilers, Error, TEXT("Shader compiler returned a non-zero error code (%d)."), Result.ReturnCode);
		}

		if (Result.bCompleted)
		{
			// Check the output file exists. If it does, attempt to open it and serialize in the completed jobs.
			bool bCompileJobsSucceeded = false;

			if (IFileManager::Get().FileExists(*Task->OutputFilePath))
			{
				if (TUniquePtr<FArchive> OutputFileAr = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*Task->OutputFilePath, FILEREAD_Silent)))
				{
					bOutputFileReadFailed = false;
					if (FShaderCompileUtilities::DoReadTaskResults(Task->ShaderJobs, *OutputFileAr) == FSCWErrorCode::Success)
					{
						bCompileJobsSucceeded = true;
					}
				}
			}

			if (!bCompileJobsSucceeded)
			{
				// Reading result from XGE job failed, so recompile shaders in current job batch locally
				UE_LOG(LogShaderCompilers, Log, TEXT("Rescheduling shader compilation to run locally after distributed job failed: %s"), *Task->OutputFilePath);

				for (FShaderCommonCompileJobPtr Job : Task->ShaderJobs)
				{
					FShaderCompileUtilities::ExecuteShaderCompileJob(*Job);
				}
			}

			// Enter the critical section so we can access the input and output queues
			{
				FScopeLock Lock(&Manager->CompileQueueSection);
				for (const auto& Job : Task->ShaderJobs)
				{
					Manager->ProcessFinishedJob(Job);
				}
			}
		}
		else
		{
			// The compile job was canceled. Return the jobs to the manager's compile queue.
			Manager->AllJobs.SubmitJobs(Task->ShaderJobs);
		}

		// Delete input and output files, if they exist.
		while (!IFileManager::Get().Delete(*Task->InputFilePath, false, true, true))
		{
			FPlatformProcess::Sleep(0.01f);
		}

		if (!bOutputFileReadFailed)
		{
			while (!IFileManager::Get().Delete(*Task->OutputFilePath, false, true, true))
			{
				FPlatformProcess::Sleep(0.01f);
			}
		}

		Iter.RemoveCurrent();
		delete Task;
	}

	// Yield for a short while to stop this thread continuously polling the disk.
	FPlatformProcess::Sleep(0.01f);

	// normally we expect to have at least one task in 5 minutes, although there could be edge cases
	double TimeSinceLastCompletedTask = FPlatformTime::Seconds() - LastTimeTaskCompleted;
	if (TimeSinceLastCompletedTask >= DistributedShaderCompilerVariables::GDistributedControllerTimeout)
	{
		if (!bIsHung)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("Distributed compilation controller didn't receive a completed task in %f seconds!"), TimeSinceLastCompletedTask);
			bIsHung = true;
			// TODO: resubmit the hung jobs
		}
	}

	// Return true if there is more work to be done.
	return Manager->AllJobs.GetNumOutstandingJobs() > 0;
}

const TCHAR* FShaderCompileDistributedThreadRunnable_Interface::GetThreadName() const
{
	return TEXT("ShaderCompilingThread-Distributed");
}
