// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PipelineStateCache.cpp: Pipeline state cache implementation.
=============================================================================*/

#include "PipelineStateCache.h"
#include "Async/AsyncWork.h"
#include "Async/TaskGraphInterfaces.h"
#include "PipelineFileCache.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/App.h"
#include "Misc/TimeGuard.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHICommandList.h"
#include "RHIFwd.h"
#include "RHIImmutableSamplerState.h"
#include "Stats/StatsTrace.h"
#include "Templates/TypeHash.h"

// 5.4.2 local change to avoid modifying public headers
namespace PipelineStateCache
{
	// Waits for any pending tasks to complete.
	extern RHI_API void WaitForAllTasks();

}

// perform cache eviction each frame, used to stress the system and flush out bugs
#define PSO_DO_CACHE_EVICT_EACH_FRAME 0

// Log event and info about cache eviction
#define PSO_LOG_CACHE_EVICT 0

// Stat tracking
#define PSO_TRACK_CACHE_STATS 0

#define PIPELINESTATECACHE_VERIFYTHREADSAFE (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

CSV_DECLARE_CATEGORY_EXTERN(PSO);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Runtime Graphics PSO Hitch Count"), STAT_RuntimeGraphicsPSOHitchCount, STATGROUP_PipelineStateCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Runtime Compute PSO Hitch Count"), STAT_RuntimeComputePSOHitchCount, STATGROUP_PipelineStateCache);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Active Graphics PSO Precache Requests"), STAT_ActiveGraphicsPSOPrecacheRequests, STATGROUP_PipelineStateCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Active Compute PSO Precache Requests"), STAT_ActiveComputePSOPrecacheRequests, STATGROUP_PipelineStateCache);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("High Priority Graphics PSO Precache Requests"), STAT_HighPriorityGraphicsPSOPrecacheRequests, STATGROUP_PipelineStateCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("High Priority Compute PSO Precache Requests"), STAT_HighPriorityComputePSOPrecacheRequests, STATGROUP_PipelineStateCache);

static inline uint32 GetTypeHash(const FBoundShaderStateInput& Input)
{
	uint32 Hash = GetTypeHash(Input.VertexDeclarationRHI);
	Hash = HashCombineFast(Hash, GetTypeHash(Input.VertexShaderRHI));
	Hash = HashCombineFast(Hash, GetTypeHash(Input.PixelShaderRHI));
#if PLATFORM_SUPPORTS_MESH_SHADERS
	Hash = HashCombineFast(Hash, GetTypeHash(Input.GetMeshShader()));
	Hash = HashCombineFast(Hash, GetTypeHash(Input.GetAmplificationShader()));
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	Hash = HashCombineFast(Hash, GetTypeHash(Input.GetGeometryShader()));
#endif
	return Hash;
}

static inline uint32 GetTypeHash(const FImmutableSamplerState& Iss)
{
	return GetTypeHash(Iss.ImmutableSamplers);
}

inline uint32 GetTypeHash(const FExclusiveDepthStencil& Ds)
{
	return GetTypeHash(Ds.Value);
}

static inline uint32 GetTypeHash(const FGraphicsPipelineStateInitializer& Initializer)
{
	uint32 Hash = GetTypeHash(Initializer.BoundShaderState);
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.BlendState));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.RasterizerState));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.DepthStencilState));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.ImmutableSamplerState));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.PrimitiveType));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.RenderTargetsEnabled));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.RenderTargetFormats));
	for (int32 Index = 0; Index < Initializer.RenderTargetFlags.Num(); ++Index)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Initializer.RenderTargetFlags[Index] & FGraphicsPipelineStateInitializer::RelevantRenderTargetFlagMask));
	}
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.DepthStencilTargetFormat));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.DepthStencilTargetFlag & FGraphicsPipelineStateInitializer::RelevantDepthStencilFlagMask));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.DepthTargetLoadAction));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.DepthTargetStoreAction));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.StencilTargetLoadAction));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.StencilTargetStoreAction));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.DepthStencilAccess));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.NumSamples));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.SubpassHint));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.SubpassIndex));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.ConservativeRasterization));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.bDepthBounds));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.MultiViewCount));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.bHasFragmentDensityAttachment));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.bAllowVariableRateShading));
	Hash = HashCombineFast(Hash, GetTypeHash(Initializer.ShadingRate));
	return Hash;
}

constexpr int32 PSO_MISS_FRAME_HISTORY_SIZE = 3;
static TAtomic<uint32> GraphicsPipelineCacheMisses;
static TArray<uint32> GraphicsPipelineCacheMissesHistory;
static TAtomic<uint32> ComputePipelineCacheMisses;
static TArray<uint32> ComputePipelineCacheMissesHistory;
static bool	ReportFrameHitchThisFrame;

enum class EPSOCompileAsyncMode
{
	None = 0,
	All = 1,
	Precompile = 2,
	NonPrecompiled = 3,
};

static TAutoConsoleVariable<int32> GCVarAsyncPipelineCompile(
	TEXT("r.AsyncPipelineCompile"),
	(int32)EPSOCompileAsyncMode::All,
	TEXT("0 to Create PSOs at the moment they are requested\n")
	TEXT("1 to Create Pipeline State Objects asynchronously(default)\n")
	TEXT("2 to Create Only precompile PSOs asynchronously\n")
	TEXT("3 to Create Only non-precompile PSOs asynchronously")
	,
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

bool GRunPSOCreateTasksOnRHIT = false;
static FAutoConsoleVariableRef CVarCreatePSOsOnRHIThread(
	TEXT("r.pso.CreateOnRHIThread"),
	GRunPSOCreateTasksOnRHIT,
	TEXT("0: Run PSO creation on task threads\n")
	TEXT("1: Run PSO creation on RHI thread."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarPSOEvictionTime(
	TEXT("r.pso.evictiontime"),
	60,
	TEXT("Time between checks to remove stale objects from the cache. 0 = no eviction (which may eventually OOM...)"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarPSORuntimeCreationHitchThreshold(
	TEXT("r.PSO.RuntimeCreationHitchThreshold"),
	20,
	TEXT("Threshold for runtime PSO creation to count as a hitch (in msec) (default 20)"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

#if RHI_RAYTRACING
static TAutoConsoleVariable<int32> CVarRTPSOCacheSize(
	TEXT("r.RayTracing.PSOCacheSize"),
	50,
	TEXT("Number of ray tracing pipelines to keep in the cache (default = 50). Set to 0 to disable eviction.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);
#endif // RHI_RAYTRACING

int32 GPSOPrecaching = 1;
static FAutoConsoleVariableRef CVarPSOPrecaching(
	TEXT("r.PSOPrecaching"),
	GPSOPrecaching,
	TEXT("0 to Disable PSOs precaching\n")
	TEXT("1 to Enable PSO precaching\n"),
	ECVF_Default
);

int32 GPSOWaitForHighPriorityRequestsOnly = 0;
static FAutoConsoleVariableRef CVarPSOWaitForHighPriorityRequestsOnly(
	TEXT("r.PSOPrecaching.WaitForHighPriorityRequestsOnly"),
	GPSOWaitForHighPriorityRequestsOnly,
	TEXT("0 to wait for all pending PSO precache requests during loading (default)g\n")
	TEXT("1 to only wait for the high priority PSO precache requests during loading\n"),
	ECVF_Default
);

extern void DumpPipelineCacheStats();

static FAutoConsoleCommand DumpPipelineCmd(
	TEXT("r.DumpPipelineCache"),
	TEXT("Dump current cache stats."),
	FConsoleCommandDelegate::CreateStatic(DumpPipelineCacheStats)
);

static inline void CheckAndUpdateHitchCountStat(FPSOPrecacheRequestID::EType PSOType, bool bIsRuntimePSO, uint64 StartTime)
{
	if (bIsRuntimePSO)
	{
		int32 RuntimePSOCreationHitchThreshold = CVarPSORuntimeCreationHitchThreshold.GetValueOnAnyThread();
		double PSOCreationTimeMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
		if (PSOCreationTimeMs > RuntimePSOCreationHitchThreshold)
		{
			if (PSOType == FPSOPrecacheRequestID::EType::Graphics)
			{
				INC_DWORD_STAT(STAT_RuntimeGraphicsPSOHitchCount);
			}
			else if (PSOType == FPSOPrecacheRequestID::EType::Compute)
			{
				INC_DWORD_STAT(STAT_RuntimeComputePSOHitchCount)
			}
		}
	}
}

static int32 GPSOPrecompileThreadPoolSize = 0;
static FAutoConsoleVariableRef GPSOPrecompileThreadPoolSizeVar(
	TEXT("r.pso.PrecompileThreadPoolSize"),
	GPSOPrecompileThreadPoolSize,
	TEXT("The number of threads available for concurrent PSO Precompiling.\n")
	TEXT("0 to disable threadpool usage when precompiling PSOs. (default)")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static int32 GPSOPrecompileThreadPoolPercentOfHardwareThreads = 75;
static FAutoConsoleVariableRef GPSOPrecompileThreadPoolPercentOfHardwareThreadsVar(
	TEXT("r.pso.PrecompileThreadPoolPercentOfHardwareThreads"),
	GPSOPrecompileThreadPoolPercentOfHardwareThreads,
	TEXT("If > 0, use this percentage of cores (rounded up) for the PSO precompile thread pool\n")
	TEXT("Use this as an alternative to r.pso.PrecompileThreadPoolSize\n")
	TEXT("0 to disable threadpool usage when precompiling PSOs. (default 75%)")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static int32 GPSOPrecompileThreadPoolSizeMin = 2;
static FAutoConsoleVariableRef GPSOPrecompileThreadPoolSizeMinVar(
	TEXT("r.pso.PrecompileThreadPoolSizeMin"),
	GPSOPrecompileThreadPoolSizeMin,
	TEXT("The minimum number of threads available for concurrent PSO Precompiling.\n")
	TEXT("Ignored unless r.pso.PrecompileThreadPoolPercentOfHardwareThreads is specified\n")
	TEXT("0 = no minimum (default 2)")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static int32 GPSOPrecompileThreadPoolSizeMax = INT_MAX;
static FAutoConsoleVariableRef GPSOPrecompileThreadPoolSizeMaxVar(
	TEXT("r.pso.PrecompileThreadPoolSizeMax"),
	GPSOPrecompileThreadPoolSizeMax,
	TEXT("The maximum number of threads available for concurrent PSO Precompiling.\n")
	TEXT("Ignored unless r.pso.PrecompileThreadPoolPercentOfHardwareThreads is specified\n")
	TEXT("Default is no maximum (INT_MAX)")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

int32 GPSOPrecompileThreadPoolThreadPriority = (int32)EThreadPriority::TPri_BelowNormal;
static FAutoConsoleVariableRef CVarStreamingTextureIOPriority(
	TEXT("r.pso.PrecompileThreadPoolThreadPriority"),
	GPSOPrecompileThreadPoolThreadPriority,
	TEXT("Thread priority for the PSO precompile pool"),
	ECVF_RenderThreadSafe);


class FPSOPrecacheThreadPool
{
public:
	~FPSOPrecacheThreadPool()
	{
		// Thread pool needs to be shutdown before the global object is deleted
		check(PSOPrecompileCompileThreadPool.load() == nullptr);
	}

	FQueuedThreadPool& Get()
	{
		if (PSOPrecompileCompileThreadPool.load() == nullptr)
		{
			FScopeLock lock(&LockCS);
			if (PSOPrecompileCompileThreadPool.load() == nullptr)
			{
				check(UsePool());

				FQueuedThreadPool* PSOPrecompileCompileThreadPoolLocal = FQueuedThreadPool::Allocate();
				PSOPrecompileCompileThreadPoolLocal->Create(GetDesiredPoolSize(), 512 * 1024, (EThreadPriority)GPSOPrecompileThreadPoolThreadPriority, TEXT("PSOPrecompilePool"));
				PSOPrecompileCompileThreadPool = PSOPrecompileCompileThreadPoolLocal;
			}
		}
		return *PSOPrecompileCompileThreadPool;
	}

	void ShutdownThreadPool()
	{
		FQueuedThreadPool* LocalPool = PSOPrecompileCompileThreadPool.exchange(nullptr);
		if (LocalPool != nullptr)
		{

			LocalPool->Destroy();
			delete LocalPool;
		}
	}

	static int32 GetDesiredPoolSize()
	{
		if (GPSOPrecompileThreadPoolSize > 0)
		{
			ensure(GPSOPrecompileThreadPoolPercentOfHardwareThreads == 0); // These settings are mutually exclusive
			return GPSOPrecompileThreadPoolSize;
		}
		if (GPSOPrecompileThreadPoolPercentOfHardwareThreads > 0)
		{
			int32 NumThreads = FMath::CeilToInt((float)FPlatformMisc::NumberOfCoresIncludingHyperthreads() * (float)GPSOPrecompileThreadPoolPercentOfHardwareThreads / 100.0f);
			return FMath::Clamp(NumThreads, GPSOPrecompileThreadPoolSizeMin, GPSOPrecompileThreadPoolSizeMax);
		}
		return 0;
	}

	static bool UsePool()
	{
		return GetDesiredPoolSize() > 0;
	}

private:
	FCriticalSection LockCS;
	std::atomic<FQueuedThreadPool*> PSOPrecompileCompileThreadPool {};
};

static FPSOPrecacheThreadPool GPSOPrecacheThreadPool;

void PipelineStateCache::PreCompileComplete()
{
	// free up our threads when the precompile completes and don't have precaching enabled (otherwise the thread are used during gameplay as well)
	if (!PipelineStateCache::IsPSOPrecachingEnabled())
	{
		GPSOPrecacheThreadPool.ShutdownThreadPool();
	}
}

extern RHI_API FComputePipelineState* FindComputePipelineState(FRHIComputeShader* ComputeShader, bool bVerifyUse);
extern RHI_API FComputePipelineState* GetComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader);
extern RHI_API FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
extern RHI_API FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(FGraphicsPipelineState* GraphicsPipelineState);

// Prints out information about a failed compilation from Init.
// This is fatal unless the compilation request is coming from the precaching system.
static void HandlePipelineCreationFailure(const FGraphicsPipelineStateInitializer& Init)
{
	FSHA1 PipelineHasher;
	FString ShaderHashList;

	const auto AddShaderHash = [&PipelineHasher, &ShaderHashList](const FRHIShader* Shader)
	{
		FSHAHash ShaderHash;
		if (Shader)
		{
			ShaderHash = Shader->GetHash();
			ShaderHashList.Appendf(TEXT("%s: %s, "), GetShaderFrequencyString(Shader->GetFrequency(), false), *ShaderHash.ToString());
		}
		PipelineHasher.Update(&ShaderHash.Hash[0], sizeof(FSHAHash));
	};

	// Log the shader and pipeline hashes, so we can look them up in the stable keys (SHK) file. Please note that NeedsShaderStableKeys must be set to
	// true in the [DevOptions.Shaders] section of *Engine.ini in order for the cook process to produce SHK files for the shader libraries. The contents
	// of those files can be extracted as text using the ShaderPipelineCacheTools commandlet, like this:
	//		UnrealEditor-Cmd.exe ProjectName -run=ShaderPipelineCacheTools dump File.shk
	// The pipeline hash is created by hashing together the individual shader hashes, see FShaderCodeLibraryPipeline::GetPipelineHash for details.
	AddShaderHash(Init.BoundShaderState.GetVertexShader());
	AddShaderHash(Init.BoundShaderState.GetMeshShader());
	AddShaderHash(Init.BoundShaderState.GetAmplificationShader());
	AddShaderHash(Init.BoundShaderState.GetPixelShader());
	AddShaderHash(Init.BoundShaderState.GetGeometryShader());

	PipelineHasher.Final();
	FSHAHash PipelineHash;
	PipelineHasher.GetHash(&PipelineHash.Hash[0]);

	UE_LOG(LogRHI, Error, TEXT("Failed to create graphics pipeline, hashes: %sPipeline: %s."), *ShaderHashList, *PipelineHash.ToString());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(Init.BoundShaderState.VertexShaderRHI)
	{
		UE_LOG(LogRHI, Error, TEXT("Vertex: %s"), Init.BoundShaderState.VertexShaderRHI->GetShaderName());
	}
	if (Init.BoundShaderState.GetMeshShader())
	{
		UE_LOG(LogRHI, Error, TEXT("Mesh: %s"), Init.BoundShaderState.GetMeshShader()->GetShaderName());
	}
	if (Init.BoundShaderState.GetAmplificationShader())
	{
		UE_LOG(LogRHI, Error, TEXT("Amplification: %s"), Init.BoundShaderState.GetAmplificationShader()->GetShaderName());
	}
	if(Init.BoundShaderState.GetGeometryShader())
	{
		UE_LOG(LogRHI, Error, TEXT("Geometry: %s"), Init.BoundShaderState.GetGeometryShader()->GetShaderName());
	}
	if(Init.BoundShaderState.PixelShaderRHI)
	{
		UE_LOG(LogRHI, Error, TEXT("Pixel: %s"), Init.BoundShaderState.PixelShaderRHI->GetShaderName());
	}

	UE_LOG(LogRHI, Error, TEXT("Render Targets: (%u)"), Init.RenderTargetFormats.Num());
	for(int32 i = 0; i < Init.RenderTargetFormats.Num(); ++i)
	{
		//#todo-mattc GetPixelFormatString is not available in scw. Need to move it so we can print more info here.
		UE_LOG(LogRHI, Error, TEXT("0x%x"), (uint32)Init.RenderTargetFormats[i]);
	}

	UE_LOG(LogRHI, Error, TEXT("Depth Stencil Format:"));
	UE_LOG(LogRHI, Error, TEXT("0x%x"), Init.DepthStencilTargetFormat);
#endif

	if(Init.bFromPSOFileCache)
	{
		// Let the cache know so it hopefully won't give out this one again
		FPipelineFileCacheManager::RegisterPSOCompileFailure(GetTypeHash(Init), Init);
	}
	else if(!Init.bPSOPrecache)
	{
		// Precache requests are allowed to fail, but if the PSO is needed by a draw/dispatch command, we cannot continue.
		UE_LOG(LogRHI, Fatal, TEXT("Shader compilation failures are Fatal."));
	}
}

// Prints out information about a failed compute pipeline compilation.
// This is fatal unless the compilation request is coming from the precaching system.
static void HandlePipelineCreationFailure(const FRHIComputeShader* ComputeShader, bool bPrecache)
{
	// Dump the shader hash so it can be looked up in the SHK data. See the previous function for details.
	UE_LOG(LogRHI, Error, TEXT("Failed to create compute pipeline with hash %s."), *ComputeShader->GetHash().ToString());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogRHI, Error, TEXT("Shader: %s"), ComputeShader->GetShaderName());
#endif

	if (!bPrecache)
	{
		// Same as above, precache failures are not fatal.
		UE_LOG(LogRHI, Fatal, TEXT("Shader compilation failures are Fatal."));
	}
}

// FAsyncTask used by the PSO threadpool that will reschedule once only.
template<typename TTask>
class FAsyncTaskLimitedReschedule : public FAsyncTask<TTask>
{
public:
	/** Forwarding constructor. */
	template<typename...T>
	explicit FAsyncTaskLimitedReschedule(T&&... Args) : FAsyncTask<TTask>(Forward<T>(Args)...)	{ }

	bool Reschedule(FQueuedThreadPool* InQueuedPool, EQueuedWorkPriority InQueuedWorkPriority)
	{
		if (bCanReschedule.exchange(false))
		{
			return FAsyncTask<TTask>::Reschedule(InQueuedPool, InQueuedWorkPriority);
		}
		return false;
	}
private:
	std::atomic_bool bCanReschedule = true;
};

class FPSOPrecompileTask : public FNonAbandonableTask
{
	friend class FAsyncTask<FPSOPrecompileTask>;
private:
	TUniqueFunction<void()> Func;
	TUniqueFunction<void()> OnComplete;
public:
	FPSOPrecompileTask(TUniqueFunction<void()> InFunc, TUniqueFunction<void()> InOnComplete) : Func(MoveTemp(InFunc)), OnComplete(MoveTemp(InOnComplete)) {}

protected:
	void DoWork() 
	{ 
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FPSOPrecompileTask_Work);

		Func(); 
		Func = nullptr;
		OnComplete(); 
		OnComplete = nullptr;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPSOPrecompileTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

/**
 * Base class to hold pipeline state (and optionally stats)
 */
class FPipelineState
{
public:

	FPipelineState()
	: Stats(nullptr)
	{
		InitStats();
	}

	virtual ~FPipelineState() 
	{
		check(IsComplete());
		check(!WaitCompletion());
		check(!PrecompileTask.IsValid());
	}

	virtual bool IsCompute() const = 0;

	FGraphEventRef CompletionEvent;
	TUniquePtr<FAsyncTaskLimitedReschedule<FPSOPrecompileTask>> PrecompileTask;

	bool IsComplete()
	{
		return (!CompletionEvent.IsValid() || CompletionEvent->IsComplete()) && (!PrecompileTask.IsValid() || PrecompileTask->IsDone());
	}

	// return true if we actually waited on the task
	bool WaitCompletion()
	{
		bool bNeedsToWait = false;
		if(CompletionEvent.IsValid() && !CompletionEvent->IsComplete())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FPipelineState_WaitCompletion);
#if PSO_TRACK_CACHE_STATS
			UE_LOG(LogRHI, Log, TEXT("FTaskGraphInterface Waiting on FPipelineState completionEvent"));
#endif
			bNeedsToWait = true;
			FTaskGraphInterface::Get().WaitUntilTaskCompletes( CompletionEvent );
		}
		CompletionEvent = nullptr;

		if (PrecompileTask.IsValid())
		{
			bNeedsToWait = bNeedsToWait || !PrecompileTask->IsDone();
			PrecompileTask->EnsureCompletion();
			PrecompileTask = nullptr;
		}

		return bNeedsToWait;
	}

	inline void AddUse()
	{
		FPipelineStateStats::UpdateStats(Stats);
	}
	
#if PSO_TRACK_CACHE_STATS
	
	void InitStats()
	{
		FirstUsedTime = LastUsedTime = FPlatformTime::Seconds();
		FirstFrameUsed = LastFrameUsed = 0;
		Hits = HitsAcrossFrames = 0;
	}
	
	void AddHit()
	{
		LastUsedTime = FPlatformTime::Seconds();
		Hits++;

		if (LastFrameUsed != GFrameCounter)
		{
			LastFrameUsed = GFrameCounter;
			HitsAcrossFrames++;
		}
	}

	double			FirstUsedTime;
	double			LastUsedTime;
	uint64			FirstFrameUsed;
	uint64			LastFrameUsed;
	int				Hits;
	int				HitsAcrossFrames;

#else
	void InitStats() {}
	void AddHit() {}
#endif // PSO_TRACK_CACHE_STATS

	FPipelineStateStats* Stats;
};

/* State for compute  */
class FComputePipelineState : public FPipelineState
{
public:
	FComputePipelineState(FRHIComputeShader* InComputeShader)
		: ComputeShader(InComputeShader)
	{
		ComputeShader->AddRef();
	}
	
	~FComputePipelineState()
	{
		ComputeShader->Release();
	}

	virtual bool IsCompute() const
	{
		return true;
	}

	inline void Verify_IncUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		int32 Result = InUseCount.Increment();
		check(Result >= 1);
	#endif
	}

	inline void Verify_DecUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		int32 Result = InUseCount.Decrement();
		check(Result >= 0);
	#endif
	}

	inline void Verify_NoUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		check(InUseCount.GetValue() == 0);
	#endif
	}

	FRHIComputeShader* ComputeShader;
	TRefCountPtr<FRHIComputePipelineState> RHIPipeline;
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
	FThreadSafeCounter InUseCount;
#endif
};

/* State for graphics */
class FGraphicsPipelineState : public FPipelineState
{
public:
	FGraphicsPipelineState() 
	{
	}

	virtual bool IsCompute() const
	{
		return false;
	}

	inline void Verify_IncUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		int32 Result = InUseCount.Increment();
		check(Result >= 1);
	#endif
	}

	inline void Verify_DecUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		int32 Result = InUseCount.Decrement();
		check(Result >= 0);
	#endif
	}

	inline void Verify_NoUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		check(InUseCount.GetValue() == 0);
	#endif
	}

	TRefCountPtr<FRHIGraphicsPipelineState> RHIPipeline;
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
	FThreadSafeCounter InUseCount;
#endif
	uint64 SortKey = 0;
};

#if RHI_RAYTRACING
/* State for ray tracing */
class FRayTracingPipelineState : public FPipelineState
{
public:
	FRayTracingPipelineState(const FRayTracingPipelineStateInitializer& Initializer)
	{
		{
			int32 Index = 0;
			for (FRHIRayTracingShader* Shader : Initializer.GetHitGroupTable())
			{
				HitGroupShaderMap.Add(Shader->GetHash(), Index++);
			}
		}

		{
			int32 Index = 0;
			for (FRHIRayTracingShader* Shader : Initializer.GetCallableTable())
			{
				CallableShaderMap.Add(Shader->GetHash(), Index++);
			}
		}

		{
			int32 Index = 0;
			for (FRHIRayTracingShader* Shader : Initializer.GetMissTable())
			{
				MissShaderMap.Add(Shader->GetHash(), Index++);
			}
		}
	}

	virtual bool IsCompute() const
	{
		return false;
	}

	inline void AddHit()
	{
		if (LastFrameHit != GFrameCounter)
		{
			LastFrameHit = GFrameCounter;
			HitsAcrossFrames++;
		}

		FPipelineState::AddHit();
	}

	bool operator < (const FRayTracingPipelineState& Other)
	{
		if (LastFrameHit != Other.LastFrameHit)
		{
			return LastFrameHit < Other.LastFrameHit;
		}
		return HitsAcrossFrames < Other.HitsAcrossFrames;
	}

	bool IsCompilationComplete() const 
	{
		return !CompletionEvent.IsValid() || CompletionEvent->IsComplete();
	}

	inline void Verify_NoUse()
	{
	#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		check(InUseCount.GetValue() == 0);
	#endif
	}

	FRayTracingPipelineStateRHIRef RHIPipeline;

	uint64 HitsAcrossFrames = 0;
	uint64 LastFrameHit = 0;

	TMap<FSHAHash, int32> HitGroupShaderMap;
	TMap<FSHAHash, int32> CallableShaderMap;
	TMap<FSHAHash, int32> MissShaderMap;

#if PIPELINESTATECACHE_VERIFYTHREADSAFE
	FThreadSafeCounter InUseCount;
#endif
};

RHI_API FRHIComputePipelineState* GetRHIComputePipelineState(FComputePipelineState* PipelineState)
{
	ensure(PipelineState->RHIPipeline);
	PipelineState->CompletionEvent = nullptr;
	return PipelineState->RHIPipeline;
}

//extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState* PipelineState);
RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState* PipelineState)
{
	ensure(PipelineState->RHIPipeline);
	PipelineState->CompletionEvent = nullptr;
	return PipelineState->RHIPipeline;
}

#endif // RHI_RAYTRACING

int32 FindRayTracingHitGroupIndex(FRayTracingPipelineState* Pipeline, FRHIRayTracingShader* HitGroupShader, bool bRequired)
{
#if RHI_RAYTRACING
	if (int32* FoundIndex = Pipeline->HitGroupShaderMap.Find(HitGroupShader->GetHash()))
	{
		return *FoundIndex;
	}
	checkf(!bRequired, TEXT("Required hit group shader was not found in the ray tracing pipeline."));
#endif // RHI_RAYTRACING

	return INDEX_NONE;
}

int32 FindRayTracingCallableShaderIndex(FRayTracingPipelineState* Pipeline, FRHIRayTracingShader* CallableShader, bool bRequired)
{
#if RHI_RAYTRACING
	if (int32* FoundIndex = Pipeline->CallableShaderMap.Find(CallableShader->GetHash()))
	{
		return *FoundIndex;
	}
	checkf(!bRequired, TEXT("Required callable shader was not found in the ray tracing pipeline."));
#endif // RHI_RAYTRACING

	return INDEX_NONE;
}

int32 FindRayTracingMissShaderIndex(FRayTracingPipelineState* Pipeline, FRHIRayTracingShader* MissShader, bool bRequired)
{
#if RHI_RAYTRACING
	if (int32* FoundIndex = Pipeline->MissShaderMap.Find(MissShader->GetHash()))
	{
		return *FoundIndex;
	}
	checkf(!bRequired, TEXT("Required miss shader was not found in the ray tracing pipeline."));
#endif // RHI_RAYTRACING

	return INDEX_NONE;
}

bool IsPrecachedPSO(const FGraphicsPipelineStateInitializer& Initializer)
{
	return Initializer.bFromPSOFileCache || Initializer.bPSOPrecache;
}

FComputePipelineState* FindComputePipelineState(FRHIComputeShader* ComputeShader, bool bVerifyUse)
{
	return PipelineStateCache::FindComputePipelineState(ComputeShader, bVerifyUse);
}

FComputePipelineState* GetComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader, EPSOPrecacheResult PSOPrecacheResult)
{
	FComputePipelineState* PipelineState = PipelineStateCache::GetAndOrCreateComputePipelineState(RHICmdList, ComputeShader, false, PSOPrecacheResult);
	PipelineState->Verify_IncUse();
	return PipelineState;
}

FComputePipelineState* GetComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader)
{
	return GetComputePipelineState(RHICmdList, ComputeShader, EPSOPrecacheResult::Untracked);
}

void SetComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader, EPSOPrecacheResult PSOPrecacheResult)
{
	FComputePipelineState* PipelineState = GetComputePipelineState(RHICmdList, ComputeShader, PSOPrecacheResult);
	RHICmdList.SetComputePipelineState(PipelineState, ComputeShader);
}

void SetGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& Initializer, uint32 StencilRef, EApplyRendertargetOption ApplyFlags, bool bApplyAdditionalState, EPSOPrecacheResult PSOPrecacheResult)
{
#if PLATFORM_USE_FALLBACK_PSO
	RHICmdList.SetGraphicsPipelineState(Initializer, StencilRef, bApplyAdditionalState);
#else
	FGraphicsPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateGraphicsPipelineState(RHICmdList, Initializer, ApplyFlags, PSOPrecacheResult);

	if (PipelineState && !Initializer.bFromPSOFileCache)
	{
		PipelineState->Verify_IncUse();
		check(IsInRenderingThread() || IsInParallelRenderingThread());
		RHICmdList.SetGraphicsPipelineState(PipelineState, Initializer.BoundShaderState, StencilRef, bApplyAdditionalState);
	}
#endif
}

/* TSharedPipelineStateCache
 * This is a cache of the * pipeline states
 * there is a local thread cache which is consolidated with the global thread cache
 * global thread cache is read only until the end of the frame when the local thread caches are consolidated
 */
template<class TMyKey,class TMyValue>
class TSharedPipelineStateCache
{
private:

	TMap<TMyKey, TMyValue>& GetLocalCache()
	{
		void* TLSValue = FPlatformTLS::GetTlsValue(TLSSlot);
		if (TLSValue == nullptr)
		{
			FPipelineStateCacheType* PipelineStateCache = new FPipelineStateCacheType;
			FPlatformTLS::SetTlsValue(TLSSlot, (void*)(PipelineStateCache) );

			FScopeLock S(&AllThreadsLock);
			AllThreadsPipelineStateCache.Add(PipelineStateCache);
			return *PipelineStateCache;
		}
		return *((FPipelineStateCacheType*)TLSValue);
	}

#if PIPELINESTATECACHE_VERIFYTHREADSAFE
	struct FScopeVerifyIncrement
	{
		volatile int32 &VerifyMutex;
		FScopeVerifyIncrement(volatile int32& InVerifyMutex) : VerifyMutex(InVerifyMutex)
		{
			int32 Result = FPlatformAtomics::InterlockedIncrement(&VerifyMutex);
			if (Result <= 0)
			{
				UE_LOG(LogRHI, Fatal, TEXT("Find was hit while Consolidate was running"));
			}
		}

		~FScopeVerifyIncrement()
		{
			int32 Result = FPlatformAtomics::InterlockedDecrement(&VerifyMutex);
			if (Result < 0)
			{
				UE_LOG(LogRHI, Fatal, TEXT("Find was hit while Consolidate was running"));
			}
		}
	};

	struct FScopeVerifyDecrement
	{
		volatile int32 &VerifyMutex;
		FScopeVerifyDecrement(volatile int32& InVerifyMutex) : VerifyMutex(InVerifyMutex)
		{
			int32 Result = FPlatformAtomics::InterlockedDecrement(&VerifyMutex);
			if (Result >= 0)
			{
				UE_LOG(LogRHI, Fatal, TEXT("Consolidate was hit while Get/SetPSO was running"));
			}
		}

		~FScopeVerifyDecrement()
		{
			int32 Result = FPlatformAtomics::InterlockedIncrement(&VerifyMutex);
			if (Result != 0)
			{
				UE_LOG(LogRHI, Fatal, TEXT("Consolidate was hit while Get/SetPSO was running"));
			}
		}
	};
#endif

public:
	typedef TMap<TMyKey,TMyValue> FPipelineStateCacheType;

	TSharedPipelineStateCache()
	{
		CurrentMap = &Map1;
		BackfillMap = &Map2;
		DuplicateStateGenerated = 0;
		TLSSlot = FPlatformTLS::AllocTlsSlot();
	}

	bool Find( const TMyKey& InKey, TMyValue& OutResult )
	{
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		FScopeVerifyIncrement S(VerifyMutex);
#endif
		// safe because we only ever find when we don't add
		TMyValue* Result = CurrentMap->Find(InKey);

		if ( Result )
		{
			OutResult = *Result;
			return true;
		}

		// check the local cahce which is safe because only this thread adds to it
		TMap<TMyKey, TMyValue> &LocalCache = GetLocalCache();
		// if it's not in the local cache then it will rebuild
		Result = LocalCache.Find(InKey);
		if (Result)
		{
			OutResult = *Result;
			return true;
		}

		Result = BackfillMap->Find(InKey);

		if ( Result )
		{
			LocalCache.Add(InKey, *Result);
			OutResult = *Result;
			return true;
		}


		return false;
		
		
	}
	// The list of tasks that are still in progress.
	TArray<TTuple<TMyKey,TMyValue>> Uncompleted;

	bool Add(const TMyKey& InKey, const TMyValue& InValue)
	{
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		FScopeVerifyIncrement S(VerifyMutex);
#endif
		// everything is added to the local cache then at end of frame we consolidate them all
		TMap<TMyKey, TMyValue> &LocalCache = GetLocalCache();

		check( LocalCache.Contains(InKey) == false );
		LocalCache.Add(InKey, InValue);
		checkfSlow(LocalCache.Contains(InKey), TEXT("PSO not found immediately after adding.  Likely cause is an uninitialized field in a constructor or copy constructor"));
		return true;
	}

	void ConsolidateThreadedCaches()
	{

		SCOPE_TIME_GUARD_MS(TEXT("ConsolidatePipelineCache"), 0.1);
		check(IsInRenderingThread());
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		FScopeVerifyDecrement S(VerifyMutex);
#endif
		
		// consolidate all the local threads keys with the current thread
		// No one is allowed to call GetLocalCache while this is running
		// this is verified by the VerifyMutex.
		for ( FPipelineStateCacheType* PipelineStateCache : AllThreadsPipelineStateCache)
		{
			for (auto PipelineStateCacheIterator = PipelineStateCache->CreateIterator(); PipelineStateCacheIterator; ++PipelineStateCacheIterator)
			{
				const TMyKey& ThreadKey = PipelineStateCacheIterator->Key;
				const TMyValue& ThreadValue = PipelineStateCacheIterator->Value;

				{
					BackfillMap->Remove(ThreadKey);

					TMyValue* CurrentValue = CurrentMap->Find(ThreadKey);
					if (CurrentValue)
					{
						// if two threads get from the backfill map then we might just be dealing with one pipelinestate, in which case we have already added it to the currentmap and don't need to do anything else
						if ( *CurrentValue != ThreadValue )
						{
							++DuplicateStateGenerated;
							DeleteArray.Add(ThreadValue);
						}
					}
					else
					{
						CurrentMap->Add(ThreadKey, ThreadValue);
						Uncompleted.Add(TTuple<TMyKey, TMyValue>(ThreadKey, ThreadValue));
					}
					PipelineStateCacheIterator.RemoveCurrent();
				}
			}
		}

		// tick and complete any uncompleted PSO tasks (we free up precompile tasks here).
		for (int32 i = Uncompleted.Num() - 1; i >= 0; i--)
		{
			checkSlow(CurrentMap->Find(Uncompleted[i].Key));
			if (Uncompleted[i].Value->IsComplete())
			{
				Uncompleted[i].Value->WaitCompletion();
				Uncompleted.RemoveAtSwap(i, 1, EAllowShrinking::No);
			}
		}
	}

	void ProcessDelayedCleanup()
	{
		check(IsInRenderingThread());

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		RHICmdList.EnqueueLambda([DeleteArray = MoveTemp(DeleteArray)](FRHICommandListImmediate& RHICmdList) mutable
		{
			for (TMyValue& OldPipelineState : DeleteArray)
			{
				// Once in the delayed list this object should not be findable anymore, so the 0 should remain, making this safe
				OldPipelineState->Verify_NoUse();

				// Duplicate entries must wait for in progress compiles to complete.
				// inprogress tasks could also remain in this container and deferred for the next tick.
				bool bWaited = OldPipelineState->WaitCompletion();
				UE_CLOG(bWaited, LogRHI, Log, TEXT("Waited on a pipeline compile task while discarding duplicate."));
				delete OldPipelineState;
			}
			DeleteArray.Empty();
		});
	}

	int32 DiscardAndSwap()
	{
		check(IsInRenderingThread());

		// the consolidate should always be run before the DiscardAndSwap.
		// there should be no inuse pipeline states in the backfill map (because they should have been moved into the CurrentMap).
		int32 Discarded = BackfillMap->Num();

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		RHICmdList.EnqueueLambda([BackfillMap = MoveTemp(*BackfillMap)](FRHICommandListImmediate& RHICmdList) mutable
		{
			for (const auto& DiscardIterator :  BackfillMap)
			{
				DiscardIterator.Value->Verify_NoUse();

				// Incomplete tasks should be put back to the current map. There should be no incomplete tasks encountered here.
				bool bWaited = DiscardIterator.Value->WaitCompletion();
				UE_CLOG(bWaited, LogRHI, Error, TEXT("Waited on a pipeline compile task while discarding retired PSOs."));
				delete DiscardIterator.Value;
			}
		});

		if ( CurrentMap == &Map1 )
		{
			CurrentMap = &Map2;
			BackfillMap = &Map1;
		}
		else
		{
			CurrentMap = &Map1;
			BackfillMap = &Map2;
		}

		// keep alive incomplete tasks by moving them back to the current map.
		for (int32 i = Uncompleted.Num() - 1; i >= 0; i--)
		{
			int32 Removed = BackfillMap->Remove(Uncompleted[i].Key);
			checkSlow(Removed);
			if (Removed)
			{
				CurrentMap->Add(Uncompleted[i].Key, Uncompleted[i].Value);
			}
		}

		return Discarded;
	}
	
	void WaitTasksComplete()
	{
		FScopeLock S(&AllThreadsLock);
		
		for ( FPipelineStateCacheType* PipelineStateCache : AllThreadsPipelineStateCache )
		{
			WaitTasksComplete(PipelineStateCache);
		}
		
		WaitTasksComplete(BackfillMap);
		WaitTasksComplete(CurrentMap);
	}

private:

	void WaitTasksComplete(FPipelineStateCacheType* PipelineStateCache)
	{
		FScopeLock S(&AllThreadsLock);
		for (auto PipelineStateCacheIterator = PipelineStateCache->CreateIterator(); PipelineStateCacheIterator; ++PipelineStateCacheIterator)
		{
			auto PipelineState = PipelineStateCacheIterator->Value;
			if (PipelineState != nullptr)
			{
				PipelineState->WaitCompletion();
			}
		}
	}
	
private:
	uint32 TLSSlot;
	FPipelineStateCacheType *CurrentMap;
	FPipelineStateCacheType *BackfillMap;

	FPipelineStateCacheType Map1;
	FPipelineStateCacheType Map2;

	TArray<TMyValue> DeleteArray;

	FCriticalSection AllThreadsLock;
	TArray<FPipelineStateCacheType*> AllThreadsPipelineStateCache;

	uint32 DuplicateStateGenerated;
#if PSO_TRACK_CACHE_STATS
	friend void DumpPipelineCacheStats();
#endif

#if PIPELINESTATECACHE_VERIFYTHREADSAFE
	volatile int32 VerifyMutex;
#endif

};

// Request state
enum class EPSOPrecacheStateMask : uint8
{
	None = 0,
	Compiling = 1 << 0, // once the start is scheduled
	Succeeded = 1 << 1, // once compilation is finished
	Failed = 1 << 2,    // once compilation is finished
	Boosted = 1 << 3,
};

ENUM_CLASS_FLAGS(EPSOPrecacheStateMask)

template<class TPrecachePipelineCacheDerived, class TPrecachedPSOInitializer, class TPipelineState>
class TPrecachePipelineCacheBase
{
public:

	TPrecachePipelineCacheBase(FPSOPrecacheRequestID::EType InType) : PSOType(InType) {}

	~TPrecachePipelineCacheBase()
	{
		// Wait for all precache tasks to finished
		WaitTasksComplete();
	}
	
protected:

	void RescheduleTaskToHighPriority(TPipelineState* PipelineState)
	{
		if (FPSOPrecacheThreadPool::UsePool())
		{
			check(PipelineState->PrecompileTask);
			PipelineState->PrecompileTask->Reschedule(&GPSOPrecacheThreadPool.Get(), EQueuedWorkPriority::Highest);
		}

		UpdateHighPriorityCompileCount(true /*Increment*/);
	}

	FPSOPrecacheRequestResult TryAddNewState(const TPrecachedPSOInitializer& Initializer, bool bDoAsyncCompile)
	{
		FPSOPrecacheRequestResult Result;
		uint32 InitializerHash = TPrecachePipelineCacheDerived::PipelineStateInitializerHash(Initializer);

		// Fast check first with read lock
		{
			FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
			if (HasPSOBeenRequested(Initializer, InitializerHash, Result))
			{
				return Result;
			}
		}

		// Now try and add with write lock
		TPipelineState* NewPipelineState = nullptr;
		{
			FRWScopeLock WriteLock(PrecachePSOsRWLock, SLT_Write);
			if (HasPSOBeenRequested(Initializer, InitializerHash, Result))
			{
				return Result;
			}

			// Add to array to get the new RequestID
			Result.RequestID.Type = (uint32)PSOType;
			Result.RequestID.RequestID = PrecachedPSOInitializers.Add(Initializer);

			// create new graphics state
			NewPipelineState = TPrecachePipelineCacheDerived::CreateNewPSO(Initializer);

			// Add data to map
			FPrecacheTask PrecacheTask;
			PrecacheTask.PipelineState = NewPipelineState;
			PrecacheTask.RequestID = Result.RequestID;
			PrecachedPSOInitializerData.AddByHash(InitializerHash, Initializer, PrecacheTask);

			if (bDoAsyncCompile)
			{
				// Assign the event at this point because we need to release the lock before calling OnNewPipelineStateCreated which
				// might call PrecacheFinished directly (The background task might get abandoned) and FRWLock can't be acquired recursively
				// Note that calling IsComplete will return false until we link it somehow like we do below
				NewPipelineState->CompletionEvent = FGraphEvent::CreateGraphEvent();
				Result.AsyncCompileEvent = NewPipelineState->CompletionEvent;

				UpdateActiveCompileCount(true /*Increment*/);
			}
		}

		TPrecachePipelineCacheDerived::OnNewPipelineStateCreated(Initializer, NewPipelineState, bDoAsyncCompile);

		// A boost request might have been issued while we were kicking the task, need to check it here
		{
			FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
			FPrecacheTask* FindResult = PrecachedPSOInitializerData.FindByHash(InitializerHash, Initializer);
			check(FindResult != nullptr);
			if (FindResult != nullptr)
			{
				EPSOPrecacheStateMask PreviousStateMask = FindResult->AddPSOPrecacheState(EPSOPrecacheStateMask::Compiling);
				// by the time we're here, PrecacheFinished might already have been called, so boost it only if we know we will call it
				if ( PreviousStateMask == EPSOPrecacheStateMask::Boosted)
				{
					RescheduleTaskToHighPriority(FindResult->PipelineState);
				}
			}
		}

		// Make sure that we don't access NewPipelineState here as the task might have already been finished, ProcessDelayedCleanup may have been called
		// and NewPipelineState already been deleted
		
		return Result;
	}
public:

	void WaitTasksComplete()
	{
		// We hold the lock to observe task state, releasing it if tasks are still in flight
		// precache tasks may also attempt to lock PrecachePSOsRWLock (TPrecachePipelineCacheBase::PrecacheFinished).
		// TODO: Replace all of this spin wait.
		bool bTasksWaiting = true;
		while(bTasksWaiting)
		{
			bTasksWaiting = false;
			{
				FRWScopeLock WriteLock(PrecachePSOsRWLock, SLT_Write);
				for (auto Iterator = PrecachedPSOInitializerData.CreateIterator(); Iterator; ++Iterator)
				{
					FPrecacheTask& PrecacheTask = Iterator->Value;
					if (PrecacheTask.PipelineState && !PrecacheTask.PipelineState->IsComplete())
					{
						bTasksWaiting = true;						
						break; // release PrecachePSOsRWLock so's to avoid any further blocking of in-progress tasks.
					}
					else if (PrecacheTask.PipelineState)
					{
						check(EnumHasAnyFlags(PrecacheTask.ReadPSOPrecacheState(), (EPSOPrecacheStateMask::Succeeded | EPSOPrecacheStateMask::Failed)));
						delete PrecacheTask.PipelineState;
						PrecacheTask.PipelineState = nullptr;
					}
				}
				if (!bTasksWaiting)
				{
					PrecachedPSOs.Empty();
				}
			}
			if (bTasksWaiting)
			{
				// Yield while we wait.
				FPlatformProcess::Sleep(0.01f);
			}
		}
	}

	EPSOPrecacheResult GetPrecachingState(const FPSOPrecacheRequestID& RequestID)
	{
		check(RequestID.GetType() == PSOType);

		TPrecachedPSOInitializer Initializer;
		{
			FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
			Initializer = PrecachedPSOInitializers[RequestID.RequestID];
		}
		return GetPrecachingStateInternal(Initializer);
	}

	EPSOPrecacheResult GetPrecachingState(const TPrecachedPSOInitializer& Initializer)
	{
		return GetPrecachingStateInternal(Initializer);
	}

	bool IsPrecaching()
	{
		FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
		return ActiveCompileCount != 0;
	}

	void BoostPriority(const FPSOPrecacheRequestID& RequestID)
	{
		check(RequestID.GetType() == PSOType);

		// Won't modify anything in this cache so readlock should be enough?
		FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
		const TPrecachedPSOInitializer& Initializer = PrecachedPSOInitializers[RequestID.RequestID];
		FPrecacheTask* FindResult = PrecachedPSOInitializerData.Find(Initializer);
		check(FindResult);
		EPSOPrecacheStateMask PreviousStateMask = FindResult->AddPSOPrecacheState(EPSOPrecacheStateMask::Boosted);
		// It's possible to get a boost request while the task has not been started yet. In this case, TryAddNewState will take care of it
		// if TryAddNewState is done, then we can proceed to boost it, if the task is not done yet
		if (PreviousStateMask == EPSOPrecacheStateMask::Compiling)
		{
			RescheduleTaskToHighPriority(FindResult->PipelineState);
		}
	}

	uint32 NumActivePrecacheRequests()
	{
		if (GPSOWaitForHighPriorityRequestsOnly)
		{
			return FPlatformAtomics::AtomicRead(&HighPriorityCompileCount);
		}
		else
		{
			return FPlatformAtomics::AtomicRead(&ActiveCompileCount);
		}
	}

	void PrecacheFinished(const TPrecachedPSOInitializer& Initializer, bool bValid)
	{
		uint32 InitializerHash = TPrecachePipelineCacheDerived::PipelineStateInitializerHash(Initializer);

		FRWScopeLock WriteLock(PrecachePSOsRWLock, SLT_Write);
		
		// Mark compiled (either succeeded or failed)
		FPrecacheTask* FindResult = PrecachedPSOInitializerData.FindByHash(InitializerHash, Initializer);
		check(FindResult);
		// We still add the 'compiling' bit here because if the task is fast enough, we can get here before the end of TryAddNewState
		const EPSOPrecacheStateMask CompleteStateMask = bValid ? (EPSOPrecacheStateMask::Succeeded | EPSOPrecacheStateMask::Compiling) : (EPSOPrecacheStateMask::Failed | EPSOPrecacheStateMask::Compiling);
		const EPSOPrecacheStateMask PreviousStateMask = FindResult->AddPSOPrecacheState(CompleteStateMask);

		// Add to array of precached PSOs so it can be cleaned up
		PrecachedPSOs.Add(Initializer);

        // Need to ensure that the boost request was actually executed: if only it was asked by BoostPriority, but not requested (ie TryAddNewState has not set the Compiling bit
        // yet) then we must ignore the request
		if (EnumHasAllFlags(PreviousStateMask, EPSOPrecacheStateMask::Boosted | EPSOPrecacheStateMask::Compiling))
		{
			UpdateHighPriorityCompileCount(false /*Increment*/);
		}
		UpdateActiveCompileCount(false /*Increment*/);
	}

	static bool IsCompilationDone(EPSOPrecacheStateMask StateMask)
	{
		return EnumHasAnyFlags(StateMask, EPSOPrecacheStateMask::Succeeded | EPSOPrecacheStateMask::Failed);
	}

	void ProcessDelayedCleanup()
	{
		SET_DWORD_STAT_FName(TPrecachePipelineCacheDerived::GetActiveCompileStatName(), ActiveCompileCount);
		SET_DWORD_STAT_FName(TPrecachePipelineCacheDerived::GetHighPriorityCompileStatName(), HighPriorityCompileCount);

		FRWScopeLock WriteLock(PrecachePSOsRWLock, SLT_Write);
		for (int32 Index = 0; Index < PrecachedPSOs.Num(); ++Index)		
		{
			const TPrecachedPSOInitializer& Initializer = PrecachedPSOs[Index];
			uint32 InitializerHash = TPrecachePipelineCacheDerived::PipelineStateInitializerHash(Initializer);

			FPrecacheTask* FindResult = PrecachedPSOInitializerData.FindByHash(InitializerHash, Initializer);
			check(FindResult && IsCompilationDone((FindResult->ReadPSOPrecacheState())));
			if (FindResult->PipelineState->IsComplete())
			{
				// This is needed to cleanup the members - bit strange because it's complete already
				verify(!FindResult->PipelineState->WaitCompletion());

				delete FindResult->PipelineState;
				FindResult->PipelineState = nullptr;

				PrecachedPSOs.RemoveAtSwap(Index);
				Index--;
			}
		}
	}

protected:
	bool HasPSOBeenRequested(const TPrecachedPSOInitializer& Initializer, uint32 InitializerHash, FPSOPrecacheRequestResult& Result)
	{
		FPrecacheTask* FindResult = PrecachedPSOInitializerData.FindByHash(InitializerHash, Initializer);
		if (FindResult)
		{			
			// If not compiled yet, then return the request ID so the caller can check the state
			if (!IsCompilationDone(FindResult->ReadPSOPrecacheState()))
			{
				Result.RequestID = FindResult->RequestID;
				Result.AsyncCompileEvent = FindResult->PipelineState->CompletionEvent;
				check(Result.RequestID.IsValid());
			}
			return true;
		}

		return false;
	}

	EPSOPrecacheResult GetPrecachingStateInternal(const TPrecachedPSOInitializer& Initializer)
	{
		FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
		uint32 InitializerHash = TPrecachePipelineCacheDerived::PipelineStateInitializerHash(Initializer);		
		FPrecacheTask* FindResult = PrecachedPSOInitializerData.FindByHash(InitializerHash, Initializer);
		if (FindResult == nullptr)
		{
			return EPSOPrecacheResult::Missed;
		}

		EPSOPrecacheStateMask CompilationState = FindResult->ReadPSOPrecacheState();
		if (!IsCompilationDone(CompilationState))
		{
			return EPSOPrecacheResult::Active;
		}

		// check we only set 1 completion bit
		const EPSOPrecacheStateMask CompletionMask = EPSOPrecacheStateMask::Succeeded | EPSOPrecacheStateMask::Failed;
		check(EnumHasAnyFlags(CompilationState, CompletionMask) && !EnumHasAllFlags(CompilationState, CompletionMask));

		return (EnumHasAnyFlags(CompilationState, EPSOPrecacheStateMask::Failed)) ? EPSOPrecacheResult::NotSupported : EPSOPrecacheResult::Complete;
	}

	void UpdateActiveCompileCount(bool bIncrement)
	{
		if (bIncrement)
		{
			FPlatformAtomics::InterlockedIncrement(&ActiveCompileCount);
		}
		else
		{
			FPlatformAtomics::InterlockedDecrement(&ActiveCompileCount);
		}
	}

	void UpdateHighPriorityCompileCount(bool bIncrement)
	{
		if (bIncrement)
		{
			FPlatformAtomics::InterlockedIncrement(&HighPriorityCompileCount);
		}
		else
		{
			FPlatformAtomics::InterlockedDecrement(&HighPriorityCompileCount);
		}
	}

	// Type of PSOs which the cache manages
	FPSOPrecacheRequestID::EType PSOType;

	// Cache of all precached PSOs - need correct compare to make sure we precache all of them
	FRWLock PrecachePSOsRWLock;

	// Array containing all the precached PSO initializers thus far - the index in this array is used to uniquely identify the PSO requests
	TArray<TPrecachedPSOInitializer> PrecachedPSOInitializers;

	// Hash map used for fast retrieval of already precached PSOs
	struct FPrecacheTask
	{
		TPipelineState* PipelineState = nullptr;
		FPSOPrecacheRequestID RequestID;

		EPSOPrecacheStateMask AddPSOPrecacheState(EPSOPrecacheStateMask DesiredState)
		{
			static_assert(sizeof(EPSOPrecacheStateMask) == sizeof(int8), "Fix the cast below");
			return (EPSOPrecacheStateMask)FPlatformAtomics::InterlockedOr((volatile int8*)&StateMask, (int8)DesiredState);
		}

		inline EPSOPrecacheStateMask ReadPSOPrecacheState()
		{
			static_assert(sizeof(EPSOPrecacheStateMask) == sizeof(int8), "Fix the cast below");
			return (EPSOPrecacheStateMask)FPlatformAtomics::AtomicRead((volatile int8*)&StateMask);
		}

	private:
		volatile EPSOPrecacheStateMask StateMask = EPSOPrecacheStateMask::None;
	};

	using TMapKeyFuncsBaseClass = TDefaultMapKeyFuncs<TPrecachedPSOInitializer, FPrecacheTask, false>;
	struct TMapKeyFuncs : public TMapKeyFuncsBaseClass
	{
		static FORCEINLINE bool Matches(typename TMapKeyFuncsBaseClass::KeyInitType A, typename TMapKeyFuncsBaseClass::KeyInitType B)
		{
			return TPrecachePipelineCacheDerived::PipelineStateInitializerMatch(A, B);
		}
		static FORCEINLINE uint32 GetKeyHash(typename TMapKeyFuncsBaseClass::KeyInitType Key)
		{
			return TPrecachePipelineCacheDerived::PipelineStateInitializerHash(Key);
		}
	};
	TMap<TPrecachedPSOInitializer, FPrecacheTask, FDefaultSetAllocator, TMapKeyFuncs> PrecachedPSOInitializerData;

	// Number of open active compiles
	volatile int32 ActiveCompileCount = 0;

	// Number of open high priority compiles
	volatile int32 HighPriorityCompileCount = 0;

	// Finished Precached PSOs which can be garbage collected
	TArray<TPrecachedPSOInitializer> PrecachedPSOs;
};

struct FPrecacheComputeInitializer
{
	FPrecacheComputeInitializer()
	: RHIComputeShaderAsU64(0)
	{}


	FPrecacheComputeInitializer(const FRHIComputeShader* InRHIComputeShader)
		: RHIComputeShaderAsU64((uint64)InRHIComputeShader)
	{}

	uint64 RHIComputeShaderAsU64; // Using a U64 here rather than FRHIComputeShader* because we keep FPrecacheComputeInitializer but the FRHIComputeShader might get deleted
};

class FPrecacheComputePipelineCache : public TPrecachePipelineCacheBase<FPrecacheComputePipelineCache, FPrecacheComputeInitializer, FComputePipelineState>
{
public:
	static FComputePipelineState* CreateNewPSO(const FPrecacheComputeInitializer& ComputeShaderInitializer)
	{
		return new FComputePipelineState((FRHIComputeShader*)ComputeShaderInitializer.RHIComputeShaderAsU64);
	}

	FPrecacheComputePipelineCache() : TPrecachePipelineCacheBase(FPSOPrecacheRequestID::EType::Compute) {}
	FPSOPrecacheRequestResult PrecacheComputePipelineState(FRHIComputeShader* ComputeShader, bool bForcePrecache);
	static void OnNewPipelineStateCreated(const FPrecacheComputeInitializer& ComputeInitializer, FComputePipelineState* NewComputePipelineState, bool bDoAsyncCompile);
 
	static const FName GetActiveCompileStatName()
	{
		return GET_STATFNAME(STAT_ActiveComputePSOPrecacheRequests);
	}
	static const FName GetHighPriorityCompileStatName()
	{
		return GET_STATFNAME(STAT_HighPriorityComputePSOPrecacheRequests);
	}

	static FORCEINLINE bool PipelineStateInitializerMatch(const FPrecacheComputeInitializer& ComputeShaderInitializerA, const FPrecacheComputeInitializer& ComputeShaderInitializerB)
	{
		// todo: would be good/more robust to have the CS equivalent of RHIMatchPrecachePSOInitializers instead of relying on pointers
		// (for example if an FRHIComputeShader gets released and a new one is allocated with the same pointer value)
		return ComputeShaderInitializerA.RHIComputeShaderAsU64 == ComputeShaderInitializerB.RHIComputeShaderAsU64;
	}

	static FORCEINLINE uint32 PipelineStateInitializerHash(const FPrecacheComputeInitializer& Key)
	{
		return GetTypeHash(Key.RHIComputeShaderAsU64);
	}
};

class FPrecacheGraphicsPipelineCache : public TPrecachePipelineCacheBase<FPrecacheGraphicsPipelineCache, FGraphicsPipelineStateInitializer, FGraphicsPipelineState>
{
public:

	static FGraphicsPipelineState* CreateNewPSO(const FGraphicsPipelineStateInitializer& Initializer)
	{
		return new FGraphicsPipelineState;
	}

	static FORCEINLINE bool PipelineStateInitializerMatch(const FGraphicsPipelineStateInitializer& A, const FGraphicsPipelineStateInitializer& B)
	{
		return RHIMatchPrecachePSOInitializers(A, B);
	}

	static FORCEINLINE uint32 PipelineStateInitializerHash(const FGraphicsPipelineStateInitializer& Key)
	{
		return RHIComputePrecachePSOHash(Key);
	}

	static const FName GetActiveCompileStatName()
	{
		return GET_STATFNAME(STAT_ActiveGraphicsPSOPrecacheRequests);
	}
	static const FName GetHighPriorityCompileStatName()
	{
		return GET_STATFNAME(STAT_HighPriorityGraphicsPSOPrecacheRequests);
	}

	FPrecacheGraphicsPipelineCache() : TPrecachePipelineCacheBase(FPSOPrecacheRequestID::EType::Graphics) {}
	FPSOPrecacheRequestResult PrecacheGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer);

	static void OnNewPipelineStateCreated(const FGraphicsPipelineStateInitializer& Initializer, FGraphicsPipelineState* NewGraphicsPipelineState, bool bDoAsyncCompile);

};

// Typed caches for compute and graphics
typedef TSharedPipelineStateCache<FRHIComputeShader*, FComputePipelineState*> FComputePipelineCache;
typedef TSharedPipelineStateCache<FGraphicsPipelineStateInitializer, FGraphicsPipelineState*> FGraphicsPipelineCache;

// These are the actual caches for both pipelines
FComputePipelineCache GComputePipelineCache;
FGraphicsPipelineCache GGraphicsPipelineCache;
FPrecacheGraphicsPipelineCache GPrecacheGraphicsPipelineCache;
FPrecacheComputePipelineCache GPrecacheComputePipelineCache;

FAutoConsoleTaskPriority CPrio_FCompilePipelineStateTask(
	TEXT("TaskGraph.TaskPriorities.CompilePipelineStateTask"),
	TEXT("Task and thread priority for FCompilePipelineStateTask."),
	ENamedThreads::HighThreadPriority,		// if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority,		// .. at normal task priority
	ENamedThreads::HighTaskPriority		// if we don't have hi pri threads, then use normal priority threads at high task priority instead
);
#if RHI_RAYTRACING

// Simple thread-safe pipeline state cache that's designed for low-frequency pipeline creation operations.
// The expected use case is a very infrequent (i.e. startup / load / streaming time) creation of ray tracing PSOs.
// This cache uses a single internal lock and therefore is not designed for highly concurrent operations.
class FRayTracingPipelineCache
{
public:
	FRayTracingPipelineCache()
	{}

	~FRayTracingPipelineCache()
	{}

	bool FindBase(const FRayTracingPipelineStateInitializer& Initializer, FRayTracingPipelineState*& OutPipeline) const
	{
		FScopeLock ScopeLock(&CriticalSection);

		// Find the most recently used pipeline with compatible configuration

		FRayTracingPipelineState* BestPipeline = nullptr;

		for (const auto& It : FullPipelines)
		{
			const FRayTracingPipelineStateSignature& CandidateInitializer = It.Key;
			FRayTracingPipelineState* CandidatePipeline = It.Value;

			if (!CandidatePipeline->RHIPipeline.IsValid()
				|| CandidateInitializer.bAllowHitGroupIndexing != Initializer.bAllowHitGroupIndexing
				|| CandidateInitializer.MaxPayloadSizeInBytes != Initializer.MaxPayloadSizeInBytes
				|| CandidateInitializer.GetRayGenHash() != Initializer.GetRayGenHash()
				|| CandidateInitializer.GetRayMissHash() != Initializer.GetRayMissHash()
				|| CandidateInitializer.GetCallableHash() != Initializer.GetCallableHash())
			{
				continue;
			}

			if (BestPipeline == nullptr || *BestPipeline < *CandidatePipeline)
			{
				BestPipeline = CandidatePipeline;
			}
		}

		if (BestPipeline)
		{
			OutPipeline = BestPipeline;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool FindBySignature(const FRayTracingPipelineStateSignature& Signature, FRayTracingPipelineState*& OutCachedState) const
	{
		FScopeLock ScopeLock(&CriticalSection);

		FRayTracingPipelineState* const* FoundState = FullPipelines.Find(Signature);
		if (FoundState)
		{
			OutCachedState = *FoundState;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool Find(const FRayTracingPipelineStateInitializer& Initializer, FRayTracingPipelineState*& OutCachedState) const
	{
		FScopeLock ScopeLock(&CriticalSection);

		const FPipelineMap& Cache = Initializer.bPartial ? PartialPipelines : FullPipelines;

		FRayTracingPipelineState* const* FoundState = Cache.Find(Initializer);
		if (FoundState)
		{
			OutCachedState = *FoundState;
			return true;
		}
		else
		{
			return false;
		}
	}

	// Creates and returns a new pipeline state object, adding it to internal cache.
	// The cache itself owns the object and is responsible for destroying it.
	FRayTracingPipelineState* Add(const FRayTracingPipelineStateInitializer& Initializer)
	{
		FRayTracingPipelineState* Result = new FRayTracingPipelineState(Initializer);

		FScopeLock ScopeLock(&CriticalSection);

		FPipelineMap& Cache = Initializer.bPartial ? PartialPipelines : FullPipelines;

		Cache.Add(Initializer, Result);
		Result->AddHit();

		return Result;
	}

	void Shutdown()
	{
		FScopeLock ScopeLock(&CriticalSection);
		for (auto& It : FullPipelines)
		{
			if (It.Value)
			{
				It.Value->WaitCompletion();
				delete It.Value;
				It.Value = nullptr;
			}
		}
		for (auto& It : PartialPipelines)
		{
			if (It.Value)
			{
				It.Value->WaitCompletion();
				delete It.Value;
				It.Value = nullptr;
			}
		}
		FullPipelines.Reset();
		PartialPipelines.Reset();
	}

	void Trim(int32 TargetNumEntries)
	{
		FScopeLock ScopeLock(&CriticalSection);

		// Only full pipeline cache is automatically trimmed.
		FPipelineMap& Cache = FullPipelines;

		if (Cache.Num() < TargetNumEntries)
		{
			return;
		}

		struct FEntry
		{
			FRayTracingPipelineStateSignature Key;
			uint64 LastFrameHit;
			uint64 HitsAcrossFrames;
			FRayTracingPipelineState* Pipeline;
		};
		TArray<FEntry, FConcurrentLinearArrayAllocator> Entries;
		Entries.Reserve(Cache.Num());

		const uint64 CurrentFrame = GFrameCounter;
		const uint32 NumLatencyFrames = 10;

		// Find all pipelines that were not used in the last 10 frames

		for (const auto& It : Cache)
		{
			if (It.Value->LastFrameHit + NumLatencyFrames <= CurrentFrame
				&& It.Value->IsCompilationComplete())
			{
				FEntry Entry;
				Entry.Key = It.Key;
				Entry.HitsAcrossFrames = It.Value->HitsAcrossFrames;
				Entry.LastFrameHit = It.Value->LastFrameHit;
				Entry.Pipeline = It.Value;
				Entries.Add(Entry);
			}
		}

		Entries.Sort([](const FEntry& A, const FEntry& B)
		{
			if (A.LastFrameHit == B.LastFrameHit)
			{
				return B.HitsAcrossFrames < A.HitsAcrossFrames;
			}
			else
			{
				return B.LastFrameHit < A.LastFrameHit;
			}
		});

		// Remove least useful pipelines

		while (Cache.Num() > TargetNumEntries && Entries.Num())
		{
			FEntry& LastEntry = Entries.Last();
			check(LastEntry.Pipeline->RHIPipeline);
			check(LastEntry.Pipeline->IsCompilationComplete());
			delete LastEntry.Pipeline;
			Cache.Remove(LastEntry.Key);
			Entries.Pop(EAllowShrinking::No);
		}

		LastTrimFrame = CurrentFrame;
	}

	uint64 GetLastTrimFrame() const { return LastTrimFrame; }

private:

	mutable FCriticalSection CriticalSection;
	using FPipelineMap = TMap<FRayTracingPipelineStateSignature, FRayTracingPipelineState*>;
	FPipelineMap FullPipelines;
	FPipelineMap PartialPipelines;
	uint64 LastTrimFrame = 0;
};

FRayTracingPipelineCache GRayTracingPipelineCache;
#endif

/**
 *  Compile task
 */
static std::atomic<int32> GPipelinePrecompileTasksInFlight = { 0 };

int32 PipelineStateCache::GetNumActivePipelinePrecompileTasks()
{
	return GPipelinePrecompileTasksInFlight.load();
}

class FCompilePipelineStateTask
{
public:
	FPipelineState* Pipeline;
	FGraphicsPipelineStateInitializer Initializer;
	EPSOPrecacheResult PSOPreCacheResult;
	bool bInImmediateCmdList;

	// InInitializer is only used for non-compute tasks, a default can just be used otherwise
	FCompilePipelineStateTask(FPipelineState* InPipeline, const FGraphicsPipelineStateInitializer& InInitializer, EPSOPrecacheResult InPSOPreCacheResult, bool InbInImmediateCmdList)
		: Pipeline(InPipeline)
		, Initializer(InInitializer)
		, PSOPreCacheResult(InPSOPreCacheResult)
		, bInImmediateCmdList(InbInImmediateCmdList)
	{
		ensure(Pipeline->CompletionEvent != nullptr);
		if(Initializer.bFromPSOFileCache)
		{
			GPipelinePrecompileTasksInFlight++;
		}

		if (!Pipeline->IsCompute())
		{
			if (Initializer.BoundShaderState.GetMeshShader())
			{
				Initializer.BoundShaderState.GetMeshShader()->AddRef();
			}
			if (Initializer.BoundShaderState.GetAmplificationShader())
			{
				Initializer.BoundShaderState.GetAmplificationShader()->AddRef();
			}
			if (Initializer.BoundShaderState.VertexDeclarationRHI)
			{
				Initializer.BoundShaderState.VertexDeclarationRHI->AddRef();
			}
			if (Initializer.BoundShaderState.VertexShaderRHI)
			{
				Initializer.BoundShaderState.VertexShaderRHI->AddRef();
			}
			if (Initializer.BoundShaderState.PixelShaderRHI)
			{
				Initializer.BoundShaderState.PixelShaderRHI->AddRef();
			}
			if (Initializer.BoundShaderState.GetGeometryShader())
			{
				Initializer.BoundShaderState.GetGeometryShader()->AddRef();
			}
			if (Initializer.BlendState)
			{
				Initializer.BlendState->AddRef();
			}
			if (Initializer.RasterizerState)
			{
				Initializer.RasterizerState->AddRef();
			}
			if (Initializer.DepthStencilState)
			{
				Initializer.DepthStencilState->AddRef();
			}

			if (Initializer.BlendState)
			{
				Initializer.BlendState->AddRef();
			}
			if (Initializer.RasterizerState)
			{
				Initializer.RasterizerState->AddRef();
			}
			if (Initializer.DepthStencilState)
			{
				Initializer.DepthStencilState->AddRef();
			}
		}
	}

	~FCompilePipelineStateTask()
	{
		if (Initializer.bFromPSOFileCache)
		{
			GPipelinePrecompileTasksInFlight--;
		}
	}

	static constexpr ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		CompilePSO();
	}

	void CompilePSO()
	{
		LLM_SCOPE(ELLMTag::PSO);
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);

		if (Pipeline->IsCompute())
		{
			FComputePipelineState* ComputePipeline = static_cast<FComputePipelineState*>(Pipeline);
			
			uint64 StartTime = FPlatformTime::Cycles64();
			ComputePipeline->RHIPipeline = RHICreateComputePipelineState(ComputePipeline->ComputeShader);
			CheckAndUpdateHitchCountStat(FPSOPrecacheRequestID::EType::Compute, !IsPrecachedPSO(Initializer), StartTime);

			if (!ComputePipeline->RHIPipeline)
			{
				HandlePipelineCreationFailure(ComputePipeline->ComputeShader, Initializer.bFromPSOFileCache || Initializer.bPSOPrecache);
			}

			if (Initializer.bPSOPrecache)
			{
				bool bCSValid = ComputePipeline->RHIPipeline != nullptr && ComputePipeline->RHIPipeline->IsValid();
				GPrecacheComputePipelineCache.PrecacheFinished(ComputePipeline->ComputeShader, bCSValid);
			}
		}
		else
		{
			const TCHAR* PSOPrecacheResultString = nullptr;
			switch (PSOPreCacheResult)
			{
			case EPSOPrecacheResult::Unknown:			PSOPrecacheResultString = TEXT("PSOPrecache: Unknown"); break;
			case EPSOPrecacheResult::Active:			PSOPrecacheResultString = TEXT("PSOPrecache: Precaching"); break;
			case EPSOPrecacheResult::Complete:			PSOPrecacheResultString = TEXT("PSOPrecache: Precached"); break;
			case EPSOPrecacheResult::Missed:			PSOPrecacheResultString = TEXT("PSOPrecache: Missed"); break;
			case EPSOPrecacheResult::TooLate:			PSOPrecacheResultString = TEXT("PSOPrecache: Too Late"); break;
			case EPSOPrecacheResult::NotSupported:		PSOPrecacheResultString = TEXT("PSOPrecache: Precache Untracked"); break;
			case EPSOPrecacheResult::Untracked:			PSOPrecacheResultString = TEXT("PSOPrecache: Untracked"); break;
			}
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(PSOPrecacheResultString);

			bool bSkipCreation = false;
			if (GRHISupportsMeshShadersTier0)
			{
				if (!Initializer.BoundShaderState.VertexShaderRHI && !Initializer.BoundShaderState.GetMeshShader())
				{
					UE_LOG(LogRHI, Error, TEXT("Tried to create a Gfx Pipeline State without Vertex or Mesh Shader"));
					bSkipCreation = true;
				}
			}
			else
			{
				if (Initializer.BoundShaderState.GetMeshShader())
				{
					UE_LOG(LogRHI, Error, TEXT("Tried to create a Gfx Pipeline State with Mesh Shader on hardware without mesh shader support."));
					bSkipCreation = true;
				}

				if (!Initializer.BoundShaderState.VertexShaderRHI)
				{
					UE_LOG(LogRHI, Error, TEXT("Tried to create a Gfx Pipeline State without Vertex Shader"));
					bSkipCreation = true;
				}
			}

			FGraphicsPipelineState* GfxPipeline = static_cast<FGraphicsPipelineState*>(Pipeline);

			uint64 StartTime = FPlatformTime::Cycles64();
			GfxPipeline->RHIPipeline = bSkipCreation ? nullptr : RHICreateGraphicsPipelineState(Initializer);
			CheckAndUpdateHitchCountStat(FPSOPrecacheRequestID::EType::Graphics, !IsPrecachedPSO(Initializer), StartTime);

			if (GfxPipeline->RHIPipeline)
			{
				GfxPipeline->SortKey = GfxPipeline->RHIPipeline->GetSortKey();
			}
			else
			{
				HandlePipelineCreationFailure(Initializer);
			}

			// Mark as finished when it's a precaching job
			if (Initializer.bPSOPrecache)
			{
				GPrecacheGraphicsPipelineCache.PrecacheFinished(Initializer, GfxPipeline->RHIPipeline != nullptr);
			}

			if (Initializer.BoundShaderState.GetMeshShader())
			{
				Initializer.BoundShaderState.GetMeshShader()->Release();
			}
			if (Initializer.BoundShaderState.GetAmplificationShader())
			{
				Initializer.BoundShaderState.GetAmplificationShader()->Release();
			}
			if (Initializer.BoundShaderState.VertexDeclarationRHI)
			{
				Initializer.BoundShaderState.VertexDeclarationRHI->Release();
			}
			if (Initializer.BoundShaderState.VertexShaderRHI)
			{
				Initializer.BoundShaderState.VertexShaderRHI->Release();
			}
			if (Initializer.BoundShaderState.PixelShaderRHI)
			{
				Initializer.BoundShaderState.PixelShaderRHI->Release();
			}
			if (Initializer.BoundShaderState.GetGeometryShader())
			{
				Initializer.BoundShaderState.GetGeometryShader()->Release();
			}
			if (Initializer.BlendState)
			{
				Initializer.BlendState->Release();
			}
			if (Initializer.RasterizerState)
			{
				Initializer.RasterizerState->Release();
			}
			if (Initializer.DepthStencilState)
			{
				Initializer.DepthStencilState->Release();
			}

			if (Initializer.BlendState)
			{
				Initializer.BlendState->Release();
			}
			if (Initializer.RasterizerState)
			{
				Initializer.RasterizerState->Release();
			}
			if (Initializer.DepthStencilState)
			{
				Initializer.DepthStencilState->Release();
			}
		}

		// We kicked a task: the event really should be there
		if (ensure(Pipeline->CompletionEvent))
		{
			Pipeline->CompletionEvent->DispatchSubsequents();
			// At this point, it's not safe to use Pipeline anymore, as it might get picked up by ProcessDelayedCleanup and deleted
			Pipeline = nullptr;
		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCompilePipelineStateTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		ENamedThreads::Type DesiredThread = GRunPSOCreateTasksOnRHIT && IsRunningRHIInSeparateThread() && bInImmediateCmdList ? ENamedThreads::RHIThread : CPrio_FCompilePipelineStateTask.Get();

		// On Mac the compilation is handled using external processes, so engine threads have very little work to do
		// and it's better to leave more CPU time to these external processes and other engine threads.
		// Also use background threads for PSO precaching when the PSO thread pool is not used
		// Compute pipelines usually take much longer to compile, compile them on background thread as well.
		return (PLATFORM_MAC || PSOPreCacheResult == EPSOPrecacheResult::Active || (Pipeline && Pipeline->IsCompute() && Initializer.bFromPSOFileCache)) ? ENamedThreads::AnyBackgroundThreadNormalTask : DesiredThread;
	}
};

void PipelineStateCache::ReportFrameHitchToCSV()
{
	ReportFrameHitchThisFrame = true;
}

/**
* Called at the end of each frame during the RHI . Evicts all items left in the backfill cached based on time
*/
void PipelineStateCache::FlushResources()
{
	check(IsInRenderingThread());

	GComputePipelineCache.ConsolidateThreadedCaches();
	GComputePipelineCache.ProcessDelayedCleanup();

	GGraphicsPipelineCache.ConsolidateThreadedCaches();
	GGraphicsPipelineCache.ProcessDelayedCleanup();

	GPrecacheGraphicsPipelineCache.ProcessDelayedCleanup();
	GPrecacheComputePipelineCache.ProcessDelayedCleanup();

	{
		int32 NumMissesThisFrame = GraphicsPipelineCacheMisses.Load(EMemoryOrder::Relaxed);
		int32 NumMissesLastFrame = GraphicsPipelineCacheMissesHistory.Num() >= 2 ? GraphicsPipelineCacheMissesHistory[1] : 0;
		CSV_CUSTOM_STAT(PSO, PSOMisses, NumMissesThisFrame, ECsvCustomStatOp::Set);

		// Put a negative number in the CSV to report that there was no hitch this frame for the PSO hitch stat.
		if (!ReportFrameHitchThisFrame)
		{
			NumMissesThisFrame = -1;
			NumMissesLastFrame = -1;
		}
		CSV_CUSTOM_STAT(PSO, PSOMissesOnHitch, NumMissesThisFrame, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(PSO, PSOPrevFrameMissesOnHitch, NumMissesLastFrame, ECsvCustomStatOp::Set);
	}

	{
		int32 NumMissesThisFrame = ComputePipelineCacheMisses.Load(EMemoryOrder::Relaxed);
		int32 NumMissesLastFrame = ComputePipelineCacheMissesHistory.Num() >= 2 ? ComputePipelineCacheMissesHistory[1] : 0;
		CSV_CUSTOM_STAT(PSO, PSOComputeMisses, NumMissesThisFrame, ECsvCustomStatOp::Set);

		// Put a negative number in the CSV to report that there was no hitch this frame for the PSO hitch stat.
		if (!ReportFrameHitchThisFrame)
		{
			NumMissesThisFrame = -1;
			NumMissesLastFrame = -1;
		}
		CSV_CUSTOM_STAT(PSO, PSOComputeMissesOnHitch, NumMissesThisFrame, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(PSO, PSOComputePrevFrameMissesOnHitch, NumMissesLastFrame, ECsvCustomStatOp::Set);
	}
	ReportFrameHitchThisFrame = false;

	GraphicsPipelineCacheMissesHistory.Insert(GraphicsPipelineCacheMisses, 0);
	GraphicsPipelineCacheMissesHistory.SetNum(PSO_MISS_FRAME_HISTORY_SIZE);
	ComputePipelineCacheMissesHistory.Insert(ComputePipelineCacheMisses, 0);
	ComputePipelineCacheMissesHistory.SetNum(PSO_MISS_FRAME_HISTORY_SIZE);
	GraphicsPipelineCacheMisses = 0;
	ComputePipelineCacheMisses = 0;

	static double LastEvictionTime = FPlatformTime::Seconds();
	double CurrentTime = FPlatformTime::Seconds();

#if PSO_DO_CACHE_EVICT_EACH_FRAME
	LastEvictionTime = 0;
#endif
	
	// because it takes two cycles for an object to move from main->backfill->gone we check
	// at half the desired eviction time
	int32 EvictionPeriod = CVarPSOEvictionTime.GetValueOnAnyThread();

	if (EvictionPeriod == 0 || CurrentTime - LastEvictionTime < EvictionPeriod)
	{
		return;
	}

	// This should be very fast, if not it's likely eviction time is too high and too 
	// many items are building up.
	SCOPE_TIME_GUARD_MS(TEXT("TrimPiplelineCache"), 0.1);

#if PSO_TRACK_CACHE_STATS
	DumpPipelineCacheStats();
#endif

	LastEvictionTime = CurrentTime;

	int32 ReleasedComputeEntries = 0;
	int32 ReleasedGraphicsEntries = 0;

	ReleasedComputeEntries  =  GComputePipelineCache.DiscardAndSwap();
	ReleasedGraphicsEntries = GGraphicsPipelineCache.DiscardAndSwap();

#if PSO_TRACK_CACHE_STATS
	UE_LOG(LogRHI, Log, TEXT("Cleared state cache in %.02f ms. %d ComputeEntries, %d Graphics entries")
		, (FPlatformTime::Seconds() - CurrentTime) / 1000
		, ReleasedComputeEntries, ReleasedGraphicsEntries);
#endif // PSO_TRACK_CACHE_STATS

}

static bool IsAsyncCompilationAllowed(FRHIComputeCommandList& RHICmdList, bool bIsPrecompileRequest)
{
	const EPSOCompileAsyncMode PSOCompileAsyncMode = (EPSOCompileAsyncMode)GCVarAsyncPipelineCompile.GetValueOnAnyThread();

	const bool bCVarAllowsAsyncCreate = PSOCompileAsyncMode == EPSOCompileAsyncMode::All
		|| (PSOCompileAsyncMode == EPSOCompileAsyncMode::Precompile && bIsPrecompileRequest)
		|| (PSOCompileAsyncMode == EPSOCompileAsyncMode::NonPrecompiled && !bIsPrecompileRequest);

	return GRHISupportsAsyncPipelinePrecompile &&
		FDataDrivenShaderPlatformInfo::GetSupportsAsyncPipelineCompilation(GMaxRHIShaderPlatform) &&
		bCVarAllowsAsyncCreate && !RHICmdList.Bypass() && (IsRunningRHIInSeparateThread() && !IsInRHIThread()) && RHICmdList.AsyncPSOCompileAllowed();
}

uint64 PipelineStateCache::RetrieveGraphicsPipelineStateSortKey(const FGraphicsPipelineState* GraphicsPipelineState)
{
	return GraphicsPipelineState != nullptr ? GraphicsPipelineState->SortKey : 0;
}

static void InternalCreateComputePipelineState(FRHIComputeShader* ComputeShader, bool bDoAsyncCompile, bool bFromPSOFileCache, FComputePipelineState* CachedState, bool bInImmediateCmdList = true)
{
	FGraphEventRef GraphEvent = CachedState->CompletionEvent;

	// create a compilation task, or just do it now...
	if (bDoAsyncCompile)
	{
		check(GraphEvent != nullptr);

		FGraphicsPipelineStateInitializer GraphicsPipelineStateInitializer;
		GraphicsPipelineStateInitializer.bFromPSOFileCache = bFromPSOFileCache;
		TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(CachedState, GraphicsPipelineStateInitializer, EPSOPrecacheResult::Untracked, bInImmediateCmdList);
	}
	else
	{
		check(GraphEvent == nullptr);
		uint64 StartTime = FPlatformTime::Cycles64();
		CachedState->RHIPipeline = RHICreateComputePipelineState(ComputeShader);
		CheckAndUpdateHitchCountStat(FPSOPrecacheRequestID::EType::Compute, !bFromPSOFileCache, StartTime);

		if (!CachedState->RHIPipeline)
		{
			HandlePipelineCreationFailure(ComputeShader, bFromPSOFileCache);
		}
	}
}

FComputePipelineState* PipelineStateCache::GetAndOrCreateComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader, bool bFromFileCache, EPSOPrecacheResult PSOPrecacheResult)
{
	LLM_SCOPE(ELLMTag::PSO);

	FComputePipelineState* OutCachedState = nullptr;

	bool bWasFound = GComputePipelineCache.Find(ComputeShader, OutCachedState);
	bool DoAsyncCompile = IsAsyncCompilationAllowed(RHICmdList, bFromFileCache);

	if (bWasFound == false)
	{		
		bool bWasPSOPrecached = PSOPrecacheResult == EPSOPrecacheResult::Active || PSOPrecacheResult == EPSOPrecacheResult::Complete;
		FPipelineFileCacheManager::CacheComputePSO(GetTypeHash(ComputeShader), ComputeShader, bWasPSOPrecached);

		// create new compute state
		OutCachedState = new FComputePipelineState(ComputeShader);
		OutCachedState->Stats = FPipelineFileCacheManager::RegisterPSOStats(GetTypeHash(ComputeShader));
		if (DoAsyncCompile)
		{
			OutCachedState->CompletionEvent = FGraphEvent::CreateGraphEvent();
		}

		if (!bFromFileCache)
		{
			++ComputePipelineCacheMisses;
		}

		FGraphEventRef GraphEvent = OutCachedState->CompletionEvent;
		InternalCreateComputePipelineState(ComputeShader, DoAsyncCompile, bFromFileCache, OutCachedState, RHICmdList.IsImmediate());

		if (GraphEvent.IsValid())
		{
			check(DoAsyncCompile);
			RHICmdList.AddDispatchPrerequisite(GraphEvent);
		}

		GComputePipelineCache.Add(ComputeShader, OutCachedState);
	}
	else
	{
		if (!bFromFileCache && !OutCachedState->IsComplete())
		{
			RHICmdList.AddDispatchPrerequisite(OutCachedState->CompletionEvent);
		}

	#if PSO_TRACK_CACHE_STATS
		OutCachedState->AddHit();
	#endif
	}

	// return the state pointer
	return OutCachedState;
}

#if RHI_RAYTRACING

class FCompileRayTracingPipelineStateTask
{
public:

	UE_NONCOPYABLE(FCompileRayTracingPipelineStateTask)

	FPipelineState* Pipeline;

	FRayTracingPipelineStateInitializer Initializer;
	const bool bBackgroundTask;

	FCompileRayTracingPipelineStateTask(FPipelineState* InPipeline, const FRayTracingPipelineStateInitializer& InInitializer, bool bInBackgroundTask)
		: Pipeline(InPipeline)
		, Initializer(InInitializer)
		, bBackgroundTask(bInBackgroundTask)
	{
		Initializer.bBackgroundCompilation = bBackgroundTask;

		// Copy all referenced shaders and AddRef them while the task is alive

		RayGenTable   = CopyShaderTable(InInitializer.GetRayGenTable());
		MissTable     = CopyShaderTable(InInitializer.GetMissTable());
		HitGroupTable = CopyShaderTable(InInitializer.GetHitGroupTable());
		CallableTable = CopyShaderTable(InInitializer.GetCallableTable());

		// Point initializer to shader tables owned by this task

		Initializer.SetRayGenShaderTable(RayGenTable, InInitializer.GetRayGenHash());
		Initializer.SetMissShaderTable(MissTable, InInitializer.GetRayMissHash());
		Initializer.SetHitGroupTable(HitGroupTable, InInitializer.GetHitGroupHash());
		Initializer.SetCallableTable(CallableTable, InInitializer.GetCallableHash());
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
		FRayTracingPipelineState* RayTracingPipeline = static_cast<FRayTracingPipelineState*>(Pipeline);
		check(!RayTracingPipeline->RHIPipeline.IsValid());
		RayTracingPipeline->RHIPipeline = RHICreateRayTracingPipelineState(Initializer);

		// References to shaders no longer need to be held by this task

		ReleaseShaders(CallableTable);
		ReleaseShaders(HitGroupTable);
		ReleaseShaders(MissTable);
		ReleaseShaders(RayGenTable);

		Initializer = FRayTracingPipelineStateInitializer();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCompileRayTracingPipelineStateTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		// NOTE: RT PSO compilation internally spawns high-priority shader compilation tasks and waits on them.
		// FCompileRayTracingPipelineStateTask itself must run at lower priority to prevent deadlocks when
		// there are multiple RTPSO tasks that all wait on compilation via WaitUntilTasksComplete().
		return bBackgroundTask ? ENamedThreads::AnyBackgroundThreadNormalTask : ENamedThreads::AnyNormalThreadNormalTask;
	}

private:

	void AddRefShaders(TArray<FRHIRayTracingShader*>& ShaderTable)
	{
		for (FRHIRayTracingShader* Ptr : ShaderTable)
		{
			Ptr->AddRef();
		}
	}

	void ReleaseShaders(TArray<FRHIRayTracingShader*>& ShaderTable)
	{
		for (FRHIRayTracingShader* Ptr : ShaderTable)
		{
			Ptr->Release();
		}
	}

	TArray<FRHIRayTracingShader*> CopyShaderTable(const TArrayView<FRHIRayTracingShader*>& Source)
	{
		TArray<FRHIRayTracingShader*> Result(Source.GetData(), Source.Num());
		AddRefShaders(Result);
		return Result;
	}

	TArray<FRHIRayTracingShader*> RayGenTable;
	TArray<FRHIRayTracingShader*> MissTable;
	TArray<FRHIRayTracingShader*> HitGroupTable;
	TArray<FRHIRayTracingShader*> CallableTable;
};
#endif // RHI_RAYTRACING

#if RHI_RAYTRACING

static bool ValidateRayTracingPipelinePayloadMask(const FRayTracingPipelineStateInitializer& InInitializer)
{
	if (InInitializer.GetRayGenTable().IsEmpty())
	{
		// if we don't have any raygen shaders, the RTPSO is not complete and we can't really do any validation
		return true;
	}
	uint32 BaseRayTracingPayloadType = 0;
	for (FRHIRayTracingShader* Shader : InInitializer.GetRayGenTable())
	{
		checkf(Shader != nullptr, TEXT("RayGen shader table should not contain any NULL entries."));
		BaseRayTracingPayloadType |= Shader->RayTracingPayloadType; // union of all possible bits the raygen shaders want
		checkf(Shader->RayTracingPayloadSize <= InInitializer.MaxPayloadSizeInBytes,
			TEXT("Raytracing shader has a %u byte payload, but RTPSO has max set to %u"),
			Shader->RayTracingPayloadSize,
			InInitializer.MaxPayloadSizeInBytes);
	}
	for (FRHIRayTracingShader* Shader : InInitializer.GetMissTable())
	{
		checkf(Shader != nullptr, TEXT("Miss shader table should not contain any NULL entries"));
		checkf((Shader->RayTracingPayloadType & BaseRayTracingPayloadType) == Shader->RayTracingPayloadType,
			TEXT("Mismatched Ray Tracing Payload type among miss shaders! Found payload type %d but expecting %d"),
			Shader->RayTracingPayloadType,
			BaseRayTracingPayloadType);
		checkf(Shader->RayTracingPayloadSize <= InInitializer.MaxPayloadSizeInBytes,
			TEXT("Raytracing shader has a %u byte payload, but RTPSO has max set to %u"),
			Shader->RayTracingPayloadSize,
			InInitializer.MaxPayloadSizeInBytes);
	}
	for (FRHIRayTracingShader* Shader : InInitializer.GetHitGroupTable())
	{
		checkf(Shader != nullptr, TEXT("Hit group shader table should not contain any NULL entries"));
		checkf((Shader->RayTracingPayloadType & BaseRayTracingPayloadType) == Shader->RayTracingPayloadType,
			TEXT("Mismatched Ray Tracing Payload type among hitgroup shaders! Found payload type %d but expecting %d"),
			Shader->RayTracingPayloadType,
			BaseRayTracingPayloadType);
		checkf(Shader->RayTracingPayloadSize <= InInitializer.MaxPayloadSizeInBytes,
			TEXT("Raytracing shader has a %u byte payload, but RTPSO has max set to %u"),
			Shader->RayTracingPayloadSize,
			InInitializer.MaxPayloadSizeInBytes);
	}
	for (FRHIRayTracingShader* Shader : InInitializer.GetCallableTable())
	{
		checkf(Shader != nullptr, TEXT("Callable shader table should not contain any NULL entries"));
		checkf((Shader->RayTracingPayloadType & BaseRayTracingPayloadType) == Shader->RayTracingPayloadType,
			TEXT("Mismatched Ray Tracing Payload type among callable shaders! Found payload type %d but expecting %d"),
			Shader->RayTracingPayloadType,
			BaseRayTracingPayloadType);
		checkf(Shader->RayTracingPayloadSize <= InInitializer.MaxPayloadSizeInBytes,
			TEXT("Raytracing shader has a %u byte payload, but RTPSO has max set to %u"),
			Shader->RayTracingPayloadSize,
			InInitializer.MaxPayloadSizeInBytes);
	}
	// pass the check that called us, any failure above is sufficient
	return true;
}

#endif // RHI_RAYTRACING


FRayTracingPipelineState* PipelineStateCache::GetAndOrCreateRayTracingPipelineState(
	FRHICommandList& RHICmdList,
	const FRayTracingPipelineStateInitializer& InInitializer,
	ERayTracingPipelineCacheFlags Flags)
{
#if RHI_RAYTRACING
	LLM_SCOPE(ELLMTag::PSO);

	check(IsInRenderingThread() || IsInParallelRenderingThread());
	check(ValidateRayTracingPipelinePayloadMask(InInitializer));

	const bool bDoAsyncCompile = IsAsyncCompilationAllowed(RHICmdList, false);
	const bool bNonBlocking = !!(Flags & ERayTracingPipelineCacheFlags::NonBlocking);

	FRayTracingPipelineState* Result = nullptr;

	bool bWasFound = GRayTracingPipelineCache.Find(InInitializer, Result);

	if (bWasFound)
	{
		if (!Result->IsCompilationComplete())
		{
			if (!bDoAsyncCompile)
			{
				// Pipeline is in cache, but compilation is not finished and async compilation is disallowed, so block here RHI pipeline is created.
				Result->WaitCompletion();
			}
			else if (bNonBlocking)
			{
				// Pipeline is in cache, but compilation has not finished yet, so it can't be used for rendering.
				// Caller must use a fallback pipeline now and try again next frame.
				Result = nullptr;
			}
			else
			{
				// Pipeline is in cache, but compilation is not finished and caller requested blocking mode.
				// RHI command list can't begin translation until this event is complete.
				RHICmdList.AddDispatchPrerequisite(Result->CompletionEvent);
			}
		}
		else
		{
			checkf(Result->RHIPipeline.IsValid(), TEXT("If pipeline is in cache and it doesn't have a completion event, then RHI pipeline is expected to be ready"));
		}
	}
	else
	{
		FPipelineFileCacheManager::CacheRayTracingPSO(InInitializer, Flags);

		// Copy the initializer as we may want to patch it below
		FRayTracingPipelineStateInitializer Initializer = InInitializer;

		// If explicit base pipeline is not provided then find a compatible one from the cache
		if (GRHISupportsRayTracingPSOAdditions && InInitializer.BasePipeline == nullptr)
		{
			FRayTracingPipelineState* BasePipeline = nullptr;
			bool bBasePipelineFound = GRayTracingPipelineCache.FindBase(Initializer, BasePipeline);
			if (bBasePipelineFound)
			{
				Initializer.BasePipeline = BasePipeline->RHIPipeline;
			}
		}

		// Remove old pipelines once per frame
		const int32 TargetCacheSize = CVarRTPSOCacheSize.GetValueOnAnyThread();
		if (TargetCacheSize > 0 && GRayTracingPipelineCache.GetLastTrimFrame() != GFrameCounter)
		{
			GRayTracingPipelineCache.Trim(TargetCacheSize);
		}

		Result = GRayTracingPipelineCache.Add(Initializer);

		if (bDoAsyncCompile)
		{
			Result->CompletionEvent = TGraphTask<FCompileRayTracingPipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(
				Result,
				Initializer,
				bNonBlocking);

			// Partial or non-blocking pipelines can't be used for rendering, therefore this command list does not need to depend on them.

			if (bNonBlocking)
			{
				Result = nullptr;
			}
			else if (!Initializer.bPartial)
			{
				RHICmdList.AddDispatchPrerequisite(Result->CompletionEvent);
			}
		}
		else
		{
			Result->RHIPipeline = RHICreateRayTracingPipelineState(Initializer);
		}
	}

	if (Result)
	{
		Result->AddHit();
	}

	return Result;

#else // RHI_RAYTRACING
	return nullptr;
#endif // RHI_RAYTRACING
}

FRayTracingPipelineState* PipelineStateCache::GetRayTracingPipelineState(const FRayTracingPipelineStateSignature& Signature)
{
#if RHI_RAYTRACING
	FRayTracingPipelineState* Result = nullptr;
	bool bWasFound = GRayTracingPipelineCache.FindBySignature(Signature, Result);
	if (bWasFound)
	{
		Result->AddHit();
	}
	return Result;
#else // RHI_RAYTRACING
	return nullptr;
#endif // RHI_RAYTRACING
}

FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* PipelineState)
{
	ensure(PipelineState->RHIPipeline);
	PipelineState->AddUse(); // Update Stats
	PipelineState->Verify_DecUse(); // Lifetime Tracking
	return PipelineState->RHIPipeline;
}

#if PSO_TRACK_CACHE_STATS

static std::atomic_uint64_t TotalPrecompileWaitTime;
static std::atomic_int64_t TotalNumPrecompileJobs;
static std::atomic_int64_t TotalNumPrecompileJobsCompleted;

void StatsStartPrecompile()
{
	TotalNumPrecompileJobs++;
}

void StatsEndPrecompile(uint64 TimeToComplete)
{
	TotalPrecompileWaitTime += TimeToComplete;
	TotalNumPrecompileJobsCompleted++;
}

#endif

inline void ValidateGraphicsPipelineStateInitializer(const FGraphicsPipelineStateInitializer& Initializer)
{
	if (GRHISupportsMeshShadersTier0)
	{
		checkf(Initializer.BoundShaderState.VertexShaderRHI || Initializer.BoundShaderState.GetMeshShader(), TEXT("GraphicsPipelineState must include a vertex or mesh shader"));
	}
	else
	{
		checkf(Initializer.BoundShaderState.VertexShaderRHI, TEXT("GraphicsPipelineState must include a vertex shader"));
	}

	check(Initializer.DepthStencilState && Initializer.BlendState && Initializer.RasterizerState);
}

static void InternalCreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, EPSOPrecacheResult PSOPrecacheResult, bool bDoAsyncCompile, bool bPSOPrecache, FGraphicsPipelineState* CachedState, bool bInImmediateCmdList = true)
{
	FGraphEventRef GraphEvent = CachedState->CompletionEvent;

	// create a compilation task, or just do it now...
	if (bDoAsyncCompile)
	{
		check(GraphEvent != nullptr);
		// Use normal task graph for non-precompile jobs (or when thread pool is not enabled)
		if (!bPSOPrecache || !FPSOPrecacheThreadPool::UsePool())
		{
			TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(CachedState, Initializer, PSOPrecacheResult, bInImmediateCmdList);
		}
		else
		{
			// Here, PSO precompiles use a separate thread pool.
			// Note that we do not add precompile tasks as cmdlist prerequisites.
			TUniquePtr<FCompilePipelineStateTask> ThreadPoolTask = MakeUnique<FCompilePipelineStateTask>(CachedState, Initializer, PSOPrecacheResult, bInImmediateCmdList);
			uint64 StartTime = FPlatformTime::Cycles64();
#if	PSO_TRACK_CACHE_STATS
			StatsStartPrecompile();
#endif
			CachedState->PrecompileTask = MakeUnique<FAsyncTaskLimitedReschedule<FPSOPrecompileTask>>(
				[ThreadPoolTask = MoveTemp(ThreadPoolTask)]()
			{
				ThreadPoolTask->CompilePSO();
			}
			,
				[StartTime]()
			{
#if PSO_TRACK_CACHE_STATS
				StatsEndPrecompile(FPlatformTime::Cycles64() - StartTime);
#endif
			}
			);
			CachedState->PrecompileTask->StartBackgroundTask(&GPSOPrecacheThreadPool.Get(), Initializer.bFromPSOFileCache ? EQueuedWorkPriority::Normal : EQueuedWorkPriority::Low);
		}
	}
	else
	{
		check(GraphEvent == nullptr);
		uint64 StartTime = FPlatformTime::Cycles64();
		CachedState->RHIPipeline = RHICreateGraphicsPipelineState(Initializer);
		CheckAndUpdateHitchCountStat(FPSOPrecacheRequestID::EType::Graphics, !IsPrecachedPSO(Initializer), StartTime);

		if (Initializer.bPSOPrecache)
		{
			GPrecacheGraphicsPipelineCache.PrecacheFinished(Initializer, CachedState->RHIPipeline != nullptr);
		}

		if (CachedState->RHIPipeline)
		{
			CachedState->SortKey = CachedState->RHIPipeline->GetSortKey();
		}
		else
		{
			HandlePipelineCreationFailure(Initializer);
		}
	}
}

FGraphicsPipelineState* PipelineStateCache::GetAndOrCreateGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& Initializer, EApplyRendertargetOption ApplyFlags, EPSOPrecacheResult PSOPrecacheResult)
{
	LLM_SCOPE(ELLMTag::PSO);
	ValidateGraphicsPipelineStateInitializer(Initializer);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 
	if (ApplyFlags == EApplyRendertargetOption::CheckApply)
	{
		// Catch cases where the state does not match
		FGraphicsPipelineStateInitializer NewInitializer = Initializer;
		RHICmdList.ApplyCachedRenderTargets(NewInitializer);

		int32 AnyFailed = 0;
		AnyFailed |= (NewInitializer.RenderTargetsEnabled != Initializer.RenderTargetsEnabled) << 0;

		if (AnyFailed == 0)
		{
			for (int32 i = 0; i < (int32)NewInitializer.RenderTargetsEnabled; i++)
			{
				AnyFailed |= (NewInitializer.RenderTargetFormats[i] != Initializer.RenderTargetFormats[i]) << 1;
				// as long as RT formats match, the flags shouldn't matter. We only store format-influencing flags in the recorded PSOs, so the check would likely fail.
				//AnyFailed |= (NewInitializer.RenderTargetFlags[i] != Initializer.RenderTargetFlags[i]) << 2;
				if (AnyFailed)
				{
					AnyFailed |= i << 24;
					break;
				}
			}
		}

		AnyFailed |= (NewInitializer.DepthStencilTargetFormat != Initializer.DepthStencilTargetFormat) << 3;
		AnyFailed |= (NewInitializer.DepthStencilTargetFlag != Initializer.DepthStencilTargetFlag) << 4;
		AnyFailed |= (NewInitializer.DepthTargetLoadAction != Initializer.DepthTargetLoadAction) << 5;
		AnyFailed |= (NewInitializer.DepthTargetStoreAction != Initializer.DepthTargetStoreAction) << 6;
		AnyFailed |= (NewInitializer.StencilTargetLoadAction != Initializer.StencilTargetLoadAction) << 7;
		AnyFailed |= (NewInitializer.StencilTargetStoreAction != Initializer.StencilTargetStoreAction) << 8;

		checkf(!AnyFailed, TEXT("GetAndOrCreateGraphicsPipelineState RenderTarget check failed with: %i !"), AnyFailed);
	}
#endif

	// Precache PSOs should never go through here.
	ensure(!Initializer.bPSOPrecache);

	FGraphicsPipelineState* OutCachedState = nullptr;

	bool bWasFound = GGraphicsPipelineCache.Find(Initializer, OutCachedState);
	if (bWasFound == false)
	{
		bool DoAsyncCompile = IsAsyncCompilationAllowed(RHICmdList, Initializer.bFromPSOFileCache);

		bool bWasPSOPrecached = PSOPrecacheResult == EPSOPrecacheResult::Active || PSOPrecacheResult == EPSOPrecacheResult::Complete;

		FPipelineFileCacheManager::CacheGraphicsPSO(GetTypeHash(Initializer), Initializer, bWasPSOPrecached);

		// create new graphics state
		OutCachedState = new FGraphicsPipelineState();
		OutCachedState->Stats = FPipelineFileCacheManager::RegisterPSOStats(GetTypeHash(Initializer));
		if (DoAsyncCompile)
		{
			OutCachedState->CompletionEvent = FGraphEvent::CreateGraphEvent();
		}

		if (!Initializer.bFromPSOFileCache)
		{
			GraphicsPipelineCacheMisses++;
		}

		// If the PSO is still precaching then mark as too late
		if (PSOPrecacheResult == EPSOPrecacheResult::Active)
		{
			PSOPrecacheResult = EPSOPrecacheResult::TooLate;
		}

		bool bPSOPrecache = Initializer.bFromPSOFileCache;
		FGraphEventRef GraphEvent = OutCachedState->CompletionEvent;
		InternalCreateGraphicsPipelineState(Initializer, PSOPrecacheResult, DoAsyncCompile, bPSOPrecache, OutCachedState, RHICmdList.IsImmediate());

		// Add dispatch pre requisite for non precaching jobs only
		//if (GraphEvent.IsValid() && (!bPSOPrecache || !FPSOPrecacheThreadPool::UsePool()))
		if (GraphEvent.IsValid() && !bPSOPrecache)
		{
			check(DoAsyncCompile);
			RHICmdList.AddDispatchPrerequisite(GraphEvent);
		}

		GGraphicsPipelineCache.Add(Initializer, OutCachedState);
	}
	else
	{
		if (!Initializer.bFromPSOFileCache && !OutCachedState->IsComplete())
		{
			if (OutCachedState->PrecompileTask)
			{
				// if this is an in-progress threadpool precompile task then it could be seconds away in the queue.
				// Reissue this task so that it jumps the precompile queue.
				OutCachedState->PrecompileTask->Reschedule(&GPSOPrecacheThreadPool.Get(), EQueuedWorkPriority::Highest);
#if PSO_TRACK_CACHE_STATS
		UE_LOG(LogRHI, Log, TEXT("An incomplete precompile task was required for rendering!"));
#endif
			}
			RHICmdList.AddDispatchPrerequisite(OutCachedState->CompletionEvent);
		}

#if PSO_TRACK_CACHE_STATS
		OutCachedState->AddHit();
#endif
	}

	// return the state pointer
	return OutCachedState;
}

FComputePipelineState* PipelineStateCache::FindComputePipelineState(FRHIComputeShader* ComputeShader, bool bVerifyUse)
{
	LLM_SCOPE(ELLMTag::PSO);
	check(ComputeShader != nullptr);

	FComputePipelineState* PipelineState = nullptr;
	GComputePipelineCache.Find(ComputeShader, PipelineState);

	if (PipelineState && PipelineState->IsComplete())
	{
		if (bVerifyUse)
		{
			PipelineState->Verify_IncUse();
		}

		return PipelineState;
	}
	else
	{
		return nullptr;
	}
}

FGraphicsPipelineState* PipelineStateCache::FindGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, bool bVerifyUse)
{
	LLM_SCOPE(ELLMTag::PSO);
	ValidateGraphicsPipelineStateInitializer(Initializer);

	FGraphicsPipelineState* PipelineState = nullptr;
	GGraphicsPipelineCache.Find(Initializer, PipelineState);

	if (PipelineState && PipelineState->IsComplete())
	{
		if (bVerifyUse)
		{
			PipelineState->Verify_IncUse();
		}

		return PipelineState;
	}
	else
	{
		return nullptr;
	}
}

bool PipelineStateCache::IsPSOPrecachingEnabled()
{	
#if WITH_EDITOR
	// Disables in the editor for now by default untill more testing is done - still WIP
	return false;
#else
	return GPSOPrecaching != 0 && GRHISupportsPSOPrecaching;
#endif // WITH_EDITOR
}

FPSOPrecacheRequestResult FPrecacheComputePipelineCache::PrecacheComputePipelineState(FRHIComputeShader* ComputeShader, bool bForcePrecache)
{
	FPSOPrecacheRequestResult Result;
	if (!PipelineStateCache::IsPSOPrecachingEnabled() && !bForcePrecache)
	{
		return Result;
	}
	if (ComputeShader == nullptr)
	{
		return Result;
	}

	FPrecacheComputeInitializer PrecacheComputeInitializer(ComputeShader);
	static bool bDoAsyncCompile = FApp::ShouldUseThreadingForPerformance();
	return GPrecacheComputePipelineCache.TryAddNewState(PrecacheComputeInitializer, bDoAsyncCompile);
}

void FPrecacheComputePipelineCache::OnNewPipelineStateCreated(const FPrecacheComputeInitializer& ComputeInitializer, FComputePipelineState* CachedState, bool bDoAsyncCompile)
{
	// create a compilation task, or just do it now...
	if (bDoAsyncCompile)
	{
		check(CachedState->CompletionEvent != nullptr);
		FGraphicsPipelineStateInitializer GraphicsPipelineStateInitializer;
		GraphicsPipelineStateInitializer.bPSOPrecache = true;
		// Thread pool disabled?
		if (!FPSOPrecacheThreadPool::UsePool())
		{
			TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(CachedState, GraphicsPipelineStateInitializer, EPSOPrecacheResult::Active, false);
		}
		else
		{
			// Here, PSO precompiles use a separate thread pool.
			// Note that we do not add precompile tasks as cmdlist prerequisites.
			TUniquePtr<FCompilePipelineStateTask> ThreadPoolTask = MakeUnique<FCompilePipelineStateTask>(CachedState, GraphicsPipelineStateInitializer, EPSOPrecacheResult::Active, false);
			uint64 StartTime = FPlatformTime::Cycles64();
#if	PSO_TRACK_CACHE_STATS
			StatsStartPrecompile();
#endif
			CachedState->PrecompileTask = MakeUnique<FAsyncTaskLimitedReschedule<FPSOPrecompileTask>>(
				[ThreadPoolTask = MoveTemp(ThreadPoolTask)]()
			{
				ThreadPoolTask->CompilePSO();
			}
			,
				[StartTime]()
			{
#if PSO_TRACK_CACHE_STATS
				StatsEndPrecompile(FPlatformTime::Cycles64() - StartTime);
#endif
			}
			);
			CachedState->PrecompileTask->StartBackgroundTask(&GPSOPrecacheThreadPool.Get(), EQueuedWorkPriority::Normal);
		}
	}
	else
	{
		check(CachedState->CompletionEvent == nullptr);
		CachedState->RHIPipeline = RHICreateComputePipelineState(CachedState->ComputeShader);
		GPrecacheComputePipelineCache.PrecacheFinished(ComputeInitializer, CachedState->RHIPipeline != nullptr);
	}
}

FPSOPrecacheRequestResult PipelineStateCache::PrecacheComputePipelineState(FRHIComputeShader* ComputeShader, bool bForcePrecache)
{
	return GPrecacheComputePipelineCache.PrecacheComputePipelineState(ComputeShader, bForcePrecache);
}

FPSOPrecacheRequestResult FPrecacheGraphicsPipelineCache::PrecacheGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	FPSOPrecacheRequestResult Result;
	if (!PipelineStateCache::IsPSOPrecachingEnabled())
	{
		return Result;
	}

	LLM_SCOPE(ELLMTag::PSO);

	// Use async compilation if available
	static bool bDoAsyncCompile = FApp::ShouldUseThreadingForPerformance();

	// try and create new graphics state
	return GPrecacheGraphicsPipelineCache.TryAddNewState(Initializer, bDoAsyncCompile);
}
	
void FPrecacheGraphicsPipelineCache::OnNewPipelineStateCreated(const FGraphicsPipelineStateInitializer & Initializer, FGraphicsPipelineState * NewGraphicsPipelineState, bool bDoAsyncCompile)
{
	ValidateGraphicsPipelineStateInitializer(Initializer);
	check((NewGraphicsPipelineState->CompletionEvent != nullptr) == bDoAsyncCompile);

	// Mark as precache so it will try and use the background thread pool if available
	bool bPSOPrecache = true;

	FGraphicsPipelineStateInitializer InitializerCopy(Initializer);
	InitializerCopy.bPSOPrecache = bPSOPrecache;

	// Start the precache task
	InternalCreateGraphicsPipelineState(InitializerCopy, EPSOPrecacheResult::Active, bDoAsyncCompile, bPSOPrecache, NewGraphicsPipelineState);
}

FPSOPrecacheRequestResult PipelineStateCache::PrecacheGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	return GPrecacheGraphicsPipelineCache.PrecacheGraphicsPipelineState(Initializer);
}

EPSOPrecacheResult PipelineStateCache::CheckPipelineStateInCache(const FGraphicsPipelineStateInitializer& PipelineStateInitializer)
{
	if (!IsPSOPrecachingEnabled())
	{
		return EPSOPrecacheResult::Unknown;
	}

	return GPrecacheGraphicsPipelineCache.GetPrecachingState(PipelineStateInitializer);
}

EPSOPrecacheResult PipelineStateCache::CheckPipelineStateInCache(FRHIComputeShader* ComputeShader)
{
	if (ComputeShader == nullptr)
	{
		return EPSOPrecacheResult::Unknown;
	}

	return GPrecacheComputePipelineCache.GetPrecachingState(ComputeShader);
}

bool PipelineStateCache::IsPrecaching(const FPSOPrecacheRequestID& PSOPrecacheRequestID)
{
	if (!IsPSOPrecachingEnabled())
	{
		return false;
	}

	EPSOPrecacheResult PrecacheResult = EPSOPrecacheResult::Unknown;
	if (PSOPrecacheRequestID.GetType() == FPSOPrecacheRequestID::EType::Graphics)
	{
		PrecacheResult = GPrecacheGraphicsPipelineCache.GetPrecachingState(PSOPrecacheRequestID);
	}
	else
	{
		PrecacheResult = GPrecacheComputePipelineCache.GetPrecachingState(PSOPrecacheRequestID);
	}
	return PrecacheResult == EPSOPrecacheResult::Active;
}

bool PipelineStateCache::IsPrecaching(const FGraphicsPipelineStateInitializer& PipelineStateInitializer)
{
	if (!IsPSOPrecachingEnabled())
	{
		return false;
	}

	return GPrecacheGraphicsPipelineCache.GetPrecachingState(PipelineStateInitializer) == EPSOPrecacheResult::Active;
}

bool PipelineStateCache::IsPrecaching(FRHIComputeShader* ComputeShader)
{
	if (!IsPSOPrecachingEnabled())
	{
		return false;
	}

	return GPrecacheComputePipelineCache.GetPrecachingState(ComputeShader) == EPSOPrecacheResult::Active;
}

bool PipelineStateCache::IsPrecaching()
{
	if (!IsPSOPrecachingEnabled())
	{
		return false;
	}

	return GPrecacheGraphicsPipelineCache.IsPrecaching() || GPrecacheComputePipelineCache.IsPrecaching();
}

void PipelineStateCache::BoostPrecachePriority(const FPSOPrecacheRequestID& PSOPrecacheRequestID)
{
	if (IsPSOPrecachingEnabled())
	{
		if (PSOPrecacheRequestID.GetType() == FPSOPrecacheRequestID::EType::Graphics)
		{
			GPrecacheGraphicsPipelineCache.BoostPriority(PSOPrecacheRequestID);
		}
		else
		{
			GPrecacheComputePipelineCache.BoostPriority(PSOPrecacheRequestID);
		}
	}
}

uint32 PipelineStateCache::NumActivePrecacheRequests()
{
	if (!IsPSOPrecachingEnabled())
	{
		return 0;
	}

	return GPrecacheGraphicsPipelineCache.NumActivePrecacheRequests() + GPrecacheComputePipelineCache.NumActivePrecacheRequests();
}

FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(FGraphicsPipelineState* GraphicsPipelineState)
{
	FRHIGraphicsPipelineState* RHIPipeline = GraphicsPipelineState->RHIPipeline;
	GraphicsPipelineState->AddUse(); // Update Stats
	GraphicsPipelineState->Verify_DecUse(); // Lifetime Tracking
	return RHIPipeline;
}

void DumpPipelineCacheStats()
{
#if PSO_TRACK_CACHE_STATS
	double TotalTime = 0.0;
	double MinTime = FLT_MAX;
	double MaxTime = FLT_MIN;

	int MinFrames = INT_MAX;
	int MaxFrames = INT_MIN;
	int TotalFrames = 0;

	int NumUsedLastMin = 0;
	int NumHits = 0;
	int NumHitsAcrossFrames = 0;
	int NumItemsMultipleFrameHits = 0;

	int NumCachedItems = GGraphicsPipelineCache.CurrentMap->Num();

	if (NumCachedItems == 0)
	{
		return;
	}

	for (auto GraphicsPipeLine : *GGraphicsPipelineCache.CurrentMap)
	{
		FGraphicsPipelineState* State = GraphicsPipeLine.Value;

		// calc timestats
		double SinceUse = FPlatformTime::Seconds() - State->FirstUsedTime;

		TotalTime += SinceUse;

		if (SinceUse <= 30.0)
		{
			NumUsedLastMin++;
		}

		MinTime = FMath::Min(MinTime, SinceUse);
		MaxTime = FMath::Max(MaxTime, SinceUse);

		// calc frame stats
		int FramesUsed = State->LastFrameUsed - State->FirstFrameUsed;
		TotalFrames += FramesUsed;
		MinFrames = FMath::Min(MinFrames, FramesUsed);
		MaxFrames = FMath::Max(MaxFrames, FramesUsed);

		NumHits += State->Hits;

		if (State->HitsAcrossFrames > 0)
		{
			NumHitsAcrossFrames += State->HitsAcrossFrames;
			NumItemsMultipleFrameHits++;
		}
	}

	UE_LOG(LogRHI, Log, TEXT("Have %d GraphicsPipeline entries"), NumCachedItems);
	UE_LOG(LogRHI, Log, TEXT("Threadpool precompile: %d GraphicsPipeline in flight, %d Jobs started, %d completed"), GPipelinePrecompileTasksInFlight.load(), TotalNumPrecompileJobs.load(), TotalNumPrecompileJobsCompleted.load());
	UE_LOG(LogRHI, Log, TEXT("Threadpool precompile: %f s avg precompile time"), FPlatformTime::GetSecondsPerCycle64() * (TotalPrecompileWaitTime.load()/FMath::Max((int64_t)1, TotalNumPrecompileJobsCompleted.load())));

	UE_LOG(LogRHI, Log, TEXT("Secs Used: Min=%.02f, Max=%.02f, Avg=%.02f. %d used in last 30 secs"), MinTime, MaxTime, TotalTime / NumCachedItems, NumUsedLastMin);
	UE_LOG(LogRHI, Log, TEXT("Frames Used: Min=%d, Max=%d, Avg=%d"), MinFrames, MaxFrames, TotalFrames / NumCachedItems);
	UE_LOG(LogRHI, Log, TEXT("Hits: Avg=%d, Items with hits across frames=%d, Avg Hits across Frames=%d"), NumHits / NumCachedItems, NumItemsMultipleFrameHits, NumHitsAcrossFrames / NumCachedItems);

	size_t TrackingMem = sizeof(FGraphicsPipelineStateInitializer) * GGraphicsPipelineCache.CurrentMap->Num();
	UE_LOG(LogRHI, Log, TEXT("Tracking Mem: %d kb"), TrackingMem / 1024);
#else
	UE_LOG(LogRHI, Error, TEXT("DEfine PSO_TRACK_CACHE_STATS for state and stats!"));
#endif // PSO_VALIDATE_CACHE
}

/** Global cache of vertex declarations. Note we don't store TRefCountPtrs, instead we AddRef() manually. */
static TMap<uint32, FRHIVertexDeclaration*> GVertexDeclarationCache;
static FCriticalSection GVertexDeclarationLock;

void PipelineStateCache::WaitForAllTasks()
{
	GComputePipelineCache.WaitTasksComplete();
	GGraphicsPipelineCache.WaitTasksComplete();
	GPrecacheGraphicsPipelineCache.WaitTasksComplete();
	GPrecacheComputePipelineCache.WaitTasksComplete();
}

void PipelineStateCache::Shutdown()
{
	WaitForAllTasks();

#if RHI_RAYTRACING
	GRayTracingPipelineCache.Shutdown();
#endif

	// call discard twice to clear both the backing and main caches
	for (int32 i = 0; i < 2; i++)
	{
		GComputePipelineCache.DiscardAndSwap();
		GGraphicsPipelineCache.DiscardAndSwap();
	}
	FPipelineFileCacheManager::Shutdown();

	for (auto Pair : GVertexDeclarationCache)
	{
		Pair.Value->Release();
	}
	GVertexDeclarationCache.Empty();

	GPSOPrecacheThreadPool.ShutdownThreadPool();
}

FRHIVertexDeclaration*	PipelineStateCache::GetOrCreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	uint32 Key = FCrc::MemCrc_DEPRECATED(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));

	FScopeLock ScopeLock(&GVertexDeclarationLock);
	FRHIVertexDeclaration** Found = GVertexDeclarationCache.Find(Key);
	if (Found)
	{
		return *Found;
	}

	FVertexDeclarationRHIRef NewDeclaration = RHICreateVertexDeclaration(Elements);

	// Add an extra reference so we don't have TRefCountPtr in the maps
	NewDeclaration->AddRef();
	GVertexDeclarationCache.Add(Key, NewDeclaration);
	return NewDeclaration;
}
