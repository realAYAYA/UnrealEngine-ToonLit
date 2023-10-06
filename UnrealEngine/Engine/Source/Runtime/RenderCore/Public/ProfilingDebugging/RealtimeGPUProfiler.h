// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This file contains the various draw mesh macros that display draw calls
 * inside of PIX.
 */

// Colors that are defined for a particular mesh type
// Each event type will be displayed using the defined color
#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StaticArray.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "GpuProfilerTrace.h"
#include "HAL/CriticalSection.h"
#include "MultiGPU.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "UObject/NameTypes.h"

class IRHIComputeContext;
struct FColor;

// Note:  WITH_PROFILEGPU should be 0 for final builds
#define WANTS_DRAW_MESH_EVENTS (RHI_COMMAND_LIST_DEBUG_TRACES || (WITH_PROFILEGPU && PLATFORM_SUPPORTS_DRAW_MESH_EVENTS))

class FRealtimeGPUProfiler;
class FRealtimeGPUProfilerEvent;
class FRealtimeGPUProfilerFrame;
class FRenderQueryPool;
class FScopedGPUStatEvent;

#if RHI_WANT_BREADCRUMB_EVENTS
struct FBreadcrumbEvent
{
	FRHIComputeCommandList* RHICmdList{};

	FORCEINLINE FBreadcrumbEvent(FRHIComputeCommandList& InRHICmdList, const TCHAR* InText)
		: RHICmdList(&InRHICmdList)
	{
		if (RHICmdList)
		{
			RHICmdList->PushBreadcrumb(InText);
		}
	}

	template<typename... Types>
	FORCEINLINE FBreadcrumbEvent(FRHIComputeCommandList& InRHICmdList, const TCHAR* Format, Types... Arguments)
		: RHICmdList(&InRHICmdList)
	{
		if (RHICmdList)
		{
			RHICmdList->PushBreadcrumbPrintf(Format, Arguments...);
		}
	}

	// Terminate the event based upon scope
	FORCEINLINE ~FBreadcrumbEvent()
	{
		if (RHICmdList)
		{
			RHICmdList->PopBreadcrumb();
		}
	}
};

	#define BREADCRUMB_EVENT(RHICmdList, Name) FBreadcrumbEvent PREPROCESSOR_JOIN(BreadcrumbEvent_##Name,__LINE__)(RHICmdList, TEXT(#Name));
	#define BREADCRUMB_EVENTF(RHICmdList, Name, Format, ...) FBreadcrumbEvent PREPROCESSOR_JOIN(BreadcrumbEvent_##Name,__LINE__)(RHICmdList, Format, ##__VA_ARGS__);
#else
	#define BREADCRUMB_EVENT(RHICmdList, Name) do { } while(0)
	#define BREADCRUMB_EVENTF(RHICmdList, Name, Format, ...) do { } while(0)
#endif

#if WANTS_DRAW_MESH_EVENTS

	/**
	 * Class that logs draw events based upon class scope. Draw events can be seen
	 * in PIX
	 */
	struct FDrawEvent
	{
		/** Cmdlist to push onto. */
		FRHIComputeCommandList* RHICmdList;

		/** Indicates whether the event has actually been fired or not. */
		bool bStarted;

		/** Default constructor, initializing all member variables. */
		FORCEINLINE FDrawEvent()
			: RHICmdList(nullptr)
			, bStarted(false)
		{}

		/**
		 * Terminate the event based upon scope
		 */
		FORCEINLINE ~FDrawEvent()
		{
			if (bStarted)
			{
				Stop();
			}
		}

		/**
		 * Functions for logging a PIX event with var args. 
		 * - If Start is called on the rendering or RHI threads, RHICmdList must be non-null. 
		 * - On the game thread, RHICmdList must be nullptr and a render command will be enqueued on the immediate command list
		 * - Stop can be called on any thread but must be called on the same thread Start was called on and will either use the same 
		 *  command list (rendering / RHI thread) or enqueue a command on the current immediate command list (game thread)
		 */
		RENDERCORE_API void CDECL Start(FRHIComputeCommandList* RHICmdList, FColor Color, const TCHAR* Fmt, ...);
		RENDERCORE_API void Stop();
	};

	/** Legacy support for template class version. */
	template <typename TRHICmdList>
	struct TDrawEvent : FDrawEvent {};

	struct FDrawEventRHIExecute
	{
		/** Context to execute on*/
		class IRHIComputeContext* RHICommandContext;

		/** Default constructor, initializing all member variables. */
		FORCEINLINE FDrawEventRHIExecute()
			: RHICommandContext(nullptr)
		{}

		/**
		* Terminate the event based upon scope
		*/
		FORCEINLINE ~FDrawEventRHIExecute()
		{
			if (RHICommandContext)
			{
				Stop();
			}
		}

		/**
		* Function for logging a PIX event with var args
		*/
		RENDERCORE_API void CDECL Start(IRHIComputeContext& InRHICommandContext, FColor Color, const TCHAR* Fmt, ...);
		RENDERCORE_API void Stop();
	};

	// Macros to allow for scoping of draw events outside of RHI function implementations
	// Render-thread event macros:
	#define SCOPED_DRAW_EVENT(RHICmdList, Name) BREADCRUMB_EVENT(RHICmdList, Name); FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, FColor(0), TEXT(#Name));
	#define SCOPED_DRAW_EVENT_COLOR(RHICmdList, Color, Name) BREADCRUMB_EVENT(RHICmdList, Name); FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, Color, TEXT(#Name));
	#define SCOPED_DRAW_EVENTF(RHICmdList, Name, Format, ...) BREADCRUMB_EVENT(RHICmdList, Name); FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Format, ...) BREADCRUMB_EVENT(RHICmdList, Name); FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, Color, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, Name, Condition) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, FColor(0), TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENT_COLOR(RHICmdList, Name, Color, Condition) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, Color, TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, Name, Condition, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Condition, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&RHICmdList, Color, Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF(RHICmdList, Name, Event, Format, ...) if(GetEmitDrawEvents()) (Event).Start(&RHICmdList, FColor(0), Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Event, Format, ...) if(GetEmitDrawEvents()) (Event).Start&(RHICmdList, Color, Format, ##__VA_ARGS__);
	#define STOP_DRAW_EVENT(Event) (Event).Stop();
	// Non-render-thread event macros:
	#define SCOPED_DRAW_EVENT_GAMETHREAD(Name) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, FColor(0), TEXT(#Name));
	#define SCOPED_DRAW_EVENT_COLOR_GAMETHREAD(Color, Name) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, Color, TEXT(#Name));
	#define SCOPED_DRAW_EVENTF_GAMETHREAD(Name, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_DRAW_EVENTF_COLOR_GAMETHREAD(Color, Name, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, Color, Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENT_GAMETHREAD(Name, Condition) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, FColor(0), TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENT_COLOR_GAMETHREAD(Name, Color, Condition) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, Color, TEXT(#Name));
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_GAMETHREAD(Name, Condition, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR_GAMETHREAD(Color, Name, Condition, Format, ...) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(nullptr, Color, Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF_GAMETHREAD(Name, Event, Format, ...) if(GetEmitDrawEvents()) (Event).Start(nullptr, FColor(0), Format, ##__VA_ARGS__);
	#define BEGIN_DRAW_EVENTF_COLOR_GAMETHREAD(Color, Name, Event, Format, ...) if(GetEmitDrawEvents()) (Event).Start(nullptr, Color, Format, ##__VA_ARGS__);
	#define STOP_DRAW_EVENT_GAMETHREAD(Event) (Event).Stop();

	// Deprecated version : use SCOPED_DRAW_... instead:
	#define SCOPED_GPU_EVENT(RHICmdList, Name) SCOPED_DRAW_EVENT(RHICmdList, Name)
	#define SCOPED_GPU_EVENT_COLOR(RHICmdList, Color, Name) SCOPED_DRAW_EVENT_COLOR(RHICmdList, Color, Name)
	#define SCOPED_GPU_EVENTF(RHICmdList, Name, Format, ...) SCOPED_DRAW_EVENTF(RHICmdList, Name, Format, ##__VA_ARGS__)
	#define SCOPED_GPU_EVENTF_COLOR(RHICmdList, Color, Name, Format, ...) SCOPED_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Format, ##__VA_ARGS__)
	#define SCOPED_CONDITIONAL_GPU_EVENT(RHICmdList, Name, Condition) SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, Name, Condition)
	#define SCOPED_CONDITIONAL_GPU_EVENT_COLOR(RHICmdList, Name, Color, Condition) SCOPED_CONDITIONAL_DRAW_EVENT_COLOR(RHICmdList, Name, Color, Condition)
	#define SCOPED_CONDITIONAL_GPU_EVENTF(RHICmdList, Name, Condition, Format, ...) SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, Name, Condition, Format, ##__VA_ARGS__)
	#define SCOPED_CONDITIONAL_GPU_EVENTF_COLOR(RHICmdList, Color, Name, Condition, Format, ...) SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Condition, Format, ##__VA_ARGS__)
	#define BEGIN_GPU_EVENTF(RHICmdList, Name, Event, Format, ...) BEGIN_DRAW_EVENTF(RHICmdList, Name, Event, Format, ##__VA_ARGS__)
	#define BEGIN_GPU_EVENTF_COLOR(RHICmdList, Color, Name, Event, Format, ...) BEGIN_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Event, Format, ##__VA_ARGS__)
	#define STOP_GPU_EVENT(Event) STOP_DRAW_EVENT(Event)

	// Macros to allow for scoping of draw events within RHI function implementations
	#define SCOPED_RHI_DRAW_EVENT(RHICmdContext, Name) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), TEXT(#Name));
	#define SCOPED_RHI_DRAW_EVENT_COLOR(RHICmdContext, Color, Name) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, TEXT(#Name));
	#define SCOPED_RHI_DRAW_EVENTF(RHICmdContext, Name, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_RHI_DRAW_EVENTF_COLOR(RHICmdContext, Color, Name, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, Format, ##__VA_ARGS__);
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT(RHICmdContext, Name, Condition) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), TEXT(#Name));
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT_COLOR(RHICmdContext, Color, Name, Condition) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, TEXT(#Name));
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(RHICmdContext, Name, Condition, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, FColor(0), Format, ##__VA_ARGS__);
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF_COLOR(RHICmdContext, Color, Name, Condition, Format, ...) FDrawEventRHIExecute PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents() && (Condition)) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(RHICmdContext, Color, Format, ##__VA_ARGS__);

#else

	struct FDrawEvent
	{
	};

	#define SCOPED_DRAW_EVENT(RHICmdList, Name) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_DRAW_EVENT_COLOR(RHICmdList, Color, Name) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_DRAW_EVENTF(RHICmdList, Name, Format, ...) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_DRAW_EVENTF_COLOR(RHICmdList, Color, Name, Format, ...) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT_COLOR(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR(...)
	#define BEGIN_DRAW_EVENTF(...)
	#define BEGIN_DRAW_EVENTF_COLOR(...)
	#define STOP_DRAW_EVENT(...)

	#define SCOPED_DRAW_EVENT_GAMETHREAD(Name)
	#define SCOPED_DRAW_EVENT_COLOR_GAMETHREAD(Color, Name)
	#define SCOPED_DRAW_EVENTF_GAMETHREAD(Name, Format, ...)
	#define SCOPED_DRAW_EVENTF_COLOR_GAMETHREAD(Color, Name, Format, ...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT_GAMETHREAD(Name, Condition)
	#define SCOPED_CONDITIONAL_DRAW_EVENT_COLOR_GAMETHREAD(Name, Color, Condition)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_GAMETHREAD(Name, Condition, Format, ...)
	#define SCOPED_CONDITIONAL_DRAW_EVENTF_COLOR_GAMETHREAD(Color, Name, Condition, Format, ...)
	#define BEGIN_DRAW_EVENTF_GAMETHREAD(Name, Event, Format, ...)
	#define BEGIN_DRAW_EVENTF_COLOR_GAMETHREAD(Color, Name, Event, Format, ...)
	#define STOP_DRAW_EVENT_GAMETHREAD(Event)

	#define SCOPED_GPU_EVENT(RHICmdList, Name) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_GPU_EVENT_COLOR(RHICmdList, Color, Name) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_GPU_EVENTF(RHICmdList, Name, Format, ...) BREADCRUMB_EVENT(RHICmdList, Name);
	#define SCOPED_GPU_EVENTF_COLOR(RHICmdList, Color, Name, Format, ...) BREADCRUMB_EVENT(RHICmdList, Name); 
	#define SCOPED_CONDITIONAL_GPU_EVENT(...)
	#define SCOPED_CONDITIONAL_GPU_EVENT_COLOR(...)
	#define SCOPED_CONDITIONAL_GPU_EVENTF(...)
	#define SCOPED_CONDITIONAL_GPU_EVENTF_COLOR(...)
	#define BEGIN_GPU_EVENTF(...)
	#define BEGIN_GPU_EVENTF_COLOR(...)
	#define STOP_GPU_EVENT(...)

	#define SCOPED_RHI_DRAW_EVENT(...)
	#define SCOPED_RHI_DRAW_EVENT_COLOR(...)
	#define SCOPED_RHI_DRAW_EVENTF(...)
	#define SCOPED_RHI_DRAW_EVENTF_COLOR(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENT_COLOR(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(...)
	#define SCOPED_RHI_CONDITIONAL_DRAW_EVENTF_COLOR(...)

#endif

#define SCOPED_COMPUTE_EVENT SCOPED_GPU_EVENT
#define SCOPED_COMPUTE_EVENT_COLOR SCOPED_GPU_EVENT_COLOR
#define SCOPED_COMPUTE_EVENTF SCOPED_GPU_EVENTF
#define SCOPED_COMPUTE_EVENTF_COLOR SCOPED_GPU_EVENTF_COLOR
#define SCOPED_CONDITIONAL_COMPUTE_EVENT SCOPED_CONDITIONAL_GPU_EVENT
#define SCOPED_CONDITIONAL_COMPUTE_EVENT_COLOR SCOPED_CONDITIONAL_GPU_EVENT_COLOR
#define SCOPED_CONDITIONAL_COMPUTE_EVENTF SCOPED_CONDITIONAL_GPU_EVENTF
#define SCOPED_CONDITIONAL_COMPUTE_EVENTF_COLOR SCOPED_CONDITIONAL_GPU_EVENTF_COLOR

#if HAS_GPU_STATS

	CSV_DECLARE_CATEGORY_MODULE_EXTERN(RENDERCORE_API,GPU);

	// The DECLARE_GPU_STAT macros both declare and define a stat (for use in a single CPP)
	#define DECLARE_GPU_STAT(StatName)                            DECLARE_FLOAT_COUNTER_STAT(TEXT(#StatName)       , Stat_GPU_##StatName, STATGROUP_GPU  ); CSV_DEFINE_STAT(GPU,StatName);         static FDrawCallCategoryName DrawcallCountCategory_##StatName;
	#define DECLARE_GPU_STAT_NAMED(StatName, NameString)          DECLARE_FLOAT_COUNTER_STAT(NameString            , Stat_GPU_##StatName, STATGROUP_GPU  ); CSV_DEFINE_STAT(GPU,StatName);         static FDrawCallCategoryName DrawcallCountCategory_##StatName;
	#define DECLARE_GPU_DRAWCALL_STAT(StatName)                   DECLARE_FLOAT_COUNTER_STAT(TEXT(#StatName)       , Stat_GPU_##StatName, STATGROUP_GPU  ); CSV_DEFINE_STAT(GPU,StatName);         static FDrawCallCategoryName DrawcallCountCategory_##StatName((TCHAR*)TEXT(#StatName));
	#define DECLARE_GPU_DRAWCALL_STAT_NAMED(StatName, NameString) DECLARE_FLOAT_COUNTER_STAT(NameString            , Stat_GPU_##StatName, STATGROUP_GPU  ); CSV_DEFINE_STAT(GPU,StatName);         static FDrawCallCategoryName DrawcallCountCategory_##StatName((TCHAR*)TEXT(#StatName));
	#define DECLARE_GPU_DRAWCALL_STAT_EXTERN(StatName)            DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT(#StatName), Stat_GPU_##StatName, STATGROUP_GPU, ); CSV_DECLARE_STAT_EXTERN(GPU,StatName); extern FDrawCallCategoryName DrawcallCountCategory_##StatName;

	// Extern GPU stats are needed where a stat is used in multiple CPPs. Use the DECLARE_GPU_STAT_NAMED_EXTERN in the header and DEFINE_GPU_STAT in the CPPs
	#define DECLARE_GPU_STAT_NAMED_EXTERN(StatName, NameString) DECLARE_FLOAT_COUNTER_STAT_EXTERN(NameString, Stat_GPU_##StatName, STATGROUP_GPU, ); CSV_DECLARE_STAT_EXTERN(GPU,StatName); extern FDrawCallCategoryName DrawcallCountCategory_##StatName;
	#define DEFINE_GPU_STAT(StatName)                           DEFINE_STAT(Stat_GPU_##StatName);                                                    CSV_DEFINE_STAT(GPU,StatName);                FDrawCallCategoryName DrawcallCountCategory_##StatName;
	#define DEFINE_GPU_DRAWCALL_STAT(StatName)                  DEFINE_STAT(Stat_GPU_##StatName);                                                    CSV_DEFINE_STAT(GPU,StatName);                FDrawCallCategoryName DrawcallCountCategory_##StatName((TCHAR*)TEXT(#StatName));

	#if STATS
		#define SCOPED_GPU_STAT(RHICmdList, StatName)                      FScopedGPUStatEvent PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__); PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__).Begin(RHICmdList, CSV_STAT_FNAME(StatName), GET_STATID( Stat_GPU_##StatName ).GetName(), nullptr    , DrawcallCountCategory_##StatName);
		#define SCOPED_GPU_STAT_VERBOSE(RHICmdList, StatName, Description) FScopedGPUStatEvent PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__); PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__).Begin(RHICmdList, CSV_STAT_FNAME(StatName), GET_STATID( Stat_GPU_##StatName ).GetName(), Description, DrawcallCountCategory_##StatName);
	#else
		#define SCOPED_GPU_STAT(RHICmdList, StatName)                      FScopedGPUStatEvent PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__); PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__).Begin(RHICmdList, CSV_STAT_FNAME(StatName), FName(), nullptr    , DrawcallCountCategory_##StatName);
		#define SCOPED_GPU_STAT_VERBOSE(RHICmdList, StatName, Description) FScopedGPUStatEvent PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__); PREPROCESSOR_JOIN(GPUStatEvent_##StatName,__LINE__).Begin(RHICmdList, CSV_STAT_FNAME(StatName), FName(), Description, DrawcallCountCategory_##StatName);
	#endif

	#define GPU_STATS_BEGINFRAME(RHICmdList) FRealtimeGPUProfiler::Get()->BeginFrame(RHICmdList);
	#define GPU_STATS_ENDFRAME(RHICmdList)   FRealtimeGPUProfiler::Get()->EndFrame(RHICmdList);
	#define GPU_STATS_SUSPENDFRAME()         FRealtimeGPUProfiler::Get()->SuspendFrame();

#else

	#define DECLARE_GPU_STAT(StatName)
	#define DECLARE_GPU_STAT_NAMED(StatName, NameString)
	#define DECLARE_GPU_DRAWCALL_STAT(StatName)
	#define DECLARE_GPU_DRAWCALL_STAT_NAMED(StatName, NameString)
	#define DECLARE_GPU_DRAWCALL_STAT_EXTERN(StatName)

	#define DECLARE_GPU_STAT_NAMED_EXTERN(StatName, NameString)
	#define DEFINE_GPU_STAT(StatName)
	#define DEFINE_GPU_DRAWCALL_STAT(StatName)
	#define SCOPED_GPU_STAT(RHICmdList, StatName) 
	#define SCOPED_GPU_STAT_VERBOSE(RHICmdList, StatName, Description)

	#define GPU_STATS_BEGINFRAME(RHICmdList) 
	#define GPU_STATS_ENDFRAME(RHICmdList) 
	#define GPU_STATS_SUSPENDFRAME()

#endif

RENDERCORE_API bool AreGPUStatsEnabled();

#if HAS_GPU_STATS

class FRealtimeGPUProfilerEvent;
class FRealtimeGPUProfilerFrame;
class FRenderQueryPool;

class FRealtimeGPUProfilerQuery
{
public:
	FRealtimeGPUProfilerQuery() = default;
	FRealtimeGPUProfilerQuery(FRHIGPUMask InGPUMask, FRHIRenderQuery* InQuery)
		: GPUMask(InGPUMask)
		, Query(InQuery)
	{}

	void Submit(FRHICommandList& RHICmdList) const
	{
		if (Query)
		{
			SCOPED_GPU_MASK(RHICmdList, GPUMask);
			RHICmdList.EndRenderQuery(Query);
		}
	}

private:
	FRHIGPUMask GPUMask;
	FRHIRenderQuery* Query{};
};

#if GPUPROFILERTRACE_ENABLED
struct FRealtimeGPUProfilerHistoryItem
{
	FRealtimeGPUProfilerHistoryItem();

	static const uint64 HistoryCount = 64;

	// Constructor memsets everything to zero, assuming structure is Plain Old Data.  If any dynamic structures are
	// added, you'll need a more generalized constructor that zeroes out all the uninitialized data.
	bool UpdatedThisFrame;
	FRHIGPUMask LastGPUMask;
	uint64 NextWriteIndex;
	uint64 AccumulatedTime;				// Accumulated time could be computed, but may also be useful to inspect in the debugger
	TStaticArray<uint64, HistoryCount> Times;
};

struct FRealtimeGPUProfilerHistoryByDescription
{
	TMap<FString, FRealtimeGPUProfilerHistoryItem> History;
	mutable FRWLock Mutex;
};

struct FRealtimeGPUProfilerDescriptionResult
{
	// Times are in microseconds
	FString Description;
	FRHIGPUMask GPUMask;
	uint64 AverageTime;
	uint64 MinTime;
	uint64 MaxTime;
};
#endif  // GPUPROFILERTRACE_ENABLED

/**
* FRealtimeGPUProfiler class. This manages recording and reporting all for GPU stats
*/
class FRealtimeGPUProfiler
{
	static FRealtimeGPUProfiler* Instance;
public:
	// Singleton interface
	static RENDERCORE_API FRealtimeGPUProfiler* Get();

	/** *Safe release of the singleton */
	static RENDERCORE_API void SafeRelease();

	/** Per-frame update */
	RENDERCORE_API void BeginFrame(FRHICommandListImmediate& RHICmdList);
	RENDERCORE_API void EndFrame(FRHICommandListImmediate& RHICmdList);
	RENDERCORE_API void SuspendFrame();

	/** Push/pop events */
	FRealtimeGPUProfilerQuery PushEvent(FRHIGPUMask GPUMask, const FName& Name, const FName& StatName, const TCHAR* Description);
	FRealtimeGPUProfilerQuery PopEvent();

	int32 GetCurrentEventIndex() const;

	void PushEventOverride(int32 EventIndex);
	void PopEventOverride();

	/** Push/pop stats which do additional draw call tracking on top of events. */
	void PushStat(FRHICommandListImmediate& RHICmdList, const FName& Name, const FName& StatName, const TCHAR* Description, FDrawCallCategoryName& Category);
	void PopStat(FRHICommandListImmediate& RHICmdList, FDrawCallCategoryName& Category);

#if GPUPROFILERTRACE_ENABLED
	RENDERCORE_API void FetchPerfByDescription(TArray<FRealtimeGPUProfilerDescriptionResult> & OutResults) const;
#endif

private:
	FRealtimeGPUProfiler();

	/** Deinitialize of the object*/
	void Cleanup();


	/** Ringbuffer of profiler frames */
	TArray<FRealtimeGPUProfilerFrame*> Frames;

	int32 WriteBufferIndex;
	int32 ReadBufferIndex;
	uint32 WriteFrameNumber;
	uint32 QueryCount = 0;
	FRenderQueryPoolRHIRef RenderQueryPool;
	bool bStatGatheringPaused;
	bool bInBeginEndBlock;
	bool bLocked = false;

#if GPUPROFILERTRACE_ENABLED
	FRealtimeGPUProfilerHistoryByDescription HistoryByDescription;
#endif
};

/**
* Class that logs GPU Stat events for the realtime GPU profiler
*/
class FScopedGPUStatEvent
{
	/** Cmdlist to push onto. */
	FRHICommandListBase* RHICmdList = nullptr;
	FDrawCallCategoryName* Category = nullptr;

public:
	UE_NONCOPYABLE(FScopedGPUStatEvent)

	FORCEINLINE FScopedGPUStatEvent() = default;

	/**
	* Terminate the event based upon scope
	*/
	FORCEINLINE ~FScopedGPUStatEvent()
	{
		if (RHICmdList)
		{
			End();
		}
	}

	/**
	* Start/Stop functions for timer stats
	*/
	RENDERCORE_API void Begin(FRHICommandListBase& InRHICmdList, const FName& Name, const FName& StatName, const TCHAR* Description, FDrawCallCategoryName& InCategory);
	RENDERCORE_API void End();
};
#endif // HAS_GPU_STATS
