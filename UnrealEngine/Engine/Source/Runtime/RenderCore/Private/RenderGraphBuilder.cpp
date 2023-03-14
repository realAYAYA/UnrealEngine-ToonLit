// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphBuilder.h"
#include "RenderGraphPrivate.h"
#include "RenderGraphTrace.h"
#include "RenderTargetPool.h"
#include "RenderGraphResourcePool.h"
#include "VisualizeTexture.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Async/ParallelFor.h"

#if ENABLE_RHI_VALIDATION

inline void GatherPassUAVsForOverlapValidation(const FRDGPass* Pass, TArray<FRHIUnorderedAccessView*, TInlineAllocator<MaxSimultaneousUAVs, FRDGArrayAllocator>>& OutUAVs)
{
	// RHI validation tracking of Begin/EndUAVOverlaps happens on the underlying resource, so we need to be careful about not
	// passing multiple UAVs that refer to the same resource, otherwise we get double-Begin and double-End validation errors.
	// Filter UAVs to only those with unique parent resources.
	TArray<FRDGViewableResource*, TInlineAllocator<MaxSimultaneousUAVs, FRDGArrayAllocator>> UniqueParents;
	Pass->GetParameters().Enumerate([&](FRDGParameter Parameter)
	{
		if (Parameter.IsUAV())
		{
			if (FRDGUnorderedAccessViewRef UAV = Parameter.GetAsUAV())
			{
				FRDGViewableResource* Parent = UAV->GetParent();

				// Check if we've already seen this parent.
				bool bFound = false;
				for (int32 Index = 0; !bFound && Index < UniqueParents.Num(); ++Index)
				{
					bFound = UniqueParents[Index] == Parent;
				}

				if (!bFound)
				{
					UniqueParents.Add(Parent);
					OutUAVs.Add(UAV->GetRHI());
				}
			}
		}
	});
}

#endif

inline void BeginUAVOverlap(const FRDGPass* Pass, FRHIComputeCommandList& RHICmdList)
{
#if ENABLE_RHI_VALIDATION
	TArray<FRHIUnorderedAccessView*, TInlineAllocator<MaxSimultaneousUAVs, FRDGArrayAllocator>> UAVs;
	GatherPassUAVsForOverlapValidation(Pass, UAVs);

	if (UAVs.Num())
	{
		RHICmdList.BeginUAVOverlap(UAVs);
	}
#endif
}

inline void EndUAVOverlap(const FRDGPass* Pass, FRHIComputeCommandList& RHICmdList)
{
#if ENABLE_RHI_VALIDATION
	TArray<FRHIUnorderedAccessView*, TInlineAllocator<MaxSimultaneousUAVs, FRDGArrayAllocator>> UAVs;
	GatherPassUAVsForOverlapValidation(Pass, UAVs);

	if (UAVs.Num())
	{
		RHICmdList.EndUAVOverlap(UAVs);
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
					BufferAccess = ERHIAccess::BVHRead;
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

	if (GDynamicRHI->RHIIncludeOptionalFlushes())
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}
}

void FRDGBuilder::EndFlushResourcesRHI()
{
	if (!bFlushResourcesRHI)
	{
		return;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(STAT_RDG_FlushResourcesRHI);
	SCOPED_NAMED_EVENT(EndFlushResourcesRHI, FColor::Emerald);
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
}

void FRDGBuilder::TickPoolElements()
{
	GRenderGraphResourcePool.TickPoolElements();

#if RDG_ENABLE_DEBUG
	if (GRDGDumpGraph)
	{
		--GRDGDumpGraph;
	}
	if (GRDGTransitionLog > 0)
	{
		--GRDGTransitionLog;
	}
	GRDGDumpGraphUnknownCount = 0;
#endif

#if STATS
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

ERDGPassFlags FRDGBuilder::OverridePassFlags(const TCHAR* PassName, ERDGPassFlags PassFlags, bool bAsyncComputeSupported)
{
	const bool bDebugAllowedForPass =
#if RDG_ENABLE_DEBUG
		IsDebugAllowedForPass(PassName);
#else
		true;
#endif

	if (IsAsyncComputeSupported() && bAsyncComputeSupported)
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
	if (EnumHasAnyFlags(Texture->Desc.Flags, ETextureCreateFlags::Shared))
	{
		return false;
	}
	return IsTransientInternal(Texture, EnumHasAnyFlags(Texture->Desc.Flags, ETextureCreateFlags::FastVRAM));
}

bool FRDGBuilder::IsTransientInternal(FRDGViewableResource* Resource, bool bFastVRAM) const
{
	// Immediate mode can't use the transient allocator because we don't know if the user will extract the resource.
	if (!GRDGTransientAllocator || IsImmediateMode())
	{
		return false;
	}

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
	if (GRDGDebugDisableTransientResources != 0 && IsDebugAllowedForResource(Resource->Name))
	{
		return false;
	}
#endif

	return true;
}

FRDGBuilder::FRDGBuilder(FRHICommandListImmediate& InRHICmdList, FRDGEventName InName, ERDGBuilderFlags InFlags)
	: RHICmdList(InRHICmdList)
	, Blackboard(Allocator)
	, BuilderName(InName)
	, CompilePipe(TEXT("RDG_CompilePipe"))
#if RDG_CPU_SCOPES
	, CPUScopeStacks(Allocator)
#endif
	, GPUScopeStacks(Allocator)
	, bParallelExecuteEnabled(IsParallelExecuteEnabled() && EnumHasAnyFlags(InFlags, ERDGBuilderFlags::AllowParallelExecute))
	, bParallelSetupEnabled(IsParallelSetupEnabled() && EnumHasAnyFlags(InFlags, ERDGBuilderFlags::AllowParallelExecute))
#if RDG_ENABLE_DEBUG
	, UserValidation(Allocator, bParallelExecuteEnabled)
	, BarrierValidation(&Passes, BuilderName)
#endif
	, TransientResourceAllocator(GRDGTransientResourceAllocator.Get())
{
	AddProloguePass();

#if RDG_EVENTS != RDG_EVENTS_NONE
	// This is polled once as a workaround for a race condition since the underlying global is not always changed on the render thread.
	GRDGEmitEvents = GetEmitDrawEvents();
#endif

#if RHI_WANT_BREADCRUMB_EVENTS
	if (bParallelExecuteEnabled)
	{
		BreadcrumbState = FRDGBreadcrumbState::Create(Allocator);
	}
#endif
}

FRDGBuilder::~FRDGBuilder()
{
	if (bParallelExecuteEnabled)
	{
		// Move expensive operations into the async deleter, which will be called in the base class destructor.
		BeginAsyncDelete([
			Passes					= MoveTemp(Passes),
			Textures				= MoveTemp(Textures),
			Buffers					= MoveTemp(Buffers),
			Views					= MoveTemp(Views),
			UniformBuffers			= MoveTemp(UniformBuffers),
			Blackboard				= MoveTemp(Blackboard),
			ActivePooledTextures	= MoveTemp(ActivePooledTextures),
			ActivePooledBuffers		= MoveTemp(ActivePooledBuffers),
			UploadedBuffers			= MoveTemp(UploadedBuffers)
		] () mutable {});
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const TRefCountPtr<FRDGPooledBuffer>& FRDGBuilder::ConvertToExternalBuffer(FRDGBufferRef Buffer)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateConvertToExternalResource(Buffer));
	if (!Buffer->bExternal)
	{
		Buffer->bExternal = 1;
		Buffer->bForceNonTransient = 1;
		BeginResourceRHI(GetProloguePassHandle(), Buffer);
		ExternalBuffers.Add(Buffer->GetRHIUnchecked(), Buffer);
	}
	return GetPooledBuffer(Buffer);
}

const TRefCountPtr<IPooledRenderTarget>& FRDGBuilder::ConvertToExternalTexture(FRDGTextureRef Texture)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateConvertToExternalResource(Texture));
	if (!Texture->bExternal)
	{
		Texture->bExternal = 1;
		Texture->bForceNonTransient = 1;
		BeginResourceRHI(GetProloguePassHandle(), Texture);
		ExternalTextures.Add(Texture->GetRHIUnchecked(), Texture);
	}
	return GetPooledTexture(Texture);
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
		AccessModeQueue.RemoveAtSwap(Index, 1, false);
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
	Ops.Reserve(bParallelSetupEnabled ? AccessModeQueue.Num() : 0);

	for (FRDGViewableResource* Resource : AccessModeQueue)
	{
		const auto& AccessModeState = Resource->AccessModeState;
		Resource->AccessModeState.bQueued = false;

		if (bParallelSetupEnabled)
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
			Allocator,
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
			Allocator,
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
	FRDGTexture* Texture = Textures.Allocate(Allocator, Name, Desc, Flags);
	SetRHI(Texture, ExternalPooledTexture.GetReference(), GetProloguePassHandle());
	Texture->bExternal = true;
	ExternalTextures.Add(Texture->GetRHIUnchecked(), Texture);

	if (Texture->bTransient)
	{
		FRDGSubresourceState State;
		State.SetPass(ERHIPipeline::Graphics, GetProloguePassHandle());
		InitTextureSubresources(*Texture->State, Texture->Layout, State);
	}

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

	FRDGBuffer* Buffer = Buffers.Allocate(Allocator, Name, ExternalPooledBuffer->Desc, Flags);
	SetRHI(Buffer, ExternalPooledBuffer, GetProloguePassHandle());
	Buffer->bExternal = true;

	ExternalBuffers.Add(Buffer->GetRHIUnchecked(), Buffer);

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateRegisterExternalBuffer(Buffer));
	IF_RDG_ENABLE_TRACE(Trace.AddResource(Buffer));
	return Buffer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::AddPassDependency(FRDGPassHandle ProducerHandle, FRDGPassHandle ConsumerHandle)
{
	FRDGPass* Consumer = Passes[ConsumerHandle];

	auto& Producers = Consumer->Producers;
	if (Producers.Find(ProducerHandle) == INDEX_NONE)
	{
		Producers.Add(ProducerHandle);
	}

#if STATS
	GRDGStatPassDependencyCount++;
#endif
}

void FRDGBuilder::AddPassDependency(FRDGPass* Producer, FRDGPass* Consumer)
{
	auto& Producers = Consumer->Producers;
	if (Producers.Find(Producer->Handle) == INDEX_NONE)
	{
		Producers.Add(Producer->Handle);
	}

#if STATS
	GRDGStatPassDependencyCount++;
#endif
}

void FRDGBuilder::AddCullingDependency(FRDGProducerStatesByPipeline& LastProducers, const FRDGProducerState& NextState, ERHIPipeline NextPipeline)
{
	for (ERHIPipeline LastPipeline : GetRHIPipelines())
	{
		FRDGProducerState& LastProducer = LastProducers[LastPipeline];

		if (LastProducer.Access == ERHIAccess::Unknown)
		{
			continue;
		}

		if (FRDGProducerState::IsDependencyRequired(LastProducer, LastPipeline, NextState, NextPipeline))
		{
			AddPassDependency(LastProducer.Pass, NextState.Pass);
		}
	}

	if (IsWritableAccess(NextState.Access))
	{
		LastProducers[NextPipeline] = NextState;
	}
}

void FRDGBuilder::CompilePassBarriers()
{
	// Walk the culled graph and compile barriers for each subresource. Certain transitions are redundant; read-to-read, for example.
	// We can avoid them by traversing and merging compatible states together. The merging states removes a transition, but the merging
	// heuristic is conservative and choosing not to merge doesn't necessarily mean a transition is performed. They are two distinct steps.
	// Merged states track the first and last pass interval. Pass references are also accumulated onto each resource. This must happen
	// after culling since culled passes can't contribute references.

	const FRDGPassHandle ProloguePassHandle = GetProloguePassHandle();
	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	SCOPED_NAMED_EVENT(CompileBarriers, FColor::Emerald);

	for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
	{
		FRDGPass* Pass = Passes[PassHandle];

		if (Pass->bCulled || Pass->bEmptyParameters)
		{
			continue;
		}

		const ERHIPipeline PassPipeline = Pass->Pipeline;

		const auto MergeSubresourceStates = [&](ERDGViewableResourceType ResourceType, FRDGSubresourceState*& PassMergeState, FRDGSubresourceState*& ResourceMergeState, const FRDGSubresourceState& PassState)
		{
			if (!ResourceMergeState || !FRDGSubresourceState::IsMergeAllowed(ResourceType, *ResourceMergeState, PassState))
			{
				// Cross-pipeline, non-mergable state changes require a new pass dependency for fencing purposes.
				if (ResourceMergeState)
				{
					for (ERHIPipeline Pipeline : GetRHIPipelines())
					{
						if (Pipeline != PassPipeline && ResourceMergeState->LastPass[Pipeline].IsValid())
						{
							// Add a dependency from the other pipe to this pass to join back.
							AddPassDependency(ResourceMergeState->LastPass[Pipeline], PassHandle);
						}
					}
				}

				// Allocate a new pending merge state and assign it to the pass state.
				ResourceMergeState = AllocSubresource(PassState);
			}
			else
			{
				// Merge the pass state into the merged state.
				ResourceMergeState->Access |= PassState.Access;

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
			FRDGTextureRef Texture = PassState.Texture;

			if (Texture->FirstBarrier == FRDGTexture::EFirstBarrier::ImmediateRequested)
			{
				check(Texture->bExternal);
				Texture->FirstBarrier = FRDGTexture::EFirstBarrier::ImmediateConfirmed;
				Texture->FirstPass = PassHandle;

				for (FRDGSubresourceState& SubresourceState : *Texture->State)
				{
					SubresourceState.SetPass(ERHIPipeline::Graphics, PassHandle);
				}
			}

		#if STATS
			GRDGStatTextureReferenceCount += PassState.ReferenceCount;
		#endif

			for (int32 Index = 0; Index < PassState.State.Num(); ++Index)
			{
				if (PassState.State[Index].Access == ERHIAccess::Unknown)
				{
					continue;
				}

				MergeSubresourceStates(ERDGViewableResourceType::Texture, PassState.MergeState[Index], Texture->MergeState[Index], PassState.State[Index]);
			}
		}

		for (auto& PassState : Pass->BufferStates)
		{
			FRDGBufferRef Buffer = PassState.Buffer;

			if (Buffer->FirstBarrier == FRDGBuffer::EFirstBarrier::ImmediateRequested)
			{
				check(Buffer->bExternal);
				Buffer->FirstBarrier = FRDGBuffer::EFirstBarrier::ImmediateConfirmed;
				Buffer->FirstPass = PassHandle;
				Buffer->State->SetPass(ERHIPipeline::Graphics, PassHandle);
			}

		#if STATS
			GRDGStatBufferReferenceCount += PassState.ReferenceCount;
		#endif

			MergeSubresourceStates(ERDGViewableResourceType::Buffer, PassState.MergeState, Buffer->MergeState, PassState.State);
		}
	}
}

void FRDGBuilder::Compile()
{
	SCOPE_CYCLE_COUNTER(STAT_RDG_CompileTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDG_Compile, GRDGVerboseCSVStats != 0);

	const FRDGPassHandle ProloguePassHandle = GetProloguePassHandle();
	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	const uint32 CompilePassCount = Passes.Num();

	TransitionCreateQueue.Reserve(CompilePassCount);

	const bool bCullPasses = GRDGCullPasses > 0;

	if (bParallelSetupEnabled)
	{
		SetupPassQueue.Flush(TEXT("FRDGBuilder::SetupPassResources"), [this](FRDGPass* Pass) { SetupPassResources(Pass); });
	}

	if (bCullPasses || AsyncComputePassCount > 0)
	{
		SCOPED_NAMED_EVENT(PassDependencies, FColor::Emerald);

		if (!bParallelSetupEnabled)
		{
			if (bCullPasses)
			{
				CullPassStack.Reserve(CompilePassCount);
			}

			for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
			{
				SetupPassDependencies(Passes[PassHandle]);
			}
		}

		const auto AddLastProducersToCullStack = [&](const FRDGProducerStatesByPipeline& LastProducers)
		{
			for (const FRDGProducerState& LastProducer : LastProducers)
			{
				if (LastProducer.Pass)
				{
					CullPassStack.Emplace(LastProducer.Pass->Handle);
				}
			}
		};

		// The last producer of a extracted or external resource is a cull graph root as it's not contained within the graph.

		for (const FExtractedTexture& ExtractedTexture : ExtractedTextures)
		{
			FRDGTextureRef Texture = ExtractedTexture.Texture;

			for (auto& LastProducer : Texture->LastProducers)
			{
				AddLastProducersToCullStack(LastProducer);
			}

			Texture->ReferenceCount++;
		}

		for (const FExtractedBuffer& ExtractedBuffer : ExtractedBuffers)
		{
			FRDGBufferRef Buffer = ExtractedBuffer.Buffer;

			AddLastProducersToCullStack(Buffer->LastProducer);

			Buffer->ReferenceCount++;
		}

		for (const auto& Pair : ExternalTextures)
		{
			FRDGTexture* Texture = Pair.Value;

			for (auto& LastProducer : Texture->LastProducers)
			{
				AddLastProducersToCullStack(LastProducer);
			}
		}

		for (const auto& Pair : ExternalBuffers)
		{
			FRDGBuffer* Buffer = Pair.Value;

			AddLastProducersToCullStack(Buffer->LastProducer);
		}
	}
	else
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

	// All dependencies in the raw graph have been specified; if enabled, all passes are marked as culled and a
	// depth first search is employed to find reachable regions of the graph. Roots of the search are those passes
	// with outputs leaving the graph or those marked to never cull.

	if (bCullPasses)
	{
		SCOPED_NAMED_EVENT(PassCulling, FColor::Emerald);

		CullPassStack.Emplace(EpiloguePassHandle);

		// Mark the epilogue pass as culled so that it is traversed.
		EpiloguePass->bCulled = 1;

		// Manually mark the prologue passes as not culled.
		ProloguePass->bCulled = 0;

		while (CullPassStack.Num())
		{
			FRDGPass* Pass = Passes[CullPassStack.Pop()];

			if (Pass->bCulled)
			{
				Pass->bCulled = 0;

				CullPassStack.Append(Pass->Producers);
			}
		}

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

#if STATS
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

		FRDGPassBitArray PassesOnAsyncCompute(false, CompilePassCount);

		// Traverse the active passes in execution order to find latest cross-pipeline producer and the earliest
		// cross-pipeline consumer for each pass. This helps narrow the search space later when building async
		// compute overlap regions.

		const auto IsCrossPipeline = [&](FRDGPassHandle A, FRDGPassHandle B)
		{
			return PassesOnAsyncCompute[A] != PassesOnAsyncCompute[B];
		};

		FRDGPassBitArray PassesWithCrossPipelineProducer(false, Passes.Num());
		FRDGPassBitArray PassesWithCrossPipelineConsumer(false, Passes.Num());

		for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
		{
			FRDGPass* Pass = Passes[PassHandle];

			if (Pass->bCulled || Pass->bEmptyParameters)
			{
				continue;
			}

			PassesOnAsyncCompute[PassHandle] = Pass->IsAsyncCompute();

			for (FRDGPassHandle ProducerHandle : Pass->GetProducers())
			{
				const FRDGPassHandle ConsumerHandle = PassHandle;

				if (!IsCrossPipeline(ProducerHandle, ConsumerHandle))
				{
					continue;
				}

				FRDGPass* Consumer = Pass;
				FRDGPass* Producer = Passes[ProducerHandle];

				// Finds the earliest consumer on the other pipeline for the producer.
				if (Producer->CrossPipelineConsumer.IsNull() || ConsumerHandle < Producer->CrossPipelineConsumer)
				{
					Producer->CrossPipelineConsumer = PassHandle;
					PassesWithCrossPipelineConsumer[ProducerHandle] = true;
				}

				// Finds the latest producer on the other pipeline for the consumer.
				if (Consumer->CrossPipelineProducer.IsNull() || ProducerHandle > Consumer->CrossPipelineProducer)
				{
					Consumer->CrossPipelineProducer = ProducerHandle;
					PassesWithCrossPipelineProducer[ConsumerHandle] = true;
				}
			}
		}

		// Establishes fork / join overlap regions for async compute. This is used for fencing as well as resource
		// allocation / deallocation. Async compute passes can't allocate / release their resource references until
		// the fork / join is complete, since the two pipes run in parallel. Therefore, all resource lifetimes on
		// async compute are extended to cover the full async region.

		const auto IsCrossPipelineProducer = [&](FRDGPassHandle A)
		{
			return PassesWithCrossPipelineConsumer[A];
		};

		const auto IsCrossPipelineConsumer = [&](FRDGPassHandle A)
		{
			return PassesWithCrossPipelineProducer[A];
		};

		const auto FindCrossPipelineProducer = [&](FRDGPassHandle PassHandle)
		{
			FRDGPassHandle LatestProducerHandle = ProloguePassHandle;
			FRDGPassHandle ConsumerHandle = PassHandle;

			// We want to find the latest producer on the other pipeline in order to establish a fork point.
			// Since we could be consuming N resources with N producer passes, we only care about the last one.
			while (ConsumerHandle != ProloguePassHandle)
			{
				if (IsCrossPipelineConsumer(ConsumerHandle) && !IsCrossPipeline(ConsumerHandle, PassHandle))
				{
					const FRDGPass* Consumer = Passes[ConsumerHandle];

					if (Consumer->CrossPipelineProducer > LatestProducerHandle && !Consumer->bCulled)
					{
						LatestProducerHandle = Consumer->CrossPipelineProducer;
					}
				}
				--ConsumerHandle;
			}

			return LatestProducerHandle;
		};

		const auto FindCrossPipelineConsumer = [&](FRDGPassHandle PassHandle)
		{
			FRDGPassHandle EarliestConsumerHandle = EpiloguePassHandle;
			FRDGPassHandle ProducerHandle = PassHandle;

			// We want to find the earliest consumer on the other pipeline, as this establishes a join point
			// between the pipes. Since we could be producing for N consumers on the other pipeline, we only
			// care about the first one to execute.
			while (ProducerHandle != EpiloguePassHandle)
			{
				if (IsCrossPipelineProducer(ProducerHandle) && !IsCrossPipeline(ProducerHandle, PassHandle))
				{
					const FRDGPass* Producer = Passes[ProducerHandle];

					if (Producer->CrossPipelineConsumer < EarliestConsumerHandle && !Producer->bCulled)
					{
						EarliestConsumerHandle = Producer->CrossPipelineConsumer;
					}
				}
				++ProducerHandle;
			}

			return EarliestConsumerHandle;
		};

		FRDGPass* AsyncComputePassBeforeFork = nullptr;

		const auto InsertGraphicsToAsyncComputeFork = [&](FRDGPass* GraphicsPass, FRDGPass* AsyncComputePass)
		{
			FRDGBarrierBatchBegin& EpilogueBarriersToBeginForAsyncCompute = GraphicsPass->GetEpilogueBarriersToBeginForAsyncCompute(Allocator, TransitionCreateQueue);

			GraphicsPass->bGraphicsFork = 1;
			EpilogueBarriersToBeginForAsyncCompute.SetUseCrossPipelineFence();

			AsyncComputePass->bAsyncComputeBegin = 1;
			AsyncComputePass->GetPrologueBarriersToEnd(Allocator).AddDependency(&EpilogueBarriersToBeginForAsyncCompute);

			// Since we are fencing the graphics pipe to some new async compute work, make sure to flush any prior work.
			if (AsyncComputePassBeforeFork)
			{
				AsyncComputePassBeforeFork->bDispatchAfterExecute = 1;
			}
		};

		const auto InsertAsyncComputeToGraphicsJoin = [&](FRDGPass* AsyncComputePass, FRDGPass* GraphicsPass)
		{
			FRDGBarrierBatchBegin& EpilogueBarriersToBeginForGraphics = AsyncComputePass->GetEpilogueBarriersToBeginForGraphics(Allocator, TransitionCreateQueue);

			AsyncComputePass->bAsyncComputeEnd = 1;
			AsyncComputePass->bDispatchAfterExecute = 1;
			EpilogueBarriersToBeginForGraphics.SetUseCrossPipelineFence();

			GraphicsPass->bGraphicsJoin = 1;
			GraphicsPass->GetPrologueBarriersToEnd(Allocator).AddDependency(&EpilogueBarriersToBeginForGraphics);
		};

		const auto AddResourcesToBegin = [this](FRDGPass* PassToBegin, FRDGPass* PassWithResources)
		{
			Passes[PassToBegin->PrologueBarrierPass]->ResourcesToBegin.Add(PassWithResources);
		};

		const auto AddResourcesToEnd = [this](FRDGPass* PassToEnd, FRDGPass* PassWithResources)
		{
			Passes[PassToEnd->EpilogueBarrierPass]->ResourcesToEnd.Add(PassWithResources);
		};

		FRDGPassHandle CurrentGraphicsForkPassHandle;

		for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
		{
			if (!PassesOnAsyncCompute[PassHandle])
			{
				continue;
			}

			FRDGPass* AsyncComputePass = Passes[PassHandle];

			if (AsyncComputePass->bCulled)
			{
				continue;
			}

			const FRDGPassHandle GraphicsForkPassHandle = FindCrossPipelineProducer(PassHandle);

			FRDGPass* GraphicsForkPass = Passes[GraphicsForkPassHandle];

			AsyncComputePass->GraphicsForkPass = GraphicsForkPassHandle;
			AddResourcesToBegin(GraphicsForkPass, AsyncComputePass);

			if (CurrentGraphicsForkPassHandle != GraphicsForkPassHandle)
			{
				CurrentGraphicsForkPassHandle = GraphicsForkPassHandle;
				InsertGraphicsToAsyncComputeFork(GraphicsForkPass, AsyncComputePass);
			}

			AsyncComputePassBeforeFork = AsyncComputePass;
		}

		FRDGPassHandle CurrentGraphicsJoinPassHandle;

		for (FRDGPassHandle PassHandle = EpiloguePassHandle - 1; PassHandle > ProloguePassHandle; --PassHandle)
		{
			if (!PassesOnAsyncCompute[PassHandle])
			{
				continue;
			}

			FRDGPass* AsyncComputePass = Passes[PassHandle];

			if (AsyncComputePass->bCulled)
			{
				continue;
			}

			const FRDGPassHandle GraphicsJoinPassHandle = FindCrossPipelineConsumer(PassHandle);

			FRDGPass* GraphicsJoinPass = Passes[GraphicsJoinPassHandle];

			AsyncComputePass->GraphicsJoinPass = GraphicsJoinPassHandle;
			GraphicsJoinPass->ResourcesToEnd.Add(AsyncComputePass);

			if (CurrentGraphicsJoinPassHandle != GraphicsJoinPassHandle)
			{
				CurrentGraphicsJoinPassHandle = GraphicsJoinPassHandle;
				InsertAsyncComputeToGraphicsJoin(AsyncComputePass, GraphicsJoinPass);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename LambdaType>
void FRDGBuilder::FPassQueue::Flush(UE::Tasks::FPipe& Pipe, const TCHAR* Name, LambdaType&& Lambda)
{
	if (LastTask.IsCompleted())
	{
		LastTask = Pipe.Launch(Name, [this, Lambda, Name]
		{
			SCOPED_NAMED_EVENT_TCHAR(Name, FColor::Magenta);
			while (FRDGPass* Pass = Queue.Pop())
			{
				Lambda(Pass);
			}
		});
	}
}

template <typename LambdaType>
void FRDGBuilder::FPassQueue::Flush(const TCHAR* Name, LambdaType&& Lambda)
{
	SCOPED_NAMED_EVENT_TCHAR(Name, FColor::Magenta);
	LastTask.Wait();
	while (FRDGPass* Pass = Queue.Pop())
	{
		Lambda(Pass);
	}
}


void FRDGBuilder::FlushSetupQueue()
{
	if (bParallelSetupEnabled)
	{
		SetupPassQueue.Flush(CompilePipe, TEXT("FRDGBuilder::SetupPassResources"), [this](FRDGPass* Pass) { SetupPassResources(Pass); });
	}
}

template <typename LambdaType>
UE::Tasks::FTask FRDGBuilder::LaunchCompileTask(const TCHAR* Name, bool bCondition, LambdaType&& Lambda)
{
	if (bCondition)
	{
		SCOPED_NAMED_EVENT_TCHAR(Name, FColor::Magenta);
		return CompilePipe.Launch(Name, [Lambda] { Lambda(); }, LowLevelTasks::ETaskPriority::High);
	}
	else
	{
		Lambda();
		return {};
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::Execute()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RDG);
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::Execute", FColor::Magenta);

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
	SetupEmptyPass(EpiloguePass = Passes.Allocate<FRDGSentinelPass>(Allocator, RDG_EVENT_NAME("Graph Epilogue")));

	const FRDGPassHandle ProloguePassHandle = GetProloguePassHandle();
	const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

	UE::Tasks::FTask SubmitBufferUploadsTask;
	UE::Tasks::FTask CompilePassBarriersTask;
	UE::Tasks::FTask CreateUniformBuffersTask;
	UE::Tasks::FTask SetupParallelExecuteTask;

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExecuteBegin());
	IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = true);

	if (!IsImmediateMode())
	{
		UE::Tasks::FTask CompileTask = LaunchCompileTask(TEXT("FRDGBuilder::Compile"), bParallelSetupEnabled, [this] { Compile(); });

		CompilePassBarriersTask = LaunchCompileTask(TEXT("FRDGBuilder::CompilePassBarriers"), bParallelSetupEnabled, [this] { CompilePassBarriers(); });

		BeginFlushResourcesRHI();
		PrepareBufferUploads();

		GPUScopeStacks.ReserveOps(Passes.Num());
		IF_RDG_CPU_SCOPES(CPUScopeStacks.ReserveOps());

		if (bParallelExecuteEnabled)
		{
		#if RHI_WANT_BREADCRUMB_EVENTS
			RHICmdList.ExportBreadcrumbState(*BreadcrumbState);
		#endif

			// Parallel execute setup can be done off the render thread and synced prior to dispatch.
			SetupParallelExecuteTask = LaunchCompileTask(TEXT("FRDGBuilder::SetupParallelExecute"), bParallelExecuteEnabled, [this] { SetupParallelExecute(); });
		}

		CompileTask.Wait();

		{
			SCOPE_CYCLE_COUNTER(STAT_RDG_CollectResourcesTime);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RDG_CollectResources);
			SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CollectResources", FColor::Magenta);

			UniformBuffersToCreate.Reserve(UniformBuffers.Num());

			EnumerateExtendedLifetimeResources(Textures, [](FRDGTexture* Texture)
			{
				++Texture->ReferenceCount;
			});

			EnumerateExtendedLifetimeResources(Buffers, [](FRDGBuffer* Buffer)
			{
				++Buffer->ReferenceCount;
			});

			// Null out any culled external resources so that the reference is freed up.

			for (const auto& Pair : ExternalTextures)
			{
				FRDGTexture* Texture = Pair.Value;

				if (Texture->IsCulled())
				{
					EndResourceRHI(ProloguePassHandle, Texture, 0);
				}
			}

			for (const auto& Pair : ExternalBuffers)
			{
				FRDGBuffer* Buffer = Pair.Value;

				if (Buffer->IsCulled())
				{
					EndResourceRHI(ProloguePassHandle, Buffer, 0);
				}
			}

			for (FRDGPassHandle PassHandle = Passes.Begin(); PassHandle < ProloguePassHandle; ++PassHandle)
			{
				FRDGPass* Pass = Passes[PassHandle];

				if (!Pass->bCulled)
				{
					EndResourcesRHI(Pass, ProloguePassHandle);
				}
			}

			for (FRDGPassHandle PassHandle = ProloguePassHandle; PassHandle <= EpiloguePassHandle; ++PassHandle)
			{
				FRDGPass* Pass = Passes[PassHandle];

				if (!Pass->bCulled)
				{
					BeginResourcesRHI(Pass, PassHandle);
					EndResourcesRHI(Pass, PassHandle);
				}
			}

			EnumerateExtendedLifetimeResources(Textures, [&](FRDGTextureRef Texture)
			{
				EndResourceRHI(EpiloguePassHandle, Texture, 1);
			});

			EnumerateExtendedLifetimeResources(Buffers, [&](FRDGBufferRef Buffer)
			{
				EndResourceRHI(EpiloguePassHandle, Buffer, 1);
			});

			if (TransientResourceAllocator)
			{
			#if RDG_ENABLE_TRACE
				TransientResourceAllocator->Flush(RHICmdList, Trace.IsEnabled() ? &Trace.TransientAllocationStats : nullptr);
			#else
				TransientResourceAllocator->Flush(RHICmdList);
			#endif
			}
		}

		// We have to wait until after view creation to launch uploads because we can't lock / unlock while creating views simultaneously.
		SubmitBufferUploadsTask = SubmitBufferUploads();

		// Uniform buffer creation depends on view creation.
		CreateUniformBuffersTask = CreateUniformBuffers();

		{
			SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CollectBarriers", FColor::Magenta);
			SCOPE_CYCLE_COUNTER(STAT_RDG_CollectBarriersTime);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDG_CollectBarriers, GRDGVerboseCSVStats != 0);

			CompilePassBarriersTask.Wait();

			for (FRDGPassHandle PassHandle = ProloguePassHandle + 1; PassHandle < EpiloguePassHandle; ++PassHandle)
			{
				FRDGPass* Pass = Passes[PassHandle];

				if (!Pass->bCulled && !Pass->bEmptyParameters)
				{
					CollectPassBarriers(Pass);
				}
			}
		}
	}

	{
		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::Finalize", FColor::Magenta);

		EpilogueResourceAccesses.Reserve(Textures.Num() + Buffers.Num());

		Textures.Enumerate([&](FRDGTextureRef Texture)
		{
			AddEpilogueTransition(Texture);
		});

		Buffers.Enumerate([&](FRDGBufferRef Buffer)
		{
			AddEpilogueTransition(Buffer);
		});
	}

	const ENamedThreads::Type RenderThread = ENamedThreads::GetRenderThread_Local();

	CreatePassBarriers([&]
	{
		if (!ParallelSetupEvents.IsEmpty())
		{
			UE::Tasks::Wait(ParallelSetupEvents);
			ParallelSetupEvents.Empty();
		}

		CreateUniformBuffersTask.Wait();

		if (SubmitBufferUploadsTask.IsValid())
		{
			SubmitBufferUploadsTask.Wait();
			check(RHICmdListBufferUploads);
			RHICmdList.QueueAsyncCommandListSubmit(RHICmdListBufferUploads);
			RHICmdListBufferUploads = nullptr;
		}

		// Process RHI thread flush before helping with barrier compilation on the render thread.
		EndFlushResourcesRHI();
	});

	UE::Tasks::FTask ParallelExecuteTask;

	if (bParallelExecuteEnabled)
	{
		SetupParallelExecuteTask.Wait();
		ParallelExecuteTask = CompilePipe.Launch(TEXT("DispatchParallelExecute"), [this] { DispatchParallelExecute(); }, LowLevelTasks::ETaskPriority::High);
	}

	IF_RDG_ENABLE_DEBUG(GRDGAllowRHIAccess = bParallelExecuteEnabled);
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
			#if STATS
				GRDGStatPassCullCount++;
			#endif

				continue;
			}

			if (bParallelExecuteEnabled)
			{
				if (Pass->bParallelExecute)
				{
				#if RDG_CPU_SCOPES // CPU scopes are replayed on the render thread prior to executing the entire batch.
					Pass->CPUScopeOps.Execute();
				#endif

					if (Pass->bParallelExecuteBegin)
					{
						FParallelPassSet& ParallelPassSet = ParallelPassSets[Pass->ParallelPassSetIndex];

						// Busy wait until our pass set is ready. This will be set by the dispatch task.
						while (!FPlatformAtomics::AtomicRead(&ParallelPassSet.bInitialized)) {};

						check(ParallelPassSet.CmdList != nullptr);
						RHICmdList.QueueAsyncCommandListSubmit(MakeArrayView<FRHICommandListImmediate::FQueuedCommandList>(&ParallelPassSet, 1));

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
	if (NameForTemporalEffect != NAME_None)
	{
		TArray<FRHITexture*> BroadcastTexturesForTemporalEffect;
		for (const FExtractedTexture& ExtractedTexture : ExtractedTextures)
		{
			if (EnumHasAnyFlags(ExtractedTexture.Texture->Flags, ERDGTextureFlags::MultiFrame))
			{
				BroadcastTexturesForTemporalEffect.Add(ExtractedTexture.Texture->GetRHIUnchecked());
			}
		}
		RHICmdList.BroadcastTemporalEffect(NameForTemporalEffect, BroadcastTexturesForTemporalEffect);
	}

	if (bForceCopyCrossGPU)
	{
		ForceCopyCrossGPU();
	}
#endif

	RHICmdList.SetTrackedAccess(EpilogueResourceAccesses);

	// Wait for the parallel dispatch task before attempting to wait on the execute event array (the former mutates the array).
	ParallelExecuteTask.Wait();

	// Wait on the actual parallel execute tasks in the Execute call. When draining is okay to let them overlap with other graph setup.
	// This also needs to be done before extraction of external resources to be consistent with non-parallel rendering.
	if (!ParallelExecuteEvents.IsEmpty())
	{
		UE::Tasks::Wait(ParallelExecuteEvents);
		ParallelExecuteEvents.Empty();
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

#if STATS
	GRDGStatBufferCount += Buffers.Num();
	GRDGStatTextureCount += Textures.Num();
	GRDGStatViewCount += Views.Num();
	GRDGStatMemoryWatermark = FMath::Max(GRDGStatMemoryWatermark, Allocator.GetByteCount());
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
	for (auto& PassState : Pass->TextureStates)
	{
		FRDGTextureRef Texture = PassState.Texture;
		auto& LastProducers = Texture->LastProducers;

		Texture->ReferenceCount += PassState.ReferenceCount;

		for (uint32 Index = 0, Count = LastProducers.Num(); Index < Count; ++Index)
		{
			const auto& SubresourceState = PassState.State[Index];

			if (SubresourceState.Access == ERHIAccess::Unknown)
			{
				continue;
			}

			FRDGProducerState ProducerState;
			ProducerState.Pass = Pass;
			ProducerState.Access = SubresourceState.Access;
			ProducerState.NoUAVBarrierHandle = SubresourceState.NoUAVBarrierFilter.GetUniqueHandle();

			AddCullingDependency(LastProducers[Index], ProducerState, Pass->Pipeline);
		}
	}

	for (auto& PassState : Pass->BufferStates)
	{
		FRDGBufferRef Buffer = PassState.Buffer;
		const auto& SubresourceState = PassState.State;

		Buffer->ReferenceCount += PassState.ReferenceCount;

		FRDGProducerState ProducerState;
		ProducerState.Pass = Pass;
		ProducerState.Access = SubresourceState.Access;
		ProducerState.NoUAVBarrierHandle = SubresourceState.NoUAVBarrierFilter.GetUniqueHandle();

		AddCullingDependency(Buffer->LastProducer, ProducerState, Pass->Pipeline);
	}

	const bool bCullPasses = GRDGCullPasses > 0;
	Pass->bCulled = bCullPasses;

	if (bCullPasses && (Pass->bHasExternalOutputs || EnumHasAnyFlags(Pass->Flags, ERDGPassFlags::NeverCull)))
	{
		CullPassStack.Emplace(Pass->Handle);
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

		EnumerateSubresourceRange(PassState->State, Texture->Layout, Range, [&](FRDGSubresourceState& State)
		{
			IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddSubresourceAccess(Texture, State, Access));

			State.Access = MakeValidAccess(State.Access, Access);
			State.Flags |= TransitionFlags;
			State.NoUAVBarrierFilter.AddHandle(NoUAVBarrierHandle);
			State.SetPass(PassPipeline, PassHandle);
		});

		if (IsWritableAccess(Access))
		{
			bRenderPassOnlyWrites &= EnumHasAnyFlags(AccessFlags, ERDGTextureAccessFlags::RenderTarget);

			// When running in parallel this is set via MarkResourcesAsProduced. We also can't touch this as its a bitfield and not atomic.
			if (!bParallelSetupEnabled)
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

		if (!FCString::Stricmp(Buffer->Name, TEXT("DefaultBuffer")))
		{
			UE_DEBUG_BREAK();
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
			if (!bParallelSetupEnabled)
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

	if (bParallelSetupEnabled)
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

	Pass->GraphicsForkPass = PassHandle;
	Pass->GraphicsJoinPass = PassHandle;
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

	GRDGStatPassCount++;
#endif

	IF_RDG_CPU_SCOPES(Pass->CPUScopes = CPUScopeStacks.GetCurrentScopes());
	Pass->GPUScopes = GPUScopeStacks.GetCurrentScopes(PassPipeline);

#if RDG_GPU_DEBUG_SCOPES && RDG_ENABLE_TRACE
	Pass->TraceEventScope = GPUScopeStacks.GetCurrentScopes(ERHIPipeline::Graphics).Event;
#endif

#if RDG_GPU_DEBUG_SCOPES && RDG_ENABLE_DEBUG
	if (const FRDGEventScope* Scope = Pass->GPUScopes.Event)
	{
		Pass->FullPathIfDebug = Scope->GetPath(Pass->Name);
	}
#endif
}

void FRDGBuilder::SetupAuxiliaryPasses(FRDGPass* Pass)
{
	if (IsImmediateMode() && !Pass->bSentinel)
	{
		SCOPED_NAMED_EVENT(FRDGBuilder_ExecutePass, FColor::Emerald);
		RDG_ALLOW_RHI_ACCESS_SCOPE();

		// Trivially redirect the merge states to the pass states, since we won't be compiling the graph.
		for (auto& PassState : Pass->TextureStates)
		{
			const uint32 SubresourceCount = PassState.State.Num();
			PassState.MergeState.SetNum(SubresourceCount);
			for (uint32 Index = 0; Index < SubresourceCount; ++Index)
			{
				if (PassState.State[Index].Access != ERHIAccess::Unknown)
				{
					PassState.MergeState[Index] = &PassState.State[Index];
				}
			}
		}

		for (auto& PassState : Pass->BufferStates)
		{
			PassState.MergeState = &PassState.State;
		}

		check(!EnumHasAnyFlags(Pass->Pipeline, ERHIPipeline::AsyncCompute));
		check(ParallelSetupEvents.IsEmpty());

		PrepareBufferUploads();
		SubmitBufferUploads();
		CompilePassOps(Pass);
		BeginResourcesRHI(Pass, Pass->Handle);
		CreateUniformBuffers();
		CollectPassBarriers(Pass);
		CreatePassBarriers([] {});
		if (!ParallelSetupEvents.IsEmpty())
		{
			UE::Tasks::Wait(ParallelSetupEvents);
			ParallelSetupEvents.Reset();
		}
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

	if (bParallelSetupEnabled)
	{
		MarkResourcesAsProduced(Pass);
		SetupPassQueue.Push(Pass);
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
	if (!bWaitedForTemporalEffect && NameForTemporalEffect != NAME_None && Pass->Pipeline == ERHIPipeline::Graphics)
	{
		bWaitedForTemporalEffect = true;
		Pass->bWaitForTemporalEffect = 1;
	}

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

void FRDGBuilder::PrepareBufferUploads()
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::PrepareBufferUploads", FColor::Magenta);

	for (FUploadedBuffer& UploadedBuffer : UploadedBuffers)
	{
		FRDGBuffer* Buffer = UploadedBuffer.Buffer;

		if (!Buffer->HasRHI())
		{
			BeginResourceRHI(GetProloguePassHandle(), Buffer);
		}

		check(UploadedBuffer.DataSize <= Buffer->Desc.GetSize());
	}
}

UE::Tasks::FTask FRDGBuilder::SubmitBufferUploads()
{
	const auto SubmitUploadsLambda = [this](FRHICommandList& RHICmdListUpload)
	{
		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::SubmitBufferUploads", FColor::Magenta);

		for (FUploadedBuffer& UploadedBuffer : UploadedBuffers)
		{
			if (UploadedBuffer.bUseDataCallbacks)
			{
				UploadedBuffer.Data = UploadedBuffer.DataCallback();
				UploadedBuffer.DataSize = UploadedBuffer.DataSizeCallback();
			}

			if (UploadedBuffer.Data && UploadedBuffer.DataSize)
			{
				void* DestPtr = RHICmdListUpload.LockBuffer(UploadedBuffer.Buffer->GetRHIUnchecked(), 0, UploadedBuffer.DataSize, RLM_WriteOnly);
				FMemory::Memcpy(DestPtr, UploadedBuffer.Data, UploadedBuffer.DataSize);
				RHICmdListUpload.UnlockBuffer(UploadedBuffer.Buffer->GetRHIUnchecked());

				if (UploadedBuffer.bUseFreeCallbacks)
				{
					UploadedBuffer.DataFreeCallback(UploadedBuffer.Data);
				}
			}
		}

		UploadedBuffers.Reset();
	};

	if (bParallelSetupEnabled)
	{
		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::SubmitBufferUploads", FColor::Magenta);

		return UE::Tasks::Launch(TEXT("FRDGBuilder::SubmitBufferUploads"), [this, SubmitUploadsLambda]
		{
			FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
			RHICmdListBufferUploads = new FRHICommandList(FRHIGPUMask::All());
			SubmitUploadsLambda(*RHICmdListBufferUploads);
			RHICmdListBufferUploads->FinishRecording();

		}, LowLevelTasks::ETaskPriority::High);
	}
	else
	{
		SubmitUploadsLambda(RHICmdList);
		return {};
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::SetupParallelExecute()
{
	SCOPED_NAMED_EVENT(SetupParallelExecute, FColor::Emerald);
	FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);

	TArray<FRDGPass*, TInlineAllocator<64, FRDGArrayAllocator>> ParallelPassCandidates;
	int32 MergedRenderPassCandidates = 0;
	bool bDispatchAfterExecute = false;

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
			PassBegin->ParallelPassSetIndex = ParallelPassSets.Num();

			FRDGPass* PassEnd = ParallelPassCandidates[PassEndIndex - 1];
			PassEnd->bParallelExecuteEnd = 1;
			PassEnd->ParallelPassSetIndex = ParallelPassSets.Num();

			for (int32 PassIndex = PassBeginIndex; PassIndex < PassEndIndex; ++PassIndex)
			{
				ParallelPassCandidates[PassIndex]->bParallelExecute = 1;
			}

			FParallelPassSet& ParallelPassSet = ParallelPassSets.Emplace_GetRef();
			ParallelPassSet.Passes.Append(ParallelPassCandidates.GetData() + PassBeginIndex, ParallelPassCandidateCount);
			ParallelPassSet.bDispatchAfterExecute = bDispatchAfterExecute;
		}

		ParallelPassCandidates.Reset();
		MergedRenderPassCandidates = 0;
		bDispatchAfterExecute = false;
	};

	ParallelPassSets.Reserve(32);
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

		ParallelPassCandidates.Emplace(Pass);
		bDispatchAfterExecute |= Pass->bDispatchAfterExecute;

		// Don't count merged render passes for the maximum pass threshold. This avoids the case where
		// a large merged render pass span could end up forcing it back onto the render thread, since
		// it's not possible to launch a task for a subset of passes within a merged render pass.
		MergedRenderPassCandidates += Pass->bSkipRenderPassBegin | Pass->bSkipRenderPassEnd;

		if (ParallelPassCandidates.Num() - MergedRenderPassCandidates >= GRDGParallelExecutePassMax)
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
			FParallelPassSet& ParallelPassSet = ParallelPassSets[Pass->ParallelPassSetIndex];
			ParallelPassSet.BreadcrumbStateBegin = BreadcrumbState->Copy(Allocator);
			ParallelPassSet.BreadcrumbStateEnd = ParallelPassSet.BreadcrumbStateBegin;
		}

		Pass->GPUScopeOpsPrologue.Event.Execute(*BreadcrumbState);
		Pass->GPUScopeOpsEpilogue.Event.Execute(*BreadcrumbState);

		if (Pass->bParallelExecuteEnd)
		{
			FParallelPassSet& ParallelPassSet = ParallelPassSets[Pass->ParallelPassSetIndex];

			if (ParallelPassSet.BreadcrumbStateEnd->Version != BreadcrumbState->Version)
			{
				ParallelPassSet.BreadcrumbStateEnd = BreadcrumbState->Copy(Allocator);
			}
		}
	}
#endif
}

void FRDGBuilder::DispatchParallelExecute()
{
	SCOPED_NAMED_EVENT(DispatchParallelExecute, FColor::Emerald);

	check(ParallelExecuteEvents.IsEmpty());
	ParallelExecuteEvents.Reserve(ParallelPassSets.Num());

	for (FParallelPassSet& ParallelPassSet : ParallelPassSets)
	{
		ParallelExecuteEvents.Emplace(UE::Tasks::Launch(TEXT("FRDGBuilder::ParallelExecute"), [this, &ParallelPassSet]
		{
			SCOPED_NAMED_EVENT(ParallelExecute, FColor::Emerald);
			FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);

			FRHICommandList* RHICmdListPass = new FRHICommandList(FRHIGPUMask::All());
			ParallelPassSet.CmdList = RHICmdListPass;

			// Mark this set as initialized so that it can be submitted.
			FPlatformAtomics::AtomicStore(&ParallelPassSet.bInitialized, 1);

			IF_RHI_WANT_BREADCRUMB_EVENTS(RHICmdListPass->ImportBreadcrumbState(*ParallelPassSet.BreadcrumbStateBegin));

			for (FRDGPass* Pass : ParallelPassSet.Passes)
			{
				ExecutePass(Pass, *RHICmdListPass);
			}

			RHICmdListPass->FinishRecording();

		}, LowLevelTasks::ETaskPriority::High));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

UE::Tasks::FTask FRDGBuilder::CreateUniformBuffers()
{
	SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CreateUniformBuffers", FColor::Magenta);

	const int32 ParallelDispatchThreshold = 4;

	const auto CreateUniformBuffersFunction = [this]
	{
		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CreateUniformBuffers", FColor::Magenta);
		for (FRDGUniformBufferHandle UniformBufferHandle : UniformBuffersToCreate)
		{
			UniformBuffers[UniformBufferHandle]->InitRHI();
		}
		UniformBuffersToCreate.Reset();
	};

	UE::Tasks::FTask Task;

	if (bParallelSetupEnabled && UniformBuffersToCreate.Num() > ParallelDispatchThreshold)
	{
		Task = UE::Tasks::Launch(TEXT("FRDGBuilder::CreateUniformBuffer"),
			[CreateUniformBuffersFunction]
		{
			FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
			CreateUniformBuffersFunction();

		}, LowLevelTasks::ETaskPriority::High);
	}
	else
	{
		CreateUniformBuffersFunction();
	}

	return Task;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::AddProloguePass()
{
	ProloguePass = SetupEmptyPass(Passes.Allocate<FRDGSentinelPass>(Allocator, RDG_EVENT_NAME("Graph Prologue (Graphics)")));
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
		FRHICommandListScopedPipeline Scope(RHICmdListPass, Pass->Pipeline);

#if 0 // Disabled by default to reduce memory usage in Insights.
		SCOPED_NAMED_EVENT_TCHAR(Pass->GetName(), FColor::Magenta);
#endif

		// Note that we must do this before doing anything with RHICmdList for the pass.
		// For example, if this pass only executes on GPU 1 we want to avoid adding a
		// 0-duration event for this pass on GPU 0's time line.
		SCOPED_GPU_MASK(RHICmdListPass, Pass->GPUMask);

#if RDG_CPU_SCOPES
		if (!Pass->bParallelExecute)
		{
			Pass->CPUScopeOps.Execute();
		}
#endif

		IF_RDG_ENABLE_DEBUG(ConditionalDebugBreak(RDG_BREAKPOINT_PASS_EXECUTE, BuilderName.GetTCHAR(), Pass->GetName()));

#if WITH_MGPU
		if (Pass->bWaitForTemporalEffect)
		{
			static_cast<FRHICommandList&>(RHICmdListPass).WaitForTemporalEffect(NameForTemporalEffect);
		}
#endif

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

	if (!Pass->bParallelExecute && Pass->bDispatchAfterExecute)
	{
		if (Pass->Pipeline == ERHIPipeline::Graphics)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}

	if (!bParallelExecuteEnabled)
	{
		if (GRDGDebugFlushGPU && !GRDGAsyncCompute)
		{
			RHICmdList.SubmitCommandsAndFlushGPU();
			RHICmdList.BlockUntilGPUIdle();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::BeginResourcesRHI(FRDGPass* ResourcePass, FRDGPassHandle ExecutePassHandle)
{
	for (FRDGPass* PassToBegin : ResourcePass->ResourcesToBegin)
	{
		for (const auto& PassState : PassToBegin->TextureStates)
		{
			BeginResourceRHI(ExecutePassHandle, PassState.Texture);
		}

		for (const auto& PassState : PassToBegin->BufferStates)
		{
			BeginResourceRHI(ExecutePassHandle, PassState.Buffer);
		}

		for (FRDGUniformBufferHandle UniformBufferHandle : PassToBegin->UniformBuffers)
		{
			if (FRDGUniformBuffer* UniformBuffer = UniformBuffers[UniformBufferHandle]; !UniformBuffer->bQueuedForCreate)
			{
				UniformBuffer->bQueuedForCreate = true;
				UniformBuffersToCreate.Add(UniformBufferHandle);
			}
		}

		for (FRDGViewHandle ViewHandle : PassToBegin->Views)
		{
			InitRHI(Views[ViewHandle]);
		}
	}
}

void FRDGBuilder::EndResourcesRHI(FRDGPass* ResourcePass, FRDGPassHandle ExecutePassHandle)
{
	for (FRDGPass* PassToEnd : ResourcePass->ResourcesToEnd)
	{
		for (const auto& PassState : PassToEnd->TextureStates)
		{
			EndResourceRHI(ExecutePassHandle, PassState.Texture, PassState.ReferenceCount);
		}

		for (const auto& PassState : PassToEnd->BufferStates)
		{
			EndResourceRHI(ExecutePassHandle, PassState.Buffer, PassState.ReferenceCount);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::CollectPassBarriers(FRDGPass* Pass)
{
	IF_RDG_ENABLE_DEBUG(ConditionalDebugBreak(RDG_BREAKPOINT_PASS_COMPILE, BuilderName.GetTCHAR(), Pass->GetName()));

	for (auto& PassState : Pass->TextureStates)
	{
		FRDGTextureRef Texture = PassState.Texture;
		AddTransition(Pass->Handle, Texture, PassState.MergeState);

		IF_RDG_ENABLE_TRACE(Trace.AddTexturePassDependency(Texture, Pass));
	}

	for (auto& PassState : Pass->BufferStates)
	{
		FRDGBufferRef Buffer = PassState.Buffer;
		AddTransition(Pass->Handle, Buffer, *PassState.MergeState);

		IF_RDG_ENABLE_TRACE(Trace.AddBufferPassDependency(Buffer, Pass));
	}
}

void FRDGBuilder::CreatePassBarriers(TFunctionRef<void()> PreWork)
{
	const int32 NumBarriersPerTask = 32;

	if (bParallelSetupEnabled && TransitionCreateQueue.Num() > NumBarriersPerTask)
	{
		ParallelForWithPreWork(TEXT("FRDGBuilder::CreatePassBarriers"), TransitionCreateQueue.Num(), NumBarriersPerTask, [this](int32 Index)
		{
			TransitionCreateQueue[Index]->CreateTransition();

		}, PreWork);
	}
	else
	{
		PreWork();

		SCOPED_NAMED_EVENT_TEXT("FRDGBuilder::CreatePassBarriers", FColor::Magenta);

		for (FRDGBarrierBatchBegin* BeginBatch : TransitionCreateQueue)
		{
			BeginBatch->CreateTransition();
		}
	}

	TransitionCreateQueue.Reset();
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::AddEpilogueTransition(FRDGTextureRef Texture)
{
	if (!Texture->HasRHI() || Texture->IsCulled() || !Texture->bLastOwner)
	{
		return;
	}

	if (!EnumHasAnyFlags(Texture->Flags, ERDGTextureFlags::SkipTracking))
	{
		const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

		FRDGSubresourceState SubresourceState;
		SubresourceState.SetPass(ERHIPipeline::Graphics, EpiloguePassHandle);

		// Texture is using the RHI transient allocator. Transition it back to Discard in the final pass it is used.
		if (Texture->bTransient && !Texture->TransientTexture->IsAcquired())
		{
			const TInterval<uint32> DiscardPasses = Texture->TransientTexture->GetDiscardPasses();
			const FRDGPassHandle MinDiscardPassHandle(DiscardPasses.Min);
			const FRDGPassHandle MaxDiscardPassHandle(FMath::Min<uint32>(DiscardPasses.Max, EpiloguePassHandle.GetIndex()));

			AddAliasingTransition(MinDiscardPassHandle, MaxDiscardPassHandle, Texture, FRHITransientAliasingInfo::Discard(Texture->GetRHIUnchecked()));

			SubresourceState.SetPass(ERHIPipeline::Graphics, MaxDiscardPassHandle);
			Texture->EpilogueAccess = ERHIAccess::Discard;
		}

		SubresourceState.Access = Texture->EpilogueAccess;

		InitTextureSubresources(ScratchTextureState, Texture->Layout, &SubresourceState);
		AddTransition(EpiloguePassHandle, Texture, ScratchTextureState);
		ScratchTextureState.Reset();

		EpilogueResourceAccesses.Emplace(Texture->GetRHI(), Texture->EpilogueAccess);
	}

	if (Texture->Allocation)
	{
		ActivePooledTextures.Emplace(MoveTemp(Texture->Allocation));
	}
	else if (!Texture->bTransient)
	{
		// Non-transient textures need to be 'resurrected' to hold the last reference in the chain.
		// Transient textures don't have that restriction since there's no actual pooling happening.
		ActivePooledTextures.Emplace(Texture->RenderTarget);
	}
}

void FRDGBuilder::AddEpilogueTransition(FRDGBufferRef Buffer)
{
	if (!Buffer->HasRHI() || Buffer->IsCulled() || !Buffer->bLastOwner)
	{
		return;
	}

	if (!EnumHasAnyFlags(Buffer->Flags, ERDGBufferFlags::SkipTracking))
	{
		const FRDGPassHandle EpiloguePassHandle = GetEpiloguePassHandle();

		FRDGSubresourceState SubresourceState;
		SubresourceState.SetPass(ERHIPipeline::Graphics, EpiloguePassHandle);

		// Texture is using the RHI transient allocator. Transition it back to Discard in the final pass it is used.
		if (Buffer->bTransient)
		{
			const TInterval<uint32> DiscardPasses = Buffer->TransientBuffer->GetDiscardPasses();
			const FRDGPassHandle MinDiscardPassHandle(DiscardPasses.Min);
			const FRDGPassHandle MaxDiscardPassHandle(FMath::Min<uint32>(DiscardPasses.Max, EpiloguePassHandle.GetIndex()));

			AddAliasingTransition(MinDiscardPassHandle, MaxDiscardPassHandle, Buffer, FRHITransientAliasingInfo::Discard(Buffer->GetRHIUnchecked()));

			SubresourceState.SetPass(ERHIPipeline::Graphics, MaxDiscardPassHandle);
			Buffer->EpilogueAccess = ERHIAccess::Discard;
		}

		SubresourceState.Access = Buffer->EpilogueAccess;

		AddTransition(Buffer->LastPass, Buffer, SubresourceState);

		EpilogueResourceAccesses.Emplace(Buffer->GetRHI(), Buffer->EpilogueAccess);
	}

	if (Buffer->Allocation)
	{
		ActivePooledBuffers.Emplace(MoveTemp(Buffer->Allocation));
	}
	else if (!Buffer->bTransient)
	{
		// Non-transient buffers need to be 'resurrected' to hold the last reference in the chain.
		// Transient buffers don't have that restriction since there's no actual pooling happening.
		ActivePooledBuffers.Emplace(Buffer->PooledBuffer);
	}
}

void FRDGBuilder::AddTransition(FRDGPassHandle PassHandle, FRDGTextureRef Texture, FRDGTextureSubresourceStateIndirect& StateAfter)
{
	const FRDGTextureSubresourceLayout Layout = Texture->Layout;
	FRDGTextureSubresourceState& StateBefore = Texture->GetState();
	const uint32 SubresourceCount = StateBefore.Num();

	checkf(StateBefore.Num() == Layout.GetSubresourceCount() && StateBefore.Num() == StateAfter.Num(),
		TEXT("Before state array (%d) does not match after state array (%d) for resource %s on pass %s."),
		StateBefore.Num(), StateAfter.Num(), Texture->Name, Passes[PassHandle]->GetName());

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

			FRDGSubresourceState& DepthStateBefore   = StateBefore[DepthIndex];
			FRDGSubresourceState& StencilStateBefore = StateBefore[StencilIndex];

			// Case 1: transitioning into a fused copy state.
			if (DepthStateAfter && EnumHasAnyFlags(DepthStateAfter->Access, ERHIAccess::CopySrc | ERHIAccess::CopyDest))
			{
				check(StencilStateAfter && StencilStateAfter->Access == DepthStateAfter->Access);

				const FRDGPassHandle MaxPassHandle = FRDGPassHandle::Max(DepthStateBefore.LastPass[GraphicsPipe], StencilStateBefore.LastPass[GraphicsPipe]);
				DepthStateBefore.LastPass[GraphicsPipe]   = MaxPassHandle;
				StencilStateBefore.LastPass[GraphicsPipe] = MaxPassHandle;
			}
			// Case 2: transitioning out of a fused copy state.
			else if (EnumHasAnyFlags(DepthStateBefore.Access, ERHIAccess::CopySrc | ERHIAccess::CopyDest))
			{
				check(StencilStateBefore.Access        == DepthStateBefore.Access);
				check(StencilStateBefore.GetLastPass() == DepthStateBefore.GetLastPass());

				// Case 2a: depth unknown, so transition to match stencil.
				if (!DepthStateAfter)
				{
					DepthStateAfter = AllocSubresource(*StencilStateAfter);
					DepthStateAfter->SetPass(GraphicsPipe, PassHandle);
				}
				// Case 2b: stencil unknown, so transition to match depth.
				else if (!StencilStateAfter)
				{
					StencilStateAfter = AllocSubresource(*DepthStateAfter);
					StencilStateAfter->SetPass(GraphicsPipe, PassHandle);
				}
				// Two valid after states should be transitioning on this pass.
				else
				{
					check(StencilStateAfter->GetFirstPass() == PassHandle && DepthStateAfter->GetFirstPass() == PassHandle);
				}
			}
		}
	}

	for (uint32 SubresourceIndex = 0; SubresourceIndex < SubresourceCount; ++SubresourceIndex)
	{
		if (const FRDGSubresourceState* SubresourceStateAfter = StateAfter[SubresourceIndex])
		{
			check(SubresourceStateAfter->Access != ERHIAccess::Unknown);

			FRDGSubresourceState& SubresourceStateBefore = StateBefore[SubresourceIndex];

			if (FRDGSubresourceState::IsTransitionRequired(SubresourceStateBefore, *SubresourceStateAfter))
			{
				const FRDGTextureSubresource Subresource = Layout.GetSubresource(SubresourceIndex);

				FRHITransitionInfo Info;
				Info.Texture      = Texture->GetRHIUnchecked();
				Info.Type         = FRHITransitionInfo::EType::Texture;
				Info.Flags        = SubresourceStateAfter->Flags;
				Info.AccessBefore = SubresourceStateBefore.Access;
				Info.AccessAfter  = SubresourceStateAfter->Access;
				Info.MipIndex     = Subresource.MipIndex;
				Info.ArraySlice   = Subresource.ArraySlice;
				Info.PlaneSlice   = Subresource.PlaneSlice;

				if (Info.AccessBefore == ERHIAccess::Discard)
				{
					Info.Flags |= EResourceTransitionFlags::Discard;
				}

				AddTransition(Texture, SubresourceStateBefore, *SubresourceStateAfter, Info);
			}

			SubresourceStateBefore = *SubresourceStateAfter;
		}
	}
}

void FRDGBuilder::AddTransition(FRDGPassHandle PassHandle, FRDGBufferRef Buffer, FRDGSubresourceState StateAfter)
{
	check(StateAfter.Access != ERHIAccess::Unknown);

	FRDGSubresourceState& StateBefore = Buffer->GetState();

	if (FRDGSubresourceState::IsTransitionRequired(StateBefore, StateAfter))
	{
		FRHITransitionInfo Info;
		Info.Resource = Buffer->GetRHIUnchecked();
		Info.Type = FRHITransitionInfo::EType::Buffer;
		Info.Flags = StateAfter.Flags;
		Info.AccessBefore = StateBefore.Access;
		Info.AccessAfter = StateAfter.Access;

		AddTransition(Buffer, StateBefore, StateAfter, Info);
	}

	StateBefore = StateAfter;
}

void FRDGBuilder::AddTransition(
	FRDGViewableResource* Resource,
	FRDGSubresourceState StateBefore,
	FRDGSubresourceState StateAfter,
	const FRHITransitionInfo& TransitionInfo)
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
			BarriersToBegin = &BeginPass->GetEpilogueBarriersToBeginFor(Allocator, TransitionCreateQueue, PipelinesAfter);
		}
		// This is an immediate prologue transition in the same pass. Issue the begin in the prologue.
		else
		{
			checkf(PipelinesAfter == ERHIPipeline::Graphics,
				TEXT("Attempted to queue an immediate async pipe transition for %s. Pipelines: %s. Async transitions must be split."),
				Resource->Name, *GetRHIPipelineName(PipelinesAfter));

			BeginPass = GetPrologueBarrierPass(BeginPassHandle);
			BarriersToBegin = &BeginPass->GetPrologueBarriersToBegin(Allocator, TransitionCreateQueue);
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

			BarriersToBegin = Allocator.AllocNoDestruct<FRDGBarrierBatchBegin>(PipelinesBefore, PipelinesAfter, GetEpilogueBarriersToBeginDebugName(PipelinesAfter), BarrierBatchPasses);
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
		check(BeginPass->GetPipeline() == ERHIPipeline::Graphics);

		BarriersToBegin = &BeginPass->GetPrologueBarriersToBegin(Allocator, TransitionCreateQueue);
	}
	else
	{
		FRDGPass* BeginPass = GetEpilogueBarrierPass(BeginPassHandle);
		EndPass = Passes[EndPassHandle];

		check(GetPrologueBarrierPassHandle(EndPassHandle) == EndPassHandle);
		check(BeginPass->GetPipeline() == ERHIPipeline::Graphics);
		check(EndPass->GetPipeline() == ERHIPipeline::Graphics);

		BarriersToBegin = &BeginPass->GetEpilogueBarriersToBeginForGraphics(Allocator, TransitionCreateQueue);
	}

	BarriersToBegin->AddAlias(Resource, Info);
	EndPass->GetPrologueBarriersToEnd(Allocator).AddDependency(BarriersToBegin);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::SetRHI(FRDGTexture* Texture, IPooledRenderTarget* RenderTarget, FRDGPassHandle PassHandle)
{
	Texture->RenderTarget = RenderTarget;

	if (FRHITransientTexture* TransientTexture = RenderTarget->GetTransientTexture())
	{
		FRDGTransientRenderTarget* TransientRenderTarget = static_cast<FRDGTransientRenderTarget*>(RenderTarget);
		Texture->Allocation = TRefCountPtr<FRDGTransientRenderTarget>(TransientRenderTarget);

		SetRHI(Texture, TransientTexture, PassHandle);
	}
	else
	{
		FPooledRenderTarget* PooledRenderTarget = static_cast<FPooledRenderTarget*>(RenderTarget);
		Texture->Allocation = TRefCountPtr<FPooledRenderTarget>(PooledRenderTarget);

		SetRHI(Texture, &PooledRenderTarget->PooledTexture, PassHandle);
	}
}

void FRDGBuilder::SetRHI(FRDGTexture* Texture, FRDGPooledTexture* PooledTexture, FRDGPassHandle PassHandle)
{
	FRHITexture* TextureRHI = PooledTexture->GetRHI();

	Texture->ResourceRHI = TextureRHI;
	Texture->PooledTexture = PooledTexture;
	Texture->ViewCache = &PooledTexture->ViewCache;
	Texture->FirstPass = PassHandle;

	FRDGTexture*& Owner = PooledTextureOwnershipMap.FindOrAdd(PooledTexture);

	// Link the previous alias to this one.
	if (Owner)
	{
		Owner->NextOwner = Texture->Handle;
		Owner->bLastOwner = false;

		// Chain the state allocation between all RDG textures which share this pooled texture.
		Texture->State = Owner->State;
	}
	else
	{
		Texture->State = Allocator.AllocNoDestruct<FRDGTextureSubresourceState>();

		FRDGSubresourceState State;
		State.SetPass(ERHIPipeline::Graphics, GetProloguePassHandle());
		InitTextureSubresources(*Texture->State, Texture->Layout, State);
	}

	Owner = Texture;
}

void FRDGBuilder::SetRHI(FRDGTexture* Texture, FRHITransientTexture* TransientTexture, FRDGPassHandle PassHandle)
{
	Texture->ResourceRHI = TransientTexture->GetRHI();
	Texture->TransientTexture = TransientTexture;
	Texture->ViewCache = &TransientTexture->ViewCache;
	Texture->FirstPass = PassHandle;
	Texture->bTransient = true;
	Texture->State = Allocator.AllocNoDestruct<FRDGTextureSubresourceState>();
}

void FRDGBuilder::SetRHI(FRDGBuffer* Buffer, FRDGPooledBuffer* PooledBuffer, FRDGPassHandle PassHandle)
{
	FRHIBuffer* BufferRHI = PooledBuffer->GetRHI();

	Buffer->ResourceRHI = BufferRHI;
	Buffer->PooledBuffer = PooledBuffer;
	Buffer->ViewCache = &PooledBuffer->ViewCache;
	Buffer->Allocation = PooledBuffer;
	Buffer->FirstPass = PassHandle;

	FRDGBuffer*& Owner = PooledBufferOwnershipMap.FindOrAdd(PooledBuffer);

	// Link the previous owner to this one.
	if (Owner)
	{
		Owner->NextOwner = Buffer->Handle;
		Owner->bLastOwner = false;

		// Chain the state allocation between all RDG buffers which share this pooled buffer.
		Buffer->State = Owner->State;
	}
	else
	{
		FRDGSubresourceState State;
		State.SetPass(ERHIPipeline::Graphics, GetProloguePassHandle());
		Buffer->State = Allocator.AllocNoDestruct<FRDGSubresourceState>(State);
	}

	Owner = Buffer;
}

void FRDGBuilder::SetRHI(FRDGBuffer* Buffer, FRHITransientBuffer* TransientBuffer, FRDGPassHandle PassHandle)
{
	check(!Buffer->ResourceRHI);

	Buffer->ResourceRHI = TransientBuffer->GetRHI();
	Buffer->TransientBuffer = TransientBuffer;
	Buffer->ViewCache = &TransientBuffer->ViewCache;
	Buffer->FirstPass = PassHandle;
	Buffer->bTransient = true;
	Buffer->State = Allocator.AllocNoDestruct<FRDGSubresourceState>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::BeginResourceRHI(FRDGPassHandle PassHandle, FRDGTextureRef Texture)
{
	check(Texture);

	if (Texture->HasRHI())
	{
		return;
	}

	check(Texture->ReferenceCount > 0 || Texture->bExternal || IsImmediateMode());

#if RDG_ENABLE_DEBUG
	{
		FRDGPass* Pass = Passes[PassHandle];

		// Cannot begin a resource on an async compute pass.
		check(Pass->Pipeline == ERHIPipeline::Graphics);

		// Cannot begin a resource within a merged render pass region.
		checkf(GetPrologueBarrierPassHandle(PassHandle) == PassHandle,
			TEXT("Cannot begin a resource within a merged render pass. Pass (Handle: %d, Name: %s), Resource %s"), PassHandle, Pass->GetName(), Texture->Name);
	}
#endif

	if (TransientResourceAllocator && IsTransient(Texture))
	{
		if (FRHITransientTexture* TransientTexture = TransientResourceAllocator->CreateTexture(Texture->Desc, Texture->Name, PassHandle.GetIndex()))
		{
			if (Texture->bExternal || Texture->bExtracted)
			{
				SetRHI(Texture, GRDGTransientResourceAllocator.AllocateRenderTarget(TransientTexture), PassHandle);
			}
			else
			{
				SetRHI(Texture, TransientTexture, PassHandle);
			}

			const FRDGPassHandle MinAcquirePassHandle(TransientTexture->GetAcquirePasses().Min);

			AddAliasingTransition(MinAcquirePassHandle, PassHandle, Texture, FRHITransientAliasingInfo::Acquire(TransientTexture->GetRHI(), TransientTexture->GetAliasingOverlaps()));

			FRDGSubresourceState InitialState;
			InitialState.SetPass(ERHIPipeline::Graphics, MinAcquirePassHandle);
			InitialState.Access = ERHIAccess::Discard;
			InitTextureSubresources(*Texture->State, Texture->Layout, InitialState);

		#if STATS
			GRDGStatTransientTextureCount++;
		#endif
		}
	}

	if (!Texture->ResourceRHI)
	{
		SetRHI(Texture, GRenderTargetPool.FindFreeElement(Texture->Desc, Texture->Name), PassHandle);
	}
}

void FRDGBuilder::InitRHI(FRDGTextureSRVRef SRV)
{
	check(SRV);

	if (SRV->HasRHI())
	{
		return;
	}

	FRDGTextureRef Texture = SRV->Desc.Texture;
	FRHITexture* TextureRHI = Texture->GetRHIUnchecked();
	check(TextureRHI);

	SRV->ResourceRHI = Texture->ViewCache->GetOrCreateSRV(TextureRHI, SRV->Desc);
}

void FRDGBuilder::InitRHI(FRDGTextureUAVRef UAV)
{
	check(UAV);

	if (UAV->HasRHI())
	{
		return;
	}

	FRDGTextureRef Texture = UAV->Desc.Texture;
	FRHITexture* TextureRHI = Texture->GetRHIUnchecked();
	check(TextureRHI);

	UAV->ResourceRHI = Texture->ViewCache->GetOrCreateUAV(TextureRHI, UAV->Desc);
}

void FRDGBuilder::BeginResourceRHI(FRDGPassHandle PassHandle, FRDGBufferRef Buffer)
{
	check(Buffer);

	if (Buffer->HasRHI())
	{
		return;
	}

	check(Buffer->ReferenceCount > 0 || Buffer->bExternal || Buffer->bQueuedForUpload || IsImmediateMode());

#if RDG_ENABLE_DEBUG
	{
		const FRDGPass* Pass = Passes[PassHandle];

		// Cannot begin a resource on an async compute pass.
		check(Pass->Pipeline == ERHIPipeline::Graphics);

		// Cannot begin a resource within a merged render pass region.
		checkf(GetPrologueBarrierPassHandle(PassHandle) == PassHandle,
			TEXT("Cannot begin a resource within a merged render pass. Pass (Handle: %d, Name: %s), Resource %s"), PassHandle, Pass->GetName(), Buffer->Name);
	}
#endif
	Buffer->FinalizeDesc();

	// If transient then create the resource on the transient allocator. External or extracted resource can't be transient because of lifetime tracking issues.
	if (TransientResourceAllocator && IsTransient(Buffer))
	{
		if (FRHITransientBuffer* TransientBuffer = TransientResourceAllocator->CreateBuffer(Translate(Buffer->Desc), Buffer->Name, PassHandle.GetIndex()))
		{
			SetRHI(Buffer, TransientBuffer, PassHandle);

			const FRDGPassHandle MinAcquirePassHandle(TransientBuffer->GetAcquirePasses().Min);

			AddAliasingTransition(MinAcquirePassHandle, PassHandle, Buffer, FRHITransientAliasingInfo::Acquire(TransientBuffer->GetRHI(), TransientBuffer->GetAliasingOverlaps()));

			FRDGSubresourceState* InitialState = Buffer->State;
			InitialState->SetPass(ERHIPipeline::Graphics, MinAcquirePassHandle);
			InitialState->Access = ERHIAccess::Discard;

		#if STATS
			GRDGStatTransientBufferCount++;
		#endif
		}
	}

	if (!Buffer->bTransient)
	{
		const ERDGPooledBufferAlignment Alignment = Buffer->bQueuedForUpload ? ERDGPooledBufferAlignment::PowerOfTwo : ERDGPooledBufferAlignment::Page;

		SetRHI(Buffer, GRenderGraphResourcePool.FindFreeBuffer(Buffer->Desc, Buffer->Name, Alignment), PassHandle);
	}
}

void FRDGBuilder::InitRHI(FRDGBufferSRVRef SRV)
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

	SRV->ResourceRHI = Buffer->ViewCache->GetOrCreateSRV(BufferRHI, SRVCreateInfo);
}

void FRDGBuilder::InitRHI(FRDGBufferUAV* UAV)
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

	UAV->ResourceRHI = Buffer->ViewCache->GetOrCreateUAV(Buffer->GetRHIUnchecked(), UAVCreateInfo);
}

void FRDGBuilder::InitRHI(FRDGView* View)
{
	if (View->HasRHI())
	{
		return;
	}

	switch (View->Type)
	{
	case ERDGViewType::TextureUAV:
		InitRHI(static_cast<FRDGTextureUAV*>(View));
		break;
	case ERDGViewType::TextureSRV:
		InitRHI(static_cast<FRDGTextureSRV*>(View));
		break;
	case ERDGViewType::BufferUAV:
		InitRHI(static_cast<FRDGBufferUAV*>(View));
		break;
	case ERDGViewType::BufferSRV:
		InitRHI(static_cast<FRDGBufferSRV*>(View));
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FRDGBuilder::EndResourceRHI(FRDGPassHandle PassHandle, FRDGTextureRef Texture, uint32 ReferenceCount)
{
	check(Texture);
	check(Texture->ReferenceCount != FRDGViewableResource::DeallocatedReferenceCount);
	check(Texture->ReferenceCount >= ReferenceCount || IsImmediateMode());
	Texture->ReferenceCount -= ReferenceCount;

	if (Texture->ReferenceCount == 0)
	{
		if (Texture->bTransient)
		{
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
				TransientResourceAllocator->DeallocateMemory(Texture->TransientTexture, PassHandle.GetIndex());
			}
		}
		else
		{
			// Only tracked render targets are released. Untracked ones persist until the end of the frame.
			if (static_cast<FPooledRenderTarget*>(Texture->RenderTarget)->IsTracked())
			{
				// This releases the reference without invoking a virtual function call.
				TRefCountPtr<FPooledRenderTarget>(MoveTemp(Texture->Allocation));
			}
		}

		Texture->LastPass = PassHandle;
		Texture->ReferenceCount = FRDGViewableResource::DeallocatedReferenceCount;
	}
}

void FRDGBuilder::EndResourceRHI(FRDGPassHandle PassHandle, FRDGBufferRef Buffer, uint32 ReferenceCount)
{
	check(Buffer);
	check(Buffer->ReferenceCount != FRDGViewableResource::DeallocatedReferenceCount);
	check(Buffer->ReferenceCount >= ReferenceCount || IsImmediateMode());
	Buffer->ReferenceCount -= ReferenceCount;

	if (Buffer->ReferenceCount == 0)
	{
		if (Buffer->bTransient)
		{
			TransientResourceAllocator->DeallocateMemory(Buffer->TransientBuffer, PassHandle.GetIndex());
		}
		else
		{
			Buffer->Allocation = nullptr;
		}

		Buffer->LastPass = PassHandle;
		Buffer->ReferenceCount = FRDGViewableResource::DeallocatedReferenceCount;
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
				if (TextureAccess.GetAccess() == ERHIAccess::UAVCompute ||
					TextureAccess.GetAccess() == ERHIAccess::UAVGraphics ||
					TextureAccess.GetAccess() == ERHIAccess::RTV)
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
	if (!GRDGClobberResources || !AuxiliaryPasses.IsClobberAllowed())
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
			if (FRDGTextureAccess TextureAccess = Parameter.GetAsTextureAccess())
			{
				FRDGTextureRef Texture = TextureAccess.GetTexture();

				if (UserValidation.TryMarkForClobber(Texture))
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
	// Initialize set of external buffers
	TSet<FRHIBuffer*> ExternalBufferSet;
	ExternalBufferSet.Reserve(ExternalBuffers.Num());

	for (auto ExternalBufferIt = ExternalBuffers.CreateConstIterator(); ExternalBufferIt; ++ExternalBufferIt)
	{
		ExternalBufferSet.Emplace(ExternalBufferIt.Value()->GetRHIUnchecked());
	}

	// Generate list of cross GPU resources from all passes, and the GPU mask where they were last written 
	TMap<FRHIBuffer*, FRHIGPUMask> BuffersToTransfer;
	TMap<FRHITexture*, FRHIGPUMask> TexturesToTransfer;

	for (FRDGPassHandle PassHandle = GetProloguePassHandle(); PassHandle <= GetEpiloguePassHandle(); ++PassHandle)
	{
		FRDGPass* Pass = Passes[PassHandle];

		for (int32 BufferIndex = 0; BufferIndex < Pass->BufferStates.Num(); BufferIndex++)
		{
			FRHIBuffer* BufferRHI = Pass->BufferStates[BufferIndex].Buffer->GetRHIUnchecked();

			if (ExternalBufferSet.Contains(BufferRHI) &&
				!EnumHasAnyFlags(BufferRHI->GetUsage(), BUF_MultiGPUAllocate | BUF_MultiGPUGraphIgnore) &&
				EnumHasAnyFlags(Pass->BufferStates[BufferIndex].State.Access, ERHIAccess::WritableMask))
			{
				BuffersToTransfer.Emplace(BufferRHI) = Pass->GPUMask;
			}
		}

		for (int32 TextureIndex = 0; TextureIndex < Pass->TextureStates.Num(); TextureIndex++)
		{
			if (ExternalTextures.Contains(Pass->TextureStates[TextureIndex].Texture->GetRHIUnchecked()))
			{
				for (int32 StateIndex = 0; StateIndex < Pass->TextureStates[TextureIndex].State.Num(); StateIndex++)
				{
					FRHITexture* TextureRHI = Pass->TextureStates[TextureIndex].Texture->GetRHIUnchecked();

					if (TextureRHI &&
						!EnumHasAnyFlags(TextureRHI->GetFlags(), TexCreate_MultiGPUGraphIgnore) &&
						EnumHasAnyFlags(Pass->TextureStates[TextureIndex].State[StateIndex].Access, ERHIAccess::WritableMask))
					{
						TexturesToTransfer.Emplace(Pass->TextureStates[TextureIndex].Texture->GetRHIUnchecked()) = Pass->GPUMask;
					}
				}
			}
		}
	}

	// Now that we've got the list of external resources, and the GPU they were last written to, make a list of what needs to
	// be propagated to other GPUs.
	TArray<FTransferResourceParams> Transfers;
	const FRHIGPUMask AllGPUMask = FRHIGPUMask::All();
	const bool bPullData = false;
	const bool bLockstepGPUs = true;

	for (auto BufferIt = BuffersToTransfer.CreateConstIterator(); BufferIt; ++BufferIt)
	{
		FRHIBuffer* Buffer = BufferIt.Key();
		FRHIGPUMask GPUMask = BufferIt.Value();

		for (uint32 GPUIndex : AllGPUMask)
		{
			if (!GPUMask.Contains(GPUIndex))
			{
				Transfers.Add(FTransferResourceParams(Buffer, GPUMask.GetFirstIndex(), GPUIndex, bPullData, bLockstepGPUs));
			}
		}
	}

	for (auto TextureIt = TexturesToTransfer.CreateConstIterator(); TextureIt; ++TextureIt)
	{
		FRHITexture* Texture = TextureIt.Key();
		FRHIGPUMask GPUMask = TextureIt.Value();

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