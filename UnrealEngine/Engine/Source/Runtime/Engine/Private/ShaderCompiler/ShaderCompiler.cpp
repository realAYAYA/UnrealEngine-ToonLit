// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompiler.cpp: Platform independent shader compilations.
=============================================================================*/

#include "ShaderCompiler.h"
#include "Async/ParallelFor.h"
#include "ClearReplacementShaders.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/StringConv.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "DistributedBuildControllerInterface.h"
#include "EditorSupportDelegates.h"
#include "Engine/RendererSettings.h"
#include "EngineModule.h"
#include "GBufferInfo.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GlobalShader.h"
#include "HAL/ExceptionHandling.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshMaterialShaderType.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "ObjectCacheContext.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "ProfilingDebugging/StallDetector.h"
#include "RenderUtils.h"
#include "RendererInterface.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "Serialization/MemoryHasher.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/NameAsStringProxyArchive.h"
#include "Shader.h"
#include "ShaderCodeLibrary.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadata.h"
#include "StaticBoundShaderState.h"
#include "StaticBoundShaderState.h"
#include "StereoRenderUtils.h"
#include "UObject/DevObjectVersion.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEngine.h"

#if WITH_EDITOR
#include "Misc/DelayedAutoRegister.h"
#include "Compression/OodleDataCompression.h"
#include "UObject/ArchiveCookContext.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataRequestOwner.h"

#include "TextureCompiler.h"
#include "Rendering/StaticLightingSystemInterface.h"
#endif
#if WITH_ODSC
#include "ODSC/ODSCManager.h"
#endif
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "ShaderCompiler"

DEFINE_LOG_CATEGORY(LogShaderCompilers);

// Switch to Verbose after initial testing
#define UE_SHADERCACHE_LOG_LEVEL		VeryVerbose

// whether to parallelize writing/reading task files
#define UE_SHADERCOMPILER_FIFO_JOB_EXECUTION  1

int32 GShaderCompilerJobCache = 1;
static FAutoConsoleVariableRef CVarShaderCompilerJobCache(
	TEXT("r.ShaderCompiler.JobCache"),
	GShaderCompilerJobCache,
	TEXT("if != 0, shader compiler cache (based on the unpreprocessed input hash) will be disabled. By default, it is enabled."),
	ECVF_Default
);

int32 GShaderCompilerMaxJobCacheMemoryMB = 16LL * 1024LL;
static FAutoConsoleVariableRef CVarShaderCompilerMaxJobCacheMemoryMB(
	TEXT("r.ShaderCompiler.MaxJobCacheMemoryMB"),
	GShaderCompilerMaxJobCacheMemoryMB,
	TEXT("if != 0, shader compiler cache will be limited to this many megabytes (16GB by default). If 0, the usage will be unlimited. Minimum of this or r.ShaderCompiler.MaxJobCacheMemoryPercent applies."),
	ECVF_Default
);

int32 GShaderCompilerMaxJobCacheMemoryPercent = 5;
static FAutoConsoleVariableRef CVarShaderCompilerMaxJobCacheMemoryPercent(
	TEXT("r.ShaderCompiler.MaxJobCacheMemoryPercent"),
	GShaderCompilerMaxJobCacheMemoryPercent,
	TEXT("if != 0, shader compiler cache will be limited to this percentage of available physical RAM (5% by default). If 0, the usage will be unlimited. Minimum of this or r.ShaderCompiler.MaxJobCacheMemoryMB applies."),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarJobCacheDDC(
	TEXT("r.ShaderCompiler.JobCacheDDC"),
	true,
	TEXT("Skips compilation of all shaders on Material and Material Instance PostLoad and relies on on-demand shader compilation to compile what is needed."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<bool> CVarJobCacheDDCPolicy(
	TEXT("r.ShaderCompiler.JobCacheDDCEnableRemotePolicy"),
	false,
	TEXT("Whether to cache shaders in the job cache to your local machine or remotely to the network.\n"),
	ECVF_ReadOnly);

static bool IsShaderJobCacheDDCRemotePolicyEnabled()
{
	return CVarJobCacheDDCPolicy.GetValueOnAnyThread();
}

bool IsShaderJobCacheDDCEnabled()
{
	// For now we only support the editor and not commandlets like the cooker.
	if (GIsEditor && !IsRunningCommandlet())
	{
		// job cache itself must be enabled first
		return GShaderCompilerJobCache && CVarJobCacheDDC.GetValueOnAnyThread();
	}

	return false;
}

int32 GShaderCompilerDumpCompileJobInputs = 0;
static FAutoConsoleVariableRef CVarShaderCompilerDumpCompileJobInputs(
	TEXT("r.ShaderCompiler.DumpCompileJobInputs"),
	GShaderCompilerDumpCompileJobInputs,
	TEXT("if != 0, unpreprocessed input of the shader compiler jobs will be dumped into the debug directory for closer inspection. This is a debugging feature which is disabled by default."),
	ECVF_Default
);

int32 GShaderCompilerCacheStatsPrintoutInterval = 180;
static FAutoConsoleVariableRef CVarShaderCompilerCacheStatsPrintoutInterval(
	TEXT("r.ShaderCompiler.CacheStatsPrintoutInterval"),
	GShaderCompilerCacheStatsPrintoutInterval,
	TEXT("Minimum interval (in seconds) between printing out debugging stats (by default, no closer than each 3 minutes)."),
	ECVF_Default
);

int32 GShaderCompilerAllowDistributedCompilation = 1;
static FAutoConsoleVariableRef CVarShaderCompilerAllowDistributedCompilation(
	TEXT("r.ShaderCompiler.AllowDistributedCompilation"),
	GShaderCompilerAllowDistributedCompilation,
	TEXT("If 0, only local (spawned by the engine) ShaderCompileWorkers will be used. If 1, SCWs will be distributed using one of several possible backends (XGE, FASTBuild, SN-DBS)"),
	ECVF_Default
);

/** Maximum number of preprocessed shaders to dump to the log on a crash. Replace with CVar if needed. */
int32 GMaxNumDumpedShaderSources = 1;
static FAutoConsoleVariableRef CVarShaderCompilerMaxDumpedShaderSources(
	TEXT("r.ShaderCompiler.MaxDumpedShaderSources"),
	GMaxNumDumpedShaderSources,
	TEXT("Maximum number of preprocessed shader sources to dump to the log on shader compile errors. By default 1."),
	ECVF_Default
);

int32 GSShaderCheckLevel = 1;
static FAutoConsoleVariableRef CVarGSShaderCheckLevel(
	TEXT("r.Shaders.CheckLevel"),
	GSShaderCheckLevel,
	TEXT("0 => DO_CHECK=0, DO_GUARD_SLOW=0, 1 => DO_CHECK=1, DO_GUARD_SLOW=0, 2 => DO_CHECK=1, DO_GUARD_SLOW=1 for all shaders."),
	ECVF_Default
);

float GShaderCompilerTooLongIOThresholdSeconds = 0.3;
static FAutoConsoleVariableRef CVarShaderCompilerTooLongIOThresholdSeconds(
	TEXT("r.ShaderCompiler.TooLongIOThresholdSeconds"),
	GShaderCompilerTooLongIOThresholdSeconds,
	TEXT("By default, task files for SCW will be read/written sequentially, but if we ever spend more than this time (0.3s by default) doing that, we'll switch to parallel.") \
	TEXT("We don't default to parallel writes as it increases the CPU overhead from the shader compiler."),
	ECVF_Default
);

static TAutoConsoleVariable<bool> CVarShaderCompilerDumpDDCKeys(
	TEXT("r.ShaderCompiler.DumpDDCKeys"),
	false,
	TEXT("if != 0, DDC keys for each shadermap will be dumped into project's Saved directory (ShaderDDCKeys subdirectory)"),
	ECVF_Default
);

#if WITH_EDITOR
static FDelayedAutoRegisterHelper GKickOffShaderAutoGen(EDelayedRegisterRunPhase::ShaderTypesReady, []
{
	FShaderCompileUtilities::GenerateBrdfHeaders(GMaxRHIShaderPlatform);
});

static FDelayedAutoRegisterHelper GKickOffShaderAutoGenForPlatforms(EDelayedRegisterRunPhase::DeviceProfileManagerReady, []
{
	// also do this for all active target platforms (e.g. when cooking)
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		for (int32 Index = 0; Index < Platforms.Num(); ++Index)
		{
			TArray<FName> DesiredShaderFormats;
			checkf(Platforms[Index], TEXT("Null platform on the list of active platforms!"));
			Platforms[Index]->GetAllTargetedShaderFormats(DesiredShaderFormats);

			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); ++FormatIndex)
			{
				FShaderCompileUtilities::GenerateBrdfHeaders(DesiredShaderFormats[FormatIndex]);
			}
		}
	}

	// also do this for the editor mobile preview
	EShaderPlatform MobilePreviewShaderPlatform = GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1];
	if (MobilePreviewShaderPlatform != SP_NumPlatforms)
	{
		FShaderCompileUtilities::GenerateBrdfHeaders(MobilePreviewShaderPlatform);
	}
});
#endif

/** Helper functions for logging more debug info */
namespace ShaderCompiler
{
	FString GetTargetPlatformName(const ITargetPlatform* TargetPlatform)
	{
		if (TargetPlatform)
		{
			return TargetPlatform->PlatformName();
		}

		return TEXT("(current)");
	}
}

/** Storage for the global shadar map(s) that have been replaced by new one(s), which aren't yet compiled.
 * 
 *	Sometimes a mesh drawing command references a pointer to global SM's memory. To nix these MDCs when we're replacing a global SM, we would just recreate the render state for all the components, but
 *	we may need to access a global shader during such an update, creating a catch 22. So deleting the global SM and updating components is deferred until the new one is compiled. 
 */
FGlobalShaderMap* GGlobalShaderMap_DeferredDeleteCopy[SP_NumPlatforms] = {nullptr};

#if ENABLE_COOK_STATS
namespace GlobalShaderCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;

	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
		{
			UsageStats.LogStats(AddStat, TEXT("GlobalShader.Usage"), TEXT(""));
			AddStat(TEXT("GlobalShader.Misc"), FCookStatsManager::CreateKeyValueArray(
				TEXT("ShadersCompiled"), ShadersCompiled
			));
		});
}
#endif

const FString& GetGlobalShaderMapDDCKey()
{
	static FString GlobalShaderMapDDCKey = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().GLOBALSHADERMAP_DERIVEDDATA_VER).ToString();
	return GlobalShaderMapDDCKey;
}

const FString& GetMaterialShaderMapDDCKey()
{
	static FString MaterialShaderMapDDCKey = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().MATERIALSHADERMAP_DERIVEDDATA_VER).ToString();
	return MaterialShaderMapDDCKey;
}

bool ShouldDumpShaderDDCKeys()
{
	return CVarShaderCompilerDumpDDCKeys.GetValueOnAnyThread();
}

void DumpShaderDDCKeyToFile(const EShaderPlatform InPlatform, bool bWithEditor, const FString& FileName, const FString& DDCKey)
{
	const FString SubDirectory = bWithEditor ? TEXT("Editor") : TEXT("Game");
	const FString TempPath = FPaths::ProjectSavedDir() / TEXT("ShaderDDCKeys") / SubDirectory / LexToString(InPlatform);
	IFileManager::Get().MakeDirectory(*TempPath, true);

	const FString TempFile = TempPath / FileName;

	TUniquePtr<FArchive> DumpAr(IFileManager::Get().CreateFileWriter(*TempFile));
	// serializing the string via << produces a non-textual file because it saves string's length, too
	DumpAr->Serialize(const_cast<TCHAR*>(*DDCKey), DDCKey.Len() * sizeof(TCHAR));
}

static void DumpShaderSourceToLog(const FString& DumpedSource)
{
	// Log dumped shader source line by line as message lengths in UE_LOG are limited.
	TArray<FString> DumpedSourceLines;
	DumpedSource.ParseIntoArrayLines(DumpedSourceLines, /*bCullEmpty:*/ false);

	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
	GLog->Serialize(TEXT("\n======================= DUMPED SHADER BEGIN =======================\n"), ELogVerbosity::Log, NAME_None);
	for (const FString& Line : DumpedSourceLines)
		{
		GLog->Serialize(*Line, ELogVerbosity::Log, NAME_None);
	}
	GLog->Serialize(TEXT("\n======================= DUMPED SHADER END =======================\n"), ELogVerbosity::Log, NAME_None);
}

namespace ShaderCompiler
{
	bool IsJobCacheEnabled()
	{
		return GShaderCompilerJobCache != 0;
	}

	bool IsRemoteCompilingAllowed()
	{
		// commandline switches override the CVars
		static bool bDisabledFromCommandline = FParse::Param(FCommandLine::Get(), TEXT("NoRemoteShaderCompile"));
		if (bDisabledFromCommandline)
		{
			return false;
		}

		return GShaderCompilerAllowDistributedCompilation != 0;
	}
}

// The Id of 0 is reserved for global shaders
FThreadSafeCounter FShaderCommonCompileJob::JobIdCounter(2);

uint32 FShaderCommonCompileJob::GetNextJobId()
{
	uint32 Id = JobIdCounter.Increment();
	if (Id == UINT_MAX)
	{
		JobIdCounter.Set(2);
	}
	return Id;
}

FShaderPipelineCompileJob::FShaderPipelineCompileJob(uint32 InHash, uint32 InId, EShaderCompileJobPriority InPriroity, const FShaderPipelineCompileJobKey& InKey) :
	FShaderCommonCompileJob(Type, InHash, InId, InPriroity),
	Key(InKey)
{
	const auto& Stages = InKey.ShaderPipeline->GetStages();
	StageJobs.Empty(Stages.Num());
	for (const FShaderType* ShaderType : Stages)
	{
		const FShaderCompileJobKey StageKey(ShaderType, InKey.VFType, InKey.PermutationId);
		StageJobs.Add(new FShaderCompileJob(StageKey.MakeHash(InId), InId, InPriroity, StageKey));
	}
}

FShaderCompileJobCollection::FShaderCompileJobCollection()
{
	FMemory::Memzero(PendingJobs);
	FMemory::Memzero(NumPendingJobs);

	PrintStatsCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("r.ShaderCompiler.PrintStats"),
		TEXT("Prints out to the log the stats for the shader compiler."),
		FConsoleCommandDelegate::CreateRaw(this, &FShaderCompileJobCollection::HandlePrintStats),
		ECVF_Default
	);
}

void FShaderCompileJobCollection::InternalAddJob(FShaderCommonCompileJob* InJob)
{
	const int32 TypeIndex = (int32)InJob->Type;

	int32 JobIndex = INDEX_NONE;
	if (FreeIndices[TypeIndex].Num() > 0)
	{
		JobIndex = FreeIndices[TypeIndex].Pop(false);
		check(!Jobs[TypeIndex][JobIndex].IsValid());
		Jobs[TypeIndex][JobIndex] = InJob;
	}
	else
	{
		JobIndex = Jobs[TypeIndex].Add(InJob);
	}

	check(Jobs[TypeIndex][JobIndex].IsValid());
	JobHash[TypeIndex].Add(InJob->Hash, JobIndex);
	
	check(InJob->Priority != EShaderCompileJobPriority::None);
	check(InJob->PendingPriority == EShaderCompileJobPriority::None);
	check(InJob->JobIndex == INDEX_NONE);
	InJob->JobIndex = JobIndex;
}

static FShaderCommonCompileJob* CloneJob_Single(const FShaderCompileJob* SrcJob)
{
	FShaderCompileJob* Job = new FShaderCompileJob(SrcJob->Hash, SrcJob->Id, SrcJob->Priority, SrcJob->Key);
	Job->ShaderParameters = SrcJob->ShaderParameters;
	Job->PendingShaderMap = SrcJob->PendingShaderMap;
	Job->Input = SrcJob->Input;
	if (SrcJob->bInputHashSet)
	{
		Job->InputHash = SrcJob->InputHash;
		Job->bInputHashSet = true;
	}
	ensure(Job->bInputHashSet == SrcJob->bInputHashSet);
	return Job;
}

static FShaderCommonCompileJob* CloneJob_Pipeline(const FShaderPipelineCompileJob* SrcJob)
{
	FShaderPipelineCompileJob* Job = new FShaderPipelineCompileJob(SrcJob->Hash, SrcJob->Id, SrcJob->Priority, SrcJob->Key);
	check(Job->StageJobs.Num() == SrcJob->StageJobs.Num());
	Job->PendingShaderMap = SrcJob->PendingShaderMap;

	for(int32 i = 0; i < SrcJob->StageJobs.Num(); ++i)
	{
		Job->StageJobs[i]->Input = SrcJob->StageJobs[i]->Input;
	}

	if (SrcJob->bInputHashSet)
	{
		Job->InputHash = SrcJob->InputHash;
		Job->bInputHashSet = true;
	}
	ensure(Job->bInputHashSet == SrcJob->bInputHashSet);
	return Job;
}

static FShaderCommonCompileJob* CloneJob(const FShaderCommonCompileJob* SrcJob)
{
	switch (SrcJob->Type)
	{
	case EShaderCompileJobType::Single: return CloneJob_Single(static_cast<const FShaderCompileJob*>(SrcJob));
	case EShaderCompileJobType::Pipeline:  return CloneJob_Pipeline(static_cast<const FShaderPipelineCompileJob*>(SrcJob));
	default: checkNoEntry(); return nullptr;
	}
}

void FShaderCompileJobCollection::InternalSetPriority(FShaderCommonCompileJob* Job, EShaderCompileJobPriority InPriority)
{
	const int32 PriorityIndex = (int32)InPriority;

	if (Job->PendingPriority != EShaderCompileJobPriority::None)
	{
		// Job hasn't started yet, move it to the pending list for the new priority
		const int32 PrevPriorityIndex = (int32)Job->PendingPriority;
		check(Job->PendingPriority == Job->Priority);
		check(NumPendingJobs[PrevPriorityIndex] > 0);
		NumPendingJobs[PrevPriorityIndex]--;
		Job->Unlink();

		NumPendingJobs[PriorityIndex]++;
		ensure(!ShaderCompiler::IsJobCacheEnabled() || Job->bInputHashSet);
		Job->LinkHead(PendingJobs[PriorityIndex]);
		Job->Priority = InPriority;
		Job->PendingPriority = InPriority;
	}
	else if (!Job->bFinalized &&
		Job->CurrentWorker == EShaderCompilerWorkerType::Distributed &&
		InPriority == EShaderCompileJobPriority::ForceLocal)
	{
		FShaderCommonCompileJob* NewJob = CloneJob(Job);
		NewJob->Priority = InPriority;
		const int32 NewNumPendingJobs = NewJob->PendingShaderMap->NumPendingJobs.Increment();
		checkf(NewNumPendingJobs > 1, TEXT("Invalid number of pending jobs %d, should have had at least 1 job previously"), NewNumPendingJobs);
		InternalAddJob(NewJob);

		GShaderCompilerStats->RegisterNewPendingJob(*NewJob);
		NumPendingJobs[PriorityIndex]++;
		ensureMsgf(NewJob->bInputHashSet == Job->bInputHashSet, TEXT("Cloned and original jobs should either both have input hash, or both not have it. Job->bInputHashSet=%d, NewJob->bInputHashSet=%d"),
			Job->bInputHashSet,
			NewJob->bInputHashSet
			);
		ensureMsgf(!ShaderCompiler::IsJobCacheEnabled() || NewJob->GetInputHash() == Job->GetInputHash(),
			TEXT("If shader jobs cache is enabled, cloned job should have the same input hash as the original, and it doesn't.")
			);
		NewJob->LinkHead(PendingJobs[PriorityIndex]);
		NewJob->PendingPriority = InPriority;
		NumOutstandingJobs.Increment();

		//UE_LOG(LogShaderCompilers, Display, TEXT("Submitted duplicate 'ForceLocal' shader compile job to replace existing XGE job"));
	}
}

void FShaderCompileJobCollection::InternalRemoveJob(FShaderCommonCompileJob* InJob)
{
	const int32 TypeIndex = (int32)InJob->Type;
	const int32 JobIndex = InJob->JobIndex;

	check(JobIndex != INDEX_NONE);
	check(Jobs[TypeIndex][JobIndex] == InJob);
	check(InJob->PendingPriority == EShaderCompileJobPriority::None);
	InJob->JobIndex = INDEX_NONE;

	JobHash[TypeIndex].Remove(InJob->Hash, JobIndex);
	FreeIndices[TypeIndex].Add(JobIndex);
	Jobs[TypeIndex][JobIndex].SafeRelease();
}


void FShaderCompileJobCollection::RemoveJob(FShaderCommonCompileJob* InJob)
{
	FWriteScopeLock Locker(Lock);
	InternalRemoveJob(InJob);
}

int32 FShaderCompileJobCollection::RemoveAllPendingJobsWithId(uint32 InId)
{
	int32 NumRemoved = 0;
	{
		FWriteScopeLock Locker(Lock);
		for (int32 PriorityIndex = 0; PriorityIndex < NumShaderCompileJobPriorities; ++PriorityIndex)
		{
			for (FShaderCommonCompileJob::TIterator It(PendingJobs[PriorityIndex]); It;)
			{
				FShaderCommonCompileJob& Job = *It;
				It.Next();

				if (Job.Id == InId)
				{
					if (ShaderCompiler::IsJobCacheEnabled())
					{
						JobsInFlight.Remove(Job.GetInputHash());
					}

					check(NumPendingJobs[PriorityIndex] > 0);
					NumPendingJobs[PriorityIndex]--;
					Job.Unlink();
					Job.PendingPriority = EShaderCompileJobPriority::None;
					InternalRemoveJob(&Job);
					++NumRemoved;
				}
			}
		}

		if (ShaderCompiler::IsJobCacheEnabled())
		{
			// Also look into the jobs that are cached
			// Since each entry in DuplicateJobsWaitList is a list, and the head node can be removed, we essentially have to rebuild it
			for (TMap<FShaderCommonCompileJob::FInputHash, FShaderCommonCompileJob*>::TIterator Iter(DuplicateJobsWaitList); Iter; ++Iter)
			{
				FShaderCommonCompileJob* ListHead = Iter.Value();
				FShaderCommonCompileJob* NewListHead = ListHead;

				// each entry in DJWL is a linked list of jobs that share the same ihash
				for (FShaderCommonCompileJob::TIterator It(ListHead); It;)
				{
					FShaderCommonCompileJob& Job = *It;
					It.Next();

					if (Job.Id == InId)
					{
						// if we're removing the list head, we need to update the next
						if (NewListHead == &Job)
						{
							NewListHead = Job.Next();
						}

						Job.Unlink();
						Job.PendingPriority = EShaderCompileJobPriority::None;
						InternalRemoveJob(&Job);
						++NumRemoved;
					}
				}

				if (NewListHead == nullptr)
				{
					// we removed the last job for this hash
					Iter.RemoveCurrent();
				}
				else if (NewListHead != ListHead)
				{
					// update the mapping
					Iter.Value() = NewListHead;
				}
			}
		}
	}

	InternalSubtractNumOutstandingJobs(NumRemoved);

	return NumRemoved;
}

void FShaderCompileJobCollection::SubmitJobs(const TArray<FShaderCommonCompileJobPtr>& InJobs)
{
	if (InJobs.Num() > 0)
	{
		// all jobs (not just actually submitted ones) count as outstanding. This needs to be done early because
		// we may fulfill some of the jobs from the cache (and we will be subtracting them)
		NumOutstandingJobs.Add(InJobs.Num());

		int32 SubmittedJobsCount = 0;
		int32 NumSubmittedJobs[NumShaderCompileJobPriorities] = { 0 };
		{
			// Just precompute the InputHash for each job in multiple-thread.
			if (ShaderCompiler::IsJobCacheEnabled())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ShaderCompiler.GetInputHash);
				ParallelFor( TEXT("ShaderCompiler.GetInputHash.PF"), InJobs.Num(),1, [&InJobs](int32 Index) { InJobs[Index]->GetInputHash(); }, EParallelForFlags::Unbalanced);
			}

			FWriteScopeLock Locker(Lock);

			// Optimization: Only search the linked list once to prevent O(n^2) behavior
			FShaderCommonCompileJob* ExistingJobs[NumShaderCompileJobPriorities] = { nullptr };

			for (FShaderCommonCompileJob* Job : InJobs)
			{
				check(Job->JobIndex != INDEX_NONE);
				check(Job->Priority != EShaderCompileJobPriority::None);
				check(Job->PendingPriority == EShaderCompileJobPriority::None);

				const int32 PriorityIndex = (int32)Job->Priority;
				bool bNewJob = true;
				if (ShaderCompiler::IsJobCacheEnabled())
				{
					const FShaderCommonCompileJob::FInputHash& InputHash = Job->GetInputHash();

					const bool bCheckDDC = !(Job->bIsDefaultMaterial || Job->bIsGlobalShader);

					// see if we can find the job in the cache first
					if (TArray<uint8>* ExistingOutput = CompletedJobsCache.Find(InputHash, bCheckDDC))
					{
						UE_LOG(LogShaderCompilers, UE_SHADERCACHE_LOG_LEVEL, TEXT("There is already a cached job with the ihash %s, processing the new one immediately."), *LexToString(InputHash));
						FMemoryReader MemReader(*ExistingOutput);
						Job->SerializeOutput(MemReader);

						// finish the job instantly
						ProcessFinishedJob(Job, true);

						continue;
					}
					// see if another job with the same input hash is being worked on
					else if (FShaderCommonCompileJob** DuplicateInFlight = JobsInFlight.Find(InputHash))
					{
						UE_LOG(LogShaderCompilers, UE_SHADERCACHE_LOG_LEVEL, TEXT("There is an outstanding job with the ihash %s, not submitting another one (adding to wait list)."), *LexToString(InputHash));

						// because of the cloned jobs, we need to maintain a separate mapping
						FShaderCommonCompileJob** WaitListHead = DuplicateJobsWaitList.Find(InputHash);
						if (WaitListHead)
						{
							Job->LinkAfter(*WaitListHead);
						}
						else
						{
							DuplicateJobsWaitList.Add(InputHash, Job);
						}
						bNewJob = false;
					}
					else
					{
						// track new jobs so we can dedupe them
						JobsInFlight.Add(InputHash, Job);
					}
				}

				// new job
				if (bNewJob)
				{
					GShaderCompilerStats->RegisterNewPendingJob(*Job);
					ensure(!ShaderCompiler::IsJobCacheEnabled() || Job->bInputHashSet);
#if UE_SHADERCOMPILER_FIFO_JOB_EXECUTION
					// link the job at the end of pending list, so we're executing them in a FIFO and not LIFO order
					if (PendingJobs[PriorityIndex])
					{
						// Optimization: Only search the linked list if we have to
						if (ExistingJobs[PriorityIndex])
						{
							Job->LinkAfter(ExistingJobs[PriorityIndex]);
							ExistingJobs[PriorityIndex] = Job;
						}
						else
						{
							for (FShaderCommonCompileJob::TIterator It(PendingJobs[PriorityIndex]); It; ++It)
							{
								FShaderCommonCompileJob& ExistingJob = *It;
								if (ExistingJob.GetNextLink() == nullptr)
								{
									Job->LinkAfter(&ExistingJob);
									ExistingJobs[PriorityIndex] = Job;
									break;
								}
							}
						}
					}
					else
#endif // UE_SHADERCOMPILER_FIFO_JOB_EXECUTION
					{
						Job->LinkHead(PendingJobs[PriorityIndex]);
					}
					

					NumPendingJobs[PriorityIndex]++;
					NumSubmittedJobs[PriorityIndex]++;
					Job->PendingPriority = Job->Priority;
					++SubmittedJobsCount;
				}
			}
		}

		UE_LOG(LogShaderCompilers, UE_SHADERCACHE_LOG_LEVEL, TEXT("Actual jobs submitted %d (of %d new), total outstanding jobs: %d."), SubmittedJobsCount, InJobs.Num(), NumOutstandingJobs.GetValue());

		for (int32 PriorityIndex = 0; PriorityIndex < NumShaderCompileJobPriorities; ++PriorityIndex)
		{
			if (NumSubmittedJobs[PriorityIndex] > 0)
			{
				UE_LOG(LogShaderCompilers, Verbose, TEXT("Submitted %d shader compile jobs with '%s' priority"),
					NumSubmittedJobs[PriorityIndex],
					ShaderCompileJobPriorityToString((EShaderCompileJobPriority)PriorityIndex));
			}
		}
	}
}

void FShaderCompileJobCollection::HandlePrintStats()
{
	GShaderCompilingManager->PrintStats(true);
}

TRACE_DECLARE_INT_COUNTER(Shaders_Compiled, TEXT("Shaders/Compiled"));
void FShaderCompileJobCollection::ProcessFinishedJob(FShaderCommonCompileJob* FinishedJob, bool bWasCached)
{
	if (!bWasCached)
	{
		GShaderCompilerStats->RegisterFinishedJob(*FinishedJob);
		TRACE_COUNTER_ADD(Shaders_Compiled, 1);
	}

	// TODO: have a pending shader map critical section? not clear at this point if we can be accessing the results on another thread at the same time
	FShaderMapCompileResults& ShaderMapResults = *(FinishedJob->PendingShaderMap);
	ShaderMapResults.FinishedJobs.Add(FinishedJob);
	ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && FinishedJob->bSucceeded;

	const int32 NumPendingJobsForSM = ShaderMapResults.NumPendingJobs.Decrement();
	checkf(NumPendingJobsForSM >= 0, TEXT("Problem tracking pending jobs for a SM (%d), number of pending jobs (%d) is negative!"), FinishedJob->Id, NumPendingJobsForSM);

	InternalSubtractNumOutstandingJobs(1);
	if (!bWasCached && ShaderCompiler::IsJobCacheEnabled())
	{
		AddToCacheAndProcessPending(FinishedJob);
	}
}

void FShaderCompileJobCollection::AddToCacheAndProcessPending(FShaderCommonCompileJob* FinishedJob)
{
	if (!ShaderCompiler::IsJobCacheEnabled())
	{
		return;
	}

	ensureMsgf(FinishedJob->bInputHashSet, TEXT("Finished job didn't have input hash set, was shader compiler jobs cache toggled runtime?"));

	const FShaderCommonCompileJob::FInputHash& InputHash = FinishedJob->GetInputHash();
	TArray<uint8> Output;
	FMemoryWriter Writer(Output);
	FinishedJob->SerializeOutput(Writer);

	// TODO: reduce the scope - e.g. SerializeOutput and processing finished jobs can be moved out of it
	FWriteScopeLock JobLocker(Lock);

	// see if there are outstanding jobs that also need to be resolved
	int32 NumOutstandingJobsWithSameHash = 0;
	if (FShaderCommonCompileJob** WaitList = DuplicateJobsWaitList.Find(InputHash))
	{
		FShaderCommonCompileJob* CurHead = *WaitList;
		while (CurHead)
		{
			checkf(CurHead != FinishedJob, TEXT("Job that is being added to cache was also on a waiting list! Error in bookkeeping."));

			FMemoryReader MemReader(Output);
			CurHead->SerializeOutput(MemReader);
			checkf(CurHead->bSucceeded == FinishedJob->bSucceeded, TEXT("Different success status for the job with the same ihash"));

			// finish the job instantly
			ProcessFinishedJob(CurHead, true);
			++NumOutstandingJobsWithSameHash;

			CurHead = CurHead->Next();
		}

		// remove the waitlist head
		DuplicateJobsWaitList.Remove(InputHash);

		if (NumOutstandingJobsWithSameHash > 0)
		{
			UE_LOG(LogShaderCompilers, UE_SHADERCACHE_LOG_LEVEL, TEXT("Processed %d outstanding jobs with the same ihash %s."), NumOutstandingJobsWithSameHash, *LexToString(InputHash));
		}
	}

	if (FinishedJob->bSucceeded)
	{
		const bool bAddToDDC = !(FinishedJob->bIsDefaultMaterial || FinishedJob->bIsGlobalShader);
		// we only cache jobs that succeded
		CompletedJobsCache.Add(InputHash, Output, NumOutstandingJobsWithSameHash, bAddToDDC);
	}

	// remove ourselves from the jobs in flight, if we were there (if this job is a cloned job it might not have been)
	JobsInFlight.Remove(InputHash);
}

void FShaderCompileJobCollection::LogCachingStats()
{
	FWriteScopeLock Locker(Lock);	// write lock because logging actually changes the cache state (in a minor way - updating the memory used - but still).
	CompletedJobsCache.LogStats();
}

int32 FShaderCompileJobCollection::GetNumPendingJobs() const
{
	FReadScopeLock Locker(Lock);
	int32 NumJobs = 0;
	for (int32 i = 0; i < NumShaderCompileJobPriorities; ++i)
	{
		NumJobs += NumPendingJobs[i];
	}
	return NumJobs;
}

int32 FShaderCompileJobCollection::GetPendingJobs(EShaderCompilerWorkerType InWorkerType, EShaderCompileJobPriority InPriority, int32 MinNumJobs, int32 MaxNumJobs, TArray<FShaderCommonCompileJobPtr>& OutJobs)
{
	check(InWorkerType != EShaderCompilerWorkerType::None);
	check(InPriority != EShaderCompileJobPriority::None);

	const int32 PriorityIndex = (int32)InPriority;
	int32 NumPendingJobsOfPriority = 0;
	{
		FReadScopeLock Locker(Lock);
		NumPendingJobsOfPriority = NumPendingJobs[PriorityIndex];
	}

	if (NumPendingJobsOfPriority < MinNumJobs)
	{
		// Not enough jobs
		return 0;
	}

	FWriteScopeLock Locker(Lock);

	// there was a time window before we checked and then acquired the write lock - make sure the number is still sufficient
	NumPendingJobsOfPriority = NumPendingJobs[PriorityIndex];
	if (NumPendingJobsOfPriority < MinNumJobs)
	{
		// Not enough jobs
		return 0;
	}
	
	OutJobs.Reserve(OutJobs.Num() + FMath::Min(MaxNumJobs, NumPendingJobsOfPriority));
	int32 NumJobs = FMath::Min(MaxNumJobs, NumPendingJobs[PriorityIndex]);
	FShaderCommonCompileJob::TIterator It(PendingJobs[PriorityIndex]);
	// Randomize job selection by randomly skipping over jobs while traversing the list.
	// Say, we need to pick 3 jobs out of 5 total. We can skip over 2 jobs in total, e.g. like this:
	// pick one (4 more to go and we need to get 2 of 4), skip one (3 more to go, picking 2 out of 3), pick one (2 more to go, picking 1 of 2), skip one, pick one.
	// It is possible that we won't skip at all and instead pick consequential jobs
	int32 MaxJobsWeCanSkipOver = NumPendingJobs[PriorityIndex] - NumJobs;
	for (int32 i = 0; i < NumJobs; ++i)
	{
		FShaderCommonCompileJob& Job = *It;

		GShaderCompilerStats->RegisterAssignedJob(Job);
		// Temporary commented out until r.ShaderDevelopmentMode=1 shader error retry crash gets fixed
		//check(Job.CurrentWorker == EShaderCompilerWorkerType::None);
		//check(Job.PendingPriority == InPriority);
		ensure(!ShaderCompiler::IsJobCacheEnabled() || Job.bInputHashSet);

		It.Next();
		Job.Unlink();

		Job.PendingPriority = EShaderCompileJobPriority::None;
		Job.CurrentWorker = InWorkerType;
		OutJobs.Add(&Job);

		// get a random number of jobs to skip (if we can). We're skipping after taking the first job so we can ensure that we always take the latest job into the batch
		if (MaxJobsWeCanSkipOver > 0)
		{
			int32 NumJobsToSkipOver = FMath::RandHelper(MaxJobsWeCanSkipOver + 1);
			while (NumJobsToSkipOver > 0 && It)
			{
				It.Next();
				--NumJobsToSkipOver;
				--MaxJobsWeCanSkipOver;
			}
			checkf(MaxJobsWeCanSkipOver >= 0, TEXT("We skipped over too many jobs"));
			checkf(MaxJobsWeCanSkipOver <= NumPendingJobs[PriorityIndex] - i, TEXT("Number of jobs to skip should stay less or equal than the number of nodes to go"));
		}
	}

	NumPendingJobs[PriorityIndex] -= NumJobs;
	return NumJobs;
}

FShaderCompileJob* FShaderCompileJobCollection::PrepareJob(uint32 InId, const FShaderCompileJobKey& InKey, EShaderCompileJobPriority InPriority)
{
	return InternalPrepareJob<FShaderCompileJob>(InId, InKey, InPriority);
}

FShaderPipelineCompileJob* FShaderCompileJobCollection::PrepareJob(uint32 InId, const FShaderPipelineCompileJobKey& InKey, EShaderCompileJobPriority InPriority)
{
	return InternalPrepareJob<FShaderPipelineCompileJob>(InId, InKey, InPriority);
}

static float GRegularWorkerTimeToLive = 20.0f;
static float GBuildWorkerTimeToLive = 600.0f;

// Configuration to retry shader compile through workers after a worker has been abandoned
static constexpr int32 GSingleThreadedRunsIdle = -1;
static constexpr int32 GSingleThreadedRunsDisabled = -2;
static constexpr int32 GSingleThreadedRunsIncreaseFactor = 8;
static constexpr int32 GSingleThreadedRunsMaxCount = (1 << 24);

static void ModalErrorOrLog(const FString& Text, int64 CurrentFilePos = 0, int64 ExpectedFileSize = 0)
{
	FString BadFile;
	if (CurrentFilePos > ExpectedFileSize)
{
		// Corrupt file
		BadFile = FString::Printf(TEXT("(Truncated or corrupt output file! Current file pos %lld, file size %lld)"), CurrentFilePos, ExpectedFileSize);
	}

	if (FPlatformProperties::SupportsWindowedMode())
	{
		UE_LOG(LogShaderCompilers, Error, TEXT("%s%s"), *Text, *BadFile);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
		FPlatformMisc::RequestExit(false);
		return;
	}
	else
	{
		UE_LOG(LogShaderCompilers, Fatal, TEXT("%s%s"), *Text, *BadFile);
	}
}

template<class EnumType>
constexpr auto& CastEnumToUnderlyingTypeReference(EnumType& Type)
{
	static_assert(TIsEnum<EnumType>::Value, "");
	using UnderType = __underlying_type(EnumType);
	return reinterpret_cast<UnderType&>(Type);
}

// Set to 1 to debug ShaderCompileWorker.exe. Set a breakpoint in LaunchWorker() to get the cmd-line.
#define DEBUG_SHADERCOMPILEWORKER 0

// Default value comes from bPromptToRetryFailedShaderCompiles in BaseEngine.ini
// This is set as a global variable to allow changing in the debugger even in release
// For example if there are a lot of content shader compile errors you want to skip over without relaunching
bool GRetryShaderCompilation = true;

static FShaderCompilingManager::EDumpShaderDebugInfo GDumpShaderDebugInfo = FShaderCompilingManager::EDumpShaderDebugInfo::Never;
static FAutoConsoleVariableRef CVarDumpShaderDebugInfo(
	TEXT("r.DumpShaderDebugInfo"),
	CastEnumToUnderlyingTypeReference(GDumpShaderDebugInfo),
	TEXT("Dumps debug info for compiled shaders to GameName/Saved/ShaderDebugInfo\n")
	TEXT("When set to 1, debug info is dumped for all compiled shader\n")
	TEXT("When set to 2, it is restricted to shaders with compilation errors\n")
	TEXT("When set to 3, it is restricted to shaders with compilation errors or warnings\n")
	TEXT("The debug info is platform dependent, but usually includes a preprocessed version of the shader source.\n")
	TEXT("Global shaders automatically dump debug info if r.ShaderDevelopmentMode is enabled, this cvar is not necessary.\n")
	TEXT("On iOS, if the PowerVR graphics SDK is installed to the default path, the PowerVR shader compiler will be called and errors will be reported during the cook.")
	);

static int32 GDumpShaderDebugInfoShort = 0;
static FAutoConsoleVariableRef CVarDumpShaderDebugShortNames(
	TEXT("r.DumpShaderDebugShortNames"),
	GDumpShaderDebugInfoShort,
	TEXT("Only valid when r.DumpShaderDebugInfo > 0.\n")
	TEXT("When set to 1, will shorten names factory and shader type folder names to avoid issues with long paths.")
	);

static int32 GDumpShaderDebugInfoSCWCommandLine = 0;
static FAutoConsoleVariableRef CVarDumpShaderDebugSCWCommandLine(
	TEXT("r.DumpShaderDebugWorkerCommandLine"),
	GDumpShaderDebugInfoSCWCommandLine,
	TEXT("Only valid when r.DumpShaderDebugInfo > 0.\n")
	TEXT("When set to 1, it will generate a file that can be used with ShaderCompileWorker's -directcompile.")
	);

static int32 GShaderMapCompilationTimeout = 2 * 60 * 60;	// anything below an hour can hit a false positive
static FAutoConsoleVariableRef CVarShaderMapCompilationTimeout(
	TEXT("r.ShaderCompiler.ShadermapCompilationTimeout"),
	GShaderMapCompilationTimeout,
	TEXT("Maximum number of seconds a single shadermap (which can be comprised of multiple jobs) can be compiled after being considered hung.")
);

static int32 GCrashOnHungShaderMaps = 0;
static FAutoConsoleVariableRef CVarCrashOnHungShaderMaps(
	TEXT("r.ShaderCompiler.CrashOnHungShaderMaps"),
	GCrashOnHungShaderMaps,
	TEXT("If set to 1, the shader compiler will crash on hung shadermaps.")
);

static int32 GLogShaderCompilerStats = 0;
static FAutoConsoleVariableRef CVarLogShaderCompilerStats(
	TEXT("r.LogShaderCompilerStats"),
	GLogShaderCompilerStats,
	TEXT("When set to 1, Log detailed shader compiler stats.")
);


static int32 GShowShaderWarnings = 0;
static FAutoConsoleVariableRef CVarShowShaderWarnings(
	TEXT("r.ShowShaderCompilerWarnings"),
	GShowShaderWarnings,
	TEXT("When set to 1, will display all warnings.")
	);

static int32 GForceAllCoresForShaderCompiling = 0;
static FAutoConsoleVariableRef CVarForceAllCoresForShaderCompiling(
	TEXT("r.ForceAllCoresForShaderCompiling"),
	GForceAllCoresForShaderCompiling,
	TEXT("When set to 1, it will ignore INI settings and launch as many ShaderCompileWorker instances as cores are available.\n")
	TEXT("Improves shader throughput but for big projects it can make the machine run OOM")
);

static TAutoConsoleVariable<int32> CVarShadersSymbols(
	TEXT("r.Shaders.Symbols"),
	0,
	TEXT("Enables debugging of shaders in platform specific graphics debuggers. This will generate and write shader symbols.\n")
	TEXT("This enables the behavior of both r.Shaders.GenerateSymbols and r.Shaders.WriteSymbols.\n")
	TEXT("Enables shader debugging features that require shaders to be recompiled. This compiles shaders with symbols and also includes extra runtime information like shader names. When using graphical debuggers it can be useful to enable this on startup.\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShadersGenerateSymbols(
	TEXT("r.Shaders.GenerateSymbols"),
	0,
	TEXT("Enables generation of data for shader debugging when compiling shaders. This explicitly does not write any shader symbols to disk.\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShadersWriteSymbols(
	TEXT("r.Shaders.WriteSymbols"),
	0,
	TEXT("Enables writing shader symbols to disk for platforms that support that. This explicitly does not enable generation of shader symbols.\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<FString> CVarShadersSymbolPathOverride(
	TEXT("r.Shaders.SymbolPathOverride"),
	"",
	TEXT("Override output location of shader symbols. If the path contains the text '{Platform}', that will be replaced with the shader platform string.\n")
	TEXT("Empty: use default location Saved/ShaderSymbols/{Platform}\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarAllowUniqueDebugInfo(
	TEXT("r.Shaders.AllowUniqueSymbols"),
	0,
	TEXT("When enabled, this tells supported shader compilers to generate symbols based on source files.\n")
	TEXT("Enabling this can cause a drastic increase in the number of symbol files, enable only if absolutely necessary.\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShadersWriteSymbolsZip(
	TEXT("r.Shaders.WriteSymbols.Zip"),
	0,
	TEXT(" 0: Export as loose files.\n")
	TEXT(" 1: Export as an uncompressed archive.\n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShadersEnableExtraData(
	TEXT("r.Shaders.ExtraData"),
	0,
	TEXT("Enables generation of extra shader data that can be used at runtime. This includes shader names and other platform specific data.\n")
	TEXT("This can add bloat to compiled shaders and can prevent shaders from being deduplicated.\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarOptimizeShaders(
	TEXT("r.Shaders.Optimize"),
	1,
	TEXT("Whether to optimize shaders.  When using graphical debuggers like Nsight it can be useful to disable this on startup.\n")
	TEXT("This setting can be overriden in any Engine.ini under the [ShaderCompiler] section."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderFastMath(
	TEXT("r.Shaders.FastMath"),
	1,
	TEXT("Whether to use fast-math optimisations in shaders."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderZeroInitialise(
	TEXT("r.Shaders.ZeroInitialise"),
	1,
	TEXT("Whether to enforce zero initialise local variables of primitive type in shaders. Defaults to 1 (enabled). Not all shader languages can omit zero initialisation."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderBoundsChecking(
	TEXT("r.Shaders.BoundsChecking"),
	1,
	TEXT("Whether to enforce bounds-checking & flush-to-zero/ignore for buffer reads & writes in shaders. Defaults to 1 (enabled). Not all shader languages can omit bounds checking."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderWarningsAsErrors(
	TEXT("r.Shaders.WarningsAsErrors"),
	0,
	TEXT("Whether to treat warnings as errors when running the shader compiler. Defaults to 0 (disabled). Not all compilers support this mode."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderFlowControl(
	TEXT("r.Shaders.FlowControlMode"),
	0,
	TEXT("Specifies whether the shader compiler should preserve or unroll flow-control in shader code.\n")
	TEXT("This is primarily a debugging aid and will override any per-shader or per-material settings if not left at the default value (0).\n")
	TEXT("\t0: Off (Default) - Entirely at the discretion of the platform compiler or the specific shader/material.\n")
	TEXT("\t1: Prefer - Attempt to preserve flow-control.\n")
	TEXT("\t2: Avoid - Attempt to unroll and flatten flow-control.\n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarD3DRemoveUnusedInterpolators(
	TEXT("r.D3D.RemoveUnusedInterpolators"),
	1,
	TEXT("Enables removing unused interpolators mode when compiling pipelines for D3D.\n")
	TEXT(" -1: Do not actually remove, but make the app think it did (for debugging)\n")
	TEXT(" 0: Disable (default)\n")
	TEXT(" 1: Enable removing unused"),
	ECVF_ReadOnly
	);

static TAutoConsoleVariable<int32> CVarD3DCheckedForTypedUAVs(
	TEXT("r.D3D.CheckedForTypedUAVs"),
	1,
	TEXT("Whether to disallow usage of typed UAV loads, as they are unavailable in Windows 7 D3D 11.0.\n")
	TEXT(" 0: Allow usage of typed UAV loads.\n")
	TEXT(" 1: Disallow usage of typed UAV loads. (default)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarD3DForceDXC(
	TEXT("r.D3D.ForceDXC"),
	0,
	TEXT("Forces DirectX Shader Compiler (DXC) to be used for all D3D shaders. Shaders compiled with this option are only compatible with D3D12.\n")
	TEXT(" 0: Disable (default)\n")
	TEXT(" 1: Force new compiler for all shaders"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarD3DForceShaderConductorDXCRewrite(
	TEXT("r.D3D.ForceShaderConductorDXCRewrite"),
	0,
	TEXT("Forces rewriting using ShaderConductor when DXC is enabled.\n")
	TEXT(" 0: Do not rewrite (default)\n")
	TEXT(" 1: Force ShaderConductor rewrite"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarOpenGLForceDXC(
	TEXT("r.OpenGL.ForceDXC"),
	1,
	TEXT("Forces DirectX Shader Compiler (DXC) to be used for all OpenGL shaders instead of hlslcc.\n")
	TEXT(" 0: Disable\n")
	TEXT(" 1: Force new compiler for all shaders (default)"),
	ECVF_ReadOnly);

ENGINE_API int32 GCreateShadersOnLoad = 0;
static FAutoConsoleVariableRef CVarCreateShadersOnLoad(
	TEXT("r.CreateShadersOnLoad"),
	GCreateShadersOnLoad,
	TEXT("Whether to create shaders on load, which can reduce hitching, but use more memory.  Otherwise they will be created as needed.")
);

static TAutoConsoleVariable<FString> CVarShaderOverrideDebugDir(
	TEXT("r.OverrideShaderDebugDir"),
	"",
	TEXT("Override output location of shader debug files\n")
	TEXT("Empty: use default location Saved\\ShaderDebugInfo.\n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShadersValidation(
	TEXT("r.Shaders.Validation"),
	1,
	TEXT("Enabled shader compiler validation warnings and errors."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShadersRemoveDeadCode(
	TEXT("r.Shaders.RemoveDeadCode"),
	0,
	TEXT("EXPERIMENTAL: Run a preprocessing step that removes unreferenced code before compiling shaders.\n")
	TEXT("This can improve the compilation speed for shaders which include many large utility headers.\n")
	TEXT("\t0: Keep all input source code (Default).\n")
	TEXT("\t1: Remove unreferenced code before compilation\n"),
	ECVF_ReadOnly);

extern bool CompileShaderPipeline(const IShaderFormat* Compiler, FName Format, FShaderPipelineCompileJob* PipelineJob, const FString& Dir);

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"
namespace ShaderCompilerCookStats
{
	static double BlockingTimeSec = 0.0;
	static double GlobalBeginCompileShaderTimeSec = 0.0;
	static int32 GlobalBeginCompileShaderCalls = 0;
	static double ProcessAsyncResultsTimeSec = 0.0;
	static double AsyncCompileTimeSec = 0.0;

	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
		{
			AddStat(TEXT("ShaderCompiler"), FCookStatsManager::CreateKeyValueArray(
				TEXT("BlockingTimeSec"), BlockingTimeSec,
				TEXT("AsyncCompileTimeSec"), AsyncCompileTimeSec,
				TEXT("GlobalBeginCompileShaderTimeSec"), GlobalBeginCompileShaderTimeSec,
				TEXT("GlobalBeginCompileShaderCalls"), GlobalBeginCompileShaderCalls,
				TEXT("ProcessAsyncResultsTimeSec"), ProcessAsyncResultsTimeSec
			));
		});
}
#endif

// Make functions so the crash reporter can disambiguate the actual error because of the different callstacks
namespace SCWErrorCode
{
	void HandleGeneralCrash(const TCHAR* ExceptionInfo, const TCHAR* Callstack)
	{
		GLog->Panic();
		UE_LOG(LogShaderCompilers, Fatal, TEXT("ShaderCompileWorker crashed!\n%s\n\t%s"), ExceptionInfo, Callstack);
	}

	void HandleBadShaderFormatVersion(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleBadInputVersion(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleBadSingleJobHeader(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleBadPipelineJobHeader(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleCantDeleteInputFile(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleCantSaveOutputFile(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleNoTargetShaderFormatsFound(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleCantCompileForSpecificFormat(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleOutputFileEmpty(const TCHAR* Filename)
	{
		ModalErrorOrLog(FString::Printf(TEXT("Output file %s size is 0. Are you out of disk space?"), Filename));
	}

	void HandleOutputFileCorrupted(const TCHAR* Filename, int64 ExpectedSize, int64 ActualSize)
	{
		ModalErrorOrLog(FString::Printf(TEXT("Output file corrupted (expected %I64d bytes, but only got %I64d): %s"), ExpectedSize, ActualSize, Filename));
	}

	void HandleCrashInsidePlatformCompiler(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("Crash inside the platform compiler!\n%s"), Data));
	}
}

static TMap<FString, uint32> GetFormatVersionMap()
{
	TMap<FString, uint32> FormatVersionMap;

	const TArray<const class IShaderFormat*>& ShaderFormats = GetTargetPlatformManagerRef().GetShaderFormats();
	check(ShaderFormats.Num());
	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> OutFormats;
		ShaderFormats[Index]->GetSupportedFormats(OutFormats);
		check(OutFormats.Num());
		for (int32 InnerIndex = 0; InnerIndex < OutFormats.Num(); InnerIndex++)
		{
			uint32 Version = ShaderFormats[Index]->GetVersion(OutFormats[InnerIndex]);
			FormatVersionMap.Add(OutFormats[InnerIndex].ToString(), Version);
		}
	}

	return FormatVersionMap;
}

static int32 GetNumTotalJobs(const TArray<FShaderCommonCompileJobPtr>& Jobs)
{
	int32 NumJobs = 0;
	for (int32 Index = 0; Index < Jobs.Num(); ++Index)
	{
		auto* PipelineJob = Jobs[Index]->GetShaderPipelineJob();
		NumJobs += PipelineJob ? PipelineJob->StageJobs.Num() : 1;
	}

	return NumJobs;
}

static void SplitJobsByType(const TArray<FShaderCommonCompileJobPtr>& QueuedJobs, TArray<FShaderCompileJob*>& OutQueuedSingleJobs, TArray<FShaderPipelineCompileJob*>& OutQueuedPipelineJobs)
{
	for (int32 Index = 0; Index < QueuedJobs.Num(); ++Index)
	{
		FShaderCommonCompileJobPtr CommonJob = QueuedJobs[Index];
		FShaderPipelineCompileJob* PipelineJob = CommonJob->GetShaderPipelineJob();
		if (PipelineJob)
		{
			OutQueuedPipelineJobs.Add(PipelineJob);
		}
		else
		{
			FShaderCompileJob* SingleJob = CommonJob->GetSingleShaderJob();
			check(SingleJob);
			OutQueuedSingleJobs.Add(SingleJob);
		}
	}
}

// Serialize Queued Job information
bool FShaderCompileUtilities::DoWriteTasks(const TArray<FShaderCommonCompileJobPtr>& QueuedJobs, FArchive& InTransferFile, IDistributedBuildController* BuildDistributionController, bool bUseRelativePaths, bool bCompressTaskFile)
{
	int32 InputVersion = ShaderCompileWorkerInputVersion;
	InTransferFile << InputVersion;

	TArray<uint8> UncompressedArray;
	FMemoryWriter TransferMemory(UncompressedArray);
	FArchive& TransferFile = bCompressTaskFile ? TransferMemory : InTransferFile;
	if (!bCompressTaskFile)
	{
		// still write NAME_None as string
		FString FormatNone = FName(NAME_None).ToString();
		TransferFile << FormatNone;
	}

	static TMap<FString, uint32> FormatVersionMap = GetFormatVersionMap();

	TransferFile << FormatVersionMap;

	// Convert all the source directory paths to absolute, since SCW might be in a different directory to the editor executable
	TMap<FString, FString> ShaderSourceDirectoryMappings = AllShaderSourceDirectoryMappings();
	for(TPair<FString, FString>& Pair : ShaderSourceDirectoryMappings)
	{
		// Remap/enforce relative paths when bUseRelativePaths=true
		if (bUseRelativePaths && BuildDistributionController != nullptr)
		{
			FString SourcePath = FPaths::ConvertRelativePathToFull(Pair.Value);
			if (!FPaths::IsUnderDirectory(SourcePath, FPaths::RootDir()))
			{
				FString DestinationPath = BuildDistributionController->RemapPath(SourcePath);
				DestinationPath = FPaths::CreateStandardFilename(DestinationPath);
				Pair.Value = DestinationPath;
			}
			else
			{
				Pair.Value = FPaths::CreateStandardFilename(Pair.Value);
			}
		}
		else
		{
			Pair.Value = FPaths::ConvertRelativePathToFull(Pair.Value);
		}
	}
	TransferFile << ShaderSourceDirectoryMappings;

	TArray<FShaderCompileJob*> QueuedSingleJobs;
	TArray<FShaderPipelineCompileJob*> QueuedPipelineJobs;
	SplitJobsByType(QueuedJobs, QueuedSingleJobs, QueuedPipelineJobs);

	TArray<TRefCountPtr<FSharedShaderCompilerEnvironment>> SharedEnvironments;
	TArray<const FShaderParametersMetadata*> RequestShaderParameterStructures;

	// Gather External Includes and serialize separately, these are largely shared between jobs
	{
		TMap<FString, FString> ExternalIncludes;
		ExternalIncludes.Reserve(32);

		for (int32 JobIndex = 0; JobIndex < QueuedSingleJobs.Num(); JobIndex++)
		{
			QueuedSingleJobs[JobIndex]->Input.GatherSharedInputs(ExternalIncludes, SharedEnvironments, RequestShaderParameterStructures);
		}

		for (int32 JobIndex = 0; JobIndex < QueuedPipelineJobs.Num(); JobIndex++)
		{
			auto* PipelineJob = QueuedPipelineJobs[JobIndex];
			int32 NumStageJobs = PipelineJob->StageJobs.Num();

			for (int32 Index = 0; Index < NumStageJobs; Index++)
			{
				PipelineJob->StageJobs[Index]->Input.GatherSharedInputs(ExternalIncludes, SharedEnvironments, RequestShaderParameterStructures);
			}
		}

		int32 NumExternalIncludes = ExternalIncludes.Num();
		TransferFile << NumExternalIncludes;

		for (TMap<FString, FString>::TIterator It(ExternalIncludes); It; ++It)
		{
			TransferFile << It.Key();
			TransferFile << It.Value();
		}

		int32 NumSharedEnvironments = SharedEnvironments.Num();
		TransferFile << NumSharedEnvironments;

		for (int32 EnvironmentIndex = 0; EnvironmentIndex < SharedEnvironments.Num(); EnvironmentIndex++)
		{
			TransferFile << *SharedEnvironments[EnvironmentIndex];
		}
	}

	// Write shader parameter structures
	TArray<const FShaderParametersMetadata*> AllShaderParameterStructures;
	{
		// List all dependencies.
		for (int32 StructId = 0; StructId < RequestShaderParameterStructures.Num(); StructId++)
		{
			RequestShaderParameterStructures[StructId]->IterateStructureMetadataDependencies(
				[&](const FShaderParametersMetadata* Struct)
			{
				AllShaderParameterStructures.AddUnique(Struct);
			});
		}

		// Write all shader parameter structure.
		int32 NumParameterStructures = AllShaderParameterStructures.Num();
		TransferFile << NumParameterStructures;
		for (const FShaderParametersMetadata* Struct : AllShaderParameterStructures)
		{
			FString LayoutName = Struct->GetLayout().GetDebugName();
			FString StructTypeName = Struct->GetStructTypeName();
			FString ShaderVariableName = Struct->GetShaderVariableName();
			uint8 UseCase = uint8(Struct->GetUseCase());
			FString StructFileName = FString(ANSI_TO_TCHAR(Struct->GetFileName()));
			int32 StructFileLine = Struct->GetFileLine();
			uint32 Size = Struct->GetSize();
			int32 MemberCount = Struct->GetMembers().Num();

			static_assert(sizeof(UseCase) == sizeof(FShaderParametersMetadata::EUseCase), "Cast failure.");

			TransferFile << LayoutName;
			TransferFile << StructTypeName;
			TransferFile << ShaderVariableName;
			TransferFile << UseCase;
			TransferFile << StructFileName;
			TransferFile << StructFileLine;
			TransferFile << Size;
			TransferFile << MemberCount;

			for (const FShaderParametersMetadata::FMember& Member : Struct->GetMembers())
			{
				FString Name = Member.GetName();
				FString ShaderType = Member.GetShaderType();
				int32 FileLine = Member.GetFileLine();
				uint32 Offset = Member.GetOffset();
				uint8 BaseType = uint8(Member.GetBaseType());
				uint8 PrecisionModifier = uint8(Member.GetPrecision());
				uint32 NumRows = Member.GetNumRows();
				uint32 NumColumns = Member.GetNumColumns();
				uint32 NumElements = Member.GetNumElements();
				int32 StructMetadataIndex = INDEX_NONE;
				if (Member.GetStructMetadata())
				{
					StructMetadataIndex = AllShaderParameterStructures.Find(Member.GetStructMetadata());
					check(StructMetadataIndex != INDEX_NONE);
				}

				static_assert(sizeof(BaseType) == sizeof(EUniformBufferBaseType), "Cast failure.");
				static_assert(sizeof(PrecisionModifier) == sizeof(EShaderPrecisionModifier::Type), "Cast failure.");

				TransferFile << Name;
				TransferFile << ShaderType;
				TransferFile << FileLine;
				TransferFile << Offset;
				TransferFile << BaseType;
				TransferFile << PrecisionModifier;
				TransferFile << NumRows;
				TransferFile << NumColumns;
				TransferFile << NumElements;
				TransferFile << StructMetadataIndex;
			}
		}
	}

	// Write individual shader jobs
	{
		int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
		TransferFile << SingleJobHeader;

		int32 NumBatches = QueuedSingleJobs.Num();
		TransferFile << NumBatches;

		FString ShaderPlatformNameString = FDataDrivenShaderPlatformInfo::GetName(GMaxRHIShaderPlatform).ToString();
		TransferFile << ShaderPlatformNameString;

		// Serialize all the batched jobs
		for (int32 JobIndex = 0; JobIndex < QueuedSingleJobs.Num(); JobIndex++)
		{
			TransferFile << QueuedSingleJobs[JobIndex]->Input;
			QueuedSingleJobs[JobIndex]->Input.SerializeSharedInputs(TransferFile, SharedEnvironments, AllShaderParameterStructures);
		}
	}

	// Write shader pipeline jobs
	{
		int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
		TransferFile << PipelineJobHeader;

		int32 NumBatches = QueuedPipelineJobs.Num();
		TransferFile << NumBatches;
		for (int32 JobIndex = 0; JobIndex < QueuedPipelineJobs.Num(); JobIndex++)
		{
			auto* PipelineJob = QueuedPipelineJobs[JobIndex];
			FString PipelineName = PipelineJob->Key.ShaderPipeline->GetName();
			TransferFile << PipelineName;
			int32 NumStageJobs = PipelineJob->StageJobs.Num();
			TransferFile << NumStageJobs;
			for (int32 Index = 0; Index < NumStageJobs; Index++)
			{
				TransferFile << PipelineJob->StageJobs[Index]->GetSingleShaderJob()->Input;
				PipelineJob->StageJobs[Index]->Input.SerializeSharedInputs(TransferFile, SharedEnvironments, AllShaderParameterStructures);
			}
		}
	}

	if (bCompressTaskFile)
	{
		TransferFile.Close();

		FName CompressionFormatToUse = NAME_LZ4;

		FString FormatName = CompressionFormatToUse.ToString();
		InTransferFile << FormatName;

		// serialize uncompressed data size
		int32 UncompressedDataSize = UncompressedArray.Num();
		checkf(UncompressedDataSize != 0, TEXT("Did not write any data to the task file for the compression."));
		InTransferFile << UncompressedDataSize;

		// not using SerializeCompressed because it splits into smaller chunks
		int32 CompressedSizeBound = FCompression::CompressMemoryBound(CompressionFormatToUse, static_cast<int32>(UncompressedDataSize));
		TArray<uint8> CompressedBuffer;
		CompressedBuffer.SetNumUninitialized(CompressedSizeBound);

		int32 ActualCompressedSize = CompressedSizeBound;
		bool bSucceeded = FCompression::CompressMemory(CompressionFormatToUse, CompressedBuffer.GetData(), ActualCompressedSize, UncompressedArray.GetData(), UncompressedDataSize, COMPRESS_BiasSpeed);
		checkf(ActualCompressedSize <= CompressedSizeBound, TEXT("Compressed size was larger than the bound - we stomped the memory."));
		CompressedBuffer.SetNum(ActualCompressedSize, false);

		InTransferFile << CompressedBuffer;
		UE_LOG(LogShaderCompilers, Verbose, TEXT("Compressed the task file from %d bytes to %d bytes (%.2f%% savings)"), UncompressedDataSize, ActualCompressedSize,
			100.0 * (UncompressedDataSize - ActualCompressedSize) / static_cast<double>(UncompressedDataSize));
	}

	return InTransferFile.Close();
}

static void ProcessErrors(const FShaderCompileJob& CurrentJob, TArray<FString>& UniqueErrors, FString& ErrorString, int32& NumDumpedShaderSources)
{
	bool bReportedDebugInfo = false;

	for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
	{
		FShaderCompilerError CurrentError = CurrentJob.Output.Errors[ErrorIndex];
		int32 UniqueError = INDEX_NONE;

		if (UniqueErrors.Find(CurrentError.GetErrorString(), UniqueError))
		{
			// This unique error is being processed, remove it from the array
			UniqueErrors.RemoveAt(UniqueError);

			// Extract source location from error message if the shader backend doesn't provide it separated from the stripped message
			CurrentError.ExtractSourceLocation();

			// Remap filenames
			if (CurrentError.ErrorVirtualFilePath == TEXT("/Engine/Generated/Material.ush"))
			{
				// MaterialTemplate.usf is dynamically included as Material.usf
				// Currently the material translator does not add new lines when filling out MaterialTemplate.usf,
				// So we don't need the actual filled out version to find the line of a code bug.
				CurrentError.ErrorVirtualFilePath = TEXT("/Engine/Private/MaterialTemplate.ush");
			}
			else if (CurrentError.ErrorVirtualFilePath.Contains(TEXT("memory")))
			{
				check(CurrentJob.Key.ShaderType);

				// Files passed to the shader compiler through memory will be named memory
				// Only the shader's main file is passed through memory without a filename
				CurrentError.ErrorVirtualFilePath = FString(CurrentJob.Key.ShaderType->GetShaderFilename());
			}
			else if (CurrentError.ErrorVirtualFilePath == TEXT("/Engine/Generated/VertexFactory.ush"))
			{
				// VertexFactory.usf is dynamically included from whichever vertex factory the shader was compiled with.
				check(CurrentJob.Key.VFType);
				CurrentError.ErrorVirtualFilePath = FString(CurrentJob.Key.VFType->GetShaderFilename());
			}
			else if (CurrentError.ErrorVirtualFilePath == TEXT("") && CurrentJob.Key.ShaderType)
			{
				// Some shader compiler errors won't have a file and line number, so we just assume the error happened in file containing the entrypoint function.
				CurrentError.ErrorVirtualFilePath = FString(CurrentJob.Key.ShaderType->GetShaderFilename());
			}

			FString UniqueErrorPrefix;

			// If we dumped the shader info, add it before the first error string
			if (!GIsBuildMachine && !bReportedDebugInfo && CurrentJob.Input.DumpDebugInfoPath.Len() > 0)
			{
				UniqueErrorPrefix += FString::Printf(TEXT("Shader debug info dumped to: \"%s\"\n"), *CurrentJob.Input.DumpDebugInfoPath);
				bReportedDebugInfo = true;
			}

			if (CurrentJob.Key.ShaderType)
			{
				// Construct a path that will enable VS.NET to find the shader file, relative to the solution
				const FString SolutionPath = FPaths::RootDir();
				FString ShaderFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CurrentError.GetShaderSourceFilePath());
				UniqueErrorPrefix += FString::Printf(TEXT("%s(%s): Shader %s, Permutation %d, VF %s:\n\t"),
					*ShaderFilePath,
					*CurrentError.ErrorLineString,
					CurrentJob.Key.ShaderType->GetName(),
					CurrentJob.Key.PermutationId,
					CurrentJob.Key.VFType ? CurrentJob.Key.VFType->GetName() : TEXT("None"));
			}
			else
			{
				UniqueErrorPrefix += FString::Printf(TEXT("%s(0): "),
					*CurrentJob.Input.VirtualSourceFilePath);
			}

			FString UniqueErrorString = UniqueErrorPrefix + CurrentError.GetErrorStringWithLineMarker() + TEXT("\n");

			if (GIsBuildMachine)
			{
				// Format everything on one line, and with the correct verbosity, so we can display proper errors in the failure logs.
				UE_LOG(LogShaderCompilers, Error, TEXT("%s%s"), *UniqueErrorPrefix.Replace(TEXT("\n"), TEXT("")), *CurrentError.GetErrorStringWithLineMarker());

				const FString& DumpedSource = CurrentJob.Output.OptionalPreprocessedShaderSource;
				if (!DumpedSource.IsEmpty() && NumDumpedShaderSources < GMaxNumDumpedShaderSources)
				{
					// Limit number of preprocessed shaders to dump to the log as they are quite large
					DumpShaderSourceToLog(DumpedSource);
					++NumDumpedShaderSources;
				}
			}
			else if (FPlatformMisc::IsDebuggerPresent())
			{
				// Using OutputDebugString to avoid any text getting added before the filename,
				// Which will throw off VS.NET's ability to take you directly to the file and line of the error when double clicking it in the output window.
				FPlatformMisc::LowLevelOutputDebugStringf(*UniqueErrorString);
			}
			else
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("%s"), *UniqueErrorString);
			}

			ErrorString += UniqueErrorString;
		}
	}
}

static bool ReadSingleJob(FShaderCompileJob* CurrentJob, FArchive& OutputFile)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ReadSingleJob);

	check(!CurrentJob->bFinalized);
	CurrentJob->bFinalized = true;

	// Deserialize the shader compilation output.
	OutputFile << CurrentJob->Output;

	CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;
	// The job should already have a non-zero output hash
	checkf(CurrentJob->Output.OutputHash != FSHAHash() || !CurrentJob->bSucceeded, TEXT("OutputHash for a successful job was not set in the shader compile worker!"));

	if (CurrentJob->bSucceeded && CurrentJob->Input.DumpDebugInfoPath.Len() > 0)
	{
		// write down the output hash as a file
		FString HashFileName = FPaths::Combine(CurrentJob->Input.DumpDebugInfoPath, TEXT("OutputHash.txt"));
		FFileHelper::SaveStringToFile(CurrentJob->Output.OutputHash.ToString(), *HashFileName, FFileHelper::EEncodingOptions::ForceAnsi);
	}

	// Support dumping debug info for only failed compilations or those with warnings
	if (GShaderCompilingManager->ShouldRecompileToDumpShaderDebugInfo(*CurrentJob))
	{
		// Build debug info path and create the directory if it doesn't already exist
		CurrentJob->Input.DumpDebugInfoPath = GShaderCompilingManager->CreateShaderDebugInfoPath(CurrentJob->Input);
		return true;
	}

	return false;
}

static FString GetSingleJobCompilationDump(const FShaderCompileJob* SingleJob)
{
	if (!SingleJob)
	{
		return TEXT("Internal error, not a Job!");
	}
	FString String = SingleJob->Input.GenerateShaderName();
	if (SingleJob->Key.VFType)
	{
		String += FString::Printf(TEXT(" VF '%s'"), SingleJob->Key.VFType->GetName());
	}
	String += FString::Printf(TEXT(" Type '%s'"), SingleJob->Key.ShaderType->GetName());
	String += FString::Printf(TEXT(" '%s' Entry '%s' Permutation %i "), *SingleJob->Input.VirtualSourceFilePath, *SingleJob->Input.EntryPointName, SingleJob->Key.PermutationId);
	return String;
}


static void DumpCompilationJobs(const TArray<FShaderCommonCompileJobPtr>& QueuedJobs, int32 NumProcessedJobs)
{
	if (NumProcessedJobs == -1)
	{
		UE_LOG(LogShaderCompilers, Error, TEXT("SCW %d Queued Jobs, Unknown number of processed jobs!"), QueuedJobs.Num());
	}
	else
	{
		UE_LOG(LogShaderCompilers, Error, TEXT("SCW %d Queued Jobs, Finished %d single jobs"), QueuedJobs.Num(), NumProcessedJobs);
	}

	for (int32 Index = 0; Index < QueuedJobs.Num(); ++Index)
	{
		if (FShaderCompileJob* SingleJob = QueuedJobs[Index]->GetSingleShaderJob())
		{
			UE_LOG(LogShaderCompilers, Error, TEXT("Job %d [Single] %s"), Index, *GetSingleJobCompilationDump(SingleJob));
		}
		else
		{
			FShaderPipelineCompileJob* PipelineJob = QueuedJobs[Index]->GetShaderPipelineJob();
			UE_LOG(LogShaderCompilers, Error, TEXT("Job %d: Pipeline %s "), Index, PipelineJob->Key.ShaderPipeline->GetName());
			for (int32 JobIndex = 0; JobIndex < PipelineJob->StageJobs.Num(); ++JobIndex)
			{
				UE_LOG(LogShaderCompilers, Error, TEXT("PipelineJob %d %s"), JobIndex, *GetSingleJobCompilationDump(PipelineJob->StageJobs[JobIndex]->GetSingleShaderJob()));
			}
		}
	}

	// Force a log flush so we can track the crash before the cooker potentially crashes before the output shows up
	GLog->Flush();
}

// Disable optimization for this crash handler to get full access to the entire stack frame when debugging a crash dump
PRAGMA_DISABLE_OPTIMIZATION;
static void HandleWorkerCrash(const TArray<FShaderCommonCompileJobPtr>& QueuedJobs, FArchive& OutputFile, int32 OutputVersion, int64 FileSize, ESCWErrorCode ErrorCode, int32 NumProcessedJobs, int32 CallstackLength, int32 ExceptionInfoLength)
{
	TArray<TCHAR> Callstack;
	Callstack.AddUninitialized(CallstackLength + 1);
	OutputFile.Serialize(Callstack.GetData(), CallstackLength * sizeof(TCHAR));
	Callstack[CallstackLength] = 0;

	TArray<TCHAR> ExceptionInfo;
	ExceptionInfo.AddUninitialized(ExceptionInfoLength + 1);
	OutputFile.Serialize(ExceptionInfo.GetData(), ExceptionInfoLength * sizeof(TCHAR));
	ExceptionInfo[ExceptionInfoLength] = 0;

	// Store primary job information onto stack to make it part of a crash dump
	static const int32 MaxNumCharsForSourcePaths = 8192;
	int32 JobInputSourcePathsLength = 0;
	ANSICHAR JobInputSourcePaths[MaxNumCharsForSourcePaths];
	JobInputSourcePaths[0] = 0;

	auto WriteInputSourcePathOntoStack = [&JobInputSourcePathsLength, &JobInputSourcePaths](const ANSICHAR* InputSourcePath)
	{
		if (InputSourcePath != nullptr && JobInputSourcePathsLength + 3 < MaxNumCharsForSourcePaths)
		{
			// Copy input source path into stack buffer
			int32 InputSourcePathLength = FMath::Min(FCStringAnsi::Strlen(InputSourcePath), (MaxNumCharsForSourcePaths - JobInputSourcePathsLength - 2));
			FMemory::Memcpy(JobInputSourcePaths + JobInputSourcePathsLength, InputSourcePath, InputSourcePathLength);

			// Write newline character and put NUL character at the end
			JobInputSourcePathsLength += InputSourcePathLength;
			JobInputSourcePaths[JobInputSourcePathsLength] = TEXT('\n');
			++JobInputSourcePathsLength;
			JobInputSourcePaths[JobInputSourcePathsLength] = 0;
		}
	};

	auto StoreInputDebugInfo = [&WriteInputSourcePathOntoStack, &JobInputSourcePathsLength, &JobInputSourcePaths](const FShaderCompilerInput& Input)
	{
		FString DebugInfo = FString::Printf(TEXT("%s:%s"), *Input.VirtualSourceFilePath, *Input.EntryPointName);
		WriteInputSourcePathOntoStack(TCHAR_TO_UTF8(*DebugInfo));
	};

	for (auto CommonJob : QueuedJobs)
	{
		if (FShaderCompileJob* SingleJob = CommonJob->GetSingleShaderJob())
		{
			StoreInputDebugInfo(SingleJob->Input);
		}
		else if (FShaderPipelineCompileJob* PipelineJob = CommonJob->GetShaderPipelineJob())
		{
			for (int32 Job = 0; Job < PipelineJob->StageJobs.Num(); ++Job)
			{
				if (FShaderCompileJob* SingleStageJob = PipelineJob->StageJobs[Job])
				{
					StoreInputDebugInfo(SingleStageJob->Input);
				}
			}
		}
	}

	// One entry per error code as we want to have different callstacks for crash reporter...
	switch (ErrorCode)
	{
	default:
	case ESCWErrorCode::GeneralCrash:
	{
		DumpCompilationJobs(QueuedJobs, NumProcessedJobs);
		SCWErrorCode::HandleGeneralCrash(ExceptionInfo.GetData(), Callstack.GetData());
	}
	break;
	case ESCWErrorCode::BadShaderFormatVersion:
		SCWErrorCode::HandleBadShaderFormatVersion(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::BadInputVersion:
		SCWErrorCode::HandleBadInputVersion(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::BadSingleJobHeader:
		SCWErrorCode::HandleBadSingleJobHeader(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::BadPipelineJobHeader:
		SCWErrorCode::HandleBadPipelineJobHeader(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::CantDeleteInputFile:
		SCWErrorCode::HandleCantDeleteInputFile(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::CantSaveOutputFile:
		SCWErrorCode::HandleCantSaveOutputFile(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::NoTargetShaderFormatsFound:
		SCWErrorCode::HandleNoTargetShaderFormatsFound(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::CantCompileForSpecificFormat:
		SCWErrorCode::HandleCantCompileForSpecificFormat(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::CrashInsidePlatformCompiler:
		DumpCompilationJobs(QueuedJobs, NumProcessedJobs);
		SCWErrorCode::HandleCrashInsidePlatformCompiler(ExceptionInfo.GetData());
		break;
	case ESCWErrorCode::Success:
		// Can't get here...
		break;
	}
}
PRAGMA_ENABLE_OPTIMIZATION;

// Process results from Worker Process
void FShaderCompileUtilities::DoReadTaskResults(const TArray<FShaderCommonCompileJobPtr>& QueuedJobs, FArchive& OutputFile)
{
	if (OutputFile.TotalSize() == 0)
	{
		SCWErrorCode::HandleOutputFileEmpty(*OutputFile.GetArchiveName());
	}

	int32 OutputVersion = ShaderCompileWorkerOutputVersion;
	OutputFile << OutputVersion;

	if (ShaderCompileWorkerOutputVersion != OutputVersion)
	{
		FString Text = FString::Printf(TEXT("Expecting ShaderCompileWorker output version %d, got %d instead! Forgot to build ShaderCompileWorker?"), ShaderCompileWorkerOutputVersion, OutputVersion);
		ModalErrorOrLog(Text);
	}

	int64 FileSize = 0;
	OutputFile << FileSize;

	// Check for corrupted output file
	if (FileSize > OutputFile.TotalSize())
	{
		SCWErrorCode::HandleOutputFileCorrupted(*OutputFile.GetArchiveName(), FileSize, OutputFile.TotalSize());
	}

	int32 ErrorCode = 0;
	OutputFile << ErrorCode;

	int32 NumProcessedJobs = 0;
	OutputFile << NumProcessedJobs;

	int32 CallstackLength = 0;
	OutputFile << CallstackLength;

	int32 ExceptionInfoLength = 0;
	OutputFile << ExceptionInfoLength;

	// Worker crashed
	if (ESCWErrorCode(ErrorCode) != ESCWErrorCode::Success)
	{
		HandleWorkerCrash(QueuedJobs, OutputFile, OutputVersion, FileSize, ESCWErrorCode(ErrorCode), NumProcessedJobs, CallstackLength, ExceptionInfoLength);
	}

	TArray<FShaderCompileJob*> QueuedSingleJobs;
	TArray<FShaderPipelineCompileJob*> QueuedPipelineJobs;
	SplitJobsByType(QueuedJobs, QueuedSingleJobs, QueuedPipelineJobs);
	TArray<FShaderCompileJob*> ReissueSourceJobs;

	// Read single jobs
	{
		int32 SingleJobHeader = -1;
		OutputFile << SingleJobHeader;
		if (SingleJobHeader != ShaderCompileWorkerSingleJobHeader)
		{
			FString Text = FString::Printf(TEXT("Expecting ShaderCompileWorker Single Jobs %d, got %d instead! Forgot to build ShaderCompileWorker?"), ShaderCompileWorkerSingleJobHeader, SingleJobHeader);
			ModalErrorOrLog(Text, OutputFile.Tell(), FileSize);
		}

		int32 NumJobs;
		OutputFile << NumJobs;
		if (NumJobs != QueuedSingleJobs.Num())
		{
			FString Text = FString::Printf(TEXT("ShaderCompileWorker returned %u single jobs, %u expected"), NumJobs, QueuedSingleJobs.Num());
			ModalErrorOrLog(Text, OutputFile.Tell(), FileSize);
		}
		else
		{
			for (int32 JobIndex = 0; JobIndex < NumJobs; JobIndex++)
			{
				auto* CurrentJob = QueuedSingleJobs[JobIndex];
				if (ReadSingleJob(CurrentJob, OutputFile))
				{
					ReissueSourceJobs.Add(CurrentJob);
				}
			}
		}
	}

	// Pipeline jobs
	{
		int32 PipelineJobHeader = -1;
		OutputFile << PipelineJobHeader;
		if (PipelineJobHeader != ShaderCompileWorkerPipelineJobHeader)
		{
			FString Text = FString::Printf(TEXT("Expecting ShaderCompileWorker Pipeline Jobs %d, got %d instead! Forgot to build ShaderCompileWorker?"), ShaderCompileWorkerPipelineJobHeader, PipelineJobHeader);
			ModalErrorOrLog(Text, OutputFile.Tell(), FileSize);
		}

		int32 NumJobs;
		OutputFile << NumJobs;
		if (NumJobs != QueuedPipelineJobs.Num())
		{
			FString Text = FString::Printf(TEXT("Worker returned %u pipeline jobs, %u expected"), NumJobs, QueuedPipelineJobs.Num());
			ModalErrorOrLog(Text, OutputFile.Tell(), FileSize);
		}
		else
		{
			for (int32 JobIndex = 0; JobIndex < NumJobs; JobIndex++)
			{
				FShaderPipelineCompileJob* CurrentJob = QueuedPipelineJobs[JobIndex];

				FString PipelineName;
				OutputFile << PipelineName;
				if (PipelineName != CurrentJob->Key.ShaderPipeline->GetName())
				{
					FString Text = FString::Printf(TEXT("Worker returned Pipeline %s, expected %s!"), *PipelineName, CurrentJob->Key.ShaderPipeline->GetName());
					ModalErrorOrLog(Text, OutputFile.Tell(), FileSize);
				}

				check(!CurrentJob->bFinalized);
				CurrentJob->bFinalized = true;
				CurrentJob->bFailedRemovingUnused = false;

				int32 NumStageJobs = -1;
				OutputFile << NumStageJobs;

				if (NumStageJobs != CurrentJob->StageJobs.Num())
				{
					FString Text = FString::Printf(TEXT("Worker returned %u stage pipeline jobs, %u expected"), NumStageJobs, CurrentJob->StageJobs.Num());
					ModalErrorOrLog(Text, OutputFile.Tell(), FileSize);
				}
				else
				{
					CurrentJob->bSucceeded = true;
					for (int32 Index = 0; Index < NumStageJobs; Index++)
					{
						FShaderCompileJob* SingleJob = CurrentJob->StageJobs[Index];
						// cannot reissue a single stage of a pipeline job
						ReadSingleJob(SingleJob, OutputFile);
						CurrentJob->bFailedRemovingUnused = CurrentJob->bFailedRemovingUnused || SingleJob->Output.bFailedRemovingUnused;
						CurrentJob->bSucceeded = CurrentJob->bSucceeded && SingleJob->bSucceeded;
					}
				}
			}
		}
	}
	
	// Requeue any jobs we wish to run again
	if (ReissueSourceJobs.Num())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReissueShaderJobs);

		TArray<FShaderCommonCompileJobPtr> ReissueJobs;
		ReissueJobs.Reserve(ReissueSourceJobs.Num());
		const uint32 JobId = FShaderCommonCompileJob::GetNextJobId();
		for (const FShaderCompileJob* ReissueSourceJob : ReissueSourceJobs)
		{
			FShaderCompileJob* ReissueJob = GShaderCompilingManager->PrepareShaderCompileJob(JobId, ReissueSourceJob->Key, ReissueSourceJob->Priority);
			if (ReissueJob)
			{
				ReissueJob->Input = ReissueSourceJob->Input;
				ReissueJobs.Add(FShaderCommonCompileJobPtr(ReissueJob));
			}
		}

		GShaderCompilingManager->SubmitJobs(ReissueJobs, FString(""), FString(""));
	}
}

static bool CheckSingleJob(const FShaderCompileJob& SingleJob, TArray<FString>& OutErrors, FString* OutDumpedSource)
{
	if (SingleJob.bSucceeded)
	{
		checkf(SingleJob.Output.ShaderCode.GetShaderCodeSize() > 0, TEXT("Abnormal shader code size for a succeded job: %d bytes"), SingleJob.Output.ShaderCode.GetShaderCodeSize());
	}

	if (GShowShaderWarnings || !SingleJob.bSucceeded)
	{
		for (int32 ErrorIndex = 0; ErrorIndex < SingleJob.Output.Errors.Num(); ErrorIndex++)
		{
			const FShaderCompilerError& InError = SingleJob.Output.Errors[ErrorIndex];
			OutErrors.AddUnique(InError.GetErrorStringWithLineMarker());
		}
	}

	bool bSucceeded = SingleJob.bSucceeded;

	if (SingleJob.Key.ShaderType)
	{
		// Allow the shader validation to fail the compile if it sees any parameters bound that aren't supported.
		const bool bValidationResult = SingleJob.Key.ShaderType->ValidateCompiledResult(
			(EShaderPlatform)SingleJob.Input.Target.Platform,
			SingleJob.Output.ParameterMap,
			OutErrors);
		bSucceeded = bValidationResult && bSucceeded;
	}

	if (SingleJob.Key.VFType)
	{
		const int32 OriginalNumErrors = OutErrors.Num();

		// Allow the vertex factory to fail the compile if it sees any parameters bound that aren't supported
		SingleJob.Key.VFType->ValidateCompiledResult((EShaderPlatform)SingleJob.Input.Target.Platform, SingleJob.Output.ParameterMap, OutErrors);

		if (OutErrors.Num() > OriginalNumErrors)
		{
			bSucceeded = false;
		}
	}

	if (!bSucceeded && OutDumpedSource != nullptr)
	{
		*OutDumpedSource = SingleJob.Output.OptionalPreprocessedShaderSource;
	}

	return bSucceeded;
};

static void AddErrorsForFailedJob(FShaderCompileJob& CurrentJob, TArray<EShaderPlatform>& ErrorPlatforms, TArray<FString>& UniqueErrors, TArray<FShaderCommonCompileJob*>& ErrorJobs)
{
	ErrorPlatforms.AddUnique((EShaderPlatform)CurrentJob.Input.Target.Platform);

	if (CurrentJob.Output.Errors.Num() == 0)
	{
		// Job hard crashed
		FShaderCompilerError Error(*(FString("Internal Error!\n\t") + GetSingleJobCompilationDump(&CurrentJob)));
		CurrentJob.Output.Errors.Add(Error);
	}

	for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
	{
		const FShaderCompilerError& CurrentError = CurrentJob.Output.Errors[ErrorIndex];

		// Include warnings if LogShaders is unsuppressed, otherwise only include errors
		if (UE_LOG_ACTIVE(LogShaders, Log) || CurrentError.StrippedErrorMessage.Contains(TEXT("error")))
		{
			UniqueErrors.AddUnique(CurrentJob.Output.Errors[ErrorIndex].GetErrorString());
			ErrorJobs.AddUnique(&CurrentJob);
		}
	}
}

/** Information tracked for each shader compile worker process instance. */
struct FShaderCompileWorkerInfo
{
	/** Process handle of the worker app once launched.  Invalid handle means no process. */
	FProcHandle WorkerProcess;

	/** Tracks whether tasks have been issued to the worker. */
	bool bIssuedTasksToWorker;	

	/** Whether the worker has been launched for this set of tasks. */
	bool bLaunchedWorker;

	/** Tracks whether all tasks issued to the worker have been received. */
	bool bComplete;

	/** Whether this worker is available for new jobs. It will be false when shutting down the worker. */
	bool bAvailable; 

	/** Time at which the worker started the most recent batch of tasks. */
	double StartTime;

	/** Time at which the worker ended the most recent batch of tasks. */
	double FinishTime = 0.0;

	/** Jobs that this worker is responsible for compiling. */
	TArray<FShaderCommonCompileJobPtr> QueuedJobs;

	FShaderCompileWorkerInfo() :
		bIssuedTasksToWorker(false),		
		bLaunchedWorker(false),
		bComplete(false),
		bAvailable(true),
		StartTime(0)
	{
	}

	// warning: not virtual
	~FShaderCompileWorkerInfo()
	{
		if(WorkerProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(WorkerProcess);
			FPlatformProcess::CloseProc(WorkerProcess);
		}
	}
};

FShaderCompileThreadRunnableBase::FShaderCompileThreadRunnableBase(FShaderCompilingManager* InManager)
	: Manager(InManager)
	, Thread(nullptr)
	, MinPriorityIndex(0)
	, MaxPriorityIndex(NumShaderCompileJobPriorities - 1)
	, bForceFinish(false)
{
}
void FShaderCompileThreadRunnableBase::StartThread()
{
	if (Manager->bAllowAsynchronousShaderCompiling && !FPlatformProperties::RequiresCookedData())
	{
		Thread = FRunnableThread::Create(this, TEXT("ShaderCompilingThread"), 0, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
	}
}

FShaderCompileThreadRunnable::FShaderCompileThreadRunnable(FShaderCompilingManager* InManager)
	: FShaderCompileThreadRunnableBase(InManager)
	, LastCheckForWorkersTime(0)
{
	for (uint32 WorkerIndex = 0; WorkerIndex < Manager->NumShaderCompilingThreads; WorkerIndex++)
	{
		WorkerInfos.Add(MakeUnique<FShaderCompileWorkerInfo>());
	}
}

FShaderCompileThreadRunnable::~FShaderCompileThreadRunnable()
{
	FScopeLock WorkerScopeLock(&WorkerInfosLock);
	WorkerInfos.Empty();
}

void FShaderCompileThreadRunnable::OnMachineResourcesChanged()
{
	bool bWaitForWorkersToShutdown = false;
	{
		FScopeLock WorkerScopeLock(&WorkerInfosLock);
		// Set all bAvailable flags back to true
		for (TUniquePtr< FShaderCompileWorkerInfo>& WorkerInfo : WorkerInfos)
		{
			WorkerInfo->bAvailable = true;
		}

		if (Manager->NumShaderCompilingThreads >= static_cast<uint32>(WorkerInfos.Num()))
		{
			while (static_cast<uint32>(WorkerInfos.Num()) < Manager->NumShaderCompilingThreads)
			{
				WorkerInfos.Add(MakeUnique<FShaderCompileWorkerInfo>());
			}
		}
		else
		{
			for (int32 Index = 0; Index < WorkerInfos.Num(); ++Index)
			{
				FShaderCompileWorkerInfo& WorkerInfo = *WorkerInfos[Index];
				bool bReadyForShutdown = WorkerInfo.QueuedJobs.Num() == 0;
				if (bReadyForShutdown)
				{
					WorkerInfos.RemoveAtSwap(Index--);
					if (WorkerInfos.Num() == Manager->NumShaderCompilingThreads)
					{
						break;
					}
				}
			}
			bWaitForWorkersToShutdown = Manager->NumShaderCompilingThreads < static_cast<uint32>(WorkerInfos.Num());
			for (int32 Index = WorkerInfos.Num() - 1;
				static_cast<uint32>(Index) >= Manager->NumShaderCompilingThreads; --Index)
			{
				WorkerInfos[Index]->bAvailable = false;
			}
		}
	}
	const double StartTime = FPlatformTime::Seconds();
	constexpr float MaxDurationToWait = 60.f;
	const double MaxTimeToWait = StartTime + MaxDurationToWait;
	while (bWaitForWorkersToShutdown)
	{
		FPlatformProcess::Sleep(0.01f);
		const double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime > MaxTimeToWait)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("OnMachineResourcesChanged timedout waiting %.0f seconds for WorkerInfos to complete. Workers will remain allocated."),
				(float)(CurrentTime - StartTime));
			break;
		}

		FScopeLock WorkerScopeLock(&WorkerInfosLock);
		for (int32 Index = WorkerInfos.Num() - 1;
			static_cast<uint32>(Index) >= Manager->NumShaderCompilingThreads; --Index)
		{
			FShaderCompileWorkerInfo& WorkerInfo = *WorkerInfos[Index];
			check(!WorkerInfos[Index]->bAvailable); // It should still be set to false from when we changed it above
			bool bReadyForShutdown = WorkerInfo.QueuedJobs.Num() == 0;
			if (bReadyForShutdown)
			{
				WorkerInfos.RemoveAtSwap(Index);
			}
		}
		bWaitForWorkersToShutdown = Manager->NumShaderCompilingThreads < static_cast<uint32>(WorkerInfos.Num());
	}
}

/** Entry point for the shader compiling thread. */
uint32 FShaderCompileThreadRunnableBase::Run()
{
	check(Manager->bAllowAsynchronousShaderCompiling);
	while (!bForceFinish)
	{
		CompilingLoop();
	}
	UE_LOG(LogShaderCompilers, Display, TEXT("Shaders left to compile 0"));

	return 0;
}

int32 FShaderCompileThreadRunnable::PullTasksFromQueue()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompileThreadRunnable::PullTasksFromQueue);

	FScopeLock WorkerScopeLock(&WorkerInfosLock); // Must be entered before CompileQueueSection
	int32 NumActiveThreads = 0;
	int32 NumJobsStarted[NumShaderCompileJobPriorities] = { 0 };
	{
		// Enter the critical section so we can access the input and output queues
		FScopeLock Lock(&Manager->CompileQueueSection);

		const int32 NumWorkersToFeed = Manager->bCompilingDuringGame ? Manager->NumShaderCompilingThreadsDuringGame : WorkerInfos.Num();

		for (int32 PriorityIndex = MaxPriorityIndex; PriorityIndex >= MinPriorityIndex; --PriorityIndex)
		{
			int32 NumPendingJobs = Manager->AllJobs.GetNumPendingJobs((EShaderCompileJobPriority)PriorityIndex);
			// Try to distribute the work evenly between the workers
			const auto NumJobsPerWorker = (NumPendingJobs / NumWorkersToFeed) + 1;

			for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
			{
				FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

				// If this worker doesn't have any queued jobs, look for more in the input queue
				if (CurrentWorkerInfo.QueuedJobs.Num() == 0 && CurrentWorkerInfo.bAvailable && WorkerIndex < NumWorkersToFeed)
				{
					check(!CurrentWorkerInfo.bComplete);

					NumPendingJobs = Manager->AllJobs.GetNumPendingJobs((EShaderCompileJobPriority)PriorityIndex);
					if (NumPendingJobs > 0)
					{
						UE_LOG(LogShaderCompilers, Verbose, TEXT("Worker (%d/%d): shaders left to compile %i"), WorkerIndex + 1, WorkerInfos.Num(), NumPendingJobs);

						int32 MaxNumJobs = 1;
						// high priority jobs go in 1 per "batch", unless the engine is still starting up
						if (PriorityIndex < (int32)EShaderCompileJobPriority::High || Manager->IgnoreAllThrottling())
						{
							MaxNumJobs = FMath::Min3(NumJobsPerWorker, NumPendingJobs, Manager->MaxShaderJobBatchSize);
						}

						NumJobsStarted[PriorityIndex] += Manager->AllJobs.GetPendingJobs(EShaderCompilerWorkerType::LocalThread, (EShaderCompileJobPriority)PriorityIndex, 1, MaxNumJobs, CurrentWorkerInfo.QueuedJobs);

						// Update the worker state as having new tasks that need to be issued					
						// don't reset worker app ID, because the shadercompileworkers don't shutdown immediately after finishing a single job queue.
						CurrentWorkerInfo.bIssuedTasksToWorker = false;
						CurrentWorkerInfo.bLaunchedWorker = false;
						CurrentWorkerInfo.StartTime = FPlatformTime::Seconds();
						NumActiveThreads++;

						if (CurrentWorkerInfo.FinishTime > 0.0)
						{
							const double WorkerIdleTime = CurrentWorkerInfo.StartTime - CurrentWorkerInfo.FinishTime;
							GShaderCompilerStats->RegisterLocalWorkerIdleTime(WorkerIdleTime);
							if (Manager->bLogJobCompletionTimes)
							{
								UE_LOG(LogShaderCompilers, Display, TEXT("  Worker (%d/%d) started working after being idle for %fs"), WorkerIndex + 1, WorkerInfos.Num(), WorkerIdleTime);
							}
						}
					}
				}
			}
		}
	}

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		if (WorkerInfos[WorkerIndex]->QueuedJobs.Num() > 0)
		{
			NumActiveThreads++;
		}
	}

	for (int32 PriorityIndex = 0; PriorityIndex < NumShaderCompileJobPriorities; ++PriorityIndex)
	{
		if (NumJobsStarted[PriorityIndex] > 0)
		{
			UE_LOG(LogShaderCompilers, Verbose, TEXT("Started %d 'Local' shader compile jobs with '%s' priority"),
				NumJobsStarted[PriorityIndex],
				ShaderCompileJobPriorityToString((EShaderCompileJobPriority)PriorityIndex));
		}
	}

	return NumActiveThreads;
}

void FShaderCompileThreadRunnable::PushCompletedJobsToManager()
{
	FScopeLock WorkerScopeLock(&WorkerInfosLock); // Must be entered before CompileQueueSection

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// Add completed jobs to the output queue, which is ShaderMapJobs
		if (CurrentWorkerInfo.bComplete)
		{
			// Enter the critical section so we can access the input and output queues
			FScopeLock Lock(&Manager->CompileQueueSection);

			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				auto& Job = CurrentWorkerInfo.QueuedJobs[JobIndex];

				Manager->ProcessFinishedJob(Job.GetReference());
			}

			const float ElapsedTime = FPlatformTime::Seconds() - CurrentWorkerInfo.StartTime;

			Manager->WorkersBusyTime += ElapsedTime;
			COOK_STAT(ShaderCompilerCookStats::AsyncCompileTimeSec += ElapsedTime);

			// Log if requested or if there was an exceptionally slow batch, to see the offender easily
			if (Manager->bLogJobCompletionTimes || ElapsedTime > 60.0f)
			{
				TArray<FShaderCommonCompileJobPtr> SortedJobs = CurrentWorkerInfo.QueuedJobs;
				SortedJobs.Sort([](const FShaderCommonCompileJobPtr& JobA, const FShaderCommonCompileJobPtr& JobB)
					{
						const FShaderCompileJob* SingleJobA = JobA->GetSingleShaderJob();
						const FShaderCompileJob* SingleJobB = JobB->GetSingleShaderJob();

						const float TimeA = SingleJobA ? SingleJobA->Output.CompileTime : 0.0f;
						const float TimeB = SingleJobB ? SingleJobB->Output.CompileTime : 0.0f;

						return TimeA > TimeB;
					});

				FString JobNames;

				for (int32 JobIndex = 0; JobIndex < SortedJobs.Num(); JobIndex++)
				{
					const FShaderCommonCompileJob& Job = *SortedJobs[JobIndex];
					if (const FShaderCompileJob* SingleJob = Job.GetSingleShaderJob())
					{
						const TCHAR* JobName = Manager->bLogJobCompletionTimes ? *SingleJob->Input.DebugGroupName : SingleJob->Key.ShaderType->GetName();
						JobNames += FString::Printf(TEXT("%s [WorkerTime=%.3fs]"), JobName, SingleJob->Output.CompileTime);
					}
					else
					{
						const FShaderPipelineCompileJob* PipelineJob = Job.GetShaderPipelineJob();
						JobNames += FString(PipelineJob->Key.ShaderPipeline->GetName());
						if (PipelineJob->bFailedRemovingUnused)
						{
							JobNames += FString(TEXT("(failed to optimize)"));
						}
					}
					if (JobIndex < SortedJobs.Num() - 1)
					{
						JobNames += TEXT(", ");
					}
				}

				UE_LOG(LogShaderCompilers, Display, TEXT("Worker (%d/%d) finished batch of %u jobs in %.3fs, %s"), WorkerIndex + 1, WorkerInfos.Num(), SortedJobs.Num(), ElapsedTime, *JobNames);
			}

			CurrentWorkerInfo.FinishTime = FPlatformTime::Seconds();
			CurrentWorkerInfo.bComplete = false;
			CurrentWorkerInfo.QueuedJobs.Empty();
		}
	}
}

void FShaderCompileThreadRunnable::WriteNewTasks()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ShaderCompiler.WriteNewTasks);
	FScopeLock WorkerScopeLock(&WorkerInfosLock);

	// first, a quick check if anything is needed just to avoid hammering the task graph
	bool bHasTasksToWrite = false;
	for (int32 WorkerIndex = 0, NumWorkers = WorkerInfos.Num(); WorkerIndex < NumWorkers; ++WorkerIndex)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];
		if (!CurrentWorkerInfo.bIssuedTasksToWorker && CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			bHasTasksToWrite = true;
			break;
		}
	}

	if (!bHasTasksToWrite)
	{
		return;
	}


	auto LoopBody = [this](int32 WorkerIndex)
	{
		// The calling thread holds the WorkerInfosLock and will not modify WorkerInfos, 
		// so we can access it here without entering the lock
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// Only write tasks once
		if (!CurrentWorkerInfo.bIssuedTasksToWorker && CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ShaderCompiler.WriteNewTasksForWorker);
			CurrentWorkerInfo.bIssuedTasksToWorker = true;

			const FString WorkingDirectory = Manager->AbsoluteShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex);

			// To make sure that the process waiting for input file won't try to read it until it's ready
			// we use a temp file name during writing.
			FString TransferFileName;
			do
			{
				FGuid Guid;
				FPlatformMisc::CreateGuid(Guid);
				TransferFileName = WorkingDirectory + Guid.ToString();
			} while (IFileManager::Get().FileSize(*TransferFileName) != INDEX_NONE);

			// Write out the file that the worker app is waiting for, which has all the information needed to compile the shader.
			// 'Only' indicates that the worker should keep checking for more tasks after this one
			FArchive* TransferFile = nullptr;

			int32 RetryCount = 0;
			// Retry over the next two seconds if we can't write out the input file
			// Anti-virus and indexing applications can interfere and cause this write to fail
			//@todo - switch to shared memory or some other method without these unpredictable hazards
			while (TransferFile == nullptr && RetryCount < 2000)
			{
				if (RetryCount > 0)
				{
					FPlatformProcess::Sleep(0.01f);
				}
				TransferFile = IFileManager::Get().CreateFileWriter(*TransferFileName, FILEWRITE_EvenIfReadOnly);
				RetryCount++;
				if (TransferFile == nullptr)
				{
					UE_LOG(LogShaderCompilers, Warning, TEXT("Could not create the shader compiler transfer file '%s', retrying..."), *TransferFileName);
				}
			}
			if (TransferFile == nullptr)
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Could not create the shader compiler transfer file '%s'."), *TransferFileName);
			}
			check(TransferFile);

			GShaderCompilerStats->RegisterJobBatch(CurrentWorkerInfo.QueuedJobs.Num(), FShaderCompilerStats::EExecutionType::Local);
			if (!FShaderCompileUtilities::DoWriteTasks(CurrentWorkerInfo.QueuedJobs, *TransferFile))
			{
				uint64 TotalDiskSpace = 0;
				uint64 FreeDiskSpace = 0;
				FPlatformMisc::GetDiskTotalAndFreeSpace(TransferFileName, TotalDiskSpace, FreeDiskSpace);
				UE_LOG(LogShaderCompilers, Error, TEXT("Could not write the shader compiler transfer filename to '%s' (Free Disk Space: %llu."), *TransferFileName, FreeDiskSpace);
			}
			delete TransferFile;

#if 0 // debugging code to dump the worker inputs
			static FCriticalSection ArchiveLock;
			{
				FScopeLock Locker(&ArchiveLock);
				static int ArchivedTransferFileNum = 0;
				FString JobCacheDir = ShaderCompiler::IsJobCacheEnabled() ? TEXT("JobCache") : TEXT("NoJobCache");
				FString ArchiveDir = FPaths::ProjectSavedDir() / TEXT("ArchivedWorkerInputs") / JobCacheDir;
				FString ArchiveName = FString::Printf(TEXT("Input-%d"), ArchivedTransferFileNum++);
				FString ArchivePath = ArchiveDir / ArchiveName;
				if (!IFileManager::Get().Copy(*ArchivePath, *TransferFileName))
				{
					UE_LOG(LogInit, Error, TEXT("Could not copy file %s to %s"), *TransferFileName, *ArchivePath);
					ensure(false);
				}
			}
#endif

			// Change the transfer file name to proper one
			FString ProperTransferFileName = WorkingDirectory / TEXT("WorkerInputOnly.in");
			if (!IFileManager::Get().Move(*ProperTransferFileName, *TransferFileName))
			{
				uint64 TotalDiskSpace = 0;
				uint64 FreeDiskSpace = 0;
				FPlatformMisc::GetDiskTotalAndFreeSpace(TransferFileName, TotalDiskSpace, FreeDiskSpace);
				UE_LOG(LogShaderCompilers, Error, TEXT("Could not rename the shader compiler transfer filename to '%s' from '%s' (Free Disk Space: %llu)."), *ProperTransferFileName, *TransferFileName, FreeDiskSpace);
			}
		}
	};

	if (bParallelizeIO)
	{
		ParallelFor( TEXT("ShaderCompiler.WriteNewTasks.PF"), WorkerInfos.Num(),1, LoopBody, EParallelForFlags::Unbalanced);
	}
	else
	{
		double StartIOWork = FPlatformTime::Seconds();
		for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
		{
			LoopBody(WorkerIndex);
		}

		double IODuration = FPlatformTime::Seconds() - StartIOWork;
		if (IODuration > GShaderCompilerTooLongIOThresholdSeconds)
		{
			UE_LOG(LogShaderCompilers, Display, TEXT("FShaderCompileThreadRunnable::WriteNewTasks()() took too long (%.3f seconds, threshold is %.3f s), will parallelize next time."), IODuration, GShaderCompilerTooLongIOThresholdSeconds);
			bParallelizeIO = true;
		}
	}
}

bool FShaderCompileThreadRunnable::LaunchWorkersIfNeeded()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompileThreadRunnable::LaunchWorkersIfNeeded);

	const double CurrentTime = FPlatformTime::Seconds();
	// Limit how often we check for workers running since IsApplicationRunning eats up some CPU time on Windows
	const bool bCheckForWorkerRunning = (CurrentTime - LastCheckForWorkersTime > .1f);
	bool bAbandonWorkers = false;
	uint32_t NumberLaunched = 0;

	if (bCheckForWorkerRunning)
	{
		LastCheckForWorkersTime = CurrentTime;
	}

	FScopeLock WorkerScopeLock(&WorkerInfosLock);
	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];
		if (CurrentWorkerInfo.QueuedJobs.Num() == 0)
		{
			// Skip if nothing to do
			// Also, use the opportunity to free OS resources by cleaning up handles of no more running processes
			if (CurrentWorkerInfo.WorkerProcess.IsValid() && !FShaderCompilingManager::IsShaderCompilerWorkerRunning(CurrentWorkerInfo.WorkerProcess))
			{
				FPlatformProcess::CloseProc(CurrentWorkerInfo.WorkerProcess);
				CurrentWorkerInfo.WorkerProcess = FProcHandle();
			}
			continue;
		}

		if (!CurrentWorkerInfo.WorkerProcess.IsValid() || (bCheckForWorkerRunning && !FShaderCompilingManager::IsShaderCompilerWorkerRunning(CurrentWorkerInfo.WorkerProcess)))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompileThreadRunnable::LaunchingWorkers);

			// @TODO: dubious design - worker should not be launched unless we know there's more work to do.
			bool bLaunchAgain = true;

			// Detect when the worker has exited due to fatal error
			// bLaunchedWorker check here is necessary to distinguish between 'process isn't running because it crashed' and 'process isn't running because it exited cleanly and the outputfile was already consumed'
			if (CurrentWorkerInfo.WorkerProcess.IsValid())
			{
				// shader compiler exited one way or another, so clear out the stale PID.
				FPlatformProcess::CloseProc(CurrentWorkerInfo.WorkerProcess);
				CurrentWorkerInfo.WorkerProcess = FProcHandle();

				if (CurrentWorkerInfo.bLaunchedWorker)
				{
					const FString WorkingDirectory = Manager->AbsoluteShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex) + TEXT("/");
					const FString OutputFileNameAndPath = WorkingDirectory + TEXT("WorkerOutputOnly.out");

					if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutputFileNameAndPath))
					{
						// If the worker is no longer running but it successfully wrote out the output, no need to assert
						bLaunchAgain = false;
					}
					else
					{
						UE_LOG(LogShaderCompilers, Error, TEXT("ShaderCompileWorker terminated unexpectedly!  Falling back to directly compiling which will be very slow.  Thread %u."), WorkerIndex);
						DumpCompilationJobs(CurrentWorkerInfo.QueuedJobs, -1);

						bAbandonWorkers = true;
						break;
					}
				}
			}

			if (bLaunchAgain)
			{
				const FString WorkingDirectory = Manager->ShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex) + TEXT("/");
				FString InputFileName(TEXT("WorkerInputOnly.in"));
				FString OutputFileName(TEXT("WorkerOutputOnly.out"));

				// Store the handle with this thread so that we will know not to launch it again
				CurrentWorkerInfo.WorkerProcess = Manager->LaunchWorker(WorkingDirectory, Manager->ProcessId, WorkerIndex, InputFileName, OutputFileName);
				CurrentWorkerInfo.bLaunchedWorker = true;

				NumberLaunched++;
			}
		}
	}

	const double FinishTime = FPlatformTime::Seconds();
	if (NumberLaunched > 0 && (FinishTime - CurrentTime) >= 10.0)
	{
		UE_LOG(LogShaderCompilers, Warning, TEXT("Performance Warning: It took %f seconds to launch %d ShaderCompileWorkers"), FinishTime - CurrentTime, NumberLaunched);
	}

	return bAbandonWorkers;
}

int32 FShaderCompileThreadRunnable::ReadAvailableResults()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ShaderCompiler.ReadAvailableResults);
	int32 NumProcessed = 0;
	FScopeLock WorkerScopeLock(&WorkerInfosLock);

	// first, a quick check if anything is needed just to avoid hammering the task graph
	bool bHasQueuedJobs = false;
	for (int32 WorkerIndex = 0, NumWorkers = WorkerInfos.Num(); WorkerIndex < NumWorkers; ++WorkerIndex)
	{
		if (WorkerInfos[WorkerIndex]->QueuedJobs.Num() > 0)
		{
			bHasQueuedJobs = true;
			break;
		}
	}

	if (!bHasQueuedJobs)
	{
		return NumProcessed;
	}

	auto LoopBody = [this, &NumProcessed](int32 WorkerIndex)
	{
		// The calling thread holds the WorkerInfosLock and will not modify WorkerInfos, 
		// so we can access it here without entering the lock
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// Check for available result files
		if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			// Distributed compiles always use the same directory
			// 'Only' indicates to the worker that it should log and continue checking for the input file after the first one is processed
			TStringBuilder<512> OutputFileNameAndPath;
			OutputFileNameAndPath << Manager->AbsoluteShaderBaseWorkingDirectory << WorkerIndex << TEXT("/WorkerOutputOnly.out");

			// In the common case the output file will not exist, so check for existence before opening
			// This is only a win if FileExists is faster than CreateFileReader, which it is on Windows
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutputFileNameAndPath))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompileThreadRunnable::ProcessOutputFile);

				FArchive* OutputFilePtr = IFileManager::Get().CreateFileReader(*OutputFileNameAndPath, FILEREAD_Silent);

				if (OutputFilePtr)
				{
					FArchive& OutputFile = *OutputFilePtr;
					check(!CurrentWorkerInfo.bComplete);
					FShaderCompileUtilities::DoReadTaskResults(CurrentWorkerInfo.QueuedJobs, OutputFile);

					// Close the output file.
					delete OutputFilePtr;

					// Delete the output file now that we have consumed it, to avoid reading stale data on the next compile loop.
					bool bDeletedOutput = IFileManager::Get().Delete(*OutputFileNameAndPath, true, true);
					int32 RetryCount = 0;
					// Retry over the next two seconds if we couldn't delete it
					while (!bDeletedOutput && RetryCount < 200)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompileThreadRunnable::DeleteOutputFile);

						FPlatformProcess::Sleep(0.01f);
						bDeletedOutput = IFileManager::Get().Delete(*OutputFileNameAndPath, true, true);
						RetryCount++;
					}
					checkf(bDeletedOutput, TEXT("Failed to delete %s!"), *OutputFileNameAndPath);

					CurrentWorkerInfo.bComplete = true;
				}

				FPlatformAtomics::InterlockedIncrement(&NumProcessed);
			}
		}
	};

	if (bParallelizeIO)
	{
		ParallelFor( TEXT("ShaderCompiler.ReadAvailableResults.PF"),WorkerInfos.Num(),1, LoopBody, EParallelForFlags::Unbalanced);
	}
	else 
	{
		double StartIOWork = FPlatformTime::Seconds();
		for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
		{
			LoopBody(WorkerIndex);
		}

		double IODuration = FPlatformTime::Seconds() - StartIOWork;
		if (IODuration > 0.3)
		{
			UE_LOG(LogShaderCompilers, Display, TEXT("FShaderCompileThreadRunnable::WriteNewTasks() took too long (%.3f seconds, threshold is %.3f s), will parallelize next time."), IODuration, GShaderCompilerTooLongIOThresholdSeconds);
			bParallelizeIO = true;
		}
	}

	return NumProcessed;
}

void FShaderCompileThreadRunnable::CompileDirectlyThroughDll()
{
	// If we aren't compiling through workers, so we can just track the serial time here.
	COOK_STAT(FScopedDurationTimer CompileTimer (ShaderCompilerCookStats::AsyncCompileTimeSec));

	FScopeLock WorkerScopeLock(&WorkerInfosLock);
	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				FShaderCommonCompileJob& CurrentJob = *CurrentWorkerInfo.QueuedJobs[JobIndex];
				FShaderCompileUtilities::ExecuteShaderCompileJob(CurrentJob);
			}

			CurrentWorkerInfo.bComplete = true;
		}
	}
}

void FShaderCompileUtilities::ExecuteShaderCompileJob(FShaderCommonCompileJob& Job)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompileUtilities::ExecuteShaderCompileJob);

	check(!Job.bFinalized);

	static ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	auto* SingleJob = Job.GetSingleShaderJob();
	if (SingleJob)
	{
		const FName Format = (SingleJob->Input.ShaderFormat != NAME_None) ? SingleJob->Input.ShaderFormat : LegacyShaderPlatformToShaderFormat(EShaderPlatform(SingleJob->Input.Target.Platform));
		const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);

		if (!Compiler)
		{
			UE_LOG(LogShaderCompilers, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Format.ToString());
		}
		CA_ASSUME(Compiler != nullptr);

		if (IsValidRef(SingleJob->Input.SharedEnvironment))
		{
			// Merge the shared environment into the per-shader environment before calling into the compile function
			// Normally this happens in the worker
			SingleJob->Input.Environment.Merge(*SingleJob->Input.SharedEnvironment);
		}

		// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
		Compiler->CompileShader(Format, SingleJob->Input, SingleJob->Output, FString(FPlatformProcess::ShaderDir()));

		SingleJob->bSucceeded = SingleJob->Output.bSucceeded;

		if (SingleJob->Output.bSucceeded)
		{
			// Generate a hash of the output and compress the code
			// The shader processing this output will use the heash to search for existing FShaderResources
			SingleJob->Output.GenerateOutputHash();
			if (SingleJob->Input.CompressionFormat != NAME_None)
			{
				SingleJob->Output.CompressOutput(SingleJob->Input.CompressionFormat, SingleJob->Input.OodleCompressor, SingleJob->Input.OodleLevel);
			}
		}
	}
	else
	{
		FShaderPipelineCompileJob* PipelineJob = Job.GetShaderPipelineJob();
		check(PipelineJob);

		EShaderPlatform Platform = (EShaderPlatform)PipelineJob->StageJobs[0]->Input.Target.Platform;
		const FName Format = LegacyShaderPlatformToShaderFormat(Platform);
		const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);

		if (!Compiler)
		{
			UE_LOG(LogShaderCompilers, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Format.ToString());
		}
		CA_ASSUME(Compiler != nullptr);

		// Verify same platform on all stages
		for (int32 Index = 1; Index < PipelineJob->StageJobs.Num(); ++Index)
		{
			auto SingleStage = PipelineJob->StageJobs[Index];
			if (!SingleStage)
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Can't nest Shader Pipelines inside Shader Pipeline '%s'!"), PipelineJob->Key.ShaderPipeline->GetName());
			}
			else if (Platform != SingleStage->Input.Target.Platform)
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Mismatched Target Platform %s while compiling Shader Pipeline '%s'."), *Format.GetPlainNameString(), PipelineJob->Key.ShaderPipeline->GetName());
			}
		}

		CompileShaderPipeline(Compiler, Format, PipelineJob, FString(FPlatformProcess::ShaderDir()));
	}

	Job.bFinalized = true;
}

FArchive* FShaderCompileUtilities::CreateFileHelper(const FString& Filename)
{
	// TODO: This logic came from FShaderCompileThreadRunnable::WriteNewTasks().
	// We can't avoid code duplication unless we refactored the local worker too.

	FArchive* File = nullptr;
	int32 RetryCount = 0;
	// Retry over the next two seconds if we can't write out the file.
	// Anti-virus and indexing applications can interfere and cause this to fail.
	while (File == nullptr && RetryCount < 200)
	{
		if (RetryCount > 0)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		File = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_EvenIfReadOnly);
		RetryCount++;
	}
	if (File == nullptr)
	{
		File = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
	}
	checkf(File, TEXT("Failed to create file %s!"), *Filename);
	return File;
}

void FShaderCompileUtilities::MoveFileHelper(const FString& To, const FString& From)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.FileExists(*From))
	{
		FString DirectoryName;
		int32 LastSlashIndex;
		if (To.FindLastChar('/', LastSlashIndex))
		{
			DirectoryName = To.Left(LastSlashIndex);
		} else
		{
			DirectoryName = To;
		}

		// TODO: This logic came from FShaderCompileThreadRunnable::WriteNewTasks().
		// We can't avoid code duplication unless we refactored the local worker too.

		bool Success = false;
		int32 RetryCount = 0;
		// Retry over the next two seconds if we can't move the file.
		// Anti-virus and indexing applications can interfere and cause this to fail.
		while (!Success && RetryCount < 200)
		{
			if (RetryCount > 0)
			{
				FPlatformProcess::Sleep(0.01f);
			}

			// MoveFile does not create the directory tree, so try to do that now...
			Success = PlatformFile.CreateDirectoryTree(*DirectoryName);
			if (Success)
			{
				Success = PlatformFile.MoveFile(*To, *From);
			}
			RetryCount++;
		}
		checkf(Success, TEXT("Failed to move file %s to %s!"), *From, *To);
	}
}

void FShaderCompileUtilities::DeleteFileHelper(const FString& Filename)
{
	// TODO: This logic came from FShaderCompileThreadRunnable::WriteNewTasks().
	// We can't avoid code duplication unless we refactored the local worker too.

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Filename))
	{
		bool bDeletedOutput = IFileManager::Get().Delete(*Filename, true, true);

		// Retry over the next two seconds if we couldn't delete it
		int32 RetryCount = 0;
		while (!bDeletedOutput && RetryCount < 200)
		{
			FPlatformProcess::Sleep(0.01f);
			bDeletedOutput = IFileManager::Get().Delete(*Filename, true, true);
			RetryCount++;
		}
		checkf(bDeletedOutput, TEXT("Failed to delete %s!"), *Filename);
	}
}

int32 FShaderCompileThreadRunnable::CompilingLoop()
{
	// push completed jobs to Manager->ShaderMapJobs before asking for new ones, so we can free the workers now and avoid them waiting a cycle
	PushCompletedJobsToManager();

	// Grab more shader compile jobs from the input queue
	const int32 NumActiveThreads = PullTasksFromQueue();

	if (NumActiveThreads == 0 && Manager->bAllowAsynchronousShaderCompiling)
	{
		// Yield while there's nothing to do
		// Note: sleep-looping is bad threading practice, wait on an event instead!
		// The shader worker thread does it because it needs to communicate with other processes through the file system
		FPlatformProcess::Sleep(.010f);
	}

	if (Manager->bAllowCompilingThroughWorkers)
	{
		// Write out the files which are input to the shader compile workers
		WriteNewTasks();

		// Launch shader compile workers if they are not already running
		// Workers can time out when idle so they may need to be relaunched
		bool bAbandonWorkers = LaunchWorkersIfNeeded();

		if (bAbandonWorkers)
		{
			// Fall back to local compiles if the SCW crashed.
			// This is nasty but needed to work around issues where message passing through files to SCW is unreliable on random PCs
			Manager->bAllowCompilingThroughWorkers = false;

			// Try to recover from abandoned workers after a certain amount of single-threaded compilations
			if (Manager->NumSingleThreadedRunsBeforeRetry == GSingleThreadedRunsIdle)
			{
				// First try to recover, only run single-threaded approach once
				Manager->NumSingleThreadedRunsBeforeRetry = 1;
			}
			else if (Manager->NumSingleThreadedRunsBeforeRetry > GSingleThreadedRunsMaxCount)
			{
				// Stop retry approach after too many retries have failed
				Manager->NumSingleThreadedRunsBeforeRetry = GSingleThreadedRunsDisabled;
			}
			else
			{
				// Next time increase runs by factor X
				Manager->NumSingleThreadedRunsBeforeRetry *= GSingleThreadedRunsIncreaseFactor;
			}
		}
		else
		{
			// Read files which are outputs from the shader compile workers
			int32 NumProcessedResults = ReadAvailableResults();
			if (NumProcessedResults == 0)
			{
				// Reduce filesystem query rate while actively waiting for results.
				FPlatformProcess::Sleep(0.1f);
			}
		}
	}
	else
	{
		// Execute all pending worker tasks single-threaded
		CompileDirectlyThroughDll();

		// If single-threaded mode was enabled by an abandoned worker, try to recover after the given amount of runs
		if (Manager->NumSingleThreadedRunsBeforeRetry > 0)
		{
			Manager->NumSingleThreadedRunsBeforeRetry--;
			if (Manager->NumSingleThreadedRunsBeforeRetry == 0)
			{
				UE_LOG(LogShaderCompilers, Display, TEXT("Retry shader compiling through workers."));
				Manager->bAllowCompilingThroughWorkers = true;
			}
		}
	}

	return NumActiveThreads;
}

FShaderCompilerStats* GShaderCompilerStats = nullptr;

void FShaderCompilerStats::WriteStats(FOutputDevice* Ar)
{
#if ALLOW_DEBUG_FILES
	static TCHAR DebugText[] = TEXT("Wrote shader compile stats to file '%s'.");
	{
		FlushRenderingCommands();

		FString FileName = FPaths::Combine(*FPaths::ProjectSavedDir(), FString::Printf(TEXT("MaterialStats/Stats-%s.csv"), *FDateTime::Now().ToString()));
		auto DebugWriter = IFileManager::Get().CreateFileWriter(*FileName);
		FDiagnosticTableWriterCSV StatWriter(DebugWriter);
		const TSparseArray<ShaderCompilerStats>& PlatformStats = GetShaderCompilerStats();

		StatWriter.AddColumn(TEXT("Path"));
		StatWriter.AddColumn(TEXT("Platform"));
		StatWriter.AddColumn(TEXT("Compiled"));
		StatWriter.AddColumn(TEXT("Cooked"));
		StatWriter.AddColumn(TEXT("Permutations"));
		StatWriter.AddColumn(TEXT("Compiletime"));
		StatWriter.AddColumn(TEXT("CompiledDouble"));
		StatWriter.AddColumn(TEXT("CookedDouble"));
		StatWriter.CycleRow();

		
		for(int32 Platform = 0; Platform < PlatformStats.GetMaxIndex(); ++Platform)
		{
			if(PlatformStats.IsValidIndex(Platform))
			{
				const ShaderCompilerStats& Stats = PlatformStats[Platform];
				for (const auto& Pair : Stats)
				{
					const FString& Path = Pair.Key;
					const FShaderCompilerStats::FShaderStats& SingleStats = Pair.Value;

					StatWriter.AddColumn(*Path);
					StatWriter.AddColumn(TEXT("%u"), Platform);
					StatWriter.AddColumn(TEXT("%u"), SingleStats.Compiled);
					StatWriter.AddColumn(TEXT("%u"), SingleStats.Cooked);
					StatWriter.AddColumn(TEXT("%u"), SingleStats.PermutationCompilations.Num());
					StatWriter.AddColumn(TEXT("%f"), SingleStats.CompileTime);
					StatWriter.AddColumn(TEXT("%u"), SingleStats.CompiledDouble);
					StatWriter.AddColumn(TEXT("%u"), SingleStats.CookedDouble);
					StatWriter.CycleRow();
					if(GLogShaderCompilerStats)
					{
						UE_LOG(LogShaderCompilers, Log, TEXT("SHADERSTATS %s, %u, %u, %u, %u, %u, %u\n"), *Path, Platform, SingleStats.Compiled, SingleStats.Cooked, SingleStats.PermutationCompilations.Num(), SingleStats.CompiledDouble, SingleStats.CookedDouble);
					}
				}
			}
		}
		DebugWriter->Close();

		FString FullFileName = FPaths::ConvertRelativePathToFull(FileName);
		if (Ar)
		{
			Ar->Logf(DebugText, *FullFileName);
		}
		else
		{
			UE_LOG(LogShaderCompilers, Log, DebugText, *FullFileName);
		}

		if (FParse::Param(FCommandLine::Get(), TEXT("mirrorshaderstats")))
		{
			FString MirrorLocation;
			GConfig->GetString(TEXT("/Script/Engine.ShaderCompilerStats"), TEXT("MaterialStatsLocation"), MirrorLocation, GGameIni);
			FParse::Value(FCommandLine::Get(), TEXT("MaterialStatsMirror="), MirrorLocation);

			if (!MirrorLocation.IsEmpty())
			{
				FString TargetType = TEXT("Default");
				FParse::Value(FCommandLine::Get(), TEXT("target="), TargetType);
				if (TargetType == TEXT("Default"))
				{
					FParse::Value(FCommandLine::Get(), TEXT("targetplatform="), TargetType);
				}
				FString CopyLocation = FPaths::Combine(*MirrorLocation, FApp::GetProjectName(), *FApp::GetBranchName(), FString::Printf(TEXT("Stats-Latest-%d(%s).csv"), FEngineVersion::Current().GetChangelist() , *TargetType));
				TArray <FString> ExistingFiles;
				IFileManager::Get().FindFiles(ExistingFiles, *FPaths::Combine(*MirrorLocation, FApp::GetProjectName(), *FApp::GetBranchName()));
				for (FString CurFile : ExistingFiles)
				{
					if (CurFile.Contains(FString::Printf(TEXT("(%s)"), *TargetType)))
					{
						IFileManager::Get().Delete(*FPaths::Combine(*MirrorLocation, FApp::GetProjectName(), *FApp::GetBranchName(), *CurFile), false, true);
					}
				}
				IFileManager::Get().Copy(*CopyLocation, *FileName, true, true);
			}
		}
	}
	{

		FString FileName = FString::Printf(TEXT("%s/MaterialStatsDebug/StatsDebug-%s.csv"), *FPaths::ProjectSavedDir(), *FDateTime::Now().ToString());
		auto DebugWriter = IFileManager::Get().CreateFileWriter(*FileName);
		FDiagnosticTableWriterCSV StatWriter(DebugWriter);
		const TSparseArray<ShaderCompilerStats>& PlatformStats = GetShaderCompilerStats();
		StatWriter.AddColumn(TEXT("Name"));
		StatWriter.AddColumn(TEXT("Platform"));
		StatWriter.AddColumn(TEXT("Compiles"));
		StatWriter.AddColumn(TEXT("CompilesDouble"));
		StatWriter.AddColumn(TEXT("Uses"));
		StatWriter.AddColumn(TEXT("UsesDouble"));
		StatWriter.AddColumn(TEXT("PermutationString"));
		StatWriter.CycleRow();


		for (int32 Platform = 0; Platform < PlatformStats.GetMaxIndex(); ++Platform)
		{
			if (PlatformStats.IsValidIndex(Platform))
			{
				const ShaderCompilerStats& Stats = PlatformStats[Platform];
				for (const auto& Pair : Stats)
				{
					const FString& Path = Pair.Key;
					const FShaderCompilerStats::FShaderStats& SingleStats = Pair.Value;
					for (const FShaderCompilerStats::FShaderCompilerSinglePermutationStat& Stat : SingleStats.PermutationCompilations)
					{
						StatWriter.AddColumn(*Path);
						StatWriter.AddColumn(TEXT("%u"), Platform);
						StatWriter.AddColumn(TEXT("%u"), Stat.Compiled);
						StatWriter.AddColumn(TEXT("%u"), Stat.CompiledDouble);
						StatWriter.AddColumn(TEXT("%u"), Stat.Cooked);
						StatWriter.AddColumn(TEXT("%u"), Stat.CookedDouble);
						StatWriter.AddColumn(TEXT("%s"), *Stat.PermutationString);
						StatWriter.CycleRow();
					}
				}

			}
		}

		FString FullFileName = FPaths::ConvertRelativePathToFull(FileName);
		if (Ar)
		{
			Ar->Logf(DebugText, *FullFileName);
		}
		else
		{
			UE_LOG(LogShaderCompilers, Log, DebugText, *FullFileName);
		}
	}
#endif // ALLOW_DEBUG_FILES
}

void FShaderCompilerStats::WriteStatSummary()
{
	const uint32 TotalCompiled = GetTotalShadersCompiled();
	const double TotalTimeAtLeastOneJobWasInFlight = GetTimeShaderCompilationWasActive();

	UE_LOG(LogShaderCompilers, Display, TEXT("=== Shader Compilation stats ==="));
	UE_LOG(LogShaderCompilers, Display, TEXT("Shaders Compiled: %u"), TotalCompiled);

	FScopeLock Lock(&CompileStatsLock);	// make a local copy for all the stats?
	UE_LOG(LogShaderCompilers, Display, TEXT("Jobs assigned %.0f, completed %.0f (%.2f%%)"), JobsAssigned, JobsCompleted, (JobsAssigned > 0.0) ? 100.0 * JobsCompleted / JobsAssigned : 0.0);

	if (TimesLocalWorkersWereIdle > 0.0)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Average time worker was idle: %.2f s"), AccumulatedLocalWorkerIdleTime / TimesLocalWorkersWereIdle);
	}

	if (JobsAssigned > 0.0)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Time job spent in pending queue: average %.2f s, longest %.2f s"), AccumulatedPendingTime / JobsAssigned, MaxPendingTime);
	}

	if (JobsCompleted > 0.0)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Job execution time: average %.2f s, max %.2f s"), AccumulatedJobExecutionTime / JobsCompleted, MaxJobExecutionTime);
		UE_LOG(LogShaderCompilers, Display, TEXT("Job life time (pending + execution): average %.2f s, max %.2f"), AccumulatedJobLifeTime / JobsCompleted, MaxJobLifeTime);
	}

	UE_LOG(LogShaderCompilers, Display, TEXT("Time at least one job was in flight (either pending or executed): %.2f s"), TotalTimeAtLeastOneJobWasInFlight);

	// print stats about the batches
	if (LocalJobBatchesSeen > 0 && DistributedJobBatchesSeen > 0)
	{
		int64 JobBatchesSeen = LocalJobBatchesSeen + DistributedJobBatchesSeen;
		double TotalJobsReportedInJobBatches = TotalJobsReportedInLocalJobBatches + TotalJobsReportedInDistributedJobBatches;

		UE_LOG(LogShaderCompilers, Display, TEXT("Jobs were issued in %lld batches (%lld local, %lld distributed), average %.2f jobs/batch (%.2f jobs/local batch. %.2f jobs/distributed batch)"),
			JobBatchesSeen, LocalJobBatchesSeen, DistributedJobBatchesSeen,
			static_cast<double>(TotalJobsReportedInJobBatches) / static_cast<double>(JobBatchesSeen),
			static_cast<double>(TotalJobsReportedInLocalJobBatches) / static_cast<double>(LocalJobBatchesSeen),
			static_cast<double>(TotalJobsReportedInDistributedJobBatches) / static_cast<double>(DistributedJobBatchesSeen)
		);
	}
	else if (LocalJobBatchesSeen > 0)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Jobs were issued in %lld batches (only local compilation was used), average %.2f jobs/batch"), 
			LocalJobBatchesSeen, static_cast<double>(TotalJobsReportedInLocalJobBatches) / static_cast<double>(LocalJobBatchesSeen));
	}
	else if (DistributedJobBatchesSeen > 0)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Jobs were issued in %lld batches (only distributed compilation was used), average %.2f jobs/batch"),
			DistributedJobBatchesSeen, static_cast<double>(TotalJobsReportedInDistributedJobBatches) / static_cast<double>(DistributedJobBatchesSeen));
	}

	if (TotalTimeAtLeastOneJobWasInFlight > 0.0)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Average processing rate: %.2f jobs/sec"), JobsCompleted / TotalTimeAtLeastOneJobWasInFlight);
	}

	if (ShaderTimings.Num())
	{
		// calculate effective parallelization (total time needed to compile all shaders divided by actual wall clock time spent processing at least 1 shader)
		double TotalThreadTimeForAllShaders = 0.0;
		double TotalThreadPreprocessTimeForAllShaders = 0.0;
		for (TMap<FString, FShaderTimings>::TConstIterator Iter(ShaderTimings); Iter; ++Iter)
		{
			TotalThreadTimeForAllShaders += Iter.Value().TotalCompileTime;
			TotalThreadPreprocessTimeForAllShaders += Iter.Value().TotalPreprocessTime;
		}

		UE_LOG(LogShaderCompilers, Display, TEXT("Total thread time: %.2f s"), TotalThreadTimeForAllShaders);
		UE_LOG(LogShaderCompilers, Display, TEXT("Total thread preprocess time: %.2f s"), TotalThreadPreprocessTimeForAllShaders);
		UE_LOG(LogShaderCompilers, Display, TEXT("Percentage time preprocessing: %.2f%%"), TotalThreadTimeForAllShaders > 0.0 ? (TotalThreadPreprocessTimeForAllShaders / TotalThreadTimeForAllShaders) * 100.0 : 0.0);

		if (TotalTimeAtLeastOneJobWasInFlight > 0.0)
		{
			double EffectiveParallelization = TotalThreadTimeForAllShaders / TotalTimeAtLeastOneJobWasInFlight;
			if (DistributedJobBatchesSeen == 0)
			{
				UE_LOG(LogShaderCompilers, Display, TEXT("Effective parallelization: %.2f (times faster than compiling all shaders on one thread). Compare with number of workers: %d"), EffectiveParallelization, GShaderCompilingManager->GetNumLocalWorkers());
			}
			else
			{
				UE_LOG(LogShaderCompilers, Display, TEXT("Effective parallelization: %.2f (times faster than compiling all shaders on one thread). Distributed compilation was used."), EffectiveParallelization);
			}
		}


		// sort by avg time
		ShaderTimings.ValueSort([](const FShaderTimings& A, const FShaderTimings& B) { return A.AverageCompileTime > B.AverageCompileTime; });

		const int32 MaxShadersToPrint = FMath::Min(ShaderTimings.Num(), 5);
		UE_LOG(LogShaderCompilers, Display, TEXT("Top %d most expensive shader types by average time:"), MaxShadersToPrint);

		int32 Idx = 0;
		for (TMap<FString, FShaderTimings>::TConstIterator Iter(ShaderTimings); Iter; ++Iter)
		{
			const FShaderTimings& Timings = Iter.Value();

			UE_LOG(LogShaderCompilers, Display, TEXT("%60s (compiled %4d times, average %4.2f sec, max %4.2f sec, min %4.2f sec)"), *Iter.Key(), Timings.NumCompiled, Timings.AverageCompileTime, Timings.MaxCompileTime, Timings.MinCompileTime);
			if (++Idx >= MaxShadersToPrint)
			{
				break;
			}
		}

		// sort by total time
		ShaderTimings.ValueSort([](const FShaderTimings& A, const FShaderTimings& B) { return A.TotalCompileTime > B.TotalCompileTime; });

		UE_LOG(LogShaderCompilers, Display, TEXT("Top %d shader types by total compile time:"), MaxShadersToPrint);

		Idx = 0;
		for (TMap<FString, FShaderTimings>::TConstIterator Iter(ShaderTimings); Iter; ++Iter)
		{
			const FShaderTimings& Timings = Iter.Value();

			UE_LOG(LogShaderCompilers, Display, TEXT("%60s - %.2f%% of total time (compiled %4d times, average %4.2f sec, max %4.2f sec, min %4.2f sec)"), 
				*Iter.Key(), 100.0 * Timings.TotalCompileTime / TotalThreadTimeForAllShaders, Timings.NumCompiled, Timings.AverageCompileTime, Timings.MaxCompileTime, Timings.MinCompileTime);
			if (++Idx >= MaxShadersToPrint)
			{
				break;
			}
		}
	}
}

uint32 FShaderCompilerStats::GetTotalShadersCompiled()
{
	FScopeLock Lock(&CompileStatsLock);
	return JobsCompleted;
}

void FShaderCompilerStats::RegisterLocalWorkerIdleTime(double IdleTime)
{
	FScopeLock Lock(&CompileStatsLock);
	AccumulatedLocalWorkerIdleTime += IdleTime;
	TimesLocalWorkersWereIdle++;
}

void FShaderCompilerStats::RegisterNewPendingJob(FShaderCommonCompileJob& Job)
{
	// accessing job timestamps isn't arbitrated by any lock. It is assumed that the registration of a job at one of the stages
	// of its lifetime happens before the code can move it to another stage (i.e. new pending job is registered before it is added to the pending queue,
	// so it cannot be given away to a worker while it's still being registered, and an assigned job is registered before it is actually given to the worker,
	// so it cannot end up being registered as finished at the same time on some other thread).
	Job.TimeAddedToPendingQueue = FPlatformTime::Seconds();
}

void FShaderCompilerStats::RegisterAssignedJob(FShaderCommonCompileJob& Job)
{
	ensure(Job.TimeAddedToPendingQueue != 0.0);
	Job.TimeAssignedToExecution = FPlatformTime::Seconds();

	FScopeLock Lock(&CompileStatsLock);
	JobsAssigned++;
	double TimeSpendPending = (Job.TimeAssignedToExecution - Job.TimeAddedToPendingQueue);
	AccumulatedPendingTime += TimeSpendPending;
	MaxPendingTime = FMath::Max(TimeSpendPending, MaxPendingTime);
}

void FShaderCompilerStats::RegisterFinishedJob(FShaderCommonCompileJob& Job)
{
	ensure(Job.TimeAssignedToExecution != 0.0);
	Job.TimeExecutionCompleted = FPlatformTime::Seconds();

	FScopeLock Lock(&CompileStatsLock);
	JobsCompleted++;
	
	double ExecutionTime = (Job.TimeExecutionCompleted - Job.TimeAssignedToExecution);
	AccumulatedJobExecutionTime += ExecutionTime;
	MaxJobExecutionTime = FMath::Max(ExecutionTime, MaxJobExecutionTime);

	double LifeTime = (Job.TimeExecutionCompleted - Job.TimeAddedToPendingQueue);
	AccumulatedJobLifeTime += LifeTime;
	MaxJobLifeTime = FMath::Max(LifeTime, MaxJobLifeTime);

	auto AddToInterval = [](TArray<TInterval<double>>& Accumulator, const TInterval<double>& NewInterval)
	{
		bool bFoundOverlap = false;
		TInterval<double> New = NewInterval;
		do
		{
			bFoundOverlap = false;
			for (int32 Idx = 0; Idx < Accumulator.Num(); ++Idx)
			{
				const TInterval<double>& Existing = Accumulator[Idx];
				if (Existing.Max < New.Min || New.Max < Existing.Min)
				{
					continue;	// no overlap
				}

				// if fully contained within existing interval, just ignore
				if (Existing.Min <= New.Min && New.Max <= Existing.Max)
				{
					return;
				}

				bFoundOverlap = true;
				// if there's an overlap, remove the existing interval, merge with the new one and attempt to add again
				TInterval<double> Merged(FMath::Min(Existing.Min, New.Min), FMath::Max(Existing.Max, New.Max));
				check(Merged.Size() >= Existing.Size());
				check(Merged.Size() >= New.Size());
				Accumulator.RemoveAt(Idx);
				New = Merged;
				break;
			}
		} while (bFoundOverlap);

		// if we arrived here without an overlap, we have a new one
		if (!bFoundOverlap)
		{
			Accumulator.Add(New);
		}
	};

	// estimate lifetime without an overlap
	ensure(Job.TimeAddedToPendingQueue != 0.0 && Job.TimeAddedToPendingQueue <= Job.TimeExecutionCompleted);
	AddToInterval(JobLifeTimeIntervals, TInterval<double>(Job.TimeAddedToPendingQueue, Job.TimeExecutionCompleted));

	if (const FShaderCompileJob* SingleJob = Job.GetSingleShaderJob())
	{
		const FString ShaderName(SingleJob->Key.ShaderType->GetName());
		if (FShaderTimings* Existing = ShaderTimings.Find(ShaderName))
		{
			Existing->MinCompileTime = FMath::Min(Existing->MinCompileTime, static_cast<float>(SingleJob->Output.CompileTime));
			Existing->MaxCompileTime = FMath::Max(Existing->MaxCompileTime, static_cast<float>(SingleJob->Output.CompileTime));
			Existing->TotalCompileTime += SingleJob->Output.CompileTime;
			Existing->TotalPreprocessTime += SingleJob->Output.PreprocessTime;
			Existing->NumCompiled++;
			// calculate as an optimization to make sorting later faster
			Existing->AverageCompileTime = Existing->TotalCompileTime / static_cast<float>(Existing->NumCompiled);
		}
		else
		{
			FShaderTimings New;
			New.MinCompileTime = SingleJob->Output.CompileTime;
			New.MaxCompileTime = New.MinCompileTime;
			New.TotalCompileTime = New.MinCompileTime;
			New.TotalPreprocessTime += SingleJob->Output.PreprocessTime;
			New.AverageCompileTime = New.MinCompileTime;
			New.NumCompiled = 1;
			ShaderTimings.Add(ShaderName, New);
		}
	}

}

void FShaderCompilerStats::RegisterJobBatch(int32 NumJobs, EExecutionType ExecType)
{
	if (ExecType == EExecutionType::Local)
	{
		FScopeLock Lock(&CompileStatsLock);
		++LocalJobBatchesSeen;
		TotalJobsReportedInLocalJobBatches += NumJobs;
	}
	else if (ExecType == EExecutionType::Distributed)
	{
		FScopeLock Lock(&CompileStatsLock);
		++DistributedJobBatchesSeen;
		TotalJobsReportedInDistributedJobBatches += NumJobs;
	}
	else
	{
		checkNoEntry();
	}
}

void FShaderCompilerStats::RegisterCookedShaders(uint32 NumCooked, float CompileTime, EShaderPlatform Platform, const FString MaterialPath, FString PermutationString)
{
	FScopeLock Lock(&CompileStatsLock);
	if(!CompileStats.IsValidIndex(Platform))
	{
		ShaderCompilerStats Stats;
		CompileStats.Insert(Platform, Stats);
	}

	FShaderCompilerStats::FShaderStats& Stats = CompileStats[Platform].FindOrAdd(MaterialPath);
	Stats.CompileTime += CompileTime;
	bool bFound = false;
	for (FShaderCompilerSinglePermutationStat& Stat : Stats.PermutationCompilations)
	{
		if (PermutationString == Stat.PermutationString)
		{
			bFound = true;
			if (Stat.Cooked != 0)
			{
				Stat.CookedDouble += NumCooked;
				Stats.CookedDouble += NumCooked;
			}
			else
			{
				Stat.Cooked = NumCooked;
				Stats.Cooked += NumCooked;
			}
		}
	}
	if(!bFound)
	{
		Stats.Cooked += NumCooked;
	}
	if (!bFound)
	{
		Stats.PermutationCompilations.Emplace(PermutationString, 0, NumCooked);
	}
}

void FShaderCompilerStats::RegisterCompiledShaders(uint32 NumCompiled, EShaderPlatform Platform, const FString MaterialPath, FString PermutationString)
{
	FScopeLock Lock(&CompileStatsLock);
	if (!CompileStats.IsValidIndex(Platform))
	{
		ShaderCompilerStats Stats;
		CompileStats.Insert(Platform, Stats);
	}
	FShaderCompilerStats::FShaderStats& Stats = CompileStats[Platform].FindOrAdd(MaterialPath);

	bool bFound = false;
	for (FShaderCompilerSinglePermutationStat& Stat : Stats.PermutationCompilations)
	{
		if (PermutationString == Stat.PermutationString)
		{
			bFound = true;
			if (Stat.Compiled != 0)
			{
				Stat.CompiledDouble += NumCompiled;
				Stats.CompiledDouble += NumCompiled;
			}
			else
			{
				Stat.Compiled = NumCompiled;
				Stats.Compiled += NumCompiled;
			}
		}
	}
	if(!bFound)
	{
		Stats.Compiled += NumCompiled;
	}


	if (!bFound)
	{
		Stats.PermutationCompilations.Emplace(PermutationString, NumCompiled, 0);
	}
}

void FShaderCompilerStats::AddDDCMiss(uint32 NumMisses)
{
	ShaderMapDDCMisses += NumMisses;
}

uint32 FShaderCompilerStats::GetDDCMisses() const
{
	return ShaderMapDDCMisses;
}

void FShaderCompilerStats::AddDDCHit(uint32 NumHits)
{
	ShaderMapDDCHits += NumHits;
}

uint32 FShaderCompilerStats::GetDDCHits() const
{
	return ShaderMapDDCHits;
}

double FShaderCompilerStats::GetTimeShaderCompilationWasActive()
{
	FScopeLock Lock(&CompileStatsLock);
	double Sum = 0;
	for (int32 Idx = 0; Idx < JobLifeTimeIntervals.Num(); ++Idx)
	{
		const TInterval<double>& Existing = JobLifeTimeIntervals[Idx];
		Sum += Existing.Size();
	}
	return Sum;
}

FShaderCompilingManager* GShaderCompilingManager = nullptr;

bool FShaderCompilingManager::AllTargetPlatformSupportsRemoteShaderCompiling()
{
	// no compiling support
	if (!AllowShaderCompiling())
	{
		return false;
	}

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();	
	if (!TPM)
	{
		return false;
	}
	
	const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		if (!Platforms[Index]->CanSupportRemoteShaderCompile())
		{
			return false;
		}
	}
	
	return true;
}

IDistributedBuildController* FShaderCompilingManager::FindRemoteCompilerController() const
{
	// no controllers needed if not compiling
	if (!AllowShaderCompiling())
	{
		return nullptr;
	}

	TArray<IDistributedBuildController*> AvailableControllers = IModularFeatures::Get().GetModularFeatureImplementations<IDistributedBuildController>(IDistributedBuildController::GetModularFeatureType());

	auto FindXGE = [](IDistributedBuildController* Controller)
	{
		return Controller != nullptr
			&& Controller->GetName().StartsWith("XGE");
	};

	// Prefer XGE
	if (IDistributedBuildController** ControllerPtr = AvailableControllers.FindByPredicate(FindXGE))
	{
		IDistributedBuildController* Controller = *ControllerPtr;
		if (Controller && Controller->IsSupported())
		{
			Controller->InitializeController();
			return Controller;
		}
	}

	// Fall back on whatever is available
	for (IDistributedBuildController* Controller : AvailableControllers)
	{
		if (Controller != nullptr && Controller->IsSupported())
		{
			Controller->InitializeController();
			return Controller;
		}
	}

	return nullptr;
}

FShaderCompilingManager::FShaderCompilingManager() :
	bCompilingDuringGame(false),
	NumExternalJobs(0),
	NumSingleThreadedRunsBeforeRetry(GSingleThreadedRunsIdle),
	NumDumpedShaderSources(0),
#if PLATFORM_MAC
	ShaderCompileWorkerName(FPaths::EngineDir() / TEXT("Binaries/Mac/ShaderCompileWorker")),
#elif PLATFORM_LINUX
	ShaderCompileWorkerName(FPaths::EngineDir() / TEXT("Binaries/Linux/ShaderCompileWorker")),
#else
	ShaderCompileWorkerName(FPaths::EngineDir() / TEXT("Binaries/Win64/ShaderCompileWorker.exe")),
#endif
	SuppressedShaderPlatforms(0),
	BuildDistributionController(nullptr),
	bNoShaderCompilation(false),
	Notification(GetAssetNameFormat())
{
	// don't perform any initialization if compiling is not allowed
	if (!AllowShaderCompiling())
	{
		// use existing flag to disable compiling
		bNoShaderCompilation = true;
		return;
	}

	bIsEngineLoopInitialized = false;
	FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([&]() 
		{ 
			bIsEngineLoopInitialized = true; 
		}
	);

	WorkersBusyTime = 0;

	// Threads must use absolute paths on Windows in case the current directory is changed on another thread!
	ShaderCompileWorkerName = FPaths::ConvertRelativePathToFull(ShaderCompileWorkerName);

	// Read values from the engine ini
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowCompilingThroughWorkers"), bAllowCompilingThroughWorkers, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowAsynchronousShaderCompiling"), bAllowAsynchronousShaderCompiling, GEngineIni ));

	// override the use of workers, can be helpful for debugging shader compiler code
	static const IConsoleVariable* CVarAllowCompilingThroughWorkers = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.AllowCompilingThroughWorkers"), false);
	if (!FPlatformProcess::SupportsMultithreading() || FParse::Param(FCommandLine::Get(), TEXT("noshaderworker")) || (CVarAllowCompilingThroughWorkers && CVarAllowCompilingThroughWorkers->GetInt() == 0))
	{
		bAllowCompilingThroughWorkers = false;
	}

	if (!FPlatformProcess::SupportsMultithreading())
	{
		bAllowAsynchronousShaderCompiling = false;
	}

	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("MaxShaderJobBatchSize"), MaxShaderJobBatchSize, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bPromptToRetryFailedShaderCompiles"), bPromptToRetryFailedShaderCompiles, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bLogJobCompletionTimes"), bLogJobCompletionTimes, GEngineIni ));
	GConfig->GetFloat(TEXT("DevOptions.Shaders"), TEXT("WorkerTimeToLive"), GRegularWorkerTimeToLive, GEngineIni);
	GConfig->GetFloat(TEXT("DevOptions.Shaders"), TEXT("BuildWorkerTimeToLive"), GBuildWorkerTimeToLive, GEngineIni);

	verify(GConfig->GetFloat( TEXT("DevOptions.Shaders"), TEXT("ProcessGameThreadTargetTime"), ProcessGameThreadTargetTime, GEngineIni ));

#if UE_BUILD_DEBUG
	// Increase budget for processing results in debug or else it takes forever to finish due to poor framerate
	ProcessGameThreadTargetTime *= 3;
#endif

	// Get the current process Id, this will be used by the worker app to shut down when it's parent is no longer running.
	ProcessId = FPlatformProcess::GetCurrentProcessId();

	// Use a working directory unique to this game, process and thread so that it will not conflict 
	// With processes from other games, processes from the same game or threads in this same process.
	// Use IFileManager to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	//ShaderBaseWorkingDirectory = FPlatformProcess::ShaderWorkingDir() / FString::FromInt(ProcessId) + TEXT("/");

	{
		FGuid Guid;
		Guid = FGuid::NewGuid();
		FString LegacyShaderWorkingDirectory = FPaths::ProjectIntermediateDir() / TEXT("Shaders/WorkingDirectory/")  / FString::FromInt(ProcessId) + TEXT("/");
		ShaderBaseWorkingDirectory = FPaths::ShaderWorkingDir() / *Guid.ToString(EGuidFormats::Digits) + TEXT("/");
		UE_LOG(LogShaderCompilers, Log, TEXT("Guid format shader working directory is %d characters bigger than the processId version (%s)."), ShaderBaseWorkingDirectory.Len() - LegacyShaderWorkingDirectory.Len(), *LegacyShaderWorkingDirectory );
	}

	if (!IFileManager::Get().DeleteDirectory(*ShaderBaseWorkingDirectory, false, true))
	{
		UE_LOG(LogShaderCompilers, Fatal, TEXT("Could not delete the shader compiler working directory '%s'."), *ShaderBaseWorkingDirectory);
	}
	else
	{
		UE_LOG(LogShaderCompilers, Log, TEXT("Cleaned the shader compiler working directory '%s'."), *ShaderBaseWorkingDirectory);
	}
	FString AbsoluteBaseDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ShaderBaseWorkingDirectory);
	FPaths::NormalizeDirectoryName(AbsoluteBaseDirectory);
	AbsoluteShaderBaseWorkingDirectory = AbsoluteBaseDirectory + TEXT("/");

	FString AbsoluteDebugInfoDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::ProjectSavedDir() / TEXT("ShaderDebugInfo")));
	const FString OverrideShaderDebugDir = CVarShaderOverrideDebugDir.GetValueOnAnyThread();
	if (!OverrideShaderDebugDir.IsEmpty())
	{
		AbsoluteDebugInfoDirectory = OverrideShaderDebugDir;
	}
	FPaths::NormalizeDirectoryName(AbsoluteDebugInfoDirectory);
	AbsoluteShaderDebugInfoDirectory = AbsoluteDebugInfoDirectory;

	CalculateNumberOfCompilingThreads(FPlatformMisc::NumberOfCores(), FPlatformMisc::NumberOfCoresIncludingHyperthreads());

	TUniquePtr<FShaderCompileThreadRunnableBase> RemoteCompileThread;
	const bool bCanUseRemoteCompiling = bAllowCompilingThroughWorkers && ShaderCompiler::IsRemoteCompilingAllowed() && AllTargetPlatformSupportsRemoteShaderCompiling();
	BuildDistributionController = bCanUseRemoteCompiling ? FindRemoteCompilerController() : nullptr;
	
	if (BuildDistributionController)
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Using %s for Shader Compilation."), *BuildDistributionController->GetName());
		RemoteCompileThread = MakeUnique<FShaderCompileDistributedThreadRunnable_Interface>(this, *BuildDistributionController);
	}

	GConfig->SetBool(TEXT("/Script/UnrealEd.UnrealEdOptions"), TEXT("UsingXGE"), RemoteCompileThread.IsValid(), GEditorIni);

	TUniquePtr<FShaderCompileThreadRunnableBase> LocalThread = MakeUnique<FShaderCompileThreadRunnable>(this);
	if (RemoteCompileThread)
	{
		checkf(ShaderCompiler::IsRemoteCompilingAllowed(), TEXT("We have a remote compiling thread without the remote compilation being allowed"));

		// Only force-local jobs are guaranteed to stay on the local machine. Going wide with High priority jobs is important for the startup times,
		// since special materials use High priority. Possibly the partition by priority is too rigid in general.
		RemoteCompileThread->SetPriorityRange(EShaderCompileJobPriority::Low, EShaderCompileJobPriority::High);
		LocalThread->SetPriorityRange(EShaderCompileJobPriority::Normal, EShaderCompileJobPriority::ForceLocal);
		Threads.Add(MoveTemp(RemoteCompileThread));
	}
	else
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Using Local Shader Compiler with %d workers."), NumShaderCompilingThreads);

		if (GIsBuildMachine)
		{
			int32 MinSCWsToSpawnBeforeWarning = 8; // optional, default to 8
			GConfig->GetInt(TEXT("DevOptions.Shaders"), TEXT("MinSCWsToSpawnBeforeWarning"), MinSCWsToSpawnBeforeWarning, GEngineIni);
			if (NumShaderCompilingThreads < static_cast<uint32>(MinSCWsToSpawnBeforeWarning))
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("Only %d SCWs will be spawned, which will result in longer shader compile times."), NumShaderCompilingThreads);
			}
		}
	}
	Threads.Add(MoveTemp(LocalThread));

	for (const auto& Thread : Threads)
	{
		Thread->StartThread();
	}

	FAssetCompilingManager::Get().RegisterManager(this);
}

FShaderCompilingManager::~FShaderCompilingManager()
{
	// we never initialized, so nothing to do
	if (!AllowShaderCompiling())
	{
		return;
	}

	PrintStats(true);

	for (const auto& Thread : Threads)
	{
		Thread->Stop();
		Thread->WaitForCompletion();
	}

	FAssetCompilingManager::Get().UnregisterManager(this);
}

void FShaderCompilingManager::CalculateNumberOfCompilingThreads(int32 NumberOfCores, int32 NumberOfCoresIncludingHyperthreads)
{
	const int32 NumVirtualCores = NumberOfCoresIncludingHyperthreads;

	int32 NumUnusedShaderCompilingThreads;
	verify(GConfig->GetInt(TEXT("DevOptions.Shaders"), TEXT("NumUnusedShaderCompilingThreads"), NumUnusedShaderCompilingThreads, GEngineIni));

	int32 NumUnusedShaderCompilingThreadsDuringGame;
	verify(GConfig->GetInt(TEXT("DevOptions.Shaders"), TEXT("NumUnusedShaderCompilingThreadsDuringGame"), NumUnusedShaderCompilingThreadsDuringGame, GEngineIni));

	int32 ShaderCompilerCoreCountThreshold;
	verify(GConfig->GetInt(TEXT("DevOptions.Shaders"), TEXT("ShaderCompilerCoreCountThreshold"), ShaderCompilerCoreCountThreshold, GEngineIni));

	bool bForceUseSCWMemoryPressureLimits = false;
	GConfig->GetBool(TEXT("DevOptions.Shaders"), TEXT("bForceUseSCWMemoryPressureLimits"), bForceUseSCWMemoryPressureLimits, GEngineIni);

	// Don't reserve threads based on a percentage if we are in a commandlet or on a low core machine.
	// In these scenarios we should try to use as many threads as possible.
	if (!IsRunningCommandlet() && !GIsBuildMachine && NumVirtualCores > ShaderCompilerCoreCountThreshold)
	{
		// Reserve a percentage of the threads for general background work.
		float PercentageUnusedShaderCompilingThreads;
		verify(GConfig->GetFloat(TEXT("DevOptions.Shaders"), TEXT("PercentageUnusedShaderCompilingThreads"), PercentageUnusedShaderCompilingThreads, GEngineIni));

		// ensure we get a valid multiplier.
		PercentageUnusedShaderCompilingThreads = FMath::Clamp(PercentageUnusedShaderCompilingThreads, 0.0f, 100.0f) / 100.0f;

		NumUnusedShaderCompilingThreads = FMath::CeilToInt(NumVirtualCores * PercentageUnusedShaderCompilingThreads);
		NumUnusedShaderCompilingThreadsDuringGame = NumUnusedShaderCompilingThreads;
	}

	// Use all the cores on the build machines.
	if (GForceAllCoresForShaderCompiling != 0)
	{
		NumUnusedShaderCompilingThreads = 0;
	}

	NumShaderCompilingThreads = (bAllowCompilingThroughWorkers && NumVirtualCores > NumUnusedShaderCompilingThreads) ? (NumVirtualCores - NumUnusedShaderCompilingThreads) : 1;

	// Make sure there's at least one worker allowed to be active when compiling during the game
	NumShaderCompilingThreadsDuringGame = (bAllowCompilingThroughWorkers && NumVirtualCores > NumUnusedShaderCompilingThreadsDuringGame) ? (NumVirtualCores - NumUnusedShaderCompilingThreadsDuringGame) : 1;

	// On machines with few cores, each core will have a massive impact on compile time, so we prioritize compile latency over editor performance during the build
	if (NumVirtualCores <= 4)
	{
		NumShaderCompilingThreads = NumVirtualCores - 1;
		NumShaderCompilingThreadsDuringGame = NumVirtualCores - 1;
	}
#if PLATFORM_DESKTOP
	else if (GIsBuildMachine || bForceUseSCWMemoryPressureLimits)
	{
		// Cooker ends up running OOM so use a simple heuristic based on some INI values
		float CookerMemoryUsedInGB = 0.0f;
		float MemoryToLeaveForTheOSInGB = 0.0f;
		float MemoryUsedPerSCWProcessInGB = 0.0f;
		bool bFoundEntries = true;
		bFoundEntries = bFoundEntries && GConfig->GetFloat(TEXT("DevOptions.Shaders"), TEXT("CookerMemoryUsedInGB"), CookerMemoryUsedInGB, GEngineIni);
		bFoundEntries = bFoundEntries && GConfig->GetFloat(TEXT("DevOptions.Shaders"), TEXT("MemoryToLeaveForTheOSInGB"), MemoryToLeaveForTheOSInGB, GEngineIni);
		bFoundEntries = bFoundEntries && GConfig->GetFloat(TEXT("DevOptions.Shaders"), TEXT("MemoryUsedPerSCWProcessInGB"), MemoryUsedPerSCWProcessInGB, GEngineIni);
		if (bFoundEntries)
		{
			uint32 PhysicalGBRam = FPlatformMemory::GetPhysicalGBRam();
			float AvailableMemInGB = (float)PhysicalGBRam - CookerMemoryUsedInGB;
			if (AvailableMemInGB > 0.0f)
			{
				if (AvailableMemInGB > MemoryToLeaveForTheOSInGB)
				{
					AvailableMemInGB -= MemoryToLeaveForTheOSInGB;
				}
				else
				{
					UE_LOG(LogShaderCompilers, Warning, TEXT("Machine has %d GBs of RAM, cooker might take %f GBs, but not enough memory left for the OS! (Requested %f GBs for the OS)"), PhysicalGBRam, CookerMemoryUsedInGB, MemoryToLeaveForTheOSInGB);
				}
			}
			else
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("Machine has %d GBs of RAM, but cooker might take %f GBs!"), PhysicalGBRam, CookerMemoryUsedInGB);
			}
			if (MemoryUsedPerSCWProcessInGB > 0.0f)
			{
				float NumSCWs = AvailableMemInGB / MemoryUsedPerSCWProcessInGB;
				NumShaderCompilingThreads = FMath::RoundToInt(NumSCWs);

				bool bUseVirtualCores = true;
				GConfig->GetBool(TEXT("DevOptions.Shaders"), TEXT("bUseVirtualCores"), bUseVirtualCores, GEngineIni);
				uint32 MaxNumCoresToUse = bUseVirtualCores ? NumVirtualCores : NumberOfCores;
				NumShaderCompilingThreads = FMath::Clamp<uint32>(NumShaderCompilingThreads, 1, MaxNumCoresToUse - 1);
				NumShaderCompilingThreadsDuringGame = FMath::Min<int32>(NumShaderCompilingThreads, NumShaderCompilingThreadsDuringGame);
			}
		}
		else if (bForceUseSCWMemoryPressureLimits)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("bForceUseSCWMemoryPressureLimits was set but missing one or more prerequisite setting(s): CookerMemoryUsedInGB, MemoryToLeaveForTheOSInGB, MemoryUsedPerSCWProcessInGB.  Ignoring bForceUseSCWMemoryPressureLimits"));
		}

		if (GIsBuildMachine)
		{
			// force crashes on hung shader maps on build machines, to prevent builds running for days
			GCrashOnHungShaderMaps = 1;
		}
	}
#endif

	NumShaderCompilingThreads = FMath::Max<int32>(1, NumShaderCompilingThreads);
	NumShaderCompilingThreadsDuringGame = FMath::Max<int32>(1, NumShaderCompilingThreadsDuringGame);

	NumShaderCompilingThreadsDuringGame = FMath::Min<int32>(NumShaderCompilingThreadsDuringGame, NumShaderCompilingThreads);
}

void FShaderCompilingManager::OnMachineResourcesChanged(int32 NumberOfCores, int32 NumberOfCoresIncludingHyperthreads)
{
	CalculateNumberOfCompilingThreads(NumberOfCores, NumberOfCoresIncludingHyperthreads);
	for (const auto& Thread : Threads)
	{
		Thread->OnMachineResourcesChanged();
	}
}

FName FShaderCompilingManager::GetStaticAssetTypeName()
{
	return TEXT("UE-Shader");
}

FName FShaderCompilingManager::GetAssetTypeName() const
{
	return GetStaticAssetTypeName();
}

FTextFormat FShaderCompilingManager::GetAssetNameFormat() const
{
	return LOCTEXT("ShaderNameFormat", "{0}|plural(one=Shader,other=Shaders)");
}

TArrayView<FName> FShaderCompilingManager::GetDependentTypeNames() const
{
#if WITH_EDITOR
	static FName DependentTypeNames[] = 
	{
		// Texture can require materials to be updated,
		// they should be processed first to avoid unecessary material updates.
		FTextureCompilingManager::GetStaticAssetTypeName() 
	};
	return TArrayView<FName>(DependentTypeNames);
#else
	return TArrayView<FName>();
#endif	
}

int32 FShaderCompilingManager::GetNumRemainingAssets() const
{
	// Currently, jobs are difficult to track but the purpose of the GetNumRemainingAssets function is to never return 0
	// if there are still shaders that have not had their primitives updated on the render thread.
	// So we track jobs first and when everything is finished compiling but are still lying around in other structures
	// waiting to be further processed, we show those numbers and ultimately we always return 1 unless IsCompiling() is false.
	return FMath::Max3(GetNumRemainingJobs(), ShaderMapJobs.Num() + PendingFinalizeShaderMaps.Num(), IsCompiling() ? 1 : 0);
}

void FShaderCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	ProcessAsyncResults(bLimitExecutionTime, false);
}

int32 FShaderCompilingManager::GetNumPendingJobs() const
{
	return AllJobs.GetNumPendingJobs();
}

int32 FShaderCompilingManager::GetNumOutstandingJobs() const
{
	return AllJobs.GetNumOutstandingJobs();
}

FShaderCompilingManager::EDumpShaderDebugInfo FShaderCompilingManager::GetDumpShaderDebugInfo() const
{
	if (GDumpShaderDebugInfo < EDumpShaderDebugInfo::Never || GDumpShaderDebugInfo > EDumpShaderDebugInfo::OnErrorOrWarning)
	{
		return EDumpShaderDebugInfo::Never;
	}

	return static_cast<FShaderCompilingManager::EDumpShaderDebugInfo>(GDumpShaderDebugInfo);
}

FString FShaderCompilingManager::CreateShaderDebugInfoPath(const FShaderCompilerInput& ShaderCompilerInput) const
{
	FString DumpDebugInfoPath = ShaderCompilerInput.DumpDebugInfoRootPath / ShaderCompilerInput.DebugGroupName + ShaderCompilerInput.DebugExtension;

	// Sanitize the name to be used as a path
	// List mostly comes from set of characters not allowed by windows in a path.  Just try to rename a file and type one of these for the list.
	DumpDebugInfoPath.ReplaceInline(TEXT("<"), TEXT("("));
	DumpDebugInfoPath.ReplaceInline(TEXT(">"), TEXT(")"));
	DumpDebugInfoPath.ReplaceInline(TEXT("::"), TEXT("=="));
	DumpDebugInfoPath.ReplaceInline(TEXT("|"), TEXT("_"));
	DumpDebugInfoPath.ReplaceInline(TEXT("*"), TEXT("-"));
	DumpDebugInfoPath.ReplaceInline(TEXT("?"), TEXT("!"));
	DumpDebugInfoPath.ReplaceInline(TEXT("\""), TEXT("\'"));

	if (!IFileManager::Get().DirectoryExists(*DumpDebugInfoPath))
	{
		if (!IFileManager::Get().MakeDirectory(*DumpDebugInfoPath, true))
		{
			const uint32 ErrorCode = FPlatformMisc::GetLastError();
			UE_LOG(LogShaderCompilers, Warning, TEXT("Last Error %u: Failed to create directory for shader debug info '%s'. Try enabling large file paths or r.DumpShaderDebugShortNames."), ErrorCode, *DumpDebugInfoPath);
			return FString();
		}
	}

	return DumpDebugInfoPath;
}

bool FShaderCompilingManager::ShouldRecompileToDumpShaderDebugInfo(const FShaderCompileJob& Job) const
{
	return ShouldRecompileToDumpShaderDebugInfo(Job.Input, Job.Output, Job.bSucceeded);
}

bool FShaderCompilingManager::ShouldRecompileToDumpShaderDebugInfo(const FShaderCompilerInput& Input, const FShaderCompilerOutput& Output, bool bSucceeded) const
{
	if (Input.DumpDebugInfoPath.IsEmpty())
	{
		const EDumpShaderDebugInfo DumpShaderDebugInfo = GetDumpShaderDebugInfo();

		if (DumpShaderDebugInfo == EDumpShaderDebugInfo::OnError)
		{
			return !bSucceeded;
		}
		else if (DumpShaderDebugInfo == EDumpShaderDebugInfo::OnErrorOrWarning)
		{
			return !bSucceeded || Output.Errors.Num() > 0;
		}
	}

	return false;
}

void FShaderCompilingManager::ReleaseJob(FShaderCommonCompileJobPtr& Job)
{
	ReleaseJob(Job.GetReference());
	Job.SafeRelease();
}

void FShaderCompilingManager::ReleaseJob(FShaderCommonCompileJob* Job)
{
	Job->PendingShaderMap.SafeRelease();
	Job->bReleased = true;
	AllJobs.RemoveJob(Job);
}

void FShaderCompilingManager::SubmitJobs(TArray<FShaderCommonCompileJobPtr>& NewJobs, const FString MaterialBasePath, const FString PermutationString)
{
	// make sure no compiling can start if not allowed
	if (!AllowShaderCompiling())
	{
		return;
	}
	
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::SubmitJobs);
	check(!FPlatformProperties::RequiresCookedData());

	if (NewJobs.Num() == 0)
	{
		return;
	}

	check(GShaderCompilerStats);
	if (FShaderCompileJob* SingleJob = NewJobs[0]->GetSingleShaderJob()) //assume that all jobs are for the same platform
	{
		GShaderCompilerStats->RegisterCompiledShaders(NewJobs.Num(), SingleJob->Input.Target.GetPlatform(), MaterialBasePath, PermutationString);
	}
	else
	{
		GShaderCompilerStats->RegisterCompiledShaders(NewJobs.Num(), SP_NumPlatforms, MaterialBasePath, PermutationString);
	}

	{
		FScopeLock Lock(&CompileQueueSection);
		for (auto& Job : NewJobs)
		{
			FPendingShaderMapCompileResultsPtr& PendingShaderMap = ShaderMapJobs.FindOrAdd(Job->Id);
			if (!PendingShaderMap)
			{
				PendingShaderMap = new FPendingShaderMapCompileResults();
			}
			PendingShaderMap->NumPendingJobs.Increment();
			Job->PendingShaderMap = PendingShaderMap;
		}
	}

	AllJobs.SubmitJobs(NewJobs);

	UpdateNumRemainingAssets();
}

bool FShaderCompilingManager::IsCompilingShaderMap(uint32 Id)
{
	if (Id != 0u)
	{
		FScopeLock Lock(&CompileQueueSection);
		FPendingShaderMapCompileResultsPtr* PendingShaderMapPtr = ShaderMapJobs.Find(Id);
		if (PendingShaderMapPtr)
		{
			return true;
		}

		FShaderMapFinalizeResults* FinalizedShaderMapPtr = PendingFinalizeShaderMaps.Find(Id);
		if (FinalizedShaderMapPtr)
		{
			return true;
		}
	}
	return false;
}

FShaderCompileJob* FShaderCompilingManager::PrepareShaderCompileJob(uint32 Id, const FShaderCompileJobKey& Key, EShaderCompileJobPriority Priority)
{
	// don't allow any jobs if not allowed
	if (!AllowShaderCompiling())
	{
		return nullptr;
	}

	return AllJobs.PrepareJob(Id, Key, Priority);
}

FShaderPipelineCompileJob* FShaderCompilingManager::PreparePipelineCompileJob(uint32 Id, const FShaderPipelineCompileJobKey& Key, EShaderCompileJobPriority Priority)
{
	// don't allow any jobs if not allowed
	if (!AllowShaderCompiling())
	{
		return nullptr;
	}

	return AllJobs.PrepareJob(Id, Key, Priority);
}

void FShaderCompilingManager::ProcessFinishedJob(FShaderCommonCompileJob* FinishedJob)
{
	AllJobs.ProcessFinishedJob(FinishedJob);
}

/** Launches the worker, returns the launched process handle. */
FProcHandle FShaderCompilingManager::LaunchWorker(const FString& WorkingDirectory, uint32 InProcessId, uint32 ThreadId, const FString& WorkerInputFile, const FString& WorkerOutputFile)
{
	// don't allow any jobs if not allowed
	if (!AllowShaderCompiling())
	{
		return FProcHandle();
	}

	// Setup the parameters that the worker application needs
	// Surround the working directory with double quotes because it may contain a space 
	// WorkingDirectory ends with a '\', so we have to insert another to meet the Windows commandline parsing rules 
	// http://msdn.microsoft.com/en-us/library/17w5ykft.aspx 
	// Use IFileManager to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	FString WorkerAbsoluteDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*WorkingDirectory);
	FPaths::NormalizeDirectoryName(WorkerAbsoluteDirectory);
	FString WorkerParameters = FString(TEXT("\"")) + WorkerAbsoluteDirectory + TEXT("/\" ") + FString::FromInt(InProcessId) + TEXT(" ") + FString::FromInt(ThreadId) + TEXT(" ") + WorkerInputFile + TEXT(" ") + WorkerOutputFile;
	WorkerParameters += FString(TEXT(" -communicatethroughfile "));
	if ( GIsBuildMachine )
	{
		WorkerParameters += FString::Printf(TEXT(" -TimeToLive=%f -buildmachine"), GBuildWorkerTimeToLive);
	}
	else
	{
		WorkerParameters += FString::Printf(TEXT(" -TimeToLive=%f"), GRegularWorkerTimeToLive);
	}
	if (PLATFORM_LINUX) //-V560
	{
		// suppress log generation as much as possible
		WorkerParameters += FString(TEXT(" -logcmds=\"Global None\" "));

		if (UE_BUILD_DEBUG)
		{
			// when running a debug build under Linux, make SCW crash with core for easier debugging
			WorkerParameters += FString(TEXT(" -core "));
		}
	}
	WorkerParameters += FCommandLine::GetSubprocessCommandline();

	// Launch the worker process
	int32 PriorityModifier = -1; // below normal
	GConfig->GetInt(TEXT("DevOptions.Shaders"), TEXT("WorkerProcessPriority"), PriorityModifier, GEngineIni);

	if (DEBUG_SHADERCOMPILEWORKER)
	{
		// Note: Set breakpoint here and launch the ShaderCompileWorker with WorkerParameters a cmd-line
		const TCHAR* WorkerParametersText = *WorkerParameters;
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Launching shader compile worker w/ WorkerParameters\n\t%s\n"), WorkerParametersText);
		FProcHandle DummyHandle;
		return DummyHandle;
	}
	else
	{
#if UE_BUILD_DEBUG && PLATFORM_LINUX
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Launching shader compile worker:\n\t%s\n"), *WorkerParameters);
#endif
		// Disambiguate between SCW.exe missing vs other errors.
		static bool bFirstLaunch = true;
		uint32 WorkerId = 0;
		FProcHandle WorkerHandle = FPlatformProcess::CreateProc(*ShaderCompileWorkerName, *WorkerParameters, true, false, false, &WorkerId, PriorityModifier, nullptr, nullptr);
		if (WorkerHandle.IsValid())
		{
			// Process launched at least once successfully
			bFirstLaunch = false;
		}
		else
		{
			// If this doesn't error, the app will hang waiting for jobs that can never be completed
			if (bFirstLaunch)
			{
				// When using source builds users are likely to make a mistake of not building SCW (e.g. in particular on Linux, even though default makefile target builds it).
				// Make the engine exit gracefully with a helpful message instead of a crash.
				static bool bShowedMessageBox = false;
				if (!bShowedMessageBox && !IsRunningCommandlet() && !FApp::IsUnattended())
				{
					bShowedMessageBox = true;
					FText ErrorMessage = FText::Format(LOCTEXT("LaunchingShaderCompileWorkerFailed", "Unable to launch {0} - make sure you built ShaderCompileWorker."), FText::FromString(ShaderCompileWorkerName));
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(),
												 *LOCTEXT("LaunchingShaderCompileWorkerFailedTitle", "Unable to launch ShaderCompileWorker.").ToString());
				}
				UE_LOG(LogShaderCompilers, Error, TEXT("Couldn't launch %s! Make sure you build ShaderCompileWorker."), *ShaderCompileWorkerName);
				// duplicate to printf() since threaded logs may not be always flushed
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Couldn't launch %s! Make sure you build ShaderCompileWorker.\n"), *ShaderCompileWorkerName);
				FPlatformMisc::RequestExitWithStatus(true, 1);
			}
			else
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Couldn't launch %s!"), *ShaderCompileWorkerName);
			}
		}

		return WorkerHandle;
	}
}

void FShaderCompilingManager::AddCompiledResults(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps, int32 ShaderMapIdx, const FShaderMapFinalizeResults& Results)
{
	// merge with the previous unprocessed jobs, if any
	if (FShaderMapCompileResults const* PrevResults = CompiledShaderMaps.Find(ShaderMapIdx))
	{
		FShaderMapFinalizeResults NewResults(Results);

		NewResults.bAllJobsSucceeded = NewResults.bAllJobsSucceeded && PrevResults->bAllJobsSucceeded;
		NewResults.bSkipResultProcessing = NewResults.bSkipResultProcessing || PrevResults->bSkipResultProcessing;
		NewResults.TimeStarted = FMath::Min(NewResults.TimeStarted, PrevResults->TimeStarted);
		NewResults.bIsHung = NewResults.bIsHung || PrevResults->bIsHung;
		NewResults.FinishedJobs.Append(PrevResults->FinishedJobs);

		CompiledShaderMaps.Add(ShaderMapIdx, NewResults);
	}
	else
	{
		CompiledShaderMaps.Add(ShaderMapIdx, Results);
	}
}

/** Flushes all pending jobs for the given shader maps. */
void FShaderCompilingManager::BlockOnShaderMapCompletion(const TArray<int32>& ShaderMapIdsToFinishCompiling, TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps)
{
	// never block if no compiling, just in case
	if (!AllowShaderCompiling())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::BlockOnShaderMapCompletion);
	SCOPE_STALL_REPORTER(FShaderCompilingManager::BlockOnShaderMapCompletion, 2.0);

	COOK_STAT(FScopedDurationTimer BlockingTimer(ShaderCompilerCookStats::BlockingTimeSec));
	if (bAllowAsynchronousShaderCompiling)
	{
		// Calculate how many shader jobs there are total to provide the slow task with the correct amount of work.
		int NumJobs = 0;
		{
			FScopeLock Lock(&CompileQueueSection);
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < ShaderMapIdsToFinishCompiling.Num(); ShaderMapIndex++)
			{
				FPendingShaderMapCompileResultsPtr* ResultsPtr = ShaderMapJobs.Find(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
				if (ResultsPtr)
				{
					FShaderMapCompileResults* Results = *ResultsPtr;
					NumJobs += Results->NumPendingJobs.GetValue();
				}
			}
		}

		FScopedSlowTask SlowTask(NumJobs, FText::Format(LOCTEXT("BlockOnShaderMapCompletion", "Compiling Shaders ({0})"), NumJobs), GIsEditor && !IsRunningCommandlet());

		int32 NumPendingJobs = 0;
		// Keep track of previous number of pending jobs so we can update the slow task with the amount of work done.
		int32 NumPreviousPendingJobs = NumJobs;
		int32 LogCounter = 0;
		do 
		{
			NumPendingJobs = 0;
			{
				// Lock CompileQueueSection so we can access the input and output queues
				FScopeLock Lock(&CompileQueueSection);

				for (int32 ShaderMapIndex = 0; ShaderMapIndex < ShaderMapIdsToFinishCompiling.Num(); ShaderMapIndex++)
				{
					FPendingShaderMapCompileResultsPtr* ResultsPtr = ShaderMapJobs.Find(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
					if (ResultsPtr)
					{
						FShaderMapCompileResults* Results = *ResultsPtr;

						if (Results->NumPendingJobs.GetValue() == 0)
						{
							if (Results->FinishedJobs.Num() > 0)
							{
								AddCompiledResults(CompiledShaderMaps, ShaderMapIdsToFinishCompiling[ShaderMapIndex], *Results);
							}
							ShaderMapJobs.Remove(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
						}
						else
						{
							Results->CheckIfHung();
							NumPendingJobs += Results->NumPendingJobs.GetValue();
						}
					}
				}
			}

			if (NumPendingJobs > 0)
			{
				const float SleepTime =.01f;
				
				// We need to manually tick the Distributed build controller while the game thread is blocked
				// otherwise we can get stuck in a infinite loop waiting for jobs that never will be done
				// because for example, some controllers depend on the HTTP module which needs to be ticked in the main thread
				if (BuildDistributionController && IsInGameThread())
				{
					BuildDistributionController->Tick(SleepTime);
				}

				// Progress the slow task with how many jobs we've completed since last tick.  Update the slow task message with the current number of pending jobs
				// we are waiting on.
				const int32 CompletedJobsSinceLastTick = NumPreviousPendingJobs - NumPendingJobs;
				SlowTask.EnterProgressFrame(CompletedJobsSinceLastTick, FText::Format(LOCTEXT("BlockOnShaderMapCompletion", "Compiling Shaders ({0})"), NumPendingJobs));
				NumPreviousPendingJobs = NumPendingJobs;
				
				// Yield CPU time while waiting
				FPlatformProcess::Sleep(SleepTime);

				// Flush threaded logs around every 500ms or so based on Sleep of 0.01f seconds above
				if (++LogCounter > 50)
				{
					LogCounter = 0;
					GLog->FlushThreadedLogs(EOutputDeviceRedirectorFlushOptions::Async);
				}
			}
		} 
		while (NumPendingJobs > 0);
	}
	else
	{
		int32 NumActiveWorkers = 0;
		do 
		{
			for (const auto& Thread : Threads)
			{
				NumActiveWorkers = Thread->CompilingLoop();
			}
		} 
		while (NumActiveWorkers > 0);

		check(AllJobs.GetNumPendingJobs() == 0);

		for (int32 ShaderMapIndex = 0; ShaderMapIndex < ShaderMapIdsToFinishCompiling.Num(); ShaderMapIndex++)
		{
			const FPendingShaderMapCompileResultsPtr* ResultsPtr = ShaderMapJobs.Find(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);

			if (ResultsPtr)
			{
				const FShaderMapCompileResults* Results = *ResultsPtr;
				check(Results->NumPendingJobs.GetValue() == 0);
				check(Results->FinishedJobs.Num() > 0);

				AddCompiledResults(CompiledShaderMaps, ShaderMapIdsToFinishCompiling[ShaderMapIndex], *Results);
				ShaderMapJobs.Remove(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
			}
		}
	}

	UpdateNumRemainingAssets();
}

void FShaderCompilingManager::BlockOnAllShaderMapCompletion(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps)
{
	// never block if no compiling, just in case
	if (!AllowShaderCompiling())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::BlockOnAllShaderMapCompletion);
	SCOPE_STALL_REPORTER(FShaderCompilingManager::BlockOnAllShaderMapCompletion, 2.0);

	COOK_STAT(FScopedDurationTimer BlockingTimer(ShaderCompilerCookStats::BlockingTimeSec));
	if (bAllowAsynchronousShaderCompiling)
	{
		// Calculate how many shader jobs there are total to provide the slow task with the correct amount of work.
		int NumJobs = 0;
		{
			FScopeLock Lock(&CompileQueueSection);
			for (TMap<int32, FPendingShaderMapCompileResultsPtr>::TIterator It(ShaderMapJobs); It; ++It)
			{
				FShaderMapCompileResults* Results = It.Value();
				NumJobs += Results->NumPendingJobs.GetValue();
			}
		}

		FScopedSlowTask SlowTask(NumJobs, FText::Format(LOCTEXT("BlockOnAllShaderMapCompletion", "Compiling Shaders ({0})"), NumJobs), NumJobs && GIsEditor && !IsRunningCommandlet());
		SlowTask.MakeDialog(false, true);

		int32 NumPendingJobs = 0;
		// Keep track of previous number of pending jobs so we can update the slow task with the amount of work done.
		int32 NumPreviousPendingJobs = NumJobs;

		do 
		{
			NumPendingJobs = 0;
			{
				// Lock CompileQueueSection so we can access the input and output queues
				FScopeLock Lock(&CompileQueueSection);

				int32 ShaderMapIdx = 0;
				for (TMap<int32, FPendingShaderMapCompileResultsPtr>::TIterator It(ShaderMapJobs); It; ++It)
				{
					FShaderMapCompileResults* Results = It.Value();

					if (Results->NumPendingJobs.GetValue() == 0)
					{
						AddCompiledResults(CompiledShaderMaps, It.Key(), *Results);
						It.RemoveCurrent();
					}
					else
					{
						Results->CheckIfHung();
						NumPendingJobs += Results->NumPendingJobs.GetValue();
					}
				}
			}

			if (NumPendingJobs > 0)
			{
				const float SleepTime =.01f;
				
				// We need to manually tick the Distributed build controller while the game thread is blocked
				// otherwise we can get stuck in a infinite loop waiting for jobs that never will be done
				// because for example, some controllers depend on the HTTP module which needs to be ticked in the main thread
				if (BuildDistributionController && IsInGameThread())
				{
					BuildDistributionController->Tick(SleepTime);
				}

				// Progress the slow task with how many jobs we've completed since last tick.  Update the slow task message with the current number of pending jobs
				// we are waiting on.
				const int32 CompletedJobsSinceLastTick = NumPreviousPendingJobs - NumPendingJobs;
				SlowTask.EnterProgressFrame(CompletedJobsSinceLastTick, FText::Format(LOCTEXT("BlockOnAllShaderMapCompletion", "Compiling Shaders ({0})"), NumPendingJobs));
				NumPreviousPendingJobs = NumPendingJobs;
				
				// Yield CPU time while waiting
				FPlatformProcess::Sleep(SleepTime);
			}
		} 
		while (NumPendingJobs > 0);
	}
	else
	{
		int32 NumActiveWorkers = 0;
		do 
		{
			for (const auto& Thread : Threads)
			{
				NumActiveWorkers = Thread->CompilingLoop();
			}

			for (TMap<int32, FPendingShaderMapCompileResultsPtr>::TIterator It(ShaderMapJobs); It; ++It)
			{
				FShaderMapCompileResults* Results = It.Value();
				Results->CheckIfHung();
			}
		} 
		while (NumActiveWorkers > 0);

		check(AllJobs.GetNumPendingJobs() == 0);

		for (TMap<int32, FPendingShaderMapCompileResultsPtr>::TIterator It(ShaderMapJobs); It; ++It)
		{
			const FShaderMapCompileResults* Results = It.Value();
			check(Results->NumPendingJobs.GetValue()== 0);

			AddCompiledResults(CompiledShaderMaps, It.Key(), *Results);
			It.RemoveCurrent();
		}
	}

	UpdateNumRemainingAssets();
}

namespace
{
	void PropagateGlobalShadersToAllPrimitives()
	{
		// Re-register everything to work around FShader lifetime issues - it currently lives and dies with the
		// shadermap it is stored in, while cached MDCs can reference its memory. Re-registering will
		// re-create the cache.
		TRACE_CPUPROFILER_EVENT_SCOPE(PropagateGlobalShadersToAllPrimitives);

		FObjectCacheContextScope ObjectCacheScope;
		TSet<FSceneInterface*> ScenesToUpdate;
		TIndirectArray<FComponentRecreateRenderStateContext> ComponentContexts;
		for (UPrimitiveComponent* PrimitiveComponent : ObjectCacheScope.GetContext().GetPrimitiveComponents())
		{
			if (PrimitiveComponent->IsRenderStateCreated())
			{
				ComponentContexts.Add(new FComponentRecreateRenderStateContext(PrimitiveComponent, &ScenesToUpdate));
#if WITH_EDITOR
				if (PrimitiveComponent->HasValidSettingsForStaticLighting(false))
				{
					FStaticLightingSystemInterface::OnPrimitiveComponentUnregistered.Broadcast(PrimitiveComponent);
					FStaticLightingSystemInterface::OnPrimitiveComponentRegistered.Broadcast(PrimitiveComponent);
				}
#endif
			}
		}

		UpdateAllPrimitiveSceneInfosForScenes(ScenesToUpdate);
		ComponentContexts.Empty();
		UpdateAllPrimitiveSceneInfosForScenes(ScenesToUpdate);
	}
}

void FShaderCompilingManager::ProcessCompiledShaderMaps(
	TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps, 
	float TimeBudget)
{
	// never process anything if not allowed, just in case
	if (!AllowShaderCompiling())
	{
		return;
	}

#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::ProcessCompiledShaderMaps);

	TMap<TRefCountPtr<FMaterial>, TRefCountPtr<FMaterialShaderMap>> MaterialsToUpdate;
	TArray<TRefCountPtr<FMaterial>> MaterialsToReleaseCompilingId;

	// Process compiled shader maps in FIFO order, in case a shader map has been enqueued multiple times,
	// Which can happen if a material is edited while a background compile is going on
	for (TMap<int32, FShaderMapFinalizeResults>::TIterator ProcessIt(CompiledShaderMaps); ProcessIt; ++ProcessIt)
	{
		const uint32 CompilingId = ProcessIt.Key();
		FShaderMapFinalizeResults& CompileResults = ProcessIt.Value();
		if (CompileResults.bSkipResultProcessing)
		{
			ProcessIt.RemoveCurrent();
			continue;
		}

		TRefCountPtr<FMaterialShaderMap> CompilingShaderMap = FMaterialShaderMap::FindCompilingShaderMap(CompilingId);

		if (CompilingShaderMap)
		{
			TArray<TRefCountPtr<FMaterial>>& MaterialDependencies = CompilingShaderMap->CompilingMaterialDependencies;

			TArray<FString> Errors;
			FString DumpedSource;
			TArray<FShaderCommonCompileJobPtr>& ResultArray = CompileResults.FinishedJobs;

			bool bSuccess = true;
			for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
			{
				FShaderCommonCompileJob& CurrentJob = *ResultArray[JobIndex];

				if (FShaderCompileJob* SingleJob = CurrentJob.GetSingleShaderJob())
				{
					const bool bCheckSucceeded = CheckSingleJob(*SingleJob, Errors, (NumDumpedShaderSources < GMaxNumDumpedShaderSources ? &DumpedSource : nullptr));
					bSuccess = bCheckSucceeded && bSuccess;
				}
				else
				{
					FShaderPipelineCompileJob* PipelineJob = CurrentJob.GetShaderPipelineJob();
					for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
					{
						const bool bCheckSucceeded = CheckSingleJob(*PipelineJob->StageJobs[Index], Errors, (NumDumpedShaderSources < GMaxNumDumpedShaderSources ? &DumpedSource : nullptr));
						bSuccess = PipelineJob->StageJobs[Index]->bSucceeded && bCheckSucceeded && bSuccess;
					}
				}
			}

			FMaterialShaderMap* ShaderMapToUseForRendering = nullptr;
			if (bSuccess)
			{
				int32 JobIndex = 0;
				if (ResultArray.Num() > 0)
				{
					CompilingShaderMap->ProcessCompilationResults(ResultArray, JobIndex, TimeBudget);
					{
						FScopeLock Lock(&CompileQueueSection);
						for (int32 i = 0; i < JobIndex; ++i)
						{
							ReleaseJob(ResultArray[i]);
						}
					}
					ResultArray.RemoveAt(0, JobIndex);
				}

				// Make a clone of the compiling shader map to use for rendering
				// This will allow rendering to proceed with the clone, while async compilation continues to potentially update the compiling shader map
				double StartTime = FPlatformTime::Seconds();
				ShaderMapToUseForRendering = CompilingShaderMap->AcquireFinalizedClone();
				TimeBudget -= (FPlatformTime::Seconds() - StartTime);
			}

			if (!bSuccess || ResultArray.Num() == 0)
			{
				ProcessIt.RemoveCurrent();
			}

#if DEBUG_INFINITESHADERCOMPILE
			UE_LOG(LogTemp, Display, TEXT("Finished compile of shader map 0x%08X%08X"), (int)((int64)(ShaderMap.GetReference()) >> 32), (int)((int64)(ShaderMap.GetReference())));
#endif
			int32 NumIncompleteMaterials = 0;
			int32 MaterialIndex = 0;
			while (MaterialIndex < MaterialDependencies.Num())
			{
				FMaterial* Material = MaterialDependencies[MaterialIndex];
				check(Material->GetGameThreadCompilingShaderMapId() == CompilingShaderMap->GetCompilingId());

#if DEBUG_INFINITESHADERCOMPILE
				UE_LOG(LogTemp, Display, TEXT("Shader map %s complete, GameThreadShaderMap 0x%08X%08X, marking material %s as finished"), *ShaderMap->GetFriendlyName(), (int)((int64)(ShaderMap.GetReference()) >> 32), (int)((int64)(ShaderMap.GetReference())), *Material->GetFriendlyName());
				UE_LOG(LogTemp, Display, TEXT("Marking material as finished 0x%08X%08X"), (int)((int64)(Material) >> 32), (int)((int64)(Material)));
#endif
				//Material->RemoveOutstandingCompileId(ShaderMap->CompilingId);

				bool bReleaseCompilingId = false;

				// Only process results that still match the ID which requested a compile
				// This avoids applying shadermaps which are out of date and a newer one is in the async compiling pipeline
				if (Material->GetMaterialId() != CompilingShaderMap->GetShaderMapId().BaseMaterialId)
				{
					bReleaseCompilingId = true;
				}
				else if (bSuccess)
				{
					MaterialsToUpdate.Add(Material, ShaderMapToUseForRendering);
					if (ShaderMapToUseForRendering->IsComplete(Material, true))
					{
						bReleaseCompilingId = true;
					}
					else
					{
						++NumIncompleteMaterials;
					}

					if (GShowShaderWarnings && Errors.Num() > 0)
					{
						UE_LOG(LogShaderCompilers, Warning, TEXT("Warnings while compiling Material %s for platform %s:"),
							*Material->GetDebugName(),
							*LegacyShaderPlatformToShaderFormat(ShaderMapToUseForRendering->GetShaderPlatform()).ToString());
						for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
						{
							UE_LOG(LogShaders, Warning, TEXT("  %s"), *Errors[ErrorIndex]);
						}
					}
				}
				else
				{
					bReleaseCompilingId = true;
					// Propagate error messages
					Material->CompileErrors = Errors;

					MaterialsToUpdate.Add(Material, nullptr);

					if (!DumpedSource.IsEmpty() && NumDumpedShaderSources < GMaxNumDumpedShaderSources)
					{
						// Limit number of preprocessed shaders to dump to the log as they are quite large
						DumpShaderSourceToLog(DumpedSource);
						++NumDumpedShaderSources;
					}

					if (Material->IsDefaultMaterial())
					{
						FString ErrorString;

						// Log the errors unsuppressed before the fatal error, so it's always obvious from the log what the compile error was
						for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
						{
							ErrorString += FString::Printf(TEXT("  %s\n"), *Errors[ErrorIndex]);
						}

						ErrorString += FString::Printf(TEXT("Failed to compile default material %s!"), *Material->GetBaseMaterialPathName());
						// Assert if a default material could not be compiled, since there will be nothing for other failed materials to fall back on.
						UE_LOG(LogShaderCompilers, Fatal, TEXT("%s"), *ErrorString);
					}
					
					FString ErrorString;

					ErrorString += FString::Printf(TEXT("Failed to compile Material %s for platform %s, Default Material will be used in game.\n"),
						*Material->GetDebugName(), *LegacyShaderPlatformToShaderFormat(CompilingShaderMap->GetShaderPlatform()).ToString());

					for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
					{
						FString ErrorMessage = Errors[ErrorIndex];
						// Work around build machine string matching heuristics that will cause a cook to fail
						ErrorMessage.ReplaceInline(TEXT("error "), TEXT("err0r "), ESearchCase::CaseSensitive);
						ErrorString += FString::Printf(TEXT("  %s\n"), *ErrorMessage);
					}

					UE_LOG(LogShaderCompilers, Warning, TEXT("%s"), *ErrorString);
				}

				if (bReleaseCompilingId)
				{
					check(Material->GameThreadCompilingShaderMapId != 0u);
					Material->GameThreadCompilingShaderMapId = 0u;
					Material->GameThreadPendingCompilerEnvironment.SafeRelease();
					MaterialDependencies.RemoveAt(MaterialIndex);
					MaterialsToReleaseCompilingId.Add(Material);
				}
				else
				{
					++MaterialIndex;
				}
			}

			if (NumIncompleteMaterials == 0)
			{
				CompilingShaderMap->bCompiledSuccessfully = bSuccess;
				CompilingShaderMap->bCompilationFinalized = true;
				if (bSuccess)
				{
					check(ShaderMapToUseForRendering);
					ShaderMapToUseForRendering->bCompiledSuccessfully = true;
					ShaderMapToUseForRendering->bCompilationFinalized = true;
					if (ShaderMapToUseForRendering->bIsPersistent)
					{
						ShaderMapToUseForRendering->SaveToDerivedDataCache();
					}
				}

				CompilingShaderMap->ReleaseCompilingId();
			}

			if (TimeBudget < 0)
			{
				break;
			}
		}
		else if (CompilingId == GlobalShaderMapId)
		{
			ProcessCompiledGlobalShaders(CompileResults.FinishedJobs);
			
			PropagateGlobalShadersToAllPrimitives();

			{
				FScopeLock Lock(&CompileQueueSection);
				for (auto& Job : CompileResults.FinishedJobs)
				{
					ReleaseJob(Job);
				}
			}
			ProcessIt.RemoveCurrent();
		}
		else
		{
			// ShaderMap was removed from compiling list or is being used by another type of shader map which is maintaining a reference
			// to the results, either way the job can be released
			{
				FScopeLock Lock(&CompileQueueSection);
				for (auto& Job : CompileResults.FinishedJobs)
				{
					ReleaseJob(Job);
				}
			}
			ProcessIt.RemoveCurrent();
		}
	}

	if (MaterialsToReleaseCompilingId.Num() > 0)
	{
		ENQUEUE_RENDER_COMMAND(ReleaseCompilingShaderMapIds)([MaterialsToReleaseCompilingId = MoveTemp(MaterialsToReleaseCompilingId)](FRHICommandListImmediate& RHICmdList)
		{
			for (FMaterial* Material : MaterialsToReleaseCompilingId)
			{
				check(Material->RenderingThreadCompilingShaderMapId != 0u);
				Material->RenderingThreadCompilingShaderMapId = 0u;
				Material->RenderingThreadPendingCompilerEnvironment.SafeRelease();
			}
		});
	}

	if (MaterialsToUpdate.Num() > 0)
	{
		FMaterial::SetShaderMapsOnMaterialResources(MaterialsToUpdate);

		for (const auto& It : MaterialsToUpdate)
		{
			It.Key->NotifyCompilationFinished();
		}

		if (FApp::CanEverRender())
		{
			// This empties MaterialsToUpdate, see the comment inside the function for the reason.
			PropagateMaterialChangesToPrimitives(MaterialsToUpdate);

			FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		}
	}

	GShaderCompilingManager->PrintStats();

	UpdateNumRemainingAssets();
#endif // WITH_EDITOR
}

void FShaderCompilingManager::PropagateMaterialChangesToPrimitives(TMap<TRefCountPtr<FMaterial>, TRefCountPtr<FMaterialShaderMap>>& MaterialsToUpdate)
{
	// don't perform any work if no compiling
	if (!AllowShaderCompiling())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::PropagateMaterialChangesToPrimitives);

	TSet<FSceneInterface*> ScenesToUpdate;
	FObjectCacheContextScope ObjectCacheScope;
	TIndirectArray<FComponentRecreateRenderStateContext> ComponentContexts;
	for (UPrimitiveComponent* PrimitiveComponent : ObjectCacheScope.GetContext().GetPrimitiveComponents())
	{
		if (PrimitiveComponent->IsRenderStateCreated())
		{
			bool bPrimitiveIsDependentOnMaterial = false;

			// Note: relying on GetUsedMaterials to be accurate, or else we won't propagate to the right primitives and the renderer will crash later
			// FPrimitiveSceneProxy::VerifyUsedMaterial is used to make sure that all materials used for rendering are reported in GetUsedMaterials
			TObjectCacheIterator<UMaterialInterface> UsedMaterials = ObjectCacheScope.GetContext().GetUsedMaterials(PrimitiveComponent);
			if (!UsedMaterials.IsEmpty())
			{
				for (TMap<TRefCountPtr<FMaterial>, TRefCountPtr<FMaterialShaderMap>>::TConstIterator MaterialIt(MaterialsToUpdate); MaterialIt; ++MaterialIt)
				{
					FMaterial* UpdatedMaterial = MaterialIt.Key();
					UMaterialInterface* UpdatedMaterialInterface = UpdatedMaterial->GetMaterialInterface();

					if (UpdatedMaterialInterface)
					{
						for (UMaterialInterface* TestMaterial : UsedMaterials)
						{
							if (TestMaterial && (TestMaterial == UpdatedMaterialInterface || TestMaterial->IsDependent(UpdatedMaterialInterface)))
							{
								bPrimitiveIsDependentOnMaterial = true;
								break;
							}
						}
					}
				}

				if (bPrimitiveIsDependentOnMaterial)
				{
					ComponentContexts.Add(new FComponentRecreateRenderStateContext(PrimitiveComponent, &ScenesToUpdate));
				}
			}
		}
	}

	// Recreating the render state for the primitives may end up recreating the material resources if some materials are missing some usage flags.
	// For example, if some materials are not marked as used with static lighting and we build lightmaps, UMaterialInstance::CheckMaterialUsage
	// will catch the problem and try to set the flag. However, since MaterialsToUpdate stores smart pointers, the material resources will have
	// a refcount of 2, so the FMaterial destructor will trigger a check failure because the refcount doesn't reach 0. Empty this map before
	// recreating the render state to allow resources to be deleted cleanly.
	MaterialsToUpdate.Empty();

	UpdateAllPrimitiveSceneInfosForScenes(ScenesToUpdate);
	ComponentContexts.Empty();
	UpdateAllPrimitiveSceneInfosForScenes(ScenesToUpdate);
}


/**
 * Shutdown the shader compile manager
 * this function should be used when ending the game to shutdown shader compile threads
 * will not complete current pending shader compilation
 */
void FShaderCompilingManager::Shutdown()
{
	// Shutdown has been moved to the destructor because the shader compiler lifetime is expected to
	// be longer than other asset compilers, otherwise niagara compilations might get stuck.
}

void FShaderCompilingManager::PrintStats(bool bForceLogIgnoringTimeInverval)
{
	static double LastTimeStatsPrinted = FPlatformTime::Seconds();
	// do not print if
	//  - job cache is disabled
	//  - not enough time passed since the previous time and we're not forced to print
	if ((!bForceLogIgnoringTimeInverval && GShaderCompilerCacheStatsPrintoutInterval > 0 && FPlatformTime::Seconds() - LastTimeStatsPrinted < GShaderCompilerCacheStatsPrintoutInterval))
	{
		return;
	}
	if (!AllowShaderCompiling())
	{
		return;
	}

	UE_LOG(LogShaderCompilers, Display, TEXT("================================================"));

	if (ShaderCompiler::IsJobCacheEnabled())
	{
		AllJobs.LogCachingStats();
	}

	GShaderCompilerStats->WriteStatSummary();

	UE_LOG(LogShaderCompilers, Display, TEXT("================================================"));

	LastTimeStatsPrinted = FPlatformTime::Seconds();
}

bool FShaderCompilingManager::HandlePotentialRetryOnError(TMap<int32, FShaderMapFinalizeResults>& CompletedShaderMaps)
{
	bool bRetryCompile = false;

	for (TMap<int32, FShaderMapFinalizeResults>::TIterator It(CompletedShaderMaps); It; ++It)
	{
		FShaderMapFinalizeResults& Results = It.Value();

		if (!Results.bAllJobsSucceeded)
		{
			bool bSpecialEngineMaterial = false;
			const FMaterialShaderMap* ShaderMap = FMaterialShaderMap::FindCompilingShaderMap(It.Key());
			if (ShaderMap)
			{
				for (const FMaterial* Material : ShaderMap->CompilingMaterialDependencies)
				{
					if (Material->IsSpecialEngineMaterial())
					{
						bSpecialEngineMaterial = true;
						break;
					}
				}
			}

#if WITH_EDITORONLY_DATA

			if (UE_LOG_ACTIVE(LogShaders, Log) 
				// Always log detailed errors when a special engine material or global shader fails to compile, as those will be fatal errors
				|| bSpecialEngineMaterial 
				|| It.Key() == GlobalShaderMapId)
			{
				TArray<FShaderCommonCompileJobPtr>& CompleteJobs = Results.FinishedJobs;
				TArray<FShaderCommonCompileJob*> ErrorJobs;
				TArray<FString> UniqueErrors;
				TArray<EShaderPlatform> ErrorPlatforms;

				// Gather unique errors
				for (int32 JobIndex = 0; JobIndex < CompleteJobs.Num(); JobIndex++)
				{
					FShaderCommonCompileJob& CurrentJob = *CompleteJobs[JobIndex];
					if (!CurrentJob.bSucceeded)
					{
						FShaderCompileJob* SingleJob = CurrentJob.GetSingleShaderJob();
						if (SingleJob)
						{
							AddErrorsForFailedJob(*SingleJob, ErrorPlatforms, UniqueErrors, ErrorJobs);
						}
						else
						{
							FShaderPipelineCompileJob* PipelineJob = CurrentJob.GetShaderPipelineJob();
							check(PipelineJob);
							for (auto CommonJob : PipelineJob->StageJobs)
							{
								AddErrorsForFailedJob(*CommonJob, ErrorPlatforms, UniqueErrors, ErrorJobs);
							}
						}
					}
				}

				FString TargetShaderPlatformString;

				for (int32 PlatformIndex = 0; PlatformIndex < ErrorPlatforms.Num(); PlatformIndex++)
				{
					if (TargetShaderPlatformString.IsEmpty())
					{
						TargetShaderPlatformString = FDataDrivenShaderPlatformInfo::GetName(ErrorPlatforms[PlatformIndex]).ToString();
					}
					else
					{
						TargetShaderPlatformString += FString(TEXT(", ")) + FDataDrivenShaderPlatformInfo::GetName(ErrorPlatforms[PlatformIndex]).ToString();
					}
				}

				const TCHAR* MaterialName = ShaderMap ? ShaderMap->GetFriendlyName() : TEXT("global shaders");
				FString ErrorString = FString::Printf(TEXT("%i Shader compiler errors compiling %s for platform %s:"), UniqueErrors.Num(), MaterialName, *TargetShaderPlatformString);
				UE_LOG(LogShaderCompilers, Warning, TEXT("%s"), *ErrorString);
				ErrorString += TEXT("\n");
				bool bAnyErrorLikelyToBeCodeError = false;

				for (int32 JobIndex = 0; JobIndex < CompleteJobs.Num(); JobIndex++)
				{
					const FShaderCommonCompileJob& CurrentJob = *CompleteJobs[JobIndex];
					if (!CurrentJob.bSucceeded)
					{
						bAnyErrorLikelyToBeCodeError |= CurrentJob.bErrorsAreLikelyToBeCode;
						const auto* SingleJob = CurrentJob.GetSingleShaderJob();
						if (SingleJob)
						{
							ProcessErrors(*SingleJob, UniqueErrors, ErrorString, NumDumpedShaderSources);
						}
						else
						{
							const auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
							check(PipelineJob);
							for (auto CommonJob : PipelineJob->StageJobs)
							{
								ProcessErrors(*CommonJob, UniqueErrors, ErrorString, NumDumpedShaderSources);
							}
						}
					}
				}

				if (UE_LOG_ACTIVE(LogShaders, Log) && (bAnyErrorLikelyToBeCodeError || bPromptToRetryFailedShaderCompiles || bSpecialEngineMaterial))
				{
#if UE_BUILD_DEBUG
					// Use debug break in debug with the debugger attached, otherwise message box
					if (FPlatformMisc::IsDebuggerPresent())
					{
						// A shader compile error has occurred, see the debug output for information.
						// Double click the errors in the VS.NET output window and the IDE will take you directly to the file and line of the error.
						// Check ErrorJobs for more state on the failed shaders, for example in-memory includes like Material.usf
						UE_DEBUG_BREAK();
						// Set GRetryShaderCompilation to true in the debugger to enable retries in debug
						// NOTE: MaterialTemplate.usf will not be reloaded when retrying!
						bRetryCompile = GRetryShaderCompilation;
					}
					else
#endif	//UE_BUILD_DEBUG
					{
						if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *FText::Format(NSLOCTEXT("UnrealEd", "Error_RetryShaderCompilation", "{0}\r\n\r\nRetry compilation?"),
							FText::FromString(ErrorString)).ToString(), TEXT("Error")) == EAppReturnType::Type::Yes)
						{
							bRetryCompile = true;
						}
					}
				}

				if (bRetryCompile)
				{
					break;
				}
			}
#endif	//WITH_EDITORONLY_DATA
		}
	}

	if (bRetryCompile)
	{
		// Flush the shader file cache so that any changes will be propagated.
		FlushShaderFileCache();

		TArray<int32> MapsToRemove;

		for (TMap<int32, FShaderMapFinalizeResults>::TIterator It(CompletedShaderMaps); It; ++It)
		{
			FShaderMapFinalizeResults& Results = It.Value();

			if (!Results.bAllJobsSucceeded)
			{
				MapsToRemove.Add(It.Key());

				// Reset outputs
				for (int32 JobIndex = 0; JobIndex < Results.FinishedJobs.Num(); JobIndex++)
				{
					FShaderCommonCompileJob& CurrentJob = *Results.FinishedJobs[JobIndex];
					auto* SingleJob = CurrentJob.GetSingleShaderJob();

					// NOTE: Changes to MaterialTemplate.usf before retrying won't work, because the entry for Material.usf in CurrentJob.Environment.IncludeFileNameToContentsMap isn't reset
					if (SingleJob)
					{
						if (GShaderCompilingManager->ShouldRecompileToDumpShaderDebugInfo(*SingleJob))
						{
							SingleJob->Input.DumpDebugInfoPath = GShaderCompilingManager->CreateShaderDebugInfoPath(SingleJob->Input);
						}
						SingleJob->Output = FShaderCompilerOutput();
					}
					else
					{
						auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
						for (auto CommonJob : PipelineJob->StageJobs)
						{
							CommonJob->Output = FShaderCompilerOutput();
							CommonJob->bFinalized = false;
						}
					}
					CurrentJob.bFinalized = false;
				}

				// Send all the shaders from this shader map through the compiler again
				SubmitJobs(Results.FinishedJobs, FString(""), FString(""));
			}
		}

		const int32 OriginalNumShaderMaps = CompletedShaderMaps.Num();

		// Remove the failed shader maps
		for (int32 RemoveIndex = 0; RemoveIndex < MapsToRemove.Num(); RemoveIndex++)
		{
			CompletedShaderMaps.Remove(MapsToRemove[RemoveIndex]);
		}

		check(CompletedShaderMaps.Num() == OriginalNumShaderMaps - MapsToRemove.Num());

		// Block until the failed shader maps have been compiled again
		BlockOnShaderMapCompletion(MapsToRemove, CompletedShaderMaps);

		check(CompletedShaderMaps.Num() == OriginalNumShaderMaps);
	}

	return bRetryCompile;
}

void FShaderMapCompileResults::CheckIfHung()
{
	if (!bIsHung)
	{
		double DurationSoFar = FPlatformTime::Seconds() - TimeStarted;
		if (DurationSoFar >= static_cast<double>(GShaderMapCompilationTimeout))
		{
			bIsHung = true;
			// always produce an error message first, even if going to crash, as the automation controller does not seem to be picking up Fatal messages
			UE_LOG(LogShaderCompilers, Error, TEXT("Hung shadermap detected, time spent compiling: %f seconds, NumPendingJobs: %d, FinishedJobs: %d"),
				DurationSoFar,
				NumPendingJobs.GetValue(),
				FinishedJobs.Num()
			);

			if (GCrashOnHungShaderMaps)
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Crashing on a hung shadermap, time spent compiling: %f seconds, NumPendingJobs: %d, FinishedJobs: %d"),
					DurationSoFar,
					NumPendingJobs.GetValue(),
					FinishedJobs.Num()
				);
			}
		}
	}
}

void FShaderCompilingManager::CancelCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToCancel)
{
	// nothing to cancel here, just in case
	if (!AllowShaderCompiling())
	{
		return;
	}

	check(IsInGameThread());
	check(!FPlatformProperties::RequiresCookedData());

	// Lock CompileQueueSection so we can access the input and output queues
	FScopeLock Lock(&CompileQueueSection);

	int32 TotalNumJobsRemoved = 0;
	for (int32 IdIndex = 0; IdIndex < ShaderMapIdsToCancel.Num(); ++IdIndex)
	{
		int32 MapIdx = ShaderMapIdsToCancel[IdIndex];
		if (const FPendingShaderMapCompileResultsPtr* ResultsPtr = ShaderMapJobs.Find(MapIdx))
		{
			const int32 NumJobsRemoved = AllJobs.RemoveAllPendingJobsWithId(MapIdx);
	
			TotalNumJobsRemoved += NumJobsRemoved;

			FShaderMapCompileResults* ShaderMapJob = *ResultsPtr;
			const int32 PrevNumPendingJobs = ShaderMapJob->NumPendingJobs.Subtract(NumJobsRemoved);
			check(PrevNumPendingJobs >= NumJobsRemoved);

			// The shader map job result should be skipped since it is out of date.
			ShaderMapJob->bSkipResultProcessing = true;
		
			if (PrevNumPendingJobs == NumJobsRemoved && ShaderMapJob->FinishedJobs.Num() == 0)
			{
				//We've removed all the jobs for this shader map so remove it.
				ShaderMapJobs.Remove(MapIdx);
			}
		}

		// Don't continue finalizing once compilation has been canceled
		// the CompilingId has been removed from ShaderMapsBeingCompiled, which will cause crash when attempting to do any further processing
		const int32 NumPendingRemoved = PendingFinalizeShaderMaps.Remove(MapIdx);
	}

	if (TotalNumJobsRemoved > 0)
	{
		UE_LOG(LogShaders, Display, TEXT("CancelCompilation %s, Removed %d jobs"), MaterialName ? MaterialName : TEXT(""), TotalNumJobsRemoved);
	}
}

void FShaderCompilingManager::FinishCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToFinishCompiling)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::FinishCompilation);

	// nothing to do
	if (!AllowShaderCompiling())
	{
		return;
	}

	check(IsInGameThread());
	check(!FPlatformProperties::RequiresCookedData());
	const double StartTime = FPlatformTime::Seconds();

	FText StatusUpdate;
	if ( MaterialName != nullptr)
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaterialName"), FText::FromString( MaterialName ) );
		StatusUpdate = FText::Format( NSLOCTEXT("ShaderCompilingManager", "CompilingShadersForMaterialStatus", "Compiling shaders: {MaterialName}..."), Args );
	}
	else
	{
		StatusUpdate = NSLOCTEXT("ShaderCompilingManager", "CompilingShadersStatus", "Compiling shaders...");
	}

	FScopedSlowTask SlowTask(1, StatusUpdate, GIsEditor && !IsRunningCommandlet());
	SlowTask.EnterProgressFrame(1);

	TMap<int32, FShaderMapFinalizeResults> CompiledShaderMaps;
	CompiledShaderMaps.Append( PendingFinalizeShaderMaps );
	PendingFinalizeShaderMaps.Empty();
	BlockOnShaderMapCompletion(ShaderMapIdsToFinishCompiling, CompiledShaderMaps);

	bool bRetry = false;
	do 
	{
		bRetry = HandlePotentialRetryOnError(CompiledShaderMaps);
	} 
	while (bRetry);

	ProcessCompiledShaderMaps(CompiledShaderMaps, FLT_MAX);
	check(CompiledShaderMaps.Num() == 0);

	const double EndTime = FPlatformTime::Seconds();

	UE_LOG(LogShaders, Verbose, TEXT("FinishCompilation %s %.3fs"), MaterialName ? MaterialName : TEXT(""), (float)(EndTime - StartTime));
}

void FShaderCompilingManager::FinishAllCompilation()
{
#if WITH_EDITOR
	// This is here for backward compatibility since textures are most probably expected to be ready too.
	FTextureCompilingManager::Get().FinishAllCompilation();
#endif

	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::FinishAllCompilation);
	check(IsInGameThread());
	check(!FPlatformProperties::RequiresCookedData());
	const double StartTime = FPlatformTime::Seconds();

	TMap<int32, FShaderMapFinalizeResults> CompiledShaderMaps;
	CompiledShaderMaps.Append( PendingFinalizeShaderMaps );
	PendingFinalizeShaderMaps.Empty();
	BlockOnAllShaderMapCompletion(CompiledShaderMaps);

	bool bRetry = false;
	do 
	{
		bRetry = HandlePotentialRetryOnError(CompiledShaderMaps);
	} 
	while (bRetry);

	ProcessCompiledShaderMaps(CompiledShaderMaps, FLT_MAX);
	check(CompiledShaderMaps.Num() == 0);

	const double EndTime = FPlatformTime::Seconds();

	UE_LOG(LogShaders, Verbose, TEXT("FinishAllCompilation %.3fs"), (float)(EndTime - StartTime));
}

void FShaderCompilingManager::ProcessAsyncResults(bool bLimitExecutionTime, bool bBlockOnGlobalShaderCompletion)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderCompilingManager::ProcessAsyncResults)

	COOK_STAT(FScopedDurationTimer Timer(ShaderCompilerCookStats::ProcessAsyncResultsTimeSec));
	check(IsInGameThread());
	if (bAllowAsynchronousShaderCompiling)
	{
		{
			const double StartTime = FPlatformTime::Seconds();

			// Some controllers need to be manually ticked if the engine loop is not initialized or blocked
			// to do things like tick the HTTPModule.
			// Otherwise the results from the controller will never be processed.
			// We check for bBlockOnGlobalShaderCompletion because the BlockOnShaderMapCompletion methods already do this.
			if (!bBlockOnGlobalShaderCompletion && BuildDistributionController)
			{
				BuildDistributionController->Tick(0.0f);
			}
			
			// Block on global shaders before checking for shader maps to finalize
			// So if we block on global shaders for a long time, we will get a chance to finalize all the non-global shader maps completed during that time.
			if (bBlockOnGlobalShaderCompletion)
			{
				TArray<int32> ShaderMapId;
				ShaderMapId.Add(GlobalShaderMapId);

				// Block until the global shader map jobs are complete
				GShaderCompilingManager->BlockOnShaderMapCompletion(ShaderMapId, PendingFinalizeShaderMaps);
			}

			int32 NumCompilingShaderMaps = 0;

			{
				// Lock CompileQueueSection so we can access the input and output queues
				FScopeLock Lock(&CompileQueueSection);

				if (!bBlockOnGlobalShaderCompletion)
				{
					bCompilingDuringGame = true;
				}

				// Get all material shader maps to finalize
				//
				for (TMap<int32, FPendingShaderMapCompileResultsPtr>::TIterator It(ShaderMapJobs); It; ++It)
				{
					FPendingShaderMapCompileResultsPtr& Results = It.Value();
					if (Results->FinishedJobs.Num() > 0)
					{
						FShaderMapFinalizeResults& FinalizeResults = PendingFinalizeShaderMaps.FindOrAdd(It.Key());
						FinalizeResults.FinishedJobs.Append(Results->FinishedJobs);
						Results->FinishedJobs.Reset();
						FinalizeResults.bAllJobsSucceeded = FinalizeResults.bAllJobsSucceeded && Results->bAllJobsSucceeded;
					}

					checkf(Results->FinishedJobs.Num() == 0, TEXT("Failed to remove finished jobs, %d remain"), Results->FinishedJobs.Num());
					if (Results->NumPendingJobs.GetValue() == 0)
					{
						It.RemoveCurrent();
					}
				}

				NumCompilingShaderMaps = ShaderMapJobs.Num();
			}

			int32 NumPendingShaderMaps = PendingFinalizeShaderMaps.Num();

			if (PendingFinalizeShaderMaps.Num() > 0)
			{
				bool bRetry = false;
				do 
				{
					bRetry = HandlePotentialRetryOnError(PendingFinalizeShaderMaps);
				} 
				while (bRetry);

				const float TimeBudget = bLimitExecutionTime ? ProcessGameThreadTargetTime : FLT_MAX;
				ProcessCompiledShaderMaps(PendingFinalizeShaderMaps, TimeBudget);
				check(bLimitExecutionTime || PendingFinalizeShaderMaps.Num() == 0);
			}


			if (bBlockOnGlobalShaderCompletion && !bLimitExecutionTime)
			{
				check(PendingFinalizeShaderMaps.Num() == 0);

				if (NumPendingShaderMaps - PendingFinalizeShaderMaps.Num() > 0)
				{
					UE_LOG(LogShaders, Warning, TEXT("Blocking ProcessAsyncResults for %.1fs, processed %u shader maps, %u being compiled"), 
						(float)(FPlatformTime::Seconds() - StartTime),
						NumPendingShaderMaps - PendingFinalizeShaderMaps.Num(), 
						NumCompilingShaderMaps);
				}
			}
			else if (NumPendingShaderMaps - PendingFinalizeShaderMaps.Num() > 0)
			{
				UE_LOG(LogShaders, Verbose, TEXT("Completed %u async shader maps, %u more pending, %u being compiled"),
					NumPendingShaderMaps - PendingFinalizeShaderMaps.Num(), 
					PendingFinalizeShaderMaps.Num(),
					NumCompilingShaderMaps);
			}
		}
	}
	else
	{
		check(AllJobs.GetNumPendingJobs() == 0);
	}

	UpdateNumRemainingAssets();
}

void FShaderCompilingManager::UpdateNumRemainingAssets()
{
	if (IsInGameThread())
	{
		const int32 NumRemainingAssets = GetNumRemainingAssets();
		if (LastNumRemainingAssets != NumRemainingAssets)
		{
			if (NumRemainingAssets == 0)
			{
				// This is important to at least broadcast once we reach 0 remaining assets
				// even if we don't have any UObject to report because some listener are only 
				// interested to be notified when the number of async compilation reaches 0.
				FAssetCompilingManager::Get().OnAssetPostCompileEvent().Broadcast({});
			}

			LastNumRemainingAssets = NumRemainingAssets;
			Notification.Update(NumRemainingAssets);
		}
	}
}

bool FShaderCompilingManager::IsShaderCompilerWorkerRunning(FProcHandle & WorkerHandle)
{
	return FPlatformProcess::IsProcRunning(WorkerHandle);
}

/* Generates a uniform buffer struct member hlsl declaration using the member's metadata. */
static void GenerateUniformBufferStructMember(FString& Result, const FShaderParametersMetadata::FMember& Member, EShaderPlatform ShaderPlatform)
{
	// Generate the base type name.
	FString TypeName;
	Member.GenerateShaderParameterType(TypeName, ShaderPlatform);

	// Generate array dimension post fix
	FString ArrayDim;
	if (Member.GetNumElements() > 0)
	{
		ArrayDim = FString::Printf(TEXT("[%u]"), Member.GetNumElements());
	}

	Result = FString::Printf(TEXT("%s %s%s"), *TypeName, Member.GetName(), *ArrayDim);
}

/* Generates the instanced stereo hlsl code that's dependent on view uniform declarations. */
ENGINE_API void GenerateInstancedStereoCode(FString& Result, EShaderPlatform ShaderPlatform)
{
	// Find the InstancedView uniform buffer struct
	const FShaderParametersMetadata* InstancedView = nullptr;
	for (TLinkedList<FShaderParametersMetadata*>::TIterator StructIt(FShaderParametersMetadata::GetStructList()); StructIt; StructIt.Next())
	{
		if (StructIt->GetShaderVariableName() == FString(TEXT("InstancedView")))
		{
			InstancedView = *StructIt;
			break;
		}
	}
	checkSlow(InstancedView != nullptr);
	const TArray<FShaderParametersMetadata::FMember>& StructMembers = InstancedView->GetMembers();

	// ViewState definition
	Result =  "struct ViewState\r\n";
	Result += "{\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];
		FString MemberDecl;
		GenerateUniformBufferStructMember(MemberDecl, StructMembers[MemberIndex], ShaderPlatform);
		Result += FString::Printf(TEXT("\t%s;\r\n"), *MemberDecl);
	}
	Result += "\tFLWCInverseMatrix WorldToClip;\r\n";
	Result += "\tFLWCMatrix ClipToWorld;\r\n";
	Result += "\tFLWCMatrix ScreenToWorld;\r\n";
	Result += "\tFLWCMatrix PrevClipToWorld;\r\n";
	Result += "\tFLWCVector3 WorldCameraOrigin;\r\n";
	Result += "\tFLWCVector3 WorldViewOrigin;\r\n";
	Result += "\tFLWCVector3 PrevWorldCameraOrigin;\r\n";
	Result += "\tFLWCVector3 PrevWorldViewOrigin;\r\n";
	Result += "\tFLWCVector3 PreViewTranslation;\r\n";
	Result += "\tFLWCVector3 PrevPreViewTranslation;\r\n";
	Result += "};\r\n";

	Result += "\tvoid FinalizeViewState(inout ViewState InOutView);\r\n";

	// GetPrimaryView definition
	Result += "ViewState GetPrimaryView()\r\n";
	Result += "{\r\n";
	Result += "\tViewState Result;\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];
		Result += FString::Printf(TEXT("\tResult.%s = View.%s;\r\n"), Member.GetName(), Member.GetName());
	}
	Result += "\tFinalizeViewState(Result);\r\n";
	Result += "\treturn Result;\r\n";
	Result += "}\r\n";

	// GetInstancedView definition
	Result += "ViewState GetInstancedView()\r\n";
	Result += "{\r\n";
	Result += "\tViewState Result;\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];
		Result += FString::Printf(TEXT("\tResult.%s = InstancedView.%s;\r\n"), Member.GetName(), Member.GetName());
	}
	Result += "\tFinalizeViewState(Result);\r\n";
	Result += "\treturn Result;\r\n";
	Result += "}\r\n";
	
	// ResolveView definition for metal, this allows us to change the branch to a conditional move in the cross compiler
	Result += "#if COMPILER_METAL && (COMPILER_HLSLCC == 1)\r\n";
	Result += "ViewState ResolveView(uint ViewIndex)\r\n";
	Result += "{\r\n";
	Result += "\tViewState Result;\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FShaderParametersMetadata::FMember& Member = StructMembers[MemberIndex];
		Result += FString::Printf(TEXT("\tResult.%s = (ViewIndex == 0) ? View.%s : InstancedView.%s;\r\n"), Member.GetName(), Member.GetName(), Member.GetName());
	}
	Result += "\tFinalizeViewState(Result);\r\n";
	Result += "\treturn Result;\r\n";
	Result += "}\r\n";
	Result += "#endif\r\n";
}

void ValidateShaderFilePath(const FString& VirtualShaderFilePath, const FString& VirtualSourceFilePath)
{
	check(CheckVirtualShaderFilePath(VirtualShaderFilePath));

	checkf(VirtualShaderFilePath.Contains(TEXT("/Generated/")),
		TEXT("Incorrect virtual shader path for generated file '%s': Generated files must be located under an "
				"non existing 'Generated' directory, for instance: /Engine/Generated/ or /Plugin/FooBar/Generated/."),
		*VirtualShaderFilePath);

	checkf(VirtualShaderFilePath == VirtualSourceFilePath || FPaths::GetExtension(VirtualShaderFilePath) == TEXT("ush"),
		TEXT("Incorrect virtual shader path extension for generated file '%s': Generated file must either be the "
				"USF to compile, or a USH file to be included."),
		*VirtualShaderFilePath);
}

/** Lock for the storage of instanced stereo code. */
FCriticalSection GCachedGeneratedInstancedStereoCodeLock;

/** Storage for instanced stereo code so it is not generated every time we compile a shader. */
TMap<EShaderPlatform, FThreadSafeSharedStringPtr> GCachedGeneratedInstancedStereoCode;

void GlobalBeginCompileShader(
	const FString& DebugGroupName,
	const FVertexFactoryType* VFType,
	const FShaderType* ShaderType,
	const FShaderPipelineType* ShaderPipelineType,
	int32 PermutationId,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	FShaderCompilerInput& Input,
	bool bAllowDevelopmentShaderCompile,
	const FString& DebugDescription,
	const FString& DebugExtension
)
{
	GlobalBeginCompileShader(
		DebugGroupName,
		VFType,
		ShaderType,
		ShaderPipelineType,
		PermutationId,
		SourceFilename,
		FunctionName,
		Target,
		Input,
		bAllowDevelopmentShaderCompile,
		*DebugDescription,
		*DebugExtension
		);
}


void GlobalBeginCompileShader(
	const FString& DebugGroupName,
	const FVertexFactoryType* VFType,
	const FShaderType* ShaderType,
	const FShaderPipelineType* ShaderPipelineType,
	int32 PermutationId,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	FShaderCompilerInput& Input,
	bool bAllowDevelopmentShaderCompile,
	const TCHAR* DebugDescription,
	const TCHAR* DebugExtension
	)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GlobalBeginCompileShader);
	COOK_STAT(ShaderCompilerCookStats::GlobalBeginCompileShaderCalls++);
	COOK_STAT(FScopedDurationTimer DurationTimer(ShaderCompilerCookStats::GlobalBeginCompileShaderTimeSec));

	EShaderPlatform ShaderPlatform = EShaderPlatform(Target.Platform);
	const FName ShaderFormatName = LegacyShaderPlatformToShaderFormat(ShaderPlatform);

#if WITH_EDITOR
	FShaderCompileUtilities::GenerateBrdfHeaders(ShaderPlatform);
#endif

	Input.Target = Target;
	Input.ShaderPlatformName = FDataDrivenShaderPlatformInfo::GetName(ShaderPlatform);
	Input.ShaderFormat = ShaderFormatName;
	Input.CompressionFormat = GetShaderCompressionFormat();
	GetShaderCompressionOodleSettings(Input.OodleCompressor, Input.OodleLevel);
	Input.VirtualSourceFilePath = SourceFilename;
	Input.EntryPointName = FunctionName;
	Input.bCompilingForShaderPipeline = false;
	Input.bIncludeUsedOutputs = false;
	Input.bGenerateDirectCompileFile = (GDumpShaderDebugInfoSCWCommandLine != 0);
	Input.DumpDebugInfoRootPath = GShaderCompilingManager->GetAbsoluteShaderDebugInfoDirectory() / Input.ShaderPlatformName.ToString();
	// asset material name or "Global"
	Input.DebugGroupName = DebugGroupName;
	Input.DebugDescription = DebugDescription;
	Input.DebugExtension = DebugExtension;
	Input.RootParametersStructure = ShaderType->GetRootParametersMetadata();
	Input.ShaderName = ShaderType->GetName();

	// Verify FShaderCompilerInput's file paths are consistent. 
	#if DO_CHECK
		check(CheckVirtualShaderFilePath(Input.VirtualSourceFilePath));

		checkf(FPaths::GetExtension(Input.VirtualSourceFilePath) == TEXT("usf"),
			TEXT("Incorrect virtual shader path extension for shader file to compile '%s': Only .usf files should be "
				 "compiled. .ush file are meant to be included only."),
			*Input.VirtualSourceFilePath);

		for (const auto& Entry : Input.Environment.IncludeVirtualPathToContentsMap)
		{
			ValidateShaderFilePath(Entry.Key, Input.VirtualSourceFilePath);
		}

		for (const auto& Entry : Input.Environment.IncludeVirtualPathToExternalContentsMap)
		{
			ValidateShaderFilePath(Entry.Key, Input.VirtualSourceFilePath);
		}
	#endif

	if (ShaderPipelineType)
	{
		Input.DebugGroupName = Input.DebugGroupName / ShaderPipelineType->GetName();
	}
	
	if (VFType)
	{
		FString VFName = VFType->GetName();
		if (GDumpShaderDebugInfoShort)
		{
			// Shorten vertex factory name
			if (VFName[0] == TCHAR('F') || VFName[0] == TCHAR('T'))
			{
				VFName.RemoveAt(0);
			}
			VFName.ReplaceInline(TEXT("VertexFactory"), TEXT("VF"));
			VFName.ReplaceInline(TEXT("GPUSkinAPEXCloth"), TEXT("APEX"));
			VFName.ReplaceInline(TEXT("true"), TEXT("_1"));
			VFName.ReplaceInline(TEXT("false"), TEXT("_0"));
		}
		Input.DebugGroupName = Input.DebugGroupName / VFName;
	}
	
	{
		FString ShaderTypeName = ShaderType->GetName();
		if (GDumpShaderDebugInfoShort)
		{
			// Shorten known types
			if (ShaderTypeName[0] == TCHAR('F') || ShaderTypeName[0] == TCHAR('T'))
			{
				ShaderTypeName.RemoveAt(0);
			}
		}
		Input.DebugGroupName = Input.DebugGroupName / ShaderTypeName / FString::Printf(TEXT("%i"), PermutationId);
		
		if (GDumpShaderDebugInfoShort)
		{
			Input.DebugGroupName.ReplaceInline(TEXT("BasePass"), TEXT("BP"));
			Input.DebugGroupName.ReplaceInline(TEXT("ForForward"), TEXT("Fwd"));
			Input.DebugGroupName.ReplaceInline(TEXT("Shadow"), TEXT("Shdw"));
			Input.DebugGroupName.ReplaceInline(TEXT("LightMap"), TEXT("LM"));
			Input.DebugGroupName.ReplaceInline(TEXT("EHeightFogFeature==E_"), TEXT(""));
			Input.DebugGroupName.ReplaceInline(TEXT("Capsule"), TEXT("Caps"));
			Input.DebugGroupName.ReplaceInline(TEXT("Movable"), TEXT("Mov"));
			Input.DebugGroupName.ReplaceInline(TEXT("Culling"), TEXT("Cull"));
			Input.DebugGroupName.ReplaceInline(TEXT("Atmospheric"), TEXT("Atm"));
			Input.DebugGroupName.ReplaceInline(TEXT("Atmosphere"), TEXT("Atm"));
			Input.DebugGroupName.ReplaceInline(TEXT("Exponential"), TEXT("Exp"));
			Input.DebugGroupName.ReplaceInline(TEXT("Ambient"), TEXT("Amb"));
			Input.DebugGroupName.ReplaceInline(TEXT("Perspective"), TEXT("Persp"));
			Input.DebugGroupName.ReplaceInline(TEXT("Occlusion"), TEXT("Occ"));
			Input.DebugGroupName.ReplaceInline(TEXT("Position"), TEXT("Pos"));
			Input.DebugGroupName.ReplaceInline(TEXT("Skylight"), TEXT("Sky"));
			Input.DebugGroupName.ReplaceInline(TEXT("LightingPolicy"), TEXT("LP"));
			Input.DebugGroupName.ReplaceInline(TEXT("TranslucentLighting"), TEXT("TranslLight"));
			Input.DebugGroupName.ReplaceInline(TEXT("Translucency"), TEXT("Transl"));
			Input.DebugGroupName.ReplaceInline(TEXT("DistanceField"), TEXT("DistFiel"));
			Input.DebugGroupName.ReplaceInline(TEXT("Indirect"), TEXT("Ind"));
			Input.DebugGroupName.ReplaceInline(TEXT("Cached"), TEXT("Cach"));
			Input.DebugGroupName.ReplaceInline(TEXT("Inject"), TEXT("Inj"));
			Input.DebugGroupName.ReplaceInline(TEXT("Visualization"), TEXT("Viz"));
			Input.DebugGroupName.ReplaceInline(TEXT("Instanced"), TEXT("Inst"));
			Input.DebugGroupName.ReplaceInline(TEXT("Evaluate"), TEXT("Eval"));
			Input.DebugGroupName.ReplaceInline(TEXT("Landscape"), TEXT("Land"));
			Input.DebugGroupName.ReplaceInline(TEXT("Dynamic"), TEXT("Dyn"));
			Input.DebugGroupName.ReplaceInline(TEXT("Vertex"), TEXT("Vtx"));
			Input.DebugGroupName.ReplaceInline(TEXT("Output"), TEXT("Out"));
			Input.DebugGroupName.ReplaceInline(TEXT("Directional"), TEXT("Dir"));
			Input.DebugGroupName.ReplaceInline(TEXT("Irradiance"), TEXT("Irr"));
			Input.DebugGroupName.ReplaceInline(TEXT("Deferred"), TEXT("Def"));
			Input.DebugGroupName.ReplaceInline(TEXT("true"), TEXT("_1"));
			Input.DebugGroupName.ReplaceInline(TEXT("false"), TEXT("_0"));
			Input.DebugGroupName.ReplaceInline(TEXT("PROPAGATE_AO"), TEXT("AO"));
			Input.DebugGroupName.ReplaceInline(TEXT("PROPAGATE_SECONDARY_OCCLUSION"), TEXT("SEC_OCC"));
			Input.DebugGroupName.ReplaceInline(TEXT("PROPAGATE_MULTIPLE_BOUNCES"), TEXT("MULT_BOUNC"));
			Input.DebugGroupName.ReplaceInline(TEXT("PostProcess"), TEXT("Post"));
			Input.DebugGroupName.ReplaceInline(TEXT("AntiAliasing"), TEXT("AA"));
			Input.DebugGroupName.ReplaceInline(TEXT("Mobile"), TEXT("Mob"));
			Input.DebugGroupName.ReplaceInline(TEXT("Linear"), TEXT("Lin"));
			Input.DebugGroupName.ReplaceInline(TEXT("INT32_MAX"), TEXT("IMAX"));
			Input.DebugGroupName.ReplaceInline(TEXT("Policy"), TEXT("Pol"));
			Input.DebugGroupName.ReplaceInline(TEXT("EAtmRenderFlag==E_"), TEXT(""));
		}
	}

	// Setup the debug info path if requested, or if this is a global shader and shader development mode is enabled
	Input.DumpDebugInfoPath.Empty();
	if (GShaderCompilingManager->GetDumpShaderDebugInfo() == FShaderCompilingManager::EDumpShaderDebugInfo::Always)
	{
		Input.DumpDebugInfoPath = GShaderCompilingManager->CreateShaderDebugInfoPath(Input);
	}

	// Add the appropriate definitions for the shader frequency.
	{
		Input.Environment.SetDefine(TEXT("PIXELSHADER"), Target.Frequency == SF_Pixel);
		Input.Environment.SetDefine(TEXT("VERTEXSHADER"), Target.Frequency == SF_Vertex);
		Input.Environment.SetDefine(TEXT("MESHSHADER"), Target.Frequency == SF_Mesh);
		Input.Environment.SetDefine(TEXT("AMPLIFICATIONSHADER"), Target.Frequency == SF_Amplification);
		Input.Environment.SetDefine(TEXT("GEOMETRYSHADER"), Target.Frequency == SF_Geometry);
		Input.Environment.SetDefine(TEXT("COMPUTESHADER"), Target.Frequency == SF_Compute);
		Input.Environment.SetDefine(TEXT("RAYCALLABLESHADER"), Target.Frequency == SF_RayCallable);
		Input.Environment.SetDefine(TEXT("RAYHITGROUPSHADER"), Target.Frequency == SF_RayHitGroup);
		Input.Environment.SetDefine(TEXT("RAYGENSHADER"), Target.Frequency == SF_RayGen);
		Input.Environment.SetDefine(TEXT("RAYMISSSHADER"), Target.Frequency == SF_RayMiss);
	}

	// Enables HLSL 2021
	const bool bRayGenShader = Target.Frequency == SF_RayGen;

	uint32 EnablesHLSL2021ByDefault = FDataDrivenShaderPlatformInfo::GetEnablesHLSL2021ByDefault(EShaderPlatform(Target.Platform));
	if (EnablesHLSL2021ByDefault == uint32(1) && DebugGroupName == TEXT("Global"))
	{
		Input.Environment.CompilerFlags.Add(CFLAG_HLSL2021);
	}
	else if (EnablesHLSL2021ByDefault == uint32(2))
	{
		Input.Environment.CompilerFlags.Add(CFLAG_HLSL2021);
	}
	else if (bRayGenShader)
	{
		// We want to make sure that function overloads follow c++ rules for FRayDesc.
		Input.Environment.CompilerFlags.Add(CFLAG_HLSL2021);
	}

	// #defines get stripped out by the preprocessor without this. We can override with this
	Input.Environment.SetDefine(TEXT("COMPILER_DEFINE"), TEXT("#define"));

	if (FSceneInterface::GetShadingPath(GetMaxSupportedFeatureLevel(ShaderPlatform)) == EShadingPath::Deferred)
	{
		Input.Environment.SetDefine(TEXT("SHADING_PATH_DEFERRED"), 1);
	}

	const bool bUsingMobileRenderer = FSceneInterface::GetShadingPath(GetMaxSupportedFeatureLevel(ShaderPlatform)) == EShadingPath::Mobile;
	if (bUsingMobileRenderer)
	{
		Input.Environment.SetDefine(TEXT("SHADING_PATH_MOBILE"), 1);
		
		const bool bMobileDeferredShading = IsMobileDeferredShadingEnabled((EShaderPlatform)Target.Platform);
		Input.Environment.SetDefine(TEXT("MOBILE_DEFERRED_SHADING"), bMobileDeferredShading ? 1 : 0);

		if (bMobileDeferredShading)
		{
			bool bGLESDeferredShading = Target.Platform == SP_OPENGL_ES3_1_ANDROID;
			Input.Environment.SetDefine(TEXT("USE_GLES_FBF_DEFERRED"), bGLESDeferredShading ? 1 : 0);
			Input.Environment.SetDefine(TEXT("MOBILE_EXTENDED_GBUFFER"), MobileUsesExtenedGBuffer((EShaderPlatform)Target.Platform) ? 1 : 0);
		}
	}

	if (ShaderPlatform == SP_VULKAN_ES3_1_ANDROID || ShaderPlatform == SP_VULKAN_SM5_ANDROID)
	{
		bool bIsStripReflect = true;
		GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bStripShaderReflection"), bIsStripReflect, GEngineIni);
		if (!bIsStripReflect)
		{
			Input.Environment.SetDefine(TEXT("STRIP_REFLECT_ANDROID"), false);
		}
	}

	// Set VR definitions
	{
		const UE::StereoRenderUtils::FStereoShaderAspects Aspects(ShaderPlatform);

		Input.Environment.SetDefine(TEXT("INSTANCED_STEREO"), Aspects.IsInstancedStereoEnabled());
		Input.Environment.SetDefine(TEXT("MULTI_VIEW"), Aspects.IsInstancedMultiViewportEnabled());
		Input.Environment.SetDefine(TEXT("MOBILE_MULTI_VIEW"), Aspects.IsMobileMultiViewEnabled());

		// Throw a warning if we are silently disabling ISR due to missing platform support (but don't have MMV enabled).
		static const auto CVarInstancedStereo = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.InstancedStereo"));
		const bool bIsInstancedStereoEnabledInSettings = CVarInstancedStereo ? (CVarInstancedStereo->GetValueOnAnyThread() != 0) : false;
		static const auto CVarMultiview = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
		const bool bIsMultiviewEnabledInSettings = CVarMultiview ? (CVarMultiview->GetValueOnAnyThread() != 0) : false;
		bool bWarningIssued = false;
		// warn if ISR was enabled in settings, but aspects show that it's not enabled AND we don't use Mobile MultiView as an alternative
		if (bIsInstancedStereoEnabledInSettings && !Aspects.IsInstancedStereoEnabled() && !(bIsMultiviewEnabledInSettings && Aspects.IsMobileMultiViewEnabled()) && !GShaderCompilingManager->AreWarningsSuppressed(ShaderPlatform))
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("Instanced stereo rendering is not supported for %s shader platform."), *ShaderFormatName.ToString());
			bWarningIssued = true;
		}
		// Warn if MMV was enabled in settings, but aspects show that it's not enabled AND we don't use Instanced Stereo as an alternative
		if (bIsMultiviewEnabledInSettings && !Aspects.IsMobileMultiViewEnabled() && !(bIsInstancedStereoEnabledInSettings && Aspects.IsInstancedStereoEnabled()) && !GShaderCompilingManager->AreWarningsSuppressed(ShaderPlatform))
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("Multiview rendering is not supported for %s shader platform."), *ShaderFormatName.ToString());
			bWarningIssued = true;
		}
		if (bWarningIssued)
		{
			GShaderCompilingManager->SuppressWarnings(ShaderPlatform);
		}
	}

	ShaderType->AddReferencedUniformBufferIncludes(Input.Environment, Input.SourceFilePrefix, ShaderPlatform);

	if (VFType)
	{
		VFType->AddReferencedUniformBufferIncludes(Input.Environment, Input.SourceFilePrefix, ShaderPlatform);
	}

	// Add generated instanced stereo code
	{
		// this function may be called on multiple threads, so protect the storage
		FScopeLock GeneratedInstancedCodeLock(&GCachedGeneratedInstancedStereoCodeLock);

		FThreadSafeSharedStringPtr* Existing = GCachedGeneratedInstancedStereoCode.Find(ShaderPlatform);
		FThreadSafeSharedStringPtr CachedCodePtr = Existing ? *Existing : nullptr;
		if (!CachedCodePtr.IsValid())
		{
			CachedCodePtr = MakeShareable(new FString());
			GenerateInstancedStereoCode(*CachedCodePtr.Get(), ShaderPlatform);
			GCachedGeneratedInstancedStereoCode.Add(ShaderPlatform, CachedCodePtr);
		}

		Input.Environment.IncludeVirtualPathToExternalContentsMap.Add(TEXT("/Engine/Generated/GeneratedInstancedStereo.ush"), CachedCodePtr);
	}

	{
		// Check if the compile environment explicitly wants to force optimization
		const bool bForceOptimization = Input.Environment.CompilerFlags.Contains(CFLAG_ForceOptimization);

		if (!bForceOptimization && !ShouldOptimizeShaders(ShaderFormatName))
		{
			Input.Environment.CompilerFlags.Add(CFLAG_Debug);
		}
	}

	// Extra data (names, etc)
	if (ShouldEnableExtraShaderData(ShaderFormatName))
	{
		Input.Environment.CompilerFlags.Add(CFLAG_ExtraShaderData);
	}

	// Symbols
	if (ShouldGenerateShaderSymbols(ShaderFormatName))
	{
		Input.Environment.CompilerFlags.Add(CFLAG_GenerateSymbols);
	}

	// Are symbols based on source or results
	if (ShouldAllowUniqueShaderSymbols(ShaderFormatName))
	{
		Input.Environment.CompilerFlags.Add(CFLAG_AllowUniqueSymbols);
	}

	if (CVarShaderFastMath.GetValueOnAnyThread() == 0)
	{
		Input.Environment.CompilerFlags.Add(CFLAG_NoFastMath);
	}
    
    {
        static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.FloatPrecisionMode"));
        Input.Environment.FullPrecisionInPS = CVar ? (CVar->GetInt() == EMobileFloatPrecisionMode::Full) : false;
    }
	
	{
		int32 FlowControl = CVarShaderFlowControl.GetValueOnAnyThread();
		switch (FlowControl)
		{
			case 2:
				Input.Environment.CompilerFlags.Add(CFLAG_AvoidFlowControl);
				break;
			case 1:
				Input.Environment.CompilerFlags.Add(CFLAG_PreferFlowControl);
				break;
			case 0:
				// Fallback to nothing...
			default:
				break;
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Validation"));
		if (CVar && CVar->GetInt() == 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_SkipValidation);
		}
	}

	{
		Input.Environment.SetDefine(TEXT("DO_CHECK"), GSShaderCheckLevel > 0 ? 1 : 0);
		Input.Environment.SetDefine(TEXT("DO_GUARD_SLOW"), GSShaderCheckLevel > 1 ? 1 : 0);
	}

	if (CVarShaderWarningsAsErrors.GetValueOnAnyThread())
	{
		Input.Environment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
	}

	if (IsD3DPlatform((EShaderPlatform)Target.Platform) && IsPCPlatform((EShaderPlatform)Target.Platform))
	{
		if (CVarD3DRemoveUnusedInterpolators.GetValueOnAnyThread() != 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_ForceRemoveUnusedInterpolators);
		}

		if (CVarD3DCheckedForTypedUAVs.GetValueOnAnyThread() == 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		}

		if (CVarD3DForceShaderConductorDXCRewrite.GetValueOnAnyThread() != 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_D3D12ForceShaderConductorRewrite);
		}
		{
			static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.D3D.CheckedForTypedUAVs"));
			if (CVar && CVar->GetInt() == 0)
			{
				Input.Environment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
			}
		}
	}

	if (IsMetalPlatform((EShaderPlatform)Target.Platform))
	{
		if (CVarShaderZeroInitialise.GetValueOnAnyThread() != 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_ZeroInitialise);
		}

		if (CVarShaderBoundsChecking.GetValueOnAnyThread() != 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_BoundsChecking);
		}
		
		// Check whether we can compile metal shaders to bytecode - avoids poisoning the DDC
		static ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const IShaderFormat* Compiler = TPM.FindShaderFormat(ShaderFormatName);
		static const bool bCanCompileOfflineMetalShaders = Compiler && Compiler->CanCompileBinaryShaders();
		if (!bCanCompileOfflineMetalShaders)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_Debug);
		}
		else
		{
			// populate the data in the shader input environment
			FString RemoteServer;
			FString UserName;
			FString SSHKey;
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RemoteServerName"), RemoteServer, GEngineIni);
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RSyncUsername"), UserName, GEngineIni);
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("SSHPrivateKeyOverridePath"), SSHKey, GEngineIni);
			Input.Environment.RemoteServerData.Add(TEXT("RemoteServerName"), RemoteServer);
			Input.Environment.RemoteServerData.Add(TEXT("RSyncUsername"), UserName);
			if (SSHKey.Len() > 0)
			{
				Input.Environment.RemoteServerData.Add(TEXT("SSHPrivateKeyOverridePath"), SSHKey);
			}
		}
		
		// Shaders built for archiving - for Metal that requires compiling the code in a different way so that we can strip it later
		bool bArchive = false;
		GConfig->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bSharedMaterialNativeLibraries"), bArchive, GGameIni);
		if (bCanCompileOfflineMetalShaders && bArchive)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_Archive);
		}
		
		{
			uint32 ShaderVersion = RHIGetMetalShaderLanguageVersion(EShaderPlatform(Target.Platform));
			Input.Environment.SetDefine(TEXT("SHADER_LANGUAGE_VERSION"), ShaderVersion);
			
			bool bAllowFastIntrinsics = false;
			bool bForceFloats = false;
			FString IndirectArgumentTier;
			bool bEnableMathOptimisations = true;
            bool bSupportAppleA8 = false;
            
			if (IsPCPlatform(EShaderPlatform(Target.Platform)))
			{
				GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("UseFastIntrinsics"), bAllowFastIntrinsics, GEngineIni);
				GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("EnableMathOptimisations"), bEnableMathOptimisations, GEngineIni);
				GConfig->GetString(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("IndirectArgumentTier"), IndirectArgumentTier, GEngineIni);
                
                // No half precision support on MacOS at the moment
                bForceFloats = true;
			}
			else
			{
				GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("UseFastIntrinsics"), bAllowFastIntrinsics, GEngineIni);
				GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("EnableMathOptimisations"), bEnableMathOptimisations, GEngineIni);
				GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("ForceFloats"), bForceFloats, GEngineIni);
				GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("IndirectArgumentTier"), IndirectArgumentTier, GEngineIni);
                GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportAppleA8"), bSupportAppleA8, GEngineIni);
                
				// Force no development shaders on iOS
				bAllowDevelopmentShaderCompile = false;
			}
            
            Input.Environment.FullPrecisionInPS |= bForceFloats;
            
			Input.Environment.SetDefine(TEXT("METAL_USE_FAST_INTRINSICS"), bAllowFastIntrinsics);
			Input.Environment.SetDefine(TEXT("METAL_INDIRECT_ARGUMENT_BUFFERS"), IndirectArgumentTier);
            Input.Environment.SetDefine(TEXT("SUPPORT_APPLE_A8"), bSupportAppleA8);
			
			// Same as console-variable above, but that's global and this is per-platform, per-project
			if (!bEnableMathOptimisations)
			{
				Input.Environment.CompilerFlags.Add(CFLAG_NoFastMath);
			}
		}
	}

	if (IsAndroidPlatform(EShaderPlatform(Target.Platform)))
	{
		// Force no development shaders on Android platforms
		bAllowDevelopmentShaderCompile = false;
	}

	// Mobile emulation should be defined when a PC platform is using a mobile renderer (limited to feature level ES3_1)...  eg SP_PCD3D_ES3_1,SP_VULKAN_PCES3_1,SP_METAL_MACES3_1
	if (IsSimulatedPlatform(EShaderPlatform(Target.Platform)) && bAllowDevelopmentShaderCompile)
	{
		Input.Environment.SetDefine(TEXT("MOBILE_EMULATION"), 1);
	}

	// Add compiler flag CFLAG_ForceDXC if DXC is enabled
	const bool bHlslVersion2021 = Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021);
	const bool bIsDxcEnabled = IsDxcEnabledForPlatform((EShaderPlatform)Target.Platform, bHlslVersion2021);
	Input.Environment.SetDefine(TEXT("COMPILER_DXC"), bIsDxcEnabled);
	if (bIsDxcEnabled)
	{
		Input.Environment.CompilerFlags.Add(CFLAG_ForceDXC);
	}

	bool bIsMobilePlatform = IsMobilePlatform((EShaderPlatform)Target.Platform);

	if (bIsMobilePlatform)
	{
		if (IsUsingEmulatedUniformBuffers((EShaderPlatform)Target.Platform))
		{
			Input.Environment.CompilerFlags.Add(CFLAG_UseEmulatedUB);
		}
	}

	Input.Environment.SetDefine(TEXT("HAS_INVERTED_Z_BUFFER"), (bool)ERHIZBuffer::IsInverted);

	if (Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021))
	{
		Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_HLSL2021"), 1);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ClearCoatNormal"));
		Input.Environment.SetDefine(TEXT("CLEAR_COAT_BOTTOM_NORMAL"), CVar ? (CVar->GetValueOnAnyThread() != 0) && !bIsMobilePlatform : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.IrisNormal"));
		Input.Environment.SetDefine(TEXT("IRIS_NORMAL"), CVar ? (CVar->GetValueOnAnyThread() != 0) : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Compat.UseDXT5NormalMaps"));
		Input.Environment.SetDefine(TEXT("DXT5_NORMALMAPS"), CVar ? (CVar->GetValueOnAnyThread() != 0) : 0);
	}

	if (bAllowDevelopmentShaderCompile)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CompileShadersForDevelopment"));
		Input.Environment.SetDefine(TEXT("COMPILE_SHADERS_FOR_DEVELOPMENT"), CVar ? (CVar->GetValueOnAnyThread() != 0) : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		Input.Environment.SetDefine(TEXT("ALLOW_STATIC_LIGHTING"), CVar ? (CVar->GetValueOnAnyThread() != 0) : 1);
	}

	{
		// Allow GBuffer containing a velocity target to be overridden at a higher level with GBUFFER_LAYOUT
		bool bGBufferHasVelocity = IsUsingBasePassVelocity((EShaderPlatform)Target.Platform);
	#if WITH_EDITOR
		if (!bGBufferHasVelocity)
		{
			const EGBufferLayout Layout = FShaderCompileUtilities::FetchGBufferLayout(Input.Environment);
			bGBufferHasVelocity |= (Layout == GBL_ForceVelocity);
		}
	#endif
		Input.Environment.SetDefine(TEXT("GBUFFER_HAS_VELOCITY"), bGBufferHasVelocity ? 1 : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GBufferDiffuseSampleOcclusion"));
		Input.Environment.SetDefine(TEXT("GBUFFER_HAS_DIFFUSE_SAMPLE_OCCLUSION"), CVar ? (CVar->GetValueOnAnyThread() != 0) : 1);
	}

	{
		Input.Environment.SetDefine(TEXT("SELECTIVE_BASEPASS_OUTPUTS"), IsUsingSelectiveBasePassOutputs((EShaderPlatform)Target.Platform) ? 1 : 0);
	}

	{
		Input.Environment.SetDefine(TEXT("USE_DBUFFER"), IsUsingDBuffers((EShaderPlatform)Target.Platform) ? 1 : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowGlobalClipPlane"));
		Input.Environment.SetDefine(TEXT("PROJECT_ALLOW_GLOBAL_CLIP_PLANE"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatformWithSupport(TEXT("ShaderFormat"), ShaderFormatName);
	bool bForwardShading = false;
	{
		if (TargetPlatform)
		{
			bForwardShading = TargetPlatform->UsesForwardShading();
		}
		else
		{
			static IConsoleVariable* CVarForwardShading = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForwardShading"));
			bForwardShading = CVarForwardShading ? (CVarForwardShading->GetInt() != 0) : false;
		}
		Input.Environment.SetDefine(TEXT("FORWARD_SHADING"), bForwardShading);
	}

	{
		if (VelocityEncodeDepth((EShaderPlatform)Target.Platform))
		{
			Input.Environment.SetDefine(TEXT("VELOCITY_ENCODE_DEPTH"), 1);
		}
		else
		{
			Input.Environment.SetDefine(TEXT("VELOCITY_ENCODE_DEPTH"), 0);
		}
	}

	{
		if (MaskedInEarlyPass((EShaderPlatform)Target.Platform))
		{
			Input.Environment.SetDefine(TEXT("EARLY_Z_PASS_ONLY_MATERIAL_MASKING"), 1);
		}
		else
		{
			Input.Environment.SetDefine(TEXT("EARLY_Z_PASS_ONLY_MATERIAL_MASKING"), 0);
		}
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VertexFoggingForOpaque"));
		bool bVertexFoggingForOpaque = false;
		if (bForwardShading)
		{
			bVertexFoggingForOpaque = CVar ? (CVar->GetInt() != 0) : 0;
			if (TargetPlatform)
			{
				const int32 PlatformHeightFogMode = TargetPlatform->GetHeightFogModeForOpaque();
				if (PlatformHeightFogMode == 1)
				{
					bVertexFoggingForOpaque = false;
				}
				else if (PlatformHeightFogMode == 2)
				{
					bVertexFoggingForOpaque = true;
				}
			}
		}
		Input.Environment.SetDefine(TEXT("PROJECT_VERTEX_FOGGING_FOR_OPAQUE"), bVertexFoggingForOpaque);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DisableVertexFog"));
		Input.Environment.SetDefine(TEXT("PROJECT_MOBILE_DISABLE_VERTEX_FOG"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	bool bSupportSkyAtmosphere = false;
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SupportSkyAtmosphere"));
		bSupportSkyAtmosphere = CVar && CVar->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("PROJECT_SUPPORT_SKY_ATMOSPHERE"), bSupportSkyAtmosphere ? 1 : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SupportCloudShadowOnForwardLitTranslucent"));
		const bool bSupportCloudShadowOnForwardLitTranslucent = CVar && CVar->GetInt() > 0;
		Input.Environment.SetDefine(TEXT("SUPPORT_CLOUD_SHADOW_ON_FORWARD_LIT_TRANSLUCENT"), bSupportCloudShadowOnForwardLitTranslucent ? 1 : 0);
	}

	{
		static IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.SingleLayerWater.SupportCloudShadow"));
		const bool bSupportCloudShadowOnSingleLayerWater = CVar && CVar->GetInt() > 0;
		Input.Environment.SetDefine(TEXT("SUPPORT_CLOUD_SHADOW_ON_SINGLE_LAYER_WATER"), bSupportCloudShadowOnSingleLayerWater ? 1 : 0);
	}

	{
		static IConsoleVariable* CVarStrata = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Strata"));
		const bool bStrata = CVarStrata && CVarStrata->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("STRATA_ENABLED"), bStrata ? 1 : 0);

		// Force rough diffuse to be disable on platform which explicitly disable it from their settings
		if (bStrata && IsConsolePlatform(Target.GetPlatform()))
		{
			static FShaderPlatformCachedIniValue<int32> CVarStrataRoughDiffuse(TEXT("r.Strata.RoughDiffuse"));
			const bool bStrataRoughDiffuse = CVarStrataRoughDiffuse.Get(Target.GetPlatform()) != 0;
			Input.Environment.SetDefine(TEXT("STRATA_DIFFUSE_CHAN"), bStrataRoughDiffuse ? 1 : 0);
		}

		// Force rough diffuse to be disable on platform which explicitly disable it from their settings
		if (bStrata)
		{
			static FShaderPlatformCachedIniValue<int32> CVarStrataShadingQuality(TEXT("r.Strata.ShadingQuality"));
			const uint32 StrataShadingQuality = CVarStrataShadingQuality.Get(Target.GetPlatform());
			Input.Environment.SetDefine(TEXT("STRATA_SHADING_QUALITY"), FMath::Max(StrataShadingQuality, 1u));
			if (StrataShadingQuality > 1)
			{
				Input.Environment.SetDefine(TEXT("USE_ACHROMATIC_BXDF_ENERGY"), 1u);
			}

			static FShaderPlatformCachedIniValue<int32> CVarStrataTileCoord8Bits(TEXT("r.Strata.TileCoord8bits"));
			const uint32 StrataTileCoord8Bits = CVarStrataTileCoord8Bits.Get(Target.GetPlatform());
			Input.Environment.SetDefine(TEXT("USE_8BIT_TILE_COORD"), StrataTileCoord8Bits > 0 ? 1 : 0);
		}

		static IConsoleVariable* CVarBackCompatibility = IConsoleManager::Get().FindConsoleVariable(TEXT("r.StrataBackCompatibility"));
		const bool bStrataBackCompatibility = CVarBackCompatibility && CVarBackCompatibility->GetInt() > 0;
		Input.Environment.SetDefine(TEXT("PROJECT_STRATA_BACKCOMPATIBILITY"), bStrataBackCompatibility ? 1 : 0);

		static IConsoleVariable* CVarOpaqueRoughRefrac = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Strata.OpaqueMaterialRoughRefraction"));
		const bool bStrataOpaqueRoughRefrac = bStrata && CVarOpaqueRoughRefrac && CVarOpaqueRoughRefrac->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("STRATA_OPAQUE_ROUGH_REFRACTION_ENABLED"), bStrataOpaqueRoughRefrac ? 1 : 0);

		static IConsoleVariable* CVarAdvDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Strata.Debug.AdvancedVisualizationShaders"));
		const bool bStrataAdvDebug = bStrata && CVarAdvDebug && CVarAdvDebug->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("STRATA_ADVANCED_DEBUG_ENABLED"), bStrataAdvDebug ? 1 : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Material.RoughDiffuse"));
		const bool bMaterialRoughDiffuse = CVar && CVar->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("MATERIAL_ROUGHDIFFUSE"), bMaterialRoughDiffuse ? 1 : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Supported"));
		const bool bLumenSupported = CVar->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("PROJECT_SUPPORTS_LUMEN"), bLumenSupported ? 1 : 0);
	}

	{
		const bool bSupportOIT = FDataDrivenShaderPlatformInfo::GetSupportsOIT(EShaderPlatform(Target.Platform));
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.OIT.SortedPixels"));
		const bool bOIT = CVar && CVar->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("PROJECT_OIT"), (bSupportOIT && bOIT) ? 1 : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Material.EnergyConservation"));
		const bool bMaterialEnergyConservation = CVar && CVar->GetInt() != 0;
		Input.Environment.SetDefine(TEXT("MATERIAL_ENERGYCONSERVATION"), bMaterialEnergyConservation ? 1 : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SupportSkyAtmosphereAffectsHeightFog"));
		Input.Environment.SetDefine(TEXT("PROJECT_SUPPORT_SKY_ATMOSPHERE_AFFECTS_HEIGHFOG"), (CVar && bSupportSkyAtmosphere) ? (CVar->GetInt() != 0) : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
		int32 PropagateAlpha = CVar->GetInt();

		if (bIsMobilePlatform)
		{
			static FShaderPlatformCachedIniValue<int32> MobilePropagateAlphaIniValue(TEXT("r.Mobile.PropagateAlpha"));
			int MobilePropagateAlphaIniValueInt = MobilePropagateAlphaIniValue.Get((EShaderPlatform)ShaderPlatform);
			PropagateAlpha = MobilePropagateAlphaIniValueInt > 0 ? 2 : 0;
		}

		if (PropagateAlpha < 0 || PropagateAlpha > 2)
		{
			PropagateAlpha = 0;
		}
		Input.Environment.SetDefine(TEXT("POST_PROCESS_ALPHA"), PropagateAlpha);
	}

	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_RENDERTARGET_WRITE_MASK"), RHISupportsRenderTargetWriteMask(EShaderPlatform(Target.Platform)) ? 1 : 0);
	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_PER_PIXEL_DBUFFER_MASK"), FDataDrivenShaderPlatformInfo::GetSupportsPerPixelDBufferMask(EShaderPlatform(Target.Platform)) ? 1 : 0);
	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_DISTANCE_FIELDS"), DoesPlatformSupportDistanceFields(EShaderPlatform(Target.Platform)) ? 1 : 0);
	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_MESH_SHADERS_TIER0"), RHISupportsMeshShadersTier0(EShaderPlatform(Target.Platform)) ? 1 : 0);
	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_MESH_SHADERS_TIER1"), RHISupportsMeshShadersTier1(EShaderPlatform(Target.Platform)) ? 1 : 0);
	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_BINDLESS"), RHISupportsBindless(EShaderPlatform(Target.Platform)) ? 1 : 0);
	Input.Environment.SetDefine(TEXT("PLATFORM_ALLOW_SCENE_DATA_COMPRESSED_TRANSFORMS"), FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(EShaderPlatform(Target.Platform)) ? 1 : 0);

	if (RHISupportsBindless(EShaderPlatform(Target.Platform)))
	{
		const bool bIsRaytracingShader = IsRayTracingShaderFrequency(Input.Target.GetFrequency());

		const ERHIBindlessConfiguration ResourcesConfig = RHIGetBindlessResourcesConfiguration(EShaderPlatform(Target.Platform));
		const ERHIBindlessConfiguration SamplersConfig = RHIGetBindlessSamplersConfiguration(EShaderPlatform(Target.Platform));

		if (ResourcesConfig == ERHIBindlessConfiguration::AllShaders || (ResourcesConfig == ERHIBindlessConfiguration::RayTracingShaders && bIsRaytracingShader))
		{
			Input.Environment.CompilerFlags.Add(CFLAG_BindlessResources);
			Input.Environment.SetDefine(TEXT("ENABLE_BINDLESS_RESOURCES"), true);
		}

		if (SamplersConfig == ERHIBindlessConfiguration::AllShaders || (SamplersConfig == ERHIBindlessConfiguration::RayTracingShaders && bIsRaytracingShader))
		{
			Input.Environment.CompilerFlags.Add(CFLAG_BindlessSamplers);
			Input.Environment.SetDefine(TEXT("ENABLE_BINDLESS_SAMPLERS"), true);
		}
	}

	if (CVarShadersRemoveDeadCode.GetValueOnAnyThread())
	{
		Input.Environment.CompilerFlags.Add(CFLAG_RemoveDeadCode);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VT.AnisotropicFiltering"));
		Input.Environment.SetDefine(TEXT("VIRTUAL_TEXTURE_ANISOTROPIC_FILTERING"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	if (IsMobilePlatform((EShaderPlatform)Target.Platform))
	{
		const bool bMobileMovableSpotlightShadowsEnabled = IsMobileMovableSpotlightShadowsEnabled(Target.GetPlatform());
		Input.Environment.SetDefine(TEXT("PROJECT_MOBILE_ENABLE_MOVABLE_SPOTLIGHT_SHADOWS"), bMobileMovableSpotlightShadowsEnabled ? 1 : 0);
	}

	const double TileSize = FLargeWorldRenderScalar::GetTileSize();
	Input.Environment.SetDefine(TEXT("UE_LWC_RENDER_TILE_SIZE"), (float)TileSize);
	Input.Environment.SetDefine(TEXT("UE_LWC_RENDER_TILE_SIZE_SQRT"), (float)FMath::Sqrt(TileSize));
	Input.Environment.SetDefine(TEXT("UE_LWC_RENDER_TILE_SIZE_RSQRT"), (float)FMath::InvSqrt(TileSize));
	Input.Environment.SetDefine(TEXT("UE_LWC_RENDER_TILE_SIZE_RCP"), (float)(1.0 / TileSize));
	Input.Environment.SetDefine(TEXT("UE_LWC_RENDER_TILE_SIZE_FMOD_PI"), (float)FMath::Fmod(TileSize, UE_DOUBLE_PI));
	Input.Environment.SetDefine(TEXT("UE_LWC_RENDER_TILE_SIZE_FMOD_2PI"), (float)FMath::Fmod(TileSize, 2.0 * UE_DOUBLE_PI));

	// Allow the target shader format to modify the shader input before we add it as a job
	const IShaderFormat* Format = GetTargetPlatformManagerRef().FindShaderFormat(ShaderFormatName);
	checkf(Format, TEXT("Shader format %s cannot be found"), *ShaderFormatName.ToString());
	Format->ModifyShaderCompilerInput(Input);

	// Allow the GBuffer and other shader defines to cause dependend environment changes, but minimizing the #ifdef magic in the shaders, which
	// is nearly impossible to debug when it goes wrong.
#if WITH_EDITOR
	FShaderCompileUtilities::ApplyDerivedDefines(Input.Environment, Input.SharedEnvironment, (EShaderPlatform)Target.Platform);
#endif
}


/** Timer class used to report information on the 'recompileshaders' console command. */
class FRecompileShadersTimer
{
public:
	FRecompileShadersTimer(const TCHAR* InInfoStr=TEXT("Test")) :
		InfoStr(InInfoStr),
		bAlreadyStopped(false)
	{
		StartTime = FPlatformTime::Seconds();
	}

	FRecompileShadersTimer(const FString& InInfoStr) :
		InfoStr(InInfoStr),
		bAlreadyStopped(false)
	{
		StartTime = FPlatformTime::Seconds();
	}

	void Stop(bool DisplayLog = true)
	{
		if (!bAlreadyStopped)
		{
			bAlreadyStopped = true;
			EndTime = FPlatformTime::Seconds();
			TimeElapsed = EndTime-StartTime;
			if (DisplayLog)
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("		[%s] took [%.4f] s"),*InfoStr,TimeElapsed);
			}
		}
	}

	~FRecompileShadersTimer()
	{
		Stop(true);
	}

protected:
	double StartTime,EndTime;
	double TimeElapsed;
	FString InfoStr;
	bool bAlreadyStopped;
};

namespace
{
	ODSCRecompileCommand ParseRecompileCommandString(const TCHAR* CmdString, TArray<FString>& OutMaterialsToLoad, FString& OutShaderTypesToLoad)
	{
		FString CmdName = FParse::Token(CmdString, 0);

		ODSCRecompileCommand CommandType = ODSCRecompileCommand::None;
		OutMaterialsToLoad.Empty();

		if( !CmdName.IsEmpty() && FCString::Stricmp(*CmdName,TEXT("Material"))==0 )
		{
			CommandType = ODSCRecompileCommand::Material;

			// tell other side the material to load, by pathname
			FString RequestedMaterialName( FParse::Token( CmdString, 0 ) );

			for( TObjectIterator<UMaterialInterface> It; It; ++It )
			{
				UMaterial* Material = It->GetMaterial();

				if( Material && Material->GetName() == RequestedMaterialName)
				{
					OutMaterialsToLoad.Add(It->GetPathName());
					break;
				}
			}
		}
		else if (!CmdName.IsEmpty() && FCString::Stricmp(*CmdName, TEXT("Global")) == 0)
		{
			CommandType = ODSCRecompileCommand::Global;
		}
		else if (!CmdName.IsEmpty() && FCString::Stricmp(*CmdName, TEXT("Changed")) == 0)
		{
			CommandType = ODSCRecompileCommand::Changed;

			// Compile all the shaders that have changed for the materials we have loaded.
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				OutMaterialsToLoad.Add(It->GetPathName());
			}
		}
		else if (FCString::Stricmp(*CmdName, TEXT("All")) == 0)
		{
			CommandType = ODSCRecompileCommand::Material;

			// tell other side all the materials to load, by pathname
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				OutMaterialsToLoad.Add(It->GetPathName());
			}
		}
		else
		{
			CommandType = ODSCRecompileCommand::SingleShader;

			OutShaderTypesToLoad = CmdName;

			// tell other side which materials to load and compile the single
			// shader for.
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				OutMaterialsToLoad.Add(It->GetPathName());
			}
		}

		return CommandType;
	}
}

void ProcessCookOnTheFlyShaders(bool bReloadGlobalShaders, const TArray<uint8>& MeshMaterialMaps, const TArray<FString>& MaterialsToLoad, const TArray<uint8>& GlobalShaderMap)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessCookOnTheFlyShaders);
	check(IsInGameThread());

	bool bHasFlushed = false;

	auto DoFlushIfNecessary = [&bHasFlushed]() {
		if (!bHasFlushed )
		{
			// now we need to refresh the RHI resources
			FlushRenderingCommands();
			bHasFlushed = true;
		}
	};

	// reload the global shaders
	if (bReloadGlobalShaders)
	{
		DoFlushIfNecessary();

		// Some platforms rely on global shaders to be created to implement basic RHI functionality
		TGuardValue<int32> Guard(GCreateShadersOnLoad, 1);
		CompileGlobalShaderMap(true);
	}

	// load all the mesh material shaders if any were sent back
	if (MeshMaterialMaps.Num() > 0)
	{
		DoFlushIfNecessary();

		// parse the shaders
		FMemoryReader MemoryReader(MeshMaterialMaps, true);
		FNameAsStringProxyArchive Ar(MemoryReader);

		TArray<UMaterialInterface*> LoadedMaterials;
		FMaterialShaderMap::LoadForRemoteRecompile(Ar, GMaxRHIShaderPlatform, LoadedMaterials);

		// Only update materials if we need to.
		if (LoadedMaterials.Num())
		{
			// this will stop the rendering thread, and reattach components, in the destructor
			FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::RecreateRenderStates);

			// gather the shader maps to reattach
			for (UMaterialInterface* Material : LoadedMaterials)
			{
				Material->RecacheUniformExpressions(true);
				UpdateContext.AddMaterialInterface(Material);
			}
		}
	}

	// load all the global shaders if any were sent back
	if (GlobalShaderMap.Num() > 0)
	{
		DoFlushIfNecessary();

		// parse the shaders
		FMemoryReader MemoryReader(GlobalShaderMap, true);
		FNameAsStringProxyArchive Ar(MemoryReader);

		LoadGlobalShadersForRemoteRecompile(Ar, GMaxRHIShaderPlatform);
	}
}

/**
* Forces a recompile of the global shaders.
*/
void RecompileGlobalShaders()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
			auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
			GetGlobalShaderMap(ShaderPlatform)->Empty();
			VerifyGlobalShaders(ShaderPlatform, nullptr, false);
		});

		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}
}

void GetOutdatedShaderTypes(TArray<const FShaderType*>& OutdatedShaderTypes, TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes)
{
#if WITH_EDITOR
	for (int PlatformIndex = 0; PlatformIndex < SP_NumPlatforms; ++PlatformIndex)
	{
		const FGlobalShaderMap* ShaderMap = GGlobalShaderMap[PlatformIndex];
		if (ShaderMap)
		{
			ShaderMap->GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
		}
	}

	FMaterialShaderMap::GetAllOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);

	for (int32 TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
	{
		UE_LOG(LogShaders, Warning, TEXT("		Recompiling %s"), OutdatedShaderTypes[TypeIndex]->GetName());
	}
	for (int32 TypeIndex = 0; TypeIndex < OutdatedShaderPipelineTypes.Num(); TypeIndex++)
	{
		UE_LOG(LogShaders, Warning, TEXT("		Recompiling %s"), OutdatedShaderPipelineTypes[TypeIndex]->GetName());
	}
	for (int32 TypeIndex = 0; TypeIndex < OutdatedFactoryTypes.Num(); TypeIndex++)
	{
		UE_LOG(LogShaders, Warning, TEXT("		Recompiling %s"), OutdatedFactoryTypes[TypeIndex]->GetName());
	}
#endif // WITH_EDITOR
}

bool RecompileShaders(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// if this platform can't compile shaders, then we try to send a message to a file/cooker server
	if (FPlatformProperties::RequiresCookedData())
	{
#if WITH_ODSC
		TArray<FString> MaterialsToLoad;
		FString ShaderTypesToLoad;
		ODSCRecompileCommand CommandType = ParseRecompileCommandString(Cmd, MaterialsToLoad, ShaderTypesToLoad);

		ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(GMaxRHIShaderPlatform);
		const EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		GODSCManager->AddThreadedRequest(MaterialsToLoad, ShaderTypesToLoad, GMaxRHIShaderPlatform, TargetFeatureLevel, ActiveQualityLevel, CommandType);
#endif
		return true;
	}

	FString FlagStr(FParse::Token(Cmd, 0));
	if( FlagStr.Len() > 0 )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RecompileShaders);
		GWarn->BeginSlowTask( NSLOCTEXT("ShaderCompilingManager", "BeginRecompilingShadersTask", "Recompiling shaders"), true );

		// Flush the shader file cache so that any changes to shader source files will be detected
		FlushShaderFileCache();
		FlushRenderingCommands();

		if( FCString::Stricmp(*FlagStr,TEXT("Changed"))==0)
		{
			TArray<const FShaderType*> OutdatedShaderTypes;
			TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
			TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;
			{
				FRecompileShadersTimer SearchTimer(TEXT("Searching for changed files"));
				GetOutdatedShaderTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
			}

			if (OutdatedShaderPipelineTypes.Num() > 0 || OutdatedShaderTypes.Num() > 0 || OutdatedFactoryTypes.Num() > 0)
			{
				FRecompileShadersTimer TestTimer(TEXT("RecompileShaders Changed"));

				UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
					auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
					BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);
				});

				// Block on global shaders
				FinishRecompileGlobalShaders();

				// Kick off global shader recompiles
				UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
					auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
					UMaterial::UpdateMaterialShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes, ShaderPlatform);
				});

				GWarn->StatusUpdate(0, 1, NSLOCTEXT("ShaderCompilingManager", "CompilingGlobalShaderStatus", "Compiling global shaders..."));
			}
			else
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("No Shader changes found."));
			}
		}
		else if( FCString::Stricmp(*FlagStr,TEXT("Global"))==0)
		{
			FRecompileShadersTimer TestTimer(TEXT("RecompileShaders Global"));
			RecompileGlobalShaders();
		}
		else if( FCString::Stricmp(*FlagStr,TEXT("Material"))==0)
		{
			FString RequestedMaterialName(FParse::Token(Cmd, 0));
			FRecompileShadersTimer TestTimer(FString::Printf(TEXT("Recompile Material %s"), *RequestedMaterialName));

			ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
			FString TargetPlatformName(FParse::Token(Cmd, 0));
			const ITargetPlatform* TargetPlatform = nullptr;
			if (TargetPlatformName.Len() > 0)
			{
				TargetPlatform = TPM.FindTargetPlatform(TargetPlatformName);
			}

			bool bMaterialFound = false;
			for( TObjectIterator<UMaterialInterface> It; It; ++It )
			{
				UMaterialInterface* Material = *It;
				if( Material && Material->GetName() == RequestedMaterialName)
				{
					bMaterialFound = true;
#if WITH_EDITOR
					// <Pre/Post>EditChange will force a re-creation of the resource,
					// in turn recompiling the shader.
					if (TargetPlatform)
					{
						Material->BeginCacheForCookedPlatformData(TargetPlatform);
						while (!Material->IsCachedCookedPlatformDataLoaded(TargetPlatform))
						{
							FPlatformProcess::Sleep(0.1f);
							GShaderCompilingManager->ProcessAsyncResults(false, false);
						}
						Material->ClearCachedCookedPlatformData(TargetPlatform);
					}
					else
					{
						Material->PreEditChange(nullptr);
						Material->PostEditChange();
					}
#endif // WITH_EDITOR
					break;
				}
			}

			if (!bMaterialFound)
			{
				TestTimer.Stop(false);
				UE_LOG(LogShaderCompilers, Warning, TEXT("Couldn't find Material %s!"), *RequestedMaterialName);
			}
		}
		else if( FCString::Stricmp(*FlagStr,TEXT("All"))==0)
		{
			FRecompileShadersTimer TestTimer(TEXT("RecompileShaders"));
			RecompileGlobalShaders();

			FMaterialUpdateContext UpdateContext(0);
			for( TObjectIterator<UMaterial> It; It; ++It )
			{
				UMaterial* Material = *It;
				if( Material )
				{
					UE_LOG(LogShaderCompilers, Log, TEXT("recompiling [%s]"),*Material->GetFullName());
					UpdateContext.AddMaterial(Material);
#if WITH_EDITOR
					// <Pre/Post>EditChange will force a re-creation of the resource,
					// in turn recompiling the shader.
					Material->PreEditChange(nullptr);
					Material->PostEditChange();
#endif // WITH_EDITOR
				}
			}
		}
		else
		{
			TArray<const FShaderType*> ShaderTypes = FShaderType::GetShaderTypesByFilename(*FlagStr);
			TArray<const FShaderPipelineType*> ShaderPipelineTypes = FShaderPipelineType::GetShaderPipelineTypesByFilename(*FlagStr);
			if (ShaderTypes.Num() > 0 || ShaderPipelineTypes.Num() > 0)
			{
				FRecompileShadersTimer TestTimer(TEXT("RecompileShaders SingleShader"));
				
				TArray<const FVertexFactoryType*> FactoryTypes;

				UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
					auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
					BeginRecompileGlobalShaders(ShaderTypes, ShaderPipelineTypes, ShaderPlatform);
					//UMaterial::UpdateMaterialShaders(ShaderTypes, ShaderPipelineTypes, FactoryTypes, ShaderPlatform);
					FinishRecompileGlobalShaders();
				});
			}
		}

		GWarn->EndSlowTask();

		return 1;
	}

	UE_LOG(LogShaderCompilers, Warning, TEXT("Invalid parameter. Options are: \n'Changed', 'Global', 'Material [name]', 'All'."));
	return 1;
}

static void PrepareGlobalShaderCompileJob(EShaderPlatform Platform,
	EShaderPermutationFlags PermutationFlags,
	const FShaderPipelineType* ShaderPipeline,
	FShaderCompileJob* NewJob)
{
	const FShaderCompileJobKey& Key = NewJob->Key;
	const FGlobalShaderType* ShaderType = Key.ShaderType->AsGlobalShaderType();

	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("	%s (permutation %d)"), ShaderType->GetName(), Key.PermutationId);
	COOK_STAT(GlobalShaderCookStats::ShadersCompiled++);

	// Allow the shader type to modify the compile environment.
	ShaderType->SetupCompileEnvironment(Platform, Key.PermutationId, PermutationFlags, ShaderEnvironment);

	static FString GlobalName(TEXT("Global"));

	NewJob->bErrorsAreLikelyToBeCode = true;
	NewJob->bIsGlobalShader = true;
	NewJob->bIsDefaultMaterial = false;

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		GlobalName,
		nullptr,
		ShaderType,
		ShaderPipeline,
		Key.PermutationId,
		ShaderType->GetShaderFilename(),
		ShaderType->GetFunctionName(),
		FShaderTarget(ShaderType->GetFrequency(), Platform),
		NewJob->Input
	);
}

void FGlobalShaderTypeCompiler::BeginCompileShader(const FGlobalShaderType* ShaderType, int32 PermutationId, EShaderPlatform Platform, EShaderPermutationFlags PermutationFlags, TArray<FShaderCommonCompileJobPtr>& NewJobs)
{
	// Global shaders are always high priority (often need to block on completion)
	FShaderCompileJob* NewJob = GShaderCompilingManager->PrepareShaderCompileJob(GlobalShaderMapId, FShaderCompileJobKey(ShaderType, nullptr, PermutationId), EShaderCompileJobPriority::High);
	if (NewJob)
	{
		PrepareGlobalShaderCompileJob(Platform, PermutationFlags, nullptr, NewJob);
		NewJobs.Add(FShaderCommonCompileJobPtr(NewJob));
	}
}

void FGlobalShaderTypeCompiler::BeginCompileShaderPipeline(EShaderPlatform Platform, EShaderPermutationFlags PermutationFlags, const FShaderPipelineType* ShaderPipeline, TArray<FShaderCommonCompileJobPtr>& NewJobs)
{
	check(ShaderPipeline);
	UE_LOG(LogShaders, Verbose, TEXT("	Pipeline: %s"), ShaderPipeline->GetName());

	// Add all the jobs as individual first, then add the dependencies into a pipeline job
	FShaderPipelineCompileJob* NewPipelineJob = GShaderCompilingManager->PreparePipelineCompileJob(GlobalShaderMapId, FShaderPipelineCompileJobKey(ShaderPipeline, nullptr, kUniqueShaderPermutationId), EShaderCompileJobPriority::High);
	if (NewPipelineJob)
	{
		for (FShaderCompileJob* StageJob : NewPipelineJob->StageJobs)
		{
			PrepareGlobalShaderCompileJob(Platform, PermutationFlags, ShaderPipeline, StageJob);
		}
		NewJobs.Add(FShaderCommonCompileJobPtr(NewPipelineJob));
	}
}

FShader* FGlobalShaderTypeCompiler::FinishCompileShader(const FGlobalShaderType* ShaderType, const FShaderCompileJob& CurrentJob, const FShaderPipelineType* ShaderPipelineType)
{
	FShader* Shader = nullptr;
	if (CurrentJob.bSucceeded)
	{
		EShaderPlatform Platform = CurrentJob.Input.Target.GetPlatform();
		FGlobalShaderMapSection* Section = GGlobalShaderMap[Platform]->FindOrAddSection(ShaderType);

		Section->GetResourceCode()->AddShaderCompilerOutput(CurrentJob.Output);

		if (ShaderPipelineType && !ShaderPipelineType->ShouldOptimizeUnusedOutputs(CurrentJob.Input.Target.GetPlatform()))
		{
			// If sharing shaders in this pipeline, remove it from the type/id so it uses the one in the shared shadermap list
			ShaderPipelineType = nullptr;
		}

		// Create the global shader map hash
		FSHAHash GlobalShaderMapHash;
		{
			FSHA1 HashState;
			const TCHAR* GlobalShaderString = TEXT("GlobalShaderMap");
			HashState.UpdateWithString(GlobalShaderString, FCString::Strlen(GlobalShaderString));
			HashState.Final();
			HashState.GetHash(&GlobalShaderMapHash.Hash[0]);
		}

		Shader = ShaderType->ConstructCompiled(FGlobalShaderType::CompiledShaderInitializerType(ShaderType, nullptr, CurrentJob.Key.PermutationId, CurrentJob.Output, GlobalShaderMapHash, ShaderPipelineType, nullptr));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(ShaderType->GetName(), CurrentJob.Output.Target, CurrentJob.Key.VFType);
	}

	if (CurrentJob.Output.Errors.Num() > 0)
	{
		if (CurrentJob.bSucceeded == false)
		{
			UE_LOG(LogShaderCompilers, Error, TEXT("Errors compiling global shader %s %s %s:\n"), CurrentJob.Key.ShaderType->GetName(), ShaderPipelineType ? TEXT("ShaderPipeline") : TEXT(""), ShaderPipelineType ? ShaderPipelineType->GetName() : TEXT(""));
			for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
			{
				UE_LOG(LogShaderCompilers, Display, TEXT("%s"), *CurrentJob.Output.Errors[ErrorIndex].GetErrorStringWithLineMarker());
			}
		}
		else if (GShowShaderWarnings)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("Warnings compiling global shader %s %s %s:\n"), CurrentJob.Key.ShaderType->GetName(), ShaderPipelineType ? TEXT("ShaderPipeline") : TEXT(""), ShaderPipelineType ? ShaderPipelineType->GetName() : TEXT(""));
			for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
			{
				UE_LOG(LogShaderCompilers, Display, TEXT("%s"), *CurrentJob.Output.Errors[ErrorIndex].GetErrorStringWithLineMarker());
			}
		}
	}

	return Shader;
}

namespace ShaderCompilerUtil
{
	FOnGlobalShadersCompilation GOnGlobalShdersCompilationDelegate;
}

FOnGlobalShadersCompilation& GetOnGlobalShaderCompilation()
{
	return ShaderCompilerUtil::GOnGlobalShdersCompilationDelegate;
}

/**
* Makes sure all global shaders are loaded and/or compiled for the passed in platform.
* Note: if compilation is needed, this only kicks off the compile.
*
* @param	Platform	Platform to verify global shaders for
*/
void VerifyGlobalShaders(EShaderPlatform Platform, const ITargetPlatform* TargetPlatform, bool bLoadedFromCacheFile, const TArray<const FShaderType*>* OutdatedShaderTypes, const TArray<const FShaderPipelineType*>* OutdatedShaderPipelineTypes)
{
	SCOPED_LOADTIMER(VerifyGlobalShaders);

	check(IsInGameThread());
	check(!FPlatformProperties::IsServerOnly());
	check(GGlobalShaderMap[Platform]);

	UE_LOG(LogMaterial, Verbose, TEXT("Verifying Global Shaders for %s (%s)"), *LegacyShaderPlatformToShaderFormat(Platform).ToString(), *ShaderCompiler::GetTargetPlatformName(TargetPlatform));

	// Ensure that the global shader map contains all global shader types.
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(Platform);
	const bool bEmptyMap = GlobalShaderMap->IsEmpty();
	if (bEmptyMap)
	{
		UE_LOG(LogShaders, Log, TEXT("	Empty global shader map, recompiling all global shaders"));
	}

	FPlatformTypeLayoutParameters LayoutParams;
	LayoutParams.InitializeForPlatform(TargetPlatform);
	EShaderPermutationFlags PermutationFlags = GetShaderPermutationFlags(LayoutParams);

	// if the target is the current platform, then we are not cooking for another platform, in which case we want to use
	// the loaded permutation flags that are in the shader map (or the current platform's permutation if it wasn't loaded, 
	// see the FShaderMapBase constructor)
	if (bLoadedFromCacheFile)
	{
		PermutationFlags = GlobalShaderMap->GetFirstSection()->GetPermutationFlags();
	}

	bool bErrorOnMissing = bLoadedFromCacheFile;
	if (FPlatformProperties::RequiresCookedData())
	{
		// We require all shaders to exist on cooked platforms because we can't compile them.
		bErrorOnMissing = true;
	}

	// All jobs, single & pipeline
	TArray<FShaderCommonCompileJobPtr> GlobalShaderJobs;

	// Add the single jobs first
	TMap<TShaderTypePermutation<const FShaderType>, FShaderCompileJob*> SharedShaderJobs;

	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (!GlobalShaderType)
		{
			continue;
		}

		int32 PermutationCountToCompile = 0;
		for (int32 PermutationId = 0; PermutationId < GlobalShaderType->GetPermutationCount(); PermutationId++)
		{
			if (GlobalShaderType->ShouldCompilePermutation(Platform, PermutationId, PermutationFlags))
			{
				bool bOutdated = OutdatedShaderTypes && OutdatedShaderTypes->Contains(GlobalShaderType);
				TShaderRef<FShader> GlobalShader = GlobalShaderMap->GetShader(GlobalShaderType, PermutationId);
				if (bOutdated || !GlobalShader.IsValid())
				{
					if (bErrorOnMissing)
					{
						UE_LOG(LogShaders, Fatal, TEXT("Missing global shader %s's permutation %i, Please make sure cooking was successful."),
							GlobalShaderType->GetName(), PermutationId);
					}

					if (OutdatedShaderTypes)
					{
						// Remove old shader, if it exists
						GlobalShaderMap->RemoveShaderTypePermutaion(GlobalShaderType, PermutationId);
					}

					// Compile this global shader type.
					FGlobalShaderTypeCompiler::BeginCompileShader(GlobalShaderType, PermutationId, Platform, PermutationFlags, GlobalShaderJobs);
					//TShaderTypePermutation<const FShaderType> ShaderTypePermutation(GlobalShaderType, PermutationId);
					//check(!SharedShaderJobs.Find(ShaderTypePermutation));
					//SharedShaderJobs.Add(ShaderTypePermutation, Job);
					PermutationCountToCompile++;
				}
			}
		}

		ensureMsgf(
			PermutationCountToCompile <= 832,	// Nanite culling as of today (2022-01-11) can go up to 832 permutations
			TEXT("Global shader %s has %i permutations: probably more than it needs."),
			GlobalShaderType->GetName(), PermutationCountToCompile);

		if (!bEmptyMap && PermutationCountToCompile > 0)
		{
			UE_LOG(LogShaders, Log, TEXT("	%s (%i out of %i)"),
				GlobalShaderType->GetName(), PermutationCountToCompile, GlobalShaderType->GetPermutationCount());
		}
	}

	// Now the pipeline jobs; if it's a shareable pipeline, do not add duplicate jobs
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline())
		{
			if (FGlobalShaderType::ShouldCompilePipeline(Pipeline, Platform, PermutationFlags)
				&& (!GlobalShaderMap->HasShaderPipeline(Pipeline) || (OutdatedShaderPipelineTypes && OutdatedShaderPipelineTypes->Contains(Pipeline))))
			{
				if (OutdatedShaderPipelineTypes)
				{
					// Remove old pipeline
					GlobalShaderMap->RemoveShaderPipelineType(Pipeline);
				}

				if (bErrorOnMissing)
				{
					UE_LOG(LogShaders, Fatal, TEXT("Missing global shader pipeline %s, Please make sure cooking was successful."), Pipeline->GetName());
				}

				if (!bEmptyMap)
				{
					UE_LOG(LogShaders, Log, TEXT("	%s"), Pipeline->GetName());
				}

				if (Pipeline->ShouldOptimizeUnusedOutputs(Platform))
				{
					// Make a pipeline job with all the stages
					FGlobalShaderTypeCompiler::BeginCompileShaderPipeline(Platform, PermutationFlags, Pipeline, GlobalShaderJobs);
				}
				else
				{
					// If sharing shaders amongst pipelines, add this pipeline as a dependency of an existing individual job
					for (const FShaderType* ShaderType : Pipeline->GetStages())
					{
						TShaderTypePermutation<const FShaderType> ShaderTypePermutation(ShaderType, kUniqueShaderPermutationId);

						FShaderCompileJob** Job = SharedShaderJobs.Find(ShaderTypePermutation);
						checkf(Job, TEXT("Couldn't find existing shared job for global shader %s on pipeline %s!"), ShaderType->GetName(), Pipeline->GetName());
						auto* SingleJob = (*Job)->GetSingleShaderJob();
						check(SingleJob);
						auto& SharedPipelinesInJob = SingleJob->SharingPipelines.FindOrAdd(nullptr);
						check(!SharedPipelinesInJob.Contains(Pipeline));
						SharedPipelinesInJob.Add(Pipeline);
					}
				}
			}
		}
	}

	if (GlobalShaderJobs.Num() > 0)
	{
		GetOnGlobalShaderCompilation().Broadcast();
		GShaderCompilingManager->SubmitJobs(GlobalShaderJobs, "Globals");

		const bool bAllowAsynchronousGlobalShaderCompiling =
			// OpenGL requires that global shader maps are compiled before attaching
			// primitives to the scene as it must be able to find FNULLPS.
			// TODO_OPENGL: Allow shaders to be compiled asynchronously.
			// Metal also needs this when using RHI thread because it uses TOneColorVS very early in RHIPostInit()
			!IsOpenGLPlatform(GMaxRHIShaderPlatform) && !IsVulkanPlatform(GMaxRHIShaderPlatform) &&
			!IsMetalPlatform(GMaxRHIShaderPlatform) &&
			FDataDrivenShaderPlatformInfo::GetSupportsAsyncPipelineCompilation(GMaxRHIShaderPlatform) &&
			GShaderCompilingManager->AllowAsynchronousShaderCompiling();

		if (!bAllowAsynchronousGlobalShaderCompiling)
		{
			TArray<int32> ShaderMapIds;
			ShaderMapIds.Add(GlobalShaderMapId);

			GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
		}
	}
}

void VerifyGlobalShaders(EShaderPlatform Platform, bool bLoadedFromCacheFile, const TArray<const FShaderType*>* OutdatedShaderTypes, const TArray<const FShaderPipelineType*>* OutdatedShaderPipelineTypes)
{
	VerifyGlobalShaders(Platform, nullptr, bLoadedFromCacheFile, OutdatedShaderTypes, OutdatedShaderPipelineTypes);
}

void PrecacheComputePipelineStatesForGlobalShaders(EShaderPlatform Platform, const ITargetPlatform* TargetPlatform)
{
	static IConsoleVariable* PrecacheGlobalComputeShadersCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PSOPrecache.GlobalComputeShaders"));
	if (!PipelineStateCache::IsPSOPrecachingEnabled() || PrecacheGlobalComputeShadersCVar == nullptr || PrecacheGlobalComputeShadersCVar->GetInt() == 0)
	{
		return;
	}

	FPlatformTypeLayoutParameters LayoutParams;
	LayoutParams.InitializeForPlatform(TargetPlatform);
	EShaderPermutationFlags PermutationFlags = GetShaderPermutationFlags(LayoutParams);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(Platform);

	// some RHIs (OpenGL) can only create shaders on the Render thread. Queue the creation instead of doing it here.
	TArray<TShaderRef<FShader>> ComputeShadersToPrecache;
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (!GlobalShaderType)
		{
			continue;
		}

		for (int32 PermutationId = 0; PermutationId < GlobalShaderType->GetPermutationCount(); PermutationId++)
		{
			if (GlobalShaderType->ShouldCompilePermutation(Platform, PermutationId, PermutationFlags))
			{
				TShaderRef<FShader> GlobalShader = GlobalShaderMap->GetShader(GlobalShaderType, PermutationId);
				if (GlobalShader.IsValid() && GlobalShader->GetFrequency() == SF_Compute)
				{
					ComputeShadersToPrecache.Add(GlobalShader);
				}
			}
		}
	}

	if (ComputeShadersToPrecache.Num() > 0)
	{
		ENQUEUE_RENDER_COMMAND(PrecachePSOsForGlobalShaders)(
			[ComputeShadersToPrecache](FRHICommandListImmediate& RHICmdList)
			{
				for (TShaderRef<FShader> GlobalShader : ComputeShadersToPrecache)
				{
					FRHIComputeShader* RHIComputeShader = GlobalShader.GetComputeShader();
					PipelineStateCache::PrecacheComputePipelineState(RHIComputeShader);
				}
			});
	}
}

#include "Misc/PreLoadFile.h"
#include "Serialization/LargeMemoryReader.h"
static FPreLoadFile GGlobalShaderPreLoadFile(*(FString(TEXT("../../../Engine")) / TEXT("GlobalShaderCache-SP_") + FPlatformProperties::IniPlatformName() + TEXT(".bin")));

static const ITargetPlatform* GGlobalShaderTargetPlatform[SP_NumPlatforms] = { nullptr };

static FString GGlobalShaderCacheOverrideDirectory;

static FString GetGlobalShaderCacheOverrideFilename(EShaderPlatform Platform)
{
	FString DirectoryPrefix = FPaths::EngineDir() / TEXT("OverrideGlobalShaderCache-");

	if (!GGlobalShaderCacheOverrideDirectory.IsEmpty())
	{
		DirectoryPrefix = GGlobalShaderCacheOverrideDirectory / TEXT("GlobalShaderCache-");
	}

	return DirectoryPrefix + FDataDrivenShaderPlatformInfo::GetName(Platform).ToString() + TEXT(".bin");
}

static FString GetGlobalShaderCacheFilename(EShaderPlatform Platform)
{
	return FString(TEXT("Engine")) / TEXT("GlobalShaderCache-") + FDataDrivenShaderPlatformInfo::GetName(Platform).ToString() + TEXT(".bin");
}

#if WITH_EDITOR

static FString GetGlobalShaderMapKeyString(const FGlobalShaderMapId& ShaderMapId, EShaderPlatform Platform, TArray<FShaderTypeDependency> const& Dependencies)
{
	FName Format = LegacyShaderPlatformToShaderFormat(Platform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	ShaderMapAppendKeyString(Platform, ShaderMapKeyString);
	ShaderMapId.AppendKeyString(ShaderMapKeyString, Dependencies);
	return FString::Printf(TEXT("%s_%s_%s"), TEXT("GSM"), *GetGlobalShaderMapDDCKey(), *ShaderMapKeyString);
}

/** Creates a string key for the derived data cache entry for the global shader map. */
static UE::DerivedData::FCacheKey GetGlobalShaderMapKey(const FGlobalShaderMapId& ShaderMapId, EShaderPlatform Platform, const ITargetPlatform* TargetPlatform, TArray<FShaderTypeDependency> const& Dependencies)
{
	const FString DataKey = GetGlobalShaderMapKeyString(ShaderMapId, Platform, Dependencies);
	static const UE::DerivedData::FCacheBucket Bucket("GlobalShaderMap");
	return {Bucket, FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(DataKey)))};
}

static UE::DerivedData::FSharedString GetGlobalShaderMapName(const FGlobalShaderMapId& ShaderMapId, EShaderPlatform Platform, const FString& Key)
{
	return UE::DerivedData::FSharedString(WriteToString<256>(TEXTVIEW("GlobalShaderMap ["), LegacyShaderPlatformToShaderFormat(Platform), TEXTVIEW(", "), Key, TEXTVIEW("]")));
}
#endif

/** Saves the platform's shader map to the DDC. It is assumed that the caller will check IsComplete() first before calling the function. */
static void SaveGlobalShaderMapToDerivedDataCache(EShaderPlatform Platform)
{
#if WITH_EDITOR
	// We've finally built the global shader map, so we can count the miss as we put it in the DDC.
	COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());

	const ITargetPlatform* TargetPlatform = GGlobalShaderTargetPlatform[Platform];
	TArray<uint8> SaveData;

	FGlobalShaderMapId ShaderMapId(Platform, TargetPlatform);
	// caller should prevent incomplete shadermaps to be saved
	FGlobalShaderMap* GlobalSM = GetGlobalShaderMap(Platform);
	for (auto const& ShaderFilenameDependencies : ShaderMapId.GetShaderFilenameToDependeciesMap())
	{
		FGlobalShaderMapSection* Section = GlobalSM->FindSection(ShaderFilenameDependencies.Key);
		if (Section)
		{
			Section->FinalizeContent();

			SaveData.Reset();
			FMemoryWriter Ar(SaveData, true);
			Section->Serialize(Ar);
			COOK_STAT(Timer.AddMiss(SaveData.Num()));

			using namespace UE::DerivedData;
			FCachePutValueRequest Request;
			Request.Name = GetGlobalShaderMapName(ShaderMapId, Platform, ShaderFilenameDependencies.Key);
			Request.Key = GetGlobalShaderMapKey(ShaderMapId, Platform, TargetPlatform, ShaderFilenameDependencies.Value);
			Request.Value = FValue::Compress(MakeSharedBufferFromArray(MoveTemp(SaveData)));
			FRequestOwner AsyncOwner(EPriority::Normal);
			FRequestBarrier AsyncBarrier(AsyncOwner);
			GetCache().PutValue({Request}, AsyncOwner);
			AsyncOwner.KeepAlive();
		}
	}
#endif // WITH_EDITOR
}

/** Saves the global shader map as a file for the target platform. */
FString SaveGlobalShaderFile(EShaderPlatform Platform, FString SavePath, class ITargetPlatform* TargetPlatform)
{
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(Platform);

	// Wait until all global shaders are compiled
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}

	TArray<uint8> GlobalShaderData;
	{
		FMemoryWriter MemoryWriter(GlobalShaderData, true);

#if WITH_EDITOR
		TOptional<FArchiveCookData> CookData;
		FArchiveCookContext CookContext(nullptr /*InPackage*/, FArchiveCookContext::ECookTypeUnknown);
		if (TargetPlatform != nullptr)
		{
			CookData.Emplace(*TargetPlatform, CookContext);
		}
		
		MemoryWriter.SetCookData(CookData.GetPtrOrNull());
#endif

		GlobalShaderMap->SaveToGlobalArchive(MemoryWriter);
	}

	// make the final name
	FString FullPath = SavePath / GetGlobalShaderCacheFilename(Platform);
	if (!FFileHelper::SaveArrayToFile(GlobalShaderData, *FullPath))
	{
		UE_LOG(LogShaders, Fatal, TEXT("Could not save global shader file to '%s'"), *FullPath);
	}

#if WITH_EDITOR
	if (FShaderLibraryCooker::NeedsShaderStableKeys(Platform))
	{
		GlobalShaderMap->SaveShaderStableKeys(Platform);
	}
#endif // WITH_EDITOR
	return FullPath;
}


static inline bool ShouldCacheGlobalShaderTypeName(const FGlobalShaderType* GlobalShaderType, int32 PermutationId, const TCHAR* TypeNameSubstring, EShaderPlatform Platform, EShaderPermutationFlags PermutationFlags)
{
	return GlobalShaderType
		&& (TypeNameSubstring == nullptr || (FPlatformString::Strstr(GlobalShaderType->GetName(), TypeNameSubstring) != nullptr))
		&& GlobalShaderType->ShouldCompilePermutation(Platform, PermutationId, PermutationFlags);
};


bool IsGlobalShaderMapComplete(const TCHAR* TypeNameSubstring)
{
	for (int32 i = 0; i < SP_NumPlatforms; ++i)
	{
		EShaderPlatform Platform = (EShaderPlatform)i;

		FGlobalShaderMap* GlobalShaderMap = GGlobalShaderMap[Platform];

		// look at any shadermap in the GlobalShaderMap for the permutation flags, as they will all be the same
		if (GlobalShaderMap)
		{
			const FGlobalShaderMapSection* FirstShaderMap = GlobalShaderMap->GetFirstSection();
			if (FirstShaderMap == nullptr)
			{
				// if we had no sections at all, we know we aren't complete
				return false;
			}
			EShaderPermutationFlags GlobalShaderPermutation = FirstShaderMap->GetPermutationFlags();

			// Check if the individual shaders are complete
			for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
			{
				FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
				int32 PermutationCount = GlobalShaderType ? GlobalShaderType->GetPermutationCount() : 1;
				for (int32 PermutationId = 0; PermutationId < PermutationCount; PermutationId++)
				{
					if (ShouldCacheGlobalShaderTypeName(GlobalShaderType, PermutationId, TypeNameSubstring, Platform, GlobalShaderPermutation))
					{
						if (!GlobalShaderMap->HasShader(GlobalShaderType, PermutationId))
						{
							return false;
						}
					}
				}
			}

			// Then the pipelines as it may be sharing shaders
			for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
			{
				const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
				if (Pipeline->IsGlobalTypePipeline())
				{
					auto& Stages = Pipeline->GetStages();
					int32 NumStagesNeeded = 0;
					for (const FShaderType* Shader : Stages)
					{
						const FGlobalShaderType* GlobalShaderType = Shader->GetGlobalShaderType();
						if (ShouldCacheGlobalShaderTypeName(GlobalShaderType, kUniqueShaderPermutationId, TypeNameSubstring, Platform, GlobalShaderPermutation))
						{
							++NumStagesNeeded;
						}
						else
						{
							break;
						}
					}

					if (NumStagesNeeded == Stages.Num())
					{
						if (!GlobalShaderMap->HasShaderPipeline(Pipeline))
						{
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

static bool TryLoadCookedGlobalShaderMap(EShaderPlatform Platform, FScopedSlowTask& SlowTask)
{
	SlowTask.EnterProgressFrame(50);

	bool bLoadedFromCacheFile = false;

	// Load from the override global shaders first, this allows us to hot reload in cooked / pak builds
	TArray<uint8> GlobalShaderData;
	const bool bAllowOverrideGlobalShaders = !WITH_EDITOR && !UE_BUILD_SHIPPING;
	if (bAllowOverrideGlobalShaders)
	{
		FString OverrideGlobalShaderCacheFilename = GetGlobalShaderCacheOverrideFilename(Platform);
		FPaths::MakeStandardFilename(OverrideGlobalShaderCacheFilename);

		bool bFileExist = IFileManager::Get().FileExists(*OverrideGlobalShaderCacheFilename);

		if (!bFileExist)
		{
			UE_LOG(LogShaders, Display, TEXT("%s doesn't exists"), *OverrideGlobalShaderCacheFilename);
		}
		else
		{
			bLoadedFromCacheFile = FFileHelper::LoadFileToArray(GlobalShaderData, *OverrideGlobalShaderCacheFilename, FILEREAD_Silent);

			if (bLoadedFromCacheFile)
			{
				UE_LOG(LogShaders, Display, TEXT("%s has been loaded successfully"), *OverrideGlobalShaderCacheFilename);
			}
			else
			{
				UE_LOG(LogShaders, Error, TEXT("%s failed to load"), *OverrideGlobalShaderCacheFilename);
			}
		}
	}

	// is the data already loaded?
	int64 PreloadedSize = 0;
	void* PreloadedData = nullptr;
	if (!bLoadedFromCacheFile)
	{
		PreloadedData = GGlobalShaderPreLoadFile.TakeOwnershipOfLoadedData(&PreloadedSize);
	}

	if (PreloadedData != nullptr)
	{
		FLargeMemoryReader MemoryReader((uint8*)PreloadedData, PreloadedSize, ELargeMemoryReaderFlags::TakeOwnership);
		GGlobalShaderMap[Platform]->LoadFromGlobalArchive(MemoryReader);
		bLoadedFromCacheFile = true;
	}
	else
	{
		FString GlobalShaderCacheFilename = FPaths::GetRelativePathToRoot() / GetGlobalShaderCacheFilename(Platform);
		FPaths::MakeStandardFilename(GlobalShaderCacheFilename);
		if (!bLoadedFromCacheFile)
		{
			bLoadedFromCacheFile = FFileHelper::LoadFileToArray(GlobalShaderData, *GlobalShaderCacheFilename, FILEREAD_Silent);
		}

		if (bLoadedFromCacheFile)
		{
			FMemoryReader MemoryReader(GlobalShaderData);
			GGlobalShaderMap[Platform]->LoadFromGlobalArchive(MemoryReader);
		}
	}

	return bLoadedFromCacheFile;
}

void CompileGlobalShaderMap(EShaderPlatform Platform, const ITargetPlatform* TargetPlatform, bool bRefreshShaderMap)
{
	// No global shaders needed on dedicated server or clients that use NullRHI. Note that cook commandlet needs to have them, even if it is not allowed to render otherwise.
	if (FPlatformProperties::IsServerOnly() || (!IsRunningCommandlet() && !FApp::CanEverRender()))
	{
		if (!GGlobalShaderMap[Platform])
		{
			GGlobalShaderMap[Platform] = new FGlobalShaderMap(Platform);
		}
		return;
	}

	if (bRefreshShaderMap || GGlobalShaderTargetPlatform[Platform] != TargetPlatform)
	{
		// defer the deletion the current global shader map, delete the previous one if it is still valid
		delete GGlobalShaderMap_DeferredDeleteCopy[Platform];	// deleting null is Okay
		GGlobalShaderMap_DeferredDeleteCopy[Platform] = GGlobalShaderMap[Platform];
		GGlobalShaderMap[Platform] = nullptr;

		GGlobalShaderTargetPlatform[Platform] = TargetPlatform;

		// make sure we look for updated shader source files
		FlushShaderFileCache();
	}

	// If the global shader map hasn't been created yet, create it.
	if (!GGlobalShaderMap[Platform])
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("GetGlobalShaderMap"), STAT_GetGlobalShaderMap, STATGROUP_LoadTime);
		// GetGlobalShaderMap is called the first time during startup in the main thread.
		check(IsInGameThread());

		FScopedSlowTask SlowTask(70, LOCTEXT("CreateGlobalShaderMap", "Creating Global Shader Map..."));

		// verify that all shader source files are intact
		SlowTask.EnterProgressFrame(20, LOCTEXT("VerifyShaderSourceFiles", "Verifying Global Shader source files..."));
		VerifyShaderSourceFiles(Platform);

		GGlobalShaderMap[Platform] = new FGlobalShaderMap(Platform);

		bool bShaderMapIsBeingCompiled = false;

		// Try to load the global shaders from a local cache file if it exists
		// We always try this first, even when running in the editor or if shader compiler is enabled
		// It's always possible we'll find a cooked local cache
		const bool bLoadedFromCacheFile = TryLoadCookedGlobalShaderMap(Platform, SlowTask);
#if WITH_EDITOR
		const bool bAllowShaderCompiling = !FPlatformProperties::RequiresCookedData() && AllowShaderCompiling();
#else
		const bool bAllowShaderCompiling = false;
#endif

#if WITH_EDITOR
		if (!bLoadedFromCacheFile && bAllowShaderCompiling)
		{
			// If we didn't find cooked shaders, we can try loading from the DDC or compiling them if supported by the current configuration
			FGlobalShaderMapId ShaderMapId(Platform, TargetPlatform);

			const int32 ShaderFilenameNum = ShaderMapId.GetShaderFilenameToDependeciesMap().Num();
			const float ProgressStep = 25.0f / ShaderFilenameNum;

			// If NoShaderDDC then don't check for a material the first time we encounter it to simulate
			// a cold DDC
			static bool bNoShaderDDC =
				FParse::Param(FCommandLine::Get(), TEXT("noshaderddc")) || 
				FParse::Param(FCommandLine::Get(), TEXT("noglobalshaderddc"));
			if (UNLIKELY(bNoShaderDDC))
			{
				bShaderMapIsBeingCompiled = true;
			}
			else
			{
				using namespace UE::DerivedData;

				int32 BufferIndex = 0;
				TArray<FCacheGetValueRequest> Requests;

				// Submit DDC requests.
				SlowTask.EnterProgressFrame(ProgressStep, LOCTEXT("SubmitDDCRequests", "Submitting global shader DDC Requests..."));
				for (const auto& ShaderFilenameDependencies : ShaderMapId.GetShaderFilenameToDependeciesMap())
				{
					FCacheGetValueRequest& Request = Requests.AddDefaulted_GetRef();
					Request.Name = GetGlobalShaderMapName(ShaderMapId, Platform, ShaderFilenameDependencies.Key);
					Request.Key = GetGlobalShaderMapKey(ShaderMapId, Platform, TargetPlatform, ShaderFilenameDependencies.Value);
					Request.UserData = uint64(BufferIndex);
					++BufferIndex;

					if (UNLIKELY(ShouldDumpShaderDDCKeys()))
					{
						const FString ShaderName = ShaderFilenameDependencies.Key.Replace(TEXT("/"), TEXT(".")).Replace(TEXT(".usf"), TEXT(""));
						const FString FileName = FString::Printf(TEXT("GlobalShaderMap%s.txt"), *ShaderName);
						const FString DataKey = GetGlobalShaderMapKeyString(ShaderMapId, Platform, ShaderFilenameDependencies.Value);
						DumpShaderDDCKeyToFile(Platform, ShaderMapId.WithEditorOnly(), FileName, DataKey);
					}
				}

				int32 DDCHits = 0;
				int32 DDCMisses = 0;

				// Process finished DDC requests.
				SlowTask.EnterProgressFrame(ProgressStep, LOCTEXT("ProcessDDCRequests", "Processing global shader DDC requests..."));
				TArray<FValue> GlobalShaderMapBuffers;
				GlobalShaderMapBuffers.SetNum(Requests.Num());
				{
					COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());
					COOK_STAT(Timer.TrackCyclesOnly());
					FRequestOwner BlockingOwner(EPriority::Blocking);
					GetCache().GetValue(Requests, BlockingOwner, [&GlobalShaderMapBuffers](FCacheGetValueResponse&& Response)
					{
						GlobalShaderMapBuffers[int32(Response.UserData)] = MoveTemp(Response.Value);
					});
					BlockingOwner.Wait();
				}

				BufferIndex = 0;
				for (const auto& ShaderFilenameDependencies : ShaderMapId.GetShaderFilenameToDependeciesMap())
				{
					COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());
					if (GlobalShaderMapBuffers[BufferIndex].HasData())
					{
						COOK_STAT(Timer.AddHit(int64(GlobalShaderMapBuffers[BufferIndex].GetRawSize())));
						const FSharedBuffer CachedData = GlobalShaderMapBuffers[BufferIndex].GetData().Decompress();
						FMemoryReaderView MemoryReader(CachedData);
						GGlobalShaderMap[Platform]->AddSection(FGlobalShaderMapSection::CreateFromArchive(MemoryReader));
						DDCHits++;
					}
					else
					{
						// it's a miss, but we haven't built anything yet. Save the counting until we actually have it built.
						COOK_STAT(Timer.TrackCyclesOnly());
						bShaderMapIsBeingCompiled = true;
						DDCMisses++;
					}
					++BufferIndex;
				}

				GShaderCompilerStats->AddDDCHit(DDCHits);
				GShaderCompilerStats->AddDDCMiss(DDCMisses);
			}
		}
#endif // WITH_EDITOR
		
		if (!bLoadedFromCacheFile && !bAllowShaderCompiling)
		{
			// Failed to load cooked shaders, and no support for compiling
			// Handle this gracefully and exit.
			const FString GlobalShaderCacheFilename = FPaths::GetRelativePathToRoot() / GetGlobalShaderCacheFilename(Platform);
			const FString SandboxPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GlobalShaderCacheFilename);
			// This can be too early to localize in some situations.
			const FText Message = FText::Format(NSLOCTEXT("Engine", "GlobalShaderCacheFileMissing", "The global shader cache file '{0}' is missing.\n\nYour application is built to load COOKED content. No COOKED content was found; This usually means you did not cook content for this build.\nIt also may indicate missing cooked data for a shader platform(e.g., OpenGL under Windows): Make sure your platform's packaging settings include this Targeted RHI.\n\nAlternatively build and run the UNCOOKED version instead."), FText::FromString(SandboxPath));
			if (FPlatformProperties::SupportsWindowedMode())
			{
				UE_LOG(LogShaders, Error, TEXT("%s"), *Message.ToString());
				FMessageDialog::Open(EAppMsgType::Ok, Message);
				FPlatformMisc::RequestExit(false);
				return;
			}
			else
			{
				UE_LOG(LogShaders, Fatal, TEXT("%s"), *Message.ToString());
			}
		}

		// If any shaders weren't loaded, compile them now.
		VerifyGlobalShaders(Platform, TargetPlatform, bLoadedFromCacheFile);

		if (GCreateShadersOnLoad && Platform == GMaxRHIShaderPlatform)
		{
			GGlobalShaderMap[Platform]->BeginCreateAllShaders();
		}

		// While we're early in the game's startup, create certain global shaders that may be later created on random threads otherwise. 
		if (!bShaderMapIsBeingCompiled && !GRHISupportsMultithreadedShaderCreation)
		{
			CreateClearReplacementShaders();
		}
	}
}

void CompileGlobalShaderMap(EShaderPlatform Platform, bool bRefreshShaderMap)
{
	CompileGlobalShaderMap(Platform, nullptr, bRefreshShaderMap);
}

void CompileGlobalShaderMap(ERHIFeatureLevel::Type InFeatureLevel, bool bRefreshShaderMap)
{
	EShaderPlatform Platform = GShaderPlatformForFeatureLevel[InFeatureLevel];
	CompileGlobalShaderMap(Platform, nullptr, bRefreshShaderMap);
}

void CompileGlobalShaderMap(bool bRefreshShaderMap)
{
	CompileGlobalShaderMap(GMaxRHIFeatureLevel, bRefreshShaderMap);
}

void ReloadGlobalShaders()
{
	UE_LOG(LogShaders, Display, TEXT("Reloading global shaders..."));

	// Flush pending accesses to the existing global shaders.
	FlushRenderingCommands();

	UMaterialInterface::IterateOverActiveFeatureLevels(
		[&](ERHIFeatureLevel::Type InFeatureLevel)
		{
			auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
			GetGlobalShaderMap(ShaderPlatform)->ReleaseAllSections();
			CompileGlobalShaderMap(InFeatureLevel, true);
			VerifyGlobalShaders(ShaderPlatform, nullptr, false);
		}
	);

	// Invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
	for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
	{
		BeginUpdateResourceRHI(*It);
	}

	PropagateGlobalShadersToAllPrimitives();
}

static FAutoConsoleCommand CCmdReloadGlobalShaders = FAutoConsoleCommand(
	TEXT("ReloadGlobalShaders"),
	TEXT("Reloads the global shaders file"),
	FConsoleCommandDelegate::CreateStatic(ReloadGlobalShaders)
);

void SetGlobalShaderCacheOverrideDirectory(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogShaders, Error, TEXT("Failed to set GGlobalShaderCacheOverrideDirectory without any arguments"));
		return; 
	}
	
	GGlobalShaderCacheOverrideDirectory = Args[0];
	UE_LOG(LogShaders, Log, TEXT("GGlobalShaderCacheOverrideDirectory = %s"), *GGlobalShaderCacheOverrideDirectory);
}

static FAutoConsoleCommand CCmdSetGlobalShaderCacheOverrideDirectory = FAutoConsoleCommand(
	TEXT("SetGlobalShaderCacheOverrideDirectory"),
	TEXT("Set the directory to read the override global shader map file from."),
	FConsoleCommandWithArgsDelegate::CreateStatic(SetGlobalShaderCacheOverrideDirectory));

bool RecompileChangedShadersForPlatform(const FString& PlatformName)
{
	// figure out what shader platforms to recompile
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	ITargetPlatform* TargetPlatform = TPM->FindTargetPlatform(PlatformName);
	if (TargetPlatform == nullptr)
	{
		UE_LOG(LogShaders, Display, TEXT("Failed to find target platform module for %s"), *PlatformName);
		return false;
	}

	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	// figure out which shaders are out of date
	TArray<const FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;

	// Pick up new changes to shader files
	FlushShaderFileCache();

	GetOutdatedShaderTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
	UE_LOG(LogShaders, Display, TEXT("We found %d out of date shader types, %d outdated pipeline types, and %d out of date VF types!"), OutdatedShaderTypes.Num(), OutdatedShaderPipelineTypes.Num(), OutdatedFactoryTypes.Num());

	for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
	{
		// get the shader platform enum
		const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

		// Only compile for the desired platform if requested
		// Kick off global shader recompiles
		BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);

		// Block on global shaders
		FinishRecompileGlobalShaders();
#if WITH_EDITOR
		// we only want to actually compile mesh shaders if we have out of date ones
		if (OutdatedShaderTypes.Num() || OutdatedFactoryTypes.Num())
		{
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				(*It)->ClearCachedCookedPlatformData(TargetPlatform);
			}
		}
#endif
	}

	if (OutdatedFactoryTypes.Num() || OutdatedShaderTypes.Num())
	{
		return true;
	}
	return false;
}

FArchive& operator<<(FArchive& Ar, FODSCRequestPayload& Payload)
{
	int32 iShaderPlatform = static_cast<int32>(Payload.ShaderPlatform);
	int32 iFeatureLevel = static_cast<int32>(Payload.FeatureLevel);
	int32 iQualityLevel = static_cast<int32>(Payload.QualityLevel);

	Ar << iShaderPlatform;
	Ar << iFeatureLevel;
	Ar << iQualityLevel;
	Ar << Payload.MaterialName;
	Ar << Payload.VertexFactoryName;
	Ar << Payload.PipelineName;
	Ar << Payload.ShaderTypeNames;
	Ar << Payload.RequestHash;

	if (Ar.IsLoading())
	{
		Payload.ShaderPlatform = static_cast<EShaderPlatform>(iShaderPlatform);
		Payload.FeatureLevel = static_cast<ERHIFeatureLevel::Type>(iFeatureLevel);
		Payload.QualityLevel = static_cast<EMaterialQualityLevel::Type>(iQualityLevel);
	}

	return Ar;
}

FShaderRecompileData::FShaderRecompileData(const FString& InPlatformName, TArray<FString>* OutModifiedFiles, TArray<uint8>* OutMeshMaterialMaps, TArray<uint8>* OutGlobalShaderMap)
: PlatformName(InPlatformName),
  ModifiedFiles(OutModifiedFiles),
  MeshMaterialMaps(OutMeshMaterialMaps),
  GlobalShaderMap(OutGlobalShaderMap)
{
}

FShaderRecompileData::FShaderRecompileData(const FString& InPlatformName, EShaderPlatform InShaderPlatform, ODSCRecompileCommand InCommandType, TArray<FString>* OutModifiedFiles, TArray<uint8>* OutMeshMaterialMaps, TArray<uint8>* OutGlobalShaderMap)
: PlatformName(InPlatformName),
  ShaderPlatform(InShaderPlatform),
  ModifiedFiles(OutModifiedFiles),
  MeshMaterialMaps(OutMeshMaterialMaps),
  CommandType(InCommandType),
  GlobalShaderMap(OutGlobalShaderMap)
{
}

FArchive& operator<<(FArchive& Ar, FShaderRecompileData& RecompileData)
{

	int32 iShaderPlatform = static_cast<int32>(RecompileData.ShaderPlatform);
	int32 iFeatureLevel = static_cast<int32>(RecompileData.FeatureLevel);
	int32 iQualityLevel = static_cast<int32>(RecompileData.QualityLevel);

	Ar << RecompileData.MaterialsToLoad;
	Ar << RecompileData.ShaderTypesToLoad;
	Ar << iShaderPlatform;
	Ar << iFeatureLevel;
	Ar << iQualityLevel;
	Ar << RecompileData.CommandType;
	Ar << RecompileData.ShadersToRecompile;

	if (Ar.IsLoading())
	{
		RecompileData.ShaderPlatform = static_cast<EShaderPlatform>(iShaderPlatform);
		RecompileData.FeatureLevel = static_cast<ERHIFeatureLevel::Type>(iFeatureLevel);
		RecompileData.QualityLevel = static_cast<EMaterialQualityLevel::Type>(iQualityLevel);
	}

	return Ar;
}

extern ENGINE_API const TCHAR* ODSCCmdEnumToString(ODSCRecompileCommand Cmd)
{
	switch (Cmd)
	{
	case ODSCRecompileCommand::None:
		return TEXT("None");
	case ODSCRecompileCommand::Changed:
		return TEXT("Change");
	case ODSCRecompileCommand::Global:
		return TEXT("Global");
	case ODSCRecompileCommand::Material:
		return TEXT("Material");
	case ODSCRecompileCommand::SingleShader:
		return TEXT("SingleShader");
	}
	ensure(false);
	return TEXT("Unknown");
}

#if WITH_EDITOR

void CompileGlobalShaderMapForRemote(
	const TArray<const FShaderType*>& OutdatedShaderTypes, 
	const TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, 
	const EShaderPlatform ShaderPlatform, 
	const ITargetPlatform* TargetPlatform,
	TArray<uint8>* OutArray)
{
	UE_LOG(LogShaders, Display, TEXT("Recompiling global shaders."));

	// Kick off global shader recompiles
	BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform, TargetPlatform);

	// Block on global shaders
	FinishRecompileGlobalShaders();

	// Write the shader compilation info to memory, converting FName to strings
	FMemoryWriter MemWriter(*OutArray, true);
	FNameAsStringProxyArchive Ar(MemWriter);

	TOptional<FArchiveCookData> CookData;
	FArchiveCookContext CookContext(nullptr /*InPackage*/, FArchiveCookContext::ECookTypeUnknown);
	if (TargetPlatform != nullptr)
	{
		CookData.Emplace(*TargetPlatform, CookContext);
	}
	Ar.SetCookData(CookData.GetPtrOrNull());

	// save out the global shader map to the byte array
	SaveGlobalShadersForRemoteRecompile(Ar, ShaderPlatform);
}

void SaveShaderMapsForRemote(ITargetPlatform* TargetPlatform, const TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap>>>& CompiledShaderMaps, TArray<uint8>* OutArray)
{
	// write the shader compilation info to memory, converting fnames to strings
	FMemoryWriter MemWriter(*OutArray, true);
	FNameAsStringProxyArchive Ar(MemWriter);

	TOptional<FArchiveCookData> CookData;
	FArchiveCookContext CookContext(nullptr /*InPackage*/, FArchiveCookContext::ECookTypeUnknown);
	if (TargetPlatform != nullptr)
	{
		CookData.Emplace(*TargetPlatform, CookContext);
	}

	Ar.SetCookData(CookData.GetPtrOrNull());

	// save out the shader map to the byte array
	FMaterialShaderMap::SaveForRemoteRecompile(Ar, CompiledShaderMaps);
}

void RecompileShadersForRemote(
	FShaderRecompileData& Args,
	const FString& OutputDirectory)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RecompileShadersForRemote);

	// figure out what shader platforms to recompile
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	ITargetPlatform* TargetPlatform = TPM->FindTargetPlatform(Args.PlatformName);
	if (TargetPlatform == nullptr)
	{
		UE_LOG(LogShaders, Display, TEXT("Failed to find target platform module for %s"), *Args.PlatformName);
		return;
	}

	const double StartTime = FPlatformTime::Seconds();

	UE_LOG(LogShaders, Display, TEXT(""));
	UE_LOG(LogShaders, Display, TEXT("********************************"));
	UE_LOG(LogShaders, Display, TEXT("Received compile shader request %s."), ODSCCmdEnumToString(Args.CommandType));

	const bool bPreviousState = GShaderCompilingManager->IsShaderCompilationSkipped();
	GShaderCompilingManager->SkipShaderCompilation(false);

	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	UE_LOG(LogShaders, Verbose, TEXT("Loading %d materials..."), Args.MaterialsToLoad.Num());
	// make sure all materials the client has loaded will be processed
	TArray<UMaterialInterface*> MaterialsToCompile;

	for (int32 Index = 0; Index < Args.MaterialsToLoad.Num(); Index++)
	{
		UE_LOG(LogShaders, Verbose, TEXT("   --> %s"), *Args.MaterialsToLoad[Index]);
		MaterialsToCompile.Add(LoadObject<UMaterialInterface>(nullptr, *Args.MaterialsToLoad[Index]));
	}

	UE_LOG(LogShaders, Verbose, TEXT("  Done!"));

	const uint32 StartTotalShadersCompiled = GShaderCompilerStats->GetTotalShadersCompiled();

	// figure out which shaders are out of date
	TArray<const FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;

	// Pick up new changes to shader files
	FlushShaderFileCache();

	// If we have an explicit list of shaders to compile from ODSC just compile those.
	if (Args.ShadersToRecompile.Num() && (Args.MeshMaterialMaps != nullptr))
	{
		TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap>>> CompiledShaderMaps;
		UMaterial::CompileODSCMaterialsForRemoteRecompile(Args.ShadersToRecompile, CompiledShaderMaps);
		SaveShaderMapsForRemote(TargetPlatform, CompiledShaderMaps, Args.MeshMaterialMaps);
	}
	else
	{
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			// get the shader platform enum
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

			// Only compile for the desired platform if requested
			if (ShaderPlatform == Args.ShaderPlatform || Args.ShaderPlatform == SP_NumPlatforms)
			{
				if (Args.CommandType == ODSCRecompileCommand::SingleShader &&
					Args.ShaderTypesToLoad.Len() > 0)
				{
					TArray<const FShaderType*> ShaderTypes = FShaderType::GetShaderTypesByFilename(*Args.ShaderTypesToLoad);
					TArray<const FShaderPipelineType*> ShaderPipelineTypes = FShaderPipelineType::GetShaderPipelineTypesByFilename(*Args.ShaderTypesToLoad);

					for (const FShaderType* ShaderType : ShaderTypes)
					{
						UE_LOG(LogShaders, Display, TEXT("\t%s..."), ShaderType->GetName());
					}

					CompileGlobalShaderMapForRemote(ShaderTypes, ShaderPipelineTypes, ShaderPlatform, TargetPlatform, Args.GlobalShaderMap);
				}
				else if (Args.CommandType == ODSCRecompileCommand::Global ||
						 Args.CommandType == ODSCRecompileCommand::Changed)
				{
					// Explicitly get outdated types for global shaders.
					const FGlobalShaderMap* ShaderMap = GGlobalShaderMap[ShaderPlatform];
					if (ShaderMap)
					{
						ShaderMap->GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
					}

					UE_LOG(LogShaders, Display, TEXT("\tFound %d outdated shader types."), OutdatedShaderTypes.Num() + OutdatedShaderPipelineTypes.Num());

					CompileGlobalShaderMapForRemote(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform, TargetPlatform, Args.GlobalShaderMap);
				}

				// we only want to actually compile mesh shaders if a client directly requested it
				if ((Args.CommandType == ODSCRecompileCommand::Material || Args.CommandType == ODSCRecompileCommand::Changed) &&
					Args.MeshMaterialMaps != nullptr)
				{
					TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap>>> CompiledShaderMaps;
					UMaterial::CompileMaterialsForRemoteRecompile(MaterialsToCompile, ShaderPlatform, TargetPlatform, CompiledShaderMaps);
					SaveShaderMapsForRemote(TargetPlatform, CompiledShaderMaps, Args.MeshMaterialMaps);
				}

				// save it out so the client can get it (and it's up to date next time)
				FString GlobalShaderFilename = SaveGlobalShaderFile(ShaderPlatform, OutputDirectory, TargetPlatform);

				// add this to the list of files to tell the other end about
				if (Args.ModifiedFiles)
				{
					// need to put it in non-sandbox terms
					FString SandboxPath(GlobalShaderFilename);
					check(SandboxPath.StartsWith(OutputDirectory));
					SandboxPath.ReplaceInline(*OutputDirectory, TEXT("../../../"));
					FPaths::NormalizeFilename(SandboxPath);
					Args.ModifiedFiles->Add(SandboxPath);
				}
			}
		}
	}

	UE_LOG(LogShaders, Display, TEXT(""));
	UE_LOG(LogShaders, Display, TEXT("Compiled %u shaders in %.2f seconds."), GShaderCompilerStats->GetTotalShadersCompiled() - StartTotalShadersCompiled, FPlatformTime::Seconds() - StartTime);

	// Restore compilation state.
	GShaderCompilingManager->SkipShaderCompilation(bPreviousState);
}
#endif // WITH_EDITOR

void BeginRecompileGlobalShaders(const TArray<const FShaderType*>& OutdatedShaderTypes, const TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, EShaderPlatform ShaderPlatform, const ITargetPlatform* TargetPlatform)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		// Calling CompileGlobalShaderMap will force starting the compile jobs if the map is empty (by calling VerifyGlobalShaders)
		CompileGlobalShaderMap(ShaderPlatform, TargetPlatform, false);
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ShaderPlatform);

		// Now check if there is any work to be done wrt outdates types
		if (OutdatedShaderTypes.Num() > 0 || OutdatedShaderPipelineTypes.Num() > 0)
		{
			VerifyGlobalShaders(ShaderPlatform, TargetPlatform, false, &OutdatedShaderTypes, &OutdatedShaderPipelineTypes);
		}
	}
}

void FinishRecompileGlobalShaders()
{
	// Block until global shaders have been compiled and processed
	GShaderCompilingManager->ProcessAsyncResults(false, true);
}

static inline FShader* ProcessCompiledJob(FShaderCompileJob* SingleJob, const FShaderPipelineType* Pipeline, TArray<EShaderPlatform>& ShaderPlatformsProcessed, TArray<const FShaderPipelineType*>& OutSharedPipelines)
{
	const FGlobalShaderType* GlobalShaderType = SingleJob->Key.ShaderType->GetGlobalShaderType();
	check(GlobalShaderType);
	FShader* Shader = FGlobalShaderTypeCompiler::FinishCompileShader(GlobalShaderType, *SingleJob, Pipeline);
	if (Shader)
	{
		// Add the new global shader instance to the global shader map if it's a shared shader
		EShaderPlatform Platform = (EShaderPlatform)SingleJob->Input.Target.Platform;
		if (!Pipeline || !Pipeline->ShouldOptimizeUnusedOutputs(Platform))
		{
			Shader = GGlobalShaderMap[Platform]->FindOrAddShader(GlobalShaderType, SingleJob->Key.PermutationId, Shader);
			// Add this shared pipeline to the list
			if (!Pipeline)
			{
				auto* JobSharedPipelines = SingleJob->SharingPipelines.Find(nullptr);
				if (JobSharedPipelines)
				{
					for (auto* SharedPipeline : *JobSharedPipelines)
					{
						OutSharedPipelines.AddUnique(SharedPipeline);
					}
				}
			}
		}
		ShaderPlatformsProcessed.AddUnique(Platform);
	}
	else
	{
		UE_LOG(LogShaders, Fatal, TEXT("Failed to compile global shader %s %s %s.  Enable 'r.ShaderDevelopmentMode' in ConsoleVariables.ini for retries."),
			GlobalShaderType->GetName(),
			Pipeline ? TEXT("for pipeline") : TEXT(""),
			Pipeline ? Pipeline->GetName() : TEXT(""));
	}

	return Shader;
};

void ProcessCompiledGlobalShaders(const TArray<FShaderCommonCompileJobPtr>& CompilationResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessCompiledGlobalShaders);

	UE_LOG(LogShaders, Verbose, TEXT("Compiled %u global shaders"), CompilationResults.Num());

	TArray<EShaderPlatform> ShaderPlatformsProcessed;
	TArray<const FShaderPipelineType*> SharedPipelines;

	for (int32 ResultIndex = 0; ResultIndex < CompilationResults.Num(); ResultIndex++)
	{
		const FShaderCommonCompileJob& CurrentJob = *CompilationResults[ResultIndex];
		FShaderCompileJob* SingleJob = nullptr;
		if ((SingleJob = (FShaderCompileJob*)CurrentJob.GetSingleShaderJob()) != nullptr)
		{
			ProcessCompiledJob(SingleJob, nullptr, ShaderPlatformsProcessed, SharedPipelines);
		}
		else
		{
			const auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
			check(PipelineJob);

			FShaderPipeline* ShaderPipeline = new FShaderPipeline(PipelineJob->Key.ShaderPipeline);
			for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
			{
				SingleJob = PipelineJob->StageJobs[Index]->GetSingleShaderJob();
				FShader* Shader = ProcessCompiledJob(SingleJob, PipelineJob->Key.ShaderPipeline, ShaderPlatformsProcessed, SharedPipelines);
				ShaderPipeline->AddShader(Shader, SingleJob->Key.PermutationId);
			}
			ShaderPipeline->Validate(PipelineJob->Key.ShaderPipeline);

			EShaderPlatform Platform = (EShaderPlatform)PipelineJob->StageJobs[0]->GetSingleShaderJob()->Input.Target.Platform;
			check(ShaderPipeline && !GGlobalShaderMap[Platform]->HasShaderPipeline(PipelineJob->Key.ShaderPipeline));
			GGlobalShaderMap[Platform]->FindOrAddShaderPipeline(PipelineJob->Key.ShaderPipeline, ShaderPipeline);
		}
	}

	for (int32 PlatformIndex = 0; PlatformIndex < ShaderPlatformsProcessed.Num(); PlatformIndex++)
	{
		EShaderPlatform Platform = ShaderPlatformsProcessed[PlatformIndex];
		FGlobalShaderMap* GlobalShaderMap = GGlobalShaderMap[Platform];
		const ITargetPlatform* TargetPlatform = GGlobalShaderTargetPlatform[Platform];

		// Process the shader pipelines that share shaders
		FPlatformTypeLayoutParameters LayoutParams;
		LayoutParams.InitializeForPlatform(TargetPlatform);
		const EShaderPermutationFlags PermutationFlags = GetShaderPermutationFlags(LayoutParams);

		for (const FShaderPipelineType* ShaderPipelineType : SharedPipelines)
		{
			check(ShaderPipelineType->IsGlobalTypePipeline());
			if (!GlobalShaderMap->HasShaderPipeline(ShaderPipelineType))
			{
				auto& StageTypes = ShaderPipelineType->GetStages();

				FShaderPipeline* ShaderPipeline = new FShaderPipeline(ShaderPipelineType);
				for (int32 Index = 0; Index < StageTypes.Num(); ++Index)
				{
					FGlobalShaderType* GlobalShaderType = ((FShaderType*)(StageTypes[Index]))->GetGlobalShaderType();
					if (GlobalShaderType->ShouldCompilePermutation(Platform, kUniqueShaderPermutationId, PermutationFlags))
					{
						TShaderRef<FShader> Shader = GlobalShaderMap->GetShader(GlobalShaderType, kUniqueShaderPermutationId);
						check(Shader.IsValid());
						ShaderPipeline->AddShader(Shader.GetShader(), kUniqueShaderPermutationId);
					}
					else
					{
						break;
					}
				}
				ShaderPipeline->Validate(ShaderPipelineType);
				GlobalShaderMap->FindOrAddShaderPipeline(ShaderPipelineType, ShaderPipeline);
			}
		}

		// at this point the new global sm is populated and we can delete the deferred copy, if any
		delete GGlobalShaderMap_DeferredDeleteCopy[ShaderPlatformsProcessed[PlatformIndex]];	// even if it was nullptr, deleting null is Okay
		GGlobalShaderMap_DeferredDeleteCopy[ShaderPlatformsProcessed[PlatformIndex]] = nullptr;

		// Save the global shader map for any platforms that were recompiled, but only if it is complete (it can be also a subject to ODSC, perhaps unnecessarily, as we cannot use a partial global SM)
		FGlobalShaderMapId ShaderMapId(Platform, TargetPlatform);
		if (GlobalShaderMap->IsComplete(TargetPlatform))
		{
			SaveGlobalShaderMapToDerivedDataCache(ShaderPlatformsProcessed[PlatformIndex]);

			if (!GRHISupportsMultithreadedShaderCreation && Platform == GMaxRHIShaderPlatform)
			{
				CreateClearReplacementShaders();
			}
		}
	}
}

void SaveGlobalShadersForRemoteRecompile(FArchive& Ar, EShaderPlatform ShaderPlatform)
{
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ShaderPlatform);
	uint8 bIsValid = GlobalShaderMap != nullptr;
	Ar << bIsValid;

	if (GlobalShaderMap)
	{
		GlobalShaderMap->SaveToGlobalArchive(Ar);
	}
}

void LoadGlobalShadersForRemoteRecompile(FArchive& Ar, EShaderPlatform ShaderPlatform)
{
	uint8 bIsValid = 0;
	Ar << bIsValid;

	if (bIsValid)
	{
		FlushRenderingCommands();

		FGlobalShaderMap* NewGlobalShaderMap = new FGlobalShaderMap(ShaderPlatform);
		if (NewGlobalShaderMap)
		{
			NewGlobalShaderMap->LoadFromGlobalArchive(Ar);

			if (GGlobalShaderMap[ShaderPlatform])
			{
				GGlobalShaderMap[ShaderPlatform]->ReleaseAllSections();

				delete GGlobalShaderMap[ShaderPlatform];
				GGlobalShaderMap[ShaderPlatform] = nullptr;
				GGlobalShaderMap[ShaderPlatform] = NewGlobalShaderMap;

				VerifyGlobalShaders(ShaderPlatform, nullptr, false);

				// Invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
				for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
				{
					BeginUpdateResourceRHI(*It);
				}

				PropagateGlobalShadersToAllPrimitives();
			}
			else
			{
				delete NewGlobalShaderMap;
			}
		}
	}
}

FShaderCommonCompileJob::FInputHash FShaderCompileJob::GetInputHash()
{
	if (bInputHashSet)
	{
		return InputHash;
	}

	auto SerializeInputs = [this](FArchive& Archive)
	{
		checkf(Archive.IsSaving() && !Archive.IsLoading(), TEXT("A loading archive is passed to FShaderCompileJob::GetInputHash(), this is not supported as it may corrupt its data"));

		Archive << Input;
		Input.Environment.SerializeEverythingButFiles(Archive);

		// hash the source file so changes to files during the development are picked up
		const FSHAHash& SourceHash = GetShaderFileHash(*Input.VirtualSourceFilePath, Input.Target.GetPlatform());
		Archive << const_cast<FSHAHash&>(SourceHash);

		// unroll the included files for the parallel processing.
		// These are temporary arrays that only exist for the ParallelFor
		TArray<const TCHAR*> IncludeVirtualPaths;
		TArray<const FString*> Contents;
		TArray<bool> OnlyHashIncludes;
		TArray<FBlake3Hash> Hashes;

		// while the contents of this is already hashed (included in Environment's operator<<()), we still need to account for includes in the generated files and hash them, too
		for (TMap<FString, FString>::TConstIterator It(Input.Environment.IncludeVirtualPathToContentsMap); It; ++It)
		{
			const FString& VirtualPath = It.Key();
			IncludeVirtualPaths.Add(*VirtualPath);
			Contents.Add(&It.Value());
			OnlyHashIncludes.Add(true);	// not hashing contents of the file itself, as it was included in Environment's operator<<()
			Hashes.AddDefaulted();
		}

		for (TMap<FString, FThreadSafeSharedStringPtr>::TConstIterator It(Input.Environment.IncludeVirtualPathToExternalContentsMap); It; ++It)
		{
			const FString& VirtualPath = It.Key();
			IncludeVirtualPaths.Add(*VirtualPath);
			check(It.Value());
			Contents.Add(&(*It.Value()));
			OnlyHashIncludes.Add(false);
			Hashes.AddDefaulted();
		}

		if (Input.SharedEnvironment)
		{
			Input.SharedEnvironment->SerializeEverythingButFiles(Archive);

			for (TMap<FString, FString>::TConstIterator It(Input.SharedEnvironment->IncludeVirtualPathToContentsMap); It; ++It)
			{
				const FString& VirtualPath = It.Key();
				IncludeVirtualPaths.Add(*VirtualPath);
				Contents.Add(&It.Value());
				OnlyHashIncludes.Add(true);	// not hashing contents of the file itself, as it was included in Environment's operator<<()
				Hashes.AddDefaulted();
			}

			for (TMap<FString, FThreadSafeSharedStringPtr>::TConstIterator It(Input.SharedEnvironment->IncludeVirtualPathToExternalContentsMap); It; ++It)
			{
				const FString& VirtualPath = It.Key();
				IncludeVirtualPaths.Add(*VirtualPath);
				check(It.Value());
				Contents.Add(&(*It.Value()));
				OnlyHashIncludes.Add(false);
				Hashes.AddDefaulted();
			}
		}

		check(IncludeVirtualPaths.Num() == Contents.Num());
		check(Contents.Num() == OnlyHashIncludes.Num());
		check(OnlyHashIncludes.Num() == Hashes.Num());

		EShaderPlatform Platform = Input.Target.GetPlatform();
		ParallelFor(Contents.Num(), [&IncludeVirtualPaths, &Contents, &OnlyHashIncludes, &Hashes, &Platform](int32 FileIndex)
			{ 
				FMemoryHasherBlake3 MemHasher;
				HashShaderFileWithIncludes(MemHasher, IncludeVirtualPaths[FileIndex], *Contents[FileIndex], Platform, OnlyHashIncludes[FileIndex]);
				Hashes[FileIndex] = MemHasher.Finalize();
			},
			EParallelForFlags::Unbalanced
		);

		// include the hashes in the main hash (consider sorting them if includes are found to have a random order)
		for (int32 HashIndex = 0, NumHashes = Hashes.Num(); HashIndex < NumHashes; ++HashIndex)
		{
			Archive << Hashes[HashIndex];
		}
	};

	// use faster hasher that doesn't allocate memory
	FMemoryHasherBlake3 MemHasher;
	SerializeInputs(MemHasher);
	InputHash = MemHasher.Finalize();

	if (GShaderCompilerDumpCompileJobInputs)
	{
		TArray<uint8> MemoryBlob;
		FMemoryWriter MemWriter(MemoryBlob);

		SerializeInputs(MemWriter);

		FString IntermediateFormatPath = FPaths::ProjectSavedDir() / TEXT("ShaderJobInputs");
#if UE_BUILD_DEBUG
		FString TempPath = IntermediateFormatPath / TEXT("DebugEditor");
#else
		FString TempPath = IntermediateFormatPath / TEXT("DevelopmentEditor");
#endif
		IFileManager::Get().MakeDirectory(*TempPath, true);

		static int32 InputHashID = 0;
		FString FileName = Input.DebugGroupName.Replace(TEXT("/"), TEXT("_")).Replace(TEXT("<"), TEXT("_")).Replace(TEXT(">"), TEXT("_")).Replace(TEXT(":"), TEXT("_")).Replace(TEXT("|"), TEXT("_"))
			+ TEXT("-") + Input.EntryPointName;
		FString TempFile = TempPath / FString::Printf(TEXT("%s-%d.bin"), *FileName, InputHashID++);

		TUniquePtr<FArchive> DumpAr(IFileManager::Get().CreateFileWriter(*TempFile));
		DumpAr->Serialize(MemoryBlob.GetData(), MemoryBlob.Num());

		// as an additional debugging feature, make sure that the hash is the same as calculated by the memhasher
		FBlake3Hash Check = FBlake3::HashBuffer(MemoryBlob.GetData(), MemoryBlob.Num());
		if (Check != InputHash)
		{
			UE_LOG(LogShaderCompilers, Error, TEXT("Job input hash disagrees between FMemoryHasherSHA1 (%s) and FMemoryWriter + FSHA1 (%s, which was dumped to disk)"), *LexToString(InputHash), *LexToString(Check));
		}
	}

	bInputHashSet = true;
	return InputHash;
}

void FShaderCompileJob::SerializeOutput(FArchive& Ar)
{
	double ActualCompileTime = 0.0;
	double ActualPreprocessTime = 0.0;
	if (Ar.IsSaving())
	{
		// Cached jobs won't have accurate results anyway, so reduce the storage requirements by setting those fields to a known value.
		// This significantly reduces the memory needed to store the outputs (by more than a half)
		ActualCompileTime = Output.CompileTime;
		ActualPreprocessTime = Output.PreprocessTime;
		Output.CompileTime = 0.0;
		Output.PreprocessTime = 0.0;
	}

	Ar << Output;
	// output hash is now serialized as part of the output, as the shader code is compressed in SCWs
	checkf(!Output.bSucceeded || Output.OutputHash != FSHAHash(), TEXT("Successful compile job does not have an OutputHash generated."));

	if (Ar.IsLoading())
	{
		bFinalized = true;
		bSucceeded = Output.bSucceeded;
	}
	else
	{
		// restore the compile time for this jobs. Jobs that will be deserialized from the cache will have a compile time of 0.0
		Output.CompileTime = ActualCompileTime;
		Output.PreprocessTime = ActualPreprocessTime;
	}
}

FShaderCommonCompileJob::FInputHash FShaderPipelineCompileJob::GetInputHash()
{
	if (bInputHashSet)
	{
		return InputHash;
	}

	FBlake3 Hasher;
	for (int32 Index = 0; Index < StageJobs.Num(); ++Index)
	{
		if (StageJobs[Index])
		{
			FShaderCommonCompileJob::FInputHash StageHash = StageJobs[Index]->GetInputHash();
			Hasher.Update(StageHash.GetBytes(), sizeof(decltype(StageHash.GetBytes())));
		}
	}

	InputHash = Hasher.Finalize();

	bInputHashSet = true;
	return InputHash;
}

void FShaderPipelineCompileJob::SerializeOutput(FArchive& Ar)
{
	bool bAllStagesSucceeded = true;
	for (int32 Index = 0, Num = StageJobs.Num(); Index < Num; ++Index)
	{
		StageJobs[Index]->SerializeOutput(Ar);
		bAllStagesSucceeded = bAllStagesSucceeded && StageJobs[Index]->bSucceeded;
	}

	if (Ar.IsLoading())
	{
		bFinalized = true;
		bSucceeded = bAllStagesSucceeded;
	}
}

TRACE_DECLARE_INT_COUNTER(Shaders_JobCacheSearchAttempts, TEXT("Shaders/JobCache/SearchAttempts"));
TRACE_DECLARE_INT_COUNTER(Shaders_JobCacheHits, TEXT("Shaders/JobCache/Hits"));

TRACE_DECLARE_INT_COUNTER(Shaders_JobCacheDDCRequests, TEXT("Shaders/JobCache/DDCRequests"));
TRACE_DECLARE_INT_COUNTER(Shaders_JobCacheDDCHits, TEXT("Shaders/JobCache/DDCHits"));
TRACE_DECLARE_MEMORY_COUNTER(Shaders_JobCacheDDCBytesReceived, TEXT("Shaders/JobCache/DDCBytesRecieved"));
TRACE_DECLARE_MEMORY_COUNTER(Shaders_JobCacheDDCBytesSent, TEXT("Shaders/JobCache/DDCBytesSent"));

#if WITH_EDITOR
namespace
{
	/** The FCacheBucket used with the DDC, cached to avoid recreating it for each request */
	UE::DerivedData::FCacheBucket ShaderJobCacheDDCBucket = UE::DerivedData::FCacheBucket(TEXT("FShaderJobCacheShaders"));
	UE::DerivedData::FValueId ShaderJobCacheId = UE::DerivedData::FValueId::FromName("FShaderJobCacheShaderID");
}
#endif

struct FShaderJobCacheStoredOutput
{
private:
	/** How many times this output is referenced by the cached jobs */
	int32 NumReferences = 0;

public:

	/** How many times this output has been returned as a cached result, no matter the input hash */
	int32 NumHits = 0;

	/** Canned output */
	TArray<uint8> JobOutput;

	/** Similar to FRefCountBase AddRef, but not atomic */
	int32 AddRef()
	{
		++NumReferences;

		return NumReferences;
	}

	/** Similar to FRefCountBase Release, but not atomic */
	int32 Release()
	{
		checkf(NumReferences >= 0, TEXT("Attempting to release shader job cache output that was already released"));

		--NumReferences;

		const int32 RemainingNumReferences = NumReferences;

		if (RemainingNumReferences == 0)
		{
			delete this;
		}

		return RemainingNumReferences;
	}

	uint64 GetAllocatedSize() const
	{
		return static_cast<uint64>(JobOutput.GetAllocatedSize() + sizeof(*this));
	}
};

FShaderJobCache::FJobCachedOutput* FShaderJobCache::Find(const FJobInputHash& Hash, const bool bCheckDDC)
{
	++TotalSearchAttempts;
	TRACE_COUNTER_INCREMENT(Shaders_JobCacheSearchAttempts);
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderJobCache::Find);

	if (ShaderCompiler::IsJobCacheEnabled())
	{
		FJobOutputHash* OutputHash = InputHashToOutput.Find(Hash);
		if (OutputHash)
		{
			++TotalCacheHits;
			TRACE_COUNTER_INCREMENT(Shaders_JobCacheHits);

			FStoredOutput** CannedOutput = Outputs.Find(*OutputHash);
			// we should not allow a dangling input to output mapping to exist
			checkf(CannedOutput != nullptr, TEXT("Inconsistency in FShaderJobCache - cache record for ihash %s exists, but output cannot be found."), *LexToString(Hash));
			// update the output hit count
			(*CannedOutput)->NumHits++;
			return &(*CannedOutput)->JobOutput;
		}
#if WITH_EDITOR
		else
		{
			// If we didn't find it in memory search the DDC if it's enabled.
			const bool bCachePerShaderDDC = IsShaderJobCacheDDCEnabled() && bCheckDDC;
			if (bCachePerShaderDDC)
			{
				FSharedBuffer Results;

				TRACE_COUNTER_INCREMENT(Shaders_JobCacheDDCRequests);

				UE::DerivedData::FRequestOwner RequestOwner(UE::DerivedData::EPriority::Blocking);

				UE::DerivedData::FCacheGetRequest Request;
				Request.Name = TEXT("FShaderJobCache");
				// Create key.
				Request.Key.Bucket = ShaderJobCacheDDCBucket;
				Request.Key.Hash = Hash;
				Request.Policy = IsShaderJobCacheDDCRemotePolicyEnabled() ? UE::DerivedData::ECachePolicy::Default : UE::DerivedData::ECachePolicy::Local;

				UE::DerivedData::GetCache().Get(
					{ Request },
					RequestOwner,
					[&Results](UE::DerivedData::FCacheGetResponse&& Response)
					{
						switch (Response.Status)
						{
							case UE::DerivedData::EStatus::Ok:
							{
								Results = Response.Record.GetValue(ShaderJobCacheId).GetData().Decompress();

								TRACE_COUNTER_INCREMENT(Shaders_JobCacheDDCHits);

								break;
							}
							case UE::DerivedData::EStatus::Error:
							{
								break;
							}
							case UE::DerivedData::EStatus::Canceled:
							{
								break;
							}
						}
					});

				RequestOwner.Wait();
				if (!Results.IsNull())
				{
					// Create a new entry to store in the FShaderJobCache
					FStoredOutput* NewStoredOutput = new FStoredOutput();
					NewStoredOutput->JobOutput.Reserve(Results.GetSize());
					NewStoredOutput->JobOutput.SetNum(Results.GetSize());

					TRACE_COUNTER_ADD(Shaders_JobCacheDDCBytesReceived, Results.GetSize());

					check(NewStoredOutput->JobOutput.GetAllocatedSize() == Results.GetSize());
					FMemory::Memcpy(NewStoredOutput->JobOutput.GetData(),  Results.GetData(), Results.GetSize());

					// Generate an output hash and cache the result in the FShaderJobCache
					FJobOutputHash NewOutputHash = FBlake3::HashBuffer(NewStoredOutput->JobOutput.GetData(), NewStoredOutput->JobOutput.Num());

					NewStoredOutput->AddRef();
					Outputs.Add(NewOutputHash, NewStoredOutput);
					InputHashToOutput.Add(Hash, NewOutputHash);
					EvictionQueue.PushLast(Hash);

					CurrentlyAllocatedMemory += NewStoredOutput->GetAllocatedSize();

					// return the results.
					return &NewStoredOutput->JobOutput;
				}
			}
		}
#endif
	}

	return nullptr;
}

uint64 FShaderJobCache::GetCurrentMemoryBudget() const
{
	uint64 AbsoluteLimit = static_cast<uint64>(GShaderCompilerMaxJobCacheMemoryMB) * 1024ULL * 1024ULL;
	uint64 RelativeLimit = FMath::Clamp(static_cast<double>(GShaderCompilerMaxJobCacheMemoryPercent), 0.0, 100.0) * (static_cast<double>(FPlatformMemory::GetPhysicalGBRam()) * 1024 * 1024 * 1024) / 100.0;
	return FMath::Min(AbsoluteLimit, RelativeLimit);
}

FShaderJobCache::FShaderJobCache()
{
	CurrentlyAllocatedMemory = sizeof(*this) + InputHashToOutput.GetAllocatedSize() + Outputs.GetAllocatedSize();
}

FShaderJobCache::~FShaderJobCache()
{
	for (TMap<FJobOutputHash, FStoredOutput*>::TIterator Iter(Outputs); Iter; ++Iter)
	{
		delete Iter.Value();
	}
}

void FShaderJobCache::Add(const FJobInputHash& Hash, const FJobCachedOutput& Contents, int32 InitialHitCount, const bool bAddToDDC)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShaderJobCache::Add);

	if (!ShaderCompiler::IsJobCacheEnabled())
	{
		return;
	}

	FJobOutputHash* ExistingOutputHash = InputHashToOutput.Find(Hash);
	if (ExistingOutputHash)
	{
		return;
	}

	FJobOutputHash OutputHash = FBlake3::HashBuffer(Contents.GetData(), Contents.Num());

	// add the record
	const uint64 InputHashToOutputOriginalSize = InputHashToOutput.GetAllocatedSize();

	InputHashToOutput.Add(Hash, OutputHash);
	EvictionQueue.PushLast(Hash);

	const bool bCachePerShaderDDC = IsShaderJobCacheDDCEnabled() && bAddToDDC;

	FStoredOutput** CannedOutput = Outputs.Find(OutputHash);
	if (CannedOutput && (bCachePerShaderDDC == false))
	{
		// update the output hit count
		(*CannedOutput)->AddRef();
	}
	else
	{
		const uint64 OutputsOriginalSize = Outputs.GetAllocatedSize();

		FStoredOutput* NewStoredOutput = new FStoredOutput();
		NewStoredOutput->NumHits = InitialHitCount;
		NewStoredOutput->JobOutput = Contents;

		NewStoredOutput->AddRef();
		Outputs.Add(OutputHash, NewStoredOutput);

		CurrentlyAllocatedMemory += NewStoredOutput->GetAllocatedSize();

#if WITH_EDITOR
		if (bCachePerShaderDDC)
		{
			// Create key.
			UE::DerivedData::FCacheKey Key;
			Key.Bucket = ShaderJobCacheDDCBucket;
			Key.Hash = Hash;
			UE::DerivedData::FCacheRecordBuilder RecordBuilder(Key);

			RecordBuilder.AddValue(ShaderJobCacheId, FSharedBuffer::MakeView(MakeMemoryView(Contents)));

			TRACE_COUNTER_ADD(Shaders_JobCacheDDCBytesSent, Contents.GetAllocatedSize());

			UE::DerivedData::FRequestOwner RequestOwner(UE::DerivedData::EPriority::Normal);
			UE::DerivedData::FRequestBarrier RequestBarrier(RequestOwner);
			RequestOwner.KeepAlive();
			UE::DerivedData::GetCache().Put(
				{ {{TEXT("FShaderJobCache")}, RecordBuilder.Build(), IsShaderJobCacheDDCRemotePolicyEnabled() ? UE::DerivedData::ECachePolicy::Default : UE::DerivedData::ECachePolicy::Local } },
				RequestOwner
			);
		}
#endif

		// delete the previous cache entries if we have a budget
		const uint64 MemoryBudgetBytes = GetCurrentMemoryBudget();
		if (MemoryBudgetBytes)
		{
			if (CurrentlyAllocatedMemory > MemoryBudgetBytes)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FShaderJobCache::Trim);
			
				FJobInputHash KeyToRemove = {};
				while (CurrentlyAllocatedMemory > MemoryBudgetBytes && EvictionQueue.TryPopFirst(KeyToRemove))
				{
					RemoveByInputHash(KeyToRemove);
				}
			}
		}

		CurrentlyAllocatedMemory += Outputs.GetAllocatedSize();
		CurrentlyAllocatedMemory -= OutputsOriginalSize;
	}

	CurrentlyAllocatedMemory += InputHashToOutput.GetAllocatedSize();
	CurrentlyAllocatedMemory -= InputHashToOutputOriginalSize;
}

void FShaderJobCache::RemoveByInputHash(const FJobInputHash& InputHash)
{
	const FJobOutputHash* FoundOutputHash = InputHashToOutput.Find(InputHash);

	if (FoundOutputHash)
	{
		const FJobOutputHash& OutputHash = *FoundOutputHash;
		FStoredOutput** FoundStoredOutput = Outputs.Find(OutputHash);

		if (FoundStoredOutput)
		{
			FStoredOutput* StoredOutput = *FoundStoredOutput;
			checkf(StoredOutput, TEXT("Invalid entry found in FShaderJobCache Output hash table. All values are expected to be valid pointers."));

			const uint64 OutputSize = StoredOutput->GetAllocatedSize();

			// Decrement reference count and remove cached object if it's no longer referenced by any input hashes
			if (StoredOutput->Release() == 0)
			{
				Outputs.Remove(OutputHash);
				CurrentlyAllocatedMemory -= OutputSize;
			}
		}

		InputHashToOutput.Remove(InputHash);
	}
}

/** Calculates memory used by the cache*/
uint64 FShaderJobCache::GetAllocatedMemory()
{
	return CurrentlyAllocatedMemory;
}

#include "Math/UnitConversion.h"

/** Logs out the statistics */
void FShaderJobCache::LogStats()
{
	UE_LOG(LogShaderCompilers, Display, TEXT("=== FShaderJobCache stats ==="), this);
	UE_LOG(LogShaderCompilers, Display, TEXT("Total job queries %lld, among them cache hits %lld (%.2f%%)"),
		TotalSearchAttempts, TotalCacheHits, (TotalSearchAttempts > 0) ? 100.0 * static_cast<double>(TotalCacheHits) / static_cast<double>(TotalSearchAttempts) : 0.0);
	UE_LOG(LogShaderCompilers, Display, TEXT("Tracking %d distinct input hashes that result in %d distinct outputs (%.2f%%)"),
		InputHashToOutput.Num(), Outputs.Num(), (InputHashToOutput.Num() > 0) ? 100.0 * static_cast<double>(Outputs.Num()) / static_cast<double>(InputHashToOutput.Num()) : 0.0);

	uint64 MemUsed = GetAllocatedMemory();
	double MemUsedMB = FUnitConversion::Convert(static_cast<double>(MemUsed), EUnit::Bytes, EUnit::Megabytes);
	double MemUsedGB = FUnitConversion::Convert(static_cast<double>(MemUsed), EUnit::Bytes, EUnit::Gigabytes);
	uint64 MemBudget = GetCurrentMemoryBudget();
	if (MemBudget > 0)
	{
		double MemBudgetMB = FUnitConversion::Convert(static_cast<double>(MemBudget), EUnit::Bytes, EUnit::Megabytes);
		double MemBudgetGB = FUnitConversion::Convert(static_cast<double>(MemBudget), EUnit::Bytes, EUnit::Gigabytes);

		UE_LOG(LogShaderCompilers, Display, TEXT("RAM used: %.2f MB (%.2f GB) of %.2f MB (%.2f GB) budget. Usage: %.2f%%"), 
			MemUsedMB, MemUsedGB, MemBudgetMB, MemBudgetGB, 100.0 * MemUsedMB / MemBudgetMB);
	}
	else
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("RAM used: %.2f MB (%.2f GB), no memory limit set"), MemUsedMB, MemUsedGB);
	}
}

#undef LOCTEXT_NAMESPACE
