// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalRHIRenderQuery.h"
#include "MetalCommandBufferFence.h"

TGlobalResource<TBoundShaderStateHistory<10000>> FMetalRHICommandContext::BoundShaderStateHistory;

FMetalDeviceContext& GetMetalDeviceContext()
{
	FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
	check(Context);
	return ((FMetalDeviceContext&)Context->GetInternalContext());
}

void SafeReleaseMetalObject(id Object)
{
	if(GIsMetalInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseObject(Object);
			return;
		}
	}
	[Object release];
}

void SafeReleaseMetalTexture(FMetalTexture& Object)
{
	if(GIsMetalInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseTexture(Object);
			return;
		}
	}
}

void SafeReleaseMetalBuffer(FMetalBuffer& Buffer)
{
	if(GIsMetalInitialized && GDynamicRHI && Buffer)
	{
		Buffer.SetOwner(nullptr, false);
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseBuffer(Buffer);
		}
	}
}

void SafeReleaseMetalFence(FMetalFence* Object)
{
	if(GIsMetalInitialized && GDynamicRHI && Object)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseFence(Object);
			return;
		}
	}
}

FMetalRHICommandContext::FMetalRHICommandContext(class FMetalProfiler* InProfiler, FMetalContext* WrapContext)
: Context(WrapContext)
, Profiler(InProfiler)
, PendingVertexDataStride(0)
, PendingIndexDataStride(0)
, PendingPrimitiveType(0)
, PendingNumPrimitives(0)
{
	check(Context);
	GlobalUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
}

FMetalRHICommandContext::~FMetalRHICommandContext()
{
	delete Context;
}

FMetalRHIComputeContext::FMetalRHIComputeContext(class FMetalProfiler* InProfiler, FMetalContext* WrapContext)
: FMetalRHICommandContext(InProfiler, WrapContext)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesFences) && FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 14, 0}, (uint32[]){12, 0, 0}, (uint32[]){12, 0, 0}))
	{
		WrapContext->GetCurrentRenderPass().SetDispatchType(mtlpp::DispatchType::Concurrent);
	}
}

FMetalRHIComputeContext::~FMetalRHIComputeContext()
{
}

void FMetalRHIComputeContext::RHISetAsyncComputeBudget(EAsyncComputeBudget Budget)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	FMetalRHICommandContext::RHISetAsyncComputeBudget(Budget);
}

void FMetalRHIComputeContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	FMetalRHICommandContext::RHISetComputePipelineState(ComputePipelineState);
}

void FMetalRHIComputeContext::RHISubmitCommandsHint()
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false, 0, 0);
	}
	Context->FinishFrame(false);
	
#if ENABLE_METAL_GPUPROFILE
	FMetalContext::MakeCurrent(&GetMetalDeviceContext());
#endif
}

FMetalRHIImmediateCommandContext::FMetalRHIImmediateCommandContext(class FMetalProfiler* InProfiler, FMetalContext* WrapContext)
	: FMetalRHICommandContext(InProfiler, WrapContext)
{
}

void FMetalRHICommandContext::RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
{
	SCOPED_AUTORELEASE_POOL;
	
	if (InInfo.NumOcclusionQueries > 0)
	{
		Context->GetCommandList().SetParallelIndex(0, 0);
	}

	Context->SetRenderPassInfo(InInfo);

	// Set the viewport to the full size of render target 0.
	if (InInfo.ColorRenderTargets[0].RenderTarget)
	{
		const FRHIRenderPassInfo::FColorEntry& RenderTargetView = InInfo.ColorRenderTargets[0];
		FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.RenderTarget);

		uint32 Width = FMath::Max((uint32)(RenderTarget->Texture.GetWidth() >> RenderTargetView.MipIndex), (uint32)1);
		uint32 Height = FMath::Max((uint32)(RenderTarget->Texture.GetHeight() >> RenderTargetView.MipIndex), (uint32)1);

		RHISetViewport(0.0f, 0.0f, 0.0f, (float)Width, (float)Height, 1.0f);
	}
	
	RenderPassInfo = InInfo;
	if (InInfo.NumOcclusionQueries > 0)
	{
		RHIBeginOcclusionQueryBatch(InInfo.NumOcclusionQueries);
	}
}

void FMetalRHICommandContext::RHIEndRenderPass()
{
	if (RenderPassInfo.NumOcclusionQueries > 0)
	{
		RHIEndOcclusionQueryBatch();
	}

	UE::RHICore::ResolveRenderPassTargets(RenderPassInfo, [this](UE::RHICore::FResolveTextureInfo Info)
	{
		ResolveTexture(Info);
	});
}

void FMetalRHICommandContext::ResolveTexture(UE::RHICore::FResolveTextureInfo Info)
{
	@autoreleasepool{
	FMetalSurface* Source = GetMetalSurfaceFromRHITexture(Info.SourceTexture);
	FMetalSurface* Destination = GetMetalSurfaceFromRHITexture(Info.DestTexture);

	const FRHITextureDesc& SourceDesc = Source->GetDesc();
	const FRHITextureDesc& DestinationDesc = Destination->GetDesc();

	const bool bDepthStencil = SourceDesc.Format == PF_DepthStencil;
	const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
	const bool bSupportsMSAAStoreAndResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAAStoreAndResolve);
	// Resolve required - Device must support this - Using Shader for resolve not supported amd NumSamples should be 1
	check((!bDepthStencil && bSupportsMSAAStoreAndResolve) || (bDepthStencil && bSupportsMSAADepthResolve));

	mtlpp::Origin Origin(0, 0, 0);
	mtlpp::Size Size(0, 0, 1);

	if (Info.ResolveRect.IsValid())
	{
		Origin.x    = Info.ResolveRect.X1;
		Origin.y    = Info.ResolveRect.Y1;
		Size.width  = Info.ResolveRect.X2 - Info.ResolveRect.X1;
		Size.height = Info.ResolveRect.Y2 - Info.ResolveRect.Y1;
	}
	else
	{
		Size.width  = FMath::Max<uint32>(1, SourceDesc.Extent.X >> Info.MipLevel);
		Size.height = FMath::Max<uint32>(1, SourceDesc.Extent.Y >> Info.MipLevel);
	}

	if (Profiler)
	{
		Profiler->RegisterGPUWork();
	}

	int32 ArraySliceBegin = Info.ArraySlice;
	int32 ArraySliceEnd   = Info.ArraySlice + 1;

	if (Info.ArraySlice < 0)
	{
		ArraySliceBegin = 0;
		ArraySliceEnd   = SourceDesc.ArraySize;
	}

	for (int32 ArraySlice = ArraySliceBegin; ArraySlice < ArraySliceEnd; ArraySlice++)
	{
		Context->CopyFromTextureToTexture(Source->MSAAResolveTexture, ArraySlice, Info.MipLevel, Origin, Size, Destination->Texture, ArraySlice, Info.MipLevel, Origin);
	}
	}
}

void FMetalRHICommandContext::RHINextSubpass()
{
#if PLATFORM_MAC
	if (RenderPassInfo.SubpassHint == ESubpassHint::DepthReadSubpass)
	{
		FMetalRenderPass& RP = Context->GetCurrentRenderPass();
		if (RP.GetCurrentCommandEncoder().IsRenderCommandEncoderActive())
		{
			RP.InsertTextureBarrier();
		}
	}
#endif
}

void FMetalRHICommandContext::RHIBeginRenderQuery(FRHIRenderQuery* QueryRHI)
{
	@autoreleasepool {
		FMetalRHIRenderQuery* Query = ResourceCast(QueryRHI);
		Query->Begin(Context, CommandBufferFence);
	}
}

void FMetalRHICommandContext::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
	@autoreleasepool {
		FMetalRHIRenderQuery* Query = ResourceCast(QueryRHI);
		Query->End(Context);
	}
}

void FMetalRHICommandContext::RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch)
{
	check(!CommandBufferFence.IsValid());
	CommandBufferFence = MakeShareable(new FMetalCommandBufferFence);
}

void FMetalRHICommandContext::RHIEndOcclusionQueryBatch()
{
	check(CommandBufferFence.IsValid());
	Context->InsertCommandBufferFence(*CommandBufferFence);
	CommandBufferFence.Reset();
}
