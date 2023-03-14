// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PipelineStateCache.cpp: Pipeline state cache implementation.
=============================================================================*/

#include "PipelineStateCache.h"
#include "PipelineFileCache.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/ScopeLock.h"
#include "CoreGlobals.h"
#include "Misc/TimeGuard.h"
#include "Containers/DiscardableKeyValueCache.h"
#include "Async/Async.h"

// perform cache eviction each frame, used to stress the system and flush out bugs
#define PSO_DO_CACHE_EVICT_EACH_FRAME 0

// Log event and info about cache eviction
#define PSO_LOG_CACHE_EVICT 0

// Stat tracking
#define PSO_TRACK_CACHE_STATS 0


#define PIPELINESTATECACHE_VERIFYTHREADSAFE (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

CSV_DECLARE_CATEGORY_EXTERN(PSO);

static inline uint32 GetTypeHash(const FBoundShaderStateInput& Input)
{
	return GetTypeHash(Input.VertexDeclarationRHI)
		^ GetTypeHash(Input.VertexShaderRHI)
		^ GetTypeHash(Input.PixelShaderRHI)
#if PLATFORM_SUPPORTS_MESH_SHADERS
		^ GetTypeHash(Input.GetMeshShader())
		^ GetTypeHash(Input.GetAmplificationShader())
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		^ GetTypeHash(Input.GetGeometryShader())
#endif
		;
}

static inline uint32 GetTypeHash(const FGraphicsPipelineStateInitializer& Initializer)
{
	//#todo-rco: Hash!
	return (GetTypeHash(Initializer.BoundShaderState) | (Initializer.NumSamples << 28)) ^ ((uint32)Initializer.PrimitiveType << 24) ^ GetTypeHash(Initializer.BlendState)
		^ Initializer.RenderTargetsEnabled ^ GetTypeHash(Initializer.RasterizerState) ^ GetTypeHash(Initializer.DepthStencilState);
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

#if RHI_RAYTRACING
static TAutoConsoleVariable<int32> CVarRTPSOCacheSize(
	TEXT("r.RayTracing.PSOCacheSize"),
	50,
	TEXT("Number of ray tracing pipelines to keep in the cache (default = 50). Set to 0 to disable eviction.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);
#endif // RHI_RAYTRACING

int32 GPSOPrecaching = 0;
static FAutoConsoleVariableRef CVarPSOPrecaching(
	TEXT("r.PSOPrecaching"),
	GPSOPrecaching,
	TEXT("0 to Disable PSOs precaching\n")
	TEXT("1 to Enable PSO precaching\n"),
	ECVF_ReadOnly
);

extern void DumpPipelineCacheStats();

static FAutoConsoleCommand DumpPipelineCmd(
	TEXT("r.DumpPipelineCache"),
	TEXT("Dump current cache stats."),
	FConsoleCommandDelegate::CreateStatic(DumpPipelineCacheStats)
);

void SetComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader)
{
	RHICmdList.SetComputePipelineState(PipelineStateCache::GetAndOrCreateComputePipelineState(RHICmdList, ComputeShader, false), ComputeShader);
}

static int32 GPSOPrecompileThreadPoolSize = 0;
static FAutoConsoleVariableRef GPSOPrecompileThreadPoolSizeVar(
	TEXT("r.pso.PrecompileThreadPoolSize"),
	GPSOPrecompileThreadPoolSize,
	TEXT("The maximum number of threads available for concurrent PSO Precompiling.\n")
	TEXT("0 to disable threadpool usage when precompiling PSOs. (default)")
	,
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

class FPSOPrecacheThreadPool
{
public:
	~FPSOPrecacheThreadPool()
	{
		ShutdownThreadPool();
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
				PSOPrecompileCompileThreadPoolLocal->Create(GPSOPrecompileThreadPoolSize, 512 * 1024, EThreadPriority::TPri_BelowNormal, TEXT("PSOPrecompilePool"));
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

	static bool UsePool()
	{
		return GPSOPrecompileThreadPoolSize > 0;
	}

private:
	FCriticalSection LockCS;
	std::atomic<FQueuedThreadPool*> PSOPrecompileCompileThreadPool {};
};

static FPSOPrecacheThreadPool GPSOPrecacheThreadPool;

void PipelineStateCache::PreCompileComplete()
{
	// free up our threads when the precompile completes.
	GPSOPrecacheThreadPool.ShutdownThreadPool();
}

extern RHI_API FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
extern RHI_API FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(FGraphicsPipelineState* GraphicsPipelineState);

// Prints out information about a failed compilation from Init.
// This is fatal unless the compilation request is from the PSO cache preload.
static void HandlePipelineCreationFailure(const FGraphicsPipelineStateInitializer& Init)
{
	UE_LOG(LogRHI, Error, TEXT("Failed to create GraphicsPipeline"));
	// Failure to compile is Fatal unless this is from the PSO file cache preloading.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(Init.BoundShaderState.VertexShaderRHI)
	{
		UE_LOG(LogRHI, Error, TEXT("Vertex: %s"), *Init.BoundShaderState.VertexShaderRHI->ShaderName);
	}
	if (Init.BoundShaderState.GetMeshShader())
	{
		UE_LOG(LogRHI, Error, TEXT("Mesh: %s"), *Init.BoundShaderState.GetMeshShader()->ShaderName);
	}
	if (Init.BoundShaderState.GetAmplificationShader())
	{
		UE_LOG(LogRHI, Error, TEXT("Amplification: %s"), *Init.BoundShaderState.GetAmplificationShader()->ShaderName);
	}
	if(Init.BoundShaderState.GetGeometryShader())
	{
		UE_LOG(LogRHI, Error, TEXT("Geometry: %s"), *Init.BoundShaderState.GetGeometryShader()->ShaderName);
	}
	if(Init.BoundShaderState.PixelShaderRHI)
	{
		UE_LOG(LogRHI, Error, TEXT("Pixel: %s"), *Init.BoundShaderState.PixelShaderRHI->ShaderName);
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
	else
	{
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

	FRHIComputeShader* ComputeShader;
	TRefCountPtr<FRHIComputePipelineState> RHIPipeline;
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

void SetGraphicsPipelineState(FRHICommandList& RHICmdList, const FGraphicsPipelineStateInitializer& Initializer, uint32 StencilRef, EApplyRendertargetOption ApplyFlags, bool bApplyAdditionalState, EPSOPrecacheResult PSOPrecacheResult)
{
#if PLATFORM_USE_FALLBACK_PSO
	RHICmdList.SetGraphicsPipelineState(Initializer, StencilRef, bApplyAdditionalState);
#else
	FGraphicsPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateGraphicsPipelineState(RHICmdList, Initializer, ApplyFlags, PSOPrecacheResult);
	if (PipelineState && (PipelineState->RHIPipeline || !Initializer.bFromPSOFileCache))
	{
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
		int32 Result = PipelineState->InUseCount.Increment();
		check(Result >= 1);
#endif
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
		// everything is added to the local cache then at end of frame we consoldate them all
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
				Uncompleted.RemoveAtSwap(i, 1, /*bAllowShrinking=*/ false);
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
				//once in the delayed list this object should not be findable anymore, so the 0 should remain, making this safe
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
				check(OldPipelineState->InUseCount.GetValue() == 0);
#endif
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
			for ( const auto& DiscardIterator :  BackfillMap)
			{
#if PIPELINESTATECACHE_VERIFYTHREADSAFE
				check( DiscardIterator.Value->InUseCount.GetValue() == 0);
#endif
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
			FGraphicsPipelineState* pGPipelineState = PipelineStateCacheIterator->Value;
			if(pGPipelineState != nullptr)
			{
				pGPipelineState->WaitCompletion();
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

class FPrecacheGraphicsPipelineCache
{
public:

	~FPrecacheGraphicsPipelineCache()
	{
		// Wait for all precache tasks to finished
		WaitTasksComplete();
	}

	bool Contains(const FGraphicsPipelineStateInitializer& Initializer)
	{
		FRWScopeLock ReadLock(PrecachePSOsRWLock, SLT_ReadOnly);
		return PrecachedPSOInitializers.Contains(Initializer);
	}

	FGraphicsPipelineState* TryAddNewState(const FGraphicsPipelineStateInitializer& Initializer, bool bDoAsyncCompile)
	{
		// Fast check first with read lock
		if (Contains(Initializer))
		{
			return nullptr;
		}

		FGraphicsPipelineState* NewState = nullptr;

		// Now try and add with write lock
		{
			FRWScopeLock WriteLock(PrecachePSOsRWLock, SLT_Write);

			// check again
			if (PrecachedPSOInitializers.Contains(Initializer))
			{
				return nullptr;
			}

			// create new graphics state
			NewState = new FGraphicsPipelineState();

			// Add to cache before starting async operation, because the async op will remove the task from active ops when done			
			PrecachedPSOInitializers.Add(Initializer);
		}

		// Add to active set of precaching PSOs if it's created async
		if (bDoAsyncCompile)
		{
			// Only need the hash of the PSO here
			uint64 PrecachePSOHash = RHIComputePrecachePSOHash(Initializer);

			FPrecacheTask PrecacheTask;
			PrecacheTask.Initializer = Initializer;
			PrecacheTask.PSO = NewState;

			FRWScopeLock WriteLock(ActivePrecachingTasksRWLock, SLT_Write);
			check(!ActivePrecachingTasks.Contains(PrecachePSOHash));
			ActivePrecachingTasks.Add(PrecachePSOHash, PrecacheTask);
		}

		return NewState;
	}

	void WaitTasksComplete()
	{
		FRWScopeLock WriteLock(ActivePrecachingTasksRWLock, SLT_Write);

		for (auto Iterator = ActivePrecachingTasks.CreateIterator(); Iterator; ++Iterator)
		{
			FPrecacheTask& PrecacheTask = Iterator->Value;
			check(PrecacheTask.PSO);
			PrecacheTask.PSO->WaitCompletion();
			delete PrecacheTask.PSO;
		}
		ActivePrecachingTasks.Empty();
	}

	bool IsPrecaching(const FGraphicsPipelineStateInitializer& Initializer)
	{
		uint64 PrecachePSOHash = RHIComputePrecachePSOHash(Initializer);

		FRWScopeLock ReadLock(ActivePrecachingTasksRWLock, SLT_ReadOnly);
		FPrecacheTask* FindResult = ActivePrecachingTasks.Find(PrecachePSOHash);
		if (FindResult)
		{
			// Check if not complete yet
			return !(FindResult->PSO->IsComplete());
		}

		// not found then assume it's not precaching
		return false;
	}

	bool IsPrecaching()
	{
		FRWScopeLock ReadLock(ActivePrecachingTasksRWLock, SLT_ReadOnly);
		return !ActivePrecachingTasks.IsEmpty();
	}

	void PrecacheFinished(const FGraphicsPipelineStateInitializer& Initializer)
	{
		uint64 PrecachePSOHash = RHIComputePrecachePSOHash(Initializer);

		FRWScopeLock WriteLock(ActivePrecachingTasksRWLock, SLT_Write);
		FPrecacheTask* FindResult = ActivePrecachingTasks.Find(PrecachePSOHash);
		// Might have already been processed because the task got rescheduled
		if (FindResult != nullptr)
		{
			verify(ActivePrecachingTasks.Remove(PrecachePSOHash) == 1);
			PrecachedPSOs.Add(FindResult->PSO);
		}
	}

	void ProcessDelayedCleanup()
	{
		FRWScopeLock WriteLock(ActivePrecachingTasksRWLock, SLT_Write);
		for (int32 Index = 0; Index < PrecachedPSOs.Num(); ++Index)
		{
			FGraphicsPipelineState* GraphicsPSO = PrecachedPSOs[Index];
			if (GraphicsPSO->IsComplete())
			{
				// This is needed to cleanup the members - bit strange because it's complete already
				verify(!GraphicsPSO->WaitCompletion());

				delete GraphicsPSO;
				PrecachedPSOs.RemoveAtSwap(Index);
				Index--;
			}			
		}

	}

private:

	struct FGraphicsPSOInitializerKeyFuncs : DefaultKeyFuncs<FGraphicsPipelineStateInitializer>
	{
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			return RHIMatchPrecachePSOInitializers(A, B);
		}

		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return RHIComputePrecachePSOHash(Key);
		}
	};

	// Cache of all precached PSOs - need correct compare to make sure we precache all of them
	FRWLock PrecachePSOsRWLock;
	TSet<FGraphicsPipelineStateInitializer, FGraphicsPSOInitializerKeyFuncs> PrecachedPSOInitializers;

	// Current active background precache operations - full PSO hash can be used for fast retrieval
	// (on hash clash it might report that a PSO is still compiling)
	FRWLock ActivePrecachingTasksRWLock;
	struct FPrecacheTask
	{
		FGraphicsPipelineStateInitializer Initializer;
		FGraphicsPipelineState* PSO = nullptr;
	};
	TMap<uint64, FPrecacheTask> ActivePrecachingTasks;
	TArray<FGraphicsPipelineState*> PrecachedPSOs;
};

// Typed caches for compute and graphics
typedef TDiscardableKeyValueCache< FRHIComputeShader*, FComputePipelineState*> FComputePipelineCache;
typedef TSharedPipelineStateCache<FGraphicsPipelineStateInitializer, FGraphicsPipelineState*> FGraphicsPipelineCache;

// These are the actual caches for both pipelines
FComputePipelineCache GComputePipelineCache;
FGraphicsPipelineCache GGraphicsPipelineCache;
FPrecacheGraphicsPipelineCache GPrecacheGraphicsPipelineCache;

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
			delete It.Value;
		}
		for (auto& It : PartialPipelines)
		{
			delete It.Value;
		}
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
			Entries.Pop(false);
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

	// InInitializer is only used for non-compute tasks, a default can just be used otherwise
	FCompilePipelineStateTask(FPipelineState* InPipeline, const FGraphicsPipelineStateInitializer& InInitializer, EPSOPrecacheResult InPSOPreCacheResult)
		: Pipeline(InPipeline)
		, Initializer(InInitializer)
		, PSOPreCacheResult(InPSOPreCacheResult)
	{
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

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

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
			ComputePipeline->RHIPipeline = RHICreateComputePipelineState(ComputePipeline->ComputeShader);
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
			case EPSOPrecacheResult::NotSupported:		PSOPrecacheResultString = TEXT("PSOPrecache: Precache Untracked"); break;
			case EPSOPrecacheResult::Untracked:			PSOPrecacheResultString = TEXT("PSOPrecache: Untracked"); break;
			}
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(PSOPrecacheResultString);

			if (GRHISupportsMeshShadersTier0)
			{
				if (!Initializer.BoundShaderState.VertexShaderRHI && !Initializer.BoundShaderState.GetMeshShader())
				{
					UE_LOG(LogRHI, Fatal, TEXT("Tried to create a Gfx Pipeline State without Vertex or Mesh Shader"));
				}
			}
			else
			{
				if (!Initializer.BoundShaderState.VertexShaderRHI)
				{
					UE_LOG(LogRHI, Fatal, TEXT("Tried to create a Gfx Pipeline State without Vertex Shader"));
				}
			}

			FGraphicsPipelineState* GfxPipeline = static_cast<FGraphicsPipelineState*>(Pipeline);
			GfxPipeline->RHIPipeline = RHICreateGraphicsPipelineState(Initializer);

			if (GfxPipeline->RHIPipeline)
			{
				GfxPipeline->SortKey = GfxPipeline->RHIPipeline->GetSortKey();
			}
			else
			{
				HandlePipelineCreationFailure(Initializer);
			}

			// Mark as finished when it's a precaching job
			if (PSOPreCacheResult == EPSOPrecacheResult::Active)
			{
				GPrecacheGraphicsPipelineCache.PrecacheFinished(Initializer);
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
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FCompilePipelineStateTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		ENamedThreads::Type DesiredThread = GRunPSOCreateTasksOnRHIT && IsRunningRHIInSeparateThread() ? ENamedThreads::RHIThread : CPrio_FCompilePipelineStateTask.Get();

		// On Mac the compilation is handled using external processes, so engine threads have very little work to do
		// and it's better to leave more CPU time to these extrenal processes and other engine threads.
		// Also use background threads for PSO precaching when the PSO thread pool is not used
		return (PLATFORM_MAC || PSOPreCacheResult == EPSOPrecacheResult::Active) ? ENamedThreads::AnyBackgroundThreadNormalTask : DesiredThread;
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

	GGraphicsPipelineCache.ConsolidateThreadedCaches();
	GGraphicsPipelineCache.ProcessDelayedCleanup();

	GPrecacheGraphicsPipelineCache.ProcessDelayedCleanup();

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

	int ReleasedComputeEntries = 0;
	int ReleasedGraphicsEntries = 0;

	ReleasedComputeEntries = GComputePipelineCache.Discard([](FComputePipelineState* CacheItem) {
		delete CacheItem;
	});

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

FComputePipelineState* PipelineStateCache::GetAndOrCreateComputePipelineState(FRHIComputeCommandList& RHICmdList, FRHIComputeShader* ComputeShader, bool bFromFileCache)
{
	LLM_SCOPE(ELLMTag::PSO);
	bool DoAsyncCompile = IsAsyncCompilationAllowed(RHICmdList, bFromFileCache);

	FComputePipelineState* OutCachedState = nullptr;

	uint32 LockFlags = GComputePipelineCache.ApplyLock(0, FComputePipelineCache::LockFlags::ReadLock);

	bool WasFound = GComputePipelineCache.Find(ComputeShader, OutCachedState, LockFlags | FComputePipelineCache::LockFlags::WriteLockOnAddFail, LockFlags);

	if (WasFound == false)
	{
		FPipelineFileCacheManager::CacheComputePSO(GetTypeHash(ComputeShader), ComputeShader);

		// create new graphics state
		OutCachedState = new FComputePipelineState(ComputeShader);
		OutCachedState->Stats = FPipelineFileCacheManager::RegisterPSOStats(GetTypeHash(ComputeShader));

		if (!bFromFileCache)
		{
			++ComputePipelineCacheMisses;
		}

		// create a compilation task, or just do it now...
		if (DoAsyncCompile)
		{
			OutCachedState->CompletionEvent = TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(OutCachedState, FGraphicsPipelineStateInitializer(), EPSOPrecacheResult::Untracked);
			RHICmdList.AddDispatchPrerequisite(OutCachedState->CompletionEvent);
		}
		else
		{
			OutCachedState->RHIPipeline = RHICreateComputePipelineState(OutCachedState->ComputeShader);
		}

		GComputePipelineCache.Add(ComputeShader, OutCachedState, LockFlags);
	}
	else
	{
		if (DoAsyncCompile)
		{
			if(!OutCachedState->IsComplete())
			{
				RHICmdList.AddDispatchPrerequisite(OutCachedState->CompletionEvent);
			}
		}

#if PSO_TRACK_CACHE_STATS
		OutCachedState->AddHit();
#endif
	}

	GComputePipelineCache.Unlock(LockFlags);

#if 0
	bool DoAsyncCompile = IsAsyncCompilationAllowed(RHICmdList, bFromFileCache);


	TSharedPtr<FComputePipelineState> OutCachedState;

	// Find or add an entry for this initializer
	bool WasFound = GComputePipelineCache.FindOrAdd(ComputeShader, OutCachedState, [&RHICmdList, &ComputeShader, &DoAsyncCompile] {
			// create new graphics state
			TSharedPtr<FComputePipelineState> PipelineState(new FComputePipelineState(ComputeShader));
			PipelineState->Stats = FPipelineFileCacheManager::RegisterPSOStats(GetTypeHash(ComputeShader));

			// create a compilation task, or just do it now...
			if (DoAsyncCompile)
			{
				PipelineState->CompletionEvent = TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(PipelineState.Get(), FGraphicsPipelineStateInitializer(), EPSOPrecacheResult::Untracked);
				RHICmdList.QueueAsyncPipelineStateCompile(PipelineState->CompletionEvent);
			}
			else
			{
				PipelineState->RHIPipeline = RHICreateComputePipelineState(PipelineState->ComputeShader);
			}

			// wrap it and return it
			return PipelineState;
		});

	check(OutCachedState.IsValid());

	// if we found an entry the block above wasn't executed
	if (WasFound)
	{
		if (DoAsyncCompile)
		{
			FRWScopeLock ScopeLock(GComputePipelineCache.RWLock(), SLT_ReadOnly);
			FGraphEventRef& CompletionEvent = OutCachedState->CompletionEvent;
			if (CompletionEvent.IsValid() && !CompletionEvent->IsComplete())
			{
				RHICmdList.QueueAsyncPipelineStateCompile(CompletionEvent);
			}
		}
#if PSO_TRACK_CACHE_STATS
		OutCachedState->AddHit();
#endif
	}
#endif
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

FRayTracingPipelineState* PipelineStateCache::GetAndOrCreateRayTracingPipelineState(
	FRHICommandList& RHICmdList,
	const FRayTracingPipelineStateInitializer& InInitializer,
	ERayTracingPipelineCacheFlags Flags)
{
#if RHI_RAYTRACING
	LLM_SCOPE(ELLMTag::PSO);

	check(IsInRenderingThread() || IsInParallelRenderingThread());

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
		FPipelineFileCacheManager::CacheRayTracingPSO(InInitializer);

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

FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState)
{
	ensure(ComputePipelineState->RHIPipeline);
	FRWScopeLock ScopeLock(GComputePipelineCache.RWLock(), SLT_Write);
	ComputePipelineState->AddUse();
	ComputePipelineState->CompletionEvent = nullptr;
	return ComputePipelineState->RHIPipeline;
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

static FGraphEventRef CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, EPSOPrecacheResult PSOPrecacheResult, bool bDoAsyncCompile, bool bPSOPrecache, FGraphicsPipelineState* CachedState)
{
	FGraphEventRef GraphEvent;

	// create a compilation task, or just do it now...
	if (bDoAsyncCompile)
	{
		// Use normal task graph for non-precompile jobs (or when thread pool is not enabled)
		if (!bPSOPrecache || !FPSOPrecacheThreadPool::UsePool())
		{
			CachedState->CompletionEvent = TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(CachedState, Initializer, PSOPrecacheResult);
			GraphEvent = CachedState->CompletionEvent;
		}
		else
		{
			// Here, PSO precompiles use a separate thread pool.
			// Note that we do not add precompile tasks as cmdlist prerequisites.
			GraphEvent = FGraphEvent::CreateGraphEvent();
			TUniquePtr<FCompilePipelineStateTask> ThreadPoolTask = MakeUnique<FCompilePipelineStateTask>(CachedState, Initializer, PSOPrecacheResult);
			CachedState->CompletionEvent = GraphEvent;
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
				[GraphEvent, StartTime]()
			{
				GraphEvent->DispatchSubsequents();
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
		CachedState->RHIPipeline = RHICreateGraphicsPipelineState(Initializer);
		if (CachedState->RHIPipeline)
		{
			CachedState->SortKey = CachedState->RHIPipeline->GetSortKey();
		}
		else
		{
			HandlePipelineCreationFailure(Initializer);
		}
	}

	return GraphEvent;
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

	FGraphicsPipelineState* OutCachedState = nullptr;

	bool bWasFound = GGraphicsPipelineCache.Find(Initializer, OutCachedState);
	bool DoAsyncCompile = IsAsyncCompilationAllowed(RHICmdList, Initializer.bFromPSOFileCache);

	if (bWasFound == false)
	{
		FPipelineFileCacheManager::CacheGraphicsPSO(GetTypeHash(Initializer), Initializer);

		// create new graphics state
		OutCachedState = new FGraphicsPipelineState();
		OutCachedState->Stats = FPipelineFileCacheManager::RegisterPSOStats(GetTypeHash(Initializer));
		
		if (!Initializer.bFromPSOFileCache)
		{
			GraphicsPipelineCacheMisses++;
		}

		bool bPSOPrecache = Initializer.bFromPSOFileCache;
		FGraphEventRef GraphEvent = CreateGraphicsPipelineState(Initializer, PSOPrecacheResult, DoAsyncCompile, bPSOPrecache, OutCachedState);

		// Add dispatch pre requisite for non precaching jobs only
		// NOTE: do we need to add dispatch prerequisite for PSO precache when precache pool is disabled and it's using regular task graph?
		if (GraphEvent.IsValid() && (!bPSOPrecache || !FPSOPrecacheThreadPool::UsePool()))
		{
			check(DoAsyncCompile);
			RHICmdList.AddDispatchPrerequisite(GraphEvent);
		}

		GGraphicsPipelineCache.Add(Initializer, OutCachedState);
	}
	else
	{
		if (DoAsyncCompile && !Initializer.bFromPSOFileCache && !OutCachedState->IsComplete())
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

FGraphicsPipelineState* PipelineStateCache::FindGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	LLM_SCOPE(ELLMTag::PSO);
	ValidateGraphicsPipelineStateInitializer(Initializer);

	FGraphicsPipelineState* PipelineState = nullptr;
	GGraphicsPipelineCache.Find(Initializer, PipelineState);
	return (PipelineState && PipelineState->IsComplete()) ? PipelineState : nullptr;
}

bool PipelineStateCache::IsPSOPrecachingEnabled()
{	
#if WITH_EDITOR
	// Disables in the editor for now by default untill more testing is done - still WIP
	return false;
#else
	return GPSOPrecaching != 0;
#endif // WITH_EDITOR
}

FGraphEventRef PipelineStateCache::PrecacheComputePipelineState(FRHIComputeShader* ComputeShader)
{
	FGraphEventRef GraphEvent = nullptr;
	if (!IsPSOPrecachingEnabled())
	{
		return GraphEvent;
	}

	if (ComputeShader == nullptr)
	{
		return GraphEvent;
	}

	FComputePipelineState* CachedState = nullptr;
	if (GComputePipelineCache.Find(ComputeShader, CachedState))
	{
		return GraphEvent;
	}

	// create new graphics state
	CachedState = new FComputePipelineState(ComputeShader);

	// create a compilation task, or just do it now...
	static bool bDoAsyncCompile = FApp::ShouldUseThreadingForPerformance();
	if (bDoAsyncCompile)
	{
		// Thread pool disabled?
		if (!FPSOPrecacheThreadPool::UsePool())
		{
			CachedState->CompletionEvent = TGraphTask<FCompilePipelineStateTask>::CreateTask().ConstructAndDispatchWhenReady(CachedState, FGraphicsPipelineStateInitializer(), EPSOPrecacheResult::Active);
			GraphEvent = CachedState->CompletionEvent;
		}
		else
		{
			// Here, PSO precompiles use a separate thread pool.
			// Note that we do not add precompile tasks as cmdlist prerequisites.
			GraphEvent = FGraphEvent::CreateGraphEvent();
			TUniquePtr<FCompilePipelineStateTask> ThreadPoolTask = MakeUnique<FCompilePipelineStateTask>(CachedState, FGraphicsPipelineStateInitializer(), EPSOPrecacheResult::Active);
			CachedState->CompletionEvent = GraphEvent;
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
				[GraphEvent, StartTime]()
			{
				GraphEvent->DispatchSubsequents();
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
		CachedState->RHIPipeline = RHICreateComputePipelineState(CachedState->ComputeShader);
	}

	GComputePipelineCache.Add(ComputeShader, CachedState);
	return GraphEvent;
}

FGraphEventRef PipelineStateCache::PrecacheGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	if (!IsPSOPrecachingEnabled())
	{
		return nullptr;
	}

	LLM_SCOPE(ELLMTag::PSO);

	// Use async compilation if available
	static bool bDoAsyncCompile = FApp::ShouldUseThreadingForPerformance();

	// try and create new graphics state
	FGraphicsPipelineState* NewGraphicsPipelineState = GPrecacheGraphicsPipelineCache.TryAddNewState(Initializer, bDoAsyncCompile);
	if (NewGraphicsPipelineState == nullptr)
	{
		return nullptr;
	}
	
	ValidateGraphicsPipelineStateInitializer(Initializer);

	// Mark as precache so it will try and use the background thread pool if available
	bool bPSOPrecache = true;

	// Start the precache task
	return CreateGraphicsPipelineState(Initializer, EPSOPrecacheResult::Active, bDoAsyncCompile, bPSOPrecache, NewGraphicsPipelineState);
}

EPSOPrecacheResult PipelineStateCache::CheckPipelineStateInCache(const FGraphicsPipelineStateInitializer& PipelineStateInitializer)
{
	if (!IsPSOPrecachingEnabled())
	{
		return EPSOPrecacheResult::Unknown;
	}

	if (!GPrecacheGraphicsPipelineCache.Contains(PipelineStateInitializer))
	{
		return EPSOPrecacheResult::Missed;
	}

	return GPrecacheGraphicsPipelineCache.IsPrecaching(PipelineStateInitializer) ? EPSOPrecacheResult::Active : EPSOPrecacheResult::Complete;
}

bool PipelineStateCache::IsPrecaching(const FGraphicsPipelineStateInitializer& PipelineStateInitializer)
{
	if (!IsPSOPrecachingEnabled())
	{
		return false;
	}

	return GPrecacheGraphicsPipelineCache.IsPrecaching(PipelineStateInitializer);
}

bool PipelineStateCache::IsPrecaching()
{
	if (!IsPSOPrecachingEnabled())
	{
		return false;
	}

	return GPrecacheGraphicsPipelineCache.IsPrecaching();
}

FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(FGraphicsPipelineState* GraphicsPipelineState)
{
	FRHIGraphicsPipelineState* RHIPipeline = GraphicsPipelineState->RHIPipeline;

	GraphicsPipelineState->AddUse();

#if PIPELINESTATECACHE_VERIFYTHREADSAFE
	int32 Result = GraphicsPipelineState->InUseCount.Decrement();
	check(Result >= 0);
#endif
	
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

void PipelineStateCache::Shutdown()
{
	GGraphicsPipelineCache.WaitTasksComplete();
#if RHI_RAYTRACING
	GRayTracingPipelineCache.Shutdown();
#endif

	// call discard twice to clear both the backing and main caches
	for (int i = 0; i < 2; i++)
	{
		GComputePipelineCache.Discard([](FComputePipelineState* CacheItem)
		{
			if(CacheItem != nullptr)
			{
				CacheItem->WaitCompletion();
				delete CacheItem;
			}
		});
		
		GGraphicsPipelineCache.DiscardAndSwap();
	}
	FPipelineFileCacheManager::Shutdown();

	for (auto Pair : GVertexDeclarationCache)
	{
		Pair.Value->Release();
	}
	GVertexDeclarationCache.Empty();
}

FRHIVertexDeclaration*	PipelineStateCache::GetOrCreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	// Actual locking/contention time should be close to unmeasurable
	FScopeLock ScopeLock(&GVertexDeclarationLock);
	uint32 Key = FCrc::MemCrc_DEPRECATED(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));
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
