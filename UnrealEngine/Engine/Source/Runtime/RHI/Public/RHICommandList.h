// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandList.h: RHI Command List definitions for queueing up & executing later.
=============================================================================*/

#pragma once
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/Box2D.h"
#include "Math/PerspectiveMatrix.h"
#include "Math/TranslationMatrix.h"
#include "Math/ScaleMatrix.h"
#include "Math/Float16Color.h"
#include "HAL/ThreadSafeCounter.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MemStack.h"
#include "Misc/App.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Trace/Trace.h"

CSV_DECLARE_CATEGORY_MODULE_EXTERN(RHI_API, RHITStalls);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(RHI_API, RHITFlushes);

// Set to 1 to capture the callstack for every RHI command. Cheap & memory efficient representation: Use the 
// value in FRHICommand::StackFrames to get the pointer to the code (ie paste on a disassembly window)
#define RHICOMMAND_CALLSTACK		0
#if RHICOMMAND_CALLSTACK
#include "HAL/PlatformStackwalk.h"
#endif

class FApp;
class FBlendStateInitializerRHI;
class FGraphicsPipelineStateInitializer;
class FLastRenderTimeContainer;
class FRHICommandListBase;
class FRHIComputeShader;
class IRHICommandContext;
class IRHIComputeContext;
struct FDepthStencilStateInitializerRHI;
struct FRasterizerStateInitializerRHI;
struct FRHIResourceCreateInfo;
struct FRHIResourceInfo;
struct FRHIUniformBufferLayout;
struct FSamplerStateInitializerRHI;
struct FTextureMemoryStats;
class FComputePipelineState;
class FGraphicsPipelineState;
class FRayTracingPipelineState;

DECLARE_STATS_GROUP(TEXT("RHICmdList"), STATGROUP_RHICMDLIST, STATCAT_Advanced);

UE_TRACE_CHANNEL_EXTERN(RHICommandsChannel, RHI_API);

// set this one to get a stat for each RHI command 
#define RHI_STATS 0

#if RHI_STATS
DECLARE_STATS_GROUP(TEXT("RHICommands"),STATGROUP_RHI_COMMANDS, STATCAT_Advanced);
#define RHISTAT(Method)	DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#Method), STAT_RHI##Method, STATGROUP_RHI_COMMANDS)
#else
#define RHISTAT(Method)
#endif

extern RHI_API bool GUseRHIThread_InternalUseOnly;
extern RHI_API bool GUseRHITaskThreads_InternalUseOnly;
extern RHI_API bool GIsRunningRHIInSeparateThread_InternalUseOnly;
extern RHI_API bool GIsRunningRHIInDedicatedThread_InternalUseOnly;
extern RHI_API bool GIsRunningRHIInTaskThread_InternalUseOnly;

namespace ERenderThreadIdleTypes
{
	enum Type
	{
		WaitingForAllOtherSleep,
		WaitingForGPUQuery,
		WaitingForGPUPresent,
		Num
	};
}

/** Accumulates how many cycles the renderthread has been idle. */
extern RHI_API uint32 GRenderThreadIdle[ERenderThreadIdleTypes::Num];
/** Accumulates how times renderthread was idle. */
extern RHI_API uint32 GRenderThreadNumIdle[ERenderThreadIdleTypes::Num];

/** private accumulator for the RHI thread. */
extern RHI_API uint32 GWorkingRHIThreadTime;
extern RHI_API uint32 GWorkingRHIThreadStallTime;
extern RHI_API uint32 GWorkingRHIThreadStartCycles;

/** Helper to mark scopes as idle time on the render or RHI threads. */
struct FRenderThreadIdleScope
{
	const ERenderThreadIdleTypes::Type Type;
	const uint32 Start;
	const bool bCondition;

	FRenderThreadIdleScope(ERenderThreadIdleTypes::Type Type, bool bCondition = true)
		: Type(Type)
		, Start(FPlatformTime::Cycles())
		, bCondition(bCondition)
	{}
	~FRenderThreadIdleScope()
	{
		if (bCondition)
		{
			uint32 End = FPlatformTime::Cycles();
			uint32 IdleCycles = End - Start;

			if (IsInRHIThread())
			{
				GWorkingRHIThreadStallTime += IdleCycles;
			}
			else if (IsInRenderingThread())
			{
				GRenderThreadIdle[Type] += IdleCycles;
				GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForAllOtherSleep]++;
			}
		}
	}
};

/** How many cycles the from sampling input to the frame being flipped. */
extern RHI_API uint64 GInputLatencyTime;

/*UE::Trace::FChannel& FORCEINLINE GetRHICommandsChannel() 
{

}*/

/**
* Whether the RHI commands are being run in a thread other than the render thread
*/
bool FORCEINLINE IsRunningRHIInSeparateThread()
{
	return GIsRunningRHIInSeparateThread_InternalUseOnly;
}

/**
* Whether the RHI commands are being run on a dedicated thread other than the render thread
*/
bool FORCEINLINE IsRunningRHIInDedicatedThread()
{
	return GIsRunningRHIInDedicatedThread_InternalUseOnly;
}

/**
* Whether the RHI commands are being run on a dedicated thread other than the render thread
*/
bool FORCEINLINE IsRunningRHIInTaskThread()
{
	return GIsRunningRHIInTaskThread_InternalUseOnly;
}


extern RHI_API bool GEnableAsyncCompute;
extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdWidth;
extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasks;

#if RHI_RAYTRACING
struct FRayTracingShaderBindings
{
	FRHITexture* Textures[64] = {};
	FRHIShaderResourceView* SRVs[64] = {};
	FRHIUniformBuffer* UniformBuffers[16] = {};
	FRHISamplerState* Samplers[16] = {};
	FRHIUnorderedAccessView* UAVs[16] = {};
};

struct FRayTracingLocalShaderBindings
{
	uint32 InstanceIndex = 0;
	uint32 SegmentIndex = 0;
	uint32 ShaderSlot = 0;
	uint32 ShaderIndexInPipeline = 0;
	uint32 UserData = 0;
	uint16 NumUniformBuffers = 0;
	uint16 LooseParameterDataSize = 0;
	FRHIUniformBuffer** UniformBuffers = nullptr;
	uint8* LooseParameterData = nullptr;
};

enum class ERayTracingBindingType : uint8
{
	HitGroup,
	CallableShader,
	MissShader,
};

// C++ counter-part of FBasicRayData declared in RayTracingCommon.ush
struct UE_DEPRECATED(5.1, "Please use an explicit ray generation shader with a custom intersection structure instead.") FBasicRayData
{
	float Origin[3];
	uint32 Mask;
	float Direction[3];
	float TFar;
};

// C++ counter-part of FIntersectionPayload declared in RayTracingCommon.ush
struct UE_DEPRECATED(5.1, "Please use an explicit ray generation shader with a custom intersection structure instead.") FIntersectionPayload
{
	float  HitT;            // Distance from ray origin to the intersection point in the ray direction. Negative on miss.
	uint32 PrimitiveIndex;  // Index of the primitive within the geometry inside the bottom-level acceleration structure instance. Undefined on miss.
	uint32 InstanceIndex;   // Index of the current instance in the top-level structure. Undefined on miss.
	float  Barycentrics[2]; // Primitive barycentric coordinates of the intersection point. Undefined on miss.
};
#endif // RHI_RAYTRACING

struct RHI_API FLockTracker
{
	struct FLockParams
	{
		void* RHIBuffer;
		void* Buffer;
		uint32 BufferSize;
		uint32 Offset;
		EResourceLockMode LockMode;

		FORCEINLINE_DEBUGGABLE FLockParams(void* InRHIBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize, EResourceLockMode InLockMode)
			: RHIBuffer(InRHIBuffer)
			, Buffer(InBuffer)
			, BufferSize(InBufferSize)
			, Offset(InOffset)
			, LockMode(InLockMode)
		{
		}
	};

	FCriticalSection CriticalSection;
	TArray<FLockParams, TInlineAllocator<16> > OutstandingLocks;
	uint32 TotalMemoryOutstanding;

	FLockTracker()
	{
		TotalMemoryOutstanding = 0;
	}

	FORCEINLINE_DEBUGGABLE void Lock(void* RHIBuffer, void* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		FScopeLock Lock(&CriticalSection);
#if DO_CHECK
		for (auto& Parms : OutstandingLocks)
		{
			check(Parms.RHIBuffer != RHIBuffer);
		}
#endif
		OutstandingLocks.Add(FLockParams(RHIBuffer, Buffer, Offset, SizeRHI, LockMode));
		TotalMemoryOutstanding += SizeRHI;
	}
	FORCEINLINE_DEBUGGABLE FLockParams Unlock(void* RHIBuffer)
	{
		FScopeLock Lock(&CriticalSection);
		for (int32 Index = 0; Index < OutstandingLocks.Num(); Index++)
		{
			if (OutstandingLocks[Index].RHIBuffer == RHIBuffer)
			{
				FLockParams Result = OutstandingLocks[Index];
				OutstandingLocks.RemoveAtSwap(Index, 1, false);
				return Result;
			}
		}
		check(!"Mismatched RHI buffer locks.");
		return FLockParams(nullptr, nullptr, 0, 0, RLM_WriteOnly);
	}
};

#ifdef CONTINUABLE_PSO_VERIFY
#define PSO_VERIFY ensure
#else
#define PSO_VERIFY	check
#endif

struct FRHICommandListDebugContext
{
	FRHICommandListDebugContext()
	{
#if RHI_COMMAND_LIST_DEBUG_TRACES
		DebugStringStore[MaxDebugStoreSize] = 1337;
#endif
	}

	void PushMarker(const TCHAR* Marker)
	{
#if RHI_COMMAND_LIST_DEBUG_TRACES
		//allocate a new slot for the stack of pointers
		//and preserve the top of the stack in case we reach the limit
		if (++DebugMarkerStackIndex >= MaxDebugMarkerStackDepth)
		{
			for (uint32 i = 1; i < MaxDebugMarkerStackDepth; i++)
			{
				DebugMarkerStack[i - 1] = DebugMarkerStack[i];
				DebugMarkerSizes[i - 1] = DebugMarkerSizes[i];
			}
			DebugMarkerStackIndex = MaxDebugMarkerStackDepth - 1;
		}

		//try and copy the sting into the debugstore on the stack
		TCHAR* Offset = &DebugStringStore[DebugStoreOffset];
		uint32 MaxLength = MaxDebugStoreSize - DebugStoreOffset;
		uint32 Length = TryCopyString(Offset, Marker, MaxLength) + 1;

		//if we reached the end reset to the start and try again
		if (Length >= MaxLength)
		{
			DebugStoreOffset = 0;
			Offset = &DebugStringStore[DebugStoreOffset];
			MaxLength = MaxDebugStoreSize;
			Length = TryCopyString(Offset, Marker, MaxLength) + 1;

			//if the sting was bigger than the size of the store just terminate what we have
			if (Length >= MaxDebugStoreSize)
			{
				DebugStringStore[MaxDebugStoreSize - 1] = TEXT('\0');
			}
		}

		//add the string to the stack
		DebugMarkerStack[DebugMarkerStackIndex] = Offset;
		DebugStoreOffset += Length;
		DebugMarkerSizes[DebugMarkerStackIndex] = Length;

		check(DebugStringStore[MaxDebugStoreSize] == 1337);
#endif
	}

	void PopMarker()
	{
#if RHI_COMMAND_LIST_DEBUG_TRACES
		//clean out the debug stack if we have valid data
		if (DebugMarkerStackIndex >= 0 && DebugMarkerStackIndex < MaxDebugMarkerStackDepth)
		{
			DebugMarkerStack[DebugMarkerStackIndex] = nullptr;
			//also free the data in the store to postpone wrapping as much as possibler
			DebugStoreOffset -= DebugMarkerSizes[DebugMarkerStackIndex];

			//in case we already wrapped in the past just assume we start allover again
			if (DebugStoreOffset >= MaxDebugStoreSize)
			{
				DebugStoreOffset = 0;
			}
		}

		//pop the stack pointer
		if (--DebugMarkerStackIndex == (~0u) - 1)
		{
			//in case we wrapped in the past just restart
			DebugMarkerStackIndex = ~0u;
		}
#endif
	}

#if RHI_COMMAND_LIST_DEBUG_TRACES
private:

	//Tries to copy a string and early exits if it hits the limit. 
	//Returns the size of the string or the limit when reached.
	uint32 TryCopyString(TCHAR* Dest, const TCHAR* Source, uint32 MaxLength)
	{
		uint32 Length = 0;
		while(Source[Length] != TEXT('\0') && Length < MaxLength)
		{
			Dest[Length] = Source[Length];
			Length++;
		}

		if (Length < MaxLength)
		{
			Dest[Length] = TEXT('\0');
		}
		return Length;
	}

	uint32 DebugStoreOffset = 0;
	static constexpr int MaxDebugStoreSize = 1023;
	TCHAR DebugStringStore[MaxDebugStoreSize + 1];

	uint32 DebugMarkerStackIndex = ~0u;
	static constexpr int MaxDebugMarkerStackDepth = 32;
	const TCHAR* DebugMarkerStack[MaxDebugMarkerStackDepth] = {};
	uint32 DebugMarkerSizes[MaxDebugMarkerStackDepth] = {};
#endif
};

struct FRHICommandBase
{
	FRHICommandBase* Next = nullptr;
	virtual void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext& DebugContext) = 0;
};

template <typename RHICmdListType, typename LAMBDA>
struct TRHILambdaCommand final : public FRHICommandBase
{
	LAMBDA Lambda;

	TRHILambdaCommand(LAMBDA&& InLambda)
		: Lambda(Forward<LAMBDA>(InLambda))
	{}

	void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext&) override final
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(TRHILambdaCommand, RHICommandsChannel);
		Lambda(*static_cast<RHICmdListType*>(&CmdList));
		Lambda.~LAMBDA();
	}
};

// Using variadic macro because some types are fancy template<A,B> stuff, which gets broken off at the comma and interpreted as multiple arguments. 
#define ALLOC_COMMAND(...) new ( AllocCommand(sizeof(__VA_ARGS__), alignof(__VA_ARGS__)) ) __VA_ARGS__
#define ALLOC_COMMAND_CL(RHICmdList, ...) new ( (RHICmdList).AllocCommand(sizeof(__VA_ARGS__), alignof(__VA_ARGS__)) ) __VA_ARGS__

// This controls if the cmd list bypass can be toggled at runtime. It is quite expensive to have these branches in there.
#define CAN_TOGGLE_COMMAND_LIST_BYPASS (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

#define RHI_COUNT_COMMANDS (DO_CHECK || STATS)

class RHI_API FRHICommandListBase : public FNoncopyable
{
public:
	enum class ERecordingThread
	{
		Render,
		Any
	};

protected:
	FRHICommandListBase(FRHIGPUMask InGPUMask, ERecordingThread InRecordingThread);

public:
	FRHICommandListBase(FRHICommandListBase&& Other);
	~FRHICommandListBase();

	inline bool IsImmediate() const;
	inline FRHICommandListImmediate& GetAsImmediate();
	const int32 GetUsedMemory() const;

	//
	// Adds a graph event as a dispatch dependency. The command list will not be dispatched to the
	// RHI / parallel translate threads until all its dispatch prerequisites have been completed.
	// 
	// Not safe to call after FinishRecording().
	//
	void AddDispatchPrerequisite(const FGraphEventRef& Prereq);

	//
	// Marks the RHI command list as completed, allowing it to be dispatched to the RHI / parallel translate threads.
	// 
	// Must be called as the last command in a parallel rendering task. It is not safe to continue using the command 
	// list after FinishRecording() has been called.
	// 
	// Never call on the immediate command list.
	//
	void FinishRecording();

	UE_DEPRECATED(5.1, "Call FinishRecording instead.")
	void HandleRTThreadTaskCompletion(const FGraphEventRef& MyCompletionGraphEvent)
	{
		FinishRecording();
	}

	void SetCurrentStat(TStatId Stat);

	FORCEINLINE_DEBUGGABLE void* Alloc(int64 AllocSize, int64 Alignment)
	{
		checkSlow(!Bypass() && "Can't use RHICommandList in bypass mode.");
		return MemManager.Alloc(AllocSize, Alignment);
	}

	template <typename T>
	FORCEINLINE_DEBUGGABLE void* Alloc()
	{
		return Alloc(sizeof(T), alignof(T));
	}

	template <typename T>
	FORCEINLINE_DEBUGGABLE const TArrayView<T> AllocArrayUninitialized(uint32 Num)
	{
		return TArrayView<T>((T*)Alloc(Num * sizeof(T), alignof(T)), Num);
	}

	template <typename T>
	FORCEINLINE_DEBUGGABLE const TArrayView<T> AllocArray(const TArrayView<T> InArray)
	{
		// @todo static_assert(TIsTrivial<T>::Value, "Only trivially constructible / copyable types can be used in RHICmdList.");
		void* NewArray = Alloc(InArray.Num() * sizeof(T), alignof(T));
		FMemory::Memcpy(NewArray, InArray.GetData(), InArray.Num() * sizeof(T));
		return TArrayView<T>((T*) NewArray, InArray.Num());
	}

	FORCEINLINE_DEBUGGABLE TCHAR* AllocString(const TCHAR* Name)
	{
		int32 Len = FCString::Strlen(Name) + 1;
		TCHAR* NameCopy  = (TCHAR*)Alloc(Len * (int32)sizeof(TCHAR), (int32)sizeof(TCHAR));
		FCString::Strcpy(NameCopy, Len, Name);
		return NameCopy;
	}

	FORCEINLINE_DEBUGGABLE void* AllocCommand(int32 AllocSize, int32 Alignment)
	{
		checkSlow(!IsExecuting());
		FRHICommandBase* Result = (FRHICommandBase*) MemManager.Alloc(AllocSize, Alignment);
#if RHI_COUNT_COMMANDS
		++NumCommands;
#endif
		*CommandLink = Result;
		CommandLink = &Result->Next;
		return Result;
	}

	template <typename TCmd>
	FORCEINLINE void* AllocCommand()
	{
		return AllocCommand(sizeof(TCmd), alignof(TCmd));
	}

	template <typename LAMBDA>
	FORCEINLINE_DEBUGGABLE void EnqueueLambda(LAMBDA&& Lambda)
	{
		if (IsBottomOfPipe())
		{
			Lambda(*this);
		}
		else
		{
			ALLOC_COMMAND(TRHILambdaCommand<FRHICommandListBase, LAMBDA>)(Forward<LAMBDA>(Lambda));
		}
	}

	FORCEINLINE uint32 GetUID()  const
	{
		return UID;
	}

	FORCEINLINE bool HasCommands() const
	{
		return Root != nullptr;
	}

	FORCEINLINE bool IsExecuting() const
	{
		return bExecuting;
	}

	FORCEINLINE bool IsBottomOfPipe() const
	{
		return Bypass() || IsExecuting();
	}

	FORCEINLINE bool IsTopOfPipe() const
	{
		return !IsBottomOfPipe();
	}

	FORCEINLINE bool IsGraphics() const
	{
		return ActivePipeline == ERHIPipeline::Graphics;
	}

	FORCEINLINE bool IsAsyncCompute() const
	{
		return ActivePipeline == ERHIPipeline::AsyncCompute;
	}

	FORCEINLINE ERHIPipeline GetPipeline() const
	{
		return ActivePipeline;
	}

	FORCEINLINE IRHICommandContext& GetContext()
	{
		checkf(GraphicsContext, TEXT("There is no active graphics context on this command list. There may be a missing call to SwitchPipeline()."));
		return *GraphicsContext;
	}

	FORCEINLINE IRHIComputeContext& GetComputeContext()
	{
		checkf(ComputeContext, TEXT("There is no active compute context on this command list. There may be a missing call to SwitchPipeline()."));
		return *ComputeContext;
	}

	inline bool Bypass() const;

	ERHIPipeline SwitchPipeline(ERHIPipeline Pipeline);

	FORCEINLINE FRHIGPUMask GetGPUMask() const { return PersistentState.CurrentGPUMask; }

	bool AsyncPSOCompileAllowed() const { return PersistentState.bAsyncPSOCompileAllowed; }
	bool IsOutsideRenderPass   () const { return !PersistentState.bInsideRenderPass; }
	bool IsInsideRenderPass    () const { return PersistentState.bInsideRenderPass;  }
	bool IsInsideComputePass   () const { return PersistentState.bInsideComputePass; }

	void SetExecuteStat(TStatId Stat) { ExecuteStat = Stat; }

	FGraphEventRef RHIThreadFence(bool bSetLockFence = false);

	FORCEINLINE void* LockBuffer(FRHIBuffer* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->RHILockBuffer(*this, Buffer, Offset, SizeRHI, LockMode);
	}

	FORCEINLINE void UnlockBuffer(FRHIBuffer* Buffer)
	{
		GDynamicRHI->RHIUnlockBuffer(*this, Buffer);
	}

	FORCEINLINE FBufferRHIRef CreateBuffer(uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
	{
		FBufferRHIRef Buffer = GDynamicRHI->RHICreateBuffer(*this, Size, Usage, Stride, ResourceState, CreateInfo);
		Buffer->SetTrackedAccess_Unsafe(ResourceState);
		return Buffer;
	}

	FORCEINLINE void UpdateUniformBuffer(FRHIUniformBuffer* UniformBufferRHI, const void* Contents)
	{
		GDynamicRHI->RHIUpdateUniformBuffer(*this, UniformBufferRHI, Contents);
	}

protected:
	FMemStackBase& GetAllocator() { return MemManager; }

	FORCEINLINE void ValidateBoundShader(FRHIVertexShader*        ShaderRHI) { checkSlow(PersistentState.BoundShaderInput.VertexShaderRHI          == ShaderRHI); }
	FORCEINLINE void ValidateBoundShader(FRHIPixelShader*         ShaderRHI) { checkSlow(PersistentState.BoundShaderInput.PixelShaderRHI           == ShaderRHI); }
	FORCEINLINE void ValidateBoundShader(FRHIGeometryShader*      ShaderRHI) { checkSlow(PersistentState.BoundShaderInput.GetGeometryShader()      == ShaderRHI); }
	FORCEINLINE void ValidateBoundShader(FRHIComputeShader*       ShaderRHI) { checkSlow(PersistentState.BoundComputeShaderRHI                     == ShaderRHI); }
	FORCEINLINE void ValidateBoundShader(FRHIMeshShader*          ShaderRHI) { checkSlow(PersistentState.BoundShaderInput.GetMeshShader()          == ShaderRHI); }
	FORCEINLINE void ValidateBoundShader(FRHIAmplificationShader* ShaderRHI) { checkSlow(PersistentState.BoundShaderInput.GetAmplificationShader() == ShaderRHI); }

	FORCEINLINE void ValidateBoundShader(FRHIGraphicsShader* ShaderRHI)
	{
#if DO_GUARD_SLOW
		switch (ShaderRHI->GetFrequency())
		{
		case SF_Vertex:        checkSlow(PersistentState.BoundShaderInput.VertexShaderRHI          == ShaderRHI); break;
		case SF_Mesh:          checkSlow(PersistentState.BoundShaderInput.GetMeshShader()          == ShaderRHI); break;
		case SF_Amplification: checkSlow(PersistentState.BoundShaderInput.GetAmplificationShader() == ShaderRHI); break;
		case SF_Pixel:         checkSlow(PersistentState.BoundShaderInput.PixelShaderRHI           == ShaderRHI); break;
		case SF_Geometry:      checkSlow(PersistentState.BoundShaderInput.GetGeometryShader()      == ShaderRHI); break;
		default: checkfSlow(false, TEXT("Unexpected graphics shader type %d"), ShaderRHI->GetFrequency());
		}
#endif // DO_GUARD_SLOW
	}

	void CacheActiveRenderTargets(const FRHIRenderPassInfo& Info)
	{
		FRHISetRenderTargetsInfo RTInfo;
		Info.ConvertToRenderTargetsInfo(RTInfo);

		for (int32 RTIdx = 0; RTIdx < RTInfo.NumColorRenderTargets; ++RTIdx)
		{
			PersistentState.CachedRenderTargets[RTIdx] = RTInfo.ColorRenderTarget[RTIdx];
		}

		PersistentState.CachedNumSimultanousRenderTargets = RTInfo.NumColorRenderTargets;
		PersistentState.CachedDepthStencilTarget = RTInfo.DepthStencilRenderTarget;
		PersistentState.HasFragmentDensityAttachment = RTInfo.ShadingRateTexture != nullptr;
		PersistentState.MultiViewCount = RTInfo.MultiViewCount;
	}

	void IncrementSubpass()
	{
		PersistentState.SubpassIndex++;
	}
	
	void ResetSubpass(ESubpassHint SubpassHint)
	{
		PersistentState.SubpassHint = SubpassHint;
		PersistentState.SubpassIndex = 0;
	}

private:
	// Replays recorded commands into the specified contexts.
	// Used internally, do not call directly.
	void Execute(TRHIPipelineArray<IRHIComputeContext*>& InOutPipeContexts);

protected:
	// Blocks the calling thread until the dispatch event is completed.
	// Used internally, do not call directly.
	void WaitForDispatchEvent();

	FRHICommandBase*    Root            = nullptr;
	FRHICommandBase**   CommandLink     = nullptr;

	// The active context into which graphics commands are recorded.
	IRHICommandContext* GraphicsContext = nullptr;

	// The active compute context into which (possibly async) compute commands are recorded.
	IRHIComputeContext* ComputeContext  = nullptr;

	// The RHI contexts available to the command list during execution.
	// These are always set for the immediate command list, see InitializeImmediateContexts().
	TRHIPipelineArray<IRHIComputeContext*> Contexts = {};

#if RHI_COUNT_COMMANDS
	uint32 NumCommands = 0;
#endif
	uint32 UID         = UINT32_MAX;
	bool bExecuting    = false;

	// The currently selected pipeline that RHI commands are directed to, during command list recording.
	// This is also adjusted during command list execution based on recorded use of SwitchPipeline().
	ERHIPipeline ActivePipeline = ERHIPipeline::None;

#if DO_CHECK
	// Used to check for valid pipelines passed to SwitchPipeline().
	ERHIPipeline AllowedPipelines = ERHIPipeline::All;
#endif

	// Graph event used to gate the execution of the command list on the completion of any dependent tasks
	// e.g. PSO async compilation and parallel RHICmdList recording tasks.
	FGraphEventRef DispatchEvent;

	TStatId	ExecuteStat = {};
	FMemStackBase MemManager;

	// The values in this struct are preserved when the command list is moved or reset.
	struct FPersistentState
	{
		uint32 CachedNumSimultanousRenderTargets = 0;
		TStaticArray<FRHIRenderTargetView, MaxSimultaneousRenderTargets> CachedRenderTargets;
		FRHIDepthRenderTargetView CachedDepthStencilTarget;

		ESubpassHint SubpassHint = ESubpassHint::None;
		uint8 SubpassIndex = 0;
		uint8 MultiViewCount = 0;
		bool HasFragmentDensityAttachment = false;

		bool bInsideRenderPass = false;
		bool bInsideComputePass = false;
		bool bInsideOcclusionQueryBatch = false;
		bool bAsyncPSOCompileAllowed = true;
		bool bImmediate = false;

		ERecordingThread RecordingThread;

		FRHIGPUMask CurrentGPUMask;
		FRHIGPUMask InitialGPUMask;

		FBoundShaderStateInput BoundShaderInput;
		FRHIComputeShader* BoundComputeShaderRHI = nullptr;

		FGraphEventRef RHIThreadBufferLockFence;

		struct FFenceCandidate : public TConcurrentLinearObject<FFenceCandidate>, public FRefCountBase
		{
			FGraphEventRef Fence;
		};

		TRefCountPtr<FFenceCandidate> FenceCandidate;
		FGraphEventArray QueuedFenceCandidateEvents;
		TArray<TRefCountPtr<FFenceCandidate>, FConcurrentLinearArrayAllocator> QueuedFenceCandidates;

		FPersistentState(FRHIGPUMask InInitialGPUMask, ERecordingThread InRecordingThread)
			: RecordingThread(InRecordingThread)
			, CurrentGPUMask(InInitialGPUMask)
			, InitialGPUMask(InInitialGPUMask)
		{}

	} PersistentState;

#if RHI_WANT_BREADCRUMB_EVENTS
public:
	struct FBreadcrumbs
	{
		enum { MaxStacks = 4 };
		const FRHIBreadcrumb* StackTop[MaxStacks] = {}; // Top of the breadcrumb stack on the RHI thread.
		int32 StackIndex = 0; // Index into the breadcrumbs, incremented for each command list submit and decremented when complete.
		FRHIBreadcrumbStack Stack;

		inline void SetStackTop(const FRHIBreadcrumb* InStackTop)
		{
			if (ensure(StackIndex >= 0))
			{
				StackTop[StackIndex] = InStackTop;
			}
		}

		inline bool PushStack()
		{
			bool DoPop = false;
			if (StackIndex < FBreadcrumbs::MaxStacks - 1)
			{
				StackIndex++;
				DoPop = true;
			}

			// If we can't fit a next stack in, we have to stomp the top one, the show must go on.
			SetStackTop(Stack.PopFirstUnsubmittedBreadcrumb());

			return DoPop;
		}

		inline void PopStack()
		{
			StackIndex--;
		}
	} Breadcrumbs = {};

	void InheritBreadcrumbs(const FRHICommandListBase& Parent) { Breadcrumbs.Stack.DeepCopy(GetAllocator(), Parent.Breadcrumbs.Stack); }
	template <typename AllocatorType> void ExportBreadcrumbState(TRHIBreadcrumbState<AllocatorType>& State) const { Breadcrumbs.Stack.ExportBreadcrumbState(State); }
	template <typename AllocatorType> void ImportBreadcrumbState(const TRHIBreadcrumbState<AllocatorType>& State) { Breadcrumbs.Stack.ImportBreadcrumbState(GetAllocator(), State); }
#endif

public:
	TStaticArray<void*, MAX_NUM_GPUS> QueryBatchData { InPlace, nullptr };

private:
	FRHICommandListBase(FPersistentState&& InPersistentState);

	friend class FRHICommandListExecutor;
	friend class FRHICommandListIterator;
	friend class FRHICommandListScopedFlushAndExecute;
	friend class FRHIComputeCommandList;
	friend class FRHICommandListImmediate;
	friend class FRHICommandList_RecursiveHazardous;
	friend class FRHIComputeCommandList_RecursiveHazardous;
	friend struct FRHICommandSetGPUMask;
};

struct FUnnamedRhiCommand
{
	static const TCHAR* TStr() { return TEXT("FUnnamedRhiCommand"); }
};

template<typename TCmd, typename NameType = FUnnamedRhiCommand>
struct FRHICommand : public FRHICommandBase
{
#if RHICOMMAND_CALLSTACK
	uint64 StackFrames[16];

	FRHICommand()
	{
		FPlatformStackWalk::CaptureStackBackTrace(StackFrames, 16);
	}
#endif

	void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext& Context) override final
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CommandList/ExecuteAndDestruct"));
		TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL_STR(NameType::TStr(), RHICommandsChannel);

		TCmd* ThisCmd = static_cast<TCmd*>(this);
#if RHI_COMMAND_LIST_DEBUG_TRACES
		ThisCmd->StoreDebugInfo(Context);
#endif
		ThisCmd->Execute(CmdList);
		ThisCmd->~TCmd();
	}

	virtual void StoreDebugInfo(FRHICommandListDebugContext& Context) {};
};

#define FRHICOMMAND_MACRO(CommandName)								\
struct PREPROCESSOR_JOIN(CommandName##String, __LINE__)				\
{																	\
	static const TCHAR* TStr() { return TEXT(#CommandName); }		\
};																	\
struct CommandName final : public FRHICommand<CommandName, PREPROCESSOR_JOIN(CommandName##String, __LINE__)>

FRHICOMMAND_MACRO(FRHICommandBeginUpdateMultiFrameResource)
{
	FRHITexture* Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandBeginUpdateMultiFrameResource(FRHITexture* InTexture)
		: Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndUpdateMultiFrameResource)
{
	FRHITexture* Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandEndUpdateMultiFrameResource(FRHITexture* InTexture)
		: Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginUpdateMultiFrameUAV)
{
	FRHIUnorderedAccessView* UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandBeginUpdateMultiFrameUAV(FRHIUnorderedAccessView* InUAV)
		: UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndUpdateMultiFrameUAV)
{
	FRHIUnorderedAccessView* UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandEndUpdateMultiFrameUAV(FRHIUnorderedAccessView* InUAV)
		: UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

#if WITH_MGPU
FRHICOMMAND_MACRO(FRHICommandSetGPUMask)
{
	FRHIGPUMask GPUMask;
	FORCEINLINE_DEBUGGABLE FRHICommandSetGPUMask(FRHIGPUMask InGPUMask)
		: GPUMask(InGPUMask)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandWaitForTemporalEffect)
{
	FName EffectName;
	FORCEINLINE_DEBUGGABLE FRHICommandWaitForTemporalEffect(const FName& InEffectName)
		: EffectName(InEffectName)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBroadcastTemporalEffectString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandBroadcastTemporalEffect"); }
};
template <typename TRHIResource>
struct FRHICommandBroadcastTemporalEffect final	: public FRHICommand<FRHICommandBroadcastTemporalEffect<TRHIResource>, FRHICommandBroadcastTemporalEffectString>
{
	FName EffectName;
	const TArrayView<TRHIResource*> Resources;
	FORCEINLINE_DEBUGGABLE FRHICommandBroadcastTemporalEffect(const FName& InEffectName, const TArrayView<TRHIResource*> InResources)
		: EffectName(InEffectName)
		, Resources(InResources)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandTransferResources)
{
	TArray<FTransferResourceParams, TInlineAllocator<4>> Params;

	FORCEINLINE_DEBUGGABLE FRHICommandTransferResources(TArrayView<const FTransferResourceParams> InParams)
		: Params(InParams)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandTransferResourceSignal)
{
	TArray<FTransferResourceFenceData*, TInlineAllocator<1>> FenceDatas;
	FRHIGPUMask SrcGPUMask;

	FORCEINLINE_DEBUGGABLE FRHICommandTransferResourceSignal(TArrayView<FTransferResourceFenceData* const> InFenceDatas, FRHIGPUMask InSrcGPUMask)
		: FenceDatas(InFenceDatas), SrcGPUMask(InSrcGPUMask)
	{
	}

	RHI_API void Execute(FRHICommandListBase & CmdList);
};

FRHICOMMAND_MACRO(FRHICommandTransferResourceWait)
{
	TArray<FTransferResourceFenceData*, TInlineAllocator<4>> FenceDatas;

	FORCEINLINE_DEBUGGABLE FRHICommandTransferResourceWait(TArrayView<FTransferResourceFenceData* const> InFenceDatas)
		: FenceDatas(InFenceDatas)
	{
	}

	RHI_API void Execute(FRHICommandListBase & CmdList);
};

#endif // WITH_MGPU

FRHICOMMAND_MACRO(FRHICommandSetStencilRef)
{
	uint32 StencilRef;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStencilRef(uint32 InStencilRef)
		: StencilRef(InStencilRef)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetShaderParameterString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetShaderParameter"); }
};
template <typename TRHIShader>
struct FRHICommandSetShaderParameter final : public FRHICommand<FRHICommandSetShaderParameter<TRHIShader>, FRHICommandSetShaderParameterString>
{
	TRHIShader* Shader;
	const void* NewValue;
	uint32 BufferIndex;
	uint32 BaseIndex;
	uint32 NumBytes;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderParameter(TRHIShader* InShader, uint32 InBufferIndex, uint32 InBaseIndex, uint32 InNumBytes, const void* InNewValue)
		: Shader(InShader)
		, NewValue(InNewValue)
		, BufferIndex(InBufferIndex)
		, BaseIndex(InBaseIndex)
		, NumBytes(InNumBytes)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetShaderUniformBufferString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetShaderUniformBuffer"); }
};
template <typename TRHIShader>
struct FRHICommandSetShaderUniformBuffer final : public FRHICommand<FRHICommandSetShaderUniformBuffer<TRHIShader>, FRHICommandSetShaderUniformBufferString>
{
	TRHIShader* Shader;
	uint32 BaseIndex;
	TRefCountPtr<FRHIUniformBuffer> UniformBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderUniformBuffer(TRHIShader* InShader, uint32 InBaseIndex, FRHIUniformBuffer* InUniformBuffer)
		: Shader(InShader)
		, BaseIndex(InBaseIndex)
		, UniformBuffer(InUniformBuffer)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetShaderTextureString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetShaderTexture"); }
};
template <typename TRHIShader>
struct FRHICommandSetShaderTexture final : public FRHICommand<FRHICommandSetShaderTexture<TRHIShader>, FRHICommandSetShaderTextureString >
{
	TRHIShader* Shader;
	uint32 TextureIndex;
	FRHITexture* Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderTexture(TRHIShader* InShader, uint32 InTextureIndex, FRHITexture* InTexture)
		: Shader(InShader)
		, TextureIndex(InTextureIndex)
		, Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetShaderResourceViewParameterString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetShaderResourceViewParameter"); }
};
template <typename TRHIShader>
struct FRHICommandSetShaderResourceViewParameter final : public FRHICommand<FRHICommandSetShaderResourceViewParameter<TRHIShader>, FRHICommandSetShaderResourceViewParameterString >
{
	TRHIShader* Shader;
	uint32 SamplerIndex;
	FRHIShaderResourceView* SRV;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderResourceViewParameter(TRHIShader* InShader, uint32 InSamplerIndex, FRHIShaderResourceView* InSRV)
		: Shader(InShader)
		, SamplerIndex(InSamplerIndex)
		, SRV(InSRV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetUAVParameterString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetUAVParameter"); }
};
template <typename TRHIShader>
struct FRHICommandSetUAVParameter final : public FRHICommand<FRHICommandSetUAVParameter<TRHIShader>, FRHICommandSetUAVParameterString >
{
	TRHIShader* Shader;
	uint32 UAVIndex;
	FRHIUnorderedAccessView* UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandSetUAVParameter(TRHIShader* InShader, uint32 InUAVIndex, FRHIUnorderedAccessView* InUAV)
		: Shader(InShader)
		, UAVIndex(InUAVIndex)
		, UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetUAVParameter_InitialCountString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetUAVParameter_InitialCount"); }
};
struct FRHICommandSetUAVParameter_InitialCount final : public FRHICommand<FRHICommandSetUAVParameter_InitialCount, FRHICommandSetUAVParameter_InitialCountString >
{
	FRHIComputeShader* Shader;
	uint32 UAVIndex;
	FRHIUnorderedAccessView* UAV;
	uint32 InitialCount;
	FORCEINLINE_DEBUGGABLE FRHICommandSetUAVParameter_InitialCount(FRHIComputeShader* InShader, uint32 InUAVIndex, FRHIUnorderedAccessView* InUAV, uint32 InInitialCount)
		: Shader(InShader)
		, UAVIndex(InUAVIndex)
		, UAV(InUAV)
		, InitialCount(InInitialCount)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetShaderSamplerString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetShaderSampler"); }
};
template <typename TRHIShader>
struct FRHICommandSetShaderSampler final : public FRHICommand<FRHICommandSetShaderSampler<TRHIShader>, FRHICommandSetShaderSamplerString >
{
	TRHIShader* Shader;
	uint32 SamplerIndex;
	FRHISamplerState* Sampler;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderSampler(TRHIShader* InShader, uint32 InSamplerIndex, FRHISamplerState* InSampler)
		: Shader(InShader)
		, SamplerIndex(InSamplerIndex)
		, Sampler(InSampler)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDrawPrimitive)
{
	uint32 BaseVertexIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitive(uint32 InBaseVertexIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: BaseVertexIndex(InBaseVertexIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDrawIndexedPrimitive)
{
	FRHIBuffer* IndexBuffer;
	int32 BaseVertexIndex;
	uint32 FirstInstance;
	uint32 NumVertices;
	uint32 StartIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedPrimitive(FRHIBuffer* InIndexBuffer, int32 InBaseVertexIndex, uint32 InFirstInstance, uint32 InNumVertices, uint32 InStartIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: IndexBuffer(InIndexBuffer)
		, BaseVertexIndex(InBaseVertexIndex)
		, FirstInstance(InFirstInstance)
		, NumVertices(InNumVertices)
		, StartIndex(InStartIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetBlendFactor)
{
	FLinearColor BlendFactor;
	FORCEINLINE_DEBUGGABLE FRHICommandSetBlendFactor(const FLinearColor& InBlendFactor)
		: BlendFactor(InBlendFactor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetStreamSource)
{
	uint32 StreamIndex;
	FRHIBuffer* VertexBuffer;
	uint32 Offset;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStreamSource(uint32 InStreamIndex, FRHIBuffer* InVertexBuffer, uint32 InOffset)
		: StreamIndex(InStreamIndex)
		, VertexBuffer(InVertexBuffer)
		, Offset(InOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetViewport)
{
	float MinX;
	float MinY;
	float MinZ;
	float MaxX;
	float MaxY;
	float MaxZ;
	FORCEINLINE_DEBUGGABLE FRHICommandSetViewport(float InMinX, float InMinY, float InMinZ, float InMaxX, float InMaxY, float InMaxZ)
		: MinX(InMinX)
		, MinY(InMinY)
		, MinZ(InMinZ)
		, MaxX(InMaxX)
		, MaxY(InMaxY)
		, MaxZ(InMaxZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetStereoViewport)
{
	float LeftMinX;
	float RightMinX;
	float LeftMinY;
	float RightMinY;
	float MinZ;
	float LeftMaxX;
	float RightMaxX;
	float LeftMaxY;
	float RightMaxY;
	float MaxZ;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStereoViewport(float InLeftMinX, float InRightMinX, float InLeftMinY, float InRightMinY, float InMinZ, float InLeftMaxX, float InRightMaxX, float InLeftMaxY, float InRightMaxY, float InMaxZ)
		: LeftMinX(InLeftMinX)
		, RightMinX(InRightMinX)
		, LeftMinY(InLeftMinY)
		, RightMinY(InRightMinY)
		, MinZ(InMinZ)
		, LeftMaxX(InLeftMaxX)
		, RightMaxX(InRightMaxX)
		, LeftMaxY(InLeftMaxY)
		, RightMaxY(InRightMaxY)
		, MaxZ(InMaxZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetScissorRect)
{
	bool bEnable;
	uint32 MinX;
	uint32 MinY;
	uint32 MaxX;
	uint32 MaxY;
	FORCEINLINE_DEBUGGABLE FRHICommandSetScissorRect(bool InbEnable, uint32 InMinX, uint32 InMinY, uint32 InMaxX, uint32 InMaxY)
		: bEnable(InbEnable)
		, MinX(InMinX)
		, MinY(InMinY)
		, MaxX(InMaxX)
		, MaxY(InMaxY)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginRenderPass)
{
	FRHIRenderPassInfo Info;
	const TCHAR* Name;

	FRHICommandBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
		: Info(InInfo)
		, Name(InName)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndRenderPass)
{
	FRHICommandEndRenderPass()
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandNextSubpass)
{
	FRHICommandNextSubpass()
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FLocalCmdListParallelRenderPass
{
	TRefCountPtr<struct FRHIParallelRenderPass> RenderPass;
};

FRHICOMMAND_MACRO(FRHICommandBeginParallelRenderPass)
{
	FRHIRenderPassInfo Info;
	FLocalCmdListParallelRenderPass* LocalRenderPass;
	const TCHAR* Name;

	FRHICommandBeginParallelRenderPass(const FRHIRenderPassInfo& InInfo, FLocalCmdListParallelRenderPass* InLocalRenderPass, const TCHAR* InName)
		: Info(InInfo)
		, LocalRenderPass(InLocalRenderPass)
		, Name(InName)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndParallelRenderPass)
{
	FLocalCmdListParallelRenderPass* LocalRenderPass;

	FRHICommandEndParallelRenderPass(FLocalCmdListParallelRenderPass* InLocalRenderPass)
		: LocalRenderPass(InLocalRenderPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FLocalCmdListRenderSubPass
{
	TRefCountPtr<struct FRHIRenderSubPass> RenderSubPass;
};

FRHICOMMAND_MACRO(FRHICommandBeginRenderSubPass)
{
	FLocalCmdListParallelRenderPass* LocalRenderPass;
	FLocalCmdListRenderSubPass* LocalRenderSubPass;

	FRHICommandBeginRenderSubPass(FLocalCmdListParallelRenderPass* InLocalRenderPass, FLocalCmdListRenderSubPass* InLocalRenderSubPass)
		: LocalRenderPass(InLocalRenderPass)
		, LocalRenderSubPass(InLocalRenderSubPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndRenderSubPass)
{
	FLocalCmdListParallelRenderPass* LocalRenderPass;
	FLocalCmdListRenderSubPass* LocalRenderSubPass;

	FRHICommandEndRenderSubPass(FLocalCmdListParallelRenderPass* InLocalRenderPass, FLocalCmdListRenderSubPass* InLocalRenderSubPass)
		: LocalRenderPass(InLocalRenderPass)
		, LocalRenderSubPass(InLocalRenderSubPass)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetComputeShaderString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetComputeShader"); }
};

struct FRHICommandSetComputeShader final : public FRHICommand<FRHICommandSetComputeShader, FRHICommandSetComputeShaderString>
{
	FRHIComputeShader* ComputeShader;
	FORCEINLINE_DEBUGGABLE FRHICommandSetComputeShader(FRHIComputeShader* InComputeShader)
		: ComputeShader(InComputeShader)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetComputePipelineStateString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetComputePipelineState"); }
};

struct FRHICommandSetComputePipelineState final : public FRHICommand<FRHICommandSetComputePipelineState, FRHICommandSetComputePipelineStateString>
{
	FComputePipelineState* ComputePipelineState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetComputePipelineState(FComputePipelineState* InComputePipelineState)
		: ComputePipelineState(InComputePipelineState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetGraphicsPipelineState)
{
	FGraphicsPipelineState* GraphicsPipelineState;
	uint32 StencilRef;
	bool bApplyAdditionalState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetGraphicsPipelineState(FGraphicsPipelineState* InGraphicsPipelineState, uint32 InStencilRef, bool bInApplyAdditionalState)
		: GraphicsPipelineState(InGraphicsPipelineState)
		, StencilRef(InStencilRef)
		, bApplyAdditionalState(bInApplyAdditionalState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

#if PLATFORM_USE_FALLBACK_PSO
FRHICOMMAND_MACRO(FRHICommandSetGraphicsPipelineStateFromInitializer)
{
	FGraphicsPipelineStateInitializer PsoInit;
	uint32 StencilRef;
	bool bApplyAdditionalState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetGraphicsPipelineStateFromInitializer(const FGraphicsPipelineStateInitializer& InPsoInit, uint32 InStencilRef, bool bInApplyAdditionalState)
		: PsoInit(InPsoInit)
		, StencilRef(InStencilRef)
		, bApplyAdditionalState(bInApplyAdditionalState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};
#endif

struct FRHICommandDispatchComputeShaderString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandDispatchComputeShader"); }
};

struct FRHICommandDispatchComputeShader final : public FRHICommand<FRHICommandDispatchComputeShader, FRHICommandDispatchComputeShaderString>
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
	FORCEINLINE_DEBUGGABLE FRHICommandDispatchComputeShader(uint32 InThreadGroupCountX, uint32 InThreadGroupCountY, uint32 InThreadGroupCountZ)
		: ThreadGroupCountX(InThreadGroupCountX)
		, ThreadGroupCountY(InThreadGroupCountY)
		, ThreadGroupCountZ(InThreadGroupCountZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDispatchIndirectComputeShaderString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandDispatchIndirectComputeShader"); }
};

struct FRHICommandDispatchIndirectComputeShader final : public FRHICommand<FRHICommandDispatchIndirectComputeShader, FRHICommandDispatchIndirectComputeShaderString>
{
	FRHIBuffer* ArgumentBuffer;
	uint32 ArgumentOffset;
	FORCEINLINE_DEBUGGABLE FRHICommandDispatchIndirectComputeShader(FRHIBuffer* InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginUAVOverlap)
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndUAVOverlap)
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginSpecificUAVOverlap)
{
	TArrayView<FRHIUnorderedAccessView* const> UAVs;
	FORCEINLINE_DEBUGGABLE FRHICommandBeginSpecificUAVOverlap(TArrayView<FRHIUnorderedAccessView* const> InUAVs) : UAVs(InUAVs) {}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndSpecificUAVOverlap)
{
	TArrayView<FRHIUnorderedAccessView* const> UAVs;
	FORCEINLINE_DEBUGGABLE FRHICommandEndSpecificUAVOverlap(TArrayView<FRHIUnorderedAccessView* const> InUAVs) : UAVs(InUAVs) {}
	RHI_API void Execute(FRHICommandListBase & CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDrawPrimitiveIndirect)
{
	FRHIBuffer* ArgumentBuffer;
	uint32 ArgumentOffset;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitiveIndirect(FRHIBuffer* InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDrawIndexedIndirect)
{
	FRHIBuffer* IndexBufferRHI;
	FRHIBuffer* ArgumentsBufferRHI;
	uint32 DrawArgumentsIndex;
	uint32 NumInstances;

	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedIndirect(FRHIBuffer* InIndexBufferRHI, FRHIBuffer* InArgumentsBufferRHI, uint32 InDrawArgumentsIndex, uint32 InNumInstances)
		: IndexBufferRHI(InIndexBufferRHI)
		, ArgumentsBufferRHI(InArgumentsBufferRHI)
		, DrawArgumentsIndex(InDrawArgumentsIndex)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDrawIndexedPrimitiveIndirect)
{
	FRHIBuffer* IndexBuffer;
	FRHIBuffer* ArgumentsBuffer;
	uint32 ArgumentOffset;

	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedPrimitiveIndirect(FRHIBuffer* InIndexBuffer, FRHIBuffer* InArgumentsBuffer, uint32 InArgumentOffset)
		: IndexBuffer(InIndexBuffer)
		, ArgumentsBuffer(InArgumentsBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDispatchMeshShader)
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;

	FORCEINLINE_DEBUGGABLE FRHICommandDispatchMeshShader(uint32 InThreadGroupCountX, uint32 InThreadGroupCountY, uint32 InThreadGroupCountZ)
		: ThreadGroupCountX(InThreadGroupCountX)
		, ThreadGroupCountY(InThreadGroupCountY)
		, ThreadGroupCountZ(InThreadGroupCountZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase & CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDispatchIndirectMeshShader)
{
	FRHIBuffer* ArgumentBuffer;
	uint32 ArgumentOffset;

	FORCEINLINE_DEBUGGABLE FRHICommandDispatchIndirectMeshShader(FRHIBuffer * InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase & CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetDepthBounds)
{
	float MinDepth;
	float MaxDepth;

	FORCEINLINE_DEBUGGABLE FRHICommandSetDepthBounds(float InMinDepth, float InMaxDepth)
		: MinDepth(InMinDepth)
		, MaxDepth(InMaxDepth)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetShadingRate)
{
	EVRSShadingRate   ShadingRate;
	EVRSRateCombiner  Combiner;

	FORCEINLINE_DEBUGGABLE FRHICommandSetShadingRate(EVRSShadingRate InShadingRate, EVRSRateCombiner InCombiner)
		: ShadingRate(InShadingRate),
		Combiner(InCombiner)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandClearUAVFloat)
{
	FRHIUnorderedAccessView* UnorderedAccessViewRHI;
	FVector4f Values;

	FORCEINLINE_DEBUGGABLE FRHICommandClearUAVFloat(FRHIUnorderedAccessView* InUnorderedAccessViewRHI, const FVector4f& InValues)
		: UnorderedAccessViewRHI(InUnorderedAccessViewRHI)
		, Values(InValues)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandClearUAVUint)
{
	FRHIUnorderedAccessView* UnorderedAccessViewRHI;
	FUintVector4 Values;

	FORCEINLINE_DEBUGGABLE FRHICommandClearUAVUint(FRHIUnorderedAccessView* InUnorderedAccessViewRHI, const FUintVector4& InValues)
		: UnorderedAccessViewRHI(InUnorderedAccessViewRHI)
		, Values(InValues)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandCopyToResolveTarget)
{
	FResolveParams ResolveParams;
	FRHITexture* SourceTexture;
	FRHITexture* DestTexture;

	FORCEINLINE_DEBUGGABLE FRHICommandCopyToResolveTarget(FRHITexture* InSourceTexture, FRHITexture* InDestTexture, const FResolveParams& InResolveParams)
		: ResolveParams(InResolveParams)
		, SourceTexture(InSourceTexture)
		, DestTexture(InDestTexture)
	{
		ensure(SourceTexture);
		ensure(DestTexture);
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandCopyTexture)
{
	FRHICopyTextureInfo CopyInfo;
	FRHITexture* SourceTexture;
	FRHITexture* DestTexture;

	FORCEINLINE_DEBUGGABLE FRHICommandCopyTexture(FRHITexture* InSourceTexture, FRHITexture* InDestTexture, const FRHICopyTextureInfo& InCopyInfo)
		: CopyInfo(InCopyInfo)
		, SourceTexture(InSourceTexture)
		, DestTexture(InDestTexture)
	{
		ensure(SourceTexture);
		ensure(DestTexture);
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandResummarizeHTile)
{
	FRHITexture2D* DepthTexture;

	FORCEINLINE_DEBUGGABLE FRHICommandResummarizeHTile(FRHITexture2D* InDepthTexture)
	: DepthTexture(InDepthTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginTransitions)
{
	TArrayView<const FRHITransition*> Transitions;

	FRHICommandBeginTransitions(TArrayView<const FRHITransition*> InTransitions)
		: Transitions(InTransitions)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndTransitions)
{
	TArrayView<const FRHITransition*> Transitions;

	FRHICommandEndTransitions(TArrayView<const FRHITransition*> InTransitions)
		: Transitions(InTransitions)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandResourceTransition)
{
	FRHITransition* Transition;

	FRHICommandResourceTransition(FRHITransition* InTransition)
		: Transition(InTransition)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetTrackedAccess)
{
	TArrayView<const FRHITrackedAccessInfo> Infos;

	FRHICommandSetTrackedAccess(TArrayView<const FRHITrackedAccessInfo> InInfos)
		: Infos(InInfos)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetAsyncComputeBudgetString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetAsyncComputeBudget"); }
};

struct FRHICommandSetAsyncComputeBudget final : public FRHICommand<FRHICommandSetAsyncComputeBudget, FRHICommandSetAsyncComputeBudgetString>
{
	EAsyncComputeBudget Budget;

	FORCEINLINE_DEBUGGABLE FRHICommandSetAsyncComputeBudget(EAsyncComputeBudget InBudget)
		: Budget(InBudget)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandCopyToStagingBufferString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandCopyToStagingBuffer"); }
};

struct FRHICommandCopyToStagingBuffer final : public FRHICommand<FRHICommandCopyToStagingBuffer, FRHICommandCopyToStagingBufferString>
{
	FRHIBuffer* SourceBuffer;
	FRHIStagingBuffer* DestinationStagingBuffer;
	uint32 Offset;
	uint32 NumBytes;

	FORCEINLINE_DEBUGGABLE FRHICommandCopyToStagingBuffer(FRHIBuffer* InSourceBuffer, FRHIStagingBuffer* InDestinationStagingBuffer, uint32 InOffset, uint32 InNumBytes)
		: SourceBuffer(InSourceBuffer)
		, DestinationStagingBuffer(InDestinationStagingBuffer)
		, Offset(InOffset)
		, NumBytes(InNumBytes)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandWriteGPUFence)
{
	FRHIGPUFence* Fence;

	FORCEINLINE_DEBUGGABLE FRHICommandWriteGPUFence(FRHIGPUFence* InFence)
		: Fence(InFence)
	{
		if (Fence)
		{
			Fence->NumPendingWriteCommands.Increment();
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandClearColorTexture)
{
	FRHITexture* Texture;
	FLinearColor Color;

	FORCEINLINE_DEBUGGABLE FRHICommandClearColorTexture(
		FRHITexture* InTexture,
		const FLinearColor& InColor
		)
		: Texture(InTexture)
		, Color(InColor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandClearDepthStencilTexture)
{
	FRHITexture* Texture;
	float Depth;
	uint32 Stencil;
	EClearDepthStencil ClearDepthStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandClearDepthStencilTexture(
		FRHITexture* InTexture,
		EClearDepthStencil InClearDepthStencil,
		float InDepth,
		uint32 InStencil
	)
		: Texture(InTexture)
		, Depth(InDepth)
		, Stencil(InStencil)
		, ClearDepthStencil(InClearDepthStencil)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandClearColorTextures)
{
	FLinearColor ColorArray[MaxSimultaneousRenderTargets];
	FRHITexture* Textures[MaxSimultaneousRenderTargets];
	int32 NumClearColors;

	FORCEINLINE_DEBUGGABLE FRHICommandClearColorTextures(
		int32 InNumClearColors,
		FRHITexture** InTextures,
		const FLinearColor* InColorArray
		)
		: NumClearColors(InNumClearColors)
	{
		check(InNumClearColors <= MaxSimultaneousRenderTargets);
		for (int32 Index = 0; Index < InNumClearColors; Index++)
		{
			ColorArray[Index] = InColorArray[Index];
			Textures[Index] = InTextures[Index];
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetStaticUniformBuffers)
{
	FUniformBufferStaticBindings UniformBuffers;

	FORCEINLINE_DEBUGGABLE FRHICommandSetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers)
		: UniformBuffers(InUniformBuffers)
	{
		const int32 NumUniformBuffers = UniformBuffers.GetUniformBufferCount();
		for (int32 Index = 0; Index < NumUniformBuffers; ++Index)
		{
			FRHIUniformBuffer* UniformBuffer = UniformBuffers.GetUniformBuffer(Index);
			if (UniformBuffer)
			{
				UniformBuffer->AddRef();
			}
		}
	}
	~FRHICommandSetStaticUniformBuffers()
	{
		const int32 NumUniformBuffers = UniformBuffers.GetUniformBufferCount();
		for (int32 Index = 0; Index < NumUniformBuffers; ++Index)
		{
			FRHIUniformBuffer* UniformBuffer = UniformBuffers.GetUniformBuffer(Index);
			if (UniformBuffer)
			{
				UniformBuffer->Release();
			}
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FComputedGraphicsPipelineState
{
	FGraphicsPipelineStateRHIRef GraphicsPipelineState;
	int32 UseCount;
	FComputedGraphicsPipelineState()
		: UseCount(0)
	{
	}
};

struct FComputedUniformBuffer
{
	FUniformBufferRHIRef UniformBuffer;
	mutable int32 UseCount;
	FComputedUniformBuffer()
		: UseCount(0)
	{
	}
};

struct FLocalUniformBufferWorkArea
{
	void* Contents;
	FUniformBufferLayoutRHIRef Layout;
	FComputedUniformBuffer* ComputedUniformBuffer;
#if DO_CHECK || USING_CODE_ANALYSIS // the below variables are used in check(), which can be enabled in Shipping builds (see Build.h)
	FRHICommandListBase* CheckCmdList;
	int32 UID;
#endif

	FLocalUniformBufferWorkArea(FRHICommandListBase* InCheckCmdList, const void* InContents, uint32 ContentsSize, const FRHIUniformBufferLayout* InLayout)
		: Layout(InLayout)
#if DO_CHECK || USING_CODE_ANALYSIS
		, CheckCmdList(InCheckCmdList)
		, UID(InCheckCmdList->GetUID())
#endif
	{
		check(ContentsSize);
		Contents = InCheckCmdList->Alloc(ContentsSize, SHADER_PARAMETER_STRUCT_ALIGNMENT);
		FMemory::Memcpy(Contents, InContents, ContentsSize);
		ComputedUniformBuffer = new (InCheckCmdList->Alloc<FComputedUniformBuffer>()) FComputedUniformBuffer;
	}
};

struct FLocalUniformBuffer
{
	FLocalUniformBufferWorkArea* WorkArea;
	FUniformBufferRHIRef BypassUniform; // this is only used in the case of Bypass, should eventually be deleted
	FLocalUniformBuffer()
		: WorkArea(nullptr)
	{
	}
	FLocalUniformBuffer(const FLocalUniformBuffer& Other)
		: WorkArea(Other.WorkArea)
		, BypassUniform(Other.BypassUniform)
	{
	}
	FORCEINLINE_DEBUGGABLE bool IsValid() const
	{
		return WorkArea || IsValidRef(BypassUniform);
	}
};

FRHICOMMAND_MACRO(FRHICommandBuildLocalUniformBuffer)
{
	FLocalUniformBufferWorkArea WorkArea;
	FORCEINLINE_DEBUGGABLE FRHICommandBuildLocalUniformBuffer(
		FRHICommandListBase* CheckCmdList,
		const void* Contents,
		uint32 ContentsSize,
		const FRHIUniformBufferLayout* Layout
		)
		: WorkArea(CheckCmdList, Contents, ContentsSize, Layout)

	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetLocalUniformBufferString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandSetLocalUniformBuffer"); }
};
template <typename TRHIShader>
struct FRHICommandSetLocalUniformBuffer final : public FRHICommand<FRHICommandSetLocalUniformBuffer<TRHIShader>, FRHICommandSetLocalUniformBufferString >
{
	TRHIShader* Shader;
	uint32 BaseIndex;
	FLocalUniformBuffer LocalUniformBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandSetLocalUniformBuffer(FRHICommandListBase* CheckCmdList, TRHIShader* InShader, uint32 InBaseIndex, const FLocalUniformBuffer& InLocalUniformBuffer)
		: Shader(InShader)
		, BaseIndex(InBaseIndex)
		, LocalUniformBuffer(InLocalUniformBuffer)

	{
		check(CheckCmdList == LocalUniformBuffer.WorkArea->CheckCmdList && CheckCmdList->GetUID() == LocalUniformBuffer.WorkArea->UID); // this uniform buffer was not built for this particular commandlist
		LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount++;
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginRenderQuery)
{
	FRHIRenderQuery* RenderQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginRenderQuery(FRHIRenderQuery* InRenderQuery)
		: RenderQuery(InRenderQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndRenderQuery)
{
	FRHIRenderQuery* RenderQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandEndRenderQuery(FRHIRenderQuery* InRenderQuery)
		: RenderQuery(InRenderQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandCalibrateTimers)
{
	FRHITimestampCalibrationQuery* CalibrationQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandCalibrateTimers(FRHITimestampCalibrationQuery * CalibrationQuery)
		: CalibrationQuery(CalibrationQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase & CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSubmitCommandsHint)
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandPostExternalCommandsReset)
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandPollOcclusionQueries)
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginScene)
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginScene()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndScene)
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndScene()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginFrame)
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginFrame()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndFrame)
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndFrame()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandBeginDrawingViewport)
{
	FRHIViewport* Viewport;
	FRHITexture* RenderTargetRHI;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginDrawingViewport(FRHIViewport* InViewport, FRHITexture* InRenderTargetRHI)
		: Viewport(InViewport)
		, RenderTargetRHI(InRenderTargetRHI)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandEndDrawingViewport)
{
	FRHIViewport* Viewport;
	bool bPresent;
	bool bLockToVsync;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawingViewport(FRHIViewport* InViewport, bool InbPresent, bool InbLockToVsync)
		: Viewport(InViewport)
		, bPresent(InbPresent)
		, bLockToVsync(InbLockToVsync)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandPushEventString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandPushEventString"); }
};

struct FRHICommandPushEvent final : public FRHICommand<FRHICommandPushEvent, FRHICommandPushEventString>
{
	const TCHAR *Name;
	FColor Color;

	FORCEINLINE_DEBUGGABLE FRHICommandPushEvent(const TCHAR *InName, FColor InColor)
		: Name(InName)
		, Color(InColor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);

	virtual void StoreDebugInfo(FRHICommandListDebugContext& Context)
	{
		Context.PushMarker(Name);
	};
};

struct FRHICommandPopEventString
{
	static const TCHAR* TStr() { return TEXT("FRHICommandPopEvent"); }
};

struct FRHICommandPopEvent final : public FRHICommand<FRHICommandPopEvent, FRHICommandPopEventString>
{
	RHI_API void Execute(FRHICommandListBase& CmdList);

	virtual void StoreDebugInfo(FRHICommandListDebugContext& Context)
	{
		Context.PopMarker();
	};
};

#if RHI_WANT_BREADCRUMB_EVENTS
FRHICOMMAND_MACRO(FRHICommandSetBreadcrumbStackTop)
{
	FRHIBreadcrumb* Breadcrumb;

	FORCEINLINE_DEBUGGABLE FRHICommandSetBreadcrumbStackTop(FRHIBreadcrumb* InBreadcrumb)
		: Breadcrumb(InBreadcrumb)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};
#endif

FRHICOMMAND_MACRO(FRHICommandInvalidateCachedState)
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandDiscardRenderTargets)
{
	uint32 ColorBitMask;
	bool Depth;
	bool Stencil;

	FORCEINLINE_DEBUGGABLE FRHICommandDiscardRenderTargets(bool InDepth, bool InStencil, uint32 InColorBitMask)
		: ColorBitMask(InColorBitMask)
		, Depth(InDepth)
		, Stencil(InStencil)
	{
	}
	
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHIShaderResourceViewUpdateInfo
{
	FRHIShaderResourceView* SRV;
	FRHIBuffer* Buffer;
	uint32 Stride;
	uint8 Format;
};

struct FRHIBufferUpdateInfo
{
	FRHIBuffer* DestBuffer;
	FRHIBuffer* SrcBuffer;
};

struct FRHIRayTracingGeometryUpdateInfo
{
	FRHIRayTracingGeometry* DestGeometry;
	FRHIRayTracingGeometry* SrcGeometry;
};

struct FRHIResourceUpdateInfo
{
	enum EUpdateType
	{
		/** Take over underlying resource from an intermediate buffer */
		UT_Buffer,
		/** Update an SRV to view on a different buffer */
		UT_BufferSRV,
		/** Update an SRV to view on a different buffer with a given format */
		UT_BufferFormatSRV,
		/** Take over underlying resource from an intermediate geometry */
		UT_RayTracingGeometry,
		/** Number of update types */
		UT_Num
	};

	EUpdateType Type;
	union
	{
		FRHIBufferUpdateInfo Buffer;
		FRHIShaderResourceViewUpdateInfo BufferSRV;
		FRHIRayTracingGeometryUpdateInfo RayTracingGeometry;
	};

	void ReleaseRefs();
};

FRHICOMMAND_MACRO(FRHICommandUpdateRHIResources)
{
	FRHIResourceUpdateInfo* UpdateInfos;
	int32 Num;
	bool bNeedReleaseRefs;

	FRHICommandUpdateRHIResources(FRHIResourceUpdateInfo* InUpdateInfos, int32 InNum, bool bInNeedReleaseRefs)
		: UpdateInfos(InUpdateInfos)
		, Num(InNum)
		, bNeedReleaseRefs(bInNeedReleaseRefs)
	{}

	~FRHICommandUpdateRHIResources();

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandCopyBufferRegion)
{
	FRHIBuffer* DestBuffer;
	uint64 DstOffset;
	FRHIBuffer* SourceBuffer;
	uint64 SrcOffset;
	uint64 NumBytes;

	explicit FRHICommandCopyBufferRegion(FRHIBuffer* InDestBuffer, uint64 InDstOffset, FRHIBuffer* InSourceBuffer, uint64 InSrcOffset, uint64 InNumBytes)
		: DestBuffer(InDestBuffer)
		, DstOffset(InDstOffset)
		, SourceBuffer(InSourceBuffer)
		, SrcOffset(InSrcOffset)
		, NumBytes(InNumBytes)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

#if RHI_RAYTRACING

struct FRHICommandBindAccelerationStructureMemory final : public FRHICommand<FRHICommandBindAccelerationStructureMemory>
{
	FRHIRayTracingScene* Scene;
	FRHIBuffer* Buffer;
	uint32 BufferOffset;

	FRHICommandBindAccelerationStructureMemory(FRHIRayTracingScene* InScene, FRHIBuffer* InBuffer, uint32 InBufferOffset)
		: Scene(InScene)
		, Buffer(InBuffer)
		, BufferOffset(InBufferOffset)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBuildAccelerationStructure final : public FRHICommand<FRHICommandBuildAccelerationStructure>
{
	FRayTracingSceneBuildParams SceneBuildParams;

	explicit FRHICommandBuildAccelerationStructure(const FRayTracingSceneBuildParams& InSceneBuildParams)
		: SceneBuildParams(InSceneBuildParams)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandClearRayTracingBindings)
{
	FRHIRayTracingScene* Scene;

	explicit FRHICommandClearRayTracingBindings(FRHIRayTracingScene* InScene)
		: Scene(InScene)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBuildAccelerationStructures final : public FRHICommand<FRHICommandBuildAccelerationStructures>
{
	const TArrayView<const FRayTracingGeometryBuildParams> Params;
	FRHIBufferRange ScratchBufferRange;
	TRefCountPtr<FRHIBuffer> ScratchBuffer;

	explicit FRHICommandBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> InParams, const FRHIBufferRange& ScratchBufferRange)
		: Params(InParams), ScratchBufferRange(ScratchBufferRange), ScratchBuffer(ScratchBufferRange.Buffer)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandRayTraceOcclusion)
{
	FRHIRayTracingScene* Scene;
	FRHIShaderResourceView* Rays;
	FRHIUnorderedAccessView* Output;
	uint32 NumRays;

	FRHICommandRayTraceOcclusion(FRHIRayTracingScene* InScene,
		FRHIShaderResourceView* InRays,
		FRHIUnorderedAccessView* InOutput,
		uint32 InNumRays)
		: Scene(InScene)
		, Rays(InRays)
		, Output(InOutput)
		, NumRays(InNumRays)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandRayTraceIntersection)
{
	FRHIRayTracingScene* Scene;
	FRHIShaderResourceView* Rays;
	FRHIUnorderedAccessView* Output;
	uint32 NumRays;

	FRHICommandRayTraceIntersection(FRHIRayTracingScene* InScene,
		FRHIShaderResourceView* InRays,
		FRHIUnorderedAccessView* InOutput,
		uint32 InNumRays)
		: Scene(InScene)
		, Rays(InRays)
		, Output(InOutput)
		, NumRays(InNumRays)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandRayTraceDispatch)
{
	FRayTracingPipelineState* Pipeline;
	FRHIRayTracingScene* Scene;
	FRayTracingShaderBindings GlobalResourceBindings;
	FRHIRayTracingShader* RayGenShader;
	FRHIBuffer* ArgumentBuffer;
	uint32 ArgumentOffset;
	uint32 Width;
	uint32 Height;

	FRHICommandRayTraceDispatch(FRayTracingPipelineState* InPipeline, FRHIRayTracingShader* InRayGenShader, FRHIRayTracingScene* InScene, const FRayTracingShaderBindings& InGlobalResourceBindings, uint32 InWidth, uint32 InHeight)
		: Pipeline(InPipeline)
		, Scene(InScene)
		, GlobalResourceBindings(InGlobalResourceBindings)
		, RayGenShader(InRayGenShader)
		, ArgumentBuffer(nullptr)
		, ArgumentOffset(0)
		, Width(InWidth)
		, Height(InHeight)
	{}

	FRHICommandRayTraceDispatch(FRayTracingPipelineState* InPipeline, FRHIRayTracingShader* InRayGenShader, FRHIRayTracingScene* InScene, const FRayTracingShaderBindings& InGlobalResourceBindings, FRHIBuffer* InArgumentBuffer, uint32 InArgumentOffset)
		: Pipeline(InPipeline)
		, Scene(InScene)
		, GlobalResourceBindings(InGlobalResourceBindings)
		, RayGenShader(InRayGenShader)
		, ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
		, Width(0)
		, Height(0)
	{}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

FRHICOMMAND_MACRO(FRHICommandSetRayTracingBindings)
{
	FRHIRayTracingScene* Scene = nullptr;
	ERayTracingBindingType BindingType = ERayTracingBindingType::HitGroup;
	uint32 InstanceIndex = 0;
	uint32 SegmentIndex = 0;
	uint32 ShaderSlot = 0;
	FRayTracingPipelineState* Pipeline = nullptr;
	uint32 ShaderIndex = 0;
	uint32 NumUniformBuffers = 0;
	FRHIUniformBuffer* const* UniformBuffers = nullptr; // Pointer to an array of uniform buffers, allocated inline within the command list
	uint32 LooseParameterDataSize = 0;
	const void* LooseParameterData = nullptr;
	uint32 UserData = 0;

	// Batched bindings
	int32 NumBindings = -1;
	const FRayTracingLocalShaderBindings* Bindings = nullptr;

	// Hit group bindings
	FRHICommandSetRayTracingBindings(FRHIRayTracingScene* InScene, uint32 InInstanceIndex, uint32 InSegmentIndex, uint32 InShaderSlot,
		FRayTracingPipelineState* InPipeline, uint32 InHitGroupIndex, uint32 InNumUniformBuffers, FRHIUniformBuffer* const* InUniformBuffers,
		uint32 InLooseParameterDataSize, const void* InLooseParameterData,
		uint32 InUserData)
		: Scene(InScene)
		, BindingType(ERayTracingBindingType::HitGroup)
		, InstanceIndex(InInstanceIndex)
		, SegmentIndex(InSegmentIndex)
		, ShaderSlot(InShaderSlot)
		, Pipeline(InPipeline)
		, ShaderIndex(InHitGroupIndex)
		, NumUniformBuffers(InNumUniformBuffers)
		, UniformBuffers(InUniformBuffers)
		, LooseParameterDataSize(InLooseParameterDataSize)
		, LooseParameterData(InLooseParameterData)
		, UserData(InUserData)
	{
	}

	// Callable and Miss shader bindings
	FRHICommandSetRayTracingBindings(FRHIRayTracingScene* InScene, uint32 InShaderSlot,
		FRayTracingPipelineState* InPipeline, uint32 InShaderIndex,
		uint32 InNumUniformBuffers, FRHIUniformBuffer* const* InUniformBuffers,
		uint32 InUserData, ERayTracingBindingType InBindingType)
		: Scene(InScene)
		, BindingType(InBindingType)
		, InstanceIndex(0)
		, SegmentIndex(0)
		, ShaderSlot(InShaderSlot)
		, Pipeline(InPipeline)
		, ShaderIndex(InShaderIndex)
		, NumUniformBuffers(InNumUniformBuffers)
		, UniformBuffers(InUniformBuffers)
		, UserData(InUserData)
	{
		checkf(InBindingType != ERayTracingBindingType::HitGroup, TEXT("Hit group bindings must specify Instance and Segment Index."));
	}

	// Bindings Batch
	FRHICommandSetRayTracingBindings(FRHIRayTracingScene* InScene, FRayTracingPipelineState* InPipeline, uint32 InNumBindings, const FRayTracingLocalShaderBindings* InBindings, ERayTracingBindingType InBindingType)
		: Scene(InScene)
		, BindingType(InBindingType)
		, Pipeline(InPipeline)
		, NumBindings(InNumBindings)
		, Bindings(InBindings)
	{

	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};
#endif // RHI_RAYTRACING

template<> RHI_API void FRHICommandSetShaderParameter            <FRHIComputeShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderUniformBuffer        <FRHIComputeShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderTexture              <FRHIComputeShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderResourceViewParameter<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderSampler              <FRHIComputeShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetUAVParameter<FRHIComputeShader>::Execute(FRHICommandListBase& CmdList);

class RHI_API FRHIComputeCommandList : public FRHICommandListBase
{
public:
	FRHIComputeCommandList(FRHIGPUMask GPUMask = FRHIGPUMask::All(), ERecordingThread InRecordingThread = ERecordingThread::Render)
		: FRHICommandListBase(GPUMask, InRecordingThread)
	{}

	FRHIComputeCommandList(FRHICommandListBase&& Other)
		: FRHICommandListBase(MoveTemp(Other))
	{}

	template <typename LAMBDA>
	FORCEINLINE_DEBUGGABLE void EnqueueLambda(LAMBDA&& Lambda)
	{
		if (IsBottomOfPipe())
		{
			Lambda(*this);
		}
		else
		{
			ALLOC_COMMAND(TRHILambdaCommand<FRHIComputeCommandList, LAMBDA>)(Forward<LAMBDA>(Lambda));
		}
	}

	inline FRHIComputeShader* GetBoundComputeShader() const { return PersistentState.BoundComputeShaderRHI; }

	UE_DEPRECATED(5.0, "Please rename to SetStaticUniformBuffers")
	FORCEINLINE_DEBUGGABLE void SetGlobalUniformBuffers(const FUniformBufferStaticBindings& UniformBuffers)
	{
		SetStaticUniformBuffers(UniformBuffers);
	}

	FORCEINLINE_DEBUGGABLE void SetStaticUniformBuffers(const FUniformBufferStaticBindings& UniformBuffers)
	{
		if (Bypass())
		{
			GetComputeContext().RHISetStaticUniformBuffers(UniformBuffers);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetStaticUniformBuffers)(UniformBuffers);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(FRHIComputeShader* Shader, uint32 BaseIndex, FRHIUniformBuffer* UniformBuffer)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetComputeContext().RHISetShaderUniformBuffer(Shader, BaseIndex, UniformBuffer);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderUniformBuffer<FRHIComputeShader>)(Shader, BaseIndex, UniformBuffer);
	}

	FORCEINLINE void SetShaderUniformBuffer(const FComputeShaderRHIRef& Shader, uint32 BaseIndex, FRHIUniformBuffer* UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	UE_DEPRECATED(5.1, "Local uniform buffers are now deprecated. Use RHICreateUniformBuffer instead.")
	FORCEINLINE_DEBUGGABLE FLocalUniformBuffer BuildLocalUniformBuffer(const void* Contents, uint32 ContentsSize, const FRHIUniformBufferLayout* Layout)
	{
		FLocalUniformBuffer Result;
		if (Bypass())
		{
			Result.BypassUniform = RHICreateUniformBuffer(Contents, Layout, UniformBuffer_SingleFrame);
		}
		else
		{
			check(Contents && ContentsSize && (&Layout != nullptr));
			auto* Cmd = ALLOC_COMMAND(FRHICommandBuildLocalUniformBuffer)(this, Contents, ContentsSize, Layout);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	UE_DEPRECATED(5.0, "Use Layout pointers instead")
	FORCEINLINE_DEBUGGABLE FLocalUniformBuffer BuildLocalUniformBuffer(const void* Contents, uint32 ContentsSize, const FRHIUniformBufferLayout& Layout)
	{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return BuildLocalUniformBuffer(Contents, ContentsSize, &Layout);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	template <typename TRHIShader>
	UE_DEPRECATED(5.1, "Local uniform buffers are now deprecated. Use RHICreateUniformBuffer instead.")
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TRHIShader* Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetContext().RHISetShaderUniformBuffer(Shader, BaseIndex, UniformBuffer.BypassUniform);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetLocalUniformBuffer<TRHIShader>)(this, Shader, BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(const TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		SetLocalShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderParameter(FRHIComputeShader* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetComputeContext().RHISetShaderParameter(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		ALLOC_COMMAND(FRHICommandSetShaderParameter<FRHIComputeShader>)(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}

	FORCEINLINE void SetShaderParameter(FComputeShaderRHIRef& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderTexture(FRHIComputeShader* Shader, uint32 TextureIndex, FRHITexture* Texture)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetComputeContext().RHISetShaderTexture(Shader, TextureIndex, Texture);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderTexture<FRHIComputeShader>)(Shader, TextureIndex, Texture);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(FRHIComputeShader* Shader, uint32 SamplerIndex, FRHIShaderResourceView* SRV)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetComputeContext().RHISetShaderResourceViewParameter(Shader, SamplerIndex, SRV);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderResourceViewParameter<FRHIComputeShader>)(Shader, SamplerIndex, SRV);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderSampler(FRHIComputeShader* Shader, uint32 SamplerIndex, FRHISamplerState* State)
	{
		// Immutable samplers can't be set dynamically
		check(!State->IsImmutable());
		if (State->IsImmutable())
		{
			return;
		}

		if (Bypass())
		{
			GetComputeContext().RHISetShaderSampler(Shader, SamplerIndex, State);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderSampler<FRHIComputeShader>)(Shader, SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FRHIComputeShader* Shader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetComputeContext().RHISetUAVParameter(Shader, UAVIndex, UAV);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetUAVParameter<FRHIComputeShader>)(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FRHIComputeShader* Shader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV, uint32 InitialCount)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetComputeContext().RHISetUAVParameter(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetUAVParameter_InitialCount)(Shader, UAVIndex, UAV, InitialCount);
	}

	UE_DEPRECATED(5.1, "ComputePipelineStates should be used instead of direct ComputeShaders. You can use SetComputePipelineState(RHICmdList, ComputeShader).")
	FORCEINLINE_DEBUGGABLE void SetComputeShader(FRHIComputeShader* ComputeShader)
	{
		PersistentState.BoundComputeShaderRHI = ComputeShader;
		ComputeShader->UpdateStats();
		if (Bypass())
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			GetComputeContext().RHISetComputeShader(ComputeShader);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			return;
		}
		ALLOC_COMMAND(FRHICommandSetComputeShader)(ComputeShader);
	}

	FORCEINLINE_DEBUGGABLE void SetComputePipelineState(FComputePipelineState* ComputePipelineState, FRHIComputeShader* ComputeShader)
	{
		PersistentState.BoundComputeShaderRHI = ComputeShader;
		if (Bypass())
		{
			extern RHI_API FRHIComputePipelineState* ExecuteSetComputePipelineState(FComputePipelineState* ComputePipelineState);
			FRHIComputePipelineState* RHIComputePipelineState = ExecuteSetComputePipelineState(ComputePipelineState);
			GetComputeContext().RHISetComputePipelineState(RHIComputePipelineState);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetComputePipelineState)(ComputePipelineState);
	}

	FORCEINLINE_DEBUGGABLE void SetAsyncComputeBudget(EAsyncComputeBudget Budget)
	{
		if (Bypass())
		{
			GetComputeContext().RHISetAsyncComputeBudget(Budget);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetAsyncComputeBudget)(Budget);
	}

	FORCEINLINE_DEBUGGABLE void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			GetComputeContext().RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		ALLOC_COMMAND(FRHICommandDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			GetComputeContext().RHIDispatchIndirectComputeShader(ArgumentBuffer, ArgumentOffset);
			return;
		}
		ALLOC_COMMAND(FRHICommandDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void ClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values)
	{
		if (Bypass())
		{
			GetComputeContext().RHIClearUAVFloat(UnorderedAccessViewRHI, Values);
			return;
		}
		ALLOC_COMMAND(FRHICommandClearUAVFloat)(UnorderedAccessViewRHI, Values);
	}

	FORCEINLINE_DEBUGGABLE void ClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
	{
		if (Bypass())
		{
			GetComputeContext().RHIClearUAVUint(UnorderedAccessViewRHI, Values);
			return;
		}
		ALLOC_COMMAND(FRHICommandClearUAVUint)(UnorderedAccessViewRHI, Values);
	}

	FORCEINLINE_DEBUGGABLE void BeginTransitions(TArrayView<const FRHITransition*> Transitions)
	{
		if (Bypass())
		{
			GetComputeContext().RHIBeginTransitions(Transitions);

			for (const FRHITransition* Transition : Transitions)
			{
				Transition->MarkBegin(GetPipeline());
			}
		}
		else
		{
			// Copy the transition array into the command list
			FRHITransition** DstTransitionArray = (FRHITransition**)Alloc(sizeof(FRHITransition*) * Transitions.Num(), alignof(FRHITransition*));
			FMemory::Memcpy(DstTransitionArray, Transitions.GetData(), sizeof(FRHITransition*) * Transitions.Num());

			ALLOC_COMMAND(FRHICommandBeginTransitions)(MakeArrayView((const FRHITransition**)DstTransitionArray, Transitions.Num()));
		}
	}

	FORCEINLINE_DEBUGGABLE void EndTransitions(TArrayView<const FRHITransition*> Transitions)
	{
		if (Bypass())
		{
			GetComputeContext().RHIEndTransitions(Transitions);

			for (const FRHITransition* Transition : Transitions)
			{
				Transition->MarkEnd(GetPipeline());
			}
		}
		else
		{
			// Copy the transition array into the command list
			FRHITransition** DstTransitionArray = (FRHITransition**)Alloc(sizeof(FRHITransition*) * Transitions.Num(), alignof(FRHITransition*));
			FMemory::Memcpy(DstTransitionArray, Transitions.GetData(), sizeof(FRHITransition*) * Transitions.Num());

			ALLOC_COMMAND(FRHICommandEndTransitions)(MakeArrayView((const FRHITransition**)DstTransitionArray, Transitions.Num()));
		}
	}

	void Transition(TArrayView<const FRHITransitionInfo> Infos);

	FORCEINLINE_DEBUGGABLE void BeginTransition(const FRHITransition* Transition)
	{
		BeginTransitions(MakeArrayView(&Transition, 1));
	}

	FORCEINLINE_DEBUGGABLE void EndTransition(const FRHITransition* Transition)
	{
		EndTransitions(MakeArrayView(&Transition, 1));
	}

	FORCEINLINE_DEBUGGABLE void Transition(const FRHITransitionInfo& Info)
	{
		Transition(MakeArrayView(&Info, 1));
	}

	FORCEINLINE_DEBUGGABLE void SetTrackedAccess(TArrayView<const FRHITrackedAccessInfo> Infos)
	{
		if (Bypass())
		{
			for (const FRHITrackedAccessInfo& Info : Infos)
			{
				GetComputeContext().SetTrackedAccess(Info);
			}
		}
		else
		{
			ALLOC_COMMAND(FRHICommandSetTrackedAccess)(AllocArray(Infos));
		}
	}

	FORCEINLINE_DEBUGGABLE void SetTrackedAccess(TArrayView<const FRHITransitionInfo> Infos)
	{
		for (const FRHITransitionInfo& Info : Infos)
		{
			if (FRHIViewableResource* Resource = GetViewableResource(Info))
			{
				SetTrackedAccess({ FRHITrackedAccessInfo(Resource, Info.AccessAfter) });
			}
		}
	}

	/* LEGACY API */

	UE_DEPRECATED(5.1, "TransitionResource is deprecated. Use Transition instead.")
	FORCEINLINE_DEBUGGABLE void TransitionResource(ERHIAccess TransitionType, const FTextureRHIRef& InTexture)
	{
		Transition(FRHITransitionInfo(InTexture.GetReference(), ERHIAccess::Unknown, TransitionType));
	}

	UE_DEPRECATED(5.1, "TransitionResource is deprecated. Use Transition instead.")
	FORCEINLINE_DEBUGGABLE void TransitionResource(ERHIAccess TransitionType, FRHITexture* InTexture)
	{
		Transition(FRHITransitionInfo(InTexture, ERHIAccess::Unknown, TransitionType));
	}

	UE_DEPRECATED(5.1, "TransitionResources is deprecated. Use Transition instead.")
	inline void TransitionResources(ERHIAccess TransitionType, FRHITexture* const* InTextures, int32 NumTextures)
	{
		// Stack allocate the transition descriptors. These will get memcpy()ed onto the RHI command list if required.
		TArray<FRHITransitionInfo, FConcurrentLinearArrayAllocator> Infos;
		Infos.Reserve(NumTextures);

		for (int32 Index = 0; Index < NumTextures; ++Index)
		{
			Infos.Add(FRHITransitionInfo(InTextures[Index], ERHIAccess::Unknown, TransitionType));
		}

		Transition(Infos);
	}

	UE_DEPRECATED(5.1, "TransitionResourceArrayNoCopy is deprecated. Use Transition instead.")
	FORCEINLINE_DEBUGGABLE void TransitionResourceArrayNoCopy(ERHIAccess TransitionType, TArray<FRHITexture*>& InTextures)
	{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TransitionResources(TransitionType, &InTextures[0], InTextures.Num());
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UE_DEPRECATED(5.1, "WaitComputeFence is deprecated. Use RHI transitions instead.")
	FORCEINLINE_DEBUGGABLE void WaitComputeFence(FRHIComputeFence* WaitFence)
	{
		check(WaitFence->Transition);
		EndTransitions(MakeArrayView(&WaitFence->Transition, 1));
		WaitFence->Transition = nullptr;
	}

	FORCEINLINE_DEBUGGABLE void BeginUAVOverlap()
	{
		if (Bypass())
		{
			GetComputeContext().RHIBeginUAVOverlap();
			return;
		}
		ALLOC_COMMAND(FRHICommandBeginUAVOverlap)();
	}

	FORCEINLINE_DEBUGGABLE void EndUAVOverlap()
	{
		if (Bypass())
		{
			GetContext().RHIEndUAVOverlap();
			return;
		}
		ALLOC_COMMAND(FRHICommandEndUAVOverlap)();
	}

	FORCEINLINE_DEBUGGABLE void BeginUAVOverlap(FRHIUnorderedAccessView* UAV)
	{
		FRHIUnorderedAccessView* UAVs[1] = { UAV };
		BeginUAVOverlap(MakeArrayView(UAVs, 1));
	}

	FORCEINLINE_DEBUGGABLE void EndUAVOverlap(FRHIUnorderedAccessView* UAV)
	{
		FRHIUnorderedAccessView* UAVs[1] = { UAV };
		EndUAVOverlap(MakeArrayView(UAVs, 1));
	}

	FORCEINLINE_DEBUGGABLE void BeginUAVOverlap(TArrayView<FRHIUnorderedAccessView* const> UAVs)
	{
		if (Bypass())
		{
			GetComputeContext().RHIBeginUAVOverlap(UAVs);
			return;
		}

		const uint32 AllocSize = UAVs.Num() * sizeof(FRHIUnorderedAccessView*);
		FRHIUnorderedAccessView** InlineUAVs = (FRHIUnorderedAccessView**)Alloc(AllocSize, alignof(FRHIUnorderedAccessView*));
		FMemory::Memcpy(InlineUAVs, UAVs.GetData(), AllocSize);
		ALLOC_COMMAND(FRHICommandBeginSpecificUAVOverlap)(MakeArrayView(InlineUAVs, UAVs.Num()));
	}

	FORCEINLINE_DEBUGGABLE void EndUAVOverlap(TArrayView<FRHIUnorderedAccessView* const> UAVs)
	{
		if (Bypass())
		{
			GetComputeContext().RHIEndUAVOverlap(UAVs);
			return;
		}

		const uint32 AllocSize = UAVs.Num() * sizeof(FRHIUnorderedAccessView*);
		FRHIUnorderedAccessView** InlineUAVs = (FRHIUnorderedAccessView**)Alloc(AllocSize, alignof(FRHIUnorderedAccessView*));
		FMemory::Memcpy(InlineUAVs, UAVs.GetData(), AllocSize);
		ALLOC_COMMAND(FRHICommandEndSpecificUAVOverlap)(MakeArrayView(InlineUAVs, UAVs.Num()));
	}

	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name, FColor Color)
	{
		if (Bypass())
		{
			GetComputeContext().RHIPushEvent(Name, Color);
			return;
		}
		TCHAR* NameCopy = AllocString(Name);
		ALLOC_COMMAND(FRHICommandPushEvent)(NameCopy, Color);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			GetComputeContext().RHIPopEvent();
			return;
		}
		ALLOC_COMMAND(FRHICommandPopEvent)();
	}

	FORCEINLINE_DEBUGGABLE void PushBreadcrumb(const TCHAR* InText)
	{
#if RHI_WANT_BREADCRUMB_EVENTS
		FRHIBreadcrumb* Breadcrumb = Breadcrumbs.Stack.PushBreadcrumb(GetAllocator(), InText);
		if (Bypass())
		{
			Breadcrumbs.SetStackTop(Breadcrumb);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetBreadcrumbStackTop)(Breadcrumb);
#endif
	}

	template<typename... Types>
	FORCEINLINE_DEBUGGABLE void PushBreadcrumbPrintf(const TCHAR* Format, Types... Arguments)
	{
#if RHI_WANT_BREADCRUMB_EVENTS
		FRHIBreadcrumb* Breadcrumb = Breadcrumbs.Stack.PushBreadcrumbPrintf(GetAllocator(), Format, Arguments...);
		if (Bypass())
		{
			Breadcrumbs.SetStackTop(Breadcrumb);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetBreadcrumbStackTop)(Breadcrumb);
#endif
	}

	FORCEINLINE_DEBUGGABLE void PopBreadcrumb()
	{
#if RHI_WANT_BREADCRUMB_EVENTS
		FRHIBreadcrumb* Breadcrumb = Breadcrumbs.Stack.PopBreadcrumb();
		if (Bypass())
		{
			Breadcrumbs.SetStackTop(Breadcrumb);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetBreadcrumbStackTop)(Breadcrumb);
#endif
	}

	//UE_DEPRECATED(5.1, "SubmitCommandsHint is deprecated, and has no effect if called on a non-immediate RHI command list. Consider calling ImmediateFlush(EImmediateFlushType::DispatchToRHIThread) on the immediate command list instead.")
	inline void SubmitCommandsHint();

	FORCEINLINE_DEBUGGABLE void CopyToStagingBuffer(FRHIBuffer* SourceBuffer, FRHIStagingBuffer* DestinationStagingBuffer, uint32 Offset, uint32 NumBytes)
	{
		if (Bypass())
		{
			GetComputeContext().RHICopyToStagingBuffer(SourceBuffer, DestinationStagingBuffer, Offset, NumBytes);
			return;
		}
		ALLOC_COMMAND(FRHICommandCopyToStagingBuffer)(SourceBuffer, DestinationStagingBuffer, Offset, NumBytes);
	}

	FORCEINLINE_DEBUGGABLE void WriteGPUFence(FRHIGPUFence* Fence)
	{
		GDynamicRHI->RHIWriteGPUFence_TopOfPipe(*this, Fence);
	}

	FORCEINLINE_DEBUGGABLE void SetGPUMask(FRHIGPUMask InGPUMask)
	{
		if (PersistentState.CurrentGPUMask != InGPUMask)
		{
			PersistentState.CurrentGPUMask = InGPUMask;
#if WITH_MGPU
			if (Bypass())
			{
				// Apply the new mask to all contexts owned by this command list.
				for (IRHIComputeContext* Context : Contexts)
				{
					if (Context)
					{
						Context->RHISetGPUMask(PersistentState.CurrentGPUMask);
					}
				}
				return;
			}
			else
			{
				ALLOC_COMMAND(FRHICommandSetGPUMask)(PersistentState.CurrentGPUMask);
			}
#endif // WITH_MGPU
		}
	}

	FORCEINLINE_DEBUGGABLE void TransferResources(const TArrayView<const FTransferResourceParams> Params)
	{
#if WITH_MGPU
		if (Bypass())
		{
			GetComputeContext().RHITransferResources(Params);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandTransferResources)(Params);
		}
#endif // WITH_MGPU
	}

	FORCEINLINE_DEBUGGABLE void TransferResourceSignal(const TArrayView<FTransferResourceFenceData* const> FenceDatas, FRHIGPUMask SrcGPUMask)
	{
#if WITH_MGPU
		if (Bypass())
		{
			GetComputeContext().RHITransferResourceSignal(FenceDatas, SrcGPUMask);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandTransferResourceSignal)(FenceDatas, SrcGPUMask);
		}
#endif // WITH_MGPU
	}

	FORCEINLINE_DEBUGGABLE void TransferResourceWait(const TArrayView<FTransferResourceFenceData* const> FenceDatas)
	{
#if WITH_MGPU
		if (Bypass())
		{
			GetComputeContext().RHITransferResourceWait(FenceDatas);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandTransferResourceWait)(FenceDatas);
		}
#endif // WITH_MGPU
	}

#if WITH_MGPU
	FORCEINLINE_DEBUGGABLE void WaitForTemporalEffect(const FName& EffectName)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIWaitForTemporalEffect(EffectName);
			return;
		}
		ALLOC_COMMAND(FRHICommandWaitForTemporalEffect)(EffectName);
	}

	FORCEINLINE_DEBUGGABLE void BroadcastTemporalEffect(const FName& EffectName, const TArrayView<FRHITexture*> Textures)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIBroadcastTemporalEffect(EffectName, Textures);
			return;
		}

		ALLOC_COMMAND(FRHICommandBroadcastTemporalEffect<FRHITexture>)(EffectName, AllocArray(Textures));
	}

	FORCEINLINE_DEBUGGABLE void BroadcastTemporalEffect(const FName& EffectName, const TArrayView<FRHIBuffer*> Buffers)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIBroadcastTemporalEffect(EffectName, Buffers);
			return;
		}

		ALLOC_COMMAND(FRHICommandBroadcastTemporalEffect<FRHIBuffer>)(EffectName, AllocArray(Buffers));
	}
#endif // WITH_MGPU

#if RHI_RAYTRACING
	void BuildAccelerationStructure(FRHIRayTracingGeometry* Geometry);
	void BuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params);

	FORCEINLINE_DEBUGGABLE void BuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange)
	{
		if (Bypass())
		{
			GetComputeContext().RHIBuildAccelerationStructures(Params, ScratchBufferRange);
		}
		else
		{
			// Copy the params themselves as well their segment lists, if there are any.
			// AllocArray() can't be used here directly, as we have to modify the params after copy.
			size_t DataSize = sizeof(FRayTracingGeometryBuildParams) * Params.Num();
			FRayTracingGeometryBuildParams* InlineParams = (FRayTracingGeometryBuildParams*) Alloc(DataSize, alignof(FRayTracingGeometryBuildParams));
			FMemory::Memcpy(InlineParams, Params.GetData(), DataSize);
			for (int32 i=0; i<Params.Num(); ++i)
			{
				if (Params[i].Segments.Num())
				{
					InlineParams[i].Segments = AllocArray(Params[i].Segments);
				}
			}
			ALLOC_COMMAND(FRHICommandBuildAccelerationStructures)(MakeArrayView(InlineParams, Params.Num()), ScratchBufferRange);
		}
	}

	FORCEINLINE_DEBUGGABLE void BuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams)
	{
		if (Bypass())
		{
			GetComputeContext().RHIBuildAccelerationStructure(SceneBuildParams);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandBuildAccelerationStructure)(SceneBuildParams);
		}
	}

	FORCEINLINE_DEBUGGABLE void BindAccelerationStructureMemory(FRHIRayTracingScene* Scene, FRHIBuffer* Buffer, uint32 BufferOffset)
	{
		if (Bypass())
		{
			GetComputeContext().RHIBindAccelerationStructureMemory(Scene, Buffer, BufferOffset);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandBindAccelerationStructureMemory)(Scene, Buffer, BufferOffset);
		}
	}
#endif

	FORCEINLINE_DEBUGGABLE void PostExternalCommandsReset()
	{
		if (Bypass())
		{
			GetContext().RHIPostExternalCommandsReset();
			return;
		}
		ALLOC_COMMAND(FRHICommandPostExternalCommandsReset)();
	}
};

template<> RHI_API void FRHICommandSetShaderParameter<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderUniformBuffer<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderTexture<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderResourceViewParameter<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetShaderSampler<FRHIGraphicsShader>::Execute(FRHICommandListBase& CmdList);
template<> RHI_API void FRHICommandSetUAVParameter<FRHIPixelShader>::Execute(FRHICommandListBase& CmdList);

class RHI_API FRHICommandList : public FRHIComputeCommandList
{
public:
	FRHICommandList(FRHIGPUMask GPUMask = FRHIGPUMask::All(), ERecordingThread InRecordingThread = ERecordingThread::Render)
		: FRHIComputeCommandList(GPUMask, InRecordingThread)
	{}

	FRHICommandList(FRHICommandListBase&& Other)
		: FRHIComputeCommandList(MoveTemp(Other))
	{}

	inline FRHIVertexShader*        GetBoundVertexShader       () const { return PersistentState.BoundShaderInput.VertexShaderRHI;          }
	inline FRHIMeshShader*          GetBoundMeshShader         () const { return PersistentState.BoundShaderInput.GetMeshShader();          }
	inline FRHIAmplificationShader* GetBoundAmplificationShader() const { return PersistentState.BoundShaderInput.GetAmplificationShader(); }
	inline FRHIPixelShader*         GetBoundPixelShader        () const { return PersistentState.BoundShaderInput.PixelShaderRHI;           }
	inline FRHIGeometryShader*      GetBoundGeometryShader     () const { return PersistentState.BoundShaderInput.GetGeometryShader();      }

	template <typename LAMBDA>
	FORCEINLINE_DEBUGGABLE void EnqueueLambda(LAMBDA&& Lambda)
	{
		if (IsBottomOfPipe())
		{
			Lambda(*this);
		}
		else
		{
			ALLOC_COMMAND(TRHILambdaCommand<FRHICommandList, LAMBDA>)(Forward<LAMBDA>(Lambda));
		}
	}

	FORCEINLINE_DEBUGGABLE void BeginUpdateMultiFrameResource(FRHITexture* Texture)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIBeginUpdateMultiFrameResource( Texture);
			return;
		}
		ALLOC_COMMAND(FRHICommandBeginUpdateMultiFrameResource)(Texture);
	}

	FORCEINLINE_DEBUGGABLE void EndUpdateMultiFrameResource(FRHITexture* Texture)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIEndUpdateMultiFrameResource(Texture);
			return;
		}
		ALLOC_COMMAND(FRHICommandEndUpdateMultiFrameResource)(Texture);
	}

	FORCEINLINE_DEBUGGABLE void BeginUpdateMultiFrameResource(FRHIUnorderedAccessView* UAV)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIBeginUpdateMultiFrameResource(UAV);
			return;
		}
		ALLOC_COMMAND(FRHICommandBeginUpdateMultiFrameUAV)(UAV);
	}

	FORCEINLINE_DEBUGGABLE void EndUpdateMultiFrameResource(FRHIUnorderedAccessView* UAV)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIEndUpdateMultiFrameResource(UAV);
			return;
		}
		ALLOC_COMMAND(FRHICommandEndUpdateMultiFrameUAV)(UAV);
	}

	using FRHIComputeCommandList::SetShaderUniformBuffer;

	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(FRHIGraphicsShader* Shader, uint32 BaseIndex, FRHIUniformBuffer* UniformBuffer)
	{
		//check(IsOutsideRenderPass());
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetContext().RHISetShaderUniformBuffer(Shader, BaseIndex, UniformBuffer);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderUniformBuffer<FRHIGraphicsShader>)(Shader, BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE void SetShaderUniformBuffer(const TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, FRHIUniformBuffer* UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	using FRHIComputeCommandList::SetShaderParameter;

	FORCEINLINE_DEBUGGABLE void SetShaderParameter(FRHIGraphicsShader* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		//check(IsOutsideRenderPass());
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetContext().RHISetShaderParameter(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		ALLOC_COMMAND(FRHICommandSetShaderParameter<FRHIGraphicsShader>)(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}

	template <typename TShaderRHI>
	FORCEINLINE void SetShaderParameter(const TRefCountPtr<TShaderRHI>& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}

	using FRHIComputeCommandList::SetShaderTexture;

	FORCEINLINE_DEBUGGABLE void SetShaderTexture(FRHIGraphicsShader* Shader, uint32 TextureIndex, FRHITexture* Texture)
	{
		//check(IsOutsideRenderPass());
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetContext().RHISetShaderTexture(Shader, TextureIndex, Texture);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderTexture<FRHIGraphicsShader>)(Shader, TextureIndex, Texture);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(const TRefCountPtr<TShaderRHI>& Shader, uint32 TextureIndex, FRHITexture* Texture)
	{
		SetShaderTexture(Shader.GetReference(), TextureIndex, Texture);
	}

	using FRHIComputeCommandList::SetShaderResourceViewParameter;

	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(FRHIGraphicsShader* Shader, uint32 SamplerIndex, FRHIShaderResourceView* SRV)
	{
		//check(IsOutsideRenderPass());
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetContext().RHISetShaderResourceViewParameter(Shader, SamplerIndex, SRV);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderResourceViewParameter<FRHIGraphicsShader>)(Shader, SamplerIndex, SRV);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(const TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FRHIShaderResourceView* SRV)
	{
		SetShaderResourceViewParameter(Shader.GetReference(), SamplerIndex, SRV);
	}

	using FRHIComputeCommandList::SetShaderSampler;

	FORCEINLINE_DEBUGGABLE void SetShaderSampler(FRHIGraphicsShader* Shader, uint32 SamplerIndex, FRHISamplerState* State)
	{
		//check(IsOutsideRenderPass());
		ValidateBoundShader(Shader);
		
		// Immutable samplers can't be set dynamically
		check(!State->IsImmutable());
		if (State->IsImmutable())
		{
			return;
		}

		if (Bypass())
		{
			GetContext().RHISetShaderSampler(Shader, SamplerIndex, State);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShaderSampler<FRHIGraphicsShader>)(Shader, SamplerIndex, State);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(const TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FRHISamplerState* State)
	{
		SetShaderSampler(Shader.GetReference(), SamplerIndex, State);
	}

	using FRHIComputeCommandList::SetUAVParameter;

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FRHIPixelShader* Shader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV)
	{
		ValidateBoundShader(Shader);
		if (Bypass())
		{
			GetContext().RHISetUAVParameter(Shader, UAVIndex, UAV);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetUAVParameter<FRHIPixelShader>)(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(const TRefCountPtr<FRHIPixelShader>& Shader, uint32 UAVIndex, FRHIUnorderedAccessView* UAV)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetBlendFactor(const FLinearColor& BlendFactor = FLinearColor::White)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetBlendFactor(BlendFactor);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetBlendFactor)(BlendFactor);
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIDrawPrimitive(BaseVertexIndex, NumPrimitives, NumInstances);
			return;
		}
		ALLOC_COMMAND(FRHICommandDrawPrimitive)(BaseVertexIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitive(FRHIBuffer* IndexBuffer, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (!IndexBuffer)
		{
			UE_LOG(LogRHI, Fatal, TEXT("Tried to call DrawIndexedPrimitive with null IndexBuffer!"));
		}

		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIDrawIndexedPrimitive(IndexBuffer, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
			return;
		}
		ALLOC_COMMAND(FRHICommandDrawIndexedPrimitive)(IndexBuffer, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void SetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBuffer, uint32 Offset)
	{
		if (Bypass())
		{
			GetContext().RHISetStreamSource(StreamIndex, VertexBuffer, Offset);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetStreamSource)(StreamIndex, VertexBuffer, Offset);
	}

	FORCEINLINE_DEBUGGABLE void SetStencilRef(uint32 StencilRef)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetStencilRef(StencilRef);
			return;
		}

		ALLOC_COMMAND(FRHICommandSetStencilRef)(StencilRef);
	}

	FORCEINLINE_DEBUGGABLE void SetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetStereoViewport(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetStereoViewport)(LeftMinX, RightMinX, LeftMinY, RightMinY, MinZ, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetScissorRect(bEnable, MinX, MinY, MaxX, MaxY);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
	}

	void ApplyCachedRenderTargets(
		FGraphicsPipelineStateInitializer& GraphicsPSOInit
		)
	{
		GraphicsPSOInit.RenderTargetsEnabled = PersistentState.CachedNumSimultanousRenderTargets;

		for (uint32 i = 0; i < GraphicsPSOInit.RenderTargetsEnabled; ++i)
		{
			if (PersistentState.CachedRenderTargets[i].Texture)
			{
				GraphicsPSOInit.RenderTargetFormats[i] = UE_PIXELFORMAT_TO_UINT8(PersistentState.CachedRenderTargets[i].Texture->GetFormat());
				GraphicsPSOInit.RenderTargetFlags[i] = PersistentState.CachedRenderTargets[i].Texture->GetFlags();
			}
			else
			{
				GraphicsPSOInit.RenderTargetFormats[i] = PF_Unknown;
			}

			if (GraphicsPSOInit.RenderTargetFormats[i] != PF_Unknown)
			{
				GraphicsPSOInit.NumSamples = static_cast<uint16>(PersistentState.CachedRenderTargets[i].Texture->GetNumSamples());
			}
		}

		if (PersistentState.CachedDepthStencilTarget.Texture)
		{
			GraphicsPSOInit.DepthStencilTargetFormat = PersistentState.CachedDepthStencilTarget.Texture->GetFormat();
			GraphicsPSOInit.DepthStencilTargetFlag = PersistentState.CachedDepthStencilTarget.Texture->GetFlags();
			const FRHITexture2DArray* TextureArray = PersistentState.CachedDepthStencilTarget.Texture->GetTexture2DArray();
		}
		else
		{
			GraphicsPSOInit.DepthStencilTargetFormat = PF_Unknown;
		}

		GraphicsPSOInit.DepthTargetLoadAction = PersistentState.CachedDepthStencilTarget.DepthLoadAction;
		GraphicsPSOInit.DepthTargetStoreAction = PersistentState.CachedDepthStencilTarget.DepthStoreAction;
		GraphicsPSOInit.StencilTargetLoadAction = PersistentState.CachedDepthStencilTarget.StencilLoadAction;
		GraphicsPSOInit.StencilTargetStoreAction = PersistentState.CachedDepthStencilTarget.GetStencilStoreAction();
		GraphicsPSOInit.DepthStencilAccess = PersistentState.CachedDepthStencilTarget.GetDepthStencilAccess();

		if (GraphicsPSOInit.DepthStencilTargetFormat != PF_Unknown)
		{
			GraphicsPSOInit.NumSamples =  static_cast<uint16>(PersistentState.CachedDepthStencilTarget.Texture->GetNumSamples());
		}

		GraphicsPSOInit.SubpassHint = PersistentState.SubpassHint;
		GraphicsPSOInit.SubpassIndex = PersistentState.SubpassIndex;
		GraphicsPSOInit.MultiViewCount = PersistentState.MultiViewCount;
		GraphicsPSOInit.bHasFragmentDensityAttachment = PersistentState.HasFragmentDensityAttachment;
	}

	FORCEINLINE_DEBUGGABLE void SetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState, const FBoundShaderStateInput& ShaderInput, uint32 StencilRef, bool bApplyAdditionalState)
	{
		//check(IsOutsideRenderPass());
		PersistentState.BoundShaderInput = ShaderInput;
		if (Bypass())
		{
			extern RHI_API FRHIGraphicsPipelineState* ExecuteSetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState);
			FRHIGraphicsPipelineState* RHIGraphicsPipelineState = ExecuteSetGraphicsPipelineState(GraphicsPipelineState);
			GetContext().RHISetGraphicsPipelineState(RHIGraphicsPipelineState, StencilRef, bApplyAdditionalState);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetGraphicsPipelineState)(GraphicsPipelineState, StencilRef, bApplyAdditionalState);
	}

	UE_DEPRECATED(5.0, "SetGraphicsPipelineState now requires a StencilRef argument")
	FORCEINLINE_DEBUGGABLE void SetGraphicsPipelineState(class FGraphicsPipelineState* GraphicsPipelineState, const FBoundShaderStateInput& ShaderInput, bool bApplyAdditionalState)
	{
		SetGraphicsPipelineState(GraphicsPipelineState, ShaderInput, 0, bApplyAdditionalState);
	}

#if PLATFORM_USE_FALLBACK_PSO
	FORCEINLINE_DEBUGGABLE void SetGraphicsPipelineState(const FGraphicsPipelineStateInitializer& PsoInit, uint32 StencilRef, bool bApplyAdditionalState)
	{
		//check(IsOutsideRenderPass());
		PersistentState.BoundShaderInput = PsoInit.BoundShaderState;
		if (Bypass())
		{
			GetContext().RHISetGraphicsPipelineState(PsoInit, StencilRef, bApplyAdditionalState);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetGraphicsPipelineStateFromInitializer)(PsoInit, StencilRef, bApplyAdditionalState);
	}
#endif

	FORCEINLINE_DEBUGGABLE void DrawPrimitiveIndirect(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIDrawPrimitiveIndirect(ArgumentBuffer, ArgumentOffset);
			return;
		}
		ALLOC_COMMAND(FRHICommandDrawPrimitiveIndirect)(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, uint32 DrawArgumentsIndex, uint32 NumInstances)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIDrawIndexedIndirect(IndexBufferRHI, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
			return;
		}
		ALLOC_COMMAND(FRHICommandDrawIndexedIndirect)(IndexBufferRHI, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBuffer, FRHIBuffer* ArgumentsBuffer, uint32 ArgumentOffset)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHIDrawIndexedPrimitiveIndirect(IndexBuffer, ArgumentsBuffer, ArgumentOffset);
			return;
		}
		ALLOC_COMMAND(FRHICommandDrawIndexedPrimitiveIndirect)(IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void DispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			GetContext().RHIDispatchMeshShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		ALLOC_COMMAND(FRHICommandDispatchMeshShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectMeshShader(FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			GetContext().RHIDispatchIndirectMeshShader(ArgumentBuffer, ArgumentOffset);
			return;
		}
		ALLOC_COMMAND(FRHICommandDispatchIndirectMeshShader)(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void SetDepthBounds(float MinDepth, float MaxDepth)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHISetDepthBounds(MinDepth, MaxDepth);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetDepthBounds)(MinDepth, MaxDepth);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShadingRate(EVRSShadingRate ShadingRate, EVRSRateCombiner Combiner)
	{
#if PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
		if (Bypass())
		{
			GetContext().RHISetShadingRate(ShadingRate, Combiner);
			return;
		}
		ALLOC_COMMAND(FRHICommandSetShadingRate)(ShadingRate, Combiner);
#endif
	}

	UE_DEPRECATED(5.1, "CopyToResolveTarget is deprecated. Use render passes for MSAA resolves, or CopyTexture for copies instead.")
	FORCEINLINE_DEBUGGABLE void CopyToResolveTarget(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FResolveParams& ResolveParams)
	{
		//check(IsOutsideRenderPass());
		if (Bypass())
		{
			GetContext().RHICopyToResolveTarget(SourceTextureRHI, DestTextureRHI, ResolveParams);
			return;
		}
		ALLOC_COMMAND(FRHICommandCopyToResolveTarget)(SourceTextureRHI, DestTextureRHI, ResolveParams);
	}

	FORCEINLINE_DEBUGGABLE void CopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo)
	{
		check(SourceTextureRHI && DestTextureRHI);
		check(SourceTextureRHI != DestTextureRHI);
		check(IsOutsideRenderPass());

		if (Bypass())
		{
			GetContext().RHICopyTexture(SourceTextureRHI, DestTextureRHI, CopyInfo);
			return;
		}
		ALLOC_COMMAND(FRHICommandCopyTexture)(SourceTextureRHI, DestTextureRHI, CopyInfo);
	}

	FORCEINLINE_DEBUGGABLE void ResummarizeHTile(FRHITexture2D* DepthTexture)
	{
		if (Bypass())
		{
			GetContext().RHIResummarizeHTile(DepthTexture);
			return;
		}
		ALLOC_COMMAND(FRHICommandResummarizeHTile)(DepthTexture);
	}

	FORCEINLINE_DEBUGGABLE void BeginRenderQuery(FRHIRenderQuery* RenderQuery)
	{
		GDynamicRHI->RHIBeginRenderQuery_TopOfPipe(*this, RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void EndRenderQuery(FRHIRenderQuery* RenderQuery)
	{
		GDynamicRHI->RHIEndRenderQuery_TopOfPipe(*this, RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void CalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery)
	{
		if (Bypass())
		{
			GetContext().RHICalibrateTimers(CalibrationQuery);
			return;
		}
		ALLOC_COMMAND(FRHICommandCalibrateTimers)(CalibrationQuery);
	}

	FORCEINLINE_DEBUGGABLE void PollOcclusionQueries()
	{
		if (Bypass())
		{
			GetContext().RHIPollOcclusionQueries();
			return;
		}
		ALLOC_COMMAND(FRHICommandPollOcclusionQueries)();
	}

	/* LEGACY API */

	using FRHIComputeCommandList::TransitionResource;
	using FRHIComputeCommandList::TransitionResources;

	UE_DEPRECATED(5.1, "TransitionResource has been deprecated. Use Transition instead.")
	FORCEINLINE_DEBUGGABLE void TransitionResource(FExclusiveDepthStencil DepthStencilMode, FRHITexture* DepthTexture)
	{
		check(DepthStencilMode.IsUsingDepth() || DepthStencilMode.IsUsingStencil());

		TArray<FRHITransitionInfo, TInlineAllocator<2>> Infos;

		DepthStencilMode.EnumerateSubresources([&](ERHIAccess NewAccess, uint32 PlaneSlice)
		{
			FRHITransitionInfo Info;
			Info.Type = FRHITransitionInfo::EType::Texture;
			Info.Texture = DepthTexture;
			Info.AccessAfter = NewAccess;
			Info.PlaneSlice = PlaneSlice;
			Infos.Emplace(Info);
		});

		FRHIComputeCommandList::Transition(MakeArrayView(Infos));
	}

	/* LEGACY API */

	FORCEINLINE_DEBUGGABLE void BeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* Name)
	{
		check(!IsInsideRenderPass());
		check(!IsInsideComputePass());

		InInfo.Validate();

		if (Bypass())
		{
			GetContext().RHIBeginRenderPass(InInfo, Name);
		}
		else
		{
			TCHAR* NameCopy  = AllocString(Name);
			ALLOC_COMMAND(FRHICommandBeginRenderPass)(InInfo, NameCopy);
		}

		CacheActiveRenderTargets(InInfo);
		ResetSubpass(InInfo.SubpassHint);
		PersistentState.bInsideRenderPass = true;

		if (InInfo.NumOcclusionQueries)
		{
			PersistentState.bInsideOcclusionQueryBatch = true;
			GDynamicRHI->RHIBeginOcclusionQueryBatch_TopOfPipe(*this, InInfo.NumOcclusionQueries);
		}
	}

	void EndRenderPass()
	{
		check(IsInsideRenderPass());
		check(!IsInsideComputePass());

		if (PersistentState.bInsideOcclusionQueryBatch)
		{
			GDynamicRHI->RHIEndOcclusionQueryBatch_TopOfPipe(*this);
			PersistentState.bInsideOcclusionQueryBatch = false;
		}

		if (Bypass())
		{
			GetContext().RHIEndRenderPass();
		}
		else
		{
			ALLOC_COMMAND(FRHICommandEndRenderPass)();
		}
		PersistentState.bInsideRenderPass = false;
		ResetSubpass(ESubpassHint::None);
	}

	FORCEINLINE_DEBUGGABLE void NextSubpass()
	{
		check(IsInsideRenderPass());
		if (Bypass())
		{
			GetContext().RHINextSubpass();
		}
		else
		{
			ALLOC_COMMAND(FRHICommandNextSubpass)();
		}
		IncrementSubpass();
	}

	// These 6 are special in that they must be called on the immediate command list and they force a flush only when we are not doing RHI thread
	void BeginScene();
	void EndScene();
	void BeginDrawingViewport(FRHIViewport* Viewport, FRHITexture* RenderTargetRHI);
	void EndDrawingViewport(FRHIViewport* Viewport, bool bPresent, bool bLockToVsync);
	void BeginFrame();
	void EndFrame();

	FORCEINLINE_DEBUGGABLE void RHIInvalidateCachedState()
	{
		if (Bypass())
		{
			GetContext().RHIInvalidateCachedState();
			return;
		}
		ALLOC_COMMAND(FRHICommandInvalidateCachedState)();
	}

	FORCEINLINE void DiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
	{
		if (Bypass())
		{
			GetContext().RHIDiscardRenderTargets(Depth, Stencil, ColorBitMask);
			return;
		}
		ALLOC_COMMAND(FRHICommandDiscardRenderTargets)(Depth, Stencil, ColorBitMask);
	}
	
	FORCEINLINE_DEBUGGABLE void CopyBufferRegion(FRHIBuffer* DestBuffer, uint64 DstOffset, FRHIBuffer* SourceBuffer, uint64 SrcOffset, uint64 NumBytes)
	{
		// No copy/DMA operation inside render passes
		check(IsOutsideRenderPass());

		if (Bypass())
		{
			GetContext().RHICopyBufferRegion(DestBuffer, DstOffset, SourceBuffer, SrcOffset, NumBytes);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandCopyBufferRegion)(DestBuffer, DstOffset, SourceBuffer, SrcOffset, NumBytes);
		}
	}

#if RHI_RAYTRACING
	// Ray tracing API

	FORCEINLINE_DEBUGGABLE void ClearRayTracingBindings(FRHIRayTracingScene* Scene)
	{
		if (Bypass())
		{
			GetContext().RHIClearRayTracingBindings(Scene);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandClearRayTracingBindings)(Scene);
		}
	}

	/**
	 * Trace rays from an input buffer of FBasicRayData.
	 * Binary intersection results are written to output buffer as R32_UINTs.
	 * 0xFFFFFFFF is written if ray intersects any scene triangle, 0 otherwise.
	 */
	UE_DEPRECATED(5.1, "Please use an explicit ray generation shader and RayTraceDispatch() instead.")
	FORCEINLINE_DEBUGGABLE void RayTraceOcclusion(FRHIRayTracingScene* Scene,
		FRHIShaderResourceView* Rays,
		FRHIUnorderedAccessView* Output,
		uint32 NumRays)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (Bypass())
		{
			GetContext().RHIRayTraceOcclusion(Scene, Rays, Output, NumRays);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandRayTraceOcclusion)(Scene, Rays, Output, NumRays);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Trace rays from an input buffer of FBasicRayData.
	 * Primitive intersection results are written to output buffer as FIntersectionPayload.
	 */
	UE_DEPRECATED(5.1, "Please use an explicit ray generation shader and RayTraceDispatch() instead.")
	FORCEINLINE_DEBUGGABLE void RayTraceIntersection(FRHIRayTracingScene* Scene,
		FRHIShaderResourceView* Rays,
		FRHIUnorderedAccessView* Output,
		uint32 NumRays)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (Bypass())
		{
			GetContext().RHIRayTraceIntersection(Scene, Rays, Output, NumRays);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandRayTraceIntersection)(Scene, Rays, Output, NumRays);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	FORCEINLINE_DEBUGGABLE void RayTraceDispatch(FRayTracingPipelineState* Pipeline, FRHIRayTracingShader* RayGenShader, FRHIRayTracingScene* Scene, const FRayTracingShaderBindings& GlobalResourceBindings, uint32 Width, uint32 Height)
	{
		if (Bypass())
		{
			extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
			GetContext().RHIRayTraceDispatch(GetRHIRayTracingPipelineState(Pipeline), RayGenShader, Scene, GlobalResourceBindings, Width, Height);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandRayTraceDispatch)(Pipeline, RayGenShader, Scene, GlobalResourceBindings, Width, Height);
		}
	}

	/**
	 * Trace rays using dimensions from a GPU buffer containing uint[3], interpreted as number of rays in X, Y and Z dimensions.
	 * ArgumentBuffer must be in IndirectArgs|SRVCompute state.
	 */
	FORCEINLINE_DEBUGGABLE void RayTraceDispatchIndirect(FRayTracingPipelineState* Pipeline, FRHIRayTracingShader* RayGenShader, FRHIRayTracingScene* Scene, const FRayTracingShaderBindings& GlobalResourceBindings, FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
	{
		checkf(GRHISupportsRayTracingDispatchIndirect, TEXT("Indirect ray tracing is not supported on this machine."));
		if (Bypass())
		{
			extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
			GetContext().RHIRayTraceDispatchIndirect(GetRHIRayTracingPipelineState(Pipeline), RayGenShader, Scene, GlobalResourceBindings, ArgumentBuffer, ArgumentOffset);
		}
		else
		{
			ALLOC_COMMAND(FRHICommandRayTraceDispatch)(Pipeline, RayGenShader, Scene, GlobalResourceBindings, ArgumentBuffer, ArgumentOffset);
		}
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingBindings(
		FRHIRayTracingScene* Scene, FRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
		ERayTracingBindingType BindingType,
		bool bCopyDataToInlineStorage = true)
	{
		if (Bypass())
		{
			extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
			GetContext().RHISetRayTracingBindings(Scene, GetRHIRayTracingPipelineState(Pipeline), NumBindings, Bindings, BindingType);
		}
		else
		{
			FRayTracingLocalShaderBindings* InlineBindings = nullptr;

			// By default all batch binding data is stored in the command list memory.
			// However, user may skip this copy if they take responsibility for keeping data alive until this command is executed.
			if (bCopyDataToInlineStorage)
			{
				if (NumBindings)
				{
					uint32 Size = sizeof(FRayTracingLocalShaderBindings) * NumBindings;
					InlineBindings = (FRayTracingLocalShaderBindings*)Alloc(Size, alignof(FRayTracingLocalShaderBindings));
					FMemory::Memcpy(InlineBindings, Bindings, Size);
				}

				for (uint32 i = 0; i < NumBindings; ++i)
				{
					if (InlineBindings[i].NumUniformBuffers)
					{
						InlineBindings[i].UniformBuffers = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * InlineBindings[i].NumUniformBuffers, alignof(FRHIUniformBuffer*));
						for (uint32 Index = 0; Index < InlineBindings[i].NumUniformBuffers; ++Index)
						{
							InlineBindings[i].UniformBuffers[Index] = Bindings[i].UniformBuffers[Index];
						}
					}

					if (InlineBindings[i].LooseParameterDataSize)
					{
						InlineBindings[i].LooseParameterData = (uint8*)Alloc(InlineBindings[i].LooseParameterDataSize, 16);
						FMemory::Memcpy(InlineBindings[i].LooseParameterData, Bindings[i].LooseParameterData, InlineBindings[i].LooseParameterDataSize);
					}
				}

				ALLOC_COMMAND(FRHICommandSetRayTracingBindings)(Scene, Pipeline, NumBindings, InlineBindings, BindingType);
			}
			else
			{
				ALLOC_COMMAND(FRHICommandSetRayTracingBindings)(Scene, Pipeline, NumBindings, Bindings, BindingType);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingHitGroups(
		FRHIRayTracingScene* Scene, FRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
		bool bCopyDataToInlineStorage = true)
	{
		SetRayTracingBindings(Scene, Pipeline, NumBindings, Bindings, ERayTracingBindingType::HitGroup, bCopyDataToInlineStorage);
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingCallableShaders(
		FRHIRayTracingScene* Scene, FRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
		bool bCopyDataToInlineStorage = true)
	{
		SetRayTracingBindings(Scene, Pipeline, NumBindings, Bindings, ERayTracingBindingType::CallableShader, bCopyDataToInlineStorage);
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingMissShaders(
		FRHIRayTracingScene* Scene, FRayTracingPipelineState* Pipeline,
		uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
		bool bCopyDataToInlineStorage = true)
	{
		SetRayTracingBindings(Scene, Pipeline, NumBindings, Bindings, ERayTracingBindingType::MissShader, bCopyDataToInlineStorage);
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingHitGroup(
		FRHIRayTracingScene* Scene, uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot,
		FRayTracingPipelineState* Pipeline, uint32 HitGroupIndex,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 LooseParameterDataSize, const void* LooseParameterData,
		uint32 UserData)
	{
		if (Bypass())
		{
			extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
			GetContext().RHISetRayTracingHitGroup(Scene, InstanceIndex, SegmentIndex, ShaderSlot, GetRHIRayTracingPipelineState(Pipeline), HitGroupIndex, 
				NumUniformBuffers, UniformBuffers,
				LooseParameterDataSize, LooseParameterData,
				UserData);
		}
		else
		{
			FRHIUniformBuffer** InlineUniformBuffers = nullptr;
			if (NumUniformBuffers)
			{
				InlineUniformBuffers = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * NumUniformBuffers, alignof(FRHIUniformBuffer*));
				for (uint32 Index = 0; Index < NumUniformBuffers; ++Index)
				{
					InlineUniformBuffers[Index] = UniformBuffers[Index];
				}
			}

			void* InlineLooseParameterData = nullptr;
			if (LooseParameterDataSize)
			{
				InlineLooseParameterData = Alloc(LooseParameterDataSize, 16);
				FMemory::Memcpy(InlineLooseParameterData, LooseParameterData, LooseParameterDataSize);
			}

			ALLOC_COMMAND(FRHICommandSetRayTracingBindings)(Scene, InstanceIndex, SegmentIndex, ShaderSlot, Pipeline, HitGroupIndex, 
				NumUniformBuffers, InlineUniformBuffers, 
				LooseParameterDataSize, InlineLooseParameterData,
				UserData);
		}
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingCallableShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData)
	{
		if (Bypass())
		{
			extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
			GetContext().RHISetRayTracingCallableShader(Scene, ShaderSlotInScene, GetRHIRayTracingPipelineState(Pipeline), ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, UserData);
		}
		else
		{
			FRHIUniformBuffer** InlineUniformBuffers = nullptr;
			if (NumUniformBuffers)
			{
				InlineUniformBuffers = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * NumUniformBuffers, alignof(FRHIUniformBuffer*));
				for (uint32 Index = 0; Index < NumUniformBuffers; ++Index)
				{
					InlineUniformBuffers[Index] = UniformBuffers[Index];
				}
			}

			ALLOC_COMMAND(FRHICommandSetRayTracingBindings)(Scene, ShaderSlotInScene, Pipeline, ShaderIndexInPipeline, NumUniformBuffers, InlineUniformBuffers, UserData, ERayTracingBindingType::CallableShader);
		}
	}

	FORCEINLINE_DEBUGGABLE void SetRayTracingMissShader(
		FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
		FRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
		uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
		uint32 UserData)
	{
		if (Bypass())
		{
			extern RHI_API FRHIRayTracingPipelineState* GetRHIRayTracingPipelineState(FRayTracingPipelineState*);
			GetContext().RHISetRayTracingMissShader(Scene, ShaderSlotInScene, GetRHIRayTracingPipelineState(Pipeline), ShaderIndexInPipeline, NumUniformBuffers, UniformBuffers, UserData);
		}
		else
		{
			FRHIUniformBuffer** InlineUniformBuffers = nullptr;
			if (NumUniformBuffers)
			{
				InlineUniformBuffers = (FRHIUniformBuffer**)Alloc(sizeof(FRHIUniformBuffer*) * NumUniformBuffers, alignof(FRHIUniformBuffer*));
				for (uint32 Index = 0; Index < NumUniformBuffers; ++Index)
				{
					InlineUniformBuffers[Index] = UniformBuffers[Index];
				}
			}

			ALLOC_COMMAND(FRHICommandSetRayTracingBindings)(Scene, ShaderSlotInScene, Pipeline, ShaderIndexInPipeline, NumUniformBuffers, InlineUniformBuffers, UserData, ERayTracingBindingType::MissShader);
		}
	}

#endif // RHI_RAYTRACING
};

namespace EImmediateFlushType
{
	enum Type
	{ 
		WaitForOutstandingTasksOnly  = 0, 
		DispatchToRHIThread          = 1, 
		FlushRHIThread               = 2,
		FlushRHIThreadFlushResources = 3
	};
};

class FScopedRHIThreadStaller
{
	class FRHICommandListImmediate* Immed; // non-null if we need to unstall
public:
	FScopedRHIThreadStaller() = delete;
	FScopedRHIThreadStaller(class FRHICommandListImmediate& InImmed, bool bDoStall = true);
	~FScopedRHIThreadStaller();
};


// Forward declare RHI creation function so they can still be called from the deprecated immediate command list resource creation functions
FBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
FBufferRHIRef RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
FBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);

extern RHI_API ERHIAccess RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData);
extern RHI_API ERHIAccess RHIGetDefaultResourceState(EBufferUsageFlags InUsage, bool bInHasInitialData);

class RHI_API FRHICommandListImmediate : public FRHICommandList
{
	friend class FRHICommandListExecutor;

	static FGraphEventArray WaitOutstandingTasks;
	static FGraphEventRef   RHIThreadTask;

	FRHICommandListImmediate()
		: FRHICommandList(FRHIGPUMask::All(), ERecordingThread::Render)
	{
		PersistentState.bImmediate = true;
	}

	~FRHICommandListImmediate()
	{
		// Need to close the graph event when the engine is shutting down.
		DispatchEvent->DispatchSubsequents();
	}

	//
	// Executes commands recorded in the immediate RHI command list, and resets the command list to a default constructed state.
	//
	// This is the main function for submitting work from the render thread to the RHI thread. Work is also submitted to the GPU
	// as soon as possible. Does not wait for command completion on either the RHI thread or the GPU.
	//
	// Used internally. Do not call directly. Use FRHICommandListImmediate::ImmediateFlush() to submit GPU work.
	//
	void ExecuteAndReset();

	//
	// Blocks the calling thread until all dispatch prerequisites of enqueued parallel command lists are completed.
	//
	void WaitForTasks();

	//
	// Blocks the calling thread until the RHI thread is idle.
	//
	void WaitForRHIThreadTasks();

	//
	// Destroys and recreates the immediate command list.
	//
	void Reset();

public:
	struct FQueuedCommandList
	{
		// The command list to enqueue.
		FRHICommandListBase* CmdList = nullptr;

		// The total number of draw calls made for this command list, if known. Used to load-balance the parallel translate worker threads.
		// If not specified, each queued command list will generate its own parallel translate task + platform RHI command list submission, which may be inefficient.
		TOptional<uint32> NumDraws;

		FQueuedCommandList() = default;
		FQueuedCommandList(FRHICommandListBase* InCmdList, TOptional<uint32> InNumDraws = {})
			: CmdList(InCmdList)
			, NumDraws(InNumDraws)
		{}
	};

	enum class ETranslatePriority
	{
		Disabled, // Parallel translate is disabled. Command lists will be replayed by the RHI thread into the default context.
		Normal,   // Parallel translate is enabled, and runs on a normal priority task thread.
		High      // Parallel translate is enabled, and runs on a high priority task thread.
	};

	//
	// Chains together one or more RHI command lists into the immediate command list, allowing in-order submission of parallel rendering work.
	// The provided command lists are not dispatched until FinishRecording() is called on them, and their dispatch prerequisites have been completed.
	//
	void QueueAsyncCommandListSubmit(TArrayView<FQueuedCommandList> CommandLists, ETranslatePriority ParallelTranslatePriority = ETranslatePriority::Disabled, int32 MinDrawsPerTranslate = 0);
	
	inline void QueueAsyncCommandListSubmit(FQueuedCommandList QueuedCommandList, ETranslatePriority ParallelTranslatePriority = ETranslatePriority::Disabled, int32 MinDrawsPerTranslate = 0)
	{
		QueueAsyncCommandListSubmit(MakeArrayView(&QueuedCommandList, 1), ParallelTranslatePriority, MinDrawsPerTranslate);
	}

	//
	// Dispatches work to the RHI thread and the GPU.
	// Also optionally waits for its completion on the RHI thread. Does not wait for the GPU.
	//
	void ImmediateFlush(EImmediateFlushType::Type FlushType);

	bool StallRHIThread();
	void UnStallRHIThread();
	static bool IsStalled();

	static FGraphEventArray& GetRenderThreadTaskArray();

	void InitializeImmediateContexts();

	// Global graph events must be destroyed explicitly to avoid undefined order of static destruction, as they can be destroyed after their allocator.
	static void CleanupGraphEvents();

	//
	// Performs an immediate transition with the option of broadcasting to multiple pipelines.
	// Uses both the immediate and async compute contexts. Falls back to graphics-only if async compute is not supported.
	//
	void Transition(TArrayView<const FRHITransitionInfo> Infos, ERHIPipeline SrcPipelines, ERHIPipeline DstPipelines);
	using FRHIComputeCommandList::Transition;

	template <typename LAMBDA>
	FORCEINLINE_DEBUGGABLE void EnqueueLambda(LAMBDA&& Lambda)
	{
		if (IsBottomOfPipe())
		{
			Lambda(*this);
		}
		else
		{
			ALLOC_COMMAND(TRHILambdaCommand<FRHICommandListImmediate, LAMBDA>)(Forward<LAMBDA>(Lambda));
		}
	}

	FORCEINLINE FSamplerStateRHIRef CreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateSamplerState"));
		return RHICreateSamplerState(Initializer);
	}
	
	FORCEINLINE FRasterizerStateRHIRef CreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateRasterizerState"));
		return RHICreateRasterizerState(Initializer);
	}
	
	FORCEINLINE FDepthStencilStateRHIRef CreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateDepthStencilState"));
		return RHICreateDepthStencilState(Initializer);
	}
	
	FORCEINLINE FBlendStateRHIRef CreateBlendState(const FBlendStateInitializerRHI& Initializer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateBlendState"));
		return RHICreateBlendState(Initializer);
	}
	
	FORCEINLINE FPixelShaderRHIRef CreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreatePixelShader_RenderThread(*this, Code, Hash);
	}
	
	FORCEINLINE FVertexShaderRHIRef CreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateVertexShader_RenderThread(*this, Code, Hash);
	}

	FORCEINLINE FMeshShaderRHIRef CreateMeshShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateMeshShader_RenderThread(*this, Code, Hash);
	}

	FORCEINLINE FAmplificationShaderRHIRef CreateAmplificationShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateAmplificationShader_RenderThread(*this, Code, Hash);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateGeometryShader_RenderThread(*this, Code, Hash);
	}
	
	FORCEINLINE FComputeShaderRHIRef CreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return GDynamicRHI->CreateComputeShader_RenderThread(*this, Code, Hash);
	}
	
	FORCEINLINE FComputeFenceRHIRef CreateComputeFence(const FName& Name)
	{		
		return GDynamicRHI->RHICreateComputeFence(Name);
	}	

	FORCEINLINE FGPUFenceRHIRef CreateGPUFence(const FName& Name)
	{
		return GDynamicRHI->RHICreateGPUFence(Name);
	}

	FORCEINLINE FStagingBufferRHIRef CreateStagingBuffer()
	{
		return GDynamicRHI->RHICreateStagingBuffer();
	}

	FORCEINLINE FBoundShaderStateRHIRef CreateBoundShaderState(FRHIVertexDeclaration* VertexDeclaration, FRHIVertexShader* VertexShader, FRHIPixelShader* PixelShader, FRHIGeometryShader* GeometryShader)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return RHICreateBoundShaderState(VertexDeclaration, VertexShader, PixelShader, GeometryShader);
	}

	FORCEINLINE FGraphicsPipelineStateRHIRef CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return RHICreateGraphicsPipelineState(Initializer);
	}

	FORCEINLINE TRefCountPtr<FRHIComputePipelineState> CreateComputePipelineState(FRHIComputeShader* ComputeShader)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		return RHICreateComputePipelineState(ComputeShader);
	}

	FORCEINLINE FUniformBufferRHIRef CreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage)
	{
		return RHICreateUniformBuffer(Contents, Layout, Usage);
	}

	UE_DEPRECATED(5.0, "Use Layout pointers instead")
	FORCEINLINE FUniformBufferRHIRef CreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
	{
		return RHICreateUniformBuffer(Contents, &Layout, Usage);
	}

	UE_DEPRECATED(5.1, "Use CreateBuffer and LockBuffer separately")
	FORCEINLINE FBufferRHIRef CreateAndLockIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags InUsage, ERHIAccess InResourceState, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		FBufferRHIRef IndexBuffer = CreateBuffer(Size, InUsage | BUF_IndexBuffer, Stride, InResourceState, CreateInfo);
		OutDataBuffer = LockBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);
		return IndexBuffer;
	}

	UE_DEPRECATED(5.1, "Use CreateBuffer and LockBuffer separately")
	FORCEINLINE FBufferRHIRef CreateAndLockIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		EBufferUsageFlags Usage = InUsage | BUF_IndexBuffer;
		ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, true);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return CreateAndLockIndexBuffer(Stride, Size, Usage, ResourceState, CreateInfo, OutDataBuffer);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	UE_DEPRECATED(5.0, "Buffer locks have been unified. Use LockBuffer() instead.")
	FORCEINLINE void* LockIndexBuffer(FRHIBuffer* IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->RHILockBuffer(*this, IndexBuffer, Offset, SizeRHI, LockMode);
	}
	
	UE_DEPRECATED(5.0, "Buffer locks have been unified. Use UnlockBuffer() instead.")
	FORCEINLINE void UnlockIndexBuffer(FRHIBuffer* IndexBuffer)
	{
		GDynamicRHI->RHIUnlockBuffer(*this, IndexBuffer);
	}
	
	FORCEINLINE void* LockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 SizeRHI)
	{
		return GDynamicRHI->LockStagingBuffer_RenderThread(*this, StagingBuffer, Fence, Offset, SizeRHI);
	}
	
	FORCEINLINE void UnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer)
	{
		GDynamicRHI->UnlockStagingBuffer_RenderThread(*this, StagingBuffer);
	}

	UE_DEPRECATED(5.1, "Use CreateBuffer and LockBuffer separately")
	FORCEINLINE FBufferRHIRef CreateAndLockVertexBuffer(uint32 Size, EBufferUsageFlags InUsage, ERHIAccess InResourceState, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		FBufferRHIRef VertexBuffer = CreateBuffer(Size, InUsage | BUF_VertexBuffer, 0, InResourceState, CreateInfo);
		OutDataBuffer = LockBuffer(VertexBuffer, 0, Size, RLM_WriteOnly);
		return VertexBuffer;
	}

	UE_DEPRECATED(5.1, "Use CreateBuffer and LockBuffer separately")
	FORCEINLINE FBufferRHIRef CreateAndLockVertexBuffer(uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		EBufferUsageFlags Usage = InUsage | BUF_VertexBuffer;
		ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, true);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return CreateAndLockVertexBuffer(Size, Usage, ResourceState, CreateInfo, OutDataBuffer);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UE_DEPRECATED(5.0, "Buffer locks have been unified. Use LockBuffer() instead.")
	FORCEINLINE void* LockVertexBuffer(FRHIBuffer* VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->RHILockBuffer(*this, VertexBuffer, Offset, SizeRHI, LockMode);
	}
	
	UE_DEPRECATED(5.0, "Buffer locks have been unified. Use UnlockBuffer() instead.")
	FORCEINLINE void UnlockVertexBuffer(FRHIBuffer* VertexBuffer)
	{
		GDynamicRHI->RHIUnlockBuffer(*this, VertexBuffer);
	}
	
	FORCEINLINE void CopyBuffer(FRHIBuffer* SourceBuffer, FRHIBuffer* DestBuffer)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_CopyBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHICopyBuffer(SourceBuffer, DestBuffer);
	}

	UE_DEPRECATED(5.0, "CopyVertexBuffer() has been replaced with a general CopyBuffer() function.")
	FORCEINLINE void CopyVertexBuffer(FRHIBuffer* SourceBuffer, FRHIBuffer* DestBuffer)
	{
		CopyBuffer(SourceBuffer, DestBuffer);
	}

	UE_DEPRECATED(5.0, "Buffer locks have been unified. Use LockBuffer() instead.")
	FORCEINLINE void* LockStructuredBuffer(FRHIBuffer* StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->RHILockBuffer(*this, StructuredBuffer, Offset, SizeRHI, LockMode);
	}

	UE_DEPRECATED(5.0, "Buffer locks have been unified. Use UnlockBuffer() instead.")
	FORCEINLINE void UnlockStructuredBuffer(FRHIBuffer* StructuredBuffer)
	{
		GDynamicRHI->RHIUnlockBuffer(*this, StructuredBuffer);
	}

	// LockBufferMGPU / UnlockBufferMGPU may ONLY be called for buffers with the EBufferUsageFlags::MultiGPUAllocate flag set!
	// And buffers with that flag set may not call the regular (single GPU) LockBuffer / UnlockBuffer.  The single GPU version
	// of LockBuffer uses driver mirroring to propagate the updated buffer to other GPUs, while the MGPU / MultiGPUAllocate
	// version requires the caller to manually lock and initialize the buffer separately on each GPU.  This can be done by
	// iterating over FRHIGPUMask::All() and calling LockBufferMGPU / UnlockBufferMGPU for each version.
	//
	// EBufferUsageFlags::MultiGPUAllocate is only needed for cases where CPU initialized data needs to be different per GPU,
	// which is a rare edge case.  Currently, this is only used for the ray tracing acceleration structure address buffer,
	// which contains virtual address references to other GPU resources, which may be in a different location on each GPU.
	//
	FORCEINLINE void* LockBufferMGPU(FRHIBuffer* Buffer, uint32 GPUIndex, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->RHILockBufferMGPU(*this, Buffer, GPUIndex, Offset, SizeRHI, LockMode);
	}

	FORCEINLINE void UnlockBufferMGPU(FRHIBuffer* Buffer, uint32 GPUIndex)
	{
		GDynamicRHI->RHIUnlockBufferMGPU(*this, Buffer, GPUIndex);
	}

	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHIBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateUnorderedAccessView"));
		checkf(Buffer, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, Buffer, bUseUAVCounter, bAppendBuffer);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHITexture* Texture, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateUnorderedAccessView"));
		checkf(Texture, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, Texture, MipLevel, FirstArraySlice, NumArraySlices);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHITexture* Texture, uint32 MipLevel, uint8 Format, uint16 FirstArraySlice, uint16 NumArraySlices)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateUnorderedAccessView"));
		checkf(Texture, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, Texture, MipLevel, Format, FirstArraySlice, NumArraySlices);
	}

	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHIBuffer* Buffer, uint8 Format)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateUnorderedAccessView"));
		checkf(Buffer, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, Buffer, Format);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FRHIBuffer* Buffer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceView"));
		checkf(Buffer, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Buffer);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceView"));
		checkf(Buffer, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->CreateShaderResourceView_RenderThread(*this, Buffer, Stride, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(const FShaderResourceViewInitializer& Initializer)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceView"));
		return GDynamicRHI->CreateShaderResourceView_RenderThread(*this, Initializer);
	}
	
	UE_DEPRECATED(5.1, "The CalcTexture... functions on the immediate RHI command list are deprecated. Use the global scope RHICalcTexturePlatformSize instead.")
	FORCEINLINE uint64 CalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo, uint32& OutAlign)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RHICalcTexture2DPlatformSize(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo, OutAlign);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	UE_DEPRECATED(5.1, "The CalcTexture... functions on the immediate RHI command list are deprecated. Use the global scope RHICalcTexturePlatformSize instead.")
	FORCEINLINE uint64 CalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo, uint32& OutAlign)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RHICalcTexture3DPlatformSize(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo, OutAlign);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	UE_DEPRECATED(5.1, "The CalcTexture... functions on the immediate RHI command list are deprecated. Use the global scope RHICalcTexturePlatformSize instead.")
	FORCEINLINE uint64 CalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo, uint32& OutAlign)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RHICalcTextureCubePlatformSize(Size, Format, NumMips, Flags, CreateInfo, OutAlign);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	FORCEINLINE void GetTextureMemoryStats(FTextureMemoryStats& OutStats)
	{
		RHIGetTextureMemoryStats(OutStats);
	}
	
	FORCEINLINE bool GetTextureMemoryVisualizeData(FColor* TextureData,int32 SizeX,int32 SizeY,int32 Pitch,int32 PixelSize)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetTextureMemoryVisualizeData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		return GDynamicRHI->RHIGetTextureMemoryVisualizeData(TextureData,SizeX,SizeY,Pitch,PixelSize);
	}
	
	FORCEINLINE void CopySharedMips(FRHITexture* DestTexture, FRHITexture* SrcTexture)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_CopySharedMips_Flush);
		DestTexture->AddRef();
		SrcTexture->AddRef();
		EnqueueLambda([DestTexture, SrcTexture](FRHICommandList&)
		{
			LLM_SCOPE(ELLMTag::Textures);
			//@todo move this function onto the IRHIComputeCommandContext
			GDynamicRHI->RHICopySharedMips(DestTexture, SrcTexture);
			DestTexture->Release();
			SrcTexture->Release();
		});
	}

	UE_DEPRECATED(5.0, "RHIGetResourceInfo is no longer implemented in favor of FRHIResource::GetResourceInfo.")
	FORCEINLINE void GetResourceInfo(FRHITexture* Ref, FRHIResourceInfo& OutInfo)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RHIGetResourceInfo(Ref, OutInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceView"));
		checkf(Texture, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture, CreateInfo);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FRHITexture* Texture, uint8 MipLevel)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceView"));
		checkf(Texture, TEXT("Can't create a view off a null resource!"));
		const FRHITextureSRVCreateInfo CreateInfo(MipLevel, 1, Texture->GetFormat());
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture, CreateInfo);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FRHITexture* Texture, uint8 MipLevel, uint8 NumMipLevels, EPixelFormat Format)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceView"));
		checkf(Texture, TEXT("Can't create a view off a null resource!"));
		const FRHITextureSRVCreateInfo CreateInfo(MipLevel, NumMipLevels, Format);
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture, CreateInfo);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceViewWriteMask(FRHITexture2D* Texture2DRHI)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceViewWriteMask"));
		checkf(Texture2DRHI, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateShaderResourceViewWriteMask_RenderThread(*this, Texture2DRHI);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceViewFMask(FRHITexture2D* Texture2DRHI)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateShaderResourceViewFMask"));
		checkf(Texture2DRHI, TEXT("Can't create a view off a null resource!"));
		return GDynamicRHI->RHICreateShaderResourceViewFMask_RenderThread(*this, Texture2DRHI);
	}

	//UE_DEPRECATED(4.23, "This function is deprecated and will be removed in future releases. Renderer version implemented.")
	FORCEINLINE void GenerateMips(FRHITexture* Texture)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GenerateMips_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); return GDynamicRHI->RHIGenerateMips(Texture);
	}
	
	FORCEINLINE uint32 ComputeMemorySize(FRHITexture* TextureRHI)
	{
		return RHIComputeMemorySize(TextureRHI);
	}
	
	FORCEINLINE FTexture2DRHIRef AsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->AsyncReallocateTexture2D_RenderThread(*this, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
	
	FORCEINLINE ETextureReallocationStatus FinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->FinalizeAsyncReallocateTexture2D_RenderThread(*this, Texture2D, bBlockUntilCompleted);
	}
	
	FORCEINLINE ETextureReallocationStatus CancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
	{
		return GDynamicRHI->CancelAsyncReallocateTexture2D_RenderThread(*this, Texture2D, bBlockUntilCompleted);
	}
	
	FORCEINLINE void* LockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bFlushRHIThread = true)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->LockTexture2D_RenderThread(*this, Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, bFlushRHIThread);
	}
	
	FORCEINLINE void UnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bFlushRHIThread = true)
	{		
		GDynamicRHI->UnlockTexture2D_RenderThread(*this, Texture, MipIndex, bLockWithinMiptail, bFlushRHIThread);
	}
	
	FORCEINLINE void* LockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->LockTexture2DArray_RenderThread(*this, Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	
	FORCEINLINE void UnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->UnlockTexture2DArray_RenderThread(*this, Texture, TextureIndex, MipIndex, bLockWithinMiptail);
	}
	
	FORCEINLINE void UpdateTexture2D(FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
	{		
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateTexture2D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateTexture2D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->UpdateTexture2D_RenderThread(*this, Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
	}

	FORCEINLINE void UpdateFromBufferTexture2D(FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, FRHIBuffer* Buffer, uint32 BufferOffset)
	{
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateFromBufferTexture2D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateFromBufferTexture2D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->UpdateFromBufferTexture2D_RenderThread(*this, Texture, MipIndex, UpdateRegion, SourcePitch, Buffer, BufferOffset);
	}

	FORCEINLINE FUpdateTexture3DData BeginUpdateTexture3D(FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
	{
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateTexture3D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateTexture3D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		checkf(UpdateRegion.DestZ + UpdateRegion.Depth <= Texture->GetSizeZ(), TEXT("UpdateTexture3D out of bounds on Z. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestZ, UpdateRegion.Depth, Texture->GetSizeZ());
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->BeginUpdateTexture3D_RenderThread(*this, Texture, MipIndex, UpdateRegion);
	}

	FORCEINLINE void EndUpdateTexture3D(FUpdateTexture3DData& UpdateData)
	{		
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->EndUpdateTexture3D_RenderThread(*this, UpdateData);
	}

	FORCEINLINE void EndMultiUpdateTexture3D(TArray<FUpdateTexture3DData>& UpdateDataArray)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->EndMultiUpdateTexture3D_RenderThread(*this, UpdateDataArray);
	}
	
	FORCEINLINE void UpdateTexture3D(FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
	{
		checkf(UpdateRegion.DestX + UpdateRegion.Width <= Texture->GetSizeX(), TEXT("UpdateTexture3D out of bounds on X. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestX, UpdateRegion.Width, Texture->GetSizeX());
		checkf(UpdateRegion.DestY + UpdateRegion.Height <= Texture->GetSizeY(), TEXT("UpdateTexture3D out of bounds on Y. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestY, UpdateRegion.Height, Texture->GetSizeY());
		checkf(UpdateRegion.DestZ + UpdateRegion.Depth <= Texture->GetSizeZ(), TEXT("UpdateTexture3D out of bounds on Z. Texture: %s, %i, %i, %i"), *Texture->GetName().ToString(), UpdateRegion.DestZ, UpdateRegion.Depth, Texture->GetSizeZ());
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->UpdateTexture3D_RenderThread(*this, Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	
	FORCEINLINE void* LockTextureCubeFace(FRHITexture* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
	{
		LLM_SCOPE(ELLMTag::Textures);
		return GDynamicRHI->RHILockTextureCubeFace_RenderThread(*this, Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	
	FORCEINLINE void UnlockTextureCubeFace(FRHITexture* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->RHIUnlockTextureCubeFace_RenderThread(*this, Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
	}
	
	FORCEINLINE void BindDebugLabelName(FRHITexture* Texture, const TCHAR* Name)
	{
		RHIBindDebugLabelName(Texture, Name);
	}

	FORCEINLINE void BindDebugLabelName(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name)
	{
		RHIBindDebugLabelName(UnorderedAccessViewRHI, Name);
	}

	FORCEINLINE void ReadSurfaceData(FRHITexture* Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceData_Flush);
		LLM_SCOPE(ELLMTag::Textures);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIReadSurfaceData(Texture,Rect,OutData,InFlags);
	}

	FORCEINLINE void ReadSurfaceData(FRHITexture* Texture, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceData_Flush);
		LLM_SCOPE(ELLMTag::Textures);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHIReadSurfaceData(Texture, Rect, OutData, InFlags);
	}
	
	FORCEINLINE void MapStagingSurface(FRHITexture* Texture, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex = INDEX_NONE)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->RHIMapStagingSurface_RenderThread(*this, Texture, GPUIndex, nullptr, OutData, OutWidth, OutHeight);
	}

	FORCEINLINE void MapStagingSurface(FRHITexture* Texture, FRHIGPUFence* Fence, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex = INDEX_NONE)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->RHIMapStagingSurface_RenderThread(*this, Texture, GPUIndex, Fence, OutData, OutWidth, OutHeight);
	}
	
	FORCEINLINE void UnmapStagingSurface(FRHITexture* Texture, uint32 GPUIndex = INDEX_NONE)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->RHIUnmapStagingSurface_RenderThread(*this, Texture, GPUIndex);
	}
	
	FORCEINLINE void ReadSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->RHIReadSurfaceFloatData_RenderThread(*this, Texture,Rect,OutData,CubeFace,ArrayIndex,MipIndex);
	}

	FORCEINLINE void ReadSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,FReadSurfaceDataFlags Flags)
	{
		LLM_SCOPE(ELLMTag::Textures);
		GDynamicRHI->RHIReadSurfaceFloatData_RenderThread(*this, Texture,Rect,OutData,Flags);
	}

	FORCEINLINE void Read3DSurfaceFloatData(FRHITexture* Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData, FReadSurfaceDataFlags Flags = FReadSurfaceDataFlags())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_Read3DSurfaceFloatData_Flush);
		LLM_SCOPE(ELLMTag::Textures);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIRead3DSurfaceFloatData(Texture,Rect,ZMinMax,OutData,Flags);
	}

	UE_DEPRECATED(5.0, "AcquireTransientResource_RenderThread API is deprecated; use IRHITransientResourceAllocator instead.")
	FORCEINLINE void AcquireTransientResource_RenderThread(FRHITexture* Texture) {}

	UE_DEPRECATED(5.0, "DiscardTransientResource_RenderThread API is deprecated; use IRHITransientResourceAllocator instead.")
	FORCEINLINE void DiscardTransientResource_RenderThread(FRHITexture* Texture) {}

	UE_DEPRECATED(5.0, "AcquireTransientResource_RenderThread API is deprecated; use IRHITransientResourceAllocator instead.")
	FORCEINLINE void AcquireTransientResource_RenderThread(FRHIBuffer* Buffer) {}

	UE_DEPRECATED(5.0, "DiscardTransientResource_RenderThread API is deprecated; use IRHITransientResourceAllocator instead.")
	FORCEINLINE void DiscardTransientResource_RenderThread(FRHIBuffer* Buffer) {}

	FORCEINLINE bool GetRenderQueryResult(FRHIRenderQuery* RenderQuery, uint64& OutResult, bool bWait, uint32 GPUIndex = INDEX_NONE)
	{
		return RHIGetRenderQueryResult(RenderQuery, OutResult, bWait, GPUIndex);
	}

	FORCEINLINE uint32 GetViewportNextPresentGPUIndex(FRHIViewport* Viewport)
	{
		return GDynamicRHI->RHIGetViewportNextPresentGPUIndex(Viewport);
	}

	FORCEINLINE FTexture2DRHIRef GetViewportBackBuffer(FRHIViewport* Viewport)
	{
		return RHIGetViewportBackBuffer(Viewport);
	}
	
	FORCEINLINE void AdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport)
	{
		return RHIAdvanceFrameForGetViewportBackBuffer(Viewport);
	}
	
	FORCEINLINE void AcquireThreadOwnership()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_AcquireThreadOwnership_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIAcquireThreadOwnership();
	}
	
	FORCEINLINE void ReleaseThreadOwnership()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReleaseThreadOwnership_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIReleaseThreadOwnership();
	}
	
	FORCEINLINE void FlushResources()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_FlushResources_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIFlushResources();
	}
	
	FORCEINLINE uint32 GetGPUFrameCycles()
	{
		return RHIGetGPUFrameCycles(GetGPUMask().ToIndex());
	}
	
	FORCEINLINE FViewportRHIRef CreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		LLM_SCOPE(ELLMTag::RenderTargets);
		return RHICreateViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
	
	FORCEINLINE void ResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		LLM_SCOPE(ELLMTag::RenderTargets);
		RHIResizeViewport(Viewport, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
	
	FORCEINLINE void Tick(float DeltaTime)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/RHITick"));
		RHITick(DeltaTime);
	}
	
	FORCEINLINE void BlockUntilGPUIdle()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_BlockUntilGPUIdle_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIBlockUntilGPUIdle();
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsAndFlushGPU()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_SubmitCommandsAndFlushGPU_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHISubmitCommandsAndFlushGPU();
	}
	
	FORCEINLINE void SuspendRendering()
	{
		RHISuspendRendering();
	}
	
	FORCEINLINE void ResumeRendering()
	{
		RHIResumeRendering();
	}
	
	FORCEINLINE bool IsRenderingSuspended()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_IsRenderingSuspended_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		return GDynamicRHI->RHIIsRenderingSuspended();
	}
	
	UE_DEPRECATED(5.1, "No longer used: FCompression::UncompressMemory should be used instead")
	FORCEINLINE bool EnqueueDecompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int CompressedSize, void* ErrorCodeBuffer)
	{
		return false;
	}

	FORCEINLINE bool GetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
	{
		return RHIGetAvailableResolutions(Resolutions, bIgnoreRefreshRate);
	}
	
	FORCEINLINE void GetSupportedResolution(uint32& Width, uint32& Height)
	{
		RHIGetSupportedResolution(Width, Height);
	}
	
	FORCEINLINE void VirtualTextureSetFirstMipInMemory(FRHITexture2D* Texture, uint32 FirstMip)
	{
		GDynamicRHI->VirtualTextureSetFirstMipInMemory_RenderThread(*this, Texture, FirstMip);
	}
	
	FORCEINLINE void VirtualTextureSetFirstMipVisible(FRHITexture2D* Texture, uint32 FirstMip)
	{
		GDynamicRHI->VirtualTextureSetFirstMipVisible_RenderThread(*this, Texture, FirstMip);
	}

	FORCEINLINE void ExecuteCommandList(FRHICommandList* CmdList)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIExecuteCommandList(CmdList);
	}
	
	FORCEINLINE void* GetNativeDevice()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativeDevice_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIGetNativeDevice();
	}
	
	FORCEINLINE void* GetNativePhysicalDevice()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativePhysicalDevice_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIGetNativePhysicalDevice();
	}
	
	FORCEINLINE void* GetNativeGraphicsQueue()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativeGraphicsQueue_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIGetNativeGraphicsQueue();
	}
	
	FORCEINLINE void* GetNativeComputeQueue()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativeComputeQueue_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIGetNativeComputeQueue();
	}
	
	FORCEINLINE void* GetNativeInstance()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativeInstance_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);

		return GDynamicRHI->RHIGetNativeInstance();
	}
	
	FORCEINLINE void* GetNativeCommandBuffer()
	{
		return GDynamicRHI->RHIGetNativeCommandBuffer();
	}

	FORCEINLINE class IRHICommandContext* GetDefaultContext()
	{
		return RHIGetDefaultContext();
	}

	void UpdateTextureReference(FRHITextureReference* TextureRef, FRHITexture* NewTexture);

	FORCEINLINE void PollRenderQueryResults()
	{
		GDynamicRHI->RHIPollRenderQueryResults();
	}

	/**
	 * @param UpdateInfos - an array of update infos
	 * @param Num - number of update infos
	 * @param bNeedReleaseRefs - whether Release need to be called on RHI resources referenced by update infos
	 */
	void UpdateRHIResources(FRHIResourceUpdateInfo* UpdateInfos, int32 Num, bool bNeedReleaseRefs);

	//UE_DEPRECATED(5.1, "SubmitCommandsHint is deprecated. Consider calling ImmediateFlush(EImmediateFlushType::DispatchToRHIThread) instead.")
	FORCEINLINE_DEBUGGABLE void SubmitCommandsHint()
	{
		if (Bypass())
		{
			GetComputeContext().RHISubmitCommandsHint();
		}
		else
		{
			ALLOC_COMMAND(FRHICommandSubmitCommandsHint)();
		}

		ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}
};

// All command list members should be contained within FRHICommandListBase. The Immediate/Compute/regular types are just interfaces.
static_assert(sizeof(FRHICommandListImmediate) == sizeof(FRHICommandListBase), "FRHICommandListImmediate should not contain additional members.");
static_assert(sizeof(FRHIComputeCommandList  ) == sizeof(FRHICommandListBase), "FRHIComputeCommandList should not contain additional members.");
static_assert(sizeof(FRHICommandList         ) == sizeof(FRHICommandListBase), "FRHICommandList should not contain additional members.");

class FRHICommandListScopedFlushAndExecute
{
	FRHICommandListImmediate& RHICmdList;

public:
	FRHICommandListScopedFlushAndExecute(FRHICommandListImmediate& InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
		check(RHICmdList.IsTopOfPipe());
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		RHICmdList.bExecuting = true;
	}
	~FRHICommandListScopedFlushAndExecute()
	{
		RHICmdList.bExecuting = false;
	}
};

//
// Helper to activate a specific RHI pipeline within a block of renderer code.
// Allows command list recording code to switch between graphics / async compute etc.
// Restores the previous active pipeline when the scope is ended.
//
class FRHICommandListScopedPipeline
{
	FRHICommandListBase& RHICmdList;
	ERHIPipeline PreviousPipeline;

public:
	FRHICommandListScopedPipeline(FRHICommandListBase& RHICmdList, ERHIPipeline Pipeline)
		: RHICmdList(RHICmdList)
		, PreviousPipeline(RHICmdList.SwitchPipeline(Pipeline))
	{}

	~FRHICommandListScopedPipeline()
	{
		RHICmdList.SwitchPipeline(PreviousPipeline);
	}
};

struct FRHIScopedGPUMask
{
	FRHIComputeCommandList& RHICmdList;
	FRHIGPUMask PrevGPUMask;

	FORCEINLINE FRHIScopedGPUMask(FRHIComputeCommandList& InRHICmdList, FRHIGPUMask InGPUMask)
		: RHICmdList(InRHICmdList)
		, PrevGPUMask(InRHICmdList.GetGPUMask())
	{
		InRHICmdList.SetGPUMask(InGPUMask);
	}

	FORCEINLINE ~FRHIScopedGPUMask()
	{
		RHICmdList.SetGPUMask(PrevGPUMask);
	}

	FRHIScopedGPUMask(FRHIScopedGPUMask const&) = delete;
	FRHIScopedGPUMask(FRHIScopedGPUMask&&) = delete;
};

#if WITH_MGPU
	#define SCOPED_GPU_MASK(RHICmdList, GPUMask) FRHIScopedGPUMask PREPROCESSOR_JOIN(ScopedGPUMask, __LINE__){ RHICmdList, GPUMask }
#else
	#define SCOPED_GPU_MASK(RHICmdList, GPUMask)
#endif // WITH_MGPU

struct RHI_API FScopedUniformBufferStaticBindings
{
	FScopedUniformBufferStaticBindings(FRHIComputeCommandList& InRHICmdList, FUniformBufferStaticBindings UniformBuffers)
		: RHICmdList(InRHICmdList)
	{
#if VALIDATE_UNIFORM_BUFFER_STATIC_BINDINGS
		checkf(!bRecursionGuard, TEXT("Uniform buffer global binding scope has been called recursively!"));
		bRecursionGuard = true;
#endif

		RHICmdList.SetStaticUniformBuffers(UniformBuffers);
	}

	template <typename... TArgs>
	FScopedUniformBufferStaticBindings(FRHIComputeCommandList& InRHICmdList, TArgs... Args)
		: FScopedUniformBufferStaticBindings(InRHICmdList, FUniformBufferStaticBindings{ Args... })
	{}

	~FScopedUniformBufferStaticBindings()
	{
		RHICmdList.SetStaticUniformBuffers(FUniformBufferStaticBindings());

#if VALIDATE_UNIFORM_BUFFER_STATIC_BINDINGS
		bRecursionGuard = false;
#endif
	}

	FRHIComputeCommandList& RHICmdList;

#if VALIDATE_UNIFORM_BUFFER_STATIC_BINDINGS
	static bool bRecursionGuard;
#endif
};

#define SCOPED_UNIFORM_BUFFER_STATIC_BINDINGS(RHICmdList, UniformBuffers) FScopedUniformBufferStaticBindings PREPROCESSOR_JOIN(UniformBuffers, __LINE__){ RHICmdList, UniformBuffers }

UE_DEPRECATED(5.0, "Please rename to FScopedUniformBufferStaticBindings, or use the SCOPED_UNIFORM_BUFFER_STATIC_BINDINGS instead.")
typedef FScopedUniformBufferStaticBindings FScopedUniformBufferGlobalBindings;

// UE_DEPRECATED(5.0)
#define SCOPED_UNIFORM_BUFFER_GLOBAL_BINDINGS(RHICmdList, UniformBuffers) FScopedUniformBufferGlobalBindings PREPROCESSOR_JOIN(UniformBuffers, __LINE__){ RHICmdList, UniformBuffers }

// Helper to enable the use of graphics RHI command lists from within platform RHI implementations.
// Recorded commands are dispatched when the command list is destructed. Intended for use on the stack / in a scope block.
class RHI_API FRHICommandList_RecursiveHazardous : public FRHICommandList
{
public:
	FRHICommandList_RecursiveHazardous(IRHICommandContext* Context);
	~FRHICommandList_RecursiveHazardous();
};

// Helper class used internally by RHIs to make use of FRHICommandList_RecursiveHazardous safer.
// Access to the underlying context is exposed via RunOnContext() to ensure correct ordering of commands.
template <typename ContextType>
class TRHICommandList_RecursiveHazardous : public FRHICommandList_RecursiveHazardous
{
	template <typename LAMBDA>
	struct TRHILambdaCommand final : public FRHICommandBase
	{
		LAMBDA Lambda;

		TRHILambdaCommand(LAMBDA&& InLambda)
			: Lambda(Forward<LAMBDA>(InLambda))
		{}

		void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext&) override final
		{
			// RunOnContext always requires the lowest level (platform) context, not the validation RHI context.
			Lambda(static_cast<ContextType&>(CmdList.GetContext().GetLowestLevelContext()));
			Lambda.~LAMBDA();
		}
	};

public:
	TRHICommandList_RecursiveHazardous(ContextType* Context)
		: FRHICommandList_RecursiveHazardous(Context)
	{}

	template <typename LAMBDA>
	FORCEINLINE_DEBUGGABLE void RunOnContext(LAMBDA&& Lambda)
	{
		if (Bypass())
		{
			// RunOnContext always requires the lowest level (platform) context, not the validation RHI context.
			Lambda(static_cast<ContextType&>(GetContext().GetLowestLevelContext()));
		}
		else
		{
			ALLOC_COMMAND(TRHILambdaCommand<LAMBDA>)(Forward<LAMBDA>(Lambda));
		}
	}
};

// Helper to enable the use of compute RHI command lists from within platform RHI implementations.
// Recorded commands are dispatched when the command list is destructed. Intended for use on the stack / in a scope block.
class RHI_API FRHIComputeCommandList_RecursiveHazardous : public FRHIComputeCommandList
{
public:
	FRHIComputeCommandList_RecursiveHazardous(IRHIComputeContext* Context);
	~FRHIComputeCommandList_RecursiveHazardous();
};

// Helper class used internally by RHIs to make use of FRHIComputeCommandList_RecursiveHazardous safer.
// Access to the underlying context is exposed via RunOnContext() to ensure correct ordering of commands.
template <typename ContextType>
class TRHIComputeCommandList_RecursiveHazardous : public FRHIComputeCommandList_RecursiveHazardous
{
	template <typename LAMBDA>
	struct TRHILambdaCommand final : public FRHICommandBase
	{
		LAMBDA Lambda;

		TRHILambdaCommand(LAMBDA&& InLambda)
			: Lambda(Forward<LAMBDA>(InLambda))
		{}

		void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandListDebugContext&) override final
		{
			// RunOnContext always requires the lowest level (platform) context, not the validation RHI context.
			Lambda(static_cast<ContextType&>(CmdList.GetComputeContext().GetLowestLevelContext()));
			Lambda.~LAMBDA();
		}
	};

public:
	TRHIComputeCommandList_RecursiveHazardous(ContextType* Context)
		: FRHIComputeCommandList_RecursiveHazardous(Context)
	{}

	template <typename LAMBDA>
	FORCEINLINE_DEBUGGABLE void RunOnContext(LAMBDA&& Lambda)
	{
		if (Bypass())
		{
			// RunOnContext always requires the lowest level (platform) context, not the validation RHI context.
			Lambda(static_cast<ContextType&>(GetComputeContext().GetLowestLevelContext()));
		}
		else
		{
			ALLOC_COMMAND(TRHILambdaCommand<LAMBDA>)(Forward<LAMBDA>(Lambda));
		}
	}
};

class RHI_API FRHICommandListExecutor
{
public:
	enum
	{
		DefaultBypass = PLATFORM_RHITHREAD_DEFAULT_BYPASS
	};
	FRHICommandListExecutor()
		: bLatchedBypass(!!DefaultBypass)
		, bLatchedUseParallelAlgorithms(false)
	{
	}
	static inline FRHICommandListImmediate& GetImmediateCommandList();
	void LatchBypass();

	static void WaitOnRHIThreadFence(FGraphEventRef& Fence);

	FORCEINLINE_DEBUGGABLE bool Bypass()
	{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		return bLatchedBypass;
#else
		return !!DefaultBypass;
#endif
	}

	FORCEINLINE_DEBUGGABLE bool UseParallelAlgorithms()
	{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		return bLatchedUseParallelAlgorithms;
#else
		return  FApp::ShouldUseThreadingForPerformance() && !Bypass() && (GSupportsParallelRenderingTasksWithSeparateRHIThread || !IsRunningRHIInSeparateThread());
#endif
	}

	static inline void CheckNoOutstandingCmdLists();

	static bool IsRHIThreadActive();
	static bool IsRHIThreadCompletelyFlushed();

private:
	bool bLatchedBypass;
	bool bLatchedUseParallelAlgorithms;
	friend class FRHICommandListBase;
	FThreadSafeCounter UIDCounter;
#if DO_CHECK
	FThreadSafeCounter OutstandingCmdListCount;
#endif
	FRHICommandListImmediate CommandListImmediate;
};

extern RHI_API FRHICommandListExecutor GRHICommandList;

extern RHI_API FAutoConsoleTaskPriority CPrio_SceneRenderingTask;

inline void FRHICommandListExecutor::CheckNoOutstandingCmdLists()
{
	// If this assert fires, there is at least one unaccounted instance of FRHICommandListBase, aside from the immediate command list itself.
	// This may be a problem if, for example, attempting to delete RHI resources while an existing FRHICommandList may be refering to them in recorded commands.
	checkf(GRHICommandList.OutstandingCmdListCount.GetValue() == 1, TEXT("Expected only 1 outstanding RHI command list. Outstanding: %i"), GRHICommandList.OutstandingCmdListCount.GetValue());
}

/** Used to separate which command list is used for ray tracing operations. */
using FRHIRayTracingCommandList = FRHICommandListImmediate;

class FRenderTask
{
public:
	FORCEINLINE static ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_SceneRenderingTask.Get();
	}
};


FORCEINLINE_DEBUGGABLE FRHICommandListImmediate& FRHICommandListExecutor::GetImmediateCommandList()
{
	return GRHICommandList.CommandListImmediate;
}

FORCEINLINE FPixelShaderRHIRef RHICreatePixelShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreatePixelShader(Code, Hash);
}

FORCEINLINE FVertexShaderRHIRef RHICreateVertexShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexShader(Code, Hash);
}

FORCEINLINE FMeshShaderRHIRef RHICreateMeshShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateMeshShader(Code, Hash);
}

FORCEINLINE FAmplificationShaderRHIRef RHICreateAmplificationShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAmplificationShader(Code, Hash);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShader(Code, Hash);
}

FORCEINLINE FComputeShaderRHIRef RHICreateComputeShader(TArrayView<const uint8> Code, const FSHAHash& Hash)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeShader(Code, Hash);
}

FORCEINLINE FComputeFenceRHIRef RHICreateComputeFence(const FName& Name)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeFence(Name);
}

FORCEINLINE FGPUFenceRHIRef RHICreateGPUFence(const FName& Name)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGPUFence(Name);
}

FORCEINLINE FStagingBufferRHIRef RHICreateStagingBuffer()
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateStagingBuffer();
}

UE_DEPRECATED(5.0, "Use RHICreateBuffer() and RHILockBuffer() instead.")
FORCEINLINE FBufferRHIRef RHICreateAndLockIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	check(IsInRenderingThread());
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAndLockIndexBuffer(Stride, Size, InUsage, CreateInfo, OutDataBuffer);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FORCEINLINE FBufferRHIRef RHICreateBuffer(uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(Size, Usage, Stride, ResourceState, CreateInfo);
}

FORCEINLINE FBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags Usage, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(Size, Usage | EBufferUsageFlags::IndexBuffer, Stride, ResourceState, CreateInfo);
}

UE_DEPRECATED(5.1, "RHIAsyncCreateIndexBuffer is deprecated. Use FRHICommandList::CreateBuffer instead.")
FORCEINLINE FBufferRHIRef RHIAsyncCreateIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags Usage, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(Size, Usage, Stride, ResourceState, CreateInfo);
}

FORCEINLINE FBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	bool bHasInitialData = CreateInfo.BulkData != nullptr;
	EBufferUsageFlags Usage = InUsage | EBufferUsageFlags::IndexBuffer;
	ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, bHasInitialData);
	return RHICreateIndexBuffer(Stride, Size, Usage, ResourceState, CreateInfo);
}

UE_DEPRECATED(5.1, "RHIAsyncCreateIndexBuffer is deprecated. Use FRHICommandList::CreateBuffer instead.")
FORCEINLINE FBufferRHIRef RHIAsyncCreateIndexBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	bool bHasInitialData = CreateInfo.BulkData != nullptr;
	EBufferUsageFlags Usage = InUsage | EBufferUsageFlags::IndexBuffer;
	ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, bHasInitialData);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHIAsyncCreateIndexBuffer(Stride, Size, Usage, ResourceState, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.0, "Buffer locks have been unified. Use RHILockBuffer() instead.")
FORCEINLINE void* RHILockIndexBuffer(FRHIBuffer* IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().LockBuffer(IndexBuffer, Offset, Size, LockMode);
}

UE_DEPRECATED(5.0, "Buffer locks have been unified. Use RHIUnlockBuffer() instead.")
FORCEINLINE void RHIUnlockIndexBuffer(FRHIBuffer* IndexBuffer)
{
	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().UnlockBuffer(IndexBuffer);
}

FORCEINLINE void RHIUpdateUniformBuffer(FRHIUniformBuffer* UniformBufferRHI, const void* Contents)
{
	return FRHICommandListExecutor::GetImmediateCommandList().UpdateUniformBuffer(UniformBufferRHI, Contents);
}

UE_DEPRECATED(5.0, "Use RHICreateBuffer() and RHILockBuffer() instead.")
FORCEINLINE FBufferRHIRef RHICreateAndLockVertexBuffer(uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	check(IsInRenderingThread());
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAndLockVertexBuffer(Size, InUsage, CreateInfo, OutDataBuffer);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FORCEINLINE FBufferRHIRef RHICreateVertexBuffer(uint32 Size, EBufferUsageFlags Usage, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(Size, Usage | EBufferUsageFlags::VertexBuffer, 0, ResourceState, CreateInfo);
}

UE_DEPRECATED(5.1, "RHIAsyncCreateVertexBuffer is deprecated. Use FRHICommandList::CreateBuffer instead.")
FORCEINLINE FBufferRHIRef RHIAsyncCreateVertexBuffer(uint32 Size, EBufferUsageFlags Usage, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(Size, Usage, 0, ResourceState, CreateInfo);
}

FORCEINLINE FBufferRHIRef RHICreateVertexBuffer(uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	bool bHasInitialData = CreateInfo.BulkData != nullptr;
	EBufferUsageFlags Usage = InUsage | EBufferUsageFlags::VertexBuffer;
	ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, bHasInitialData);
	return RHICreateVertexBuffer(Size, Usage, ResourceState, CreateInfo);
}

UE_DEPRECATED(5.1, "RHIAsyncCreateVertexBuffer is deprecated. Use FRHICommandList::CreateBuffer instead.")
FORCEINLINE FBufferRHIRef RHIAsyncCreateVertexBuffer(uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	bool bHasInitialData = CreateInfo.BulkData != nullptr;
	EBufferUsageFlags Usage = InUsage | EBufferUsageFlags::VertexBuffer;
	ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, bHasInitialData);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHIAsyncCreateVertexBuffer(Size, Usage, ResourceState, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.0, "Buffer locks have been unified. Use RHILockBuffer() instead.")
FORCEINLINE void* RHILockVertexBuffer(FRHIBuffer* VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().LockBuffer(VertexBuffer, Offset, SizeRHI, LockMode);
}

UE_DEPRECATED(5.0, "Buffer locks have been unified. Use RHIUnlockBuffer() instead.")
FORCEINLINE void RHIUnlockVertexBuffer(FRHIBuffer* VertexBuffer)
{
	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().UnlockBuffer(VertexBuffer);
}

FORCEINLINE FBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags Usage, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(Size, Usage | EBufferUsageFlags::StructuredBuffer, Stride, ResourceState, CreateInfo);
}

FORCEINLINE FBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, EBufferUsageFlags InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	bool bHasInitialData = CreateInfo.BulkData != nullptr;
	EBufferUsageFlags Usage = InUsage | BUF_StructuredBuffer;
	ERHIAccess ResourceState = RHIGetDefaultResourceState(Usage, bHasInitialData);
	return RHICreateStructuredBuffer(Stride, Size, Usage, ResourceState, CreateInfo);
}

UE_DEPRECATED(5.0, "Buffer locks have been unified. Use RHILockBuffer() instead.")
FORCEINLINE void* RHILockStructuredBuffer(FRHIBuffer* StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().LockBuffer(StructuredBuffer, Offset, SizeRHI, LockMode);
}

UE_DEPRECATED(5.0, "Buffer locks have been unified. Use RHIUnlockBuffer() instead.")
FORCEINLINE void RHIUnlockStructuredBuffer(FRHIBuffer* StructuredBuffer)
{
	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().UnlockBuffer(StructuredBuffer);
}

FORCEINLINE void* RHILockBuffer(FRHIBuffer* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().LockBuffer(Buffer, Offset, SizeRHI, LockMode);
}

FORCEINLINE void RHIUnlockBuffer(FRHIBuffer* Buffer)
{
	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().UnlockBuffer(Buffer);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHIBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(Buffer, bUseUAVCounter, bAppendBuffer);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHITexture* Texture, uint32 MipLevel = 0, uint16 FirstArraySlice = 0, uint16 NumArraySlices = 0)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(Texture, MipLevel, FirstArraySlice, NumArraySlices);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHITexture* Texture, uint32 MipLevel, uint8 Format, uint16 FirstArraySlice = 0, uint16 NumArraySlices = 0)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(Texture, MipLevel, Format, FirstArraySlice, NumArraySlices);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FRHIBuffer* Buffer, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(Buffer, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHIBuffer* Buffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Buffer);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Buffer, Stride, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(const FShaderResourceViewInitializer& Initializer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Initializer);
}

FORCEINLINE void RHIUpdateRHIResources(FRHIResourceUpdateInfo* UpdateInfos, int32 Num, bool bNeedReleaseRefs)
{
	check(IsInRenderingThread());
	return FRHICommandListExecutor::GetImmediateCommandList().UpdateRHIResources(UpdateInfos, Num, bNeedReleaseRefs);
}

FORCEINLINE FTextureReferenceRHIRef RHICreateTextureReference()
{
	return new FRHITextureReference();
}

UE_DEPRECATED(5.0, "The LastRenderTime parameter will be removed in the future")
FORCEINLINE FTextureReferenceRHIRef RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return RHICreateTextureReference();
}

FORCEINLINE void RHIUpdateTextureReference(FRHITextureReference* TextureRef, FRHITexture* NewTexture)
{
	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().UpdateTextureReference(TextureRef, NewTexture);
}

FORCEINLINE FTextureRHIRef RHICreateTexture(const FRHITextureCreateDesc& CreateDesc)
{
	//check(IsInRenderingThread()); // @todo: texture type unification Some passes call this function on parallel rendering threads (e.g. FRHIGPUTextureReadback::EnqueueCopyInternal)
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	LLM_SCOPE(EnumHasAnyFlags(CreateDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable) ? ELLMTag::RenderTargets : ELLMTag::Textures);

	if (CreateDesc.InitialState == ERHIAccess::Unknown)
	{
		// Need to copy the incoming descriptor since we need to override the initial state.
		FRHITextureCreateDesc NewCreateDesc(CreateDesc);
		NewCreateDesc.SetInitialState(RHIGetDefaultResourceState(CreateDesc.Flags, CreateDesc.BulkData != nullptr));

		return GDynamicRHI->RHICreateTexture_RenderThread(RHICmdList, NewCreateDesc);
	}

	return GDynamicRHI->RHICreateTexture_RenderThread(RHICmdList, CreateDesc);
}

UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, ERHIAccess ResourceState, const FRHIResourceCreateInfo& CreateInfo)
{
	return RHICreateTexture(
		FRHITextureCreateDesc::Create2D(CreateInfo.DebugName)
			.SetExtent((int32)SizeX, (int32)SizeY)
			.SetFormat((EPixelFormat)Format)
			.SetNumMips((uint8)NumMips)
			.SetNumSamples((uint8)NumSamples)
			.SetFlags(Flags)
			.SetInitialState(ResourceState)
			.SetExtData(CreateInfo.ExtData)
			.SetBulkData(CreateInfo.BulkData)
			.SetGPUMask(CreateInfo.GPUMask)
			.SetClearValue(CreateInfo.ClearValueBinding)
	);
}

UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DRHIRef RHICreateTextureExternal2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, ERHIAccess ResourceState, const  FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags | ETextureCreateFlags::External, ResourceState, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

//UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips)
{
	LLM_SCOPE(EnumHasAnyFlags(Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable) ? ELLMTag::RenderTargets : ELLMTag::Textures);
	const ERHIAccess ResourceState = InResourceState == ERHIAccess::Unknown ? RHIGetDefaultResourceState((ETextureCreateFlags)Flags, InitialMipData != nullptr) : InResourceState;
	return GDynamicRHI->RHIAsyncCreateTexture2D(SizeX, SizeY, Format, NumMips, Flags, ResourceState, InitialMipData, NumInitialMips);
}

UE_DEPRECATED(5.1, "FRHITexture2DArray is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, ERHIAccess ResourceState, const FRHIResourceCreateInfo& CreateInfo)
{
	return RHICreateTexture(
		FRHITextureCreateDesc::Create2DArray(CreateInfo.DebugName)
			.SetExtent((int32)SizeX, (int32)SizeY)
			.SetArraySize((uint16)ArraySize)
			.SetFormat((EPixelFormat)Format)
			.SetNumMips((uint8)NumMips)
			.SetNumSamples((uint8)NumSamples)
			.SetFlags(Flags)
			.SetInitialState(ResourceState)
			.SetExtData(CreateInfo.ExtData)
			.SetBulkData(CreateInfo.BulkData)
			.SetGPUMask(CreateInfo.GPUMask)
			.SetClearValue(CreateInfo.ClearValueBinding)
	);
}

UE_DEPRECATED(5.1, "FRHITexture3D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess ResourceState, const FRHIResourceCreateInfo& CreateInfo)
{
	return RHICreateTexture(
		FRHITextureCreateDesc::Create3D(CreateInfo.DebugName)
			.SetExtent((int32)SizeX, (int32)SizeY)
			.SetDepth((uint16)SizeZ)
			.SetFormat((EPixelFormat)Format)
			.SetNumMips((uint8)NumMips)
			.SetFlags(Flags)
			.SetInitialState(ResourceState)
			.SetExtData(CreateInfo.ExtData)
			.SetBulkData(CreateInfo.BulkData)
			.SetGPUMask(CreateInfo.GPUMask)
			.SetClearValue(CreateInfo.ClearValueBinding)
	);
}

UE_DEPRECATED(5.1, "FRHITextureCube is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess ResourceState, const FRHIResourceCreateInfo& CreateInfo)
{
	return RHICreateTexture(
		FRHITextureCreateDesc::CreateCube(CreateInfo.DebugName)
			.SetExtent(Size)
			.SetFormat((EPixelFormat)Format)
			.SetNumMips((uint8)NumMips)
			.SetFlags(Flags)
			.SetInitialState(ResourceState)
			.SetExtData(CreateInfo.ExtData)
			.SetBulkData(CreateInfo.BulkData)
			.SetGPUMask(CreateInfo.GPUMask)
			.SetClearValue(CreateInfo.ClearValueBinding)
	);
}

UE_DEPRECATED(5.1, "FRHITextureCube is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess ResourceState, const FRHIResourceCreateInfo& CreateInfo)
{
	return RHICreateTexture(
		FRHITextureCreateDesc::CreateCubeArray(CreateInfo.DebugName)
			.SetExtent(Size)
			.SetArraySize((uint16)ArraySize)
			.SetFormat((EPixelFormat)Format)
			.SetNumMips((uint8)NumMips)
			.SetFlags(Flags)
			.SetInitialState(ResourceState)
			.SetExtData(CreateInfo.ExtData)
			.SetBulkData(CreateInfo.BulkData)
			.SetGPUMask(CreateInfo.GPUMask)
			.SetClearValue(CreateInfo.ClearValueBinding)
	);
}

UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, ERHIAccess::Unknown, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DRHIRef RHICreateTextureExternal2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTextureExternal2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, ERHIAccess::Unknown, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

//UE_DEPRECATED(5.1, "FRHITexture2D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, void** InitialMipData, uint32 NumInitialMips)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHIAsyncCreateTexture2D(SizeX, SizeY, Format, NumMips, Flags, ERHIAccess::Unknown, InitialMipData, NumInitialMips);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.1, "FRHITexture2DArray is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, NumSamples, Flags, ERHIAccess::Unknown, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.1, "FRHITexture3D is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, ERHIAccess::Unknown, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.1, "FRHITextureCube is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTextureCube(Size, Format, NumMips, Flags, ERHIAccess::Unknown, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UE_DEPRECATED(5.1, "FRHITextureCube is deprecated, please use RHICreateTexture(const FRHITextureCreateDesc&).")
FORCEINLINE FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, const FRHIResourceCreateInfo& CreateInfo)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RHICreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, ERHIAccess::Unknown, CreateInfo);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FORCEINLINE void RHICopySharedMips(FRHITexture* DestTexture, FRHITexture* SrcTexture)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CopySharedMips(DestTexture, SrcTexture);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHITexture* Texture, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHITexture* Texture, uint8 MipLevel, uint8 NumMipLevels, EPixelFormat Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture, MipLevel, NumMipLevels, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture, CreateInfo);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceViewWriteMask(FRHITexture2D* Texture2D)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceViewWriteMask(Texture2D);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceViewFMask(FRHITexture2D* Texture2D)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceViewFMask(Texture2D);
}

FORCEINLINE FTexture2DRHIRef RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	return FRHICommandListExecutor::GetImmediateCommandList().AsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

FORCEINLINE ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	return FRHICommandListExecutor::GetImmediateCommandList().FinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FORCEINLINE ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FORCEINLINE void* RHILockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bFlushRHIThread = true)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, bFlushRHIThread);
}

FORCEINLINE void RHIUnlockTexture2D(FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bFlushRHIThread = true)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTexture2D(Texture, MipIndex, bLockWithinMiptail, bFlushRHIThread);
}

FORCEINLINE void* RHILockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTexture2DArray(Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

FORCEINLINE void RHIUnlockTexture2DArray(FRHITexture2DArray* Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTexture2DArray(Texture, TextureIndex, MipIndex, bLockWithinMiptail);
}

FORCEINLINE void RHIUpdateTexture2D(FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
}

FORCEINLINE FUpdateTexture3DData RHIBeginUpdateTexture3D(FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	return FRHICommandListExecutor::GetImmediateCommandList().BeginUpdateTexture3D(Texture, MipIndex, UpdateRegion);
}

FORCEINLINE void RHIEndUpdateTexture3D(FUpdateTexture3DData& UpdateData)
{
	FRHICommandListExecutor::GetImmediateCommandList().EndUpdateTexture3D(UpdateData);
}

FORCEINLINE void RHIEndMultiUpdateTexture3D(TArray<FUpdateTexture3DData>& UpdateDataArray)
{
	FRHICommandListExecutor::GetImmediateCommandList().EndMultiUpdateTexture3D(UpdateDataArray);
}

FORCEINLINE void RHIUpdateTexture3D(FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
}

FORCEINLINE void* RHILockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

FORCEINLINE void RHIUnlockTextureCubeFace(FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
}

UE_DEPRECATED(5.0, "RHIAcquireTransientResource API is deprecated; use IRHITransientResourceAllocator instead.")
FORCEINLINE void RHIAcquireTransientResource(FRHITexture*) {}

UE_DEPRECATED(5.0, "RHIDiscardTransientResource API is deprecated; use IRHITransientResourceAllocator instead.")
FORCEINLINE void RHIDiscardTransientResource(FRHITexture*) {}

UE_DEPRECATED(5.0, "RHIAcquireTransientResource API is deprecated; use IRHITransientResourceAllocator instead.")
FORCEINLINE void RHIAcquireTransientResource(FRHIBuffer*)  {}

UE_DEPRECATED(5.0, "RHIDiscardTransientResource API is deprecated; use IRHITransientResourceAllocator instead.")
FORCEINLINE void RHIDiscardTransientResource(FRHIBuffer*)  {}

FORCEINLINE void RHIAcquireThreadOwnership()
{
	return FRHICommandListExecutor::GetImmediateCommandList().AcquireThreadOwnership();
}

FORCEINLINE void RHIReleaseThreadOwnership()
{
	return FRHICommandListExecutor::GetImmediateCommandList().ReleaseThreadOwnership();
}

FORCEINLINE void RHIFlushResources()
{
	return FRHICommandListExecutor::GetImmediateCommandList().FlushResources();
}

FORCEINLINE void RHIVirtualTextureSetFirstMipInMemory(FRHITexture2D* Texture, uint32 FirstMip)
{
	 FRHICommandListExecutor::GetImmediateCommandList().VirtualTextureSetFirstMipInMemory(Texture, FirstMip);
}

FORCEINLINE void RHIVirtualTextureSetFirstMipVisible(FRHITexture2D* Texture, uint32 FirstMip)
{
	 FRHICommandListExecutor::GetImmediateCommandList().VirtualTextureSetFirstMipVisible(Texture, FirstMip);
}

FORCEINLINE void RHIExecuteCommandList(FRHICommandList* CmdList)
{
	 FRHICommandListExecutor::GetImmediateCommandList().ExecuteCommandList(CmdList);
}

FORCEINLINE void* RHIGetNativeDevice()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeDevice();
}

FORCEINLINE void* RHIGetNativePhysicalDevice()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativePhysicalDevice();
}

FORCEINLINE void* RHIGetNativeGraphicsQueue()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeGraphicsQueue();
}

FORCEINLINE void* RHIGetNativeComputeQueue()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeComputeQueue();
}

FORCEINLINE void* RHIGetNativeInstance()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeInstance();
}

FORCEINLINE void* RHIGetNativeCommandBuffer()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeCommandBuffer();
}

FORCEINLINE FRHIShaderLibraryRef RHICreateShaderLibrary(EShaderPlatform Platform, FString const& FilePath, FString const& Name)
{
    return GDynamicRHI->RHICreateShaderLibrary(Platform, FilePath, Name);
}

FORCEINLINE void* RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, uint32 Offset, uint32 Size)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockStagingBuffer(StagingBuffer, nullptr, Offset, Size);
}

FORCEINLINE void* RHILockStagingBuffer(FRHIStagingBuffer* StagingBuffer, FRHIGPUFence* Fence, uint32 Offset, uint32 Size)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockStagingBuffer(StagingBuffer, Fence, Offset, Size);
}

FORCEINLINE void RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockStagingBuffer(StagingBuffer);
}

template <uint32 MaxNumUpdates>
struct TRHIResourceUpdateBatcher
{
	FRHIResourceUpdateInfo UpdateInfos[MaxNumUpdates];
	uint32 NumBatched;

	TRHIResourceUpdateBatcher()
		: NumBatched(0)
	{}

	~TRHIResourceUpdateBatcher()
	{
		Flush();
	}

	void Flush()
	{
		if (NumBatched > 0)
		{
			RHIUpdateRHIResources(UpdateInfos, NumBatched, true);
			NumBatched = 0;
		}
	}

	void QueueUpdateRequest(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
	{
		FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
		UpdateInfo.Type = FRHIResourceUpdateInfo::UT_Buffer;
		UpdateInfo.Buffer = { DestBuffer, SrcBuffer };
		DestBuffer->AddRef();
		if (SrcBuffer)
		{
			SrcBuffer->AddRef();
		}
	}

	void QueueUpdateRequest(FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry)
	{
		FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
		UpdateInfo.Type = FRHIResourceUpdateInfo::UT_RayTracingGeometry;
		UpdateInfo.RayTracingGeometry = { DestGeometry, SrcGeometry };
		DestGeometry->AddRef();
		if (SrcGeometry)
		{
			SrcGeometry->AddRef();
		}
	}

	void QueueUpdateRequest(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
	{
		FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
		UpdateInfo.Type = FRHIResourceUpdateInfo::UT_BufferFormatSRV;
		UpdateInfo.BufferSRV = { SRV, Buffer, Stride, Format };
		SRV->AddRef();
		if (Buffer)
		{
			Buffer->AddRef();
		}
	}

	void QueueUpdateRequest(FRHIShaderResourceView* SRV, FRHIBuffer* Buffer)
	{
		FRHIResourceUpdateInfo& UpdateInfo = GetNextUpdateInfo();
		UpdateInfo.Type = FRHIResourceUpdateInfo::UT_BufferSRV;
		UpdateInfo.BufferSRV = { SRV, Buffer };
		SRV->AddRef();
		if (Buffer)
		{
			Buffer->AddRef();
		}
	}

private:
	FRHIResourceUpdateInfo & GetNextUpdateInfo()
	{
		check(NumBatched <= MaxNumUpdates);
		if (NumBatched >= MaxNumUpdates)
		{
			Flush();
		}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6385) // Access is always in-bound due to the Flush above
#endif
		return UpdateInfos[NumBatched++];
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	}
};

#undef RHICOMMAND_CALLSTACK

#include "RHICommandList.inl" // IWYU pragma: export
