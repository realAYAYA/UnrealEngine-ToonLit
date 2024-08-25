// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphBuilder.h"
#include "RenderGraphPrivate.h"
#include "RenderGraphTrace.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderGraphResourcePool.h"
#include "VisualizeTexture.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Async/ParallelFor.h"

struct FParallelPassSet : public FRHICommandListImmediate::FQueuedCommandList
{
	FParallelPassSet() = default;

	TArray<FRDGPass*, FRDGArrayAllocator> Passes;
	IF_RHI_WANT_BREADCRUMB_EVENTS(FRDGBreadcrumbState* BreadcrumbStateBegin{});
	IF_RHI_WANT_BREADCRUMB_EVENTS(FRDGBreadcrumbState* BreadcrumbStateEnd{});
	bool bDispatchAfterExecute = false;
	bool bParallelTranslate = false;
};

inline void BeginUAVOverlap(const FRDGPass* Pass, FRHIComputeCommandList& RHICmdList)
{
#if ENABLE_RHI_VALIDATION
	if (GRHIValidationEnabled)
	{
		RHICmdList.BeginUAVOverlap();
	}
#endif
}

inline void EndUAVOverlap(const FRDGPass* Pass, FRHIComputeCommandList& RHICmdList)
{
#if ENABLE_RHI_VALIDATION
	if (GRHIValidationEnabled)
	{
		RHICmdList.EndUAVOverlap();
	}
#endif
}

inline ERHIAccess MakeValidAccess(ERHIAccess AccessOld, ERHIAccess AccessNew)
{
	const ERHIAccess AccessUnion = AccessOld | AccessNew;
	const ERHIAccess NonMergeableAccessMask = ~GRHIMergeableAccessMask;

	// Return the union of new and old if they are okay to merge.
	if (!EnumHasAnyFlags(AccessUnion, NonMergeableAccessMask))
	{
		return IsWritableAccess(AccessUnion) ? (AccessUnion & ~ERHIAccess::ReadOnlyExclusiveMask) : AccessUnion;
	}

	// Keep the old one if it can't be merged.
	if (EnumHasAnyFlags(AccessOld, NonMergeableAccessMask))
	{
		return AccessOld;
	}

	// Replace with the new one if it can't be merged.
	return AccessNew;
}

inline void GetPassAccess(ERDGPassFlags PassFlags, ERHIAccess& SRVAccess, ERHIAccess& UAVAccess)
{
	SRVAccess = ERHIAccess::Unknown;
	UAVAccess = ERHIAccess::Unknown;

	if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::Raster))
	{
		SRVAccess |= ERHIAccess::SRVGraphics;
		UAVAccess |= ERHIAccess::UAVGraphics;
	}

	if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::AsyncCompute | ERDGPassFlags::Compute))
	{
		SRVAccess |= ERHIAccess::SRVCompute;
		UAVAccess |= ERHIAccess::UAVCompute;
	}

	if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::Copy))
	{
		SRVAccess |= ERHIAccess::CopySrc;
	}
}

enum class ERDGTextureAccessFlags
{
	None = 0,

	// Access is within the fixed-function render pass.
	RenderTarget = 1 << 0
};
ENUM_CLASS_FLAGS(ERDGTextureAccessFlags);

/** Enumerates all texture accesses and provides the access and subresource range info. This results in
 *  multiple invocations of the same resource, but with different access / subresource range.
 */
template <typename TAccessFunction>
void EnumerateTextureAccess(FRDGParameterStruct PassParameters, ERDGPassFlags PassFlags, TAccessFunction AccessFunction)
{
	const ERDGTextureAccessFlags NoneFlags = ERDGTextureAccessFlags::None;

	ERHIAccess SRVAccess, UAVAccess;
	GetPassAccess(PassFlags, SRVAccess, UAVAccess);

	PassParameters.EnumerateTextures([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RDG_TEXTURE:
			if (FRDGTextureRef Texture = Parameter.GetAsTexture())
			{
				AccessFunction(nullptr, Texture, SRVAccess, NoneFlags, Texture->GetSubresourceRangeSRV());
			}
		break;
		case UBMT_RDG_TEXTURE_ACCESS:
		{
			if (FRDGTextureAccess TextureAccess = Parameter.GetAsTextureAccess())
			{
				AccessFunction(nullptr, TextureAccess.GetTexture(), TextureAccess.GetAccess(), NoneFlags, TextureAccess->GetSubresourceRange());
			}
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
		{
			const FRDGTextureAccessArray& TextureAccessArray = Parameter.GetAsTextureAccessArray();

			for (FRDGTextureAccess TextureAccess : TextureAccessArray)
			{
				AccessFunction(nullptr, TextureAccess.GetTexture(), TextureAccess.GetAccess(), NoneFlags, TextureAccess->GetSubresourceRange());
			}
		}
		break;
		case UBMT_RDG_TEXTURE_SRV:
			if (FRDGTextureSRVRef SRV = Parameter.GetAsTextureSRV())
			{
				AccessFunction(SRV, SRV->GetParent(), SRVAccess, NoneFlags, SRV->GetSubresourceRange());
			}
		break;
		case UBMT_RDG_TEXTURE_UAV:
			if (FRDGTextureUAVRef UAV = Parameter.GetAsTextureUAV())
			{
				AccessFunction(UAV, UAV->GetParent(), UAVAccess, NoneFlags, UAV->GetSubresourceRange());
			}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			const ERDGTextureAccessFlags RenderTargetAccess = ERDGTextureAccessFlags::RenderTarget;

			const ERHIAccess RTVAccess = ERHIAccess::RTV;

			const FRenderTargetBindingSlots& RenderTargets = Parameter.GetAsRenderTargetBindingSlots();

			RenderTargets.Enumerate([&](FRenderTargetBinding RenderTarget)
			{
				FRDGTextureRef Texture = RenderTarget.GetTexture();
				FRDGTextureRef ResolveTexture = RenderTarget.GetResolveTexture();

				FRDGTextureSubresourceRange Range(Texture->GetSubresourceRange());
				Range.MipIndex = RenderTarget.GetMipIndex();
				Range.NumMips = 1;

				if (RenderTarget.GetArraySlice() != -1)
				{
					Range.ArraySlice = RenderTarget.GetArraySlice();
					Range.NumArraySlices = 1;
				}

				AccessFunction(nullptr, Texture, RTVAccess, RenderTargetAccess, Range);

				if (ResolveTexture && ResolveTexture != Texture)
				{
					// Resolve targets must use the RTV|ResolveDst flag combination when the resolve is performed through the render
					// pass. The ResolveDst flag must be used alone only when the resolve is performed using RHICopyToResolveTarget.
					AccessFunction(nullptr, ResolveTexture, ERHIAccess::RTV | ERHIAccess::ResolveDst, RenderTargetAccess, Range);
				}
			});

			const FDepthStencilBinding& DepthStencil = RenderTargets.DepthStencil;

			if (FRDGTextureRef Texture = DepthStencil.GetTexture())
			{
				DepthStencil.GetDepthStencilAccess().EnumerateSubresources([&](ERHIAccess NewAccess, uint32 PlaneSlice)
				{
					FRDGTextureSubresourceRange Range = Texture->GetSubresourceRange();

					// Adjust the range to use a single plane slice if not using of them all.
					if (PlaneSlice != FRHITransitionInfo::kAllSubresources)
					{
						Range.PlaneSlice = PlaneSlice;
						Range.NumPlaneSlices = 1;
					}

					AccessFunction(nullptr, Texture, NewAccess, RenderTargetAccess, Range);
				});
			}

			if (FRDGTextureRef Texture = RenderTargets.ShadingRateTexture)
			{
				AccessFunction(nullptr, Texture, ERHIAccess::ShadingRateSource, RenderTargetAccess, Texture->GetSubresourceRangeSRV());
			}
		}
		break;
		}
	});
}

/** Enumerates all buffer accesses and provides the access info. */
template <typename TAccessFunction>
void EnumerateBufferAccess(FRDGParameterStruct PassParameters, ERDGPassFlags PassFlags, TAccessFunction AccessFunction)
{
	ERHIAccess SRVAccess, UAVAccess;
	GetPassAccess(PassFlags, SRVAccess, UAVAccess);

	PassParameters.EnumerateBuffers([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RDG_BUFFER_ACCESS:
			if (FRDGBufferAccess BufferAccess = Parameter.GetAsBufferAccess())
			{
				AccessFunction(nullptr, BufferAccess.GetBuffer(), BufferAccess.GetAccess());
			}
		break;
		case UBMT_RDG_BUFFER_ACCESS_ARRAY:
		{
			const FRDGBufferAccessArray& BufferAccessArray = Parameter.GetAsBufferAccessArray();

			for (FRDGBufferAccess BufferAccess : BufferAccessArray)
			{
				AccessFunction(nullptr, BufferAccess.GetBuffer(), BufferAccess.GetAccess());
			}
		}
		break;
		case UBMT_RDG_BUFFER_SRV:
			if (FRDGBufferSRVRef SRV = Parameter.GetAsBufferSRV())
			{
				FRDGBufferRef Buffer = SRV->GetParent();
				ERHIAccess BufferAccess = SRVAccess;

				if (EnumHasAnyFlags(Buffer->Desc.Usage, BUF_AccelerationStructure))
				{
					BufferAccess = ERHIAccess::BVHRead | ERHIAccess::SRVMask;
				}

				AccessFunction(SRV, Buffer, BufferAccess);
			}
		break;
		case UBMT_RDG_BUFFER_UAV:
			if (FRDGBufferUAVRef UAV = Parameter.GetAsBufferUAV())
			{
				AccessFunction(UAV, UAV->GetParent(), UAVAccess);
			}
		break;
		}
	});
}

inline FRDGViewHandle GetHandleIfNoUAVBarrier(FRDGViewRef Resource)
{
	if (Resource && (Resource->Type == ERDGViewType::BufferUAV || Resource->Type == ERDGViewType::TextureUAV))
	{
		if (EnumHasAnyFlags(static_cast<FRDGUnorderedAccessViewRef>(Resource)->Flags, ERDGUnorderedAccessViewFlags::SkipBarrier))
		{
			return Resource->GetHandle();
		}
	}
	return FRDGViewHandle::Null;
}

inline EResourceTransitionFlags GetTextureViewTransitionFlags(FRDGViewRef Resource, FRDGTextureRef Texture)
{
	if (Resource)
	{
		switch (Resource->Type)
		{
		case ERDGViewType::TextureUAV:
		{
			FRDGTextureUAVRef UAV = static_cast<FRDGTextureUAVRef>(Resource);
			if (UAV->Desc.MetaData != ERDGTextureMetaDataAccess::None)
			{
				return EResourceTransitionFlags::MaintainCompression;
			}
		}
		break;
		case ERDGViewType::TextureSRV:
		{
			FRDGTextureSRVRef SRV = static_cast<FRDGTextureSRVRef>(Resource);
			if (SRV->Desc.MetaData != ERDGTextureMetaDataAccess::None)
			{
				return EResourceTransitionFlags::MaintainCompression;
			}
		}
		break;
		}
	}
	else
	{
		if (EnumHasAnyFlags(Texture->Flags, ERDGTextureFlags::MaintainCompression))
		{
			return EResourceTransitionFlags::MaintainCompression;
		}
	}
	return EResourceTransitionFlags::None;
}

void FRDGBuilder::SetFlushResourcesRHI()
{
	if (GRHINeedsExtraDeletionLatency || !GRHICommandList.Bypass())
	{
		checkf(!bFlushResourcesRHI, TEXT("SetFlushRHIResources has been already been called. It may only be called once."));
		bFlushResourcesRHI = true;

		if (IsImmediateMode())
		{
			BeginFlushResourcesRHI();
			EndFlushResourcesRHI();
		}
	}
}

void FRDGBuilder::BeginFlushResourcesRHI()
{
	if (!bFlushResourcesRHI)
	{
		return;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(STAT_RDG_FlushResourcesRHI);
	SCOPED_NAMED_EVENT(BeginFlushResourcesRHI, FColor::Emerald);
	RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
}

void FRDGBuilder::EndFlushResourcesRHI()
{
	if (!bFlushResourcesRHI)
	{
		return;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(STAT_RDG_FlushResourcesRHI);
	CSV_SCOPED_SET_WAIT_STAT(FlushResourcesRHI);
	SCOPED_NAMED_EVENT(EndFlushResourcesRHI, FColor::Emerald);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	PipelineStateCache::FlushResources();
}

void FRDGBuilder::TickPoolElements()
{
	GRenderGraphResourcePool.TickPoolElements();

#if RDG_ENABLE_DEBUG
	if (GRDGTransitionLog > 0)
	{
		--GRDGTransitionLog;
	}
#endif

#if RDG_STATS
	CSV_CUSTOM_STAT(RDGCount, Passes, GRDGStatPassCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RDGCount, Buffers, GRDGStatBufferCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RDGCount, Textures, GRDGStatTextureCount, ECsvCustomStatOp::Set);

	TRACE_COUNTER_SET(COUNTER_RDG_PassCount, GRDGStatPassCount);
	TRACE_COUNTER_SET(COUNTER_RDG_PassCullCount, GRDGStatPassCullCount);
	TRACE_COUNTER_SET(COUNTER_RDG_RenderPassMergeCount, GRDGStatRenderPassMergeCount);
	TRACE_COUNTER_SET(COUNTER_RDG_PassDependencyCount, GRDGStatPassDependencyCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TextureCount, GRDGStatTextureCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TextureReferenceCount, GRDGStatTextureReferenceCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TextureReferenceAverage, (float)(GRDGStatTextureReferenceCount / FMath::Max((float)GRDGStatTextureCount, 1.0f)));
	TRACE_COUNTER_SET(COUNTER_RDG_BufferCount, GRDGStatBufferCount);
	TRACE_COUNTER_SET(COUNTER_RDG_BufferReferenceCount, GRDGStatBufferReferenceCount);
	TRACE_COUNTER_SET(COUNTER_RDG_BufferReferenceAverage, (float)(GRDGStatBufferReferenceCount / FMath::Max((float)GRDGStatBufferCount, 1.0f)));
	TRACE_COUNTER_SET(COUNTER_RDG_ViewCount, GRDGStatViewCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TransientTextureCount, GRDGStatTransientTextureCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TransientBufferCount, GRDGStatTransientBufferCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TransitionCount, GRDGStatTransitionCount);
	TRACE_COUNTER_SET(COUNTER_RDG_AliasingCount, GRDGStatAliasingCount);
	TRACE_COUNTER_SET(COUNTER_RDG_TransitionBatchCount, GRDGStatTransitionBatchCount);
	TRACE_COUNTER_SET(COUNTER_RDG_MemoryWatermark, int64(GRDGStatMemoryWatermark));

	SET_DWORD_STAT(STAT_RDG_PassCount, GRDGStatPassCount);
	SET_DWORD_STAT(STAT_RDG_PassCullCount, GRDGStatPassCullCount);
	SET_DWORD_STAT(STAT_RDG_RenderPassMergeCount, GRDGStatRenderPassMergeCount);
	SET_DWORD_STAT(STAT_RDG_PassDependencyCount, GRDGStatPassDependencyCount);
	SET_DWORD_STAT(STAT_RDG_TextureCount, GRDGStatTextureCount);
	SET_DWORD_STAT(STAT_RDG_TextureReferenceCount, GRDGStatTextureReferenceCount);
	SET_FLOAT_STAT(STAT_RDG_TextureReferenceAverage, (float)(GRDGStatTextureReferenceCount / FMath::Max((float)GRDGStatTextureCount, 1.0f)));
	SET_DWORD_STAT(STAT_RDG_BufferCount, GRDGStatBufferCount);
	SET_DWORD_STAT(STAT_RDG_BufferReferenceCount, GRDGStatBufferReferenceCount);
	SET_FLOAT_STAT(STAT_RDG_BufferReferenceAverage, (float)(GRDGStatBufferReferenceCount / FMath::Max((float)GRDGStatBufferCount, 1.0f)));
	SET_DWORD_STAT(STAT_RDG_ViewCount, GRDGStatViewCount);
	SET_DWORD_STAT(STAT_RDG_TransientTextureCount, GRDGStatTransientTextureCount);
	SET_DWORD_STAT(STAT_RDG_TransientBufferCount, GRDGStatTransientBufferCount);
	SET_DWORD_STAT(STAT_RDG_TransitionCount, GRDGStatTransitionCount);
	SET_DWORD_STAT(STAT_RDG_AliasingCount, GRDGStatAliasingCount);
	SET_DWORD_STAT(STAT_RDG_TransitionBatchCount, GRDGStatTransitionBatchCount);
	SET_MEMORY_STAT(STAT_RDG_MemoryWatermark, int64(GRDGStatMemoryWatermark));

	GRDGStatPassCount = 0;
	GRDGStatPassCullCount = 0;
	GRDGStatRenderPassMergeCount = 0;
	GRDGStatPassDependencyCount = 0;
	GRDGStatTextureCount = 0;
	GRDGStatTextureReferenceCount = 0;
	GRDGStatBufferCount = 0;
	GRDGStatBufferReferenceCount = 0;
	GRDGStatViewCount = 0;
	GRDGStatTransientTextureCount = 0;
	GRDGStatTransientBufferCount = 0;
	GRDGStatTransitionCount = 0;
	GRDGStatAliasingCount = 0;
	GRDGStatTransitionBatchCount = 0;
	GRDGStatMemoryWatermark = 0;
#endif
}

bool FRDGBuilder::IsImmediateMode()
{
	return ::IsImmediateMode();
}

ERDGPassFlags FRDGBuilder::OverridePassFlags(const TCHAR* PassName, ERDGPassFlags PassFlags)
{
	const bool bDebugAllowedForPass =
#if RDG_ENABLE_DEBUG
		IsDebugAllowedForPass(PassName);
#else
		true;
#endif

	if (IsAsyncComputeSupported())
	{
		if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::Compute) && GRDGAsyncCompute == RDG_ASYNC_COMPUTE_FORCE_ENABLED)
		{
			PassFlags &= ~ERDGPassFlags::Compute;
			PassFlags |= ERDGPassFlags::AsyncCompute;
		}
	}
	else
	{
		if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::AsyncCompute))
		{
			PassFlags &= ~ERDGPassFlags::AsyncCompute;
			PassFlags |= ERDGPassFlags::Compute;
		}
	}

	return PassFlags;
}

bool FRDGBuilder::IsTransient(FRDGBufferRef Buffer) const
{
	if (!bSupportsTransientBuffers || Buffer->bQueuedForUpload)
	{
		return false;
	}

	if (!IsTransientInternal(Buffer, EnumHasAnyFlags(Buffer->Desc.Usage, BUF_FastVRAM)))
	{
		return false;
	}

	if (!GRDGTransientIndirectArgBuffers && EnumHasAnyFlags(Buffer->Desc.Usage, BUF_DrawIndirect))
	{
		return false;
	}

	return EnumHasAnyFlags(Buffer->Desc.Usage, BUF_UnorderedAccess);
}

bool FRDGBuilder::IsTransient(FRDGTextureRef Texture) const
{
	if (!bSupportsTransientTextures)
	{
		return false;
	}

	if (EnumHasAnyFlags(Texture->Desc.Flags, ETextureCreateFlags::Shared))
	{
		return false;
	}

	return IsTransientInternal(Texture, EnumHasAnyFlags(Texture->Desc.Flags, ETextureCreateFlags::FastVRAM));
}

bool FRDGBuilder::IsTransientInternal(FRDGViewableResource* Resource, bool bFastVRAM) const
{
	// FastVRAM resources are always transient regardless of extraction or other hints, since they are performance critical.
	if (!bFastVRAM || !FPlatformMemory::SupportsFastVRAMMemory())
	{
		if (GRDGTransientAllocator == 2)
		{
			return false;
		}
	
		if (Resource->bForceNonTransient)
		{
			return false;
		}

		if (Resource->bExtracted)
		{
			if (GRDGTransientExtractedResources == 0)
			{
				return false;
			}

			if (GRDGTransientExtractedResources == 1 && Resource->TransientExtractionHint == FRDGViewableResource::ETransientExtractionHint::Disable)
			{
				return false;
			}
		}
	}

#if RDG_ENABLE_DEBUG
	if (GRDGDebugDisableTransientResources != 0)
	{
		const bool bDebugAllowed = IsDebugAllowedForResource(Resource->Name);

		if (GRDGDebugDisableTransientResources == 2 && Resource->Type == ERDGViewableResourceType::Buffer && bDebugAllowed)
		{
			return false;
		}

		if (GRDGDebugDisableTransientResources == 3 && Resource->Type == ERDGViewableResourceType::Texture && bDebugAllowed)
		{
			return false;
		}
	}
#endif

	return true;
}

FRDGBuilder::FRDGBuilder(FRHICommandListImmediate& InRHICmdList, FRDGEventName InName, ERDGBuilderFlags InFlags)
	: RootAllocatorScope(Allocators.Root)
	, RHICmdList(InRHICmdList)
	, Blackboard(Allocators.Root)
	, BuilderName(InName)
	, TransientResourceAllocator(GRDGTransientAllocator != 0 && !::IsImmediateMode() ? GRDGTransientResourceAllocator.Get() : nullptr)
	, ExtendResourceLifetimeScope(RHICmdList)
#if RDG_ENABLE_DEBUG
	, UserValidation(Allocators.Root, ParallelExecute.bEnabled)
	, BarrierValidation(&Passes, BuilderName)
#endif
{
	ProloguePass = SetupEmptyPass(Passes.Allocate<FRDGSentinelPass>(Allocators.Root, RDG_EVENT_NAME("Graph Prologue (Graphics)")));

	ParallelExecute.bEnabled = ::IsParallelExecuteEnabled() && EnumHasAnyFlags(InFlags, ERDGBuilderFlags::AllowParallelExecute);
	ParallelSetup.bEnabled   = ::IsParallelSetupEnabled()   && EnumHasAnyFlags(InFlags, ERDGBuilderFlags::AllowParallelExecute);

	if (TransientResourceAllocator)
	{
		bSupportsTransientTextures = TransientResourceAllocator->SupportsResourceType(ERHITransientResourceType::Texture);
		bSupportsTransientBuffers  = TransientResourceAllocator->SupportsResourceType(ERHITransientResourceType::Buffer);
	}

#if RDG_EVENTS != RDG_EVENTS_NONE
	// This is polled once as a workaround for a race condition since the underlying global is not always changed on the render thread.
	GRDGEmitDrawEvents_RenderThread = GetEmitDrawEvents();
#endif

#if RHI_WANT_BREADCRUMB_EVENTS
	if (ParallelExecute.bEnabled)
	{
		BreadcrumbState = FRDGBreadcrumbState::Create(Allocators.Root);
	}
#endif

#if RDG_DUMP_RESOURCES
	DumpNewGraphBuilder();
#endif
}

UE::Tasks::FTask FRDGBuilder::FAsyncDeleter::LastTask;

FRDGBuilder::FAsyncDeleter::~FAsyncDeleter()
{
	if (Function)
	{
		// Launch the task with a prerequisite on any previously launched RDG async delete task.
		LastTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Function = MoveTemp(Function)]() mutable {}, LastTask);
	}
}

void FRDGBuilder::WaitForAsyncDeleteTask()
{
	FAsyncDeleter::LastTask.Wait();
}

FRDGBuilder::~FRDGBuilder()
{
	if (ParallelExecute.bEnabled && GRDGParallelDestruction > 0)
	{
		AsyncDeleter.Function = [
			Allocators				= MoveTemp(Allocators),
			Passes					= MoveTemp(Passes),
			Textures				= MoveTemp(Textures),
			Buffers					= MoveTemp(Buffers),
			Views					= MoveTemp(Views),
			UniformBuffers			= MoveTemp(UniformBuffers),
			Blackboard				= MoveTemp(Blackboard),
			ActivePooledTextures	= MoveTemp(ActivePooledTextures),
			ActivePooledBuffers		= MoveTemp(ActivePooledBuffers),
			UploadedBuffers			= MoveTemp(UploadedBuffers)
		] () mutable {};
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const TRefCountPtr<FRDGPooledBuffer>& FRDGBuilder::ConvertToExternalBuffer(FRDGBufferRef Buffer)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateConvertToExternalResource(Buffer));
	if (!Buffer->bExternal)
	{
		Buffer->bExternal = 1;
		if (!Buffer->ResourceRHI)
		{
			SetPooledBufferRHI(Buffer, AllocatePooledBufferRHI(RHICmdList, Buffer));
		}
		ExternalBuffers.FindOrAdd(Buffer->GetRHIUnchecked(), Buffer);
		AsyncSetupQueue.Push(FAsyncSetupOp::CullRootBuffer(Buffer));
	}
	return GetPooledBuffer(Buffer);
}

const TRefCountPtr<IPooledRenderTarget>& FRDGBuilder::ConvertToExternalTexture(FRDGTextureRef Texture)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateConvertToExternalResource(Texture));
	if (!Texture->bExternal)
	{
		Texture->bExternal = 1;
		if (!Texture->ResourceRHI)
		{
			SetPooledRenderTargetRHI(Texture, AllocatePooledRenderTargetRHI(RHICmdList, Texture));
		}
		ExternalTextures.FindOrAdd(Texture->GetRHIUnchecked(), Texture);
		AsyncSetupQueue.Push(FAsyncSetupOp::CullRootTexture(Texture));
	}
	return GetPooledTexture(Texture);
}

FRHIUniformBuffer* FRDGBuilder::ConvertToExternalUniformBuffer(FRDGUniformBufferRef UniformBuffer)
{
	if (!UniformBuffer->bExternal)
	{
		UniformBuffer->GetParameters().Enumerate([this](const FRDGParameter& Param)
		{
			const auto ConvertTexture = [](FRDGBuilder* Builder, FRDGTextureRef Texture)
			{
				if (Texture && !Texture->IsExternal())
				{
					Builder->ConvertToExternalTexture(Texture);
				}
			};

			const auto ConvertBuffer = [](FRDGBuilder* Builder, FRDGBufferRef Buffer)
			{
				if (Buffer && !Buffer->IsExternal())
				{
					Builder->ConvertToExternalBuffer(Buffer);
				}
			};

			const auto ConvertView = [this] (FRDGView* View)
			{
				if (!View->ResourceRHI)
				{
					InitViewRHI(RHICmdList, View);
				}
			};

			switch (Param.GetType())
			{
			case UBMT_RDG_TEXTURE:
			{
				ConvertTexture(this, Param.GetAsTexture());
			}
			break;
			case UBMT_RDG_TEXTURE_ACCESS:
			{
				ConvertTexture(this, Param.GetAsTextureAccess().GetTexture());
			}
			break;
			case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
			{
				const FRDGTextureAccessArray& Array = Param.GetAsTextureAccessArray();
				for (int Index = 0; Index < Array.Num(); ++Index)
				{
					ConvertTexture(this, Array[Index].GetTexture());
				}
			}
			break;
			case UBMT_RDG_TEXTURE_SRV:
			{
				ConvertTexture(this, Param.GetAsTextureSRV()->Desc.Texture);
				ConvertView(Param.GetAsView());
			}
			break;
			case UBMT_RDG_TEXTURE_UAV:
			{
				ConvertTexture(this, Param.GetAsTextureUAV()->Desc.Texture);
				ConvertView(Param.GetAsView());
			}
			break;
			case UBMT_RDG_BUFFER_ACCESS:
			{
				ConvertBuffer(this, Param.GetAsBufferAccess().GetBuffer());
			}
			break;
			case UBMT_RDG_BUFFER_ACCESS_ARRAY:
			{
				const FRDGBufferAccessArray& Array = Param.GetAsBufferAccessArray();
				for (int Index = 0; Index < Array.Num(); ++Index)
				{
					ConvertBuffer(this, Array[Index].GetBuffer());
				}
			}
			break;
			case UBMT_RDG_BUFFER_SRV:
			{
				ConvertBuffer(this, Param.GetAsBufferSRV()->Desc.Buffer);
				ConvertView(Param.GetAsView());
			}
			break;
			case UBMT_RDG_BUFFER_UAV:
			{
				ConvertBuffer(this, Param.GetAsBufferUAV()->Desc.Buffer);
				ConvertView(Param.GetAsView());
			}
			break;
			case UBMT_RDG_UNIFORM_BUFFER:
			{
				FRDGUniformBufferRef Buffer = Param.GetAsUniformBuffer().GetUniformBuffer();
				if (Buffer)
				{
					ConvertToExternalUniformBuffer(Buffer);
				}
			}
			break;

			// Non-RDG cases
			case UBMT_INT32:
			case UBMT_UINT32:
			case UBMT_FLOAT32:
			case UBMT_TEXTURE:
			case UBMT_SRV:
			case UBMT_UAV:
			case UBMT_SAMPLER:
			case UBMT_NESTED_STRUCT:
			case UBMT_INCLUDED_STRUCT:
			case UBMT_REFERENCED_STRUCT:
			case UBMT_RENDER_TARGET_BINDING_SLOTS:
			break;

			default:
				check(0);
			}
		});
	}
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateConvertToExternalUniformBuffer(UniformBuffer));
	if (!UniformBuffer->bExternal)
	{
		UniformBuffer->bExternal = true;

		// It's safe to reset the access to false because validation won't allow this call during execution.
		IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = true);
		UniformBuffer->InitRHI();
		IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = false);
	}
	return UniformBuffer->GetRHIUnchecked();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SHADER_PARAMETER_STRUCT(FAccessModePassParameters, )
	RDG_TEXTURE_ACCESS_ARRAY(Textures)
	RDG_BUFFER_ACCESS_ARRAY(Buffers)
END_SHADER_PARAMETER_STRUCT()

void FRDGBuilder::UseExternalAccessMode(FRDGViewableResource* Resource, ERHIAccess ReadOnlyAccess, ERHIPipeline Pipelines)
{
	if (!IsAsyncComputeSupported())
	{
		Pipelines = ERHIPipeline::Graphics;
	}

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateUseExternalAccessMode(Resource, ReadOnlyAccess, Pipelines));

	auto& AccessModeState = Resource->AccessModeState;

	// We already validated that back-to-back calls to UseExternalAccessMode are valid only if the parameters match,
	// so we can safely no-op this call.
	if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::External || AccessModeState.bLocked)
	{
		return;
	}

	// We have to flush the queue when going from QueuedInternal -> External. A queued internal state
	// implies that the resource was in an external access mode before, so it needs an 'end' pass to 
	// contain any passes which might have used the resource in its external state.
	if (AccessModeState.bQueued)
	{
		FlushAccessModeQueue();
	}

	check(!AccessModeState.bQueued);
	AccessModeQueue.Emplace(Resource);
	AccessModeState.bQueued = 1;

	Resource->SetExternalAccessMode(ReadOnlyAccess, Pipelines);
}

void FRDGBuilder::UseInternalAccessMode(FRDGViewableResource* Resource)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateUseInternalAccessMode(Resource));

	auto& AccessModeState = Resource->AccessModeState;

	// Just no-op if the resource is already in (or queued for) the Internal state.
	if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::Internal || AccessModeState.bLocked)
	{
		return;
	}

	// If the resource has a queued transition to the external access state, then we can safely back it out.
	if (AccessModeState.bQueued)
	{
		int32 Index = AccessModeQueue.IndexOfByKey(Resource);
		check(Index < AccessModeQueue.Num());
		AccessModeQueue.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		AccessModeState.bQueued = 0;
	}
	else
	{
		AccessModeQueue.Emplace(Resource);
		AccessModeState.bQueued = 1;
	}

	AccessModeState.Mode = FRDGViewableResource::EAccessMode::Internal;
}

void FRDGBuilder::FlushAccessModeQueue()
{
	if (AccessModeQueue.IsEmpty() || !AuxiliaryPasses.IsFlushAccessModeQueueAllowed())
	{
		return;
	}

	// Don't allow Dump GPU to dump access mode passes. We rely on FlushAccessQueue in dump GPU to transition things back to external access.
	RDG_RECURSION_COUNTER_SCOPE(AuxiliaryPasses.Dump);
	RDG_RECURSION_COUNTER_SCOPE(AuxiliaryPasses.FlushAccessModeQueue);

	FAccessModePassParameters* ParametersByPipeline[] =
	{
		AllocParameters<FAccessModePassParameters>(),
		AllocParameters<FAccessModePassParameters>()
	};

	const ERHIAccess AccessMaskByPipeline[] =
	{
		ERHIAccess::ReadOnlyExclusiveMask,
		ERHIAccess::ReadOnlyExclusiveComputeMask
	};

	ERHIPipeline ParameterPipelines = ERHIPipeline::None;

	TArray<FRDGPass::FExternalAccessOp, FRDGArrayAllocator> Ops;
	Ops.Reserve(ParallelSetup.bEnabled ? AccessModeQueue.Num() : 0);

	for (FRDGViewableResource* Resource : AccessModeQueue)
	{
		const auto& AccessModeState = Resource->AccessModeState;
		Resource->AccessModeState.bQueued = false;

		if (ParallelSetup.bEnabled)
		{
			Ops.Emplace(Resource, AccessModeState.Mode);
		}
		else
		{
			Resource->AccessModeState.ActiveMode = Resource->AccessModeState.Mode;
		}

		ParameterPipelines |= AccessModeState.Pipelines;

		if (AccessModeState.Mode == FRDGViewableResource::EAccessMode::External)
		{
			ExternalAccessResources.Emplace(Resource);
		}
		else
		{
			ExternalAccessResources.Remove(Resource);
		}

		for (uint32 PipelineIndex = 0; PipelineIndex < GetRHIPipelineCount(); ++PipelineIndex)
		{
			const ERHIPipeline Pipeline = static_cast<ERHIPipeline>(1 << PipelineIndex);

			if (EnumHasAnyFlags(AccessModeState.Pipelines, Pipeline))
			{
				const ERHIAccess Access = AccessModeState.Access & AccessMaskByPipeline[PipelineIndex];
				check(Access != ERHIAccess::None);

				switch (Resource->Type)
				{
				case ERDGViewableResourceType::Texture:
					ParametersByPipeline[PipelineIndex]->Textures.Emplace(GetAsTexture(Resource), Access);
					break;
				case ERDGViewableResourceType::Buffer:
					ParametersByPipeline[PipelineIndex]->Buffers.Emplace(GetAsBuffer(Resource), Access);
					break;
				}
			}
		}
	}

	if (EnumHasAnyFlags(ParameterPipelines, ERHIPipeline::Graphics))
	{
		auto ExecuteLambda = [](FRHIComputeCommandList&) {};
		using LambdaPassType = TRDGLambdaPass<FAccessModePassParameters, decltype(ExecuteLambda)>;

		FAccessModePassParameters* Parameters = ParametersByPipeline[GetRHIPipelineIndex(ERHIPipeline::Graphics)];

		FRDGPass* Pass = Passes.Allocate<LambdaPassType>(
			Allocators.Root,
			RDG_EVENT_NAME("AccessModePass[Graphics] (Textures: %d, Buffers: %d)", Parameters->Textures.Num(), Parameters->Buffers.Num()),
			FAccessModePassParameters::FTypeInfo::GetStructMetadata(),
			Parameters,
			// Use all of the work flags so that any access is valid.
			ERDGPassFlags::Copy | ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull,
			MoveTemp(ExecuteLambda));

		Pass->ExternalAccessOps = MoveTemp(Ops);
		Pass->bExternalAccessPass = 1;
		SetupParameterPass(Pass);
	}

	if (EnumHasAnyFlags(ParameterPipelines, ERHIPipeline::AsyncCompute))
	{
		auto ExecuteLambda = [](FRHIComputeCommandList&) {};
		using LambdaPassType = TRDGLambdaPass<FAccessModePassParameters, decltype(ExecuteLambda)>;

		FAccessModePassParameters* Parameters = ParametersByPipeline[GetRHIPipelineIndex(ERHIPipeline::AsyncCompute)];

		FRDGPass* Pass = Passes.Allocate<LambdaPassType>(
			Allocators.Root,
			RDG_EVENT_NAME("AccessModePass[AsyncCompute] (Textures: %d, Buffers: %d)", Parameters->Textures.Num(), Parameters->Buffers.Num()),
			FAccessModePassParameters::FTypeInfo::GetStructMetadata(),
			Parameters,
			ERDGPassFlags::AsyncCompute | ERDGPassFlags::NeverCull,
			MoveTemp(ExecuteLambda));

		Pass->ExternalAccessOps = MoveTemp(Ops);
		Pass->bExternalAccessPass = 1;
		SetupParameterPass(Pass);
	}

	AccessModeQueue.Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FRDGTextureRef FRDGBuilder::RegisterExternalTexture(
	const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
	ERDGTextureFlags Flags)
{
#if RDG_ENABLE_DEBUG
	checkf(ExternalPooledTexture.IsValid(), TEXT("Attempted to register NULL external texture."));
#endif

	const TCHAR* Name = ExternalPooledTexture->GetDesc().DebugName;
	if (!Name)
	{
		Name = TEXT("External");
	}
	return RegisterExternalTexture(ExternalPooledTexture, Name, Flags);
}

FRDGTexture* FRDGBuilder::RegisterExternalTexture(
	const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
	const TCHAR* Name,
	ERDGTextureFlags Flags)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateRegisterExternalTexture(ExternalPooledTexture, Name, Flags));
	FRHITexture* ExternalTextureRHI = ExternalPooledTexture->GetRHI();
	IF_RDG_ENABLE_DEBUG(checkf(ExternalTextureRHI, TEXT("Attempted to register texture %s, but its RHI texture is null."), Name));

	if (FRDGTexture* FoundTexture = FindExternalTexture(ExternalTextureRHI))
	{
		return FoundTexture;
	} 

	const FRDGTextureDesc Desc = Translate(ExternalPooledTexture->GetDesc());
	FRDGTexture* Texture = Textures.Allocate(Allocators.Root, Name, Desc, Flags);
	SetPooledRenderTargetRHI(Texture, ExternalPooledTexture.GetReference());
	Texture->bExternal = true;
	ExternalTextures.FindOrAdd(Texture->GetRHIUnchecked(), Texture);

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateRegisterExternalTexture(Texture));
	IF_RDG_ENABLE_TRACE(Trace.AddResource(Texture));
	return Texture;
}

FRDGBufferRef FRDGBuilder::RegisterExternalBuffer(const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer, ERDGBufferFlags Flags)
{
#if RDG_ENABLE_DEBUG
	checkf(ExternalPooledBuffer.IsValid(), TEXT("Attempted to register NULL external buffer."));
#endif

	const TCHAR* Name = ExternalPooledBuffer->Name;
	if (!Name)
	{
		Name = TEXT("External");
	}
	return RegisterExternalBuffer(ExternalPooledBuffer, Name, Flags);
}

FRDGBufferRef FRDGBuilder::RegisterExternalBuffer(
	const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer,
	const TCHAR* Name,
	ERDGBufferFlags Flags)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateRegisterExternalBuffer(ExternalPooledBuffer, Name, Flags));

	if (FRDGBuffer* FoundBuffer = FindExternalBuffer(ExternalPooledBuffer))
	{
		return FoundBuffer;
	}

	FRDGBuffer* Buffer = Buffers.Allocate(Allocators.Root, Name, ExternalPooledBuffer->Desc, Flags);
	SetPooledBufferRHI(Buffer, ExternalPooledBuffer);
	Buffer->bExternal = true;
	ExternalBuffers.FindOrAdd(Buffer->GetRHIUnchecked(), Buffer);

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateRegisterExternalBuffer(Buffer));
	IF_RDG_ENABLE_TRACE(Trace.AddResource(Buffer));
	return Buffer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::AddPassDependency(FRDGPass* Producer, FRDGPass* Consumer)
{
	auto& Producers = Consumer->Producers;

	if (Producers.Find(Producer) == INDEX_NONE)
	{
#if RDG_STATS
		GRDGStatPassDependencyCount++;
#endif

		if (Producer->Pipeline != Consumer->Pipeline)
		{
			const auto BinarySearchOrAdd = [](auto& Range, FRDGPassHandle Handle)
			{
				const int32 LowerBoundIndex = Algo::LowerBound(Range, Handle);
				if (LowerBoundIndex < Range.Num())
				{
					if (Range[LowerBoundIndex] == Handle)
					{
						return;
					}
				}
				Range.Insert(Handle, LowerBoundIndex);
			};

			// Consumers could be culled, so we have to store all of them in a sorted list.
			BinarySearchOrAdd(Producer->CrossPipelineConsumers, Consumer->Handle);

			// Finds the latest producer on the other pipeline for the consumer.
			if (Consumer->CrossPipelineProducer.IsNull() || Producer->Handle > Consumer->CrossPipelineProducer)
			{
				Consumer->CrossPipelineProducer = Producer->Handle;
			}
		}

		Producers.Add(Producer);
	}
}

bool FRDGBuilder::AddCullingDependency(FRDGProducerStatesByPipeline& LastProducers, const FRDGProducerState& NextState, ERHIPipeline NextPipeline)
{
	for (ERHIPipeline LastPipeline : GetRHIPipelines())
	{
		FRDGProducerState& LastProducer = LastProducers[LastPipeline];

		if (LastProducer.Access != ERHIAccess::Unknown)
		{
			FRDGPass* LastProducerPass = LastProducer.Pass;

			if (LastPipeline != NextPipeline)
			{
				// Only certain platforms allow multi-pipe UAV access.
				const ERHIAccess MultiPipelineUAVMask = ERHIAccess::UAVMask & GRHIMultiPipelineMergeableAccessMask;

				// If skipping a UAV barrier across pipelines, use the producer pass that will emit the correct async fence.
				if (EnumHasAnyFlags(NextState.Access, MultiPipelineUAVMask) && SkipUAVBarrier(LastProducer.NoUAVBarrierHandle, NextState.NoUAVBarrierHandle))
				{
					LastProducerPass = LastProducer.PassIfSkipUAVBarrier;
				}
			}

			if (LastProducerPass)
			{
				AddPassDependency(LastProducerPass, NextState.Pass);
			}
		}
	}

	if (IsWritableAccess(NextState.Access))
	{
		FRDGProducerState& LastProducer = LastProducers[NextPipeline];

		// A separate producer pass is tracked for UAV -> UAV dependencies that are skipped. Consider the following scenario:
		//
		//     Graphics:       A   ->    B         ->         D      ->     E       ->        G         ->            I
		//                   (UAV)   (SkipUAV0)           (SkipUAV1)    (SkipUAV1)          (SRV)                   (UAV2)
		//
		// Async Compute:                           C                ->               F       ->         H
		//                                      (SkipUAV0)                        (SkipUAV1)           (SRV)
		//
		// Expected Cross Pipe Dependencies: [A -> C], C -> D, [B -> F], F -> G, E -> H, F -> I. The dependencies wrapped in
		// braces are only introduced properly by tracking a different producer for cross-pipeline skip UAV dependencies, which
		// is only updated if skip UAV is inactive, or if transitioning from one skip UAV set to another (or another writable resource).

		if (LastProducer.NoUAVBarrierHandle.IsNull())
		{
			if (NextState.NoUAVBarrierHandle.IsNull())
			{
				// Assigns the next producer when no skip UAV sets are active.
				LastProducer.PassIfSkipUAVBarrier = NextState.Pass;
			}
		}
		else if (LastProducer.NoUAVBarrierHandle != NextState.NoUAVBarrierHandle)
		{
			// Assigns the last producer in the prior skip UAV barrier set when moving out of a skip UAV barrier set.
			LastProducer.PassIfSkipUAVBarrier = LastProducer.Pass;
		}

		LastProducer.Access             = NextState.Access;
		LastProducer.Pass               = NextState.Pass;
		LastProducer.NoUAVBarrierHandle = NextState.NoUAVBarrierHandle;
		return true;
	}
	return false;
}

void FRDGBuilder::AddCullRootTexture(FRDGTexture* Texture)
{
	check(Texture->IsCullRoot());

	for (auto& LastProducer : Texture->LastProducers)
	{
		AddLastProducersToCullStack(LastProducer);
	}

	FlushCullStack();
}

void FRDGBuilder::AddCullRootBuffer(FRDGBuffer* Buffer)
{
	check(Buffer->IsCullRoot());

	AddLastProducersToCullStack(Buffer->LastProducer);

	FlushCullStack();
}

void FRDGBuilder::AddLastProducersToCullStack(const FRDGProducerStatesByPipeline& LastProducers)
{
	for (const FRDGProducerState& LastProducer : LastProducers)
	{
		if (LastProducer.Pass)
		{
			CullPassStack.Emplace(LastProducer.Pass);
		}
	}
}

void FRDGBuilder::FlushCullStack()
{
	while (CullPassStack.Num())
	{
		FRDGPass* Pass = CullPassStack.Pop(EAllowShrinking::No);

		if (Pass->bCulled)
		{
			Pass->bCulled = 0;

			CullPassStack.Append(Pass->Producers);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::Compile()
{
	SCOPE_CYCLE_COUNTER(STAT_RDG_CompileTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDG_Compile, GRDGVerboseCSVStats != 0);

	const FRDGPassHandle ProloguePassHandle = GetProloguePassHandle();
	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	const uint32 CompilePassCount = Passes.Num();

	TransitionCreateQueue.Reserve(CompilePassCount);

	const bool bCullPasses = GRDGCullPasses > 0;

	if (bCullPasses || AsyncComputePassCount > 0)
	{
		SCOPED_NAMED_EVENT(PassDependencies, FColor::Emerald);

		if (!ParallelSetup.bEnabled)
		{
			for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
			{
				SetupPassDependencies(Passes[PassHandle]);
			}
		}
	}
	else if (!ParallelSetup.bEnabled)
	{
		for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
		{
			FRDGPass* Pass = Passes[PassHandle];

			// Add reference counts for passes.

			for (auto& PassState : Pass->TextureStates)
			{
				PassState.Texture->ReferenceCount += PassState.ReferenceCount;
			}

			for (auto& PassState : Pass->BufferStates)
			{
				PassState.Buffer->ReferenceCount += PassState.ReferenceCount;
			}
		}
	}

	for (const FExtractedTexture& ExtractedTexture : ExtractedTextures)
	{
		ExtractedTexture.Texture->ReferenceCount++;
	}

	for (const FExtractedBuffer& ExtractedBuffer : ExtractedBuffers)
	{
		ExtractedBuffer.Buffer->ReferenceCount++;
	}

	// All dependencies in the raw graph have been specified; if enabled, all passes are marked as culled and a
	// depth first search is employed to find reachable regions of the graph. Roots of the search are those passes
	// with outputs leaving the graph or those marked to never cull.

	if (bCullPasses)
	{
		SCOPED_NAMED_EVENT(PassCulling, FColor::Emerald);

		// Manually mark the prologue / epilogue passes as not culled.
		EpiloguePass->bCulled = 0;
		ProloguePass->bCulled = 0;

		check(CullPassStack.IsEmpty());

		for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
		{
			FRDGPass* Pass = Passes[PassHandle];

			if (!Pass->bCulled)
			{
				continue;
			}

			// Subtract reference counts from culled passes that were added during pass setup.
		
			for (auto& PassState : Pass->TextureStates)
			{
				PassState.Texture->ReferenceCount -= PassState.ReferenceCount;
			}

			for (auto& PassState : Pass->BufferStates)
			{
				PassState.Buffer->ReferenceCount -= PassState.ReferenceCount;
			}
		}
	}

	// Traverses passes on the graphics pipe and merges raster passes with the same render targets into a single RHI render pass.
	if (IsRenderPassMergeEnabled() && RasterPassCount > 0)
	{
		SCOPED_NAMED_EVENT(MergeRenderPasses, FColor::Emerald);

		TArray<FRDGPassHandle, TInlineAllocator<32, FRDGArrayAllocator>> PassesToMerge;
		FRDGPass* PrevPass = nullptr;
		const FRenderTargetBindingSlots* PrevRenderTargets = nullptr;

		const auto CommitMerge = [&]
		{
			if (PassesToMerge.Num())
			{
				const auto SetEpilogueBarrierPass = [&](FRDGPass* Pass, FRDGPassHandle EpilogueBarrierPassHandle)
				{
					Pass->EpilogueBarrierPass = EpilogueBarrierPassHandle;
					Pass->ResourcesToEnd.Reset();
					Passes[EpilogueBarrierPassHandle]->ResourcesToEnd.Add(Pass);
				};

				const auto SetPrologueBarrierPass = [&](FRDGPass* Pass, FRDGPassHandle PrologueBarrierPassHandle)
				{
					Pass->PrologueBarrierPass = PrologueBarrierPassHandle;
					Pass->ResourcesToBegin.Reset();
					Passes[PrologueBarrierPassHandle]->ResourcesToBegin.Add(Pass);
				};

				const FRDGPassHandle FirstPassHandle = PassesToMerge[0];
				const FRDGPassHandle LastPassHandle = PassesToMerge.Last();
				Passes[FirstPassHandle]->ResourcesToBegin.Reserve(PassesToMerge.Num());
				Passes[LastPassHandle]->ResourcesToEnd.Reserve(PassesToMerge.Num());

				// Given an interval of passes to merge into a single render pass: [B, X, X, X, X, E]
				//
				// The begin pass (B) and end (E) passes will call {Begin, End}RenderPass, respectively. Also,
				// begin will handle all prologue barriers for the entire merged interval, and end will handle all
				// epilogue barriers. This avoids transitioning of resources within the render pass and batches the
				// transitions more efficiently. This assumes we have filtered out dependencies between passes from
				// the merge set, which is done during traversal.

				// (B) First pass in the merge sequence.
				{
					FRDGPass* Pass = Passes[FirstPassHandle];
					Pass->bSkipRenderPassEnd = 1;
					SetEpilogueBarrierPass(Pass, LastPassHandle);
				}

				// (X) Intermediate passes.
				for (int32 PassIndex = 1, PassCount = PassesToMerge.Num() - 1; PassIndex < PassCount; ++PassIndex)
				{
					const FRDGPassHandle PassHandle = PassesToMerge[PassIndex];
					FRDGPass* Pass = Passes[PassHandle];
					Pass->bSkipRenderPassBegin = 1;
					Pass->bSkipRenderPassEnd = 1;
					SetPrologueBarrierPass(Pass, FirstPassHandle);
					SetEpilogueBarrierPass(Pass, LastPassHandle);
				}

				// (E) Last pass in the merge sequence.
				{
					FRDGPass* Pass = Passes[LastPassHandle];
					Pass->bSkipRenderPassBegin = 1;
					SetPrologueBarrierPass(Pass, FirstPassHandle);
				}

#if RDG_STATS
				GRDGStatRenderPassMergeCount += PassesToMerge.Num();
#endif
			}
			PassesToMerge.Reset();
			PrevPass = nullptr;
			PrevRenderTargets = nullptr;
		};

		for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
		{
			FRDGPass* NextPass = Passes[PassHandle];

			if (NextPass->bCulled || NextPass->bEmptyParameters)
			{
				continue;
			}

			if (EnumHasAnyFlags(NextPass->Flags, ERDGPassFlags::Raster))
			{
				// A pass where the user controls the render pass or it is forced to skip pass merging can't merge with other passes
				if (EnumHasAnyFlags(NextPass->Flags, ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverMerge))
				{
					CommitMerge();
					continue;
				}

				// A pass which writes to resources outside of the render pass introduces new dependencies which break merging.
				if (!NextPass->bRenderPassOnlyWrites)
				{
					CommitMerge();
					continue;
				}

				const FRenderTargetBindingSlots& RenderTargets = NextPass->GetParameters().GetRenderTargets();

				if (PrevPass)
				{
					check(PrevRenderTargets);

					if (PrevRenderTargets->CanMergeBefore(RenderTargets)
#if WITH_MGPU
						&& PrevPass->GPUMask == NextPass->GPUMask
#endif
						)
					{
						if (!PassesToMerge.Num())
						{
							PassesToMerge.Add(PrevPass->GetHandle());
						}
						PassesToMerge.Add(PassHandle);
					}
					else
					{
						CommitMerge();
					}
				}

				PrevPass = NextPass;
				PrevRenderTargets = &RenderTargets;
			}
			else if (!EnumHasAnyFlags(NextPass->Flags, ERDGPassFlags::AsyncCompute))
			{
				// A non-raster pass on the graphics pipe will invalidate the render target merge.
				CommitMerge();
			}
		}

		CommitMerge();
	}

	if (AsyncComputePassCount > 0)
	{
		SCOPED_NAMED_EVENT(AsyncComputeFences, FColor::Emerald);

		// Establishes fork / join overlap regions for async compute. This is used for fencing as well as resource
		// allocation / deallocation. Async compute passes can't allocate / release their resource references until
		// the fork / join is complete, since the two pipes run in parallel. Therefore, all resource lifetimes on
		// async compute are extended to cover the full async region.

		FRDGPassHandle CurrentGraphicsForkPassHandle;
		FRDGPass* AsyncComputePassBeforeFork = nullptr;

		for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
		{
			FRDGPass* AsyncComputePass = Passes[PassHandle];

			if (!AsyncComputePass->IsAsyncCompute() || AsyncComputePass->bCulled)
			{
				continue;
			}

			FRDGPassHandle GraphicsForkPassHandle = FRDGPassHandle::Max(AsyncComputePass->CrossPipelineProducer, FRDGPassHandle::Max(CurrentGraphicsForkPassHandle, ProloguePassHandle));
			FRDGPass* GraphicsForkPass = Passes[GraphicsForkPassHandle];

			AsyncComputePass->GraphicsForkPass = GraphicsForkPassHandle;
			Passes[GraphicsForkPass->PrologueBarrierPass]->ResourcesToBegin.Add(AsyncComputePass);

			if (CurrentGraphicsForkPassHandle != GraphicsForkPassHandle)
			{
				CurrentGraphicsForkPassHandle = GraphicsForkPassHandle;

				FRDGBarrierBatchBegin& EpilogueBarriersToBeginForAsyncCompute = GraphicsForkPass->GetEpilogueBarriersToBeginForAsyncCompute(Allocators.Transition, TransitionCreateQueue);

				GraphicsForkPass->bGraphicsFork = 1;
				EpilogueBarriersToBeginForAsyncCompute.SetUseCrossPipelineFence();

				AsyncComputePass->bAsyncComputeBegin = 1;
				AsyncComputePass->GetPrologueBarriersToEnd(Allocators.Transition).AddDependency(&EpilogueBarriersToBeginForAsyncCompute);

				// Since we are fencing the graphics pipe to some new async compute work, make sure to flush any prior work.
				if (AsyncComputePassBeforeFork)
				{
					AsyncComputePassBeforeFork->bDispatchAfterExecute = 1;
				}
			}

			AsyncComputePassBeforeFork = AsyncComputePass;
		}

		FRDGPassHandle CurrentGraphicsJoinPassHandle;

		for (FRDGPassHandle PassHandle = EpiloguePassHandle - 1; PassHandle > ProloguePassHandle; --PassHandle)
		{
			FRDGPass* AsyncComputePass = Passes[PassHandle];

			if (!AsyncComputePass->IsAsyncCompute() || AsyncComputePass->bCulled)
			{
				continue;
			}

			FRDGPassHandle CrossPipelineConsumer;

			// Cross pipeline consumers are sorted. Find the earliest consumer that isn't culled.
			for (FRDGPassHandle ConsumerHandle : AsyncComputePass->CrossPipelineConsumers)
			{
				FRDGPass* Consumer = Passes[ConsumerHandle];

				if (!Consumer->bCulled)
				{
					CrossPipelineConsumer = ConsumerHandle;
					break;
				}
			}

			FRDGPassHandle GraphicsJoinPassHandle = FRDGPassHandle::Min(CrossPipelineConsumer, FRDGPassHandle::Min(CurrentGraphicsJoinPassHandle, EpiloguePassHandle));
			FRDGPass* GraphicsJoinPass = Passes[GraphicsJoinPassHandle];

			AsyncComputePass->GraphicsJoinPass = GraphicsJoinPassHandle;
			Passes[GraphicsJoinPass->EpilogueBarrierPass]->ResourcesToEnd.Add(AsyncComputePass);

			if (CurrentGraphicsJoinPassHandle != GraphicsJoinPassHandle)
			{
				CurrentGraphicsJoinPassHandle = GraphicsJoinPassHandle;

				FRDGBarrierBatchBegin& EpilogueBarriersToBeginForGraphics = AsyncComputePass->GetEpilogueBarriersToBeginForGraphics(Allocators.Transition, TransitionCreateQueue);

				AsyncComputePass->bAsyncComputeEnd = 1;
				AsyncComputePass->bDispatchAfterExecute = 1;
				EpilogueBarriersToBeginForGraphics.SetUseCrossPipelineFence();

				GraphicsJoinPass->bGraphicsJoin = 1;
				GraphicsJoinPass->GetPrologueBarriersToEnd(Allocators.Transition).AddDependency(&EpilogueBarriersToBeginForGraphics);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::LaunchAsyncSetupQueueTask()
{
	if (AsyncSetupQueue.LastTask.IsCompleted())
	{
		AsyncSetupQueue.LastTask = AsyncSetupQueue.Pipe.Launch(UE_SOURCE_LOCATION, [this]() mutable
		{
			ProcessAsyncSetupQueue();
		});
	}
}

void FRDGBuilder::ProcessAsyncSetupQueue()
{
	SCOPED_NAMED_EVENT_TCHAR("FRDGBuilder::ProcessAsyncSetupQueue", FColor::Magenta);
	FRDGAllocatorScope AllocatorScope(Allocators.Task);

	while (true)
	{
		AsyncSetupQueue.Mutex.Lock();
		TArray<FAsyncSetupOp, FRDGArrayAllocator> PoppedOps = MoveTemp(AsyncSetupQueue.Ops);
		AsyncSetupQueue.Mutex.Unlock();

		if (PoppedOps.IsEmpty())
		{
			break;
		}

		for (FAsyncSetupOp Op : PoppedOps)
		{
			switch (Op.Type)
			{
			case FAsyncSetupOp::EType::SetupPassResources:
				SetupPassResources(Op.Pass);
				break;

			case FAsyncSetupOp::EType::CullRootBuffer:
				AddCullRootBuffer(Op.Buffer);
				break;

			case FAsyncSetupOp::EType::CullRootTexture:
				AddCullRootTexture(Op.Texture);
				break;
			}
		}
	}
}

void FRDGBuilder::FlushSetupQueue()
{
	if (ParallelSetup.bEnabled)
	{
		LaunchAsyncSetupQueueTask();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::WaitForParallelSetupTasks()
{
	if (!ParallelSetup.Tasks.IsEmpty())
	{
		UE::Tasks::Wait(ParallelSetup.Tasks);
		ParallelSetup.Tasks.Reset();
	}
}

void FRDGBuilder::SubmitParallelSetupTasks()
{
	if (!ParallelSetup.CommandLists.IsEmpty())
	{
		RHICmdList.QueueAsyncCommandListSubmit(ParallelSetup.CommandLists);
		ParallelSetup.CommandLists.Empty();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::Execute()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RDG);
	SCOPED_DRAW_EVENTF(RHICmdList, FRDGBuilder_Execute, TEXT("FRDGBuilder::Execute"));

	GRDGTransientResourceAllocator.ReleasePendingDeallocations();

	{
		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::FlushAccessModeQueue", FColor::Magenta);
		for (FRDGViewableResource* Resource : ExternalAccessResources)
		{
			UseInternalAccessMode(Resource);
		}
		FlushAccessModeQueue();
	}

	// Create the epilogue pass at the end of the graph just prior to compilation.
	SetupEmptyPass(EpiloguePass = Passes.Allocate<FRDGSentinelPass>(Allocators.Root, RDG_EVENT_NAME("Graph Epilogue")));

	const FRDGPassHandle ProloguePassHandle = GetProloguePassHandle();
	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	UE::Tasks::FTask CollectPassBarriersTask;
	UE::Tasks::FTask CreateViewsTask;

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExecuteBegin());
	IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = true);

	FCollectResourceContext CollectResourceContext;

	if (!IsImmediateMode())
	{
		SubmitParallelSetupTasks();
		BeginFlushResourcesRHI();
		WaitForParallelSetupTasks();

		if (ParallelSetup.bEnabled)
		{
			AsyncSetupQueue.LastTask.Wait();
			ProcessAsyncSetupQueue();
		}

		// Pre-allocate containers.
		{
			const int32 NumBuffers           = Buffers.Num();
			const int32 NumTextures          = Textures.Num();
			const int32 NumExternalBuffers   = ExternalBuffers.Num();
			const int32 NumExternalTextures  = ExternalTextures.Num();
			const int32 NumTransientBuffers  = bSupportsTransientBuffers ? (NumBuffers - NumExternalBuffers) : 0;
			const int32 NumTransientTextures = bSupportsTransientTextures ? (NumTextures - NumExternalTextures) : 0;
			const int32 NumPooledTextures    = NumTextures - NumTransientTextures;
			const int32 NumPooledBuffers     = NumBuffers - NumTransientBuffers;

			CollectResourceContext.TransientResources.Reserve(NumTransientBuffers + NumTransientTextures);
			CollectResourceContext.PooledTextures.Reserve(bSupportsTransientTextures ? NumExternalTextures : NumTextures);
			CollectResourceContext.PooledBuffers.Reserve(bSupportsTransientBuffers ? NumExternalBuffers : NumBuffers);
			CollectResourceContext.UniformBuffers.Reserve(UniformBuffers.Num());
			CollectResourceContext.Views.Reserve(Views.Num());
			CollectResourceContext.UniformBufferMap.Init(true, UniformBuffers.Num());
			CollectResourceContext.ViewMap.Init(true, Views.Num());

			PooledBufferOwnershipMap.Reserve(NumPooledBuffers);
			PooledTextureOwnershipMap.Reserve(NumPooledTextures);
			ActivePooledTextures.Reserve(NumPooledTextures);
			ActivePooledBuffers.Reserve(NumPooledBuffers);
			EpilogueResourceAccesses.Reserve(NumTextures + NumBuffers);

			ProloguePass->EpilogueBarriersToBeginForGraphics.Reserve(NumPooledBuffers + NumPooledTextures);
		}

		const UE::Tasks::ETaskPriority TaskPriority = UE::Tasks::ETaskPriority::High;

		UE::Tasks::FTask BufferNumElementsCallbacksTask = AddSetupTask([this]
		{
			SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::FinalizeDescs", FColor::Magenta);

			for (FRDGBuffer* Buffer : NumElementsCallbackBuffers)
			{
				Buffer->FinalizeDesc();
			}
			NumElementsCallbackBuffers.Empty();

		}, TaskPriority);

		UE::Tasks::FTask PrepareCollectResourcesTask = AddSetupTask([this]
		{
			SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::PrepareCollectResources", FColor::Magenta);

			Buffers.Enumerate([&] (FRDGBuffer* Buffer)
			{
				if (Buffer->ResourceRHI || Buffer->bQueuedForUpload)
				{
					Buffer->bCollectForAllocate = false;
				}

				if (Buffer->TransientBuffer || (!Buffer->ResourceRHI && IsTransient(Buffer)))
				{
					Buffer->bTransient = true;
				}
			});

			Textures.Enumerate([&] (FRDGTexture* Texture)
			{
				if (Texture->ResourceRHI)
				{
					Texture->bCollectForAllocate = false;
				}

				if (Texture->TransientTexture || (!Texture->ResourceRHI && IsTransient(Texture)))
				{
					Texture->bTransient = true;
				}
			});

		}, TaskPriority);

		UE::Tasks::FTaskEvent AllocateUploadBuffersTask{ UE_SOURCE_LOCATION };

		UE::Tasks::FTask SubmitBufferUploadsTask = AddCommandListSetupTask([this, AllocateUploadBuffersTask] (FRHICommandListBase& RHICmdListTask) mutable
		{
			SubmitBufferUploads(RHICmdListTask, &AllocateUploadBuffersTask);

		}, BufferNumElementsCallbacksTask, TaskPriority);

		Compile();

		CollectPassBarriersTask = AddSetupTask([this]
		{
			CompilePassBarriers();
			CollectPassBarriers();

		}, TaskPriority);

		if (ParallelExecute.bEnabled)
		{
#if RHI_WANT_BREADCRUMB_EVENTS
			RHICmdList.ExportBreadcrumbState(*BreadcrumbState);
#endif

			AddSetupTask([this] { SetupParallelExecute(); });
		}

		UE::Tasks::FTask AllocatePooledBuffersTask;
		UE::Tasks::FTask AllocatePooledTexturesTask;

		{
			SCOPE_CYCLE_COUNTER(STAT_RDG_CollectResourcesTime);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RDG_CollectResources);
			SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CollectResources", FColor::Magenta);

			EnumerateExtendedLifetimeResources(Textures, [](FRDGTexture* Texture)
			{
				++Texture->ReferenceCount;
			});

			EnumerateExtendedLifetimeResources(Buffers, [](FRDGBuffer* Buffer)
			{
				++Buffer->ReferenceCount;
			});

			PrepareCollectResourcesTask.Wait();

			// Null out any culled external resources so that the reference is freed up.

			for (const auto& Pair : ExternalTextures)
			{
				FRDGTexture* Texture = Pair.Value;

				if (Texture->IsCulled())
				{
					CollectDeallocateTexture(CollectResourceContext, ProloguePassHandle, Texture, 0);
				}
			}

			for (const auto& Pair : ExternalBuffers)
			{
				FRDGBuffer* Buffer = Pair.Value;

				if (Buffer->IsCulled())
				{
					CollectDeallocateBuffer(CollectResourceContext, ProloguePassHandle, Buffer, 0);
				}
			}

			for (FRDGPassHandle PassHandle = ProloguePassHandle; PassHandle <= EpiloguePassHandle; ++PassHandle)
			{
				FRDGPass* Pass = Passes[PassHandle];

				if (!Pass->bCulled)
				{
					CollectAllocations(CollectResourceContext, Pass);
					CollectDeallocations(CollectResourceContext, Pass);
				}
			}

			EnumerateExtendedLifetimeResources(Textures, [&](FRDGTextureRef Texture)
			{
				CollectDeallocateTexture(CollectResourceContext, EpiloguePassHandle, Texture, 1);
			});

			EnumerateExtendedLifetimeResources(Buffers, [&](FRDGBufferRef Buffer)
			{
				CollectDeallocateBuffer(CollectResourceContext, EpiloguePassHandle, Buffer, 1);
			});

			BufferNumElementsCallbacksTask.Wait();

			AllocatePooledBuffersTask = AddCommandListSetupTask([this, PooledBuffers = MoveTemp(CollectResourceContext.PooledBuffers)] (FRHICommandListBase& RHICmdListTask)
			{
				AllocatePooledBuffers(RHICmdListTask, PooledBuffers);

			}, AllocateUploadBuffersTask, TaskPriority);

			AllocatePooledTexturesTask = AddCommandListSetupTask([this, PooledTextures = MoveTemp(CollectResourceContext.PooledTextures)] (FRHICommandListBase& RHICmdListTask)
			{
				AllocatePooledTextures(RHICmdListTask, PooledTextures);

			}, TaskPriority);

			AllocateTransientResources(MoveTemp(CollectResourceContext.TransientResources));

			AddSetupTask([this]
			{
				FinalizeResources();

			}, MakeArrayView<UE::Tasks::FTask>({ CollectPassBarriersTask, AllocatePooledBuffersTask, AllocatePooledTexturesTask }), TaskPriority);

			CreateViewsTask = AddCommandListSetupTask([this, Views = MoveTemp(CollectResourceContext.Views)] (FRHICommandListBase& RHICmdListTask)
			{
				CreateViews(RHICmdListTask, Views);

			}, MakeArrayView<UE::Tasks::FTask>({ AllocatePooledBuffersTask, AllocatePooledTexturesTask, SubmitBufferUploadsTask}), TaskPriority);

			if (TransientResourceAllocator)
			{
#if RDG_ENABLE_TRACE
				TransientResourceAllocator->Flush(RHICmdList, Trace.IsEnabled() ? &Trace.TransientAllocationStats : nullptr);
#else
				TransientResourceAllocator->Flush(RHICmdList);
#endif
			}
		}

		AddSetupTask([this, UniformBuffers = MoveTemp(CollectResourceContext.UniformBuffers)]
		{
			CreateUniformBuffers(UniformBuffers);

		}, CreateViewsTask, TaskPriority); // Uniform buffer creation require views to be valid.

		AllocatePooledBuffersTask.Wait();
		AllocatePooledTexturesTask.Wait();
	}
	else
	{
		SubmitBufferUploads(RHICmdList);
		FinalizeResources();
	}

	SubmitParallelSetupTasks();
	EndFlushResourcesRHI();
	WaitForParallelSetupTasks();

	if (ParallelExecute.DispatchTaskEvent)
	{
		// Launch a task to absorb the cost of waking up threads and avoid stalling the render thread.
		UE::Tasks::Launch(UE_SOURCE_LOCATION, [this] { ParallelExecute.DispatchTaskEvent->Trigger(); });
	}

	IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = ParallelExecute.bEnabled);
	IF_RDG_ENABLE_TRACE(Trace.OutputGraphBegin());

	if (!IsImmediateMode())
	{
		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::ExecutePasses", FColor::Magenta);
		SCOPE_CYCLE_COUNTER(STAT_RDG_ExecuteTime);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RDG_Execute);

		for (FRDGPassHandle PassHandle = ProloguePassHandle; PassHandle <= EpiloguePassHandle; ++PassHandle)
		{
			FRDGPass* Pass = Passes[PassHandle];

			if (Pass->bCulled)
			{
			#if RDG_STATS
				GRDGStatPassCullCount++;
			#endif

				continue;
			}

			if (ParallelExecute.bEnabled)
			{
				if (Pass->bParallelExecute)
				{
				#if RDG_CPU_SCOPES // CPU scopes are replayed on the render thread prior to executing the entire batch.
					Pass->CPUScopeOps.Execute();
				#endif

					if (Pass->bParallelExecuteBegin)
					{
						FParallelPassSet& ParallelPassSet = ParallelExecute.ParallelPassSets[Pass->ParallelPassSetIndex];

						FRHICommandListImmediate::ETranslatePriority TranslatePriority = ParallelPassSet.bParallelTranslate ? FRHICommandListImmediate::ETranslatePriority::Normal : FRHICommandListImmediate::ETranslatePriority::Disabled;

						check(ParallelPassSet.CmdList != nullptr);
						RHICmdList.QueueAsyncCommandListSubmit(MakeArrayView<FRHICommandListImmediate::FQueuedCommandList>(&ParallelPassSet, 1), TranslatePriority);

						IF_RHI_WANT_BREADCRUMB_EVENTS(RHICmdList.ImportBreadcrumbState(*ParallelPassSet.BreadcrumbStateEnd));

						if (ParallelPassSet.bDispatchAfterExecute)
						{
							RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
						}
					}

					continue;
				}
			}
			else if (!Pass->bSentinel)
			{
				CompilePassOps(Pass);
			}

			ExecutePass(Pass, RHICmdList);
		}
	}
	else
	{
		ExecutePass(EpiloguePass, RHICmdList);
	}

	RHICmdList.SetStaticUniformBuffers({});

#if WITH_MGPU
	if (bForceCopyCrossGPU)
	{
		ForceCopyCrossGPU();
	}
#endif

	RHICmdList.SetTrackedAccess(EpilogueResourceAccesses);

	// Wait on the actual parallel execute tasks in the Execute call. When draining is okay to let them overlap with other graph setup.
	// This also needs to be done before extraction of external resources to be consistent with non-parallel rendering.
	if (!ParallelExecute.Tasks.IsEmpty())
	{
		UE::Tasks::Wait(ParallelExecute.Tasks);
		ParallelExecute.Tasks.Empty();
	}

	for (const FExtractedTexture& ExtractedTexture : ExtractedTextures)
	{
		check(ExtractedTexture.Texture->RenderTarget);
		*ExtractedTexture.PooledTexture = ExtractedTexture.Texture->RenderTarget;
	}

	for (const FExtractedBuffer& ExtractedBuffer : ExtractedBuffers)
	{
		check(ExtractedBuffer.Buffer->PooledBuffer);
		*ExtractedBuffer.PooledBuffer = ExtractedBuffer.Buffer->PooledBuffer;
	}

	IF_RDG_ENABLE_TRACE(Trace.OutputGraphEnd(*this));

	GPUScopeStacks.Graphics.EndExecute(RHICmdList, ERHIPipeline::Graphics);
	GPUScopeStacks.AsyncCompute.EndExecute(RHICmdList, ERHIPipeline::AsyncCompute);
	IF_RDG_CPU_SCOPES(CPUScopeStacks.EndExecute());

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExecuteEnd());
	IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = false);

#if RDG_STATS
	GRDGStatBufferCount += Buffers.Num();
	GRDGStatTextureCount += Textures.Num();
	GRDGStatViewCount += Views.Num();
	GRDGStatMemoryWatermark = FMath::Max(GRDGStatMemoryWatermark, Allocators.GetByteCount());
#endif

	RasterPassCount = 0;
	AsyncComputePassCount = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::MarkResourcesAsProduced(FRDGPass* Pass)
{
	const auto MarkAsProduced = [&](FRDGViewableResource* Resource)
	{
		Resource->bProduced = true;
	};

	const auto MarkAsProducedIfWritable = [&](FRDGViewableResource* Resource, ERHIAccess Access)
	{
		if (IsWritableAccess(Access))
		{
			Resource->bProduced = true;
		}
	};

	Pass->GetParameters().Enumerate([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RDG_TEXTURE_UAV:
			if (FRDGTextureUAV* UAV = Parameter.GetAsTextureUAV())
			{
				MarkAsProduced(UAV->GetParent());
			}
		break;
		case UBMT_RDG_BUFFER_UAV:
			if (FRDGBufferUAV* UAV = Parameter.GetAsBufferUAV())
			{
				MarkAsProduced(UAV->GetParent());
			}
		break;
		case UBMT_RDG_TEXTURE_ACCESS:
		{
			if (FRDGTextureAccess TextureAccess = Parameter.GetAsTextureAccess())
			{
				MarkAsProducedIfWritable(TextureAccess.GetTexture(), TextureAccess.GetAccess());
			}
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
		{
			const FRDGTextureAccessArray& TextureAccessArray = Parameter.GetAsTextureAccessArray();

			for (FRDGTextureAccess TextureAccess : TextureAccessArray)
			{
				MarkAsProducedIfWritable(TextureAccess.GetTexture(), TextureAccess.GetAccess());
			}
		}
		break;
		case UBMT_RDG_BUFFER_ACCESS:
			if (FRDGBufferAccess BufferAccess = Parameter.GetAsBufferAccess())
			{
				MarkAsProducedIfWritable(BufferAccess.GetBuffer(), BufferAccess.GetAccess());
			}
		break;
		case UBMT_RDG_BUFFER_ACCESS_ARRAY:
		{
			const FRDGBufferAccessArray& BufferAccessArray = Parameter.GetAsBufferAccessArray();

			for (FRDGBufferAccess BufferAccess : BufferAccessArray)
			{
				MarkAsProducedIfWritable(BufferAccess.GetBuffer(), BufferAccess.GetAccess());
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			const FRenderTargetBindingSlots& RenderTargets = Parameter.GetAsRenderTargetBindingSlots();

			RenderTargets.Enumerate([&](FRenderTargetBinding RenderTarget)
			{
				MarkAsProduced(RenderTarget.GetTexture());

				if (FRDGTexture* ResolveTexture = RenderTarget.GetResolveTexture())
				{
					MarkAsProduced(ResolveTexture);
				}
			});

			const FDepthStencilBinding& DepthStencil = RenderTargets.DepthStencil;

			if (DepthStencil.GetDepthStencilAccess().IsAnyWrite())
			{
				MarkAsProduced(DepthStencil.GetTexture());
			}
		}
		break;
		}
	});
}

void FRDGBuilder::SetupPassDependencies(FRDGPass* Pass)
{
	bool bIsCullRootProducer = false;

	for (auto& PassState : Pass->TextureStates)
	{
		FRDGTextureRef Texture = PassState.Texture;
		auto& LastProducers = Texture->LastProducers;

		Texture->ReferenceCount += PassState.ReferenceCount;

		for (uint32 Index = 0, Count = LastProducers.Num(); Index < Count; ++Index)
		{
			const FRDGSubresourceState* SubresourceState = PassState.State[Index];

			if (!SubresourceState)
			{
				continue;
			}

			FRDGProducerState ProducerState;
			ProducerState.Pass = Pass;
			ProducerState.Access = SubresourceState->Access;
			ProducerState.NoUAVBarrierHandle = SubresourceState->NoUAVBarrierFilter.GetUniqueHandle();

			bIsCullRootProducer |= AddCullingDependency(LastProducers[Index], ProducerState, Pass->Pipeline) && Texture->IsCullRoot();
		}
	}

	for (auto& PassState : Pass->BufferStates)
	{
		FRDGBufferRef Buffer = PassState.Buffer;
		const FRDGSubresourceState& SubresourceState = PassState.State;

		Buffer->ReferenceCount += PassState.ReferenceCount;

		FRDGProducerState ProducerState;
		ProducerState.Pass = Pass;
		ProducerState.Access = SubresourceState.Access;
		ProducerState.NoUAVBarrierHandle = SubresourceState.NoUAVBarrierFilter.GetUniqueHandle();

		bIsCullRootProducer |= AddCullingDependency(Buffer->LastProducer, ProducerState, Pass->Pipeline) && Buffer->IsCullRoot();
	}

	const bool bCullPasses = GRDGCullPasses > 0;
	Pass->bCulled = bCullPasses;

	if (bCullPasses && (bIsCullRootProducer || Pass->bHasExternalOutputs || EnumHasAnyFlags(Pass->Flags, ERDGPassFlags::NeverCull)))
	{
		CullPassStack.Emplace(Pass);

		FlushCullStack();
	}
}

void FRDGBuilder::SetupPassResources(FRDGPass* Pass)
{
	const FRDGParameterStruct PassParameters = Pass->GetParameters();
	const FRDGPassHandle PassHandle = Pass->Handle;
	const ERDGPassFlags PassFlags = Pass->Flags;
	const ERHIPipeline PassPipeline = Pass->Pipeline;

	bool bRenderPassOnlyWrites = true;

	const auto TryAddView = [&](FRDGViewRef View)
	{
		if (View && View->LastPass != PassHandle)
		{
			View->LastPass = PassHandle;
			Pass->Views.Add(View->Handle);
		}
	};

	Pass->Views.Reserve(PassParameters.GetBufferParameterCount() + PassParameters.GetTextureParameterCount());
	Pass->TextureStates.Reserve(PassParameters.GetTextureParameterCount() + (PassParameters.HasRenderTargets() ? (MaxSimultaneousRenderTargets + 1) : 0));
	EnumerateTextureAccess(PassParameters, PassFlags, [&](FRDGViewRef TextureView, FRDGTextureRef Texture, ERHIAccess Access, ERDGTextureAccessFlags AccessFlags, FRDGTextureSubresourceRange Range)
	{
		TryAddView(TextureView);

		if (Texture->AccessModeState.IsExternalAccess() && !Pass->bExternalAccessPass)
		{
			// Resources in external access mode are expected to remain in the same state and are ignored by the graph.
			// As only External | Extracted resources can be set as external by the user, the graph doesn't need to track
			// them any more for culling / transition purposes. Validation checks that these invariants are true.
			IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExternalAccess(Texture, Access, Pass));
			return;
		}

		const FRDGViewHandle NoUAVBarrierHandle = GetHandleIfNoUAVBarrier(TextureView);
		const EResourceTransitionFlags TransitionFlags = GetTextureViewTransitionFlags(TextureView, Texture);

		FRDGPass::FTextureState* PassState;

		if (Texture->LastPass != PassHandle)
		{
			Texture->LastPass = PassHandle;
			Texture->PassStateIndex = static_cast<uint16>(Pass->TextureStates.Num());

			PassState = &Pass->TextureStates.Emplace_GetRef(Texture);
		}
		else
		{
			PassState = &Pass->TextureStates[Texture->PassStateIndex];
		}

		PassState->ReferenceCount++;

		EnumerateSubresourceRange(PassState->State, Texture->Layout, Range, [&](FRDGSubresourceState*& State)
		{
			if (!State)
			{
				State = AllocSubresource();
			}

			IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddSubresourceAccess(Texture, *State, Access));

			State->Access = MakeValidAccess(State->Access, Access);
			State->Flags |= TransitionFlags;
			State->NoUAVBarrierFilter.AddHandle(NoUAVBarrierHandle);
			State->SetPass(PassPipeline, PassHandle);
		});

		if (IsWritableAccess(Access))
		{
			bRenderPassOnlyWrites &= EnumHasAnyFlags(AccessFlags, ERDGTextureAccessFlags::RenderTarget);

			// When running in parallel this is set via MarkResourcesAsProduced. We also can't touch this as its a bitfield and not atomic.
			if (!ParallelSetup.bEnabled)
			{
				Texture->bProduced = true;
			}
		}
	});

	Pass->BufferStates.Reserve(PassParameters.GetBufferParameterCount());
	EnumerateBufferAccess(PassParameters, PassFlags, [&](FRDGViewRef BufferView, FRDGBufferRef Buffer, ERHIAccess Access)
	{
		TryAddView(BufferView);

		if (Buffer->AccessModeState.IsExternalAccess() && !Pass->bExternalAccessPass)
		{
			// Resources in external access mode are expected to remain in the same state and are ignored by the graph.
			// As only External | Extracted resources can be set as external by the user, the graph doesn't need to track
			// them any more for culling / transition purposes. Validation checks that these invariants are true.
			IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExternalAccess(Buffer, Access, Pass));
			return;
		}

		const FRDGViewHandle NoUAVBarrierHandle = GetHandleIfNoUAVBarrier(BufferView);

		FRDGPass::FBufferState* PassState;

		if (Buffer->LastPass != PassHandle)
		{
			Buffer->LastPass = PassHandle;
			Buffer->PassStateIndex = Pass->BufferStates.Num();

			PassState = &Pass->BufferStates.Emplace_GetRef(Buffer);
		}
		else
		{
			PassState = &Pass->BufferStates[Buffer->PassStateIndex];
		}

		IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddSubresourceAccess(Buffer, PassState->State, Access));

		PassState->ReferenceCount++;
		PassState->State.Access = MakeValidAccess(PassState->State.Access, Access);
		PassState->State.NoUAVBarrierFilter.AddHandle(NoUAVBarrierHandle);
		PassState->State.SetPass(PassPipeline, PassHandle);

		if (IsWritableAccess(Access))
		{
			bRenderPassOnlyWrites = false;

			// When running in parallel this is set via MarkResourcesAsProduced. We also can't touch this as its a bitfield and not atomic.
			if (!ParallelSetup.bEnabled)
			{
				Buffer->bProduced = true;
			}
		}
	});

	Pass->bEmptyParameters = !Pass->TextureStates.Num() && !Pass->BufferStates.Num();
	Pass->bRenderPassOnlyWrites = bRenderPassOnlyWrites;
	Pass->bHasExternalOutputs = PassParameters.HasExternalOutputs();

	Pass->UniformBuffers.Reserve(PassParameters.GetUniformBufferParameterCount());
	PassParameters.EnumerateUniformBuffers([&](FRDGUniformBufferBinding UniformBuffer)
	{
		Pass->UniformBuffers.Emplace(UniformBuffer.GetUniformBuffer()->Handle);
	});

	if (ParallelSetup.bEnabled)
	{
		SetupPassDependencies(Pass);

		for (FRDGPass::FExternalAccessOp Op : Pass->ExternalAccessOps)
		{
			Op.Resource->AccessModeState.ActiveMode = Op.Mode;
		}
	}
}

void FRDGBuilder::SetupPassInternals(FRDGPass* Pass)
{
	const FRDGPassHandle PassHandle = Pass->Handle;
	const ERDGPassFlags PassFlags = Pass->Flags;
	const ERHIPipeline PassPipeline = Pass->Pipeline;

	Pass->PrologueBarrierPass = PassHandle;
	Pass->EpilogueBarrierPass = PassHandle;

	if (Pass->Pipeline == ERHIPipeline::Graphics)
	{
		Pass->ResourcesToBegin.Add(Pass);
		Pass->ResourcesToEnd.Add(Pass);
	}

	AsyncComputePassCount += EnumHasAnyFlags(PassFlags, ERDGPassFlags::AsyncCompute) ? 1 : 0;
	RasterPassCount += EnumHasAnyFlags(PassFlags, ERDGPassFlags::Raster) ? 1 : 0;

#if WITH_MGPU
	Pass->GPUMask = RHICmdList.GetGPUMask();
#endif

#if STATS
	Pass->CommandListStat = CommandListStatScope;
#endif

#if RDG_STATS
	GRDGStatPassCount++;
#endif

	IF_RDG_CPU_SCOPES(Pass->CPUScopes = CPUScopeStacks.GetCurrentScopes());
	Pass->GPUScopes = GPUScopeStacks.GetCurrentScopes(PassPipeline);

#if RDG_GPU_DEBUG_SCOPES && RDG_ENABLE_TRACE
	Pass->TraceEventScope = GPUScopeStacks.GetCurrentScopes(ERHIPipeline::Graphics).Event;
#endif

#if RDG_GPU_DEBUG_SCOPES && RDG_ENABLE_DEBUG
	if (GRDGValidation != 0)
	{
		if (const FRDGEventScope* Scope = Pass->GPUScopes.Event)
		{
			Pass->FullPathIfDebug = Scope->GetPath(Pass->Name);
		}
	}
#endif
}

void FRDGBuilder::SetupAuxiliaryPasses(FRDGPass* Pass)
{
	if (IsImmediateMode() && !Pass->bSentinel)
	{
		SCOPED_NAMED_EVENT(FRDGBuilder_ExecutePass, FColor::Emerald);
		RDG_ALLOW_RHI_ACCESS_SCOPE();

		for (auto& PassState : Pass->TextureStates)
		{
			FRDGTexture* Texture = PassState.Texture;

			if (Texture->ResourceRHI)
			{
				Texture->bCollectForAllocate = false;
			}

			for (FRDGSubresourceState*& SubresourceState : Texture->State)
			{
				if (!SubresourceState)
				{
					SubresourceState = &PrologueSubresourceState;
				}
			}

			PassState.MergeState = PassState.State;
		}

		for (auto& PassState : Pass->BufferStates)
		{
			FRDGBuffer* Buffer = PassState.Buffer;

			if (Buffer->ResourceRHI || Buffer->bQueuedForUpload)
			{
				Buffer->bCollectForAllocate = false;
			}

			if (!Buffer->State)
			{
				Buffer->State = &PrologueSubresourceState;
			}

			PassState.MergeState = &PassState.State;
		}

		check(!EnumHasAnyFlags(Pass->Pipeline, ERHIPipeline::AsyncCompute));
		check(ParallelSetup.Tasks.IsEmpty());

		FCollectResourceContext Context;
		SubmitBufferUploads(RHICmdList);
		CompilePassOps(Pass);
		CollectAllocations(Context, Pass);
		AllocatePooledTextures(RHICmdList, Context.PooledTextures);
		AllocatePooledBuffers(RHICmdList, Context.PooledBuffers);
		CreateViews(RHICmdList, Context.Views);
		CreateUniformBuffers(Context.UniformBuffers);
		CollectPassBarriers(Pass->Handle);
		CreatePassBarriers();
		ExecutePass(Pass, RHICmdList);
	}

	IF_RDG_ENABLE_DEBUG(VisualizePassOutputs(Pass));

#if RDG_DUMP_RESOURCES
	DumpResourcePassOutputs(Pass);
#endif
}

FRDGPass* FRDGBuilder::SetupParameterPass(FRDGPass* Pass)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddPass(Pass, AuxiliaryPasses.IsActive()));
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDGBuilder_SetupPass, GRDGVerboseCSVStats != 0);

	SetupPassInternals(Pass);

	if (ParallelSetup.bEnabled)
	{
		MarkResourcesAsProduced(Pass);
		AsyncSetupQueue.Push(FAsyncSetupOp::SetupPassResources(Pass));
	}
	else
	{
		SetupPassResources(Pass);
	}

	SetupAuxiliaryPasses(Pass);
	return Pass;
}

FRDGPass* FRDGBuilder::SetupEmptyPass(FRDGPass* Pass)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddPass(Pass, AuxiliaryPasses.IsActive()));
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDGBuilder_SetupPass, GRDGVerboseCSVStats != 0);

	Pass->bEmptyParameters = true;
	SetupPassInternals(Pass);
	SetupAuxiliaryPasses(Pass);
	return Pass;
}

void FRDGBuilder::CompilePassOps(FRDGPass* Pass)
{
#if WITH_MGPU
	FRHIGPUMask GPUMask = Pass->GPUMask;
#else
	FRHIGPUMask GPUMask = FRHIGPUMask::All();
#endif

#if RDG_CMDLIST_STATS
	if (CommandListStatState != Pass->CommandListStat && !Pass->bSentinel)
	{
		CommandListStatState = Pass->CommandListStat;
		Pass->bSetCommandListStat = 1;
	}
#endif

#if RDG_CPU_SCOPES
	Pass->CPUScopeOps = CPUScopeStacks.CompilePassPrologue(Pass);
#endif

	Pass->GPUScopeOpsPrologue = GPUScopeStacks.CompilePassPrologue(Pass, GPUMask);
	Pass->GPUScopeOpsEpilogue = GPUScopeStacks.CompilePassEpilogue(Pass);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::SubmitBufferUploads(FRHICommandListBase& RHICmdListUpload, UE::Tasks::FTaskEvent* AllocateUploadBuffersTask)
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::SubmitBufferUploads", FColor::Magenta);

	{
		SCOPED_NAMED_EVENT_TEXT("Allocate", FColor::Magenta);
		UE::TScopeLock Lock(GRenderGraphResourcePool.Mutex);

		for (FUploadedBuffer& UploadedBuffer : UploadedBuffers)
		{
			FRDGBuffer* Buffer = UploadedBuffer.Buffer;
			if (!Buffer->ResourceRHI)
			{
				SetPooledBufferRHI(Buffer, AllocatePooledBufferRHI(RHICmdListUpload, Buffer));
			}
		}
	}

	if (AllocateUploadBuffersTask)
	{
		AllocateUploadBuffersTask->Trigger();
	}

	{
		SCOPED_NAMED_EVENT_TEXT("Upload", FColor::Magenta);

		for (FUploadedBuffer& UploadedBuffer : UploadedBuffers)
		{
			FRDGBuffer* Buffer = UploadedBuffer.Buffer;

			if (UploadedBuffer.DataFillCallback)
			{
				const uint32 DataSize = Buffer->Desc.GetSize();
				void* DestPtr = RHICmdListUpload.LockBuffer(Buffer->GetRHIUnchecked(), 0, DataSize, RLM_WriteOnly);
				UploadedBuffer.DataFillCallback(DestPtr, DataSize);
				RHICmdListUpload.UnlockBuffer(Buffer->GetRHIUnchecked());
			}
			else
			{
				if (UploadedBuffer.bUseDataCallbacks)
				{
					UploadedBuffer.Data = UploadedBuffer.DataCallback();
					UploadedBuffer.DataSize = UploadedBuffer.DataSizeCallback();
				}

				if (UploadedBuffer.Data && UploadedBuffer.DataSize)
				{
					check(UploadedBuffer.DataSize <= Buffer->Desc.GetSize());
					void* DestPtr = RHICmdListUpload.LockBuffer(Buffer->GetRHIUnchecked(), 0, UploadedBuffer.DataSize, RLM_WriteOnly);
					FMemory::Memcpy(DestPtr, UploadedBuffer.Data, UploadedBuffer.DataSize);
					RHICmdListUpload.UnlockBuffer(Buffer->GetRHIUnchecked());
				}
			}
		}
	}

	UploadedBuffers.Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::SetupParallelExecute()
{
	SCOPED_NAMED_EVENT(SetupParallelExecute, FColor::Emerald);
	FRDGAllocatorScope AllocatorScope(Allocators.Task);
	TArray<FRDGPass*, TInlineAllocator<64, FRDGArrayAllocator>> ParallelPassCandidates;
	uint32 ParallelPassCandidatesWorkload = 0;
	bool bDispatchAfterExecute = false;
	bool bParallelTranslate = false;

	GPUScopeStacks.ReserveOps(Passes.Num());
	IF_RDG_CPU_SCOPES(CPUScopeStacks.ReserveOps());

	const auto FlushParallelPassCandidates = [&]()
	{
		if (ParallelPassCandidates.IsEmpty())
		{
			return;
		}

		int32 PassBeginIndex = 0;
		int32 PassEndIndex = ParallelPassCandidates.Num();

		// It's possible that the first pass is inside a merged RHI render pass region. If so, we must push it forward until after the render pass ends.
		if (const FRDGPass* FirstPass = ParallelPassCandidates[PassBeginIndex]; FirstPass->PrologueBarrierPass < FirstPass->Handle)
		{
			const FRDGPass* EpilogueBarrierPass = Passes[FirstPass->EpilogueBarrierPass];

			for (; PassBeginIndex < ParallelPassCandidates.Num(); ++PassBeginIndex)
			{
				if (ParallelPassCandidates[PassBeginIndex] == EpilogueBarrierPass)
				{
					++PassBeginIndex;
					break;
				}
			}
		}

		if (PassBeginIndex < PassEndIndex)
		{
			// It's possible that the last pass is inside a merged RHI render pass region. If so, we must push it backwards until after the render pass begins.
			if (FRDGPass* LastPass = ParallelPassCandidates.Last(); LastPass->EpilogueBarrierPass > LastPass->Handle)
			{
				FRDGPass* PrologueBarrierPass = Passes[LastPass->PrologueBarrierPass];

				while (PassEndIndex > PassBeginIndex)
				{
					if (ParallelPassCandidates[--PassEndIndex] == PrologueBarrierPass)
					{
						break;
					}
				}
			}
		}

		const int32 ParallelPassCandidateCount = PassEndIndex - PassBeginIndex;

		if (ParallelPassCandidateCount >= GRDGParallelExecutePassMin)
		{
			FRDGPass* PassBegin = ParallelPassCandidates[PassBeginIndex];
			PassBegin->bParallelExecuteBegin = 1;
			PassBegin->ParallelPassSetIndex = ParallelExecute.ParallelPassSets.Num();

			FRDGPass* PassEnd = ParallelPassCandidates[PassEndIndex - 1];
			PassEnd->bParallelExecuteEnd = 1;
			PassEnd->ParallelPassSetIndex = ParallelExecute.ParallelPassSets.Num();

			for (int32 PassIndex = PassBeginIndex; PassIndex < PassEndIndex; ++PassIndex)
			{
				ParallelPassCandidates[PassIndex]->bParallelExecute = 1;
			}

			FParallelPassSet& ParallelPassSet = ParallelExecute.ParallelPassSets.Emplace_GetRef();
			ParallelPassSet.Passes.Append(ParallelPassCandidates.GetData() + PassBeginIndex, ParallelPassCandidateCount);
			ParallelPassSet.bDispatchAfterExecute = bDispatchAfterExecute;
			ParallelPassSet.bParallelTranslate = bParallelTranslate;
		}

		ParallelPassCandidates.Reset();
		ParallelPassCandidatesWorkload = 0;
		bDispatchAfterExecute = false;
		bParallelTranslate = false;
	};

	ParallelExecute.ParallelPassSets.Reserve(32);
	ParallelPassCandidates.Emplace(ProloguePass);

	for (FRDGPassHandle PassHandle = GetProloguePassHandle() + 1; PassHandle < GetEpiloguePassHandle(); ++PassHandle)
	{
		FRDGPass* Pass = Passes[PassHandle];

		if (Pass->bCulled)
		{
			continue;
		}

		CompilePassOps(Pass);

		if (!Pass->bParallelExecuteAllowed)
		{
			FlushParallelPassCandidates();
			continue;
		}

		bool bPassHasParallelTranslate = EnumHasAnyFlags(Pass->Flags, ERDGPassFlags::ParallelTranslate);
		if (bParallelTranslate != bPassHasParallelTranslate)
		{
			FlushParallelPassCandidates();
		}

		bDispatchAfterExecute |= Pass->bDispatchAfterExecute;
		bParallelTranslate |= bPassHasParallelTranslate;

		ParallelPassCandidates.Emplace(Pass);

		if (!Pass->bSkipRenderPassBegin && !Pass->bSkipRenderPassEnd)
		{
			ParallelPassCandidatesWorkload += Pass->Workload;
		}

		if (ParallelPassCandidatesWorkload >= (uint32)GRDGParallelExecutePassMax)
		{
			FlushParallelPassCandidates();
		}
	}

	ParallelPassCandidates.Emplace(EpiloguePass);
	FlushParallelPassCandidates();

#if RHI_WANT_BREADCRUMB_EVENTS
	SCOPED_NAMED_EVENT(BreadcrumbSetup, FColor::Emerald);

	for (FRDGPassHandle PassHandle = GetProloguePassHandle(); PassHandle <= GetEpiloguePassHandle(); ++PassHandle)
	{
		FRDGPass* Pass = Passes[PassHandle];

		if (Pass->bCulled)
		{
			continue;
		}

		if (Pass->bParallelExecuteBegin)
		{
			FParallelPassSet& ParallelPassSet = ParallelExecute.ParallelPassSets[Pass->ParallelPassSetIndex];
			ParallelPassSet.BreadcrumbStateBegin = BreadcrumbState->Copy(Allocators.Task);
			ParallelPassSet.BreadcrumbStateEnd = ParallelPassSet.BreadcrumbStateBegin;
		}

		Pass->GPUScopeOpsPrologue.Event.Execute(*BreadcrumbState);
		Pass->GPUScopeOpsEpilogue.Event.Execute(*BreadcrumbState);

		if (Pass->bParallelExecuteEnd)
		{
			FParallelPassSet& ParallelPassSet = ParallelExecute.ParallelPassSets[Pass->ParallelPassSetIndex];

			if (ParallelPassSet.BreadcrumbStateEnd->Version != BreadcrumbState->Version)
			{
				ParallelPassSet.BreadcrumbStateEnd = BreadcrumbState->Copy(Allocators.Task);
			}
		}
	}
#endif

	check(ParallelExecute.Tasks.IsEmpty());
	ParallelExecute.Tasks.Reserve(ParallelExecute.ParallelPassSets.Num());
	ParallelExecute.DispatchTaskEvent.Emplace(UE_SOURCE_LOCATION);

	for (FParallelPassSet& ParallelPassSet : ParallelExecute.ParallelPassSets)
	{
		FRHICommandList* RHICmdListPass = new FRHICommandList(FRHIGPUMask::All());
		ParallelPassSet.CmdList = RHICmdListPass;
		IF_RHI_WANT_BREADCRUMB_EVENTS(RHICmdListPass->ImportBreadcrumbState(*ParallelPassSet.BreadcrumbStateBegin));

		ParallelExecute.Tasks.Emplace(UE::Tasks::Launch(TEXT("FRDGBuilder::ParallelExecute"), [this, &ParallelPassSet, RHICmdListPass]
		{
			SCOPED_NAMED_EVENT(ParallelExecute, FColor::Emerald);
			FOptionalTaskTagScope Scope(ETaskTag::EParallelRenderingThread);

			for (FRDGPass* Pass : ParallelPassSet.Passes)
			{
				ExecutePass(Pass, *RHICmdListPass);
			}

			RHICmdListPass->FinishRecording();

		}, *ParallelExecute.DispatchTaskEvent, LowLevelTasks::ETaskPriority::High));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::AllocatePooledTextures(FRHICommandListBase& InRHICmdList, TConstArrayView<FCollectResourceOp> Ops)
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::AllocatePooledTextures", FColor::Magenta);
	UE::TScopeLock Lock(GRenderTargetPool.Mutex);

	for (FCollectResourceOp Op : Ops)
	{
		FRDGTexture* Texture = Textures[FRDGTextureHandle(Op.ResourceIndex)];

		switch (Op.Op)
		{
		case FCollectResourceOp::EOp::Allocate:
			SetPooledRenderTargetRHI(Texture, AllocatePooledRenderTargetRHI(InRHICmdList, Texture));
			break;
		case FCollectResourceOp::EOp::Deallocate:
			if (static_cast<FPooledRenderTarget*>(Texture->RenderTarget)->IsTracked())
			{
				// This releases the reference without invoking a virtual function call.
				TRefCountPtr<FPooledRenderTarget>(MoveTemp(Texture->Allocation));
			}
			break;
		}
	}

	for (FCollectResourceOp Op : Ops)
	{
		FRDGTexture* Texture = Textures[FRDGTextureHandle(Op.ResourceIndex)];

		if (!Texture->bSkipLastTransition)
		{
			// Hold the last reference in a chain of pooled allocations.
			Texture->Allocation = Texture->RenderTarget;
		}
	}
}

void FRDGBuilder::AllocatePooledBuffers(FRHICommandListBase& InRHICmdList, TConstArrayView<FCollectResourceOp> Ops)
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::AllocatePooledBuffers", FColor::Magenta);
	UE::TScopeLock Lock(GRenderGraphResourcePool.Mutex);

	for (FCollectResourceOp Op : Ops)
	{
		FRDGBuffer* Buffer = Buffers[FRDGBufferHandle(Op.ResourceIndex)];

		switch (Op.Op)
		{
		case FCollectResourceOp::EOp::Allocate:
			SetPooledBufferRHI(Buffer, AllocatePooledBufferRHI(InRHICmdList, Buffer));
			break;
		case FCollectResourceOp::EOp::Deallocate:
			Buffer->Allocation = nullptr;
			break;
		}
	}

	for (FCollectResourceOp Op : Ops)
	{
		FRDGBuffer* Buffer = Buffers[FRDGBufferHandle(Op.ResourceIndex)];

		if (!Buffer->bSkipLastTransition)
		{
			// Hold the last reference in a chain of pooled allocations.
			Buffer->Allocation = Buffer->PooledBuffer;
		}
	}
}

void FRDGBuilder::AllocateTransientResources(TConstArrayView<FCollectResourceOp> Ops)
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::AllocateTransientResources", FColor::Magenta);
	for (FCollectResourceOp Op : Ops)
	{
		const FRDGPassHandle PassHandle = Op.PassHandle;

		switch (Op.Op)
		{
		case FCollectResourceOp::EOp::Allocate:
		{
			if (Op.ResourceType == ERDGViewableResourceType::Buffer)
			{
				FRDGBuffer* Buffer = Buffers[FRDGBufferHandle(Op.ResourceIndex)];
				FRHITransientBuffer* TransientBuffer = TransientResourceAllocator->CreateBuffer(Translate(Buffer->Desc), Buffer->Name, PassHandle.GetIndex());

				SetTransientBufferRHI(Buffer, TransientBuffer);

				Buffer->MinAcquirePass = FRDGPassHandle(TransientBuffer->GetAcquirePasses().Min);
			}
			else
			{
				FRDGTexture* Texture = Textures[FRDGTextureHandle(Op.ResourceIndex)];
				FRHITransientTexture* TransientTexture = TransientResourceAllocator->CreateTexture(Texture->Desc, Texture->Name, PassHandle.GetIndex());

				if (Texture->bExternal || Texture->bExtracted)
				{
					SetPooledRenderTargetRHI(Texture, GRDGTransientResourceAllocator.AllocateRenderTarget(TransientTexture));
				}
				else
				{
					SetTransientTextureRHI(Texture, TransientTexture);
				}

				Texture->MinAcquirePass = FRDGPassHandle(TransientTexture->GetAcquirePasses().Min);
			}
		}
		break;
		case FCollectResourceOp::EOp::Deallocate:
		{
			if (Op.ResourceType == ERDGViewableResourceType::Buffer)
			{
				FRDGBuffer* Buffer = Buffers[FRDGBufferHandle(Op.ResourceIndex)];
				FRHITransientBuffer* TransientBuffer = Buffer->TransientBuffer;
				TransientResourceAllocator->DeallocateMemory(TransientBuffer, PassHandle.GetIndex());

				Buffer->MinDiscardPass = FRDGPassHandle(TransientBuffer->GetDiscardPasses().Min);
			}
			else
			{
				FRDGTexture* Texture = Textures[FRDGTextureHandle(Op.ResourceIndex)];
				FRHITransientTexture* TransientTexture = Texture->TransientTexture;

				// Texture is using a transient external render target.
				if (Texture->RenderTarget)
				{
					if (!Texture->bExtracted)
					{
						// This releases the reference without invoking a virtual function call.
						GRDGTransientResourceAllocator.Release(TRefCountPtr<FRDGTransientRenderTarget>(MoveTemp(Texture->Allocation)), PassHandle);
					}
				}
				// Texture is using an internal transient texture.
				else
				{
					TransientResourceAllocator->DeallocateMemory(TransientTexture, PassHandle.GetIndex());
				}

				if (!TransientTexture->IsAcquired())
				{
					Texture->MinDiscardPass = FRDGPassHandle(TransientTexture->GetDiscardPasses().Min);
				}
			}
		}
		break;
		}
	}
}

void FRDGBuilder::CreateViews(FRHICommandListBase& InRHICmdList, TConstArrayView<FRDGViewHandle> ViewsToCreate)
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CreateViews", FColor::Magenta);
	for (FRDGViewHandle ViewHandle : ViewsToCreate)
	{
		FRDGView* View = Views[ViewHandle];

		if (!View->ResourceRHI)
		{
			InitViewRHI(InRHICmdList, View);
		}
	}
}

void FRDGBuilder::CreateUniformBuffers(TConstArrayView<FRDGUniformBufferHandle> UniformBuffersToCreate)
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CreateUniformBuffers", FColor::Magenta);
	for (FRDGUniformBufferHandle UniformBufferHandle : UniformBuffersToCreate)
	{
		FRDGUniformBuffer* UniformBuffer = UniformBuffers[UniformBufferHandle];

		if (!UniformBuffer->ResourceRHI)
		{
			UniformBuffer->InitRHI();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::ExecutePassPrologue(FRHIComputeCommandList& RHICmdListPass, FRDGPass* Pass)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDGBuilder_ExecutePassPrologue, GRDGVerboseCSVStats != 0);

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExecutePassBegin(Pass));

#if RDG_CMDLIST_STATS
	if (Pass->bSetCommandListStat)
	{
		RHICmdListPass.SetCurrentStat(Pass->CommandListStat);
	}
#endif

	const ERDGPassFlags PassFlags = Pass->Flags;
	const ERHIPipeline PassPipeline = Pass->Pipeline;

	if (Pass->PrologueBarriersToBegin)
	{
		IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchBegin(Pass, *Pass->PrologueBarriersToBegin));
		Pass->PrologueBarriersToBegin->Submit(RHICmdListPass, PassPipeline);
	}

	IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchEnd(Pass, Pass->PrologueBarriersToEnd));
	Pass->PrologueBarriersToEnd.Submit(RHICmdListPass, PassPipeline);

	if (PassPipeline == ERHIPipeline::AsyncCompute && !Pass->bSentinel && AsyncComputeBudgetState != Pass->AsyncComputeBudget)
	{
		AsyncComputeBudgetState = Pass->AsyncComputeBudget;
		RHICmdListPass.SetAsyncComputeBudget(Pass->AsyncComputeBudget);
	}

	if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::Raster))
	{
		if (!EnumHasAnyFlags(PassFlags, ERDGPassFlags::SkipRenderPass) && !Pass->SkipRenderPassBegin())
		{
			static_cast<FRHICommandList&>(RHICmdListPass).BeginRenderPass(Pass->GetParameters().GetRenderPassInfo(), Pass->GetName());
		}
	}

	BeginUAVOverlap(Pass, RHICmdListPass);
}

void FRDGBuilder::ExecutePassEpilogue(FRHIComputeCommandList& RHICmdListPass, FRDGPass* Pass)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDGBuilder_ExecutePassEpilogue, GRDGVerboseCSVStats != 0);

	EndUAVOverlap(Pass, RHICmdListPass);

	const ERDGPassFlags PassFlags = Pass->Flags;
	const ERHIPipeline PassPipeline = Pass->Pipeline;
	const FRDGParameterStruct PassParameters = Pass->GetParameters();

	if (EnumHasAnyFlags(PassFlags, ERDGPassFlags::Raster) && !EnumHasAnyFlags(PassFlags, ERDGPassFlags::SkipRenderPass) && !Pass->SkipRenderPassEnd())
	{
		static_cast<FRHICommandList&>(RHICmdListPass).EndRenderPass();
	}

	FRDGTransitionQueue Transitions;

	IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchBegin(Pass, Pass->EpilogueBarriersToBeginForGraphics));
	Pass->EpilogueBarriersToBeginForGraphics.Submit(RHICmdListPass, PassPipeline, Transitions);

	if (Pass->EpilogueBarriersToBeginForAsyncCompute)
	{
		IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchBegin(Pass, *Pass->EpilogueBarriersToBeginForAsyncCompute));
		Pass->EpilogueBarriersToBeginForAsyncCompute->Submit(RHICmdListPass, PassPipeline, Transitions);
	}

	if (Pass->EpilogueBarriersToBeginForAll)
	{
		IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchBegin(Pass, *Pass->EpilogueBarriersToBeginForAll));
		Pass->EpilogueBarriersToBeginForAll->Submit(RHICmdListPass, PassPipeline, Transitions);
	}

	for (FRDGBarrierBatchBegin* BarriersToBegin : Pass->SharedEpilogueBarriersToBegin)
	{
		IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchBegin(Pass, *BarriersToBegin));
		BarriersToBegin->Submit(RHICmdListPass, PassPipeline, Transitions);
	}

	if (!Transitions.IsEmpty())
	{
		RHICmdListPass.BeginTransitions(Transitions);
	}

	if (Pass->EpilogueBarriersToEnd)
	{
		IF_RDG_ENABLE_DEBUG(BarrierValidation.ValidateBarrierBatchEnd(Pass, *Pass->EpilogueBarriersToEnd));
		Pass->EpilogueBarriersToEnd->Submit(RHICmdListPass, PassPipeline);
	}

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExecutePassEnd(Pass));
}

void FRDGBuilder::ExecutePass(FRDGPass* Pass, FRHIComputeCommandList& RHICmdListPass)
{
	{
		// Note that we must do this before doing anything with RHICmdList for the pass.
		// For example, if this pass only executes on GPU 1 we want to avoid adding a
		// 0-duration event for this pass on GPU 0's time line.
		SCOPED_GPU_MASK(RHICmdListPass, Pass->GPUMask);

		// Extra scope here to ensure nested ordering of SCOPED_GPU_MASK and FRHICommandListScopedPipeline constructor/destructors
		{
			FRHICommandListScopedPipeline Scope(RHICmdListPass, Pass->Pipeline);

#if 0 // Disabled by default to reduce memory usage in Insights.
			SCOPED_NAMED_EVENT_TCHAR(Pass->GetName(), FColor::Magenta);
#endif

#if RDG_CPU_SCOPES
			if (!Pass->bParallelExecute)
			{
				Pass->CPUScopeOps.Execute();
			}
#endif

			IF_RDG_ENABLE_DEBUG(ConditionalDebugBreak(RDG_BREAKPOINT_PASS_EXECUTE, BuilderName.GetTCHAR(), Pass->GetName()));

			Pass->GPUScopeOpsPrologue.Execute(RHICmdListPass);

			ExecutePassPrologue(RHICmdListPass, Pass);

#if RDG_DUMP_RESOURCES_AT_EACH_DRAW
			BeginPassDump(Pass);
#endif

			Pass->Execute(RHICmdListPass);

#if RDG_DUMP_RESOURCES_AT_EACH_DRAW
			EndPassDump(Pass);
#endif

			ExecutePassEpilogue(RHICmdListPass, Pass);

			Pass->GPUScopeOpsEpilogue.Execute(RHICmdListPass);
		}
	}

	if (!Pass->bParallelExecute && Pass->bDispatchAfterExecute)
	{
		if (Pass->Pipeline == ERHIPipeline::Graphics)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}

	if (GRDGDebugFlushGPU)
	{
		check(!GRDGAsyncCompute && !ParallelExecute.bEnabled);
		RHICmdList.SubmitCommandsAndFlushGPU();
		RHICmdList.BlockUntilGPUIdle();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::CollectAllocations(FCollectResourceContext& Context, FRDGPass* Pass)
{
	for (FRDGPass* PassToBegin : Pass->ResourcesToBegin)
	{
	  for (FRDGPass::FTextureState& PassState : PassToBegin->TextureStates)
	  {
		  CollectAllocateTexture(Context, Pass->Handle, PassState.Texture);
	  }
  
	  for (FRDGPass::FBufferState& PassState : PassToBegin->BufferStates)
	  {
		  CollectAllocateBuffer(Context, Pass->Handle, PassState.Buffer);
	  }
  
	  if (!IsImmediateMode())
	  {
		  for (FRDGUniformBufferHandle UniformBufferHandle : PassToBegin->UniformBuffers)
		  {
			  if (auto BitRef = Context.UniformBufferMap[UniformBufferHandle]; BitRef)
			  {
				  Context.UniformBuffers.Add(UniformBufferHandle);
				  BitRef = false;
			  }
		  }
  
		  for (FRDGViewHandle ViewHandle : PassToBegin->Views)
		  {
			  if (auto BitRef = Context.ViewMap[ViewHandle]; BitRef)
			  {
				  Context.Views.Add(ViewHandle);
				  BitRef = false;
			  }
		  }
	  }
	  else
	  {
		  Context.UniformBuffers = PassToBegin->UniformBuffers;
		  Context.Views = PassToBegin->Views;
	  }
	}
}

void FRDGBuilder::CollectDeallocations(FCollectResourceContext& Context, FRDGPass* Pass)
{
	for (FRDGPass* PassToEnd : Pass->ResourcesToEnd)
	{
		for (FRDGPass::FTextureState& PassState : PassToEnd->TextureStates)
		{
			CollectDeallocateTexture(Context, Pass->Handle, PassState.Texture, PassState.ReferenceCount);
		}

		for (FRDGPass::FBufferState& PassState : PassToEnd->BufferStates)
		{
			CollectDeallocateBuffer(Context, Pass->Handle, PassState.Buffer, PassState.ReferenceCount);
		}
	}
}

void FRDGBuilder::CollectAllocateTexture(FCollectResourceContext& Context, FRDGPassHandle PassHandle, FRDGTextureRef Texture)
{
	check(Texture->ReferenceCount > 0 || Texture->bExternal || IsImmediateMode());

#if RDG_ENABLE_DEBUG
	{
		FRDGPass* Pass = Passes[PassHandle];

		// Cannot begin a resource on an async compute pass.
		check(Pass->Pipeline == ERHIPipeline::Graphics);

		// Cannot begin a resource within a merged render pass region.
		checkf(GetPrologueBarrierPassHandle(PassHandle) == PassHandle,
			TEXT("Cannot begin a resource within a merged render pass. Pass (Handle: %d, Name: %s), Resource %s"), PassHandle.GetIndex(), Pass->GetName(), Texture->Name);
	}
#endif

	if (Texture->FirstPass.IsNull())
	{
		Texture->FirstPass = PassHandle;
	}

	if (Texture->bCollectForAllocate)
	{
		Texture->bCollectForAllocate = false;
		check(!Texture->ResourceRHI);

		const FCollectResourceOp AllocateOp = FCollectResourceOp::Allocate(PassHandle, Texture->Handle);

		if (Texture->bTransient)
		{
			Context.TransientResources.Emplace(AllocateOp);

	#if RDG_STATS
			GRDGStatTransientTextureCount++;
	#endif
		}
		else
		{
			Context.PooledTextures.Emplace(AllocateOp);
		}
	}
}

void FRDGBuilder::CollectDeallocateTexture(FCollectResourceContext& Context, FRDGPassHandle PassHandle, FRDGTexture* Texture, uint32 ReferenceCount)
{
	check(!IsImmediateMode());
	check(Texture->ReferenceCount != FRDGViewableResource::DeallocatedReferenceCount);
	check(Texture->ReferenceCount >= ReferenceCount);
	Texture->ReferenceCount -= ReferenceCount;

	if (Texture->ReferenceCount == 0)
	{
		check(!Texture->bCollectForAllocate);
		const FCollectResourceOp DeallocateOp = FCollectResourceOp::Deallocate(PassHandle, Texture->Handle);

		if (Texture->bTransient)
		{
			Context.TransientResources.Emplace(DeallocateOp);
		}
		else
		{
			Context.PooledTextures.Emplace(DeallocateOp);
		}

		Texture->LastPass = PassHandle;
		Texture->ReferenceCount = FRDGViewableResource::DeallocatedReferenceCount;
	}
}

void FRDGBuilder::CollectAllocateBuffer(FCollectResourceContext& Context, FRDGPassHandle PassHandle, FRDGBuffer* Buffer)
{
	check(Buffer->ReferenceCount > 0 || IsImmediateMode());

#if RDG_ENABLE_DEBUG
	{
		const FRDGPass* Pass = Passes[PassHandle];

		// Cannot begin a resource on an async compute pass.
		check(Pass->Pipeline == ERHIPipeline::Graphics);

		// Cannot begin a resource within a merged render pass region.
		checkf(GetPrologueBarrierPassHandle(PassHandle) == PassHandle,
			TEXT("Cannot begin a resource within a merged render pass. Pass (Handle: %d, Name: %s), Resource %s"), PassHandle.GetIndex(), Pass->GetName(), Buffer->Name);
	}
#endif

	if (Buffer->FirstPass.IsNull())
	{
		Buffer->FirstPass = PassHandle;
	}

	if (Buffer->bCollectForAllocate)
	{
		Buffer->bCollectForAllocate = false;
		check(!Buffer->ResourceRHI);

		const FCollectResourceOp AllocateOp = FCollectResourceOp::Allocate(PassHandle, Buffer->Handle);

		if (Buffer->bTransient)
		{
			Context.TransientResources.Emplace(AllocateOp);

#if RDG_STATS
			GRDGStatTransientBufferCount++;
#endif
		}
		else
		{
			Context.PooledBuffers.Emplace(AllocateOp);
		}
	}
}

void FRDGBuilder::CollectDeallocateBuffer(FCollectResourceContext& Context, FRDGPassHandle PassHandle, FRDGBuffer* Buffer, uint32 ReferenceCount)
{
	check(!IsImmediateMode());
	check(Buffer->ReferenceCount != FRDGViewableResource::DeallocatedReferenceCount);
	check(Buffer->ReferenceCount >= ReferenceCount);
	Buffer->ReferenceCount -= ReferenceCount;

	if (Buffer->ReferenceCount == 0)
	{
		const FCollectResourceOp DeallocateOp = FCollectResourceOp::Deallocate(PassHandle, Buffer->Handle);

		if (Buffer->bTransient)
		{
			Context.TransientResources.Emplace(DeallocateOp);
		}
		else
		{
			Context.PooledBuffers.Emplace(DeallocateOp);
		}

		Buffer->LastPass = PassHandle;
		Buffer->ReferenceCount = FRDGViewableResource::DeallocatedReferenceCount;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::CompilePassBarriers()
{
	// Walk the culled graph and compile barriers for each subresource. Certain transitions are redundant; read-to-read, for example.
	// We can avoid them by traversing and merging compatible states together. The merging states removes a transition, but the merging
	// heuristic is conservative and choosing not to merge doesn't necessarily mean a transition is performed. They are two distinct steps.
	// Merged states track the first and last pass used for all pipelines.

	SCOPED_NAMED_EVENT(CompileBarriers, FColor::Emerald);
	FRDGAllocatorScope AllocatorScope(Allocators.Transition);

	for (FRDGPassHandle PassHandle = GetProloguePassHandle() + 1; PassHandle < GetEpiloguePassHandle(); ++PassHandle)
	{
		FRDGPass* Pass = Passes[PassHandle];

		if (Pass->bCulled)
		{
			continue;
		}

		const ERHIPipeline PassPipeline = Pass->Pipeline;

		const auto MergeSubresourceStates = [&](ERDGViewableResourceType ResourceType, FRDGSubresourceState*& PassMergeState, FRDGSubresourceState*& ResourceMergeState, FRDGSubresourceState* PassState)
		{
			if (!ResourceMergeState || !FRDGSubresourceState::IsMergeAllowed(ResourceType, *ResourceMergeState, *PassState))
			{
				// Use the new pass state as the merge state for future passes.
				ResourceMergeState = PassState;
			}
			else
			{
				// Merge the pass state into the merged state.
				ResourceMergeState->Access |= PassState->Access;

				FRDGPassHandle& FirstPassHandle = ResourceMergeState->FirstPass[PassPipeline];

				if (FirstPassHandle.IsNull())
				{
					FirstPassHandle = PassHandle;
				}

				ResourceMergeState->LastPass[PassPipeline] = PassHandle;
			}

			PassMergeState = ResourceMergeState;
		};

		for (auto& PassState : Pass->TextureStates)
		{
			FRDGTexture* Texture = PassState.Texture;

		#if RDG_STATS
			GRDGStatTextureReferenceCount += PassState.ReferenceCount;
		#endif

			for (int32 Index = 0; Index < PassState.State.Num(); ++Index)
			{
				if (!PassState.State[Index])
				{
					continue;
				}

				MergeSubresourceStates(ERDGViewableResourceType::Texture, PassState.MergeState[Index], Texture->MergeState[Index], PassState.State[Index]);
			}
		}

		for (auto& PassState : Pass->BufferStates)
		{
			FRDGBuffer* Buffer = PassState.Buffer;

		#if RDG_STATS
			GRDGStatBufferReferenceCount += PassState.ReferenceCount;
		#endif

			MergeSubresourceStates(ERDGViewableResourceType::Buffer, PassState.MergeState, Buffer->MergeState, &PassState.State);
		}
	}
}

void FRDGBuilder::CollectPassBarriers()
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CollectBarriers", FColor::Magenta);
	SCOPE_CYCLE_COUNTER(STAT_RDG_CollectBarriersTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDG_CollectBarriers, GRDGVerboseCSVStats != 0);
	FRDGAllocatorScope AllocatorScope(Allocators.Transition);

	for (FRDGPassHandle PassHandle = GetProloguePassHandle() + 1; PassHandle < GetEpiloguePassHandle(); ++PassHandle)
	{
		CollectPassBarriers(PassHandle);
	}
}

void FRDGBuilder::CollectPassBarriers(FRDGPassHandle PassHandle)
{
	IF_RDG_ENABLE_DEBUG(ConditionalDebugBreak(RDG_BREAKPOINT_PASS_COMPILE, BuilderName.GetTCHAR(), Passes[PassHandle]->GetName()));

	FRDGPass* Pass = Passes[PassHandle];

	if (Pass->bCulled || Pass->bEmptyParameters)
	{
		return;
	}


	for (auto& PassState : Pass->TextureStates)
	{
		FRDGTexture* Texture = PassState.Texture;

		AddTextureTransition(PassState.Texture, Texture->State, PassState.MergeState, [Texture] (FRDGSubresourceState* StateAfter, int32 SubresourceIndex)
		{
			if (!Texture->FirstState[SubresourceIndex])
			{
				Texture->FirstState[SubresourceIndex] = StateAfter;
				return IsImmediateMode();
			}
			return true;
		});

		IF_RDG_ENABLE_TRACE(Trace.AddTexturePassDependency(Texture, Pass));
	}

	for (auto& PassState : Pass->BufferStates)
	{
		FRDGBuffer* Buffer = PassState.Buffer;

		AddBufferTransition(PassState.Buffer, Buffer->State, PassState.MergeState, [Buffer] (FRDGSubresourceState* StateAfter)
		{
			if (!Buffer->FirstState)
			{
				Buffer->FirstState = StateAfter;
				Buffer->FirstState->bReservedCommit = Buffer->PendingCommitSize > 0;
				return IsImmediateMode();
			}
			return true;
		});

		IF_RDG_ENABLE_TRACE(Trace.AddBufferPassDependency(Buffer, Pass));
	}
}

void FRDGBuilder::CreatePassBarriers()
{
	struct FTaskContext
	{
		TArray<FRHITransitionInfo, FConcurrentLinearArrayAllocator> Transitions;
	};

	const auto CreateTransition = [this] (FTaskContext& Context, FRDGBarrierBatchBegin* BeginBatch)
	{
		Context.Transitions.Reset(BeginBatch->Transitions.Num());

		for (FRDGTransitionInfo InfoRDG : BeginBatch->Transitions)
		{
			FRHITransitionInfo& InfoRHI = Context.Transitions.Emplace_GetRef();
			InfoRHI.AccessBefore = InfoRDG.AccessBefore;
			InfoRHI.AccessAfter  = InfoRDG.AccessAfter;
			InfoRHI.ArraySlice   = InfoRDG.ArraySlice;
			InfoRHI.MipIndex     = InfoRDG.MipIndex;
			InfoRHI.PlaneSlice   = InfoRDG.PlaneSlice;
			InfoRHI.Flags        = InfoRDG.Flags;

			if (InfoRDG.Type == ERDGViewableResourceType::Texture)
			{
				InfoRHI.Resource = Textures[FRDGTextureHandle(InfoRDG.Handle)]->ResourceRHI;
				InfoRHI.Type = FRHITransitionInfo::EType::Texture;
			}
			else
			{
				FRDGBuffer* Buffer = Buffers[FRDGBufferHandle(InfoRDG.Handle)];

				InfoRHI.Resource = Buffer->ResourceRHI;
				InfoRHI.Type = FRHITransitionInfo::EType::Buffer;

				if (InfoRDG.bReservedCommit)
				{
					InfoRHI.CommitInfo.Emplace(Buffer->PendingCommitSize);
				}
			}
		}

		BeginBatch->CreateTransition(Context.Transitions);
	};

	TArray<FTaskContext, TInlineAllocator<1, FRDGArrayAllocator>> TaskContexts;
	ParallelForWithTaskContext(TEXT("FRDGBuilder::CreatePassBarriers"), TaskContexts, TransitionCreateQueue.Num(), 1, [&](FTaskContext& TaskContext, int32 Index)
	{
		CreateTransition(TaskContext, TransitionCreateQueue[Index]);

	}, ParallelSetup.bEnabled ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);

	TransitionCreateQueue.Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::FinalizeResources()
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::FinalizeResources", FColor::Magenta);
	FRDGAllocatorScope AllocatorScope(Allocators.Transition);

	{
		SCOPED_NAMED_EVENT_TEXT("Textures", FColor::Magenta);
		Textures.Enumerate([&](FRDGTextureRef Texture)
		{
			if (Texture->FirstPass.IsValid())
			{
				if (!IsImmediateMode())
				{
					AddFirstTextureTransition(Texture);
				}

				if (!Texture->bSkipLastTransition)
				{
					AddLastTextureTransition(Texture);
				}
			}

			if (Texture->Allocation)
			{
				ActivePooledTextures.Emplace(MoveTemp(Texture->Allocation));
			}
		});
	}

	{
		SCOPED_NAMED_EVENT_TEXT("Buffers", FColor::Magenta);
		Buffers.Enumerate([&](FRDGBufferRef Buffer)
		{
			if (Buffer->FirstPass.IsValid())
			{
				if (!IsImmediateMode())
				{
					AddFirstBufferTransition(Buffer);
				}

				if (!Buffer->bSkipLastTransition)
				{
					AddLastBufferTransition(Buffer);
				}
			}

			if (Buffer->Allocation)
			{
				ActivePooledBuffers.Emplace(MoveTemp(Buffer->Allocation));
			}
		});
	}

	CreatePassBarriers();
}

void FRDGBuilder::AddFirstTextureTransition(FRDGTexture* Texture)
{
	check(!IsImmediateMode());
	check(Texture->HasRHI());

	FRDGTextureSubresourceState* StateBefore = &ScratchTextureState;
	FRDGSubresourceState& SubresourceStateBefore = *AllocSubresource(FRDGSubresourceState(ERHIPipeline::Graphics, GetProloguePassHandle()));

	if (Texture->PreviousOwner.IsValid())
	{
		// Previous state is the last used state of RDG texture that previously aliased the underlying pooled texture.
		StateBefore = &Textures[Texture->PreviousOwner]->State;

		for (int32 Index = 0; Index < Texture->FirstState.Num(); ++Index)
		{
			// If the new owner doesn't touch the subresource but the previous owner did, pull the previous owner subresource in so that the last transition is respected.
			if (!Texture->FirstState[Index])
			{
				Texture->State[Index] = (*StateBefore)[Index];
			}
			// If the previous owner didn't touch the subresource but the new owner does, assign the prologue subresource state so the first transition is respected.
			else if (!(*StateBefore)[Index])
			{
				(*StateBefore)[Index] = &SubresourceStateBefore;
			}
		}
	}
	else
	{
		if (Texture->MinAcquirePass.IsValid())
		{
			AddAliasingTransition(Texture->MinAcquirePass, Texture->FirstPass, Texture, FRHITransientAliasingInfo::Acquire(Texture->GetRHI(), Texture->AliasingOverlaps));

			SubresourceStateBefore.SetPass(ERHIPipeline::Graphics, Texture->MinAcquirePass);
			SubresourceStateBefore.Access = ERHIAccess::Discard;
		}
		else if (!Texture->bSplitFirstTransition)
		{
			SubresourceStateBefore.SetPass(ERHIPipeline::Graphics, Texture->FirstPass);
		}

		InitTextureSubresources(*StateBefore, Texture->Layout, &SubresourceStateBefore);
	}

	AddTextureTransition(Texture, *StateBefore, Texture->FirstState);

	ScratchTextureState.Reset();
}

void FRDGBuilder::AddLastTextureTransition(FRDGTexture* Texture)
{
	check(IsImmediateMode() || Texture->bExtracted || Texture->ReferenceCount == FRDGViewableResource::DeallocatedReferenceCount);
	check(Texture->HasRHI());

	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	FRDGSubresourceState* SubresourceStateBefore = nullptr;
	FRDGSubresourceState& SubresourceStateAfter = *AllocSubresource();
	SubresourceStateAfter.SetPass(ERHIPipeline::Graphics, EpiloguePassHandle);

	// Texture is using the RHI transient allocator. Transition it back to Discard in the final pass it is used.
	if (Texture->MinDiscardPass.IsValid())
	{
		FRDGPassHandle MaxDiscardPass = FRDGPassHandle(FMath::Min<uint32>(Texture->TransientTexture->GetDiscardPasses().Max, GetEpiloguePassHandle().GetIndex()));
		AddAliasingTransition(Texture->MinDiscardPass, MaxDiscardPass, Texture, FRHITransientAliasingInfo::Discard(Texture->GetRHIUnchecked()));

		SubresourceStateAfter.SetPass(ERHIPipeline::Graphics, MaxDiscardPass);
		SubresourceStateAfter.Access = ERHIAccess::Discard;
	}
	else
	{
		SubresourceStateAfter.Access = Texture->EpilogueAccess;

		// Transient resources stay in the Discard state.
		EpilogueResourceAccesses.Emplace(Texture->GetRHI(), SubresourceStateAfter.Access);
	}

	// Transition any unused (null) sub-resources to the epilogue state since we are assigning a monolithic state across all subresources.
	for (FRDGSubresourceState*& State : Texture->State)
	{
		if (!State)
		{
			if (!SubresourceStateBefore)
			{
				SubresourceStateBefore = AllocSubresource();
				SubresourceStateBefore->SetPass(ERHIPipeline::Graphics, Texture->FirstPass);
			}

			State = SubresourceStateBefore;
		}
	}

	InitTextureSubresources(ScratchTextureState, Texture->Layout, &SubresourceStateAfter);
	AddTextureTransition(Texture, Texture->State, ScratchTextureState);
	ScratchTextureState.Reset();
}

void FRDGBuilder::AddFirstBufferTransition(FRDGBuffer* Buffer)
{
	check(!IsImmediateMode());
	check(Buffer->HasRHI());

	FRDGSubresourceState* StateBefore = nullptr;

	if (Buffer->PreviousOwner.IsValid())
	{
		// Previous state is the last used state of RDG buffer that previously aliased the underlying pooled buffer.
		StateBefore = Buffers[Buffer->PreviousOwner]->State;
	}

	if (!StateBefore)
	{
		StateBefore = AllocSubresource();

		if (Buffer->MinAcquirePass.IsValid())
		{
			AddAliasingTransition(Buffer->MinAcquirePass, Buffer->FirstPass, Buffer, FRHITransientAliasingInfo::Acquire(Buffer->GetRHI(), Buffer->AliasingOverlaps));

			StateBefore->SetPass(ERHIPipeline::Graphics, Buffer->MinAcquirePass);
			StateBefore->Access = ERHIAccess::Discard;
		}
		else if (!Buffer->bSplitFirstTransition)
		{
			StateBefore->SetPass(ERHIPipeline::Graphics, Buffer->FirstPass);
		}
		else
		{
			StateBefore->SetPass(ERHIPipeline::Graphics, GetProloguePassHandle());
		}
	}

	AddBufferTransition(Buffer, StateBefore, Buffer->FirstState);
}

void FRDGBuilder::AddLastBufferTransition(FRDGBuffer* Buffer)
{
	check(IsImmediateMode() || Buffer->bExtracted || Buffer->ReferenceCount == FRDGViewableResource::DeallocatedReferenceCount);
	check(Buffer->HasRHI());

	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	FRDGSubresourceState* StateAfter = AllocSubresource();

	// Texture is using the RHI transient allocator. Transition it back to Discard in the final pass it is used.
	if (Buffer->MinDiscardPass.IsValid())
	{
		FRDGPassHandle MaxDiscardPass = FRDGPassHandle(FMath::Min<uint32>(Buffer->TransientBuffer->GetDiscardPasses().Max, GetEpiloguePassHandle().GetIndex()));
		AddAliasingTransition(Buffer->MinDiscardPass, MaxDiscardPass, Buffer, FRHITransientAliasingInfo::Discard(Buffer->GetRHIUnchecked()));

		StateAfter->SetPass(ERHIPipeline::Graphics, MaxDiscardPass);
		StateAfter->Access = ERHIAccess::Discard;
	}
	else
	{
		StateAfter->SetPass(ERHIPipeline::Graphics, EpiloguePassHandle);
		StateAfter->Access = Buffer->EpilogueAccess;

		EpilogueResourceAccesses.Emplace(Buffer->GetRHI(), StateAfter->Access);
	}

	AddBufferTransition(Buffer, Buffer->State, StateAfter);
}

template <typename FilterSubresourceLambdaType>
void FRDGBuilder::AddTextureTransition(FRDGTexture* Texture, FRDGTextureSubresourceState& StateBefore, FRDGTextureSubresourceState& StateAfter, FilterSubresourceLambdaType&& FilterSubresourceLambda)
{
	const FRDGTextureSubresourceLayout Layout = Texture->Layout;
	const uint32 SubresourceCount = Texture->SubresourceCount;

	check(SubresourceCount == Layout.GetSubresourceCount() && StateBefore.Num() == StateAfter.Num());

	if (!GRHISupportsSeparateDepthStencilCopyAccess && Texture->Desc.Format == PF_DepthStencil)
	{
		// Certain RHIs require a fused depth / stencil copy state. For any mip / slice transition involving a copy state,
		// adjust the split transitions so both subresources are transitioned using the same barrier batch (i.e. the RHI transition).
		// Note that this is only possible when async compute is disabled, as it's not possible to merge transitions from different pipes.
		// There are two cases to correct (D for depth, S for stencil, horizontal axis is time):
		//
		// Case 1: both states transitioning from previous states on passes A and B to a copy state at pass C.
		//
		// [Pass] A     B     C                         A     B     C
		// [D]          X --> X      Corrected To:            X --> X
		// [S]    X --------> X                               X --> X (S is pushed forward to transition with D on pass B)
		//
		// Case 2a|b: one plane transitioning out of a copy state on pass A to pass B (this pass), but the other is not transitioning yet.
		//
		// [Pass] A     B     ?                         A     B
		// [D]    X --> X            Corrected To:      X --> X
		// [S]    X --------> X                         X --> X (S's state is unknown, so it transitions with D and matches D's state).

		const ERHIPipeline GraphicsPipe = ERHIPipeline::Graphics;
		const uint32 NumSlicesAndMips = Layout.NumMips * Layout.NumArraySlices;

		for (uint32 DepthIndex = 0, StencilIndex = NumSlicesAndMips; DepthIndex < NumSlicesAndMips; ++DepthIndex, ++StencilIndex)
		{
			FRDGSubresourceState*& DepthStateAfter   = StateAfter[DepthIndex];
			FRDGSubresourceState*& StencilStateAfter = StateAfter[StencilIndex];

			// Skip if neither depth nor stencil are being transitioned.
			if (!DepthStateAfter && !StencilStateAfter)
			{
				continue;
			}

			FRDGSubresourceState*& DepthStateBefore   = StateBefore[DepthIndex];
			FRDGSubresourceState*& StencilStateBefore = StateBefore[StencilIndex];

			// Case 1: transitioning into a fused copy state.
			if (DepthStateAfter && EnumHasAnyFlags(DepthStateAfter->Access, ERHIAccess::CopySrc | ERHIAccess::CopyDest))
			{
				check(StencilStateAfter && StencilStateAfter->Access == DepthStateAfter->Access);

				const FRDGPassHandle MaxPassHandle = FRDGPassHandle::Max(DepthStateBefore->LastPass[GraphicsPipe], StencilStateBefore->LastPass[GraphicsPipe]);

				DepthStateBefore = AllocSubresource(*DepthStateBefore);
				DepthStateAfter  = AllocSubresource(*DepthStateAfter);

				DepthStateBefore->LastPass[GraphicsPipe]   = MaxPassHandle;
				StencilStateBefore->LastPass[GraphicsPipe] = MaxPassHandle;
			}
			// Case 2: transitioning out of a fused copy state.
			else if (DepthStateBefore && EnumHasAnyFlags(DepthStateBefore->Access, ERHIAccess::CopySrc | ERHIAccess::CopyDest))
			{
				check(StencilStateBefore->Access        == DepthStateBefore->Access);
				check(StencilStateBefore->GetLastPass() == DepthStateBefore->GetLastPass());

				// Case 2a: depth unknown, so transition to match stencil.
				if (!DepthStateAfter)
				{
					DepthStateAfter = AllocSubresource(*StencilStateAfter);
				}
				// Case 2b: stencil unknown, so transition to match depth.
				else if (!StencilStateAfter)
				{
					StencilStateAfter = AllocSubresource(*DepthStateAfter);
				}
			}
		}
	}

	for (uint32 SubresourceIndex = 0; SubresourceIndex < SubresourceCount; ++SubresourceIndex)
	{
		FRDGSubresourceState*& SubresourceStateBefore = StateBefore[SubresourceIndex];
		FRDGSubresourceState* SubresourceStateAfter = StateAfter[SubresourceIndex];

		if (!SubresourceStateAfter)
		{
			continue;
		}

		if (FilterSubresourceLambda(SubresourceStateAfter, SubresourceIndex))
		{
			check(SubresourceStateAfter->Access != ERHIAccess::Unknown);

			if (SubresourceStateBefore && FRDGSubresourceState::IsTransitionRequired(*SubresourceStateBefore, *SubresourceStateAfter))
			{
				const FRDGTextureSubresource Subresource = Layout.GetSubresource(SubresourceIndex);

				FRDGTransitionInfo Info;
				Info.AccessBefore = SubresourceStateBefore->Access;
				Info.AccessAfter = SubresourceStateAfter->Access;
				Info.Handle = Texture->Handle.GetIndex();
				Info.Type = ERDGViewableResourceType::Texture;
				Info.Flags = SubresourceStateAfter->Flags;
				Info.ArraySlice = Subresource.ArraySlice;
				Info.MipIndex = Subresource.MipIndex;
				Info.PlaneSlice = Subresource.PlaneSlice;
				Info.bReservedCommit = 0;

				if (Info.AccessBefore == ERHIAccess::Discard)
				{
					Info.Flags |= EResourceTransitionFlags::Discard;
				}

				AddTransition(Texture, *SubresourceStateBefore, *SubresourceStateAfter, Info);
			}
		}

		SubresourceStateBefore = SubresourceStateAfter;
	}
}

template <typename FilterSubresourceLambdaType>
void FRDGBuilder::AddBufferTransition(FRDGBufferRef Buffer, FRDGSubresourceState*& StateBefore, FRDGSubresourceState* StateAfter, FilterSubresourceLambdaType&& FilterSubresourceLambda)
{
	check(StateAfter);
	check(StateAfter->Access != ERHIAccess::Unknown);

	if (FilterSubresourceLambda(StateAfter))
	{
		check(StateBefore);

		if (FRDGSubresourceState::IsTransitionRequired(*StateBefore, *StateAfter))
		{
			FRDGTransitionInfo Info;
			Info.AccessBefore = StateBefore->Access;
			Info.AccessAfter = StateAfter->Access;
			Info.Handle = Buffer->Handle.GetIndex();
			Info.Type = ERDGViewableResourceType::Buffer;
			Info.Flags = StateAfter->Flags;
			Info.ArraySlice = 0;
			Info.MipIndex = 0;
			Info.PlaneSlice = 0;
			Info.bReservedCommit = StateAfter->bReservedCommit;

			AddTransition(Buffer, *StateBefore, *StateAfter, Info);
		}
	}

	StateBefore = StateAfter;
}

void FRDGBuilder::AddTransition(
	FRDGViewableResource* Resource,
	FRDGSubresourceState StateBefore,
	FRDGSubresourceState StateAfter,
	FRDGTransitionInfo TransitionInfo)
{
	const ERHIPipeline Graphics = ERHIPipeline::Graphics;
	const ERHIPipeline AsyncCompute = ERHIPipeline::AsyncCompute;

#if RDG_ENABLE_DEBUG
	StateBefore.Validate();
	StateAfter.Validate();
#endif

	if (IsImmediateMode())
	{
		// Immediate mode simply enqueues the barrier into the 'after' pass. Everything is on the graphics pipe.
		AddToPrologueBarriers(StateAfter.FirstPass[Graphics], [&](FRDGBarrierBatchBegin& Barriers)
		{
			Barriers.AddTransition(Resource, TransitionInfo);
		});
		return;
	}

	const ERHIPipeline PipelinesBefore = StateBefore.GetPipelines();
	const ERHIPipeline PipelinesAfter = StateAfter.GetPipelines();

	check(PipelinesBefore != ERHIPipeline::None && PipelinesAfter != ERHIPipeline::None);
	checkf(StateBefore.GetLastPass() <= StateAfter.GetFirstPass(), TEXT("Submitted a state for '%s' that begins before our previous state has ended."), Resource->Name);

	const FRDGPassHandlesByPipeline& PassesBefore = StateBefore.LastPass;
	const FRDGPassHandlesByPipeline& PassesAfter = StateAfter.FirstPass;

	// 1-to-1 or 1-to-N pipe transition.
	if (PipelinesBefore != ERHIPipeline::All)
	{
		const FRDGPassHandle BeginPassHandle = StateBefore.GetLastPass();
		const FRDGPassHandle FirstEndPassHandle = StateAfter.GetFirstPass();

		FRDGPass* BeginPass = nullptr;
		FRDGBarrierBatchBegin* BarriersToBegin = nullptr;

		// Issue the begin in the epilogue of the begin pass if the barrier is being split across multiple passes or the barrier end is in the epilogue.
		if (BeginPassHandle < FirstEndPassHandle)
		{
			BeginPass = GetEpilogueBarrierPass(BeginPassHandle);
			BarriersToBegin = &BeginPass->GetEpilogueBarriersToBeginFor(Allocators.Transition, TransitionCreateQueue, PipelinesAfter);
		}
		// This is an immediate prologue transition in the same pass. Issue the begin in the prologue.
		else
		{
			checkf(PipelinesAfter == ERHIPipeline::Graphics,
				TEXT("Attempted to queue an immediate async pipe transition for %s. Pipelines: %s. Async transitions must be split."),
				Resource->Name, *GetRHIPipelineName(PipelinesAfter));

			BeginPass = GetPrologueBarrierPass(BeginPassHandle);
			BarriersToBegin = &BeginPass->GetPrologueBarriersToBegin(Allocators.Transition, TransitionCreateQueue);
		}

		BarriersToBegin->AddTransition(Resource, TransitionInfo);

		for (ERHIPipeline Pipeline : GetRHIPipelines())
		{
			/** If doing a 1-to-N transition and this is the same pipe as the begin, we end it immediately afterwards in the epilogue
			 *  of the begin pass. This is because we can't guarantee that the other pipeline won't join back before the end. This can
			 *  happen if the forking async compute pass joins back to graphics (via another independent transition) before the current
			 *  graphics transition is ended.
			 *
			 *  Async Compute Pipe:               EndA  BeginB
			 *                                   /            \
			 *  Graphics Pipe:            BeginA               EndB   EndA
			 *
			 *  A is our 1-to-N transition and B is a future transition of the same resource that we haven't evaluated yet. Instead, the
			 *  same pipe End is performed in the epilogue of the begin pass, which removes the spit barrier but simplifies the tracking:
			 *
			 *  Async Compute Pipe:               EndA  BeginB
			 *                                   /            \
			 *  Graphics Pipe:            BeginA  EndA         EndB
			 */
			if ((PipelinesBefore == Pipeline && PipelinesAfter == ERHIPipeline::All))
			{
				AddToEpilogueBarriersToEnd(BeginPassHandle, *BarriersToBegin);
			}
			else if (EnumHasAnyFlags(PipelinesAfter, Pipeline))
			{
				AddToPrologueBarriersToEnd(PassesAfter[Pipeline], *BarriersToBegin);
			}
		}
	}
	// N-to-1 or N-to-N transition.
	else
	{
		checkf(StateBefore.GetLastPass() != StateAfter.GetFirstPass(),
			TEXT("Attempted to queue a transition for resource '%s' from '%s' to '%s', but previous and next passes are the same on one pipe."),
			Resource->Name, *GetRHIPipelineName(PipelinesBefore), *GetRHIPipelineName(PipelinesAfter));

		FRDGBarrierBatchBeginId Id;
		Id.PipelinesAfter = PipelinesAfter;
		for (ERHIPipeline Pipeline : GetRHIPipelines())
		{
			Id.Passes[Pipeline] = GetEpilogueBarrierPassHandle(PassesBefore[Pipeline]);
		}

		FRDGBarrierBatchBegin*& BarriersToBegin = BarrierBatchMap.FindOrAdd(Id);

		if (!BarriersToBegin)
		{
			FRDGPassesByPipeline BarrierBatchPasses;
			BarrierBatchPasses[Graphics]     = Passes[Id.Passes[Graphics]];
			BarrierBatchPasses[AsyncCompute] = Passes[Id.Passes[AsyncCompute]];

			BarriersToBegin = Allocators.Transition.AllocNoDestruct<FRDGBarrierBatchBegin>(PipelinesBefore, PipelinesAfter, GetEpilogueBarriersToBeginDebugName(PipelinesAfter), BarrierBatchPasses);
			TransitionCreateQueue.Emplace(BarriersToBegin);

			for (FRDGPass* Pass : BarrierBatchPasses)
			{
				Pass->SharedEpilogueBarriersToBegin.Add(BarriersToBegin);
			}
		}

		BarriersToBegin->AddTransition(Resource, TransitionInfo);

		for (ERHIPipeline Pipeline : GetRHIPipelines())
		{
			if (EnumHasAnyFlags(PipelinesAfter, Pipeline))
			{
				AddToPrologueBarriersToEnd(PassesAfter[Pipeline], *BarriersToBegin);
			}
		}
	}
}

void FRDGBuilder::AddAliasingTransition(FRDGPassHandle BeginPassHandle, FRDGPassHandle EndPassHandle, FRDGViewableResource* Resource, const FRHITransientAliasingInfo& Info)
{
	check(BeginPassHandle <= EndPassHandle);

	FRDGBarrierBatchBegin* BarriersToBegin{};
	FRDGPass* EndPass{};

	if (BeginPassHandle == EndPassHandle)
	{
		FRDGPass* BeginPass = Passes[BeginPassHandle];
		EndPass = BeginPass;

		check(GetPrologueBarrierPassHandle(BeginPassHandle) == BeginPassHandle);

		BarriersToBegin = &BeginPass->GetPrologueBarriersToBegin(Allocators.Transition, TransitionCreateQueue);
	}
	else
	{
		FRDGPass* BeginPass = GetEpilogueBarrierPass(BeginPassHandle);
		EndPass = Passes[EndPassHandle];

		check(GetPrologueBarrierPassHandle(EndPassHandle) == EndPassHandle);

		BarriersToBegin = &BeginPass->GetEpilogueBarriersToBeginFor(Allocators.Transition, TransitionCreateQueue, EndPass->GetPipeline());
	}

	BarriersToBegin->AddAlias(Resource, Info);
	EndPass->GetPrologueBarriersToEnd(Allocators.Transition).AddDependency(BarriersToBegin);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

TRefCountPtr<IPooledRenderTarget> FRDGBuilder::AllocatePooledRenderTargetRHI(FRHICommandListBase& InRHICmdList, FRDGTextureRef Texture)
{
	return GRenderTargetPool.FindFreeElement(InRHICmdList, Texture->Desc, Texture->Name);
}

TRefCountPtr<FRDGPooledBuffer> FRDGBuilder::AllocatePooledBufferRHI(FRHICommandListBase& InRHICmdList, FRDGBufferRef Buffer)
{
	Buffer->FinalizeDesc();
	return GRenderGraphResourcePool.FindFreeBuffer(InRHICmdList, Buffer->Desc, Buffer->Name);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::SetPooledRenderTargetRHI(FRDGTexture* Texture, IPooledRenderTarget* RenderTarget)
{
	Texture->RenderTarget = RenderTarget;

	if (FRHITransientTexture* TransientTexture = RenderTarget->GetTransientTexture())
	{
		FRDGTransientRenderTarget* TransientRenderTarget = static_cast<FRDGTransientRenderTarget*>(RenderTarget);
		Texture->Allocation = TRefCountPtr<FRDGTransientRenderTarget>(TransientRenderTarget);

		SetTransientTextureRHI(Texture, TransientTexture);
	}
	else
	{
		FPooledRenderTarget* PooledRenderTarget = static_cast<FPooledRenderTarget*>(RenderTarget);
		Texture->Allocation = TRefCountPtr<FPooledRenderTarget>(PooledRenderTarget);

		SetPooledTextureRHI(Texture, &PooledRenderTarget->PooledTexture);
	}
}

void FRDGBuilder::SetPooledTextureRHI(FRDGTexture* Texture, FRDGPooledTexture* PooledTexture)
{
	check(!Texture->ResourceRHI);

	FRHITexture* TextureRHI = PooledTexture->GetRHI();

	Texture->ResourceRHI = TextureRHI;
	Texture->PooledTexture = PooledTexture;
	Texture->ViewCache = &PooledTexture->ViewCache;

	FRDGTexture*& Owner = *PooledTextureOwnershipMap.FindOrAdd(PooledTexture, nullptr);

	// Link the previous alias to this one.
	if (Owner)
	{
		Texture->PreviousOwner = Owner->Handle;
		Owner->NextOwner = Texture->Handle;
		Owner->bSkipLastTransition = true;
	}

	Owner = Texture;
}

void FRDGBuilder::SetTransientTextureRHI(FRDGTexture* Texture, FRHITransientTexture* TransientTexture)
{
	check(!Texture->ResourceRHI);

	Texture->ResourceRHI = TransientTexture->GetRHI();
	Texture->TransientTexture = TransientTexture;
	Texture->ViewCache = &TransientTexture->ViewCache;
	Texture->bTransient = true;
	Texture->AliasingOverlaps = TransientTexture->GetAliasingOverlaps();
}

void FRDGBuilder::SetPooledBufferRHI(FRDGBuffer* Buffer, FRDGPooledBuffer* PooledBuffer)
{
	check(!Buffer->ResourceRHI);

	FRHIBuffer* BufferRHI = PooledBuffer->GetRHI();

	Buffer->ResourceRHI = BufferRHI;
	Buffer->PooledBuffer = PooledBuffer;
	Buffer->ViewCache = &PooledBuffer->ViewCache;
	Buffer->Allocation = PooledBuffer;

#if RHI_ENABLE_RESOURCE_INFO
	Buffer->ResourceRHI->SetOwnerName(Buffer->OwnerName);
#endif

	FRDGBuffer*& Owner = *PooledBufferOwnershipMap.FindOrAdd(PooledBuffer, nullptr);

	// Link the previous owner to this one.
	if (Owner)
	{
		Buffer->PreviousOwner = Owner->Handle;
		Owner->NextOwner = Buffer->Handle;
		Owner->bSkipLastTransition = true;
	}

	Owner = Buffer;
}

void FRDGBuilder::SetTransientBufferRHI(FRDGBuffer* Buffer, FRHITransientBuffer* TransientBuffer)
{
	check(!Buffer->ResourceRHI && Buffer->bTransient);

	Buffer->ResourceRHI = TransientBuffer->GetRHI();
	Buffer->TransientBuffer = TransientBuffer;
	Buffer->ViewCache = &TransientBuffer->ViewCache;
	Buffer->AliasingOverlaps = TransientBuffer->GetAliasingOverlaps();

#if RHI_ENABLE_RESOURCE_INFO
	Buffer->ResourceRHI->SetOwnerName(Buffer->OwnerName);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::InitTextureViewRHI(FRHICommandListBase& InRHICmdList, FRDGTextureSRVRef SRV)
{
	check(SRV && !SRV->ResourceRHI);

	FRDGTextureRef Texture = SRV->Desc.Texture;
	FRHITexture* TextureRHI = Texture->GetRHIUnchecked();
	check(TextureRHI);

	SRV->ResourceRHI = Texture->ViewCache->GetOrCreateSRV(InRHICmdList, TextureRHI, SRV->Desc);
}

void FRDGBuilder::InitTextureViewRHI(FRHICommandListBase& InRHICmdList, FRDGTextureUAVRef UAV)
{
	check(UAV && !UAV->ResourceRHI);

	FRDGTextureRef Texture = UAV->Desc.Texture;
	FRHITexture* TextureRHI = Texture->GetRHIUnchecked();
	check(TextureRHI);

	UAV->ResourceRHI = Texture->ViewCache->GetOrCreateUAV(InRHICmdList, TextureRHI, UAV->Desc);
}

void FRDGBuilder::InitBufferViewRHI(FRHICommandListBase& InRHICmdList, FRDGBufferSRVRef SRV)
{
	check(SRV);

	if (SRV->HasRHI())
	{
		return;
	}

	FRDGBufferRef Buffer = SRV->Desc.Buffer;
	FRHIBuffer* BufferRHI = Buffer->GetRHIUnchecked();
	check(BufferRHI);

	FRHIBufferSRVCreateInfo SRVCreateInfo = SRV->Desc;

	if (EnumHasAnyFlags(Buffer->Desc.Usage, EBufferUsageFlags::StructuredBuffer))
	{
		// RDG allows structured buffer views to be typed, but the view creation logic requires that it
		// be unknown (as do platform APIs -- structured buffers are not typed). This could be validated
		// at the high level but the current API makes it confusing. For now, it's considered a no-op.
		SRVCreateInfo.Format = PF_Unknown;
	}

	SRV->ResourceRHI = Buffer->ViewCache->GetOrCreateSRV(InRHICmdList, BufferRHI, SRVCreateInfo);
}

void FRDGBuilder::InitBufferViewRHI(FRHICommandListBase& InRHICmdList, FRDGBufferUAV* UAV)
{
	check(UAV);

	if (UAV->HasRHI())
	{
		return;
	}

	FRDGBufferRef Buffer = UAV->Desc.Buffer;
	check(Buffer);

	FRHIBufferUAVCreateInfo UAVCreateInfo = UAV->Desc;

	if (EnumHasAnyFlags(Buffer->Desc.Usage, EBufferUsageFlags::StructuredBuffer))
	{
		// RDG allows structured buffer views to be typed, but the view creation logic requires that it
		// be unknown (as do platform APIs -- structured buffers are not typed). This could be validated
		// at the high level but the current API makes it confusing. For now, it's considered a no-op.
		UAVCreateInfo.Format = PF_Unknown;
	}

	UAV->ResourceRHI = Buffer->ViewCache->GetOrCreateUAV(InRHICmdList, Buffer->GetRHIUnchecked(), UAVCreateInfo);
}

void FRDGBuilder::InitViewRHI(FRHICommandListBase& InRHICmdList, FRDGView* View)
{
	check(!View->ResourceRHI);

	switch (View->Type)
	{
	case ERDGViewType::TextureUAV:
		InitTextureViewRHI(InRHICmdList, static_cast<FRDGTextureUAV*>(View));
		break;
	case ERDGViewType::TextureSRV:
		InitTextureViewRHI(InRHICmdList, static_cast<FRDGTextureSRV*>(View));
		break;
	case ERDGViewType::BufferUAV:
		InitBufferViewRHI(InRHICmdList, static_cast<FRDGBufferUAV*>(View));
		break;
	case ERDGViewType::BufferSRV:
		InitBufferViewRHI(InRHICmdList, static_cast<FRDGBufferSRV*>(View));
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if RDG_ENABLE_DEBUG

void FRDGBuilder::VisualizePassOutputs(const FRDGPass* Pass)
{
#if SUPPORTS_VISUALIZE_TEXTURE
	if (!AuxiliaryPasses.IsVisualizeAllowed())
	{
		return;
	}

	RDG_RECURSION_COUNTER_SCOPE(AuxiliaryPasses.Visualize);

	Pass->GetParameters().EnumerateTextures([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RDG_TEXTURE_ACCESS:
		{
			if (FRDGTextureAccess TextureAccess = Parameter.GetAsTextureAccess())
			{
				if (IsWritableAccess(TextureAccess.GetAccess()))
				{
					if (TOptional<uint32> CaptureId = GVisualizeTexture.ShouldCapture(TextureAccess->Name, /* MipIndex = */ 0))
					{
						GVisualizeTexture.CreateContentCapturePass(*this, TextureAccess.GetTexture(), *CaptureId);
					}
				}
			}
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
		{
			const FRDGTextureAccessArray& TextureAccessArray = Parameter.GetAsTextureAccessArray();

			for (FRDGTextureAccess TextureAccess : TextureAccessArray)
			{
				if (IsWritableAccess(TextureAccess.GetAccess()))
				{
					if (TOptional<uint32> CaptureId = GVisualizeTexture.ShouldCapture(TextureAccess->Name, /* MipIndex = */ 0))
					{
						GVisualizeTexture.CreateContentCapturePass(*this, TextureAccess.GetTexture(), *CaptureId);
					}
				}
			}
		}
		break;
		case UBMT_RDG_TEXTURE_UAV:
		{
			if (FRDGTextureUAVRef UAV = Parameter.GetAsTextureUAV())
			{
				FRDGTextureRef Texture = UAV->Desc.Texture;
				if (TOptional<uint32> CaptureId = GVisualizeTexture.ShouldCapture(Texture->Name, UAV->Desc.MipLevel))
				{
					GVisualizeTexture.CreateContentCapturePass(*this, Texture, *CaptureId);
				}
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			const FRenderTargetBindingSlots& RenderTargets = Parameter.GetAsRenderTargetBindingSlots();

			RenderTargets.Enumerate([&](FRenderTargetBinding RenderTarget)
			{
				FRDGTextureRef Texture = RenderTarget.GetTexture();
				if (TOptional<uint32> CaptureId = GVisualizeTexture.ShouldCapture(Texture->Name, RenderTarget.GetMipIndex()))
				{
					GVisualizeTexture.CreateContentCapturePass(*this, Texture, *CaptureId);
				}
			});

			const FDepthStencilBinding& DepthStencil = RenderTargets.DepthStencil;

			if (FRDGTextureRef Texture = DepthStencil.GetTexture())
			{
				const bool bHasStoreAction = DepthStencil.GetDepthStencilAccess().IsAnyWrite();

				if (bHasStoreAction)
				{
					const uint32 MipIndex = 0;
					if (TOptional<uint32> CaptureId = GVisualizeTexture.ShouldCapture(Texture->Name, MipIndex))
					{
						GVisualizeTexture.CreateContentCapturePass(*this, Texture, *CaptureId);
					}
				}
			}
		}
		break;
		}
	});
#endif
}

void FRDGBuilder::ClobberPassOutputs(const FRDGPass* Pass)
{
	if (!GRDGValidation || !GRDGClobberResources || !AuxiliaryPasses.IsClobberAllowed())
	{
		return;
	}

	RDG_RECURSION_COUNTER_SCOPE(AuxiliaryPasses.Clobber);
	RDG_EVENT_SCOPE(*this, "RDG ClobberResources");

	const FLinearColor ClobberColor = GetClobberColor();

	const auto ClobberTextureUAV = [&](FRDGTextureUAV* TextureUAV)
	{
		if (IsInteger(TextureUAV->GetParent()->Desc.Format))
		{
			AddClearUAVPass(*this, TextureUAV, GetClobberBufferValue());
		}
		else
		{
			AddClearUAVPass(*this, TextureUAV, ClobberColor);
		}
	};

	const auto ClobberTextureAccess = [&](FRDGTextureAccess TextureAccess)
	{
		if (IsWritableAccess(TextureAccess.GetAccess()))
		{
			FRDGTextureRef Texture = TextureAccess.GetTexture();
	
			if (Texture && UserValidation.TryMarkForClobber(Texture))
			{
				if (EnumHasAnyFlags(TextureAccess.GetAccess(), ERHIAccess::UAVMask))
				{
					for (int32 MipLevel = 0; MipLevel < Texture->Desc.NumMips; MipLevel++)
					{
						ClobberTextureUAV(CreateUAV(FRDGTextureUAVDesc(Texture, MipLevel)));
					}
				}
				else if (EnumHasAnyFlags(TextureAccess.GetAccess(), ERHIAccess::RTV))
				{
					AddClearRenderTargetPass(*this, Texture, ClobberColor);
				}
			}
		}
	};

	const auto ClobberBufferAccess = [&](FRDGBufferAccess BufferAccess)
	{
		if (IsWritableAccess(BufferAccess.GetAccess()))
		{
			FRDGBufferRef Buffer = BufferAccess.GetBuffer();
	
			if (Buffer && UserValidation.TryMarkForClobber(Buffer))
			{
				AddClearUAVPass(*this, CreateUAV(Buffer), GetClobberBufferValue());
			}
		}
	};

	Pass->GetParameters().Enumerate([&](FRDGParameter Parameter)
	{
		switch (Parameter.GetType())
		{
		case UBMT_RDG_BUFFER_UAV:
		{
			if (FRDGBufferUAVRef UAV = Parameter.GetAsBufferUAV())
			{
				FRDGBufferRef Buffer = UAV->GetParent();

				if (UserValidation.TryMarkForClobber(Buffer))
				{
					AddClearUAVPass(*this, UAV, GetClobberBufferValue());
				}
			}
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS:
		{
			ClobberTextureAccess(Parameter.GetAsTextureAccess());
		}
		break;
		case UBMT_RDG_TEXTURE_ACCESS_ARRAY:
		{
			const FRDGTextureAccessArray& TextureAccessArray = Parameter.GetAsTextureAccessArray();

			for (FRDGTextureAccess TextureAccess : TextureAccessArray)
			{
				ClobberTextureAccess(TextureAccess);
			}
		}
		break;
		case UBMT_RDG_BUFFER_ACCESS:
		{
			ClobberBufferAccess(Parameter.GetAsBufferAccess());
		}
		break;
		case UBMT_RDG_BUFFER_ACCESS_ARRAY:
		{
			const FRDGBufferAccessArray& BufferAccessArray = Parameter.GetAsBufferAccessArray();

			for (FRDGBufferAccess BufferAccess : BufferAccessArray)
			{
				ClobberBufferAccess(BufferAccess);
			}
		}
		break;
		case UBMT_RDG_TEXTURE_UAV:
		{
			if (FRDGTextureUAVRef UAV = Parameter.GetAsTextureUAV())
			{
				FRDGTextureRef Texture = UAV->GetParent();

				if (UserValidation.TryMarkForClobber(Texture))
				{
					if (Texture->Desc.NumMips == 1)
					{
						ClobberTextureUAV(UAV);
					}
					else
					{
						for (int32 MipLevel = 0; MipLevel < Texture->Desc.NumMips; MipLevel++)
						{
							ClobberTextureUAV(CreateUAV(FRDGTextureUAVDesc(Texture, MipLevel)));
						}
					}
				}
			}
		}
		break;
		case UBMT_RENDER_TARGET_BINDING_SLOTS:
		{
			const FRenderTargetBindingSlots& RenderTargets = Parameter.GetAsRenderTargetBindingSlots();

			RenderTargets.Enumerate([&](FRenderTargetBinding RenderTarget)
			{
				FRDGTextureRef Texture = RenderTarget.GetTexture();

				if (UserValidation.TryMarkForClobber(Texture))
				{
					AddClearRenderTargetPass(*this, Texture, ClobberColor);
				}
			});

			if (FRDGTextureRef Texture = RenderTargets.DepthStencil.GetTexture())
			{
				if (UserValidation.TryMarkForClobber(Texture))
				{
					AddClearDepthStencilPass(*this, Texture, true, GetClobberDepth(), true, GetClobberStencil());
				}
			}
		}
		break;
		}
	});
}

#endif //! RDG_ENABLE_DEBUG

#if WITH_MGPU
void FRDGBuilder::ForceCopyCrossGPU()
{
	const auto GetLastProducerGPUMask = [](FRDGProducerStatesByPipeline& LastProducers) -> TOptional<FRHIGPUMask>
	{
		for (const FRDGProducerState& LastProducer : LastProducers)
		{
			if (LastProducer.Pass && !LastProducer.Pass->bCulled)
			{
				return LastProducer.Pass->GPUMask;
			}
		}
		return {};
	};

	Experimental::TRobinHoodHashMap<FRHIBuffer*, FRHIGPUMask, DefaultKeyFuncs<FRHIBuffer*>, FRDGArrayAllocator> BuffersToTransfer;
	BuffersToTransfer.Reserve(ExternalBuffers.Num());

	for (auto& ExternalBuffer : ExternalBuffers)
	{
		FRHIBuffer* BufferRHI = ExternalBuffer.Key;
		FRDGBuffer* BufferRDG = ExternalBuffer.Value;

		if (!EnumHasAnyFlags(BufferRDG->Desc.Usage, BUF_MultiGPUAllocate | BUF_MultiGPUGraphIgnore))
		{
			TOptional<FRHIGPUMask> GPUMask = GetLastProducerGPUMask(BufferRDG->LastProducer);

			if (GPUMask)
			{
				BuffersToTransfer.FindOrAdd(BufferRHI, *GPUMask);
			}
		}
	}

	Experimental::TRobinHoodHashMap<FRHITexture*, FRHIGPUMask, DefaultKeyFuncs<FRHITexture*>, FRDGArrayAllocator> TexturesToTransfer;
	TexturesToTransfer.Reserve(ExternalTextures.Num());

	for (auto& ExternalTexture : ExternalTextures)
	{
		FRHITexture* TextureRHI = ExternalTexture.Key;
		FRDGTexture* TextureRDG = ExternalTexture.Value;

		if (!EnumHasAnyFlags(TextureRDG->Desc.Flags, TexCreate_MultiGPUGraphIgnore))
		{
			for (auto& LastProducer : TextureRDG->LastProducers)
			{
				TOptional<FRHIGPUMask> GPUMask = GetLastProducerGPUMask(LastProducer);

				if (GPUMask)
				{
					TexturesToTransfer.FindOrAdd(TextureRHI, *GPUMask);
					break;
				}
			}
		}
	}

	// Now that we've got the list of external resources, and the GPU they were last written to, make a list of what needs to
	// be propagated to other GPUs.
	TArray<FTransferResourceParams, FRDGArrayAllocator> Transfers;
	Transfers.Reserve(BuffersToTransfer.Num() + TexturesToTransfer.Num());
	const FRHIGPUMask AllGPUMask = FRHIGPUMask::All();
	const bool bPullData = false;
	const bool bLockstepGPUs = true;

	for (auto& KeyValue : BuffersToTransfer)
	{
		FRHIBuffer* Buffer  = KeyValue.Key;
		FRHIGPUMask GPUMask = KeyValue.Value;

		for (uint32 GPUIndex : AllGPUMask)
		{
			if (!GPUMask.Contains(GPUIndex))
			{
				Transfers.Add(FTransferResourceParams(Buffer, GPUMask.GetFirstIndex(), GPUIndex, bPullData, bLockstepGPUs));
			}
		}
	}

	for (auto& KeyValue : TexturesToTransfer)
	{
		FRHITexture* Texture = KeyValue.Key;
		FRHIGPUMask GPUMask  = KeyValue.Value;

		for (uint32 GPUIndex : AllGPUMask)
		{
			if (!GPUMask.Contains(GPUIndex))
			{
				Transfers.Add(FTransferResourceParams(Texture, GPUMask.GetFirstIndex(), GPUIndex, bPullData, bLockstepGPUs));
			}
		}
	}

	if (Transfers.Num())
	{
		RHICmdList.TransferResources(Transfers);
	}
}
#endif  // WITH_MGPU
