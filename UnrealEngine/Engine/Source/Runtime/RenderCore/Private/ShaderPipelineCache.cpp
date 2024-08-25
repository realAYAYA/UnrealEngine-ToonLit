// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Shader Pipeline Precompilation Cache
 * Precompilation half of the shader pipeline cache, which builds on the runtime RHI pipeline cache.
 */
 
#include "ShaderPipelineCache.h"
#include "Containers/List.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Serialization/MemoryReader.h"
#include "RHICommandList.h"
#include "RenderUtils.h"
#include "Misc/EngineVersion.h"
#include "PipelineStateCache.h"
#include "PipelineFileCache.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreDelegates.h"
#include "ShaderCodeLibrary.h"
#include "TickableObjectRenderThread.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/AsyncFileHandle.h"
#include "Misc/ScopeLock.h"
#include <Algo/RemoveIf.h>
#include "Modules/BuildVersion.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Outstanding Tasks"), STAT_ShaderPipelineTaskCount, STATGROUP_PipelineStateCache );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Waiting Tasks"), STAT_ShaderPipelineWaitingTaskCount, STATGROUP_PipelineStateCache );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Active Tasks"), STAT_ShaderPipelineActiveTaskCount, STATGROUP_PipelineStateCache );
DECLARE_MEMORY_STAT(TEXT("Pre-Compile Memory"), STAT_PreCompileMemory, STATGROUP_PipelineStateCache);
DECLARE_CYCLE_STAT(TEXT("Pre-Compile Time"),STAT_PreCompileTime,STATGROUP_PipelineStateCache);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Total Pre-Compile Time"),STAT_PreCompileTotalTime,STATGROUP_PipelineStateCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Pipelines Pre-Compiled"), STAT_PreCompileShadersTotal, STATGROUP_PipelineStateCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("# Pipelines Pre-Compiled"), STAT_PreCompileShadersNum, STATGROUP_PipelineStateCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Batches Pre-Compiled"), STAT_PreCompileBatchTotal, STATGROUP_PipelineStateCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("# Batches Pre-Compiled"), STAT_PreCompileBatchNum, STATGROUP_PipelineStateCache);

namespace FShaderPipelineCacheConstants
{
	static TCHAR const* SectionHeading = TEXT("ShaderPipelineCache.CacheFile");
	static TCHAR const* CacheFileNameOverride = TEXT("CacheFileNameOverride");
	static TCHAR const* UserCacheFileNameOverride = TEXT("UserCacheFileNameOverride");
	static TCHAR const* SortOrderKey = TEXT("SortOrder");
	static TCHAR const* GameVersionKey = TEXT("GameVersion");
}


static TAutoConsoleVariable<int32> CVarPSOFileCacheStartupMode(
														  TEXT("r.ShaderPipelineCache.StartupMode"),
														  1,
														  TEXT("Sets the startup mode for the PSO cache, determining what the cache does after initialisation:\n")
														  TEXT("\t0: Precompilation is paused and nothing will compile until a call to ResumeBatching().\n")
														  TEXT("\t1: Precompilation is enabled in the 'Fast' mode.\n")
														  TEXT("\t2: Precompilation is enabled in the 'Background' mode.\n")
														  TEXT("Default is 1."),
														  ECVF_Default | ECVF_RenderThreadSafe
														  );

static TAutoConsoleVariable<int32> CVarPSOFileCacheBackgroundBatchSize(
														  TEXT("r.ShaderPipelineCache.BackgroundBatchSize"),
														  1,
														  TEXT("Set the number of PipelineStateObjects to compile in a single batch operation when compiling in the background. Defaults to a maximum of 1 per frame, due to async. file IO it is less in practice."),
														  ECVF_Default | ECVF_RenderThreadSafe
														  );
static TAutoConsoleVariable<int32> CVarPSOFileCacheBatchSize(
														   TEXT("r.ShaderPipelineCache.BatchSize"),
														   50,
														   TEXT("Set the number of PipelineStateObjects to compile in a single batch operation when compiling takes priority. Defaults to a maximum of 50 per frame, due to async. file IO it is less in practice."),
														   ECVF_Default | ECVF_RenderThreadSafe
														   );
static TAutoConsoleVariable<int32> CVarPSOFileCachePrecompileBatchSize(
															 TEXT("r.ShaderPipelineCache.PrecompileBatchSize"),
															 50,
															 TEXT("Set the number of PipelineStateObjects to compile in a single batch operation when pre-optimizing the cache. Defaults to a maximum of 50 per frame, due to async. file IO it is less in practice."),
															 ECVF_Default | ECVF_RenderThreadSafe
															 );
static TAutoConsoleVariable<float> CVarPSOFileCacheBackgroundBatchTime(
														  TEXT("r.ShaderPipelineCache.BackgroundBatchTime"),
														  0.0f,
														  TEXT("The target time (in ms) to spend precompiling each frame when in the background or 0.0 to disable. When precompiling is faster the batch size will grow and when slower will shrink to attempt to occupy the full amount. Defaults to 0.0 (off)."),
														  ECVF_Default | ECVF_RenderThreadSafe
														  );
static TAutoConsoleVariable<float> CVarPSOFileCacheBatchTime(
														   TEXT("r.ShaderPipelineCache.BatchTime"),
														   16.0f,
														   TEXT("The target time (in ms) to spend precompiling each frame when compiling takes priority or 0.0 to disable. When precompiling is faster the batch size will grow and when slower will shrink to attempt to occupy the full amount. Defaults to 16.0 (max. ms per-frame of precompilation)."),
														   ECVF_Default | ECVF_RenderThreadSafe
														   );
static TAutoConsoleVariable<float> CVarPSOFileCachePrecompileBatchTime(
															 TEXT("r.ShaderPipelineCache.PrecompileBatchTime"),
															 0.0f,
															 TEXT("The target time (in ms) to spend precompiling each frame when cpre-optimizing or 0.0 to disable. When precompiling is faster the batch size will grow and when slower will shrink to attempt to occupy the full amount. Defaults to 10.0 (off)."),
															 ECVF_Default | ECVF_RenderThreadSafe
															 );
static TAutoConsoleVariable<int32> CVarPSOFileCacheSaveAfterPSOsLogged(
														   TEXT("r.ShaderPipelineCache.SaveAfterPSOsLogged"),
														   0,
														   TEXT("Set the number of PipelineStateObjects to log before automatically saving. 0 will disable automatic saving (which is the default now, as automatic saving is found to be broken)."),
														   ECVF_Default | ECVF_RenderThreadSafe
														   );
 
static TAutoConsoleVariable<int32> CVarPSOFileCacheAutoSaveTime(
															TEXT("r.ShaderPipelineCache.AutoSaveTime"),
															30,
															TEXT("Set the time where any logged PSO's will be saved if the number is < r.ShaderPipelineCache.SaveAfterPSOsLogged. Disabled when r.ShaderPipelineCache.SaveAfterPSOsLogged is 0"),
															ECVF_Default | ECVF_RenderThreadSafe
														);

static TAutoConsoleVariable<int32> CVarPSOFileCachePreCompileMask(
																TEXT("r.ShaderPipelineCache.PreCompileMask"),
																-1,
																TEXT("Mask used to precompile the cache. Defaults to all PSOs (-1)"),
																ECVF_Default | ECVF_RenderThreadSafe
																);

static TAutoConsoleVariable<int32> CVarPSOFileCacheAutoSaveTimeBoundPSO(
	TEXT("r.ShaderPipelineCache.AutoSaveTimeBoundPSO"),
	MAX_int32, // This effictively disables auto-save, since the feature is broken. See FORT-430086 for details, but in short, Save function takes a broad lock, and while holding it attempts to execute async tasks (reading from pak files). 
			   // These task may not be executed if all the worker threads are blocked trying to acquire the same lock that the saving thread is holding, which happens on a low-core CPUs.
	TEXT("Set the time where any logged PSO's will be saved when -logpso is on the command line."),
	ECVF_Default | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarPSOFileCacheSaveBoundPSOLog(
																 TEXT("r.ShaderPipelineCache.SaveBoundPSOLog"),
																 (int32)0,
																 TEXT("If > 0 then a log of all bound PSOs for this run of the program will be saved to a writable user cache file. Defaults to 0 but is forced on with -logpso."),
																 ECVF_Default | ECVF_RenderThreadSafe
																 );

static TAutoConsoleVariable<int32> CVarPSOFileCacheGameFileMaskEnabled(
																TEXT("r.ShaderPipelineCache.GameFileMaskEnabled"),
																(int32)0,
																TEXT("Set non zero to use GameFileMask during PSO precompile - recording should always save out the usage masks to make that data availble when needed."),
																ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarPSOFileCachePreOptimizeEnabled(
																TEXT("r.ShaderPipelineCache.PreOptimizeEnabled"),
																(int32)0,
																TEXT("Set non zero to PreOptimize PSOs - this allows some PSOs to be compiled in the foreground before going in to game"),
																ECVF_ReadOnly | ECVF_RenderThreadSafe);


static TAutoConsoleVariable<int32> CVarPSOFileCacheMinBindCount(
                                                                TEXT("r.ShaderPipelineCache.MinBindCount"),
                                                                (int32)0,
                                                                TEXT("The minimum bind count to allow a PSO to be precompiled.  Changes to this value will not affect PSOs that have already been removed from consideration."),
                                                                ECVF_Default | ECVF_RenderThreadSafe
                                                                );

static TAutoConsoleVariable<float> CVarPSOFileCacheMaxPrecompileTime(
																TEXT("r.ShaderPipelineCache.MaxPrecompileTime"),
																(float)0.f,
																TEXT("The maximum time to allow a PSO to be precompiled.  if greather than 0, the amount of wall time we will allow pre-compile of PSOs and then switch to background processing."),
																ECVF_Default | ECVF_RenderThreadSafe
																);

static TAutoConsoleVariable<int32> CVarPSOGlobalShadersOnlyWhenPSOPrecaching(
                                                                TEXT("r.ShaderPipelineCache.GlobalShadersOnlyWhenPSOPrecaching"),
                                                                (int32)0,
                                                                TEXT("Only compile PSOs from the GlobalShader cache when runtime PSOPrecaching is enabled (default disabled)"),
                                                                ECVF_Default | ECVF_RenderThreadSafe
                                                                );

static TAutoConsoleVariable<int32> CVarPSOFileCacheOnlyOpenUserCache(
																TEXT("r.ShaderPipelineCache.OnlyOpenUserCache"),
																0,
																TEXT("If > 0, only the user cache file will be opened, and the static file cache will not be opened. Defaults to 0."),
																ECVF_ReadOnly
                                                                );

static bool GetShaderPipelineCacheSaveBoundPSOLog()
{
	static bool bOnce = false;
	static bool bCmdLineForce = false;
	if (!bOnce)
	{
		bOnce = true;
		bCmdLineForce = FParse::Param(FCommandLine::Get(), TEXT("logpso"));
	}
	return GRHISupportsPipelineFileCache && (bCmdLineForce || CVarPSOFileCacheSaveBoundPSOLog.GetValueOnAnyThread() == 1);
}

static bool GetPSOFileCacheSaveUserCache()
{
	static const auto CVarPSOFileCacheSaveUserCache = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderPipelineCache.SaveUserCache"));
	return GRHISupportsPipelineFileCache && (CVarPSOFileCacheSaveUserCache && CVarPSOFileCacheSaveUserCache->GetInt() > 0);
}

void ConsoleCommandLoadPipelineFileCache()
{
	FShaderPipelineCache::CloseUserPipelineFileCache();
	FShaderPipelineCache::OpenUserPipelineFileCache(GMaxRHIShaderPlatform);
}

void ConsoleCommandClosePipelineFileCache()
{
	FShaderPipelineCache::CloseUserPipelineFileCache();
}

void ConsoleCommandSavePipelineFileCache()
{
	if (GetShaderPipelineCacheSaveBoundPSOLog())
	{
		FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::BoundPSOsOnly);
	}
	if (GetPSOFileCacheSaveUserCache())
	{
		FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::Incremental);
	}
}

void ConsoleCommandSwitchModePipelineCacheCmd(const TArray< FString >& Args)
{
    if (Args.Num() > 0)
    {
        FString Mode = Args[0];
        if (Mode == TEXT("Pause"))
        {
            FShaderPipelineCache::PauseBatching();
        }
        else if (Mode == TEXT("Background"))
        {
            FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);
            FShaderPipelineCache::ResumeBatching();
        }
        else if (Mode == TEXT("Fast"))
        {
            FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
            FShaderPipelineCache::ResumeBatching();
        }
		else if (Mode == TEXT("Precompile"))
		{
			FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Precompile);
			FShaderPipelineCache::ResumeBatching();
		}
    }
}

static FAutoConsoleCommand LoadPipelineCacheCmd(
												TEXT("r.ShaderPipelineCache.Open"),
												TEXT("Close and reopen the user cache."),
												FConsoleCommandDelegate::CreateStatic(ConsoleCommandLoadPipelineFileCache)
												);

static FAutoConsoleCommand SavePipelineCacheCmd(
										   		TEXT("r.ShaderPipelineCache.Save"),
										   		TEXT("Save the current pipeline file cache."),
										   		FConsoleCommandDelegate::CreateStatic(ConsoleCommandSavePipelineFileCache)
										   		);

static FAutoConsoleCommand ClosePipelineCacheCmd(
												TEXT("r.ShaderPipelineCache.Close"),
												TEXT("Close the current pipeline file cache."),
												FConsoleCommandDelegate::CreateStatic(ConsoleCommandClosePipelineFileCache)
												);

static FAutoConsoleCommand SwitchModePipelineCacheCmd(
                                                TEXT("r.ShaderPipelineCache.SetBatchMode"),
                                                TEXT("Sets the compilation batch mode, which should be one of:\n\tPause: Suspend precompilation.\n\tBackground: Low priority precompilation.\n\tFast: High priority precompilation."),
                                                FConsoleCommandWithArgsDelegate::CreateStatic(ConsoleCommandSwitchModePipelineCacheCmd)
                                                );
 
int32 GShaderPipelineCacheDoNotPrecompileComputePSO = PLATFORM_ANDROID;	// temporarily (as of 2021-06-21) disable compute PSO precompilation on Android
static FAutoConsoleVariableRef CVarShaderPipelineCacheDoNotPrecompileComputePSO(
												TEXT("r.ShaderPipelineCache.DoNotPrecompileComputePSO"),
												GShaderPipelineCacheDoNotPrecompileComputePSO,
												TEXT("Disables precompilation of compute PSOs (replayed from a recorded file) on start. This is a safety switch in case things go wrong"),
												ECVF_Default
												);

uint32 FShaderPipelineCache::BatchSize = 0;
float FShaderPipelineCache::BatchTime = 0.0f;

class FShaderPipelineCacheArchive final : public FArchive
{
public:
	FShaderPipelineCacheArchive()
	{
	}
	virtual ~FShaderPipelineCacheArchive()
	{
	}

	virtual bool AttachExternalReadDependency(FExternalReadCallback& ReadCallback) override final
	{
		ExternalReadDependencies.Add(ReadCallback);
		return true;
	}

	bool PollExternalReadDependencies()
	{
		for (uint32 i = 0; i < (uint32)ExternalReadDependencies.Num(); )
		{
			FExternalReadCallback& ReadCallback = ExternalReadDependencies[i];
			bool bFinished = ReadCallback(-1.0);
			if (bFinished)
			{
				ExternalReadDependencies.RemoveAt(i);
			}
			else
			{
				++i;
			}
		}
		return (ExternalReadDependencies.Num() == 0);
	}
	
	void BlockingWaitComplete()
	{
		for (uint32 i = 0; i < (uint32)ExternalReadDependencies.Num(); ++i)
		{
			FExternalReadCallback& ReadCallback = ExternalReadDependencies[i];
			ReadCallback(0.0);
		}
	}

private:
	/**
	*  List of external read dependencies that must be finished to load this package
	*/
	TArray<FExternalReadCallback> ExternalReadDependencies;
};


namespace UE
{
	namespace ShaderPipeline
	{

		FString LibNameToPSOFCName(FString const& Name, int32 ComponentID)
		{
			if (ComponentID == -1)
			{
				return Name;
			}
			return FString::Printf(TEXT("%s_Chunk%d"), *Name, ComponentID);
		}

		bool ShouldLoadUserCache()
		{
			return GetShaderPipelineCacheSaveBoundPSOLog() || GetPSOFileCacheSaveUserCache();
		}


		FString GetShaderPipelineBaseName(bool bWantUserCacheName)
		{
			FString Name = FApp::GetProjectName();

			const TCHAR* OverrideFilenameParam = bWantUserCacheName ? FShaderPipelineCacheConstants::UserCacheFileNameOverride : FShaderPipelineCacheConstants::CacheFileNameOverride;
			FString OverrideName;
			if (GConfig && (
					(GConfig->GetString(FShaderPipelineCacheConstants::SectionHeading, OverrideFilenameParam, OverrideName, *GGameUserSettingsIni) || GConfig->GetString(FShaderPipelineCacheConstants::SectionHeading, OverrideFilenameParam, OverrideName, *GGameIni))
					&& OverrideName.Len())
				)
			{
				Name = OverrideName;
			}

			return Name;
		}


		struct CompileJob
		{
			FPipelineCacheFileFormatPSO PSO;
			FShaderPipelineCacheArchive* ReadRequests;

			/** Tracks whether the shaders were preloaded. */
			bool bShadersPreloaded = false;

			/**
			 * Schedules preload for all the shaders in the PSO. No shaders are preloaded if one of them isn't in the library.
			 * Multiple jobs can request to preload the same shaders and then release them independently.
			 *
			 * @param OutRequiredShaders shader hashes of shaders that are actually required by this PSO
			 * @param bOutCompatible if set to false, then this PSO type isn't supported (RTX as of now)
			 */
			bool PreloadShaders(TSet<FSHAHash>& OutRequiredShaders, bool& bOutCompatible);

			/**
			 * Releases preloaded entries for the shaders in the PSO. Symmetric to to PreloadShaders.
			 * Multiple jobs can request to preload the same shaders and then release them independently.
			 */
			void ReleasePreloadedShaders();
		};
	}
}

/* 
* FShaderPipelineCacheTask is encapsulates the precompile process for a single PSO file cache.
*/
class FShaderPipelineCacheTask
{
	friend class FShaderPipelineCache;

public:
	FShaderPipelineCacheTask(FShaderPipelineCache* InCache, bool bIsUserCacheIn, EShaderPlatform InShaderPlatform) :
		ShaderPlatform(InShaderPlatform), bIsUserCache(bIsUserCacheIn), Cache(InCache)
	{
		// Mark task as ready if either:
		// * The usage mask mechanism is disabled.
		// * The usage mask mechanism is enabled and the mask has already been set.
		// * Pre-optimization is enabled.
		bReady = (!CVarPSOFileCacheGameFileMaskEnabled.GetValueOnAnyThread() || FPipelineFileCacheManager::IsGameUsageMaskSet())
			|| CVarPSOFileCachePreOptimizeEnabled.GetValueOnAnyThread();
	}

	bool Open(const FString& Key, const FString& Name, EShaderPlatform Platform);
	
	void Close();
	void Shutdown();

	void BeginPrecompilingPipelineCache();

	bool Precompile(FRHICommandListImmediate& RHICmdList, EShaderPlatform Platform, FPipelineCacheFileFormatPSO const& PSO);
	void PreparePipelineBatch(TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>& PipelineBatch);
	bool ReadyForPrecompile();
	void PrecompilePipelineBatch();
	bool ReadyForNextBatch() const;
	bool ReadyForAutoSave() const;
	void PollShutdownItems();
	void Flush();
	void Tick(float DeltaTime);
	bool OnShaderLibraryStateChanged(FShaderPipelineCache::ELibraryState State, EShaderPlatform Platform, FString const& Name, int32 ComponentID);
private:
	FString PSOCacheKey;
	int32 TotalPSOsCompiled = 0;
	EShaderPlatform ShaderPlatform = EShaderPlatform::SP_NumPlatforms;
	TArray<UE::ShaderPipeline::CompileJob> ReadTasks;
	TArray<UE::ShaderPipeline::CompileJob> CompileTasks;
	TArray<FPipelineCachePSOHeader> OrderedCompileTasks;
	TDoubleLinkedList<FPipelineCacheFileFormatPSORead*> FetchTasks;
	FGuid CacheFileGuid;

	TSet<uint64> CompletedMasks;
	FShaderPipelineCache::FShaderCachePrecompileContext PrecompileContext;
	bool bPreOptimizing = false;
	TArray<FPipelineCachePSOHeader> PreFetchedTasks;
	bool bReady = false;
	bool bOpened = false;
	bool bIsUserCache;

	/** Cache which kicked off this task. */
	FShaderPipelineCache* Cache = nullptr;

	// as there is only ever one active precompile task at a time
	// these stats are available to read anywhere.
	static float TotalPrecompileWallTime;
	static int64 TotalPrecompileTasks;

	static std::atomic_int64_t TotalActiveTasks;
	static std::atomic_int64_t TotalWaitingTasks;
	static std::atomic_int64_t TotalCompleteTasks;
	static std::atomic_int64_t TotalPrecompileTime;

	static double PrecompileStartTime;
};

float FShaderPipelineCacheTask::TotalPrecompileWallTime = 0.0f;
int64 FShaderPipelineCacheTask::TotalPrecompileTasks = 0;
std::atomic_int64_t FShaderPipelineCacheTask::TotalActiveTasks = 0;
std::atomic_int64_t FShaderPipelineCacheTask::TotalWaitingTasks = 0;
std::atomic_int64_t FShaderPipelineCacheTask::TotalCompleteTasks = 0;
std::atomic_int64_t FShaderPipelineCacheTask::TotalPrecompileTime = 0;
double FShaderPipelineCacheTask::PrecompileStartTime = 0.0;



namespace UE
{
	namespace ShaderPipeline
	{
		// The shutdown container takes abandoned tasks off the PSO caches.
		// PSO Cache completion does not require these tasks to have completed.
		class FShutdownContainer
		{
		public:
			static TUniquePtr<class FShutdownContainer> Instance;
			std::atomic_int TotalRemainingTasks = 0;
			TArray<UE::ShaderPipeline::CompileJob> ShutdownReadCompileTasks;
			TDoubleLinkedList<FPipelineCacheFileFormatPSORead*> ShutdownFetchTasks;

			static TUniquePtr<class FShutdownContainer>& Get()
			{
				if (!Instance.IsValid())
				{
					Instance = MakeUnique<FShutdownContainer>();
				}
				return Instance;
			}

			void AbandonCompileJobs(TArray<UE::ShaderPipeline::CompileJob>&& AbandonedCompileJobs)
			{
				TotalRemainingTasks += AbandonedCompileJobs.Num();
				ShutdownReadCompileTasks.Append(MoveTemp(AbandonedCompileJobs));
			}

			void AbandonCompileJob(UE::ShaderPipeline::CompileJob&& AbandonedCompileJob)
			{
				ShutdownReadCompileTasks.Emplace(AbandonedCompileJob);
				TotalRemainingTasks++;
			}

			void AbandonFetchTasks(TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>&& AbandonedFetchTasks)
			{
				// Marshall the current fetch tasks into shutdown
				for (TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TIterator It(AbandonedFetchTasks.GetHead()); It; ++It)
				{
					FPipelineCacheFileFormatPSORead* Entry = *It;
					check(Entry->ReadRequest.IsValid());
					Entry->ReadRequest->Cancel();
					ShutdownFetchTasks.AddTail(Entry);
					TotalRemainingTasks++;
				}

			}

			bool IsTickable() const
			{
				return TotalRemainingTasks > 0;
			}

			void Tick()
			{
				int64 RemovedTaskCount = 0;

				const int64 InitialCompileTaskCount = ShutdownReadCompileTasks.Num();
				if (ShutdownReadCompileTasks.Num() > 0)
				{
					for (uint32 i = 0; i < (uint32)ShutdownReadCompileTasks.Num(); )
					{
						check(ShutdownReadCompileTasks[i].ReadRequests);
						if (ShutdownReadCompileTasks[i].ReadRequests->PollExternalReadDependencies())
						{
							ShutdownReadCompileTasks[i].ReleasePreloadedShaders();
							delete ShutdownReadCompileTasks[i].ReadRequests;
							ShutdownReadCompileTasks[i].ReadRequests = nullptr;

							ShutdownReadCompileTasks.RemoveAtSwap(i, 1, EAllowShrinking::No);
							++RemovedTaskCount;
						}
						else
						{
							++i;
						}
					}

					if (ShutdownReadCompileTasks.Num() == 0)
					{
						ShutdownReadCompileTasks.Shrink();
					}
				}

				if (ShutdownFetchTasks.Num() > 0)
				{
					TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TDoubleLinkedListNode* CurrentNode = ShutdownFetchTasks.GetHead();
					while (CurrentNode)
					{
						FPipelineCacheFileFormatPSORead* PSORead = CurrentNode->GetValue();
						check(PSORead);
						FShaderPipelineCacheArchive* Archive = (FShaderPipelineCacheArchive*)(PSORead->Ar);
						check(Archive);

						TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TDoubleLinkedListNode* PrevNode = CurrentNode;
						CurrentNode = CurrentNode->GetNextNode();

						if (PSORead->bReadCompleted || Archive->PollExternalReadDependencies())
						{
							delete PSORead;
							ShutdownFetchTasks.RemoveNode(PrevNode);
							++RemovedTaskCount;
						}
					}
				}

				if (RemovedTaskCount > 0)
				{
					TotalRemainingTasks -= RemovedTaskCount;
				}
			}

			void ShutdownAndWait()
			{
				for (UE::ShaderPipeline::CompileJob& Entry : ShutdownReadCompileTasks)
				{
					if (Entry.ReadRequests != nullptr)
					{
						Entry.ReadRequests->BlockingWaitComplete();
						Entry.ReleasePreloadedShaders();
						delete Entry.ReadRequests;
						Entry.ReadRequests = nullptr;
					}
				}
				ShutdownReadCompileTasks.Empty();

				for (FPipelineCacheFileFormatPSORead* Entry : ShutdownFetchTasks)
				{
					if (Entry->ReadRequest.IsValid())
					{
						Entry->ReadRequest->WaitCompletion(0.f);
					}
					delete Entry;
				}
				ShutdownFetchTasks.Empty();
				TotalRemainingTasks = 0;
				Instance = nullptr;
			}
		};
		TUniquePtr<class FShutdownContainer> FShutdownContainer::Instance;
	}
}

FShaderPipelineCache* FShaderPipelineCache::ShaderPipelineCache = nullptr;
FString FShaderPipelineCache::UserCacheTaskKey;

FShaderPipelineCache::FShaderCachePreOpenDelegate FShaderPipelineCache::OnCachePreOpen;
FShaderPipelineCache::FShaderCacheOpenedDelegate FShaderPipelineCache::OnCachedOpened;
FShaderPipelineCache::FShaderCacheClosedDelegate FShaderPipelineCache::OnCachedClosed;
FShaderPipelineCache::FShaderPrecompilationBeginDelegate FShaderPipelineCache::OnPrecompilationBegin;
FShaderPipelineCache::FShaderPrecompilationCompleteDelegate FShaderPipelineCache::OnPrecompilationComplete;

static void PipelineStateCacheOnAppDeactivate()
{
	if (GetShaderPipelineCacheSaveBoundPSOLog())
	{
		FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::BoundPSOsOnly);
	}
	if (GetPSOFileCacheSaveUserCache())
	{
		FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::Incremental);
	}
}

int32 FShaderPipelineCache::GetGameVersionForPSOFileCache()
{
	int32 GameVersion = (int32)FEngineVersion::Current().GetChangelist();
#if WITH_EDITOR
	// We might be using the editor built at a different (older) CL than the current sync (think precompiled binaries synced via UGS) when packaging.
	// As such, don't use the current CL as it does not reflect the state of the content.
	FBuildVersion BuildVersion;
	if (FBuildVersion::TryRead(FBuildVersion::GetDefaultFileName(), BuildVersion))
	{
		// Double check that compatible changelist in the build info is the same (or older) as our binary.
		// CompatibleChangelist should be populated at the time we compile the precompiled binaries.
		// If it happens to be newer than the current CL encoded in the binary, it means that we synced the project in UGS
		// without taking the new precompiled binaries (or without recompiling the engine manually)
		if (FEngineVersion::Current().GetChangelist() >= (uint32)BuildVersion.CompatibleChangelist)
		{
			GameVersion = BuildVersion.Changelist;
		}
	}
#endif
	GConfig->GetInt(FShaderPipelineCacheConstants::SectionHeading, FShaderPipelineCacheConstants::GameVersionKey, GameVersion, *GGameIni);
	return GameVersion;
}

bool FShaderPipelineCache::SetGameUsageMaskWithComparison(uint64 InMask, FPSOMaskComparisonFn InComparisonFnPtr)
{
	static bool bMaskChanged = false;

	if (ShaderPipelineCache != nullptr && CVarPSOFileCacheGameFileMaskEnabled.GetValueOnAnyThread())
	{
		FScopeLock Lock(&ShaderPipelineCache->Mutex);
		
		if (!ShaderPipelineCache->ShaderCacheTasks.IsEmpty())
		{
			uint64 OldMask = FPipelineFileCacheManager::SetGameUsageMaskWithComparison(InMask, InComparisonFnPtr);
			bMaskChanged |= OldMask != InMask;
			if (bMaskChanged)
			{
				int PendingPSOCacheCount = ShaderPipelineCache->ApplyNewUsageMaskToAllTasks(InMask);

				bMaskChanged = false;
						
				UE_LOG(LogRHI, Display, TEXT("New ShaderPipelineCacheTask GameUsageMask [%llu=>%llu], queued %d cache(s) for precompile."), OldMask, InMask, PendingPSOCacheCount);

				return OldMask != InMask;
			}
			else
			{
				UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache::SetGameUsageMaskWithComparison failed to set a new mask because the game mask was not different"));
			}
		}
		else
		{
			uint64 OldMask = FPipelineFileCacheManager::SetGameUsageMaskWithComparison(InMask, InComparisonFnPtr);
			bMaskChanged |= OldMask != InMask;

			UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache::SetGameUsageMaskWithComparison set a new mask but did not attempt to setup any tasks because there are no PSO caching tasks."));
			return OldMask != InMask;
		}
	}
	else
	{
		UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache::SetGameUsageMaskWithComparison failed to set a new mask because the cache was not open or game mask is not enabled"));
	}
	
	return bMaskChanged;
}


int FShaderPipelineCache::ApplyNewUsageMaskToAllTasks(uint64 Mask)
{
	// Reset the current queue and all of the tasks.
	ShaderPipelineCache->ClearPendingPrecompileTaskQueue();
	int PendingPSOCacheCount = 0;
	for (TPair < FString, TUniquePtr<class FShaderPipelineCacheTask>>& ShaderCacheTask : ShaderPipelineCache->ShaderCacheTasks)
	{
		if (!ShaderCacheTask.Value->CompletedMasks.Contains(Mask))
		{
			// Mask has changed and we have an open file refetch PSO's for this Mask - leave the FPipelineFileCacheManager file open - no need to close - just pull out the relevant PSOs.
			// If this PSO compile run has completed for this Mask in which case don't refetch + compile for that mask
			// Don't clear already compiled PSOHash list - this is not a full reset
			ShaderCacheTask.Value->bReady = true;
			ShaderPipelineCache->EnqueuePendingPrecompileTask(ShaderCacheTask.Key);
			PendingPSOCacheCount++;
		}
	}

	return PendingPSOCacheCount;
}

void FShaderPipelineCache::Initialize(EShaderPlatform Platform)
{
	check(ShaderPipelineCache == nullptr);
	
	if(FShaderCodeLibrary::IsEnabled())
	{
		FPipelineFileCacheManager::Initialize(GetGameVersionForPSOFileCache());
		ShaderPipelineCache = new FShaderPipelineCache(Platform);
	}
}

void FShaderPipelineCache::Shutdown(void)
{
	if (ShaderPipelineCache)
	{
		delete ShaderPipelineCache;
		ShaderPipelineCache = nullptr;
	}
}

void FShaderPipelineCache::PauseBatching()
{
	if (ShaderPipelineCache)
    {
        ShaderPipelineCache->PausedCount++;
        UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache: Paused Batching. %d"), ShaderPipelineCache->PausedCount);
        if (ShaderPipelineCache->PausedCount > 0)
        {
            ShaderPipelineCache->bPaused = true;
        }
    }
}

bool FShaderPipelineCache::IsBatchingPaused()
{
    if (ShaderPipelineCache)
    {
        return ShaderPipelineCache->bPaused;
    }
    return true;
}

void FShaderPipelineCache::SetBatchMode(BatchMode Mode)
{
	if (ShaderPipelineCache)
	{
		uint32 PreviousBatchSize = ShaderPipelineCache->BatchSize;
		float PreviousBatchTime = ShaderPipelineCache->BatchTime;
		switch (Mode)
		{
			case BatchMode::Precompile:
			{
				ShaderPipelineCache->BatchSize = CVarPSOFileCachePrecompileBatchSize.GetValueOnAnyThread();
				ShaderPipelineCache->BatchTime = CVarPSOFileCachePrecompileBatchTime.GetValueOnAnyThread();
				break;
			}
			case BatchMode::Fast:
			{
				ShaderPipelineCache->BatchSize = CVarPSOFileCacheBatchSize.GetValueOnAnyThread();
				ShaderPipelineCache->BatchTime = CVarPSOFileCacheBatchTime.GetValueOnAnyThread();
				break;
			}
			case BatchMode::Background:
			default:
			{
				ShaderPipelineCache->BatchSize = CVarPSOFileCacheBackgroundBatchSize.GetValueOnAnyThread();
				ShaderPipelineCache->BatchTime = CVarPSOFileCacheBackgroundBatchTime.GetValueOnAnyThread();
				break;
			}
		}
		UE_CLOG(PreviousBatchSize != ShaderPipelineCache->BatchSize || PreviousBatchTime != ShaderPipelineCache->BatchTime, LogRHI, Log,
			TEXT("ShaderPipelineCache: Batch mode changed (%d): Size: %d -> %d, Time: %f -> %f"), (uint32)Mode, PreviousBatchSize, ShaderPipelineCache->BatchSize, PreviousBatchTime, ShaderPipelineCache->BatchTime);
	}
}

void FShaderPipelineCache::ResumeBatching()
{
	if (ShaderPipelineCache)
    {
        ShaderPipelineCache->PausedCount--;
        UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache: Resumed Batching. %d"), ShaderPipelineCache->PausedCount);
        if (ShaderPipelineCache->PausedCount <= 0)
        {
            UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache: Batching Resumed."));
            ShaderPipelineCache->PausedCount = 0;
            ShaderPipelineCache->bPaused = false;
        }
    }
}

uint32 FShaderPipelineCache::NumPrecompilesRemaining()
{
	uint32 NumRemaining = 0;
	
	if (ShaderPipelineCache)
	{
		if (CVarPSOFileCacheMaxPrecompileTime.GetValueOnAnyThread() > 0.0 && FShaderPipelineCacheTask::TotalPrecompileTasks > 0)
		{
			float Mult = FShaderPipelineCacheTask::TotalPrecompileWallTime / CVarPSOFileCacheMaxPrecompileTime.GetValueOnAnyThread();
			NumRemaining = uint32(FMath::Max(0.f, 1.f - Mult) * FShaderPipelineCacheTask::TotalPrecompileTasks);
		}
		else
		{
			int64 NumActiveTasksRemaining = FMath::Max((int64_t)0, FShaderPipelineCacheTask::TotalActiveTasks.load());
			int64 NumWaitingTasksRemaining = FMath::Max((int64_t)0, FShaderPipelineCacheTask::TotalWaitingTasks.load());
			NumRemaining = (uint32)(NumActiveTasksRemaining + NumWaitingTasksRemaining);
		}
	}

	// Add the number of active PSO precache requests as well
	NumRemaining += PipelineStateCache::NumActivePrecacheRequests();

	// NumRemaining is correct only for the current cache.
	// This ensures we do not return 0 if there are more caches to process.
	uint32 PendingCaches = IsPrecompiling() ? 1 : 0;
	return FMath::Max(PendingCaches,NumRemaining);
}

bool FShaderPipelineCache::OpenPipelineFileCache(EShaderPlatform Platform)
{
	if (CVarPSOFileCacheOnlyOpenUserCache.GetValueOnAnyThread())
	{
		UE_LOG(LogRHI, Log, TEXT("Ignoring request to open static pipeline file cache because r.ShaderPipelineCache.OnlyOpenUserCache is set."));
		return false;
	}

	bool bFileOpen = false;
	if (ShaderPipelineCache)
	{
		FScopeLock Lock(&ShaderPipelineCache->Mutex);

		FString CacheName = UE::ShaderPipeline::GetShaderPipelineBaseName(false);
		// for Bundled files the Key == CacheName, only the user cache differs.
		const FString& Key = CacheName;

		bFileOpen = ShaderPipelineCache->OpenPipelineFileCacheInternal(false, Key, CacheName, Platform);
	}
	return bFileOpen;
}

bool FShaderPipelineCache::OpenUserPipelineFileCache(EShaderPlatform Platform)
{
	bool bFileOpen = false;
	if (ShaderPipelineCache)
	{
		FScopeLock Lock(&ShaderPipelineCache->Mutex);
		// Just use the project for the user cache name.
		FString UserCacheName = UE::ShaderPipeline::GetShaderPipelineBaseName(true);
		ShaderPipelineCache->UserCacheTaskKey = UserCacheName + TEXT("_usr");
		if (UE::ShaderPipeline::ShouldLoadUserCache() && !ShaderPipelineCache->GetTask(UserCacheTaskKey))
		{
			bFileOpen = ShaderPipelineCache->OpenPipelineFileCacheInternal(true, ShaderPipelineCache->UserCacheTaskKey, UserCacheName, Platform);
		}
	}
	return bFileOpen;
}

bool FShaderPipelineCache::OpenPipelineFileCacheInternal(bool bWantUserCache, const FString& PSOCacheKey, const FString& CacheName, EShaderPlatform Platform)
{
	if(!ShaderCacheTasks.Contains(PSOCacheKey))
	{
		TUniquePtr<FShaderPipelineCacheTask> NewPSOTask = MakeUnique<FShaderPipelineCacheTask>(this, bWantUserCache, Platform);
		bool bOpened = NewPSOTask->Open(PSOCacheKey, CacheName, Platform);
		if(bOpened)
		{
			bool bReady = NewPSOTask->bReady;
			ShaderCacheTasks.FindOrAdd(PSOCacheKey, MoveTemp(NewPSOTask));
			if(bReady)
			{
				EnqueuePendingPrecompileTask(PSOCacheKey);
			}
		}
		return bOpened;
	}
	return false;
}

bool FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode Mode)
{
	if (ShaderPipelineCache)
	{
		FScopeLock Lock(&ShaderPipelineCache->Mutex);

		bool bOK = FPipelineFileCacheManager::SavePipelineFileCache(Mode);
		UE_CLOG(!bOK, LogRHI, Warning, TEXT("Failed to save shader pipeline cache for using save mode %d."), (uint32)Mode);

		ShaderPipelineCache->LastAutoSaveTime = FPlatformTime::Seconds();

		return bOK;
	}

	return false;
}

void FShaderPipelineCache::CloseUserPipelineFileCache()
{
	if (ShaderPipelineCache)
	{
		FScopeLock Lock(&ShaderPipelineCache->Mutex);

		// Log all bound PSOs
		if (GetShaderPipelineCacheSaveBoundPSOLog())
		{
			SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::BoundPSOsOnly);
		}

		if (GetPSOFileCacheSaveUserCache())
		{
			SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::Incremental);
		}

		
 		if (FShaderPipelineCacheTask* Found = ShaderPipelineCache->GetTask(UserCacheTaskKey))
 		{
			Found->Close();
			ShaderPipelineCache->PendingPrecompilePSOCacheKeys.Remove(UserCacheTaskKey);
			ShaderPipelineCache->ShaderCacheTasks.Remove(UserCacheTaskKey);
 		}

		FPipelineFileCacheManager::CloseUserPipelineFileCache();
	}
}


void FShaderPipelineCache::ShaderLibraryStateChanged(ELibraryState State, EShaderPlatform Platform, FString const& PSOCacheBaseName, int32 ComponentID)
{
	if (ShaderPipelineCache)
	{
		const FString PSOCacheName = UE::ShaderPipeline::LibNameToPSOFCName(PSOCacheBaseName, ComponentID);
		// Key == Name for bundles PSOFCs
		const FString& PSOCacheKey = PSOCacheName;

		FScopeLock Lock(&ShaderPipelineCache->Mutex);
		if (!CVarPSOFileCacheOnlyOpenUserCache.GetValueOnAnyThread())
		{
			if (TUniquePtr<class FShaderPipelineCacheTask>* Found = ShaderPipelineCache->ShaderCacheTasks.Find(PSOCacheKey))
			{
				(*Found)->OnShaderLibraryStateChanged(State, Platform, PSOCacheName, ComponentID);
			}
			else
			{
				bool bSuccess = ShaderPipelineCache->OpenPipelineFileCacheInternal(false, PSOCacheKey, PSOCacheName, Platform);

				const FString& PSOCacheBaseKey = PSOCacheBaseName;
				// for some reason we're not always able to find the base PSO cache when the shader libs loads.
				if( !bSuccess && !ShaderPipelineCache->ShaderCacheTasks.Contains(PSOCacheBaseKey))
				{
					ShaderPipelineCache->OpenPipelineFileCacheInternal(false, PSOCacheBaseKey, PSOCacheBaseName, Platform);
				}
			}
		}
		else
		{
			// If we're only opening the user cache, disable logging new PSOs
			// as it can get noisy.
			FPipelineFileCacheManager::SetNewPSOConsoleAndCSVLogging(false);
		}

		// An opportunity to open the user cache
		OpenUserPipelineFileCache(Platform);
	}
}

bool FShaderPipelineCacheTask::Precompile(FRHICommandListImmediate& RHICmdList, EShaderPlatform Platform, FPipelineCacheFileFormatPSO const& PSO)
{
	INC_DWORD_STAT(STAT_PreCompileShadersTotal);
	INC_DWORD_STAT(STAT_PreCompileShadersNum);
    
    uint64 StartTime = FPlatformTime::Cycles64();

	bool bOk = false;
	
	if(PSO.Verify())
	{
		if(FPipelineCacheFileFormatPSO::DescriptorType::Graphics == PSO.Type)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FShaderPipelineCache::PrecompileGraphics);

			FGraphicsPipelineStateInitializer GraphicsInitializer;
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FShaderPipelineCache_PrepareInitializer);

				if (PSO.GraphicsDesc.MeshShader != FSHAHash())
				{
					// Skip creation of mesh shaders if RHI doesn't support them
					if (!GRHISupportsMeshShadersTier0)
					{
						return false;
					}

					FMeshShaderRHIRef MeshShader = FShaderCodeLibrary::CreateMeshShader(Platform, PSO.GraphicsDesc.MeshShader);
					GraphicsInitializer.BoundShaderState.SetMeshShader(MeshShader);

					if (PSO.GraphicsDesc.AmplificationShader != FSHAHash())
					{
						FAmplificationShaderRHIRef AmplificationShader = FShaderCodeLibrary::CreateAmplificationShader(Platform, PSO.GraphicsDesc.AmplificationShader);
						GraphicsInitializer.BoundShaderState.SetAmplificationShader(AmplificationShader);
					}
				}
				else
				{
					FRHIVertexDeclaration* VertexDesc = PipelineStateCache::GetOrCreateVertexDeclaration(PSO.GraphicsDesc.VertexDescriptor);
					GraphicsInitializer.BoundShaderState.VertexDeclarationRHI = VertexDesc;

					FVertexShaderRHIRef VertexShader;
					if (PSO.GraphicsDesc.VertexShader != FSHAHash())
					{
						VertexShader = FShaderCodeLibrary::CreateVertexShader(Platform, PSO.GraphicsDesc.VertexShader);
						GraphicsInitializer.BoundShaderState.VertexShaderRHI = VertexShader;
					}

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
					FGeometryShaderRHIRef GeometryShader;
					if (PSO.GraphicsDesc.GeometryShader != FSHAHash())
					{
						GeometryShader = FShaderCodeLibrary::CreateGeometryShader(Platform, PSO.GraphicsDesc.GeometryShader);
						GraphicsInitializer.BoundShaderState.SetGeometryShader(GeometryShader);
					}
#endif
				}

				FPixelShaderRHIRef FragmentShader;
				if (PSO.GraphicsDesc.FragmentShader != FSHAHash())
				{
					FragmentShader = FShaderCodeLibrary::CreatePixelShader(Platform, PSO.GraphicsDesc.FragmentShader);
					GraphicsInitializer.BoundShaderState.PixelShaderRHI = FragmentShader;
				}

				auto BlendState = FShaderPipelineCache::Get()->GetOrCreateBlendState(PSO.GraphicsDesc.BlendState);
				GraphicsInitializer.BlendState = BlendState;

				auto RasterState = FShaderPipelineCache::Get()->GetOrCreateRasterizerState(PSO.GraphicsDesc.RasterizerState);
				GraphicsInitializer.RasterizerState = RasterState;

				auto DepthState = FShaderPipelineCache::Get()->GetOrCreateDepthStencilState(PSO.GraphicsDesc.DepthStencilState);
				GraphicsInitializer.DepthStencilState = DepthState;

				for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
				{
					GraphicsInitializer.RenderTargetFormats[i] = PSO.GraphicsDesc.RenderTargetFormats[i];
					GraphicsInitializer.RenderTargetFlags[i] = PSO.GraphicsDesc.RenderTargetFlags[i];
				}

				GraphicsInitializer.RenderTargetsEnabled = PSO.GraphicsDesc.RenderTargetsActive;
				GraphicsInitializer.NumSamples = PSO.GraphicsDesc.MSAASamples;

				GraphicsInitializer.SubpassHint = (ESubpassHint)PSO.GraphicsDesc.SubpassHint;
				GraphicsInitializer.SubpassIndex = PSO.GraphicsDesc.SubpassIndex;

				GraphicsInitializer.MultiViewCount = PSO.GraphicsDesc.MultiViewCount;
				GraphicsInitializer.bHasFragmentDensityAttachment = PSO.GraphicsDesc.bHasFragmentDensityAttachment;

				GraphicsInitializer.bDepthBounds = PSO.GraphicsDesc.bDepthBounds;

				GraphicsInitializer.DepthStencilTargetFormat = PSO.GraphicsDesc.DepthStencilFormat;
				GraphicsInitializer.DepthStencilTargetFlag = PSO.GraphicsDesc.DepthStencilFlags;
				GraphicsInitializer.DepthTargetLoadAction = PSO.GraphicsDesc.DepthLoad;
				GraphicsInitializer.StencilTargetLoadAction = PSO.GraphicsDesc.StencilLoad;
				GraphicsInitializer.DepthTargetStoreAction = PSO.GraphicsDesc.DepthStore;
				GraphicsInitializer.StencilTargetStoreAction = PSO.GraphicsDesc.StencilStore;

				GraphicsInitializer.PrimitiveType = PSO.GraphicsDesc.PrimitiveType;

				// This indicates we do not want a fatal error if this compilation fails
				// (ie, if this entry in the file cache is bad)
				GraphicsInitializer.bFromPSOFileCache = true;

#if !UE_BUILD_SHIPPING
				// dump to log to describe
				if (Cache && Cache->ShaderHashToStableKey.IsInitialized())
				{
					Cache->ShaderHashToStableKey.DumpPSOToLogIfConfigured(GraphicsInitializer);
				}
#endif
			}
			// Use SetGraphicsPipelineState to call down into PipelineStateCache and also handle the fallback case used by OpenGL.
			// we lose track of our PSO precompile jobs through here. they may not be RHI dependencies and could persist beyond the next tick.
			// GetNumActivePipelinePrecompileTasks() is tracking non-rhi dependent precompiles.
			// It's being used for rate limiting and ensuring tasks are done before calling OnPrecompilationComplete delegate.
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FShaderPipelineCache_SetGraphicsPipelineState);
				SetGraphicsPipelineState(RHICmdList, GraphicsInitializer, 0, EApplyRendertargetOption::DoNothing, false);
			}
			bOk = true;
		}
		else if(FPipelineCacheFileFormatPSO::DescriptorType::Compute == PSO.Type)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FShaderPipelineCache::PrecompileCompute);
			
			if (LIKELY(!GShaderPipelineCacheDoNotPrecompileComputePSO))
			{
				FComputeShaderRHIRef ComputeInitializer = FShaderCodeLibrary::CreateComputeShader(Platform, PSO.ComputeDesc.ComputeShader);
				if (ComputeInitializer.IsValid())
				{
					FComputePipelineState* ComputeResult = PipelineStateCache::GetAndOrCreateComputePipelineState(RHICmdList, ComputeInitializer, true, EPSOPrecacheResult::Untracked);
					bOk = ComputeResult != nullptr;
				}
			}
			else
			{
				bOk = true;
			}
		}
		else if (FPipelineCacheFileFormatPSO::DescriptorType::RayTracing == PSO.Type)
		{
			// If ray tracing PSO file cache is generated using one payload size but later shaders were re-compiled with a different payload declaration 
			// it is possible for the wrong size to be used here, which leads to a D3D run-time error when attempting to create the PSO.
			// Ray tracing shader pre-compilation is disabled until a robust solution is found.
			// Consider setting r.RayTracing.NonBlockingPipelineCreation=1 meanwhile to avoid most of the RTPSO creation stalls during gameplay.
			if (IsRayTracingEnabled())
			{
				FRayTracingShaderRHIRef RayTracingShader = FShaderCodeLibrary::CreateRayTracingShader(Platform, PSO.RayTracingDesc.ShaderHash, PSO.RayTracingDesc.Frequency);
				if (RayTracingShader.IsValid())
				{
					FRayTracingPipelineStateInitializer Initializer;
						Initializer.bPartial = true; // Indicates that this RTPSO is used only as input for later RTPSO linking step (not for rendering)
						Initializer.bAllowHitGroupIndexing = PSO.RayTracingDesc.bAllowHitGroupIndexing;
						Initializer.MaxPayloadSizeInBytes = RayTracingShader->RayTracingPayloadSize;

						FRHIRayTracingShader* ShaderTable[] =
						{
							RayTracingShader.GetReference()
						};

						switch (PSO.RayTracingDesc.Frequency)
						{
						case SF_RayGen:
							Initializer.SetRayGenShaderTable(ShaderTable);
							break;
						case SF_RayMiss:
							Initializer.SetMissShaderTable(ShaderTable);
							break;
						case SF_RayHitGroup:
							Initializer.SetHitGroupTable(ShaderTable);
							break;
						case SF_RayCallable:
							Initializer.SetCallableTable(ShaderTable);
							break;
						default:
							checkNoEntry();
						}

					FRayTracingPipelineState* RayTracingPipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);
					bOk = RayTracingPipeline != nullptr;
				}
			}
		}
		else
		{
			check(false);
		}
	}
#if !UE_BUILD_SHIPPING
	else
	{
		UE_LOG(LogRHI, Warning, TEXT("FShaderPipelineCache::Precompile() - PSO Verify failure - Did not attempt to precompile"));
	}
#endif
	
    // All read dependencies have given the green light - always update task counts
    // Otherwise we end up with outstanding compiles that we can't progress or external tools may think this has not been completed and may run again.
    {
        uint64 TimeDelta = FPlatformTime::Cycles64() - StartTime;
        TotalCompleteTasks++;
        TotalPrecompileTime+= TimeDelta;
    }
	
	return bOk;
}

bool UE::ShaderPipeline::CompileJob::PreloadShaders(TSet<FSHAHash>& OutRequiredShaders, bool &bOutCompatible)
{
	static FSHAHash EmptySHA;
	bool bOK = true;
	TArray<FSHAHash> ShadersToUnpreloadInCaseOfFailure;

	auto PreloadShader = [&bOK, &ShadersToUnpreloadInCaseOfFailure](const FSHAHash& ShaderHash, FShaderPipelineCacheArchive* ReadReqs)
	{
		if (bOK && ShaderHash != EmptySHA)
		{
			bOK &= FShaderCodeLibrary::PreloadShader(ShaderHash, ReadReqs);
			if (bOK)
			{
				ShadersToUnpreloadInCaseOfFailure.Add(ShaderHash);
			}
			UE_CLOG(!bOK, LogRHI, Verbose, TEXT("Failed to read shader: %s"), *(ShaderHash.ToString()));
		}
	};

	if (PSO.Type == FPipelineCacheFileFormatPSO::DescriptorType::Graphics)
	{
		// See if the shaders exist in the current code libraries, before trying to load the shader data
		if (PSO.GraphicsDesc.MeshShader != EmptySHA)
		{
			OutRequiredShaders.Add(PSO.GraphicsDesc.MeshShader);
			bOK &= FShaderCodeLibrary::ContainsShaderCode(PSO.GraphicsDesc.MeshShader);
			UE_CLOG(!bOK, LogRHI, Verbose, TEXT("Failed to find MeshShader shader: %s"), *(PSO.GraphicsDesc.MeshShader.ToString()));

			if (PSO.GraphicsDesc.AmplificationShader != EmptySHA)
			{
				OutRequiredShaders.Add(PSO.GraphicsDesc.AmplificationShader);
				bOK &= FShaderCodeLibrary::ContainsShaderCode(PSO.GraphicsDesc.AmplificationShader);
				UE_CLOG(!bOK, LogRHI, Verbose, TEXT("Failed to find AmplificationShader shader: %s"), *(PSO.GraphicsDesc.AmplificationShader.ToString()));
			}
		}
		else if (PSO.GraphicsDesc.VertexShader != EmptySHA)
		{
			OutRequiredShaders.Add(PSO.GraphicsDesc.VertexShader);
			bOK &= FShaderCodeLibrary::ContainsShaderCode(PSO.GraphicsDesc.VertexShader);
			UE_CLOG(!bOK, LogRHI, Verbose, TEXT("Failed to find VertexShader shader: %s"), *(PSO.GraphicsDesc.VertexShader.ToString()));

			if (PSO.GraphicsDesc.GeometryShader != EmptySHA)
			{
				OutRequiredShaders.Add(PSO.GraphicsDesc.GeometryShader);
				bOK &= FShaderCodeLibrary::ContainsShaderCode(PSO.GraphicsDesc.GeometryShader);
				UE_CLOG(!bOK, LogRHI, Verbose, TEXT("Failed to find GeometryShader shader: %s"), *(PSO.GraphicsDesc.GeometryShader.ToString()));
			}
		}
		else
		{
			// if we don't have a vertex shader then we won't bother to add any shaders to the list of outstanding shaders to load
			// Later on this PSO will be killed forever because it is truly bogus.
			UE_LOG(LogRHI, Error, TEXT("PSO Entry has no vertex shader!"));
			bOK = false;
		}

		if (PSO.GraphicsDesc.FragmentShader != EmptySHA)
		{
			OutRequiredShaders.Add(PSO.GraphicsDesc.FragmentShader);
			bOK &= FShaderCodeLibrary::ContainsShaderCode(PSO.GraphicsDesc.FragmentShader);
			UE_CLOG(!bOK, LogRHI, Verbose, TEXT("Failed to find FragmentShader shader: %s"), *(PSO.GraphicsDesc.FragmentShader.ToString()));
		}

		// If everything is OK then we can issue reads of the actual shader code
		PreloadShader(PSO.GraphicsDesc.VertexShader, ReadRequests);
		PreloadShader(PSO.GraphicsDesc.MeshShader, ReadRequests);
		PreloadShader(PSO.GraphicsDesc.AmplificationShader, ReadRequests);
		PreloadShader(PSO.GraphicsDesc.FragmentShader, ReadRequests);
		PreloadShader(PSO.GraphicsDesc.GeometryShader, ReadRequests);
	}
	else if (PSO.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
	{
		if (PSO.ComputeDesc.ComputeShader != EmptySHA)
		{
			OutRequiredShaders.Add(PSO.ComputeDesc.ComputeShader);
			PreloadShader(PSO.ComputeDesc.ComputeShader, ReadRequests);
		}
		else
		{
			bOK = false;
			UE_LOG(LogRHI, Error, TEXT("Invalid PSO entry in pipeline cache!"));
		}
	}
	else if (PSO.Type == FPipelineCacheFileFormatPSO::DescriptorType::RayTracing)
	{
		if (IsRayTracingEnabled())
		{
			if (PSO.RayTracingDesc.ShaderHash != EmptySHA)
			{
				OutRequiredShaders.Add(PSO.RayTracingDesc.ShaderHash);
				PreloadShader(PSO.RayTracingDesc.ShaderHash, ReadRequests);
			}
			else
			{
				bOK = false;
				UE_LOG(LogRHI, Error, TEXT("Invalid PSO entry in pipeline cache!"));
			}
		}
		else
		{
			bOutCompatible = false;
		}
	}
	else
	{
		bOK = false;
		UE_LOG(LogRHI, Error, TEXT("Invalid PSO entry in pipeline cache!"));
	}

	if (!bOK)
	{
		for (const FSHAHash& PreloadedShader : ShadersToUnpreloadInCaseOfFailure)
		{
			FShaderCodeLibrary::ReleasePreloadedShader(PreloadedShader);
		}
		ShadersToUnpreloadInCaseOfFailure.Empty();
	}
	
	bShadersPreloaded = bOK;
	return bShadersPreloaded;
}

void UE::ShaderPipeline::CompileJob::ReleasePreloadedShaders()
{
	if (bShadersPreloaded)
	{
		static FSHAHash EmptySHA;

		auto ReleasePreloadedShader = [](const FSHAHash& ShaderHash)
		{
			if (ShaderHash != EmptySHA)
			{
				FShaderCodeLibrary::ReleasePreloadedShader(ShaderHash);
			}
		};

		if (PSO.Type == FPipelineCacheFileFormatPSO::DescriptorType::Graphics)
		{
			ReleasePreloadedShader(PSO.GraphicsDesc.VertexShader);
			ReleasePreloadedShader(PSO.GraphicsDesc.MeshShader);
			ReleasePreloadedShader(PSO.GraphicsDesc.AmplificationShader);
			ReleasePreloadedShader(PSO.GraphicsDesc.FragmentShader);
			ReleasePreloadedShader(PSO.GraphicsDesc.GeometryShader);
		}
		else if (PSO.Type == FPipelineCacheFileFormatPSO::DescriptorType::Compute)
		{
			ReleasePreloadedShader(PSO.ComputeDesc.ComputeShader);
		}
		else if (PSO.Type == FPipelineCacheFileFormatPSO::DescriptorType::RayTracing)
		{
			if (IsRayTracingEnabled())
			{
				ReleasePreloadedShader(PSO.RayTracingDesc.ShaderHash);
			}
		}

		bShadersPreloaded = true;
	}
}

void FShaderPipelineCacheTask::PreparePipelineBatch(TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>& PipelineBatch)
{
	TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TDoubleLinkedListNode* CurrentNode = PipelineBatch.GetHead();
	while(CurrentNode)
	{
		FPipelineCacheFileFormatPSORead* PSORead = CurrentNode->GetValue();
		check(PSORead);
		FShaderPipelineCacheArchive* Archive = (FShaderPipelineCacheArchive*)(PSORead->Ar);
		check(Archive);
		
		bool bRemoveEntry = false;
		
		if (PSORead->bValid &&
			(PSORead->bReadCompleted || Archive->PollExternalReadDependencies()))
		{
			check(PSORead->bReadCompleted);
		
			FPipelineCacheFileFormatPSO PSO;
			
			FMemoryReader Ar(PSORead->Data);
			Ar.SetGameNetVer(FPipelineCacheFileFormatCurrentVersion);
			Ar << PSO;
			
			// Assume that the shader is present and the PSO can be compiled by default,
			bool bOK = true;
			bool bCompatible = true;
	
            // Shaders required.
            TSet<FSHAHash> RequiredShaders;
			
			UE::ShaderPipeline::CompileJob AsyncJob;
			AsyncJob.PSO = PSO;
			AsyncJob.ReadRequests = new FShaderPipelineCacheArchive;
		
			bOK = AsyncJob.PreloadShaders(RequiredShaders, bCompatible);
			
			// Then if and only if all shaders can be found do we schedule a compile job
			// Otherwise this job needs to be put in the shutdown list to correctly release shader code
			if (bOK && bCompatible)
			{
				ReadTasks.Add(AsyncJob);
			}
			else
			{
				if (RequiredShaders.Num())
				{
					// Re-add to the OrderedCompile tasks and process later
					// We can never know when this PSO might become valid so we can't ever drop it.
					FPipelineCachePSOHeader Hdr;
					Hdr.Hash = PSORead->Hash;
					Hdr.Shaders = RequiredShaders;
					OrderedCompileTasks.Insert(Hdr, 0);
				}
				else if (bCompatible)
				{
					UE_LOG(LogRHI, Error, TEXT("Invalid PSO entry in pipeline cache: %u!"), PSORead->Hash);
				}
				// Go to async shutdown instead - some shader code reads may have been requested - let it handle this
				UE::ShaderPipeline::FShutdownContainer::Get()->AbandonCompileJob(MoveTemp(AsyncJob));
				TotalActiveTasks--;
			}
			
			bRemoveEntry = true;
		}
        else if (!PSORead->bValid)
        {
            UE_LOG(LogRHI, Error, TEXT("Invalid PSO entry in pipeline cache: %u!"), PSORead->Hash);
            
            // Invalid PSOs can be deleted
			TotalActiveTasks--;
            bRemoveEntry = true;
        }
		
		TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TDoubleLinkedListNode* PrevNode = CurrentNode;
		CurrentNode = CurrentNode->GetNextNode();
		if(bRemoveEntry)
		{
			delete PSORead;
			PipelineBatch.RemoveNode(PrevNode);
		}
	}
}


static int32 GMinPipelinePrecompileTasksInFlightThreshold = 10;

static FAutoConsoleVariableRef GPipelinePrecompileTasksInFlightVar(
	TEXT("shaderpipeline.MinPrecompileTasksInFlight"),
	GMinPipelinePrecompileTasksInFlightThreshold,
	TEXT("Note: Only used when threadpool PSO precompiling is active.\n")
	TEXT("The number of active PSO precompile jobs in flight before we will submit another batch of jobs. \n")
	TEXT("i.e. when the number of inflight precompile tasks drops below this threshold we can add the next batch of precompile tasks. \n")
	TEXT("This is to prevent bubbles between precompile batches but also to avoid saturating dispatch.")
	,
	ECVF_RenderThreadSafe
);

bool FShaderPipelineCacheTask::ReadyForPrecompile()
{
	for(uint32 i = 0; i < (uint32)ReadTasks.Num();/*NOP*/)
	{
		check(ReadTasks[i].ReadRequests);
		if (ReadTasks[i].ReadRequests->PollExternalReadDependencies())
		{
			CompileTasks.Add(ReadTasks[i]);
			ReadTasks.RemoveAt(i);
		}
		else
		{
			++i;
		}
	}

	return (CompileTasks.Num() != 0) 
		&& PipelineStateCache::GetNumActivePipelinePrecompileTasks() < GMinPipelinePrecompileTasksInFlightThreshold;
}

void FShaderPipelineCacheTask::PrecompilePipelineBatch()
{
	INC_DWORD_STAT(STAT_PreCompileBatchTotal);
	INC_DWORD_STAT(STAT_PreCompileBatchNum);
	
    int32 NumToPrecompile = FMath::Min<int32>(CompileTasks.Num(), FShaderPipelineCache::BatchSize);

	FShaderPipelineCacheTask* UserCacheTask = FShaderPipelineCache::Get()->GetTask(FShaderPipelineCache::UserCacheTaskKey);

	for(uint32 i = 0; i < (uint32)NumToPrecompile; i++)
	{
		UE::ShaderPipeline::CompileJob& CompileTask = CompileTasks[i];
		
		check(CompileTask.ReadRequests && CompileTask.ReadRequests->PollExternalReadDependencies());
		
		FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();
		
		uint32 PSOHash = GetTypeHash(CompileTask.PSO);
		UE_LOG(LogRHI, Verbose, TEXT("Precompiling PSO %u (type %d) (%d/%d)"), PSOHash, int(CompileTask.PSO.Type), i+1, NumToPrecompile);
		Precompile(RHICmdList, GMaxRHIShaderPlatform, CompileTask.PSO);
		FShaderPipelineCache::Get()->CompiledHashes.Add(PSOHash);

		CompileTask.ReleasePreloadedShaders();
		delete CompileTask.ReadRequests;
		CompileTask.ReadRequests = nullptr;
		
#if STATS
		switch(CompileTask.PSO.Type)
		{
			case FPipelineCacheFileFormatPSO::DescriptorType::Compute:
			{
				INC_DWORD_STAT(STAT_TotalComputePipelineStateCount);
				break;
			}
			case FPipelineCacheFileFormatPSO::DescriptorType::Graphics:
			{
				INC_DWORD_STAT(STAT_TotalGraphicsPipelineStateCount);
				break;
			}
			case FPipelineCacheFileFormatPSO::DescriptorType::RayTracing:
			{
				INC_DWORD_STAT(STAT_TotalRayTracingPipelineStateCount);
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
#endif
	}

	TotalPSOsCompiled += NumToPrecompile;
    TotalActiveTasks -= NumToPrecompile;

	CompileTasks.RemoveAt(0, NumToPrecompile);
}

bool FShaderPipelineCacheTask::ReadyForNextBatch() const
{
	return ReadTasks.Num() == 0;
}

void FShaderPipelineCacheTask::Flush()
{
	// reset everything
	// Abandon all the existing work.
	// This must be done on the render-thread to avoid locks.
	OrderedCompileTasks.Empty();
	
	// Marshall the current Compile Jobs into the shutdown container.
	int32 TotalAbandonedTasks = ReadTasks.Num() + CompileTasks.Num() + FetchTasks.Num();
	UE::ShaderPipeline::FShutdownContainer::Get()->AbandonCompileJobs(MoveTemp(ReadTasks));
	UE::ShaderPipeline::FShutdownContainer::Get()->AbandonCompileJobs(MoveTemp(CompileTasks));
	UE::ShaderPipeline::FShutdownContainer::Get()->AbandonFetchTasks(MoveTemp(FetchTasks));
	TotalActiveTasks -= TotalAbandonedTasks;

	ReadTasks.Empty();
	CompileTasks.Empty();
	FetchTasks.Empty();
	
	TotalWaitingTasks = 0;
	TotalPrecompileTasks = 0;
	TotalCompleteTasks = 0;
	TotalPrecompileTime = 0;
}

static bool PreCompileMaskComparison(uint64 ReferenceGameMask, uint64 PSOMask);

FShaderPipelineCache::FShaderPipelineCache(EShaderPlatform Platform)
: FTickableObjectRenderThread(true, false) // (RegisterNow, HighFrequency)
, bPaused(false)
, PausedCount(0)
 , LastAutoSaveTime(0)
 , LastAutoSaveTimeLogBoundPSO(FPlatformTime::Seconds())
 , LastAutoSaveNum(-1)
{
	SET_DWORD_STAT(STAT_ShaderPipelineTaskCount, 0);
    SET_DWORD_STAT(STAT_ShaderPipelineWaitingTaskCount, 0);
    SET_DWORD_STAT(STAT_ShaderPipelineActiveTaskCount, 0);
	
	int32 Mode = CVarPSOFileCacheStartupMode.GetValueOnAnyThread();
	switch (Mode)
	{
		case 0:
			BatchSize = CVarPSOFileCacheBatchSize.GetValueOnAnyThread();
			BatchTime = CVarPSOFileCacheBatchTime.GetValueOnAnyThread();
			PausedCount = 1;
			bPaused = true;
			UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache: Starting paused. %d"), PausedCount);
			break;
		case 2:
			BatchSize = CVarPSOFileCacheBackgroundBatchSize.GetValueOnAnyThread();
			BatchTime = CVarPSOFileCacheBackgroundBatchTime.GetValueOnAnyThread();
			break;
		case 1:
		default:
			BatchSize = CVarPSOFileCacheBatchSize.GetValueOnAnyThread();
			BatchTime = CVarPSOFileCacheBatchTime.GetValueOnAnyThread();
			break;
	}
	
	FString StableShaderKeyFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("-shkfile="), StableShaderKeyFile) && !StableShaderKeyFile.IsEmpty())
	{
		ShaderHashToStableKey.Initialize(StableShaderKeyFile);
	}

	uint64 PreCompileMask = (uint64)CVarPSOFileCachePreCompileMask.GetValueOnAnyThread();
	FPipelineFileCacheManager::SetGameUsageMaskWithComparison(PreCompileMask, PreCompileMaskComparison);

	FCoreDelegates::ApplicationWillDeactivateDelegate.AddStatic(&PipelineStateCacheOnAppDeactivate);
}

bool FShaderPipelineCache::ReadyForAutoSave() const
{
	bool bAutoSave = false;
	uint32 SaveAfterNum = CVarPSOFileCacheSaveAfterPSOsLogged.GetValueOnAnyThread();
	uint32 NumLogged = FPipelineFileCacheManager::NumPSOsLogged();

	const double TimeSinceSave = FPlatformTime::Seconds() - LastAutoSaveTime;

	// autosave if its enabled, and we have more than the desired number, or it's been a while since our last save
	if (SaveAfterNum > 0 &&
		(NumLogged >= SaveAfterNum || (NumLogged > 0 && TimeSinceSave >= CVarPSOFileCacheAutoSaveTime.GetValueOnAnyThread()))
		)
	{
		bAutoSave = true;
	}
	return bAutoSave;
}

FShaderPipelineCache::~FShaderPipelineCache()
{
	// The render thread tick should be dead now and we are safe to destroy everything that needs to wait or manual destruction

	FScopeLock Lock(&Mutex);
	if (NextPrecompileCacheTask.IsValid() && !NextPrecompileCacheTask->IsComplete())
	{
		NextPrecompileCacheTask->Wait();
	}
	NextPrecompileCacheTask = FGraphEventRef();

	FShaderPipelineCache::CloseUserPipelineFileCache();

	for (auto& PSOTask : ShaderCacheTasks)
	{
		PSOTask.Value->Close();
		// Close() should have called though to Flush() - all work is now in the shutdown lists
	}

	//
	// Clean up cached RHI resources
	//
	for (auto& Pair : BlendStateCache)
	{
		Pair.Value->Release();
	}
	BlendStateCache.Empty();

	for (auto& Pair : RasterizerStateCache)
	{
		Pair.Value->Release();
	}
	RasterizerStateCache.Empty();

	for (auto& Pair : DepthStencilStateCache)
	{
		Pair.Value->Release();
	}
	DepthStencilStateCache.Empty();
	UE::ShaderPipeline::FShutdownContainer::Get()->ShutdownAndWait();
}

 void FShaderPipelineCache::BeginNextPrecompileCacheTask()
{

	 // Mutex is always held here.
	if (!NextPrecompileCacheTask.IsValid() || NextPrecompileCacheTask->IsComplete())
	{	 
		NextPrecompileCacheTask = FGraphEventRef();
		// call begin on the GT, some of the precompile delegate handling is expecting the call to come from the GT.
		// TODO: we should be able to call this from any thread.
		if (IsInGameThread())
		{
			ShaderPipelineCache->BeginNextPrecompileCacheTaskInternal();
		}
		else
		{
			// kick off next PSOFC compile on GT, ensure we only have one request in flight.
			NextPrecompileCacheTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
				[]()
				{
					ShaderPipelineCache->BeginNextPrecompileCacheTaskInternal();
				}
			, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}

void FShaderPipelineCache::ClearPendingPrecompileTaskQueue()
{
	if (!CurrentPrecompilingPSOFCKey.IsEmpty())
	{
		GetTask(CurrentPrecompilingPSOFCKey)->Flush();
		CurrentPrecompilingPSOFCKey.Empty();
	}
	PendingPrecompilePSOCacheKeys.Reset();
	bHasShaderPipelineTask = false;
}

void FShaderPipelineCache::EnqueuePendingPrecompileTask(const FString& PSOCacheKey)
{
	check(!PendingPrecompilePSOCacheKeys.Contains(PSOCacheKey));
	check(ShaderCacheTasks.Contains(PSOCacheKey));
	PendingPrecompilePSOCacheKeys.Add(PSOCacheKey);
	BeginNextPrecompileCacheTask();
}

bool FShaderPipelineCache::IsPrecompiling()
{
	if(ShaderPipelineCache)
	{
		return ShaderPipelineCache->bHasShaderPipelineTask;
	}
	return false;
}

void FShaderPipelineCache::BeginNextPrecompileCacheTaskInternal()
{
	FScopeLock Lock(&Mutex);
	if (!CurrentPrecompilingPSOFCKey.IsEmpty())
	{
		return;
	}
	
	while(!PendingPrecompilePSOCacheKeys.IsEmpty())
	{
		CurrentPrecompilingPSOFCKey = PendingPrecompilePSOCacheKeys[0];
		PendingPrecompilePSOCacheKeys.RemoveAtSwap(0);
		FShaderPipelineCacheTask* NextPrecompileTask = ShaderCacheTasks.Find(CurrentPrecompilingPSOFCKey)->Get();
		check(NextPrecompileTask->bReady);
		UE_LOG(LogRHI, Log, TEXT("FShaderPipelineCache::BeginNextPrecompileCacheTask() - %s begining compile."), *CurrentPrecompilingPSOFCKey);			
		NextPrecompileTask->BeginPrecompilingPipelineCache();

		// FShaderPipelineCache::OnCachePreOpen can set bReady=false, if so we cannot precompile that task.
		if(NextPrecompileTask->bReady)
		{
			bHasShaderPipelineTask = true;
			break;
		}
		else
		{
			UE_LOG(LogRHI, Warning, TEXT("FShaderPipelineCache::BeginNextPrecompileCacheTask() - skipping %s, not ready for precompile."), *CurrentPrecompilingPSOFCKey);
			CurrentPrecompilingPSOFCKey.Empty();
		}
	}

	if (PendingPrecompilePSOCacheKeys.IsEmpty() && CurrentPrecompilingPSOFCKey.IsEmpty())
	{
		// all jobs done.
		UE_LOG(LogRHI, Log, TEXT("FShaderPipelineCache::BeginNextPrecompileCacheTask() - Finished, no jobs remaining."));
		bHasShaderPipelineTask = false;
	}
}

bool FShaderPipelineCache::IsTickable() const
{
	check(IsInRenderingThread());

	return 	FPlatformProperties::RequiresCookedData() && !bPaused &&
		(
			bHasShaderPipelineTask
  			|| ReadyForAutoSave()
 			|| GetShaderPipelineCacheSaveBoundPSOLog()
			|| GetPSOFileCacheSaveUserCache()
			|| UE::ShaderPipeline::FShutdownContainer::Get()->IsTickable()
		);
}

void FShaderPipelineCache::Tick(float DeltaTime)
{
	LLM_SCOPE(ELLMTag::Shaders);
	FScopeLock Lock(&Mutex);

	FShaderPipelineCacheTask* CurrentCacheTask = CurrentPrecompilingPSOFCKey.IsEmpty() ? nullptr : ShaderCacheTasks.Find(CurrentPrecompilingPSOFCKey)->Get();
	const bool bHasActiveCacheTask = CurrentCacheTask != nullptr;

	// HACK: note for the time being until the code further upstream properly uses the OnPrecompilationComplete delegate, we need to provide some delays to make sure it gets the messaging properly (the -10.0f and the -20.0f below).
	const bool bHasCurrentCacheTaskCompleted = bHasActiveCacheTask &&
		PipelineStateCache::GetNumActivePipelinePrecompileTasks() == 0 &&
				(
				FMath::Max((int64_t)0, FShaderPipelineCacheTask::TotalWaitingTasks.load()) == 0 &&
				FMath::Max((int64_t)0, FShaderPipelineCacheTask::TotalActiveTasks.load()) == 0 &&
				(FShaderPipelineCacheTask::TotalPrecompileTasks == 0 || FShaderPipelineCacheTask::TotalCompleteTasks != 0)
				);

	if (!bHasActiveCacheTask || bHasCurrentCacheTaskCompleted )
	{
		// drop to background mode if CVarPSOFileCacheMaxPrecompileTime exceeded.
		if (CVarPSOFileCacheMaxPrecompileTime.GetValueOnAnyThread() > 0.0 && FShaderPipelineCacheTask::TotalPrecompileWallTime - 20.0f > CVarPSOFileCacheMaxPrecompileTime.GetValueOnAnyThread() && FShaderPipelineCacheTask::TotalPrecompileTasks > 0)
 		{
 			SetBatchMode(BatchMode::Background);
 		}

		if (bHasCurrentCacheTaskCompleted)
		{
			UE_LOG(LogRHI, Warning, TEXT("FShaderPipelineCache %s completed %u tasks in %.2fs (%.2fs wall time since intial open)."), *CurrentCacheTask->PSOCacheKey, (uint32)FShaderPipelineCacheTask::TotalCompleteTasks, FPlatformTime::ToSeconds64(FShaderPipelineCacheTask::TotalPrecompileTime), FShaderPipelineCacheTask::TotalPrecompileWallTime);
			if (OnPrecompilationComplete.IsBound())
			{
				OnPrecompilationComplete.Broadcast((uint32)FShaderPipelineCacheTask::TotalCompleteTasks, FPlatformTime::ToSeconds64(FShaderPipelineCacheTask::TotalPrecompileTime), CurrentCacheTask->PrecompileContext);
			}

			FPipelineFileCacheManager::PreCompileComplete();
			PipelineStateCache::PreCompileComplete();

			FShaderPipelineCacheTask::PrecompileStartTime = 0.0;
			FShaderPipelineCacheTask::TotalPrecompileTasks = 0;
			FShaderPipelineCacheTask::TotalCompleteTasks = 0;
			FShaderPipelineCacheTask::TotalPrecompileTime = 0;
			CurrentCacheTask->bPreOptimizing = false;

			CurrentPrecompilingPSOFCKey.Empty();
			CurrentCacheTask = nullptr;
			BeginNextPrecompileCacheTask();
		}
	}

	if (ReadyForAutoSave())
	{
		if (GetPSOFileCacheSaveUserCache())
		{
			SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::Incremental);
		}
	}

	if (GetShaderPipelineCacheSaveBoundPSOLog())
	{
		if (LastAutoSaveNum < int32(FPipelineFileCacheManager::NumPSOsLogged()))
		{
			const double TimeSinceSave = FPlatformTime::Seconds() - LastAutoSaveTimeLogBoundPSO;

			if (TimeSinceSave >= CVarPSOFileCacheAutoSaveTimeBoundPSO.GetValueOnAnyThread())
			{
				SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::BoundPSOsOnly);
				LastAutoSaveTimeLogBoundPSO = FPlatformTime::Seconds();
				LastAutoSaveNum = FPipelineFileCacheManager::NumPSOsLogged();
			}
		}
	}

	UE::ShaderPipeline::FShutdownContainer::Get()->Tick();

	if(CurrentCacheTask)
	{
		CurrentCacheTask->Tick(DeltaTime);
	}
}

void FShaderPipelineCacheTask::Tick(float DeltaTime)
{
	if (PrecompileStartTime > 0.0)
	{
		TotalPrecompileWallTime = float(FPlatformTime::Seconds() - PrecompileStartTime);
	}
	
	if (PrecompileStartTime == 0.0 && (PreFetchedTasks.Num() || FetchTasks.Num() || OrderedCompileTasks.Num()))
	{
		PrecompileStartTime = FPlatformTime::Seconds();
	}
	
	// Copy any new items over to our 'internal' safe array
	if (PreFetchedTasks.Num())
	{
		OrderedCompileTasks.Append(PreFetchedTasks);
		PreFetchedTasks.Empty();
	}
	
	if (ReadyForPrecompile())
	{
		SCOPE_SECONDS_ACCUMULATOR(STAT_PreCompileTotalTime);
		SCOPE_CYCLE_COUNTER(STAT_PreCompileTime);
		
		uint32 Start = FPlatformTime::Cycles();

		PrecompilePipelineBatch();

		uint32 End = FPlatformTime::Cycles();

		if (FShaderPipelineCache::BatchTime > 0.0f)
		{
			float ElapsedMs = FPlatformTime::ToMilliseconds(End - Start);
			if (ElapsedMs < FShaderPipelineCache::BatchTime)
			{
				FShaderPipelineCache::BatchSize++;
			}
			else if (ElapsedMs > FShaderPipelineCache::BatchTime)
			{
				if (FShaderPipelineCache::BatchSize > 1)
				{
					FShaderPipelineCache::BatchSize--;
				}
				else
				{
					UE_LOG(LogRHI, Warning, TEXT("FShaderPipelineCache: Cannot reduce BatchSize below 1 to meet target of %f ms, elapsed time was %f ms)"), FShaderPipelineCache::BatchTime, ElapsedMs);
				}
			}
		}
	}
	
	if (ReadyForNextBatch() && (OrderedCompileTasks.Num() || FetchTasks.Num()))
	{
        uint32 Num = 0;
        if (FShaderPipelineCache::BatchSize > static_cast<uint32>(FetchTasks.Num()))
        {
            Num = FShaderPipelineCache::BatchSize - FetchTasks.Num();
        }
        Num = FMath::Min(Num, (uint32)OrderedCompileTasks.Num());
            
		if (FetchTasks.Num() < (int32)Num)
		{
			TDoubleLinkedList<FPipelineCacheFileFormatPSORead*> NewBatch;
		
			Num -= FetchTasks.Num();
            for (auto Iterator = OrderedCompileTasks.CreateIterator(); Iterator && Num > 0; ++Iterator)
            {
                bool bHasShaders = true;
                for (FSHAHash const& Hash : Iterator->Shaders)
                {
                    bHasShaders &= FShaderCodeLibrary::ContainsShaderCode(Hash);
                }
            	if (bHasShaders)
            	{
            		FPipelineCacheFileFormatPSORead* Entry = new FPipelineCacheFileFormatPSORead;
					Entry->Hash = Iterator->Hash;
					Entry->Ar = new FShaderPipelineCacheArchive;
					
					// Add to both new batch and fetch lists
					NewBatch.AddTail(Entry);
					FetchTasks.AddTail(Entry);
					
	            	Iterator.RemoveCurrent();
                    TotalActiveTasks++;
                    TotalWaitingTasks--;
                    --Num;
            	}
            }
			
			FPipelineFileCacheManager::FetchPSODescriptors(PSOCacheKey, NewBatch);
		}

        if (static_cast<uint32>(FetchTasks.Num()) > FShaderPipelineCache::BatchSize)
        {
            UE_LOG(LogRHI, Warning, TEXT("FShaderPipelineCache: Attempting to pre-compile more jobs (%d) than the batch size (%d)"), FetchTasks.Num(), FShaderPipelineCache::BatchSize);
        }
        
		PreparePipelineBatch(FetchTasks);
	}
	
	if(CVarPSOFileCacheGameFileMaskEnabled.GetValueOnAnyThread())
	{
		if(TotalActiveTasks + TotalWaitingTasks == 0)
		{
			const uint64 Mask = FPipelineFileCacheManager::GetGameUsageMask();
			bool bAlreadyInSet = false;
			CompletedMasks.Add(Mask, &bAlreadyInSet);
			if(!bAlreadyInSet)
			{
				UE_LOG(LogRHI, Display, TEXT("ShaderPipelineCache: GameUsageMask [%llu] precompile complete."), Mask);
			}
		}
	}
	
#if STATS
	int64 ActiveTaskCount = FMath::Max((int64_t)0, TotalActiveTasks.load());
    int64 WaitingTaskCount = FMath::Max((int64_t)0, TotalWaitingTasks.load());
	SET_DWORD_STAT(STAT_ShaderPipelineTaskCount, ActiveTaskCount+WaitingTaskCount);
    SET_DWORD_STAT(STAT_ShaderPipelineWaitingTaskCount, WaitingTaskCount);
    SET_DWORD_STAT(STAT_ShaderPipelineActiveTaskCount, ActiveTaskCount);
	
	using UE::ShaderPipeline::FShutdownContainer;

	// Calc in one place otherwise this computation will be splattered all over the place - this will not be exact but counts the expensive bits
	int64 InUseMemory = OrderedCompileTasks.GetAllocatedSize() + 
						ReadTasks.GetAllocatedSize() + 
						CompileTasks.GetAllocatedSize() + 
						FShaderPipelineCache::Get()->CompiledHashes.GetAllocatedSize() +
						FShutdownContainer::Get()->ShutdownReadCompileTasks.GetAllocatedSize();
	if(ActiveTaskCount+WaitingTaskCount > 0)
	{
		InUseMemory += (ReadTasks.Num() + CompileTasks.Num() + FShutdownContainer::Get()->ShutdownReadCompileTasks.Num()) * (sizeof(FShaderPipelineCacheArchive));
		InUseMemory += (FetchTasks.Num() + FShutdownContainer::Get()->ShutdownFetchTasks.Num()) * (sizeof(FPipelineCacheFileFormatPSORead));
		for (TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TIterator It(FetchTasks.GetHead()); It; ++It)
		{
			FPipelineCacheFileFormatPSORead* Entry = *It;
			InUseMemory += Entry->Data.Num();
		}
		for (TDoubleLinkedList<FPipelineCacheFileFormatPSORead*>::TIterator It(FShutdownContainer::Get()->ShutdownFetchTasks.GetHead()); It; ++It)
		{
			FPipelineCacheFileFormatPSORead* Entry = *It;
			InUseMemory += Entry->Data.Num();
		}
	}
	SET_MEMORY_STAT(STAT_PreCompileMemory, InUseMemory);
#endif
}

bool FShaderPipelineCache::NeedsRenderingResumedForRenderingThreadTick() const
{
	return true;
}

TStatId FShaderPipelineCache::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FShaderPipelineBatchCompiler, STATGROUP_Tickables);
}

// Not sure where the define is for this but most seem to be low, medium, high, epic, cinema, auto, except material quality but that's less anyway
static const int32 MaxQualityCount = 6;
static const int32 MaxPlaylistCount = 3;
static const int32 MaxUserCount = 16;

static bool PreCompileMaskComparison(uint64 ReferenceGameMask, uint64 PSOMask)
{
	// If game mask use is disabled then the precompile comparison should succeed.
	const bool bIgnoreGameMask = CVarPSOFileCacheGameFileMaskEnabled.GetValueOnAnyThread() == 0;

	uint64 UsageMask = (ReferenceGameMask & PSOMask);
	return bIgnoreGameMask || ((UsageMask & (7l << (MaxQualityCount*3+MaxPlaylistCount))) && (UsageMask & (7 << (MaxQualityCount*3))) && (UsageMask & (63 << MaxQualityCount*2)) && (UsageMask & (63 << MaxQualityCount)) && (UsageMask & 63));
}

bool FShaderPipelineCacheTask::Open(const FString& Key, FString const& Name, EShaderPlatform Platform)
{
	PSOCacheKey = Key;
	ShaderPlatform = Platform;
	PrecompileContext = FShaderPipelineCache::FShaderCachePrecompileContext(Name);

	if (bIsUserCache)
	{
		return FPipelineFileCacheManager::OpenUserPipelineFileCache(Key, Name, Platform, CacheFileGuid);
	}
	else
	{
		return FPipelineFileCacheManager::OpenPipelineFileCache(Key, Name, Platform, CacheFileGuid);
	}
}

void FShaderPipelineCacheTask::BeginPrecompilingPipelineCache()
{
	Flush();

	FShaderPipelineCacheTask::TotalPrecompileWallTime = 0.0f;
	FShaderPipelineCacheTask::PrecompileStartTime = 0.0;
	FShaderPipelineCacheTask::TotalPrecompileTasks = 0;

	// these should have been zeroed when the previous task finished.
	check(FShaderPipelineCacheTask::TotalActiveTasks == 0);
	check(FShaderPipelineCacheTask::TotalWaitingTasks == 0);
	check(FShaderPipelineCacheTask::TotalCompleteTasks == 0);
	check(FShaderPipelineCacheTask::TotalPrecompileTime == 0);

	if (FShaderPipelineCache::OnCachePreOpen.IsBound())
	{
		FShaderPipelineCache::OnCachePreOpen.Broadcast(PSOCacheKey, ShaderPlatform, bReady);
	}
		
	if(bReady)
	{
		int32 Order = (int32)FPipelineFileCacheManager::PSOOrder::Default;

		if (!GConfig->GetInt(FShaderPipelineCacheConstants::SectionHeading, FShaderPipelineCacheConstants::SortOrderKey, Order, *GGameUserSettingsIni))
		{
			GConfig->GetInt(FShaderPipelineCacheConstants::SectionHeading, FShaderPipelineCacheConstants::SortOrderKey, Order, *GGameIni);
		}

		TArray<FPipelineCachePSOHeader> LocalPreFetchedTasks;

		FPipelineFileCacheManager::GetOrderedPSOHashes(PSOCacheKey, LocalPreFetchedTasks, (FPipelineFileCacheManager::PSOOrder)Order, (int64)CVarPSOFileCacheMinBindCount.GetValueOnAnyThread(), FShaderPipelineCache::Get()->CompiledHashes);
		// Iterate over all the tasks we haven't yet begun to read data for - these are the 'waiting' tasks
		int64 EligibleTaskCount = LocalPreFetchedTasks.Num();

		// Only interested in global shaders when PSO precaching is enabled
		FString LibraryName(TEXT("Global"));

		int32 MissingShaders = 0;
		int32 NewSize = Algo::StableRemoveIf(LocalPreFetchedTasks, [&MissingShaders, LibraryName](FPipelineCachePSOHeader& Task)
		{
			bool bHasShaders = true;
			for (FSHAHash const& Hash : Task.Shaders)
			{
				if (PipelineStateCache::IsPSOPrecachingEnabled() && CVarPSOGlobalShadersOnlyWhenPSOPrecaching.GetValueOnAnyThread() > 0)
				{
					bHasShaders &= FShaderCodeLibrary::ContainsShaderCode(Hash, LibraryName);
				}
				else
				{
					bHasShaders &= FShaderCodeLibrary::ContainsShaderCode(Hash);
				}
			}
			if(!bHasShaders)
			{
				MissingShaders++;
			}
			return !bHasShaders;
		});

		LocalPreFetchedTasks.SetNum(NewSize);
		int64 PossibleTasks = LocalPreFetchedTasks.Num();
		TotalWaitingTasks = PossibleTasks;

		if (FShaderPipelineCache::OnCachedOpened.IsBound())
		{
			FShaderPipelineCache::OnCachedOpened.Broadcast(PSOCacheKey, ShaderPlatform, LocalPreFetchedTasks.Num(), CacheFileGuid, PrecompileContext);
		}

		// Copy any new items over to our 'internal' safe array
		check(PreFetchedTasks.IsEmpty());
		check(OrderedCompileTasks.IsEmpty());

		OrderedCompileTasks = MoveTemp(LocalPreFetchedTasks);

		TotalPrecompileTasks = OrderedCompileTasks.Num();

			
		bPreOptimizing = TotalPrecompileTasks > 0;
		int32 TotalPSOCount = FPipelineFileCacheManager::GetTotalPSOCount(PSOCacheKey);
		UE_LOG(LogRHI, Display, TEXT("FShaderPipelineCache starting pipeline cache '%s' and enqueued %d tasks for precompile. (cache contains %d, %d eligible, %d had missing shaders. %d already compiled). BatchSize %d and BatchTime %f."),
			*PSOCacheKey, TotalPrecompileTasks, TotalPSOCount, EligibleTaskCount, MissingShaders, TotalPSOsCompiled, FShaderPipelineCache::BatchSize, FShaderPipelineCache::BatchTime);

		if (FShaderPipelineCache::OnPrecompilationBegin.IsBound())
		{
			FShaderPipelineCache::OnPrecompilationBegin.Broadcast(TotalPrecompileTasks, PrecompileContext);
		}
	}
	else
	{
		UE_LOG(LogRHI, Display, TEXT("FShaderPipelineCache pipeline cache %s - precompile deferred on UsageMask."), *PSOCacheKey);
	}
}

void FShaderPipelineCacheTask::Close()
{
	// Signal flush of outstanding work to allow restarting for a new cache file
	Flush();
    
    if (FShaderPipelineCache::OnCachedClosed.IsBound())
    {
		FShaderPipelineCache::OnCachedClosed.Broadcast(PSOCacheKey, ShaderPlatform);
    }
	
	bOpened = false;

	FPipelineFileCacheManager::CloseUserPipelineFileCache();
}

bool FShaderPipelineCacheTask::OnShaderLibraryStateChanged(FShaderPipelineCache::ELibraryState State, EShaderPlatform Platform, FString const& Name, int32 ComponentID)
{
	check(!bIsUserCache);
	if (State == FShaderPipelineCache::Opened && Name == FApp::GetProjectName() && Platform == ShaderPlatform && !bOpened)
	{
		Close();
	}
	else if(State == FShaderPipelineCache::OpenedComponent)
	{
		check(ComponentID != INDEX_NONE);
	}

	if (ensure(!Name.IsEmpty()))
	{
		// for bundled files the key == name
		const FString& Key = Name;
		bool bDidOpen = Open(Key, Name, Platform);
		UE_LOG(LogRHI, Display, TEXT("Shader library state change %d, pipeline cache %s was opened=%d"), State, *Name, bDidOpen);
		return bDidOpen;
	}

	return false;
}

FRHIBlendState* FShaderPipelineCache::GetOrCreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	FRHIBlendState** Found = BlendStateCache.Find(Initializer);
	if (Found)
	{
		return *Found;
	}
	
	FBlendStateRHIRef NewState = RHICreateBlendState(Initializer);
	
	// Add an extra reference so we don't have TRefCountPtr in the maps
	NewState->AddRef();
	BlendStateCache.Add(Initializer, NewState);
	return NewState;
}

FRHIRasterizerState* FShaderPipelineCache::GetOrCreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FRHIRasterizerState** Found = RasterizerStateCache.Find(Initializer);
	if (Found)
	{
		return *Found;
	}
	
	FRasterizerStateRHIRef NewState = RHICreateRasterizerState(Initializer);
	
	// Add an extra reference so we don't have TRefCountPtr in the maps
	NewState->AddRef();
	RasterizerStateCache.Add(Initializer, NewState);
	return NewState;
}

FRHIDepthStencilState* FShaderPipelineCache::GetOrCreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	FRHIDepthStencilState** Found = DepthStencilStateCache.Find(Initializer);
	if (Found)
	{
		return *Found;
	}
	
	FDepthStencilStateRHIRef NewState = RHICreateDepthStencilState(Initializer);
	
	// Add an extra reference so we don't have TRefCountPtr in the maps
	NewState->AddRef();
	DepthStencilStateCache.Add(Initializer, NewState);
	return NewState;
}
