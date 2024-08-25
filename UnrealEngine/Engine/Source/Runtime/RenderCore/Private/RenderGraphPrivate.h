// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/ScopeExit.h"
#include "RenderGraphDefinitions.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "RHI.h"

DEFINE_LOG_CATEGORY_STATIC(LogRDG, Log, All);

#define RDG_DUMP_GRAPH_PRODUCERS 1
#define RDG_DUMP_GRAPH_RESOURCES 2
#define RDG_DUMP_GRAPH_TRACKS 3

#define RDG_ASYNC_COMPUTE_DISABLED 0
#define RDG_ASYNC_COMPUTE_ENABLED 1
#define RDG_ASYNC_COMPUTE_FORCE_ENABLED 2

#define RDG_BREAKPOINT_WARNINGS 1
#define RDG_BREAKPOINT_PASS_COMPILE 2
#define RDG_BREAKPOINT_PASS_EXECUTE 3

#ifndef RDG_ENABLE_PARALLEL_TASKS
#define RDG_ENABLE_PARALLEL_TASKS 1
#endif

#define RDG_RECURSION_COUNTER_SCOPE(Counter) Counter++; ON_SCOPE_EXIT { Counter--; }

#if RDG_ENABLE_DEBUG
extern int32 GRDGAsyncCompute;
extern int32 GRDGClobberResources;
extern int32 GRDGValidation;
extern int32 GRDGDebug;
extern int32 GRDGDebugFlushGPU;
extern int32 GRDGDebugExtendResourceLifetimes;
extern int32 GRDGDebugDisableTransientResources;
extern int32 GRDGBreakpoint;
extern int32 GRDGTransitionLog;
extern int32 GRDGImmediateMode;
extern int32 GRDGOverlapUAVs;
extern bool  GRDGAllowRHIAccess;

class FRDGAllowRHIAccessScope
{
public:
	FRDGAllowRHIAccessScope()
	{
		check(!GRDGAllowRHIAccess);
		GRDGAllowRHIAccess = true;
	}

	~FRDGAllowRHIAccessScope()
	{
		check(GRDGAllowRHIAccess);
		GRDGAllowRHIAccess = false;
	}
};

#define RDG_ALLOW_RHI_ACCESS_SCOPE() FRDGAllowRHIAccessScope RDGAllowRHIAccessScopeRAII;

// Colors for texture / buffer clobbering.
FLinearColor GetClobberColor();
uint32 GetClobberBufferValue();
float GetClobberDepth();
uint8 GetClobberStencil();

bool IsDebugAllowedForGraph(const TCHAR* GraphName);
bool IsDebugAllowedForPass(const TCHAR* PassName);
bool IsDebugAllowedForResource(const TCHAR* ResourceName);

inline void ConditionalDebugBreak(int32 BreakpointCVarValue, const TCHAR* GraphName, const TCHAR* PassName)
{
	if (GRDGBreakpoint == BreakpointCVarValue && IsDebugAllowedForGraph(GraphName) && IsDebugAllowedForPass(PassName))
	{
		UE_DEBUG_BREAK();
	}
}

inline void ConditionalDebugBreak(int32 BreakpointCVarValue, const TCHAR* GraphName, const TCHAR* PassName, const TCHAR* ResourceName)
{
	if (GRDGBreakpoint == BreakpointCVarValue && IsDebugAllowedForGraph(GraphName) && IsDebugAllowedForPass(PassName) && IsDebugAllowedForResource(ResourceName))
	{
		UE_DEBUG_BREAK();
	}
}

void EmitRDGWarning(const FString& WarningMessage);

#define EmitRDGWarningf(WarningMessageFormat, ...) \
	EmitRDGWarning(FString::Printf(WarningMessageFormat, ##__VA_ARGS__));

#else // !RDG_ENABLE_DEBUG

const int32 GRDGClobberResources = 0;
const int32 GRDGValidation = 0;
const int32 GRDGDebug = 0;
const int32 GRDGDebugFlushGPU = 0;
const int32 GRDGDebugExtendResourceLifetimes = 0;
const int32 GRDGDebugDisableTransientResources = 0;
const int32 GRDGBreakpoint = 0;
const int32 GRDGTransitionLog = 0;
const int32 GRDGImmediateMode = 0;
const int32 GRDGOverlapUAVs = 1;

#define RDG_ALLOW_RHI_ACCESS_SCOPE()

#define EmitRDGWarningf(WarningMessageFormat, ...)

#endif

extern int32 GRDGAsyncCompute;
extern int32 GRDGCullPasses;
extern int32 GRDGMergeRenderPasses;
extern int32 GRDGTransientAllocator;
extern int32 GRDGTransientExtractedResources;
extern int32 GRDGTransientIndirectArgBuffers;

#if RDG_ENABLE_PARALLEL_TASKS

extern int32 GRDGParallelDestruction;
extern int32 GRDGParallelSetup;
extern int32 GRDGParallelExecute;
extern int32 GRDGParallelExecutePassMin;
extern int32 GRDGParallelExecutePassMax;

#else

const int32 GRDGParallelDestruction = 0;
const int32 GRDGParallelSetup = 0;
const int32 GRDGParallelExecute = 0;
const int32 GRDGParallelExecutePassMin = 0;
const int32 GRDGParallelExecutePassMax = 0;

#endif

#if CSV_PROFILER
extern int32 GRDGVerboseCSVStats;
#else
const int32 GRDGVerboseCSVStats = 0;
#endif

CSV_DECLARE_CATEGORY_EXTERN(RDGCount);

#define RDG_STATS STATS || COUNTERSTRACE_ENABLED

#if RDG_STATS
extern int32 GRDGStatPassCount;
extern int32 GRDGStatPassCullCount;
extern int32 GRDGStatRenderPassMergeCount;
extern int32 GRDGStatPassDependencyCount;
extern int32 GRDGStatTextureCount;
extern int32 GRDGStatTextureReferenceCount;
extern int32 GRDGStatBufferCount;
extern int32 GRDGStatBufferReferenceCount;
extern int32 GRDGStatViewCount;
extern int32 GRDGStatTransientTextureCount;
extern int32 GRDGStatTransientBufferCount;
extern int32 GRDGStatTransitionCount;
extern int32 GRDGStatAliasingCount;
extern int32 GRDGStatTransitionBatchCount;
extern int32 GRDGStatMemoryWatermark;
#endif

TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_PassCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_PassWithParameterCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_PassCullCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_RenderPassMergeCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_PassDependencyCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_TextureCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_TextureReferenceCount);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_RDG_TextureReferenceAverage);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_BufferCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_BufferReferenceCount);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_RDG_BufferReferenceAverage);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_ViewCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_TransientTextureCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_TransientBufferCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_TransitionCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_AliasingCount);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_RDG_TransitionBatchCount);
TRACE_DECLARE_MEMORY_COUNTER_EXTERN(COUNTER_RDG_MemoryWatermark);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Passes"), STAT_RDG_PassCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Passes With Parameters"), STAT_RDG_PassWithParameterCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Passes Culled"), STAT_RDG_PassCullCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Render Passes Merged"), STAT_RDG_RenderPassMergeCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Pass Dependencies"), STAT_RDG_PassDependencyCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures"), STAT_RDG_TextureCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Texture References"), STAT_RDG_TextureReferenceCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Texture References Average"), STAT_RDG_TextureReferenceAverage, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Buffers"), STAT_RDG_BufferCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Buffer References"), STAT_RDG_BufferReferenceCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Buffer References Average"), STAT_RDG_BufferReferenceAverage, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Views"), STAT_RDG_ViewCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Transient Textures"), STAT_RDG_TransientTextureCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Transient Buffers"), STAT_RDG_TransientBufferCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Resource Transitions"), STAT_RDG_TransitionCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Resource Acquires and Discards"), STAT_RDG_AliasingCount, STATGROUP_RDG, RENDERCORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Resource Transition Batches"), STAT_RDG_TransitionBatchCount, STATGROUP_RDG, RENDERCORE_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Setup"), STAT_RDG_SetupTime, STATGROUP_RDG, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Compile"), STAT_RDG_CompileTime, STATGROUP_RDG, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Execute"), STAT_RDG_ExecuteTime, STATGROUP_RDG, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Collect Resources"), STAT_RDG_CollectResourcesTime, STATGROUP_RDG, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Collect Barriers"), STAT_RDG_CollectBarriersTime, STATGROUP_RDG, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear"), STAT_RDG_ClearTime, STATGROUP_RDG, RENDERCORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Flush RHI Resources"), STAT_RDG_FlushRHIResources, STATGROUP_RDG, RENDERCORE_API);

DECLARE_MEMORY_STAT_EXTERN(TEXT("Builder Watermark"), STAT_RDG_MemoryWatermark, STATGROUP_RDG, RENDERCORE_API);

#if RDG_GPU_DEBUG_SCOPES
extern int32 GRDGEvents;
#endif

#if RDG_EVENTS != RDG_EVENTS_NONE
extern int32 GRDGEmitDrawEvents_RenderThread;
#endif

inline const TCHAR* GetEpilogueBarriersToBeginDebugName(ERHIPipeline Pipelines)
{
#if RDG_ENABLE_DEBUG
	switch (Pipelines)
	{
	case ERHIPipeline::Graphics:
		return TEXT("Epilogue (For Graphics)");
	case ERHIPipeline::AsyncCompute:
		return TEXT("Epilogue (For AsyncCompute)");
	case ERHIPipeline::All:
		return TEXT("Epilogue (For All)");
	}
#endif
	return TEXT("");
}

inline bool SkipUAVBarrier(FRDGViewHandle PreviousHandle, FRDGViewHandle NextHandle)
{
	// Barrier if previous / next don't have a matching valid skip-barrier UAV handle.
	if (GRDGOverlapUAVs != 0 && NextHandle.IsValid() && PreviousHandle == NextHandle)
	{
		return true;
	}

	return false;
}

FORCEINLINE bool IsImmediateMode()
{
	return GRDGImmediateMode != 0;
}

FORCEINLINE bool IsRenderPassMergeEnabled()
{
	return GRDGMergeRenderPasses != 0 && !IsImmediateMode();
}

FORCEINLINE bool IsAsyncComputeSupported()
{
	return GRDGAsyncCompute > 0 && !IsImmediateMode() && GSupportsEfficientAsyncCompute && GRHISupportsSeparateDepthStencilCopyAccess && !GTriggerGPUProfile;
}

extern bool IsParallelExecuteEnabled();
extern bool IsParallelSetupEnabled();

template <typename ResourceRegistryType, typename FunctionType>
inline void EnumerateExtendedLifetimeResources(ResourceRegistryType& Registry, FunctionType Function)
{
#if RDG_ENABLE_DEBUG
	if (GRDGDebugExtendResourceLifetimes)
	{
		for (auto Handle = Registry.Begin(); Handle != Registry.End(); ++Handle)
		{
			auto* Resource = Registry[Handle];

			if (IsDebugAllowedForResource(Resource->Name) && !Resource->IsCulled())
			{
				Function(Resource);
			}
		}
	}
#endif
}
