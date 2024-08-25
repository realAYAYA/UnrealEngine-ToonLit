// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalRHIRenderQuery.h"
#include "MetalRHIVisionOSBridge.h"

TGlobalResource<TBoundShaderStateHistory<10000>> FMetalRHICommandContext::BoundShaderStateHistory;

FMetalDeviceContext& GetMetalDeviceContext()
{
	FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
	check(Context);
	return ((FMetalDeviceContext&)Context->GetInternalContext());
}

void SafeReleaseMetalObject(NS::Object* Object)
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
	Object->release();
}

void SafeReleaseMetalTexture(MTLTexturePtr Object)
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

void SafeReleaseMetalBuffer(FMetalBufferPtr Buffer)
{
	if(GIsMetalInitialized && GDynamicRHI && Buffer)
	{
		Buffer->SetOwner(nullptr, false);
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

void SafeReleaseFunction(TFunction<void()> ReleaseFunc)
{
    if(GIsMetalInitialized && GDynamicRHI)
    {
        FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
        if(Context)
        {
            ((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseFunction(ReleaseFunc);
            return;
        }
    }
}

FMetalRHICommandContext::FMetalRHICommandContext(class FMetalProfiler* InProfiler, FMetalDeviceContext* WrapContext)
	: Context(WrapContext)
	, Profiler(InProfiler)
{
	check(Context);
	GlobalUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
}

FMetalRHICommandContext::~FMetalRHICommandContext()
{
	delete Context;
}

FMetalRHIImmediateCommandContext::FMetalRHIImmediateCommandContext(class FMetalProfiler* InProfiler, FMetalDeviceContext* WrapContext)
	: FMetalRHICommandContext(InProfiler, WrapContext)
{
}

void FMetalRHICommandContext::RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
{
    MTL_SCOPED_AUTORELEASE_POOL;

	Context->SetRenderPassInfo(InInfo);

	// Set the viewport to the full size of render target 0.
	if (InInfo.ColorRenderTargets[0].RenderTarget)
	{
		const FRHIRenderPassInfo::FColorEntry& RenderTargetView = InInfo.ColorRenderTargets[0];
		FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.RenderTarget);

		uint32 Width = FMath::Max((uint32)(RenderTarget->Texture->width() >> RenderTargetView.MipIndex), (uint32)1);
		uint32 Height = FMath::Max((uint32)(RenderTarget->Texture->height() >> RenderTargetView.MipIndex), (uint32)1);

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
    
    Context->EndRenderPass();

	UE::RHICore::ResolveRenderPassTargets(RenderPassInfo, [this](UE::RHICore::FResolveTextureInfo Info)
	{
		ResolveTexture(Info);
	});
}

void FMetalRHICommandContext::ResolveTexture(UE::RHICore::FResolveTextureInfo Info)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalSurface* Source = GetMetalSurfaceFromRHITexture(Info.SourceTexture);
	FMetalSurface* Destination = GetMetalSurfaceFromRHITexture(Info.DestTexture);

	const FRHITextureDesc& SourceDesc = Source->GetDesc();
	const FRHITextureDesc& DestinationDesc = Destination->GetDesc();

	const bool bDepthStencil = SourceDesc.Format == PF_DepthStencil;
	const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
	const bool bSupportsMSAAStoreAndResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAAStoreAndResolve);
	// Resolve required - Device must support this - Using Shader for resolve not supported amd NumSamples should be 1
	check((!bDepthStencil && bSupportsMSAAStoreAndResolve) || (bDepthStencil && bSupportsMSAADepthResolve));

	MTL::Origin Origin(0, 0, 0);
    MTL::Size Size(0, 0, 1);

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
		Context->CopyFromTextureToTexture(Source->MSAAResolveTexture.get(), ArraySlice, Info.MipLevel, Origin, Size, Destination->Texture.get(), ArraySlice, Info.MipLevel, Origin);
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
    MTL_SCOPED_AUTORELEASE_POOL;
	FMetalRHIRenderQuery* Query = ResourceCast(QueryRHI);
    Query->Begin(Context, CommandBufferFence);
}

void FMetalRHICommandContext::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    FMetalRHIRenderQuery* Query = ResourceCast(QueryRHI);
	Query->End(Context);
}

void FMetalRHICommandContext::RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch)
{
    check(!CommandBufferFence.IsValid());
	CommandBufferFence = MakeShareable(new FMetalCommandBufferFence);
    Context->InsertCommandBufferFence(CommandBufferFence, FMetalCommandBufferCompletionHandler());
}

void FMetalRHICommandContext::RHIEndOcclusionQueryBatch()
{
	check(CommandBufferFence.IsValid());
	CommandBufferFence.Reset();
}
