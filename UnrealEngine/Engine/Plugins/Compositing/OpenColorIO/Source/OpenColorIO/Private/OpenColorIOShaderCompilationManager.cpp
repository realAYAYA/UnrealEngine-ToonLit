// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOShaderCompilationManager.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OpenColorIOShared.h"
#include "ShaderCompiler.h"

#if WITH_EDITOR
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif


DEFINE_LOG_CATEGORY_STATIC(LogOpenColorIOShaderCompiler, All, All);

static int32 GShowOpenColorIOShaderWarnings = 1;
static FAutoConsoleVariableRef CVarShowOpenColorIOShaderWarnings(
	TEXT("OpenColorIO.ShowShaderCompilerWarnings"),
	GShowOpenColorIOShaderWarnings,
	TEXT("When set to 1, will display all warnings from OpenColorIO shader compiles.")
	);



OPENCOLORIO_API FOpenColorIOShaderCompilationManager GOpenColorIOShaderCompilationManager;

void FOpenColorIOShaderCompilationManager::Tick(float DeltaSeconds)
{
#if WITH_EDITOR
	ProcessAsyncResults();
#endif
}

FOpenColorIOShaderCompilationManager::FOpenColorIOShaderCompilationManager()
{
	
}

FOpenColorIOShaderCompilationManager::~FOpenColorIOShaderCompilationManager()
{
	for (FOpenColorIOShaderCompileWorkerInfo* Info : WorkerInfos)
	{
		delete Info;
	}

	WorkerInfos.Empty();
}

void FOpenColorIOShaderCompilationManager::InitWorkerInfo()
{
	if (WorkerInfos.Num() == 0) // Check to see if it has been initialized or not
	{
		// Ew. Should we just use FShaderCompilingManager's workers instead? Is that safe?
		const int32 NumVirtualCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
		const uint32 NumOpenColorIOShaderCompilingThreads = FMath::Min(NumVirtualCores - 1, 4);

		for (uint32 WorkerIndex = 0; WorkerIndex < NumOpenColorIOShaderCompilingThreads; WorkerIndex++)
		{
			WorkerInfos.Add(new FOpenColorIOShaderCompileWorkerInfo());
		}
	}	
}

OPENCOLORIO_API void FOpenColorIOShaderCompilationManager::AddJobs(TArray<FShaderCommonCompileJobPtr> InNewJobs)
{
#if WITH_EDITOR
	check(IsInGameThread());
	JobQueue.Append(InNewJobs);

	for (FShaderCommonCompileJobPtr& Job : InNewJobs)
	{
		FOpenColorIOShaderMapCompileResults& ShaderMapInfo = OpenColorIOShaderMapJobs.FindOrAdd(Job->Id);
		ShaderMapInfo.NumJobsQueued++;

		auto CurrentJob = Job->GetSingleShaderJob();

		// Fast math breaks The ExecGrid layout script because floor(x/y) returns a bad value if x == y. Yay.
		if (IsMetalPlatform((EShaderPlatform)CurrentJob->Input.Target.Platform))
		{
			CurrentJob->Input.Environment.CompilerFlags.Add(CFLAG_NoFastMath);
		}

		CurrentJob->Input.DumpDebugInfoRootPath = GShaderCompilingManager->GetAbsoluteShaderDebugInfoDirectory() / CurrentJob->Input.ShaderPlatformName.ToString();
		FPaths::NormalizeDirectoryName(CurrentJob->Input.DumpDebugInfoRootPath);
		CurrentJob->Input.DebugExtension.Empty();
		CurrentJob->Input.DumpDebugInfoPath.Empty();
		if (GShaderCompilingManager->GetDumpShaderDebugInfo() == FShaderCompilingManager::EDumpShaderDebugInfo::Always)
		{
			CurrentJob->Input.DumpDebugInfoPath = GShaderCompilingManager->CreateShaderDebugInfoPath(CurrentJob->Input);
		}
	}
	GShaderCompilingManager->SubmitJobs(InNewJobs, FString(), FString());
#endif
}


void FOpenColorIOShaderCompilationManager::ProcessAsyncResults()
{
#if WITH_EDITOR
	check(IsInGameThread());

	// Process the results from the shader compile worker
	for (int32 JobIndex = JobQueue.Num() - 1; JobIndex >= 0; JobIndex--)
	{
		auto CurrentJob = JobQueue[JobIndex]->GetSingleShaderJob();

		if (!CurrentJob->bReleased)
		{
			continue;
		}

		CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;
		if (CurrentJob->Output.bSucceeded)
		{
			UE_LOG(LogShaders, Verbose, TEXT("GPU shader compile succeeded. Id %d"), CurrentJob->Id);
		}
		else
		{
			UE_LOG(LogShaders, Warning, TEXT("GPU shader compile failed! Id: %d Name: %s"), CurrentJob->Id, *CurrentJob->Input.DebugGroupName);
		}

		FOpenColorIOShaderMapCompileResults& ShaderMapResults = OpenColorIOShaderMapJobs.FindChecked(CurrentJob->Id);
		ShaderMapResults.FinishedJobs.Add(JobQueue[JobIndex]);
		ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && CurrentJob->bSucceeded;
		JobQueue.RemoveAt(JobIndex);
	}

	// Get all OCIO shader maps to finalize
	//
	for (TMap<int32, FOpenColorIOShaderMapCompileResults>::TIterator It(OpenColorIOShaderMapJobs); It; ++It)
	{
		const FOpenColorIOShaderMapCompileResults& Results = It.Value();

		if (Results.FinishedJobs.Num() == Results.NumJobsQueued)
		{
			PendingFinalizeOpenColorIOShaderMaps.Add(It.Key(), FOpenColorIOShaderMapCompileResults(Results));
			OpenColorIOShaderMapJobs.Remove(It.Key());
		}
	}

	if (PendingFinalizeOpenColorIOShaderMaps.Num() > 0)
	{
		ProcessCompiledOpenColorIOShaderMaps(PendingFinalizeOpenColorIOShaderMaps, 10);
	}
#endif
}


void FOpenColorIOShaderCompilationManager::ProcessCompiledOpenColorIOShaderMaps(
	TMap<int32, FOpenColorIOShaderMapFinalizeResults>& CompiledShaderMaps,
	float TimeBudget)
{
#if WITH_EDITOR
	// Keeps shader maps alive as they are passed from the shader compiler and applied to the owning ColorTransform
	TArray<TRefCountPtr<FOpenColorIOShaderMap> > LocalShaderMapReferences;
	TMap<FOpenColorIOTransformResource*, FOpenColorIOShaderMap*> TransformsToUpdate;

	// Process compiled shader maps in FIFO order, in case a shader map has been enqueued multiple times,
	// Which can happen if a ColorTransform is edited while a background compile is going on
	for (TMap<int32, FOpenColorIOShaderMapFinalizeResults>::TIterator ProcessIt(CompiledShaderMaps); ProcessIt; ++ProcessIt)
	{
		TRefCountPtr<FOpenColorIOShaderMap> ShaderMap = nullptr;
		TArray<FOpenColorIOTransformResource*>* ColorTransforms = nullptr;

		for (TMap<TRefCountPtr<FOpenColorIOShaderMap>, TArray<FOpenColorIOTransformResource*> >::TIterator ShaderMapIt(FOpenColorIOShaderMap::GetInFlightShaderMaps()); ShaderMapIt; ++ShaderMapIt)
		{
			if (ShaderMapIt.Key()->GetCompilingId() == ProcessIt.Key())
			{
				ShaderMap = ShaderMapIt.Key();
				ColorTransforms = &ShaderMapIt.Value();
				break;
			}
		}

		if (ShaderMap && ColorTransforms)
		{
			TArray<FString> Errors;
			FOpenColorIOShaderMapFinalizeResults& CompileResults = ProcessIt.Value();
			auto& ResultArray = CompileResults.FinishedJobs;

			// Make a copy of the array as this entry of FOpenColorIOShaderMap::ShaderMapsBeingCompiled will be removed below
			TArray<FOpenColorIOTransformResource*> ColorTransformArray = *ColorTransforms;
			bool bSuccess = true;

			for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
			{
				FShaderCompileJob* CurrentJob = ResultArray[JobIndex]->GetSingleShaderJob();
				bSuccess = bSuccess && CurrentJob->bSucceeded;

				if (bSuccess)
				{
					check(CurrentJob->Output.ShaderCode.GetShaderCodeSize() > 0);
				}

				if (GShowOpenColorIOShaderWarnings || !CurrentJob->bSucceeded)
				{
					for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob->Output.Errors.Num(); ErrorIndex++)
					{
						Errors.AddUnique(CurrentJob->Output.Errors[ErrorIndex].GetErrorString());
					}

					if (CurrentJob->Output.Errors.Num())
					{
						UE_LOG(LogShaders, Log, TEXT("There were errors for job \"%s\""), *CurrentJob->Input.DebugGroupName)
						for (const FShaderCompilerError& Error : CurrentJob->Output.Errors)
						{
							UE_LOG(LogShaders, Log, TEXT("Error: %s"), *Error.GetErrorString())
						}
					}
				}
				else
				{
					UE_LOG(LogShaders, Log, TEXT("There were NO errors for job \"%s\""), *CurrentJob->Input.DebugGroupName);
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
				FOpenColorIOShaderMap::GetInFlightShaderMaps().Remove(ShaderMap);

				for (FOpenColorIOTransformResource* ColorTransform : ColorTransformArray)
				{
					FOpenColorIOShaderMap* CompletedShaderMap = ShaderMap;

					ColorTransform->RemoveOutstandingCompileId(ShaderMap->GetCompilingId());

					// Only process results that still match the ID which requested a compile
					// This avoids applying shadermaps which are out of date and a newer one is in the async compiling pipeline
					if (ColorTransform->IsSame(CompletedShaderMap->GetShaderMapId()))
					{
						if (Errors.Num() != 0)
						{
							FString SourceCode;
							ColorTransform->GetColorTransformHLSLSource(SourceCode);
							UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("Compile output as text:"));
							UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("==================================================================================="));
							TArray<FString> OutputByLines;
							SourceCode.ParseIntoArrayLines(OutputByLines, false);
							for (int32 i = 0; i < OutputByLines.Num(); i++)
							{
								UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("/*%04d*/\t\t%s"), i + 1, *OutputByLines[i]);
							}
							UE_LOG(LogOpenColorIOShaderCompiler, Log, TEXT("==================================================================================="));
						}

						if (!bSuccess)
						{
							// Propagate error messages
							ColorTransform->SetCompileErrors(Errors);
							TransformsToUpdate.Add(ColorTransform, nullptr);

							for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
							{
								FString ErrorMessage = Errors[ErrorIndex];
								// Work around build machine string matching heuristics that will cause a cook to fail
								ErrorMessage.ReplaceInline(TEXT("error "), TEXT("err0r "), ESearchCase::CaseSensitive);
								UE_LOG(LogOpenColorIOShaderCompiler, Warning, TEXT("	%s"), *ErrorMessage);
							}
						}
						else
						{
							// if we succeeded and our shader map is not complete this could be because the color transform was being edited quicker then the compile could be completed
							// Don't modify color transforms for which the compiled shader map is no longer complete
							// This shouldn't happen since transforms are pretty much baked in the designated config file.
							if (CompletedShaderMap->IsComplete(ColorTransform, true))
							{
								TransformsToUpdate.Add(ColorTransform, CompletedShaderMap);
							}

							if (GShowOpenColorIOShaderWarnings && Errors.Num() > 0)
							{
								UE_LOG(LogOpenColorIOShaderCompiler, Warning, TEXT("Warnings while compiling OpenColorIO ColorTransform %s for platform %s:"),
									*ColorTransform->GetFriendlyName(),
									*LegacyShaderPlatformToShaderFormat(ShaderMap->GetShaderPlatform()).ToString());
								for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
								{
									UE_LOG(LogOpenColorIOShaderCompiler, Warning, TEXT("	%s"), *Errors[ErrorIndex]);
								}
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

	if (TransformsToUpdate.Num() > 0)
	{
		for (TMap<FOpenColorIOTransformResource*, FOpenColorIOShaderMap*>::TConstIterator It(TransformsToUpdate); It; ++It)
		{
			FOpenColorIOTransformResource* ColorTransform = It.Key();
			FOpenColorIOShaderMap* ShaderMap = It.Value();

			ColorTransform->SetGameThreadShaderMap(It.Value());

			ENQUEUE_RENDER_COMMAND(FSetShaderMapOnColorTransformResources)(
				[ColorTransform, ShaderMap](FRHICommandListImmediate& RHICmdList)
				{
					ColorTransform->SetRenderingThreadShaderMap(ShaderMap);
				});
		}
	}
#endif
}


void FOpenColorIOShaderCompilationManager::FinishCompilation(const TCHAR* InTransformName, const TArray<int32>& ShaderMapIdsToFinishCompiling)
{
#if WITH_EDITOR
	check(!FPlatformProperties::RequiresCookedData());
	GShaderCompilingManager->FinishCompilation(NULL, ShaderMapIdsToFinishCompiling);
	ProcessAsyncResults();	// grab compiled shader maps and assign them to their resources
#endif
}

