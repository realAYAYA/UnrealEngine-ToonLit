// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/SortedMap.h"
#include "Containers/StaticArray.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "MultiGPU.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "RenderGraphAllocator.h"
#include "RenderGraphDefinitions.h"
#include "RenderGraphEvent.h"
#include "RenderGraphParameter.h"
#include "RenderGraphResources.h"
#include "ShaderParameterMacros.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Templates/EnableIf.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

using FRDGTransitionQueue = TArray<const FRHITransition*, TInlineAllocator<8>>;

struct FRDGBarrierBatchBeginId
{
	FRDGBarrierBatchBeginId() = default;

	bool operator==(FRDGBarrierBatchBeginId Other) const
	{
		return Passes == Other.Passes && PipelinesAfter == Other.PipelinesAfter;
	}

	bool operator!=(FRDGBarrierBatchBeginId Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(FRDGBarrierBatchBeginId Id)
	{
		static_assert(sizeof(Id.Passes) == 4);
		uint32 Hash = *(const uint32*)Id.Passes.GetData();
		return (Hash << GetRHIPipelineCount()) | uint32(Id.PipelinesAfter);
	}

	FRDGPassHandlesByPipeline Passes;
	ERHIPipeline PipelinesAfter = ERHIPipeline::None;
};

/** Barrier location controls where the barrier is 'Ended' relative to the pass lambda being executed.
 *  Most barrier locations are done in the prologue prior to the executing lambda. But certain cases
 *  like an aliasing discard operation need to be done *after* the pass being invoked. Therefore, when
 *  adding a transition the user can specify where to place the barrier.
 */
enum class ERDGBarrierLocation : uint8
{
	/** The barrier occurs in the prologue of the pass (before execution). */
	Prologue,

	/** The barrier occurs in the epilogue of the pass (after execution). */
	Epilogue
};

struct FRDGTransitionInfo
{
	FRDGTransitionInfo() = default;

	ERHIAccess AccessBefore;
	ERHIAccess AccessAfter;
	uint16 Handle;
	ERDGViewableResourceType Type;
	EResourceTransitionFlags Flags;
	uint32 ArraySlice : 16;
	uint32 MipIndex   : 8;
	uint32 PlaneSlice : 2;
	uint32 bReservedCommit : 1;
};

struct FRDGBarrierBatchEndId
{
	FRDGBarrierBatchEndId() = default;
	FRDGBarrierBatchEndId(FRDGPassHandle InPassHandle, ERDGBarrierLocation InBarrierLocation)
		: PassHandle(InPassHandle)
		, BarrierLocation(InBarrierLocation)
	{}

	bool operator == (FRDGBarrierBatchEndId Other) const
	{
		return PassHandle == Other.PassHandle && BarrierLocation == Other.BarrierLocation;
	}

	bool operator != (FRDGBarrierBatchEndId Other) const
	{
		return *this == Other;
	}

	FRDGPassHandle PassHandle;
	ERDGBarrierLocation BarrierLocation = ERDGBarrierLocation::Epilogue;
};

class FRDGBarrierBatchBegin
{
public:
	RENDERCORE_API FRDGBarrierBatchBegin(ERHIPipeline PipelinesToBegin, ERHIPipeline PipelinesToEnd, const TCHAR* DebugName, FRDGPass* DebugPass);
	RENDERCORE_API FRDGBarrierBatchBegin(ERHIPipeline PipelinesToBegin, ERHIPipeline PipelinesToEnd, const TCHAR* DebugName, FRDGPassesByPipeline DebugPasses);

	RENDERCORE_API void AddTransition(FRDGViewableResource* Resource, FRDGTransitionInfo Info);

	RENDERCORE_API void AddAlias(FRDGViewableResource* Resource, const FRHITransientAliasingInfo& Info);

	void SetUseCrossPipelineFence()
	{
		TransitionFlags = ERHITransitionCreateFlags::None;
		bTransitionNeeded = true;
	}

	RENDERCORE_API void CreateTransition(TConstArrayView<FRHITransitionInfo> TransitionsRHI);

	RENDERCORE_API void Submit(FRHIComputeCommandList& RHICmdList, ERHIPipeline Pipeline);
	RENDERCORE_API void Submit(FRHIComputeCommandList& RHICmdList, ERHIPipeline Pipeline, FRDGTransitionQueue& TransitionsToBegin);

	void Reserve(uint32 TransitionCount)
	{
		Transitions.Reserve(TransitionCount);
		Aliases.Reserve(TransitionCount);
	}

	bool IsTransitionNeeded() const
	{
		return bTransitionNeeded;
	}

private:
	const FRHITransition* Transition = nullptr;
	TArray<FRDGTransitionInfo, FRDGArrayAllocator> Transitions;
	TArray<FRHITransientAliasingInfo, FRDGArrayAllocator> Aliases;
	ERHITransitionCreateFlags TransitionFlags = ERHITransitionCreateFlags::NoFence;
	ERHIPipeline PipelinesToBegin;
	ERHIPipeline PipelinesToEnd;
	TRHIPipelineArray<FRDGBarrierBatchEndId> BarriersToEnd;
	bool bTransitionNeeded = false;

#if RDG_ENABLE_DEBUG
	FRDGPassesByPipeline DebugPasses;
	TArray<FRDGViewableResource*, FRDGArrayAllocator> DebugTransitionResources;
	TArray<FRDGViewableResource*, FRDGArrayAllocator> DebugAliasingResources;
	const TCHAR* DebugName;
	ERHIPipeline DebugPipelinesToBegin;
	ERHIPipeline DebugPipelinesToEnd;
#endif

	friend class FRDGBarrierBatchEnd;
	friend class FRDGBarrierValidation;
	friend class FRDGBuilder;
};

using FRDGTransitionCreateQueue = TArray<FRDGBarrierBatchBegin*, FRDGArrayAllocator>;

class FRDGBarrierBatchEnd
{
public:
	FRDGBarrierBatchEnd(FRDGPass* InPass, ERDGBarrierLocation InBarrierLocation)
		: Pass(InPass)
		, BarrierLocation(InBarrierLocation)
	{}

	/** Inserts a dependency on a begin batch. A begin batch can be inserted into more than one end batch. */
	RENDERCORE_API void AddDependency(FRDGBarrierBatchBegin* BeginBatch);

	RENDERCORE_API void Submit(FRHIComputeCommandList& RHICmdList, ERHIPipeline Pipeline);

	void Reserve(uint32 TransitionBatchCount)
	{
		Dependencies.Reserve(TransitionBatchCount);
	}

	RENDERCORE_API FRDGBarrierBatchEndId GetId() const;

	RENDERCORE_API bool IsPairedWith(const FRDGBarrierBatchBegin& BeginBatch) const;

private:
	TArray<FRDGBarrierBatchBegin*, TInlineAllocator<4, FRDGArrayAllocator>> Dependencies;
	FRDGPass* Pass;
	ERDGBarrierLocation BarrierLocation;

	friend class FRDGBarrierBatchBegin;
	friend class FRDGBarrierValidation;
};

/** Base class of a render graph pass. */
class FRDGPass
{
public:
	RENDERCORE_API FRDGPass(FRDGEventName&& InName, FRDGParameterStruct InParameterStruct, ERDGPassFlags InFlags);
	FRDGPass(const FRDGPass&) = delete;
	virtual ~FRDGPass() = default;

#if RDG_ENABLE_DEBUG
	RENDERCORE_API const TCHAR* GetName() const;
#else
	FORCEINLINE const TCHAR* GetName() const
	{
		return Name.GetTCHAR();
	}
#endif

	FORCEINLINE const FRDGEventName& GetEventName() const
	{
		return Name;
	}

	FORCEINLINE ERDGPassFlags GetFlags() const
	{
		return Flags;
	}

	FORCEINLINE ERHIPipeline GetPipeline() const
	{
		return Pipeline;
	}

	FORCEINLINE FRDGParameterStruct GetParameters() const
	{
		return ParameterStruct;
	}

	FORCEINLINE FRDGPassHandle GetHandle() const
	{
		return Handle;
	}

	FORCEINLINE uint32 GetWorkload() const
	{
		return Workload;
	}

	bool IsParallelExecuteAllowed() const
	{
		return bParallelExecuteAllowed;
	}

	bool IsMergedRenderPassBegin() const
	{
		return !bSkipRenderPassBegin && bSkipRenderPassEnd;
	}

	bool IsMergedRenderPassEnd() const
	{
		return bSkipRenderPassBegin && !bSkipRenderPassEnd;
	}

	bool SkipRenderPassBegin() const
	{
		return bSkipRenderPassBegin;
	}

	bool SkipRenderPassEnd() const
	{
		return bSkipRenderPassEnd;
	}

	bool IsAsyncCompute() const
	{
		return Pipeline == ERHIPipeline::AsyncCompute;
	}

	bool IsAsyncComputeBegin() const
	{
		return bAsyncComputeBegin;
	}

	bool IsAsyncComputeEnd() const
	{
		return bAsyncComputeEnd;
	}

	bool IsGraphicsFork() const
	{
		return bGraphicsFork;
	}

	bool IsGraphicsJoin() const
	{
		return bGraphicsJoin;
	}

	bool IsCulled() const
	{
		return bCulled;
	}

	bool IsSentinel() const
	{
		return bSentinel;
	}

	/** Returns the graphics pass responsible for forking the async interval this pass is in. */
	FRDGPassHandle GetGraphicsForkPass() const
	{
		return GraphicsForkPass;
	}

	/** Returns the graphics pass responsible for joining the async interval this pass is in. */
	FRDGPassHandle GetGraphicsJoinPass() const
	{
		return GraphicsJoinPass;
	}

#if RDG_CPU_SCOPES
	FRDGCPUScopes GetCPUScopes() const
	{
		return CPUScopes;
	}
#endif

	FRDGGPUScopes GetGPUScopes() const
	{
		return GPUScopes;
	}

#if WITH_MGPU
	FRHIGPUMask GetGPUMask() const
	{
		return GPUMask;
	}
#endif

protected:
	RENDERCORE_API FRDGBarrierBatchBegin& GetPrologueBarriersToBegin(FRDGAllocator& Allocator, FRDGTransitionCreateQueue& CreateQueue);
	RENDERCORE_API FRDGBarrierBatchBegin& GetEpilogueBarriersToBeginForGraphics(FRDGAllocator& Allocator, FRDGTransitionCreateQueue& CreateQueue);
	RENDERCORE_API FRDGBarrierBatchBegin& GetEpilogueBarriersToBeginForAsyncCompute(FRDGAllocator& Allocator, FRDGTransitionCreateQueue& CreateQueue);
	RENDERCORE_API FRDGBarrierBatchBegin& GetEpilogueBarriersToBeginForAll(FRDGAllocator& Allocator, FRDGTransitionCreateQueue& CreateQueue);

	FRDGBarrierBatchBegin& GetEpilogueBarriersToBeginFor(FRDGAllocator& Allocator, FRDGTransitionCreateQueue& CreateQueue, ERHIPipeline PipelineForEnd)
	{
		switch (PipelineForEnd)
		{
		default: 
			checkNoEntry();
			// fall through

		case ERHIPipeline::Graphics:
			return GetEpilogueBarriersToBeginForGraphics(Allocator, CreateQueue);

		case ERHIPipeline::AsyncCompute:
			return GetEpilogueBarriersToBeginForAsyncCompute(Allocator, CreateQueue);

		case ERHIPipeline::All:
			return GetEpilogueBarriersToBeginForAll(Allocator, CreateQueue);
		}
	}

	RENDERCORE_API FRDGBarrierBatchEnd& GetPrologueBarriersToEnd(FRDGAllocator& Allocator);
	RENDERCORE_API FRDGBarrierBatchEnd& GetEpilogueBarriersToEnd(FRDGAllocator& Allocator);

	virtual void Execute(FRHIComputeCommandList& RHICmdList) {}

	// When r.RDG.Debug is enabled, this will include a full namespace path with event scopes included.
	IF_RDG_ENABLE_DEBUG(FString FullPathIfDebug);

	const FRDGEventName Name;
	const FRDGParameterStruct ParameterStruct;
	const ERDGPassFlags Flags;
	const ERHIPipeline Pipeline;
	FRDGPassHandle Handle;
	uint32 Workload = 1;

	union
	{
		struct
		{
			/** Whether the render pass begin / end should be skipped. */
			uint32 bSkipRenderPassBegin : 1;
			uint32 bSkipRenderPassEnd : 1;

			/** (AsyncCompute only) Whether this is the first / last async compute pass in an async interval. */
			uint32 bAsyncComputeBegin : 1;
			uint32 bAsyncComputeEnd : 1;

			/** (Graphics only) Whether this is a graphics fork / join pass. */
			uint32 bGraphicsFork : 1;
			uint32 bGraphicsJoin : 1;

			/** Whether the pass only writes to resources in its render pass. */
			uint32 bRenderPassOnlyWrites : 1;

			/** Whether the pass is allowed to execute in parallel. */
			uint32 bParallelExecuteAllowed : 1;

			/** Whether this pass is a sentinel (prologue / epilogue) pass. */
			uint32 bSentinel : 1;

			/** If set, dispatches to the RHI thread after executing this pass. */
			uint32 bDispatchAfterExecute : 1;

			/** If set, the pass should set its command list stat. */
			uint32 bSetCommandListStat : 1;
		};
		uint32 PackedBits1 = 0;
	};

	union
	{
		// Task-specific bits which are written in a task in parallel with reads from the other set.
		struct
		{
			/** If set, marks the begin / end of a span of passes executed in parallel in a task. */
			uint32 bParallelExecuteBegin : 1;
			uint32 bParallelExecuteEnd : 1;

			/** If set, marks that a pass is executing in parallel. */
			uint32 bParallelExecute : 1;

			/** Whether this pass does not contain parameters. */
			uint32 bEmptyParameters : 1;

			/** Whether this pass has external UAVs that are not tracked by RDG. */
			uint32 bHasExternalOutputs : 1;

			/** Whether this pass has been culled. */
			uint32 bCulled : 1;

			/** Whether this pass is used for external access transitions. */
			uint32 bExternalAccessPass : 1;
		};
		uint32 PacketBits2 = 0;
	};

	/** Handle of the latest cross-pipeline producer. */
	FRDGPassHandle CrossPipelineProducer;

	/** (AsyncCompute only) Graphics passes which are the fork / join for async compute interval this pass is in. */
	FRDGPassHandle GraphicsForkPass;
	FRDGPassHandle GraphicsJoinPass;

	/** The passes which are handling the epilogue / prologue barriers meant for this pass. */
	FRDGPassHandle PrologueBarrierPass;
	FRDGPassHandle EpilogueBarrierPass;

	/** Lists of producer passes and the full list of cross-pipeline consumer passes. */
	TArray<FRDGPassHandle, FRDGArrayAllocator> CrossPipelineConsumers;
	TArray<FRDGPass*, FRDGArrayAllocator> Producers;

	struct FTextureState
	{
		FTextureState() = default;

		FTextureState(FRDGTextureRef InTexture)
			: Texture(InTexture)
		{
			const uint32 SubresourceCount = Texture->GetSubresourceCount();
			State.SetNum(SubresourceCount);
			MergeState.SetNum(SubresourceCount);
		}

		FRDGTextureRef Texture = nullptr;
		FRDGTextureSubresourceState State;
		FRDGTextureSubresourceState MergeState;
		uint16 ReferenceCount = 0;
	};

	struct FBufferState
	{
		FBufferState() = default;

		FBufferState(FRDGBufferRef InBuffer)
			: Buffer(InBuffer)
		{}

		FRDGBufferRef Buffer = nullptr;
		FRDGSubresourceState State;
		FRDGSubresourceState* MergeState = nullptr;
		uint16 ReferenceCount = 0;
	};

	/** Maps textures / buffers to information on how they are used in the pass. */
	TArray<FTextureState, FRDGArrayAllocator> TextureStates;
	TArray<FBufferState, FRDGArrayAllocator> BufferStates;
	TArray<FRDGViewHandle, FRDGArrayAllocator> Views;
	TArray<FRDGUniformBufferHandle, FRDGArrayAllocator> UniformBuffers;

	struct FExternalAccessOp
	{
		FExternalAccessOp() = default;

		FExternalAccessOp(FRDGViewableResource* InResource, FRDGViewableResource::EAccessMode InMode)
			: Resource(InResource)
			, Mode(InMode)
		{}

		FRDGViewableResource* Resource;
		FRDGViewableResource::EAccessMode Mode;
	};

	TArray<FExternalAccessOp, FRDGArrayAllocator> ExternalAccessOps;

	/** Lists of pass parameters scheduled for begin during execution of this pass. */
	TArray<FRDGPass*, TInlineAllocator<1, FRDGArrayAllocator>> ResourcesToBegin;
	TArray<FRDGPass*, TInlineAllocator<1, FRDGArrayAllocator>> ResourcesToEnd;

	/** Split-barrier batches at various points of execution of the pass. */
	FRDGBarrierBatchBegin* PrologueBarriersToBegin = nullptr;
	FRDGBarrierBatchEnd PrologueBarriersToEnd;
	FRDGBarrierBatchBegin EpilogueBarriersToBeginForGraphics;
	FRDGBarrierBatchBegin* EpilogueBarriersToBeginForAsyncCompute = nullptr;
	FRDGBarrierBatchBegin* EpilogueBarriersToBeginForAll = nullptr;
	TArray<FRDGBarrierBatchBegin*, FRDGArrayAllocator> SharedEpilogueBarriersToBegin;
	FRDGBarrierBatchEnd* EpilogueBarriersToEnd = nullptr;

	EAsyncComputeBudget AsyncComputeBudget = EAsyncComputeBudget::EAll_4;

	uint16 ParallelPassSetIndex = 0;

#if WITH_MGPU
	FRHIGPUMask GPUMask;
#endif

	IF_RDG_CMDLIST_STATS(TStatId CommandListStat);

#if RDG_CPU_SCOPES
	FRDGCPUScopes CPUScopes;
	FRDGCPUScopeOpArrays CPUScopeOps;
#endif

	FRDGGPUScopes GPUScopes;
	FRDGGPUScopeOpArrays GPUScopeOpsPrologue;
	FRDGGPUScopeOpArrays GPUScopeOpsEpilogue;

#if RDG_GPU_DEBUG_SCOPES && RDG_ENABLE_TRACE
	const FRDGEventScope* TraceEventScope = nullptr;
#endif

#if RDG_ENABLE_TRACE
	TArray<FRDGTextureHandle, FRDGArrayAllocator> TraceTextures;
	TArray<FRDGBufferHandle, FRDGArrayAllocator> TraceBuffers;
#endif

	friend FRDGBuilder;
	friend FRDGPassRegistry;
	friend FRDGTrace;
	friend FRDGUserValidation;
};

/** Render graph pass with lambda execute function. */
template <typename ParameterStructType, typename ExecuteLambdaType>
class TRDGLambdaPass
	: public FRDGPass
{
	// Verify that the amount of stuff captured by the pass lambda is reasonable.
	static constexpr int32 kMaximumLambdaCaptureSize = 1024;
	static_assert(sizeof(ExecuteLambdaType) <= kMaximumLambdaCaptureSize, "The amount of data of captured for the pass looks abnormally high.");

	template <typename T>
	struct TLambdaTraits
		: TLambdaTraits<decltype(&T::operator())>
	{};
	template <typename ReturnType, typename ClassType, typename ArgType>
	struct TLambdaTraits<ReturnType(ClassType::*)(ArgType&) const>
	{
		using TRHICommandList = ArgType;
		using TRDGPass = void;
	};
	template <typename ReturnType, typename ClassType, typename ArgType>
	struct TLambdaTraits<ReturnType(ClassType::*)(ArgType&)>
	{
		using TRHICommandList = ArgType;
		using TRDGPass = void;
	};
	template <typename ReturnType, typename ClassType, typename ArgType1, typename ArgType2>
	struct TLambdaTraits<ReturnType(ClassType::*)(const ArgType1*, ArgType2&) const>
	{
		using TRHICommandList = ArgType2;
		using TRDGPass = ArgType1;
	};
	template <typename ReturnType, typename ClassType, typename ArgType1, typename ArgType2>
	struct TLambdaTraits<ReturnType(ClassType::*)(const ArgType1*, ArgType2&)>
	{
		using TRHICommandList = ArgType2;
		using TRDGPass = ArgType1;
	};
	using TRHICommandList = typename TLambdaTraits<ExecuteLambdaType>::TRHICommandList;
	using TRDGPass = typename TLambdaTraits<ExecuteLambdaType>::TRDGPass;

public:
	
	TRDGLambdaPass(
		FRDGEventName&& InName,
		const FShaderParametersMetadata* InParameterMetadata,
		const ParameterStructType* InParameterStruct,
		ERDGPassFlags InPassFlags,
		ExecuteLambdaType&& InExecuteLambda)
		: FRDGPass(MoveTemp(InName), FRDGParameterStruct(InParameterStruct, InParameterMetadata), InPassFlags)
		, ExecuteLambda(MoveTemp(InExecuteLambda))
#if RDG_ENABLE_DEBUG
		, DebugParameterStruct(InParameterStruct)
#endif
	{		
		bParallelExecuteAllowed = !std::is_same_v<TRHICommandList, FRHICommandListImmediate> && !EnumHasAnyFlags(InPassFlags, ERDGPassFlags::NeverParallel);
	}

private:
	template<class T>
	void ExecuteLambdaFunc(FRHIComputeCommandList& RHICmdList)
	{
		if constexpr (std::is_same_v<T, FRDGPass>)
		{
			ExecuteLambda(this, static_cast<TRHICommandList&>(RHICmdList));
		}
		else
		{
			ExecuteLambda(static_cast<TRHICommandList&>(RHICmdList));
		}
	}

	void Execute(FRHIComputeCommandList& RHICmdList) override
	{
#if !USE_NULL_RHI
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FRDGPass_Execute);
		RHICmdList.SetStaticUniformBuffers(ParameterStruct.GetStaticUniformBuffers());
		ExecuteLambdaFunc<TRDGPass>(static_cast<TRHICommandList&>(RHICmdList));
#else
		checkNoEntry();
#endif // !USE_NULL_RHI
	}

	ExecuteLambdaType ExecuteLambda;

	IF_RDG_ENABLE_DEBUG(const ParameterStructType* DebugParameterStruct);
};

template <typename ExecuteLambdaType>
class TRDGEmptyLambdaPass
	: public TRDGLambdaPass<FEmptyShaderParameters, ExecuteLambdaType>
{
public:
	TRDGEmptyLambdaPass(FRDGEventName&& InName, ERDGPassFlags InPassFlags, ExecuteLambdaType&& InExecuteLambda)
		: TRDGLambdaPass<FEmptyShaderParameters, ExecuteLambdaType>(MoveTemp(InName), FEmptyShaderParameters::FTypeInfo::GetStructMetadata(), &EmptyShaderParameters, InPassFlags, MoveTemp(InExecuteLambda))
	{}

private:
	FEmptyShaderParameters EmptyShaderParameters;
	friend class FRDGBuilder;
};

/** Render graph pass used for the prologue / epilogue passes. */
class FRDGSentinelPass final
	: public FRDGPass
{
public:
	FRDGSentinelPass(FRDGEventName&& Name, ERDGPassFlags InPassFlagsToAdd = ERDGPassFlags::None)
		: FRDGPass(MoveTemp(Name), FRDGParameterStruct(&EmptyShaderParameters, FEmptyShaderParameters::FTypeInfo::GetStructMetadata()), ERDGPassFlags::NeverCull | InPassFlagsToAdd) //-V1050
	{
		bSentinel = 1;
	}

private:
	FEmptyShaderParameters EmptyShaderParameters;
};

#include "RenderGraphParameters.inl" // IWYU pragma: export

class FRDGBuilder;
class FRDGPass;
class FRDGTrace;
class FRDGUserValidation;
class FShaderParametersMetadata;
