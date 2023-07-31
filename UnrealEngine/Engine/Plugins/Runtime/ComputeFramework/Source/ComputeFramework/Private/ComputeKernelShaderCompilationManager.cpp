// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernelShaderCompilationManager.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ComputeFramework/ComputeKernelShared.h"
#include "RHIShaderFormatDefinitions.inl"
#include "ShaderCompiler.h"

#if WITH_EDITOR
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif


DEFINE_LOG_CATEGORY_STATIC(LogComputeKernelShaderCompiler, All, All);

static int32 GShowComputeKernelShaderWarnings = 0;
static FAutoConsoleVariableRef CVarShowComputeKernelShaderWarnings(
	TEXT("ComputeKernel.ShowShaderCompilerWarnings"),
	GShowComputeKernelShaderWarnings,
	TEXT("When set to 1, will display all warnings from ComputeKernel shader compiles.")
	);


#if WITH_EDITOR
FComputeKernelShaderCompilationManager GComputeKernelShaderCompilationManager;

void FComputeKernelShaderCompilationManager::Tick(float DeltaSeconds)
{
#if WITH_EDITOR
	RunCompileJobs();
	ProcessAsyncResults();
#endif
}

FComputeKernelShaderCompilationManager::FComputeKernelShaderCompilationManager()
{
	
}

FComputeKernelShaderCompilationManager::~FComputeKernelShaderCompilationManager()
{
	for (FComputeKernelShaderCompileWorkerInfo* Info : WorkerInfos)
	{
		delete Info;
	}

	WorkerInfos.Empty();
}

void FComputeKernelShaderCompilationManager::RunCompileJobs()
{
#if WITH_EDITOR
	check(IsInGameThread());

	InitWorkerInfo();

	int32 NumActiveThreads = 0;

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FComputeKernelShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// If this worker doesn't have any queued jobs, look for more in the input queue
		if (CurrentWorkerInfo.QueuedJobs.Num() == 0)
		{
			check(!CurrentWorkerInfo.bComplete);

			if (JobQueue.Num() > 0)
			{
				bool bAddedLowLatencyTask = false;
				int32 JobIndex = 0;

				// Try to grab up to MaxShaderJobBatchSize jobs
				// Don't put more than one low latency task into a batch
				for (; JobIndex < JobQueue.Num(); JobIndex++)
				{
					CurrentWorkerInfo.QueuedJobs.Add(JobQueue[JobIndex]);
				}

				// Update the worker state as having new tasks that need to be issued					
				// don't reset worker app ID, because the shadercompilerworkers don't shutdown immediately after finishing a single job queue.
				CurrentWorkerInfo.bIssuedTasksToWorker = true;
				CurrentWorkerInfo.bLaunchedWorker = true;
				CurrentWorkerInfo.StartTime = FPlatformTime::Seconds();
				JobQueue.RemoveAt(0, JobIndex);
			}
		}

		if (CurrentWorkerInfo.bIssuedTasksToWorker && CurrentWorkerInfo.bLaunchedWorker)
		{
			NumActiveThreads++;
		}

		if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				FShaderCompileJob& CurrentJob = static_cast<FShaderCompileJob&>(*CurrentWorkerInfo.QueuedJobs[JobIndex].GetReference());

				check(!CurrentJob.bFinalized);
				CurrentJob.bFinalized = true;

				static ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

				const FName Format = LegacyShaderPlatformToShaderFormat(EShaderPlatform(CurrentJob.Input.Target.Platform));
				const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);

				if (!Compiler)
				{
					UE_LOG(LogComputeKernelShaderCompiler, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Format.ToString());
				}
				CA_ASSUME(Compiler != nullptr);

				UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("Compile Job processing... %s"), *CurrentJob.Input.DebugGroupName);

				CurrentJob.Input.DumpDebugInfoRootPath = GShaderCompilingManager->GetAbsoluteShaderDebugInfoDirectory() / CurrentJob.Input.ShaderPlatformName.ToString();
				FPaths::NormalizeDirectoryName(CurrentJob.Input.DumpDebugInfoRootPath);
				const FShaderCompilingManager::EDumpShaderDebugInfo DumpShaderDebugInfo = GShaderCompilingManager->GetDumpShaderDebugInfo();
				CurrentJob.Input.DebugExtension.Empty();
				if (DumpShaderDebugInfo == FShaderCompilingManager::EDumpShaderDebugInfo::Always)
				{
					CurrentJob.Input.DumpDebugInfoRootPath = GShaderCompilingManager->CreateShaderDebugInfoPath(CurrentJob.Input);
				}

				if (IsValidRef(CurrentJob.Input.SharedEnvironment))
				{
					// Merge the shared environment into the per-shader environment before calling into the compile function
					// Normally this happens in the worker
					CurrentJob.Input.Environment.Merge(*CurrentJob.Input.SharedEnvironment);
				}

				// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
				Compiler->CompileShader(Format, CurrentJob.Input, CurrentJob.Output, FString(FPlatformProcess::ShaderDir()));

				CurrentJob.bSucceeded = CurrentJob.Output.bSucceeded;

				// Recompile the shader to dump debug info if desired
				if (GShaderCompilingManager->ShouldRecompileToDumpShaderDebugInfo(CurrentJob))
				{
					CurrentJob.Input.DumpDebugInfoPath = GShaderCompilingManager->CreateShaderDebugInfoPath(CurrentJob.Input);
					CurrentJob.Output = FShaderCompilerOutput();
					Compiler->CompileShader(Format, CurrentJob.Input, CurrentJob.Output, FString(FPlatformProcess::ShaderDir()));
				}

				if (CurrentJob.Output.bSucceeded)
				{
					// Generate a hash of the output and cache it
					// The shader processing this output will use it to search for existing FShaderResources
					CurrentJob.Output.GenerateOutputHash();
					UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("GPU shader compile succeeded. Id %d"), CurrentJob.Id);
				}
				else
				{
					UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("ERROR: GPU shader compile failed! Id %d"), CurrentJob.Id);
				}

				CurrentWorkerInfo.bComplete = true;
			}
		}
	}

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FComputeKernelShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];
		if (CurrentWorkerInfo.bComplete)
		{
			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				FComputeKernelShaderMapCompileResults& ShaderMapResults = ComputeKernelShaderMapJobs.FindChecked(CurrentWorkerInfo.QueuedJobs[JobIndex]->Id);
				ShaderMapResults.FinishedJobs.Add(CurrentWorkerInfo.QueuedJobs[JobIndex]);
				ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && CurrentWorkerInfo.QueuedJobs[JobIndex]->bSucceeded;
			}
		}

		CurrentWorkerInfo.bComplete = false;
		CurrentWorkerInfo.QueuedJobs.Empty();
	}
#endif
}

void FComputeKernelShaderCompilationManager::InitWorkerInfo()
{
	if (WorkerInfos.Num() == 0) // Check to see if it has been initialized or not
	{
		// Ew. Should we just use FShaderCompilingManager's workers instead? Is that safe?
		const int32 NumVirtualCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
		const uint32 NumComputeKernelShaderCompilingThreads = FMath::Min(NumVirtualCores - 1, 4);

		for (uint32 WorkerIndex = 0; WorkerIndex < NumComputeKernelShaderCompilingThreads; WorkerIndex++)
		{
			WorkerInfos.Add(new FComputeKernelShaderCompileWorkerInfo());
		}
	}	
}

void FComputeKernelShaderCompilationManager::AddJobs(TArray<FShaderCommonCompileJobPtr> InNewJobs)
{
#if WITH_EDITOR
	for (auto& Job : InNewJobs)
	{
		FComputeKernelShaderMapCompileResults& ShaderMapInfo = ComputeKernelShaderMapJobs.FindOrAdd(Job->Id);
		//@todo : Apply shader map isn't used for now with this compile manager. Should be merged to have a generic shader compiler
		ShaderMapInfo.NumJobsQueued++;
	}

	JobQueue.Append(InNewJobs);
#endif
}


void FComputeKernelShaderCompilationManager::ProcessAsyncResults()
{
#if WITH_EDITOR
	int32 NumCompilingComputeKernelShaderMaps = 0;
	TArray<int32> ShaderMapsToRemove;

	// Get all ComputeKernel shader maps to finalize
	//
	for (TMap<int32, FComputeKernelShaderMapCompileResults>::TIterator It(ComputeKernelShaderMapJobs); It; ++It)
	{
		const FComputeKernelShaderMapCompileResults& Results = It.Value();

		if (Results.FinishedJobs.Num() == Results.NumJobsQueued)
		{
			ShaderMapsToRemove.Add(It.Key());
			PendingFinalizeComputeKernelShaderMaps.Add(It.Key(), FComputeKernelShaderMapFinalizeResults(Results));
		}
	}

	for (int32 RemoveIndex = 0; RemoveIndex < ShaderMapsToRemove.Num(); RemoveIndex++)
	{
		ComputeKernelShaderMapJobs.Remove(ShaderMapsToRemove[RemoveIndex]);
	}

	NumCompilingComputeKernelShaderMaps = ComputeKernelShaderMapJobs.Num();

	if (PendingFinalizeComputeKernelShaderMaps.Num() > 0)
	{
		ProcessCompiledComputeKernelShaderMaps(PendingFinalizeComputeKernelShaderMaps, 0.1f);
	}
#endif
}


void FComputeKernelShaderCompilationManager::ProcessCompiledComputeKernelShaderMaps(
	TMap<int32, FComputeKernelShaderMapFinalizeResults>& CompiledShaderMaps,
	float TimeBudget)
{
#if WITH_EDITOR
	// Keeps shader maps alive as they are passed from the shader compiler and applied to the owning kernel
	TArray<TRefCountPtr<FComputeKernelShaderMap> > LocalShaderMapReferences;
	TMap<FComputeKernelResource*, FComputeKernelShaderMap*> KernelsToUpdate;

	// Process compiled shader maps in FIFO order, in case a shader map has been enqueued multiple times,
	// Which can happen if a kernel is edited while a background compile is going on
	for (TMap<int32, FComputeKernelShaderMapFinalizeResults>::TIterator ProcessIt(CompiledShaderMaps); ProcessIt; ++ProcessIt)
	{
		TRefCountPtr<FComputeKernelShaderMap> ShaderMap = nullptr;
		TArray<FComputeKernelResource*>* Kernels = nullptr;

		for (TMap<TRefCountPtr<FComputeKernelShaderMap>, TArray<FComputeKernelResource*> >::TIterator ShaderMapIt(FComputeKernelShaderMap::GetInFlightShaderMaps()); ShaderMapIt; ++ShaderMapIt)
		{
			if (ShaderMapIt.Key()->GetCompilingId() == ProcessIt.Key())
			{
				ShaderMap = ShaderMapIt.Key();
				Kernels = &ShaderMapIt.Value();
				break;
			}
		}

		if (ShaderMap && Kernels)
		{
			TArray<FString> Errors;
			FComputeKernelShaderMapFinalizeResults& CompileResults = ProcessIt.Value();
			TArray<FShaderCommonCompileJobPtr>& ResultArray = CompileResults.FinishedJobs;

			// Make a copy of the array as this entry of FComputeKernelShaderMap::ShaderMapsBeingCompiled will be removed below
			TArray<FComputeKernelResource*> KernelsArray = *Kernels;
			bool bSuccess = true;

			for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
			{
				FShaderCompileJob& CurrentJob = static_cast<FShaderCompileJob&>(*ResultArray[JobIndex].GetReference());
				bSuccess = bSuccess && CurrentJob.bSucceeded;

				if (bSuccess)
				{
					check(CurrentJob.Output.ShaderCode.GetShaderCodeSize() > 0);
				}

				if (GShowComputeKernelShaderWarnings || !CurrentJob.bSucceeded)
				{
					for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
					{
						Errors.AddUnique(CurrentJob.Output.Errors[ErrorIndex].GetErrorString());
					}

					if (CurrentJob.Output.Errors.Num())
					{
						UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("There were errors for job \"%s\""), *CurrentJob.Input.DebugGroupName)
						for (const FShaderCompilerError& Error : CurrentJob.Output.Errors)
						{
							UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("Error: %s"), *Error.GetErrorString())
						}
					}
				}
				else
				{
					UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("There were NO errors for job \"%s\""), *CurrentJob.Input.DebugGroupName);
				}
			}

			bool bShaderMapComplete = true;

			if (bSuccess)
			{
				bShaderMapComplete = ShaderMap->ProcessCompilationResults(ResultArray, CompileResults.FinalizeJobIndex, TimeBudget);
			}

			if (bShaderMapComplete)
			{
				ShaderMap->SetCompiledSuccessfully(bSuccess);

				// Pass off the reference of the shader map to LocalShaderMapReferences
				LocalShaderMapReferences.Add(ShaderMap);
				FComputeKernelShaderMap::GetInFlightShaderMaps().Remove(ShaderMap);

				for (FComputeKernelResource* Kernel : KernelsArray)
				{
					FComputeKernelShaderMap* CompletedShaderMap = ShaderMap;

					Kernel->RemoveOutstandingCompileId(ShaderMap->GetCompilingId());

					// Only process results that still match the ID which requested a compile
					// This avoids applying shadermaps which are out of date and a newer one is in the async compiling pipeline
					if (Kernel->IsSame(CompletedShaderMap->GetShaderMapId()))
					{
						if (Errors.Num() != 0)
						{
							FString SourceCode;
							Kernel->GetHLSLSource(SourceCode);
							UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("Compile output as text:"));
							UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("==================================================================================="));
							TArray<FString> OutputByLines;
							SourceCode.ParseIntoArrayLines(OutputByLines, false);
							for (int32 i = 0; i < OutputByLines.Num(); i++)
							{
								UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("/*%04d*/\t\t%s"), i + 1, *OutputByLines[i]);
							}
							UE_LOG(LogComputeKernelShaderCompiler, Log, TEXT("==================================================================================="));
						}

						if (!bSuccess)
						{
							// Propagate error messages
							Kernel->SetCompileOutputMessages(Errors);
							KernelsToUpdate.Add(Kernel, nullptr);

							for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
							{
								FString ErrorMessage = Errors[ErrorIndex];
								// Work around build machine string matching heuristics that will cause a cook to fail
								ErrorMessage.ReplaceInline(TEXT("error "), TEXT("err0r "), ESearchCase::CaseSensitive);
								UE_LOG(LogComputeKernelShaderCompiler, Warning, TEXT("	%s"), *ErrorMessage);
							}
						}
						else
						{
							// if we succeeded and our shader map is not complete this could be because the kernel was being edited quicker then the compile could be completed
							// Don't modify kernel for which the compiled shader map is no longer complete
							// This shouldn't happen since kernels are pretty much baked in the designated config file.
							if (CompletedShaderMap->IsComplete(Kernel, true))
							{
								KernelsToUpdate.Add(Kernel, CompletedShaderMap);
							}

							if (GShowComputeKernelShaderWarnings && Errors.Num() > 0)
							{
								UE_LOG(LogComputeKernelShaderCompiler, Warning, TEXT("Warnings while compiling ComputeKernel %s for platform %s:"),
									*Kernel->GetFriendlyName(),
									*LegacyShaderPlatformToShaderFormat(ShaderMap->GetShaderPlatform()).ToString());
								for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
								{
									UE_LOG(LogComputeKernelShaderCompiler, Warning, TEXT("	%s"), *Errors[ErrorIndex]);
								}

								Kernel->SetCompileOutputMessages(Errors);
							}
						}
					}
					else
					{
						if (CompletedShaderMap->IsComplete(Kernel, true))
						{
							const FString PlatformName = ShaderPlatformToShaderFormatName(ShaderMap->GetShaderPlatform()).ToString();
							if (bSuccess)
							{
								const FString SuccessMessage = FString::Printf(TEXT("%s: %s shader compilation success!"), *Kernel->GetFriendlyName(), *PlatformName);
								Kernel->NotifyCompilationFinished(SuccessMessage);
							}
							else
							{
								const FString FailMessage = FString::Printf(TEXT("%s: %s shader compilation failed."), *Kernel->GetFriendlyName(), *PlatformName);
								Kernel->NotifyCompilationFinished(FailMessage);
							}
						}
					}
				}

				// Cleanup shader jobs and compile tracking structures
				ResultArray.Empty();
				CompiledShaderMaps.Remove(ShaderMap->GetCompilingId());
			}

			if (TimeBudget < 0)
			{
				break;
			}
		}
	}

	if (KernelsToUpdate.Num() > 0)
	{
		for (TMap<FComputeKernelResource*, FComputeKernelShaderMap*>::TConstIterator It(KernelsToUpdate); It; ++It)
		{
			FComputeKernelResource* Kernel = It.Key();
			FComputeKernelShaderMap* ShaderMap = It.Value();

			Kernel->SetGameThreadShaderMap(It.Value());

			ENQUEUE_RENDER_COMMAND(FSetShaderMapOnComputeKernel)(
				[Kernel, ShaderMap](FRHICommandListImmediate& RHICmdList)
				{
					Kernel->SetRenderingThreadShaderMap(ShaderMap);
				});

			if (ShaderMap && ShaderMap->CompiledSuccessfully())
			{
				const FString SuccessMessage = FString::Printf(TEXT("%s: %s shader compilation success!"), 
					*Kernel->GetFriendlyName(), 
					*ShaderPlatformToShaderFormatName(ShaderMap->GetShaderPlatform()).ToString());
				Kernel->NotifyCompilationFinished(SuccessMessage);
			}
			else
			{
				const FString FailMessage = FString::Printf(TEXT("%s: Shader compilation failed."), *Kernel->GetFriendlyName());
				Kernel->NotifyCompilationFinished(FailMessage);
			}
		}
	}
#endif
}


void FComputeKernelShaderCompilationManager::FinishCompilation(const TCHAR* InKernelName, const TArray<int32>& ShaderMapIdsToFinishCompiling)
{
#if WITH_EDITOR
	check(!FPlatformProperties::RequiresCookedData());

	RunCompileJobs();	// since we don't async compile through another process, this will run all oustanding jobs
	ProcessAsyncResults();	// grab compiled shader maps and assign them to their resources

	check(ComputeKernelShaderMapJobs.Num() == 0);
#endif
}
#endif
