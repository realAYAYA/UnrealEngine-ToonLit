// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Atomic.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Containers/Array.h"
#include "ProfilingDebugging/CsvProfiler.h"


#define WITH_GLOBAL_RUNTIME_FX_BUDGET (!UE_SERVER)
#ifndef WITH_PARTICLE_PERF_STATS
	#if (WITH_UNREAL_DEVELOPER_TOOLS || WITH_UNREAL_TARGET_DEVELOPER_TOOLS || (!UE_BUILD_SHIPPING) || WITH_GLOBAL_RUNTIME_FX_BUDGET)
	#define WITH_PARTICLE_PERF_STATS 1
	#else
	#define	WITH_PARTICLE_PERF_STATS 0
	#endif
#else
	#if !WITH_PARTICLE_PERF_STATS
		//If perf stats are explicitly disabled then we must also disable the runtime budget tracking.
		#undef WITH_GLOBAL_RUNTIME_FX_BUDGET
		#define WITH_GLOBAL_RUNTIME_FX_BUDGET 0
	#endif
#endif

#define WITH_PER_SYSTEM_PARTICLE_PERF_STATS (WITH_PARTICLE_PERF_STATS && !UE_BUILD_SHIPPING)
#define WITH_PER_COMPONENT_PARTICLE_PERF_STATS (WITH_PARTICLE_PERF_STATS && !UE_BUILD_SHIPPING)

#define WITH_PARTICLE_PERF_CSV_STATS WITH_PER_SYSTEM_PARTICLE_PERF_STATS && CSV_PROFILER && !UE_BUILD_SHIPPING
CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Particles);

struct FParticlePerfStats;
class UWorld;
class UFXSystemAsset;
class UFXSystemComponent;

#if WITH_PARTICLE_PERF_STATS

/** Stats gathered on the game thread or game thread spawned tasks. */
struct FParticlePerfStats_GT
{
	uint64 NumInstances;
	uint64 TickGameThreadCycles;
	TAtomic<uint64> TickConcurrentCycles;
	uint64 FinalizeCycles;
	TAtomic<uint64> EndOfFrameCycles;
	TAtomic<uint64> ActivationCycles;
	uint64 WaitCycles;

	FParticlePerfStats_GT() { Reset(); }
	
	FParticlePerfStats_GT(const FParticlePerfStats_GT& Other)
	{
		NumInstances = Other.NumInstances;
		TickGameThreadCycles = Other.TickGameThreadCycles;
		TickConcurrentCycles = Other.TickConcurrentCycles.Load();
		FinalizeCycles = Other.FinalizeCycles;
		EndOfFrameCycles = Other.EndOfFrameCycles.Load();
		ActivationCycles = Other.ActivationCycles.Load();
		WaitCycles = Other.WaitCycles;
	}

	FParticlePerfStats_GT& operator=(const FParticlePerfStats_GT& Other)
	{
		NumInstances = Other.NumInstances;
		TickGameThreadCycles = Other.TickGameThreadCycles;
		TickConcurrentCycles = Other.TickConcurrentCycles.Load();
		FinalizeCycles = Other.FinalizeCycles;
		EndOfFrameCycles = Other.EndOfFrameCycles.Load();
		ActivationCycles = Other.ActivationCycles.Load();
		WaitCycles = Other.WaitCycles;
		return *this;
	}

	FParticlePerfStats_GT(FParticlePerfStats_GT&& Other)
	{
		*this = Other;
		Other.Reset();
	}

	FParticlePerfStats_GT& operator=(FParticlePerfStats_GT&& Other)
	{
		*this = Other;
		Other.Reset();		
		return *this;
	}

	FParticlePerfStats_GT& operator+=(FParticlePerfStats_GT& Other)
	{
		NumInstances += Other.NumInstances;
		TickGameThreadCycles += Other.TickGameThreadCycles;
		TickConcurrentCycles += Other.TickConcurrentCycles.Load();
		FinalizeCycles += Other.FinalizeCycles;
		EndOfFrameCycles += Other.EndOfFrameCycles.Load();
		ActivationCycles += Other.ActivationCycles.Load();
		WaitCycles += Other.WaitCycles;
		return *this;
	}

	FORCEINLINE void Reset()
	{
		NumInstances = 0;
		TickGameThreadCycles = 0;
		TickConcurrentCycles = 0;
		FinalizeCycles = 0;
		EndOfFrameCycles = 0;
		ActivationCycles = 0;
		WaitCycles = 0;
	}
	FORCEINLINE uint64 GetTotalCycles_GTOnly()const { return TickGameThreadCycles + FinalizeCycles + ActivationCycles + WaitCycles; }
	FORCEINLINE uint64 GetTotalCycles()const { return GetTotalCycles_GTOnly() + TickConcurrentCycles + EndOfFrameCycles; }
	FORCEINLINE uint64 GetPerInstanceAvgCycles()const { return NumInstances > 0 ? GetTotalCycles() / NumInstances : 0; }
};

/** Stats gathered on the render thread. */
struct FParticlePerfStats_RT
{
	uint64 NumInstances = 0;
	uint64 RenderUpdateCycles = 0;
	uint64 GetDynamicMeshElementsCycles = 0;
	
	FParticlePerfStats_RT()	{ Reset();	}
	FORCEINLINE void Reset()
	{
		NumInstances = 0;
		RenderUpdateCycles = 0;
		GetDynamicMeshElementsCycles = 0;
	}
	FORCEINLINE uint64 GetTotalCycles() const { return RenderUpdateCycles + GetDynamicMeshElementsCycles; }
	FORCEINLINE uint64 GetPerInstanceAvgCycles() const { return NumInstances > 0 ? (RenderUpdateCycles + GetDynamicMeshElementsCycles) / NumInstances : 0; }

	FParticlePerfStats_RT& operator+=(FParticlePerfStats_RT& Other)
	{
		NumInstances += Other.NumInstances;
		RenderUpdateCycles += Other.RenderUpdateCycles;
		GetDynamicMeshElementsCycles += Other.GetDynamicMeshElementsCycles;
		return *this;
	}
};

/** Stats gathered from the GPU */
struct FParticlePerfStats_GPU
{
	uint64 NumInstances = 0;
	uint64 TotalMicroseconds = 0;

	FORCEINLINE uint64 GetTotalMicroseconds() const { return TotalMicroseconds; }
	FORCEINLINE uint64 GetPerInstanceAvgMicroseconds() const { return NumInstances > 0 ? GetTotalMicroseconds() / NumInstances : 0; }

	FParticlePerfStats_GPU() { Reset(); }
	FORCEINLINE void Reset()
	{
		NumInstances = 0;
		TotalMicroseconds = 0;
	}

	FParticlePerfStats_GPU& operator+=(FParticlePerfStats_GPU& Other)
	{
		NumInstances += Other.NumInstances;
		TotalMicroseconds += Other.TotalMicroseconds;
		return *this;
	}
};

struct FParticlePerfStats
{
	ENGINE_API FParticlePerfStats();

	ENGINE_API void Reset(bool bSyncWithRT);
	ENGINE_API void ResetGT();
	ENGINE_API void ResetRT();
	ENGINE_API void Tick();
	ENGINE_API void TickRT();

	FORCEINLINE static bool GetCSVStatsEnabled() { return bCSVStatsEnabled.Load(EMemoryOrder::Relaxed); }
	FORCEINLINE static bool GetStatsEnabled() { return bStatsEnabled.Load(EMemoryOrder::Relaxed); }
	FORCEINLINE static bool GetGatherWorldStats() { return WorldStatsReaders.Load(EMemoryOrder::Relaxed) > 0; }
	FORCEINLINE static bool GetGatherSystemStats() { return SystemStatsReaders.Load(EMemoryOrder::Relaxed) > 0; }
	FORCEINLINE static bool GetGatherComponentStats() { return ComponentStatsReaders.Load(EMemoryOrder::Relaxed) > 0; }
	FORCEINLINE static bool ShouldGatherStats() 
	{
		return GetStatsEnabled() && 
			(GetGatherWorldStats() 
			#if WITH_PER_SYSTEM_PARTICLE_PERF_STATS
			|| GetGatherSystemStats() 
			#endif
			#if WITH_PER_COMPONENT_PARTICLE_PERF_STATS
			|| GetGatherComponentStats()
			#endif
			);
	}


	FORCEINLINE static void SetCSVStatsEnabled(bool bEnabled) { bCSVStatsEnabled.Store(bEnabled); }
	FORCEINLINE static void SetStatsEnabled(bool bEnabled) { bStatsEnabled.Store(bEnabled); }
	FORCEINLINE static void AddWorldStatReader() { ++WorldStatsReaders; }
	FORCEINLINE static void RemoveWorldStatReader() { --WorldStatsReaders; }
	FORCEINLINE static void AddSystemStatReader() { ++SystemStatsReaders; }
	FORCEINLINE static void RemoveSystemStatReader() { --SystemStatsReaders; }
	FORCEINLINE static void AddComponentStatReader() { ++ComponentStatsReaders; }
	FORCEINLINE static void RemoveComponentStatReader() { --ComponentStatsReaders; }

	static FORCEINLINE FParticlePerfStats* GetStats(const UWorld* World)
	{
		if (World && GetGatherWorldStats() && GetStatsEnabled())
		{
			return GetWorldPerfStats(World);
		}
		return nullptr;
	}
	
	static FORCEINLINE FParticlePerfStats* GetStats(const UFXSystemAsset* System)
	{
	#if WITH_PER_SYSTEM_PARTICLE_PERF_STATS
		if (System && GetGatherSystemStats() && GetStatsEnabled())
		{
			return GetSystemPerfStats(System);
		}
	#endif
		return nullptr;
	}

	static FORCEINLINE FParticlePerfStats* GetStats(const UFXSystemComponent* Component)
	{
	#if WITH_PER_COMPONENT_PARTICLE_PERF_STATS
		if (Component && GetGatherComponentStats() && GetStatsEnabled())
		{
			return GetComponentPerfStats(Component);
		}
	#endif
		return nullptr;
	}

	static ENGINE_API TAtomic<bool>	bStatsEnabled;
	static ENGINE_API TAtomic<int32>	WorldStatsReaders;
	static ENGINE_API TAtomic<int32>	SystemStatsReaders;
	static ENGINE_API TAtomic<int32>	ComponentStatsReaders;

	static ENGINE_API TAtomic<bool>	bCSVStatsEnabled;

	/** Stats on GT and GT spawned concurrent work. */
	FParticlePerfStats_GT GameThreadStats;

	/** Stats on RT work. */
	FParticlePerfStats_RT RenderThreadStats;

	/** Stats from GPU work. */
	FParticlePerfStats_GPU GPUStats;

	/** Returns the current frame Game Thread stats. */
	FORCEINLINE FParticlePerfStats_GT& GetGameThreadStats()
	{
		return GameThreadStats; 
	}

	/** Returns the current frame Render Thread stats. */
	FORCEINLINE FParticlePerfStats_RT& GetRenderThreadStats()
	{
		return RenderThreadStats;
	}

	/** Returns the current frame GPU stats. */
	FORCEINLINE FParticlePerfStats_GPU& GetGPUStats()
	{
		return GPUStats;
	}

private:
	static ENGINE_API FParticlePerfStats* GetWorldPerfStats(const UWorld* World);
	static ENGINE_API FParticlePerfStats* GetSystemPerfStats(const UFXSystemAsset* FXAsset);
	static ENGINE_API FParticlePerfStats* GetComponentPerfStats(const UFXSystemComponent* FXComponent);

};

struct FParticlePerfStatsContext
{
	FParticlePerfStatsContext()
	: WorldStats(nullptr)
#if WITH_PER_SYSTEM_PARTICLE_PERF_STATS
	, SystemStats(nullptr)
#endif
#if WITH_PER_COMPONENT_PARTICLE_PERF_STATS
	, ComponentStats(nullptr)
#endif
	{}

	FORCEINLINE FParticlePerfStatsContext(FParticlePerfStats* InWorldStats, FParticlePerfStats* InSystemStats, FParticlePerfStats* InComponentStats)
	{
		SetWorldStats(InWorldStats);
		SetSystemStats(InSystemStats);
		SetComponentStats(InComponentStats);
	}

	FORCEINLINE FParticlePerfStatsContext(FParticlePerfStats* InWorldStats, FParticlePerfStats* InSystemStats)
	{
		SetWorldStats(InWorldStats);
		SetSystemStats(InSystemStats);
	}

	FORCEINLINE FParticlePerfStatsContext(FParticlePerfStats* InComponentStats)
	{
		SetComponentStats(InComponentStats);
	}

	FORCEINLINE FParticlePerfStatsContext(const UWorld* InWorld, const UFXSystemAsset* InSystem, const UFXSystemComponent* InComponent)
	{
		SetWorldStats(FParticlePerfStats::GetStats(InWorld));
		SetSystemStats(FParticlePerfStats::GetStats(InSystem));
		SetComponentStats(FParticlePerfStats::GetStats(InComponent));
	}

	FORCEINLINE FParticlePerfStatsContext(const UWorld* InWorld, const UFXSystemAsset* InSystem)
	{
		SetWorldStats(FParticlePerfStats::GetStats(InWorld));
		SetSystemStats(FParticlePerfStats::GetStats(InSystem));
	}

	FORCEINLINE FParticlePerfStatsContext(const UFXSystemComponent* InComponent)
	{
		SetComponentStats(FParticlePerfStats::GetStats(InComponent));
	}

	FORCEINLINE bool IsValid()
	{
		return GetWorldStats() != nullptr || GetSystemStats() != nullptr || GetComponentStats() != nullptr;
	}

	FParticlePerfStats* WorldStats = nullptr;
	FORCEINLINE FParticlePerfStats* GetWorldStats() { return WorldStats; }
	FORCEINLINE void SetWorldStats(FParticlePerfStats* Stats) { WorldStats = Stats; }

#if WITH_PER_SYSTEM_PARTICLE_PERF_STATS
	FParticlePerfStats* SystemStats = nullptr;
	FORCEINLINE FParticlePerfStats* GetSystemStats() { return SystemStats; }
	FORCEINLINE void SetSystemStats(FParticlePerfStats* Stats) { SystemStats = Stats; }
#else
	FORCEINLINE FParticlePerfStats* GetSystemStats() { return nullptr; }
	FORCEINLINE void SetSystemStats(FParticlePerfStats* Stats) { }
#endif

#if WITH_PER_COMPONENT_PARTICLE_PERF_STATS
	FParticlePerfStats* ComponentStats = nullptr;
	FORCEINLINE FParticlePerfStats* GetComponentStats() { return ComponentStats; }
	FORCEINLINE void SetComponentStats(FParticlePerfStats* Stats) { ComponentStats = Stats; }
#else
	FORCEINLINE FParticlePerfStats* GetComponentStats() { return nullptr; }
	FORCEINLINE void SetComponentStats(FParticlePerfStats* Stats) { }
#endif
};

typedef TFunction<void(FParticlePerfStats* Stats, uint64 Cycles)> FParticlePerfStatsWriterFunc;

template<typename TWriterFunc>
struct FParticlePerfStatScope
{
	FORCEINLINE FParticlePerfStatScope(FParticlePerfStatsContext InContext, int32 InCount=0)
	: Context(InContext)
	, StartCycles(INDEX_NONE)
	, Count(InCount)
	{
		if (Context.IsValid())
		{
			StartCycles = FPlatformTime::Cycles64();
		}
	}

	FORCEINLINE ~FParticlePerfStatScope()
	{
		if (StartCycles != INDEX_NONE)
		{
			uint64 Cycles = FPlatformTime::Cycles64() - StartCycles;
			TWriterFunc::Write(Context.GetWorldStats(), Cycles, Count);
			TWriterFunc::Write(Context.GetSystemStats(), Cycles, Count);
			TWriterFunc::Write(Context.GetComponentStats(), Cycles, Count);
		}
	}
	
	FParticlePerfStatsContext Context;
	uint64 StartCycles = 0;
	int32 Count = 0;
};

#define PARTICLE_PERF_STAT_CYCLES_COMMON(CONTEXT, THREAD, NAME)\
struct FParticlePerfStatsWriterCycles_##THREAD##_##NAME\
{\
	FORCEINLINE static void Write(FParticlePerfStats* Stats, uint64 Cycles, int32 Count)\
	{\
		if (Stats){ Stats->Get##THREAD##Stats().NAME##Cycles += Cycles;	}\
	}\
};\
FParticlePerfStatScope<FParticlePerfStatsWriterCycles_##THREAD##_##NAME> ANONYMOUS_VARIABLE(ParticlePerfStatScope##THREAD##NAME)(CONTEXT);

#define PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_COMMON(CONTEXT, THREAD, NAME, COUNT)\
struct FParticlePerfStatsWriterCyclesAndCount_##THREAD##_##NAME\
{\
	FORCEINLINE static void Write(FParticlePerfStats* Stats, uint64 Cycles, int32 Count)\
	{\
		if(Stats)\
		{\
			Stats->Get##THREAD##Stats().NAME##Cycles += Cycles; \
			Stats->Get##THREAD##Stats().NumInstances += Count; \
		}\
	}\
};\
FParticlePerfStatScope<FParticlePerfStatsWriterCyclesAndCount_##THREAD##_##NAME> ANONYMOUS_VARIABLE(ParticlePerfStatScope##THREAD##NAME)(CONTEXT, COUNT);

#define PARTICLE_PERF_STAT_CYCLES_GT(CONTEXT, NAME) PARTICLE_PERF_STAT_CYCLES_COMMON(CONTEXT, GameThread, NAME)
#define PARTICLE_PERF_STAT_CYCLES_RT(CONTEXT, NAME) PARTICLE_PERF_STAT_CYCLES_COMMON(CONTEXT, RenderThread, NAME)

#define PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_GT(CONTEXT, NAME, COUNT) PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_COMMON(CONTEXT, GameThread, NAME, COUNT)
#define PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_RT(CONTEXT, NAME, COUNT) PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_COMMON(CONTEXT, RenderThread, NAME, COUNT)

#else //WITH_PARTICLE_PERF_STATS

#define PARTICLE_PERF_STAT_CYCLES_GT(CONTEXT, NAME)
#define PARTICLE_PERF_STAT_CYCLES_RT(CONTEXT, NAME)

#define PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_GT(CONTEXT, NAME, COUNT)
#define PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_RT(CONTEXT, NAME, COUNT)

struct FParticlePerfStatsContext
{
	FORCEINLINE FParticlePerfStatsContext(FParticlePerfStats* InWorldStats, FParticlePerfStats* InSystemStats, FParticlePerfStats* InComponentStats){}
	FORCEINLINE FParticlePerfStatsContext(FParticlePerfStats* InWorldStats, FParticlePerfStats* InSystemStats) {}
	FORCEINLINE FParticlePerfStatsContext(FParticlePerfStats* InComponentStats) {}
	FORCEINLINE FParticlePerfStatsContext(UWorld* InWorld, UFXSystemAsset* InSystem, const UFXSystemComponent* InComponent) {}
	FORCEINLINE FParticlePerfStatsContext(UWorld* InWorld, UFXSystemAsset* InSystem) {}
	FORCEINLINE FParticlePerfStatsContext(const UFXSystemComponent* InComponent) {}
};

#endif //WITH_PARTICLE_PERF_STATS
