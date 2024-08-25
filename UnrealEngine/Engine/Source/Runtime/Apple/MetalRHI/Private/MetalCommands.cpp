// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommands.cpp: Metal RHI commands implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalComputePipelineState.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "StaticBoundShaderState.h"
#include "EngineGlobals.h"
#include "PipelineStateCache.h"
#include "RHICoreShader.h"
#include "RHIShaderParametersShared.h"
#include "RHIUtilities.h"

static const bool GUsesInvertedZ = true;

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
static TGlobalResource<FVector4VertexDeclaration> FVector4VertexDeclaration;

MTL::PrimitiveType TranslatePrimitiveType(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:	return MTL::PrimitiveTypeTriangle;
		case PT_TriangleStrip:	return MTL::PrimitiveTypeTriangleStrip;
		case PT_LineList:		return MTL::PrimitiveTypeLine;
		case PT_PointList:		return MTL::PrimitiveTypePoint;
		default:
			METAL_FATAL_ERROR(TEXT("Unsupported primitive type %d"), (int32)PrimitiveType);
			return MTL::PrimitiveTypeTriangle;
	}
}

static FORCEINLINE EMetalShaderStages GetShaderStage(FRHIGraphicsShader* ShaderRHI)
{
	EMetalShaderStages Stage = EMetalShaderStages::Num;
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:		Stage = EMetalShaderStages::Vertex; break;
	case SF_Pixel:		Stage = EMetalShaderStages::Pixel; break;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    case SF_Geometry:   Stage = EMetalShaderStages::Geometry; break;
#endif
#if PLATFORM_SUPPORTS_MESH_SHADERS
    case SF_Mesh:               Stage = EMetalShaderStages::Mesh; break;
    case SF_Amplification:      Stage = EMetalShaderStages::Amplification; break;
#endif
	default:
		checkf(0, TEXT("FRHIShader Type %d is invalid or unsupported!"), (int32)ShaderRHI->GetFrequency());
		NOT_SUPPORTED("RHIShaderStage");
		break;
	}

	return Stage;
}

void FMetalRHICommandContext::RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBufferRHI,uint32 Offset)
{
    MTL_SCOPED_AUTORELEASE_POOL;;
    
    FMetalRHIBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
    
    FMetalBufferPtr TheBuffer = nullptr;
    if(VertexBuffer && !VertexBuffer->Data)
    {
        TheBuffer = VertexBuffer->GetCurrentBuffer();
    }
    
    Context->GetCurrentState().SetVertexStream(StreamIndex, VertexBuffer ? TheBuffer : nullptr, VertexBuffer ? VertexBuffer->Data : nullptr, Offset, VertexBuffer ? VertexBuffer->GetSize() : 0);
}

template <typename TRHIShader>
void FMetalRHICommandContext::ApplyStaticUniformBuffers(TRHIShader* Shader)
{
	if (Shader)
	{
		UE::RHICore::ApplyStaticUniformBuffers(this, Shader, Shader->StaticSlots, Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes, GlobalUniformBuffers);
	}
}

void FMetalRHICommandContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
    MTL_SCOPED_AUTORELEASE_POOL;
	
    FMetalComputePipelineState* ComputePipeline = ResourceCast(ComputePipelineState);
	
	// cache this for Dispatch
	// sets this compute shader pipeline as the current (this resets all state, so we need to set all resources after calling this)
	Context->GetCurrentState().SetComputeShader(ComputePipeline->GetComputeShader());

	ApplyStaticUniformBuffers(ComputePipeline->GetComputeShader());
    
}

void FMetalRHICommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	ThreadGroupCountX = FMath::Max(ThreadGroupCountX, 1u);
	ThreadGroupCountY = FMath::Max(ThreadGroupCountY, 1u);
	ThreadGroupCountZ = FMath::Max(ThreadGroupCountZ, 1u);
	
	Context->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FMetalRHICommandContext::RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		FMetalRHIBuffer* VertexBuffer = ResourceCast(ArgumentBufferRHI);
		
		Context->DispatchIndirect(VertexBuffer, ArgumentOffset);
	}
	else
	{
		NOT_SUPPORTED("RHIDispatchIndirectComputeShader");
	}
}

void FMetalRHICommandContext::RHISetViewport(float MinX, float MinY,float MinZ, float MaxX, float MaxY,float MaxZ)
{
    MTL_SCOPED_AUTORELEASE_POOL;

	MTL::Viewport Viewport;
	Viewport.originX = MinX;
	Viewport.originY = MinY;
	Viewport.width = MaxX - MinX;
	Viewport.height = MaxY - MinY;
	Viewport.znear = MinZ;
	Viewport.zfar = MaxZ;
	
	Context->GetCurrentState().SetViewport(Viewport);
}

void FMetalRHICommandContext::RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports))
	{
        MTL_SCOPED_AUTORELEASE_POOL;
        
		MTL::Viewport Viewport[2];
		
		Viewport[0].originX = LeftMinX;
		Viewport[0].originY = LeftMinY;
		Viewport[0].width = LeftMaxX - LeftMinX;
		Viewport[0].height = LeftMaxY - LeftMinY;
		Viewport[0].znear = MinZ;
		Viewport[0].zfar = MaxZ;
		
		Viewport[1].originX = RightMinX;
		Viewport[1].originY = RightMinY;
		Viewport[1].width = RightMaxX - RightMinX;
		Viewport[1].height = RightMaxY - RightMinY;
		Viewport[1].znear = MinZ;
		Viewport[1].zfar = MaxZ;
		
		Context->GetCurrentState().SetViewports(Viewport, 2);
	}
	else
	{
		NOT_SUPPORTED("RHISetStereoViewport");
	}
}

void FMetalRHICommandContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{ 
	NOT_SUPPORTED("RHISetMultipleViewports");
}

void FMetalRHICommandContext::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	MTL::ScissorRect Scissor;
	Scissor.x = MinX;
	Scissor.y = MinY;
	Scissor.width = MaxX - MinX;
	Scissor.height = MaxY - MinY;

	// metal doesn't support 0 sized scissor rect
	if (bEnable == false || Scissor.width == 0 || Scissor.height == 0)
	{
		MTL::Viewport const& Viewport = Context->GetCurrentState().GetViewport(0);
		CGSize FBSize = Context->GetCurrentState().GetFrameBufferSize();
		
		Scissor.x = Viewport.originX;
		Scissor.y = Viewport.originY;
		Scissor.width = (Viewport.originX + Viewport.width <= FBSize.width) ? Viewport.width : FBSize.width - Viewport.originX;
		Scissor.height = (Viewport.originY + Viewport.height <= FBSize.height) ? Viewport.height : FBSize.height - Viewport.originY;
	}
	Context->GetCurrentState().SetScissorRect(bEnable, Scissor);
}

void FMetalRHICommandContext::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    FMetalGraphicsPipelineState* PipelineState = ResourceCast(GraphicsState);
    if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelResetOnBind && Context->GetCurrentState().GetGraphicsPSO() != PipelineState)
    {
        Context->GetCurrentRenderPass().GetCurrentCommandEncoder().ResetLive();
    }
    Context->GetCurrentState().SetGraphicsPipelineState(PipelineState);

    RHISetStencilRef(StencilRef);
    RHISetBlendFactor(FLinearColor(1.0f, 1.0f, 1.0f));

    if (bApplyAdditionalState)
    {
#if PLATFORM_SUPPORTS_MESH_SHADERS
        if (IsValidRef(PipelineState->MeshShader))
        {
            ApplyStaticUniformBuffers(PipelineState->MeshShader.GetReference());
            if (IsValidRef(PipelineState->AmplificationShader))
            {
                ApplyStaticUniformBuffers(PipelineState->AmplificationShader.GetReference());
            }
        }
        else if (IsValidRef(PipelineState->VertexShader))
#endif
		{
			ApplyStaticUniformBuffers(PipelineState->VertexShader.GetReference());
		}
		
		ApplyStaticUniformBuffers(PipelineState->PixelShader.GetReference());
    }
}

void FMetalRHICommandContext::RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers)
{
	FMemory::Memzero(GlobalUniformBuffers.GetData(), GlobalUniformBuffers.Num() * sizeof(FRHIUniformBuffer*));

	for (int32 Index = 0; Index < InUniformBuffers.GetUniformBufferCount(); ++Index)
	{
		GlobalUniformBuffers[InUniformBuffers.GetSlot(Index)] = InUniformBuffers.GetUniformBuffer(Index);
	}
}

void FMetalRHICommandContext::RHISetUAVParameter(FRHIPixelShader* PixelShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	Context->GetCurrentState().SetShaderUnorderedAccessView(EMetalShaderStages::Pixel, UAVIndex, UAV);
}

void FMetalRHICommandContext::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI, uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	Context->GetCurrentState().SetShaderUnorderedAccessView(EMetalShaderStages::Compute, UAVIndex, UAV);
}

void FMetalRHICommandContext::RHISetUAVParameter(FRHIComputeShader* ComputeShaderRHI,uint32 UAVIndex, FRHIUnorderedAccessView* UAVRHI, uint32 InitialCount)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalUnorderedAccessView* UAV = ResourceCast(UAVRHI);
    Context->GetCurrentState().SetShaderUnorderedAccessView(EMetalShaderStages::Compute, UAVIndex, UAV);
}


void FMetalRHICommandContext::RHISetShaderTexture(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	EMetalShaderStages Stage = GetShaderStage(ShaderRHI);
	if (Surface != nullptr)
	{
        if (Surface->Texture || !(Surface->GetDesc().Flags & TexCreate_Presentable))
        {
            Context->GetCurrentState().SetShaderTexture(Stage, Surface->Texture.get(), TextureIndex, (MTL::ResourceUsage)(MTL::ResourceUsageRead|MTL::ResourceUsageSample));
        }
        else
        {
            MTLTexturePtr Tex = Surface->GetCurrentTexture();
            Context->GetCurrentState().SetShaderTexture(Stage, Tex.get(), TextureIndex, (MTL::ResourceUsage)(MTL::ResourceUsageRead|MTL::ResourceUsageSample));
        }
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(Stage, nullptr, TextureIndex, MTL::ResourceUsage(0));
	}
}

void FMetalRHICommandContext::RHISetShaderTexture(FRHIComputeShader* ComputeShader, uint32 TextureIndex, FRHITexture* NewTextureRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
    {
        if (Surface->Texture || !(Surface->GetDesc().Flags & TexCreate_Presentable))
        {
            Context->GetCurrentState().SetShaderTexture(EMetalShaderStages::Compute, Surface->Texture.get(), TextureIndex, (MTL::ResourceUsage)(MTL::ResourceUsageRead|MTL::ResourceUsageSample));
        }
        else
        {
            MTLTexturePtr Tex = Surface->GetCurrentTexture();
            Context->GetCurrentState().SetShaderTexture(EMetalShaderStages::Compute, Tex.get(), TextureIndex, (MTL::ResourceUsage)(MTL::ResourceUsageRead|MTL::ResourceUsageSample));
        }
	}
	else
	{
		Context->GetCurrentState().SetShaderTexture(EMetalShaderStages::Compute, nullptr, TextureIndex, MTL::ResourceUsage(0));
	}
}


void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FRHIGraphicsShader* ShaderRHI, uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	EMetalShaderStages Stage = GetShaderStage(ShaderRHI);
	Context->GetCurrentState().SetShaderResourceView(Stage, TextureIndex, SRV);
}

void FMetalRHICommandContext::RHISetShaderResourceViewParameter(FRHIComputeShader* ComputeShaderRHI, uint32 TextureIndex, FRHIShaderResourceView* SRVRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	Context->GetCurrentState().SetShaderResourceView(EMetalShaderStages::Compute, TextureIndex, SRV);
}


void FMetalRHICommandContext::RHISetShaderSampler(FRHIGraphicsShader* ShaderRHI, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);
	EMetalShaderStages Stage = GetShaderStage(ShaderRHI);
	Context->GetCurrentState().SetShaderSamplerState(Stage, NewState, SamplerIndex);
}

void FMetalRHICommandContext::RHISetShaderSampler(FRHIComputeShader* ComputeShader, uint32 SamplerIndex, FRHISamplerState* NewStateRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);
	Context->GetCurrentState().SetShaderSamplerState(EMetalShaderStages::Compute, NewState, SamplerIndex);
}

void FMetalRHICommandContext::RHISetShaderParameter(FRHIGraphicsShader* ShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	EMetalShaderStages Stage = GetShaderStage(ShaderRHI);
	Context->GetCurrentState().GetShaderParameters(Stage).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalRHICommandContext::RHISetShaderParameter(FRHIComputeShader* ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
    MTL_SCOPED_AUTORELEASE_POOL;
	Context->GetCurrentState().GetShaderParameters(EMetalShaderStages::Compute).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalRHICommandContext::RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters)
{
	// TODO: CL - Implement metal specific version of RHISetShaderParametersShared
	UE::RHICore::RHISetShaderParametersShared(
		*this
		, Shader
		, InParametersData
		, InParameters
		, InResourceParameters
		, InBindlessParameters
	);

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if(IsMetalBindlessEnabled())
	{
		EMetalShaderStages Stage = GetShaderStage(Shader);
		Context->GetCurrentState().IRForwardBindlessParameters(Stage, InResourceParameters);
		Context->GetCurrentState().IRForwardBindlessParameters(Stage, InBindlessParameters);
	}
#endif
}

void FMetalRHICommandContext::RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters)
{
	UE::RHICore::RHISetShaderParametersShared(
		*this
		, Shader
		, InParametersData
		, InParameters
		, InResourceParameters
		, InBindlessParameters
	);

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if(IsMetalBindlessEnabled())
	{
		Context->GetCurrentState().IRForwardBindlessParameters(EMetalShaderStages::Compute, InResourceParameters);
		Context->GetCurrentState().IRForwardBindlessParameters(EMetalShaderStages::Compute, InBindlessParameters);
	}
#endif
}

void FMetalRHICommandContext::RHISetStencilRef(uint32 StencilRef)
{
	Context->GetCurrentState().SetStencilRef(StencilRef);
}

void FMetalRHICommandContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	Context->GetCurrentState().SetBlendFactor(BlendFactor);
}

void FMetalRHICommandContext::SetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalContext* Manager = Context;
	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTargetRHI)
	{
		DepthView = *NewDepthStencilTargetRHI;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(nullptr, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	}

	FRHISetRenderTargetsInfo Info(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	SetRenderTargetsAndClear(Info);
}

void FMetalRHICommandContext::SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
    MTL_SCOPED_AUTORELEASE_POOL;
		
	FRHIRenderPassInfo PassInfo;
	bool bHasTarget = (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr);
	FMetalContext* Manager = Context;
	
	for (uint32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; i++)
	{
		if (RenderTargetsInfo.ColorRenderTarget[i].Texture)
		{
			PassInfo.ColorRenderTargets[i].RenderTarget = RenderTargetsInfo.ColorRenderTarget[i].Texture;
			PassInfo.ColorRenderTargets[i].ArraySlice = RenderTargetsInfo.ColorRenderTarget[i].ArraySliceIndex;
			PassInfo.ColorRenderTargets[i].MipIndex = RenderTargetsInfo.ColorRenderTarget[i].MipIndex;
			PassInfo.ColorRenderTargets[i].Action = MakeRenderTargetActions(RenderTargetsInfo.ColorRenderTarget[i].LoadAction, RenderTargetsInfo.ColorRenderTarget[i].StoreAction);
		bHasTarget = (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr);
		}
	}
		
	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		PassInfo.DepthStencilRenderTarget.DepthStencilTarget = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		PassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess();
		PassInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(MakeRenderTargetActions(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction, RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction), MakeRenderTargetActions(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction, RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction()));
	}
		
	PassInfo.NumOcclusionQueries = UINT16_MAX;
	PassInfo.bOcclusionQueries = true;

	// Ignore any attempt to "clear" the render-targets as that is senseless with the way MetalRHI has to try and coalesce passes.
	if (bHasTarget)
	{
		Manager->SetRenderPassInfo(PassInfo);

		// Set the viewport to the full size of render target 0.
		if (RenderTargetsInfo.ColorRenderTarget[0].Texture)
		{
			const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[0];
			FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);

			uint32 Width = FMath::Max((uint32)(RenderTarget->Texture->width() >> RenderTargetView.MipIndex), (uint32)1);
			uint32 Height = FMath::Max((uint32)(RenderTarget->Texture->height() >> RenderTargetView.MipIndex), (uint32)1);

			RHISetViewport(0, 0, 0.0f, Width, Height, 1.0f);
		}
	}
}


void FMetalRHICommandContext::RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		
	uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	
	NumInstances = FMath::Max(NumInstances,1u);
	
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	// how many verts to render
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	
	Context->DrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
}

void FMetalRHICommandContext::RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
    {
        SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
        uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
        
        
        RHI_DRAW_CALL_STATS(PrimitiveType,1);
        FMetalRHIBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
        
        Context->DrawPrimitiveIndirect(PrimitiveType, ArgumentBuffer, ArgumentOffset);
    }
    else
    {
        NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
    }
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
void FMetalRHICommandContext::RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    MTL_SCOPED_AUTORELEASE_POOL;

#if METAL_USE_METAL_SHADER_CONVERTER
	uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
	Context->DispatchMeshShader(PrimitiveType, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
#else
	NOT_SUPPORTED("RHIDispatchMeshShader");
#endif
}

void FMetalRHICommandContext::RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;

#if METAL_USE_METAL_SHADER_CONVERTER
	uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();

	FMetalRHIBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
	Context->DispatchIndirectMeshShader(PrimitiveType, ArgumentBuffer, ArgumentOffset);
#else
	NOT_SUPPORTED("RHIDispatchIndirectMeshShader");
#endif
}
#endif

void FMetalRHICommandContext::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	checkf(GRHISupportsBaseVertexIndex || BaseVertexIndex == 0, TEXT("BaseVertexIndex must be 0, see GRHISupportsBaseVertexIndex"));
	checkf(GRHISupportsFirstInstance || FirstInstance == 0, TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));
	uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
	
		
	RHI_DRAW_CALL_STATS(PrimitiveType,FMath::Max(NumInstances,1u)*NumPrimitives);

	FMetalRHIBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	Context->DrawIndexedPrimitive(IndexBuffer->GetCurrentBuffer(), IndexBuffer->GetStride(), IndexBuffer->GetIndexType(), PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FMetalRHICommandContext::RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 /*NumInstances*/)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		
		uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
		

		RHI_DRAW_CALL_STATS(PrimitiveType,1);
		FMetalRHIBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		FMetalRHIBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);
		
		Context->DrawIndexedIndirect(IndexBuffer, PrimitiveType, ArgumentsBuffer, DrawArgumentsIndex);
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedIndirect");
	}
}

void FMetalRHICommandContext::RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		
		uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
		

		RHI_DRAW_CALL_STATS(PrimitiveType,1);
		FMetalRHIBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		FMetalRHIBuffer* ArgumentsBuffer = ResourceCast(ArgumentBufferRHI);
		
		Context->DrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
	}
}

void FMetalRHICommandContext::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil)
{
	NOT_SUPPORTED("RHIClearMRT");
}

void FMetalRHICommandContext::RHISetDepthBounds(float MinDepth, float MaxDepth)
{
	METAL_IGNORED(FMetalRHICommandContextSetDepthBounds);
}

void FMetalRHICommandContext::RHISubmitCommandsHint()
{
    MTL_SCOPED_AUTORELEASE_POOL;
    Context->SubmitCommandsHint();
}

void FMetalRHICommandContext::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	Context->GetCurrentState().DiscardRenderTargets(Depth, Stencil, ColorBitMask);
}

#if PLATFORM_USES_FIXED_RHI_CLASS
#define INTERNAL_DECORATOR(Method) ((FMetalRHICommandContext&)CmdList.GetContext()).FMetalRHICommandContext::Method
#include "RHICommandListCommandExecutes.inl"
#endif
