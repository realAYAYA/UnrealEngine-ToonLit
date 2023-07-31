// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalRHIRenderQuery.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalStateCache.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

#if PLATFORM_MAC
	#ifndef UINT128_MAX
		#define UINT128_MAX (((__uint128_t)1 << 127) - (__uint128_t)1 + ((__uint128_t)1 << 127))
	#endif
	#define FMETALTEXTUREMASK_MAX UINT128_MAX
#else
	#define FMETALTEXTUREMASK_MAX UINT32_MAX
#endif

static mtlpp::TriangleFillMode TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch (FillMode)
	{
		case FM_Wireframe:	return mtlpp::TriangleFillMode::Lines;
		case FM_Point:		return mtlpp::TriangleFillMode::Fill;
		default:			return mtlpp::TriangleFillMode::Fill;
	};
}

static mtlpp::CullMode TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
		case CM_CCW:	return mtlpp::CullMode::Front;
		case CM_CW:		return mtlpp::CullMode::Back;
		default:		return mtlpp::CullMode::None;
	}
}

static mtlpp::DepthClipMode TranslateDepthClipMode(ERasterizerDepthClipMode DepthClipMode)
{
	switch (DepthClipMode)
	{
	case ERasterizerDepthClipMode::DepthClip:	return mtlpp::DepthClipMode::Clip;
	case ERasterizerDepthClipMode::DepthClamp:	return mtlpp::DepthClipMode::Clamp;
	default:									return mtlpp::DepthClipMode::Clip;
	}
}

FORCEINLINE mtlpp::StoreAction GetMetalRTStoreAction(ERenderTargetStoreAction StoreAction)
{
	switch(StoreAction)
	{
		case ERenderTargetStoreAction::ENoAction: return mtlpp::StoreAction::DontCare;
		case ERenderTargetStoreAction::EStore: return mtlpp::StoreAction::Store;
		//default store action in the desktop renderers needs to be mtlpp::StoreAction::StoreAndMultisampleResolve.  Trying to express the renderer by the requested maxrhishaderplatform
        //because we may render to the same MSAA target twice in two separate passes.  BasePass, then some stuff, then translucency for example and we need to not lose the prior MSAA contents to do this properly.
		case ERenderTargetStoreAction::EMultisampleResolve:
		{
            static bool bNoMSAA = FParse::Param(FCommandLine::Get(), TEXT("nomsaa"));
			static bool bSupportsMSAAStoreResolve = FMetalCommandQueue::SupportsFeature(EMetalFeaturesMSAAStoreAndResolve) && (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
            if (bNoMSAA)
            {
                return mtlpp::StoreAction::Store;
            }
			else if (bSupportsMSAAStoreResolve)
			{
				return mtlpp::StoreAction::StoreAndMultisampleResolve;
			}
			else
			{
				return mtlpp::StoreAction::MultisampleResolve;
			}
		}
		default: return mtlpp::StoreAction::DontCare;
	}
}

FORCEINLINE mtlpp::StoreAction GetConditionalMetalRTStoreAction(bool bMSAATarget)
{
	if (bMSAATarget)
	{
		//this func should only be getting called when an encoder had to abnormally break.  In this case we 'must' do StoreAndResolve because the encoder will be restarted later
		//with the original MSAA rendertarget and the original data must still be there to continue the render properly.
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMSAAStoreAndResolve));
		return mtlpp::StoreAction::StoreAndMultisampleResolve;
	}
	else
	{
		return mtlpp::StoreAction::Store;
	}	
}

class FMetalRenderPassDescriptorPool
{
public:
	FMetalRenderPassDescriptorPool()
	{
		
	}
	
	~FMetalRenderPassDescriptorPool()
	{
		
	}
	
	mtlpp::RenderPassDescriptor CreateDescriptor()
	{
		MTLRenderPassDescriptor* Desc = Cache.Pop();
		if (!Desc)
		{
			Desc = [MTLRenderPassDescriptor new];
		}
		return mtlpp::RenderPassDescriptor(Desc);
	}
	
	void ReleaseDescriptor(mtlpp::RenderPassDescriptor& Desc)
	{
		mtlpp::Texture EmptyTex;
		
		ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> Attachements = Desc.GetColorAttachments();
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			mtlpp::RenderPassColorAttachmentDescriptor Color = Attachements[i];
			Color.SetTexture(EmptyTex);
			Color.SetResolveTexture(EmptyTex);
			Color.SetStoreAction(mtlpp::StoreAction::Store);
		}
		
		mtlpp::RenderPassDepthAttachmentDescriptor Depth = Desc.GetDepthAttachment();
		Depth.SetTexture(EmptyTex);
		Depth.SetResolveTexture(EmptyTex);
		Depth.SetStoreAction(mtlpp::StoreAction::Store);

		mtlpp::RenderPassStencilAttachmentDescriptor Stencil = Desc.GetStencilAttachment();
		Stencil.SetTexture(EmptyTex);
		Stencil.SetResolveTexture(EmptyTex);
		Stencil.SetStoreAction(mtlpp::StoreAction::Store);

		mtlpp::Buffer Empty;
		Desc.SetVisibilityResultBuffer(Empty);
		
#if PLATFORM_MAC
		Desc.SetRenderTargetArrayLength(1);
#endif
		
		Cache.Push(Desc.GetPtr());
	}
	
	static FMetalRenderPassDescriptorPool& Get()
	{
		static FMetalRenderPassDescriptorPool sSelf;
		return sSelf;
	}
	
private:
	TLockFreePointerListLIFO<MTLRenderPassDescriptor> Cache;
};

void SafeReleaseMetalRenderPassDescriptor(mtlpp::RenderPassDescriptor& Desc)
{
	if (Desc.GetPtr())
	{
		FMetalRenderPassDescriptorPool::Get().ReleaseDescriptor(Desc);
	}
}

FMetalStateCache::FMetalStateCache(bool const bInImmediate)
: DepthStore(mtlpp::StoreAction::Unknown)
, StencilStore(mtlpp::StoreAction::Unknown)
, VisibilityResults(nullptr)
, VisibilityMode(mtlpp::VisibilityResultMode::Disabled)
, VisibilityOffset(0)
, VisibilityWritten(0)
, DepthStencilState(nullptr)
, RasterizerState(nullptr)
, StencilRef(0)
, BlendFactor(FLinearColor::Transparent)
, FrameBufferSize(CGSizeMake(0.0, 0.0))
, RenderTargetArraySize(1)
, RenderPassDesc(nil)
, RasterBits(0)
, PipelineBits(0)
, bIsRenderTargetActive(false)
, bHasValidRenderTarget(false)
, bHasValidColorTarget(false)
, bScissorRectEnabled(false)
, bCanRestartRenderPass(false)
, bImmediate(bInImmediate)
, bFallbackDepthStencilBound(false)
{
	FMemory::Memzero(Viewport);
	FMemory::Memzero(Scissor);
	
	ActiveViewports = 0;
	ActiveScissors = 0;
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStore[i] = mtlpp::StoreAction::Unknown;
	}
	
	FMemory::Memzero(RenderPassInfo);
	FMemory::Memzero(DirtyUniformBuffers);
}

FMetalStateCache::~FMetalStateCache()
{
	RenderPassDesc = nil;
	
	for (uint32 i = 0; i < MaxVertexElementCount; i++)
	{
		VertexBuffers[i].Buffer = nil;
		VertexBuffers[i].Bytes = nil;
		VertexBuffers[i].Length = 0;
		VertexBuffers[i].Offset = 0;
	}
	for (uint32 Frequency = 0; Frequency < EMetalShaderStages::Num; Frequency++)
	{
		ShaderSamplers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxSamplers; i++)
		{
			ShaderSamplers[Frequency].Samplers[i] = nil;
		}
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			BoundUniformBuffers[Frequency][i] = nullptr;
			ShaderBuffers[Frequency].Buffers[i].Buffer = nil;
			ShaderBuffers[Frequency].Buffers[i].Bytes = nil;
			ShaderBuffers[Frequency].Buffers[i].Length = 0;
			ShaderBuffers[Frequency].Buffers[i].ElementRowPitch = 0;
			ShaderBuffers[Frequency].Buffers[i].Offset = 0;
			ShaderBuffers[Frequency].Buffers[i].Usage = mtlpp::ResourceUsage(0);
			ShaderBuffers[Frequency].Formats[i] = PF_Unknown;
		}
		ShaderBuffers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxTextures; i++)
		{
			ShaderTextures[Frequency].Textures[i] = nil;
			ShaderTextures[Frequency].Usage[i] = mtlpp::ResourceUsage(0);
		}
		ShaderTextures[Frequency].Bound = 0;
	}
	
	VisibilityResults = nil;
}

void FMetalStateCache::Reset(void)
{
	SampleCount = 0;
	
	FMemory::Memzero(Viewport);
	FMemory::Memzero(Scissor);
	
	ActiveViewports = 0;
	ActiveScissors = 0;
	
	FMemory::Memzero(RenderPassInfo);
	bIsRenderTargetActive = false;
	bHasValidRenderTarget = false;
	bHasValidColorTarget = false;
	bScissorRectEnabled = false;
	
	FMemory::Memzero(DirtyUniformBuffers);
	FMemory::Memzero(BoundUniformBuffers);
	ActiveUniformBuffers.Empty();
	
	for (uint32 i = 0; i < MaxVertexElementCount; i++)
	{
		VertexBuffers[i].Buffer = nil;
		VertexBuffers[i].Bytes = nil;
		VertexBuffers[i].Length = 0;
		VertexBuffers[i].Offset = 0;
	}
	for (uint32 Frequency = 0; Frequency < EMetalShaderStages::Num; Frequency++)
	{
		ShaderSamplers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxSamplers; i++)
		{
			ShaderSamplers[Frequency].Samplers[i] = nil;
		}
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i].Buffer = nil;
			ShaderBuffers[Frequency].Buffers[i].Bytes = nil;
			ShaderBuffers[Frequency].Buffers[i].Length = 0;
			ShaderBuffers[Frequency].Buffers[i].ElementRowPitch = 0;
			ShaderBuffers[Frequency].Buffers[i].Offset = 0;
			ShaderBuffers[Frequency].Formats[i] = PF_Unknown;
		}
		ShaderBuffers[Frequency].Bound = 0;
		for (uint32 i = 0; i < ML_MaxTextures; i++)
		{
			ShaderTextures[Frequency].Textures[i] = nil;
			ShaderTextures[Frequency].Usage[i] = mtlpp::ResourceUsage(0);
		}
		ShaderTextures[Frequency].Bound = 0;
	}
	
	VisibilityResults = nil;
	VisibilityMode = mtlpp::VisibilityResultMode::Disabled;
	VisibilityOffset = 0;
	VisibilityWritten = 0;
	
	DepthStencilState.SafeRelease();
	RasterizerState.SafeRelease();
	GraphicsPSO.SafeRelease();
	ComputeShader.SafeRelease();
	DepthStencilSurface.SafeRelease();
	StencilRef = 0;
	
	RenderPassDesc = nil;
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStore[i] = mtlpp::StoreAction::Unknown;
	}
	DepthStore = mtlpp::StoreAction::Unknown;
	StencilStore = mtlpp::StoreAction::Unknown;
	
	BlendFactor = FLinearColor::Transparent;
	FrameBufferSize = CGSizeMake(0.0, 0.0);
	RenderTargetArraySize = 0;
    bCanRestartRenderPass = false;
    
    RasterBits = EMetalRenderFlagMask;
    PipelineBits = EMetalPipelineFlagMask;
}

static bool MTLScissorRectEqual(mtlpp::ScissorRect const& Left, mtlpp::ScissorRect const& Right)
{
	return Left.x == Right.x && Left.y == Right.y && Left.width == Right.width && Left.height == Right.height;
}

void FMetalStateCache::SetScissorRect(bool const bEnable, mtlpp::ScissorRect const& Rect)
{
	if (bScissorRectEnabled != bEnable || !MTLScissorRectEqual(Scissor[0], Rect))
	{
		bScissorRectEnabled = bEnable;
		if (bEnable)
		{
			Scissor[0] = Rect;
		}
		else
		{
			Scissor[0].x = Viewport[0].originX;
			Scissor[0].y = Viewport[0].originY;
			Scissor[0].width = Viewport[0].width;
			Scissor[0].height = Viewport[0].height;
		}
		
		// Clamp to framebuffer size - Metal doesn't allow scissor to be larger.
		Scissor[0].x = Scissor[0].x;
		Scissor[0].y = Scissor[0].y;
		Scissor[0].width = FMath::Max((Scissor[0].x + Scissor[0].width <= FMath::RoundToInt32(FrameBufferSize.width)) ? Scissor[0].width : FMath::RoundToInt32(FrameBufferSize.width) - Scissor[0].x, (NSUInteger)1u);
		Scissor[0].height = FMath::Max((Scissor[0].y + Scissor[0].height <= FMath::RoundToInt32(FrameBufferSize.height)) ? Scissor[0].height : FMath::RoundToInt32(FrameBufferSize.height) - Scissor[0].y, (NSUInteger)1u);
		
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	ActiveScissors = 1;
}

void FMetalStateCache::SetBlendFactor(FLinearColor const& InBlendFactor)
{
	if(BlendFactor != InBlendFactor)
	{
		BlendFactor = InBlendFactor;
		RasterBits |= EMetalRenderFlagBlendColor;
	}
}

void FMetalStateCache::SetStencilRef(uint32 const InStencilRef)
{
	if(StencilRef != InStencilRef)
	{
		StencilRef = InStencilRef;
		RasterBits |= EMetalRenderFlagStencilReferenceValue;
	}
}

void FMetalStateCache::SetDepthStencilState(FMetalDepthStencilState* InDepthStencilState)
{
	if(DepthStencilState != InDepthStencilState)
	{
		DepthStencilState = InDepthStencilState;
		RasterBits |= EMetalRenderFlagDepthStencilState;
	}
}

void FMetalStateCache::SetRasterizerState(FMetalRasterizerState* InRasterizerState)
{
	if(RasterizerState != InRasterizerState)
	{
		RasterizerState = InRasterizerState;
		RasterBits |= EMetalRenderFlagFrontFacingWinding|EMetalRenderFlagCullMode|EMetalRenderFlagDepthBias|EMetalRenderFlagTriangleFillMode|EMetalRenderFlagDepthClipMode;
	}
}

void FMetalStateCache::SetComputeShader(FMetalComputeShader* InComputeShader)
{
	if(ComputeShader != InComputeShader)
	{
		ComputeShader = InComputeShader;
		
		PipelineBits |= EMetalPipelineFlagComputeShader;
		
		DirtyUniformBuffers[EMetalShaderStages::Compute] = 0xffffffff;

		for (uint32 Index = 0; Index < ML_MaxTextures; ++Index)
		{
			ShaderTextures[EMetalShaderStages::Compute].Textures[Index] = nil;
			ShaderTextures[EMetalShaderStages::Compute].Usage[Index] = mtlpp::ResourceUsage(0);
		}
		ShaderTextures[EMetalShaderStages::Compute].Bound = 0;

		for (const auto& PackedGlobalArray : InComputeShader->Bindings.PackedGlobalArrays)
		{
			ShaderParameters[EMetalShaderStages::Compute].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
		}
	}
}

bool FMetalStateCache::SetRenderPassInfo(FRHIRenderPassInfo const& InRenderTargets, FMetalQueryBuffer* QueryBuffer, bool const bRestart)
{
	bool bNeedsSet = false;
	
	// see if our new Info matches our previous Info
	if (NeedsToSetRenderTarget(InRenderTargets))
	{
		bool bNeedsClear = false;
		
		//Create local store action states if we support deferred store
		mtlpp::StoreAction NewColorStore[MaxSimultaneousRenderTargets];
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			NewColorStore[i] = mtlpp::StoreAction::Unknown;
		}
		
		mtlpp::StoreAction NewDepthStore = mtlpp::StoreAction::Unknown;
		mtlpp::StoreAction NewStencilStore = mtlpp::StoreAction::Unknown;
		
		// back this up for next frame
		RenderPassInfo = InRenderTargets;
		
		// at this point, we need to fully set up an encoder/command buffer, so make a new one (autoreleased)
		mtlpp::RenderPassDescriptor RenderPass = FMetalRenderPassDescriptorPool::Get().CreateDescriptor();
	
		// if we need to do queries, write to the supplied query buffer
		if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::ES3_1))
		{
			VisibilityResults = QueryBuffer;
			RenderPass.SetVisibilityResultBuffer(QueryBuffer ? QueryBuffer->Buffer : nil);
		}
		else
		{
			VisibilityResults = NULL;
		}
		
		if (QueryBuffer != VisibilityResults)
		{
			VisibilityOffset = 0;
			VisibilityWritten = 0;
		}
	
		// default to non-msaa
	    int32 OldCount = SampleCount;
		SampleCount = 0;
	
		bIsRenderTargetActive = false;
		bHasValidRenderTarget = false;
		bHasValidColorTarget = false;
		
		bFallbackDepthStencilBound = false;
		
		uint8 ArrayTargets = 0;
		uint8 BoundTargets = 0;
		uint32 ArrayRenderLayers = UINT_MAX;
		
		bool bFramebufferSizeSet = false;
		FrameBufferSize = CGSizeMake(0.f, 0.f);
		
		bCanRestartRenderPass = true;
		
		ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> Attachements = RenderPass.GetColorAttachments();
		
		uint32 NumColorRenderTargets = RenderPassInfo.GetNumColorRenderTargets();
		
		for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxSimultaneousRenderTargets; RenderTargetIndex++)
		{
			// default to invalid
			uint8 FormatKey = 0;
			// only try to set it if it was one that was set (ie less than RenderPassInfo.NumColorRenderTargets)
			if (RenderTargetIndex < NumColorRenderTargets && RenderPassInfo.ColorRenderTargets[RenderTargetIndex].RenderTarget != nullptr)
			{
				const FRHIRenderPassInfo::FColorEntry& RenderTargetView = RenderPassInfo.ColorRenderTargets[RenderTargetIndex];
				ColorTargets[RenderTargetIndex] = RenderTargetView.RenderTarget;
				ResolveTargets[RenderTargetIndex] = RenderTargetView.ResolveTarget;
				
				FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetView.RenderTarget);
				FormatKey = Surface.FormatKey;
				
				uint32 Width = FMath::Max((uint32)(Surface.GetDesc().Extent.X >> RenderTargetView.MipIndex), (uint32)1);
				uint32 Height = FMath::Max((uint32)(Surface.GetDesc().Extent.Y >> RenderTargetView.MipIndex), (uint32)1);
				if(!bFramebufferSizeSet)
				{
					bFramebufferSizeSet = true;
					FrameBufferSize.width = Width;
					FrameBufferSize.height = Height;
				}
				else
				{
					FrameBufferSize.width = FMath::Min(FrameBufferSize.width, (CGFloat)Width);
					FrameBufferSize.height = FMath::Min(FrameBufferSize.height, (CGFloat)Height);
				}
	
				// if this is the back buffer, make sure we have a usable drawable
				ConditionalUpdateBackBuffer(Surface);
				FMetalSurface* ResolveSurface = GetMetalSurfaceFromRHITexture(RenderTargetView.ResolveTarget);
				if (ResolveSurface)
				{
					ConditionalUpdateBackBuffer(*ResolveSurface);
				}
					
				BoundTargets |= 1 << RenderTargetIndex;
            
#if !PLATFORM_MAC
                if (Surface.Texture.GetPtr() == nil)
                {
                    SampleCount = OldCount;
                    bCanRestartRenderPass &= (OldCount <= 1);
                    return true;
                }
#endif
				
				// The surface cannot be nil - we have to have a valid render-target array after this call.
				check (Surface.Texture);
	
				// user code generally passes -1 as a default, but we need 0
				uint32 ArraySliceIndex = RenderTargetView.ArraySlice == 0xFFFFFFFF ? 0 : RenderTargetView.ArraySlice;
				if (Surface.GetDesc().IsTextureCube())
				{
					ArraySliceIndex = GetMetalCubeFace((ECubeFace)ArraySliceIndex);
				}
				
				switch(Surface.GetDesc().Dimension)
				{
					case ETextureDimension::Texture2DArray:
					case ETextureDimension::Texture3D:
					case ETextureDimension::TextureCube:
					case ETextureDimension::TextureCubeArray:
						if(RenderTargetView.ArraySlice == 0xFFFFFFFF)
						{
							ArrayTargets |= (1 << RenderTargetIndex);
							ArrayRenderLayers = FMath::Min(ArrayRenderLayers, Surface.GetNumFaces());
						}
						else
						{
							ArrayRenderLayers = 1;
						}
						break;
					default:
						ArrayRenderLayers = 1;
						break;
				}
	
				mtlpp::RenderPassColorAttachmentDescriptor ColorAttachment = Attachements[RenderTargetIndex];
	
				ERenderTargetStoreAction HighLevelStoreAction = GetStoreAction(RenderTargetView.Action);
				ERenderTargetLoadAction HighLevelLoadAction = GetLoadAction(RenderTargetView.Action);
				
				// on iOS with memory-less MSAA textures we can't load them
                // in case high level code wants to load and render to MSAA target, set attachment to a resolved texture
				bool bUseResolvedTexture = false;
#if PLATFORM_IOS
				bUseResolvedTexture = (
					Surface.MSAATexture && 
					Surface.MSAATexture.GetStorageMode() == mtlpp::StorageMode::Memoryless && 
					HighLevelLoadAction == ERenderTargetLoadAction::ELoad);
#endif
				
				bool bMemoryless = false;
				if (Surface.MSAATexture && !bUseResolvedTexture)
				{
#if PLATFORM_IOS
					if (Surface.MSAATexture.GetStorageMode() == mtlpp::StorageMode::Memoryless)
					{
						bMemoryless = true;
						HighLevelLoadAction = ERenderTargetLoadAction::EClear;
					}
#endif
					// set up an MSAA attachment
					ColorAttachment.SetTexture(Surface.MSAATexture);
					NewColorStore[RenderTargetIndex] = GetMetalRTStoreAction(ERenderTargetStoreAction::EMultisampleResolve);
					ColorAttachment.SetStoreAction(!bMemoryless && GRHIDeviceId > 2 ? mtlpp::StoreAction::Unknown : NewColorStore[RenderTargetIndex]);
					ColorAttachment.SetResolveTexture(Surface.MSAAResolveTexture ? Surface.MSAAResolveTexture : Surface.Texture);
					SampleCount = Surface.MSAATexture.GetSampleCount();
				}
				else
				{
#if PLATFORM_IOS
					if (Surface.Texture.GetStorageMode() == mtlpp::StorageMode::Memoryless)
					{
						bMemoryless = true;
						HighLevelStoreAction = ERenderTargetStoreAction::ENoAction;
						HighLevelLoadAction = ERenderTargetLoadAction::EClear;
					}
#endif
					// set up non-MSAA attachment
					ColorAttachment.SetTexture(Surface.Texture);
					NewColorStore[RenderTargetIndex] = GetMetalRTStoreAction(HighLevelStoreAction);
					ColorAttachment.SetStoreAction(!bMemoryless ? mtlpp::StoreAction::Unknown : NewColorStore[RenderTargetIndex]);
                    SampleCount = 1;
				}
				
				ColorAttachment.SetLevel(RenderTargetView.MipIndex);
				if(Surface.GetDesc().IsTexture3D())
				{
					ColorAttachment.SetSlice(0);
					ColorAttachment.SetDepthPlane(ArraySliceIndex);
				}
				else
				{
					ColorAttachment.SetSlice(ArraySliceIndex);
				}
				
				ColorAttachment.SetLoadAction((Surface.Written || !bImmediate || bRestart) ? GetMetalRTLoadAction(HighLevelLoadAction) : mtlpp::LoadAction::Clear);
				FPlatformAtomics::InterlockedExchange(&Surface.Written, 1);
				
				bNeedsClear |= (ColorAttachment.GetLoadAction() == mtlpp::LoadAction::Clear);
				
				const FClearValueBinding& ClearValue = RenderPassInfo.ColorRenderTargets[RenderTargetIndex].RenderTarget->GetClearBinding();
				if (ClearValue.ColorBinding == EClearBinding::EColorBound)
				{
					const FLinearColor& ClearColor = ClearValue.GetClearColor();
					ColorAttachment.SetClearColor(mtlpp::ClearColor(ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A));
				}

				bCanRestartRenderPass &= 	!bMemoryless &&
											ColorAttachment.GetLoadAction() == mtlpp::LoadAction::Load &&
											HighLevelStoreAction != ERenderTargetStoreAction::ENoAction;
	
				bHasValidRenderTarget = true;
				bHasValidColorTarget = true;
			}
			else
			{
				ColorTargets[RenderTargetIndex].SafeRelease();
				ResolveTargets[RenderTargetIndex].SafeRelease();
			}
		}
		
		RenderTargetArraySize = 1;
		
		if(ArrayTargets)
		{
			if (!GetMetalDeviceContext().SupportsFeature(EMetalFeaturesLayeredRendering))
			{
				METAL_FATAL_ASSERT(ArrayRenderLayers != 1, TEXT("Layered rendering is unsupported on this device (%d)."), ArrayRenderLayers);
			}
#if PLATFORM_MAC
			else
			{
				METAL_FATAL_ASSERT(ArrayTargets == BoundTargets, TEXT("All color render targets must be layered when performing multi-layered rendering under Metal (%d != %d)."), ArrayTargets, BoundTargets);
					RenderTargetArraySize = ArrayRenderLayers;
					RenderPass.SetRenderTargetArrayLength(ArrayRenderLayers);
			}
#endif
		}
	
		// default to invalid
		uint8 DepthFormatKey = 0;
		uint8 StencilFormatKey = 0;
		
		// setup depth and/or stencil
		if (RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget != nullptr)
		{
			FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget);
			
			switch(Surface.GetDesc().Dimension)
			{
				case ETextureDimension::Texture2DArray:
				case ETextureDimension::Texture3D:
				case ETextureDimension::TextureCube:
				case ETextureDimension::TextureCubeArray:
					ArrayRenderLayers = Surface.GetNumFaces();
					break;
				default:
					ArrayRenderLayers = 1;
					break;
			}
			if(!ArrayTargets && ArrayRenderLayers > 1)
			{
				METAL_FATAL_ASSERT(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesLayeredRendering), TEXT("Layered rendering is unsupported on this device (%d)."), ArrayRenderLayers);
#if PLATFORM_MAC
					RenderTargetArraySize = ArrayRenderLayers;
					RenderPass.SetRenderTargetArrayLength(ArrayRenderLayers);
#endif
			}
			
			if(!bFramebufferSizeSet)
			{
				bFramebufferSizeSet = true;
				FrameBufferSize.width  = Surface.GetDesc().Extent.X;
				FrameBufferSize.height = Surface.GetDesc().Extent.Y;
			}
			else
			{
				FrameBufferSize.width = FMath::Min(FrameBufferSize.width, (CGFloat)Surface.GetDesc().Extent.X);
				FrameBufferSize.height = FMath::Min(FrameBufferSize.height, (CGFloat)Surface.GetDesc().Extent.Y);
			}
			
			EPixelFormat DepthStencilPixelFormat = RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget->GetFormat();
			
			FMetalTexture DepthTexture = nil;
			FMetalTexture StencilTexture = nil;
			
            const bool bSupportSeparateMSAAResolve = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
			uint32 DepthSampleCount = (Surface.MSAATexture ? Surface.MSAATexture.GetSampleCount() : Surface.Texture.GetSampleCount());
            bool bDepthStencilSampleCountMismatchFixup = false;
            DepthTexture = Surface.MSAATexture ? Surface.MSAATexture : Surface.Texture;
			if (SampleCount == 0)
			{
				SampleCount = DepthSampleCount;
			}
			else if (SampleCount != DepthSampleCount)
            {
				static bool bLogged = false;
				if (!bSupportSeparateMSAAResolve)
				{
					//in the case of NOT support separate MSAA resolve the high level may legitimately cause a mismatch which we need to handle by binding the resolved target which we normally wouldn't do.
					DepthTexture = Surface.Texture;
					bDepthStencilSampleCountMismatchFixup = true;
					DepthSampleCount = 1;
				}
				else if (!bLogged)
				{
					UE_LOG(LogMetal, Error, TEXT("If we support separate targets the high level should always give us matching counts"));
					bLogged = true;
				}
            }

			switch (DepthStencilPixelFormat)
			{
				case PF_X24_G8:
				case PF_DepthStencil:
				case PF_D24:
				{
					mtlpp::PixelFormat DepthStencilFormat = Surface.Texture ? (mtlpp::PixelFormat)Surface.Texture.GetPixelFormat() : mtlpp::PixelFormat::Invalid;
					
					switch(DepthStencilFormat)
					{
						case mtlpp::PixelFormat::Depth32Float:
							StencilTexture =  nil;
							break;
						case mtlpp::PixelFormat::Stencil8:
							StencilTexture = DepthTexture;
							break;
						case mtlpp::PixelFormat::Depth32Float_Stencil8:
							StencilTexture = DepthTexture;
							break;
#if PLATFORM_MAC
						case mtlpp::PixelFormat::Depth24Unorm_Stencil8:
							StencilTexture = DepthTexture;
							break;
#endif
						default:
							break;
					}
					
					break;
				}
				case PF_ShadowDepth:
				{
					break;
				}
				default:
					break;
			}
			
			float DepthClearValue = 0.0f;
			uint32 StencilClearValue = 0;
			const FClearValueBinding& ClearValue = RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget->GetClearBinding();
			if (ClearValue.ColorBinding == EClearBinding::EDepthStencilBound)
			{
				ClearValue.GetDepthStencil(DepthClearValue, StencilClearValue);
			}
			else if(!ArrayTargets && ArrayRenderLayers > 1)
			{
				DepthClearValue = 1.0f;
			}

           bool const bCombinedDepthStencilUsingStencil = (DepthTexture && (mtlpp::PixelFormat)DepthTexture.GetPixelFormat() != mtlpp::PixelFormat::Depth32Float && RenderPassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingStencil());
			bool const bUsingDepth = (RenderPassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingDepth() || (bCombinedDepthStencilUsingStencil));
			if (DepthTexture && bUsingDepth)
			{
				mtlpp::RenderPassDepthAttachmentDescriptor DepthAttachment;
				
				DepthFormatKey = Surface.FormatKey;
				
				ERenderTargetActions DepthActions = GetDepthActions(RenderPassInfo.DepthStencilRenderTarget.Action);
				ERenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
				ERenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);

				// set up the depth attachment
				DepthAttachment.SetTexture(DepthTexture);
				DepthAttachment.SetLoadAction(GetMetalRTLoadAction(DepthLoadAction));
				
				bNeedsClear |= (DepthAttachment.GetLoadAction() == mtlpp::LoadAction::Clear);
				
				ERenderTargetStoreAction HighLevelStoreAction = (Surface.MSAATexture && !bDepthStencilSampleCountMismatchFixup) ? ERenderTargetStoreAction::EMultisampleResolve : DepthStoreAction;
				if (bUsingDepth && (HighLevelStoreAction == ERenderTargetStoreAction::ENoAction || bDepthStencilSampleCountMismatchFixup))
				{
					if (DepthSampleCount > 1)
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EMultisampleResolve;
					}
					else
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EStore;
					}
				}
				
				const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
				bool bDepthTextureMemoryless = false;
#if PLATFORM_IOS
				bDepthTextureMemoryless = DepthTexture.GetStorageMode() == mtlpp::StorageMode::Memoryless;
				if (bDepthTextureMemoryless)
				{
					DepthAttachment.SetLoadAction(mtlpp::LoadAction::Clear);
					
					if (bSupportsMSAADepthResolve && Surface.MSAATexture && DepthStoreAction == ERenderTargetStoreAction::EMultisampleResolve)
					{
						HighLevelStoreAction = ERenderTargetStoreAction::EMultisampleResolve;
					}
					else
					{
						HighLevelStoreAction = ERenderTargetStoreAction::ENoAction;
					}
				}
                else
                {
                	HighLevelStoreAction = DepthStoreAction;
                }
#endif
                //needed to quiet the metal validation that runs when you end renderpass. (it requires some kind of 'resolve' for an msaa target)
				//But with deferredstore we don't set the real one until submit time.
				NewDepthStore = !Surface.MSAATexture || bSupportsMSAADepthResolve ? GetMetalRTStoreAction(HighLevelStoreAction) : mtlpp::StoreAction::DontCare;
				DepthAttachment.SetStoreAction(!bDepthTextureMemoryless && Surface.MSAATexture && GRHIDeviceId > 2 ? mtlpp::StoreAction::Unknown : NewDepthStore);
				DepthAttachment.SetClearDepth(DepthClearValue);
				check(SampleCount > 0);

				if (Surface.MSAATexture && bSupportsMSAADepthResolve && DepthAttachment.GetStoreAction() != mtlpp::StoreAction::DontCare)
				{
                    if (!bDepthStencilSampleCountMismatchFixup)
                    {
                        DepthAttachment.SetResolveTexture(Surface.MSAAResolveTexture ? Surface.MSAAResolveTexture : Surface.Texture);
                    }
#if PLATFORM_MAC
					//would like to assert and do manual custom resolve, but that is causing some kind of weird corruption.
					//checkf(false, TEXT("Depth resolves need to do 'max' for correctness.  MacOS does not expose this yet unless the spec changed."));
#else
					DepthAttachment.SetDepthResolveFilter(mtlpp::MultisampleDepthResolveFilter::Max);
#endif
				}
				
				bHasValidRenderTarget = true;
				bFallbackDepthStencilBound = (RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget == FallbackDepthStencilSurface);

				bool bDepthMSAARestart = !bDepthTextureMemoryless && HighLevelStoreAction == ERenderTargetStoreAction::EMultisampleResolve;
				bCanRestartRenderPass &=	(DepthSampleCount <= 1 || bDepthMSAARestart) &&
											(
												(RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget == FallbackDepthStencilSurface) ||
												((DepthAttachment.GetLoadAction() == mtlpp::LoadAction::Load) && (bDepthMSAARestart || !RenderPassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite() || DepthStoreAction == ERenderTargetStoreAction::EStore))
											);
				
				// and assign it
				RenderPass.SetDepthAttachment(DepthAttachment);
			}
	
            //if we're dealing with a samplecount mismatch we just bail on stencil entirely as stencil
            //doesn't have an autoresolve target to use.
			
			bool const bCombinedDepthStencilUsingDepth = (StencilTexture && StencilTexture.GetPixelFormat() != mtlpp::PixelFormat::Stencil8 && RenderPassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingDepth());
			bool const bUsingStencil = RenderPassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsUsingStencil() || (bCombinedDepthStencilUsingDepth);
			if (StencilTexture && bUsingStencil)
			{
				mtlpp::RenderPassStencilAttachmentDescriptor StencilAttachment;
				
				StencilFormatKey = Surface.FormatKey;
				
				ERenderTargetActions StencilActions = GetStencilActions(RenderPassInfo.DepthStencilRenderTarget.Action);
				ERenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
				ERenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);
	
				// set up the stencil attachment
				StencilAttachment.SetTexture(StencilTexture);
				StencilAttachment.SetLoadAction(GetMetalRTLoadAction(StencilLoadAction));
				
				bNeedsClear |= (StencilAttachment.GetLoadAction() == mtlpp::LoadAction::Clear);
				
				ERenderTargetStoreAction HighLevelStoreAction = StencilStoreAction;
				if (bUsingStencil && (HighLevelStoreAction == ERenderTargetStoreAction::ENoAction || bDepthStencilSampleCountMismatchFixup))
				{
					HighLevelStoreAction = ERenderTargetStoreAction::EStore;
				}
				
				bool bStencilMemoryless = false;
#if PLATFORM_IOS
				if (StencilTexture.GetStorageMode() == mtlpp::StorageMode::Memoryless)
				{
					bStencilMemoryless = true;
					HighLevelStoreAction = ERenderTargetStoreAction::ENoAction;
					StencilAttachment.SetLoadAction(mtlpp::LoadAction::Clear);
				}
				else
				{
					HighLevelStoreAction = StencilStoreAction;
				}
#endif
				
				// For the case where Depth+Stencil is MSAA we can't Resolve depth and Store stencil - we can only Resolve + DontCare or StoreResolve + Store (on newer H/W and iOS).
				// We only allow use of StoreResolve in the Desktop renderers as the mobile renderer does not and should not assume hardware support for it.
				NewStencilStore = (StencilTexture.GetSampleCount() == 1  || GetMetalRTStoreAction(ERenderTargetStoreAction::EMultisampleResolve) == mtlpp::StoreAction::StoreAndMultisampleResolve) ? GetMetalRTStoreAction(HighLevelStoreAction) : mtlpp::StoreAction::DontCare;
				StencilAttachment.SetStoreAction(!bStencilMemoryless && StencilTexture.GetSampleCount() > 1 && GRHIDeviceId > 2 ? mtlpp::StoreAction::Unknown : NewStencilStore);
				StencilAttachment.SetClearStencil(StencilClearValue);

				if (SampleCount == 0)
				{
					SampleCount = StencilAttachment.GetTexture().GetSampleCount();
				}
				
				bHasValidRenderTarget = true;
				
				// @todo Stencil writes that need to persist must use ERenderTargetStoreAction::EStore on iOS.
				// We should probably be using deferred store actions so that we can safely lazily instantiate encoders.
				bool bStencilMSAARestart = !bStencilMemoryless && HighLevelStoreAction != ERenderTargetStoreAction::ENoAction;
				bCanRestartRenderPass &= 	(bStencilMSAARestart || SampleCount <= 1) &&
											(
												(RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget == FallbackDepthStencilSurface) ||
												((StencilAttachment.GetLoadAction() == mtlpp::LoadAction::Load) && (bStencilMSAARestart || !RenderPassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite() || (StencilStoreAction == ERenderTargetStoreAction::EStore)))
											);
				
				// and assign it
				RenderPass.SetStencilAttachment(StencilAttachment);
			}
		}
		
		//Update deferred store states if required otherwise they're already set directly on the Metal Attachement Descriptors
		{
			for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
			{
				ColorStore[i] = NewColorStore[i];
			}
			DepthStore = NewDepthStore;
			StencilStore = NewStencilStore;
		}
		
		if (SampleCount == 0)
		{
			SampleCount = 1;
		}
		
		bIsRenderTargetActive = bHasValidRenderTarget;
		
		// Only start encoding if the render target state is valid
		if (bHasValidRenderTarget)
		{
			// Retain and/or release the depth-stencil surface in case it is a temporary surface for a draw call that writes to depth without a depth/stencil buffer bound.
			DepthStencilSurface = RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget;
			DepthStencilResolve = RenderPassInfo.DepthStencilRenderTarget.ResolveTarget;
		}
		else
		{
			DepthStencilSurface.SafeRelease();
			DepthStencilResolve.SafeRelease();
		}
		
		RenderPassDesc = RenderPass;
		
		bNeedsSet = true;
	}

	return bNeedsSet;
}

void FMetalStateCache::InvalidateRenderTargets(void)
{
	bHasValidRenderTarget = false;
	bHasValidColorTarget = false;
	bIsRenderTargetActive = false;
}

void FMetalStateCache::SetRenderTargetsActive(bool const bActive)
{
	bIsRenderTargetActive = bActive;
}

static bool MTLViewportEqual(mtlpp::Viewport const& Left, mtlpp::Viewport const& Right)
{
	return FMath::IsNearlyEqual(Left.originX, Right.originX) &&
			FMath::IsNearlyEqual(Left.originY, Right.originY) &&
			FMath::IsNearlyEqual(Left.width, Right.width) &&
			FMath::IsNearlyEqual(Left.height, Right.height) &&
			FMath::IsNearlyEqual(Left.znear, Right.znear) &&
			FMath::IsNearlyEqual(Left.zfar, Right.zfar);
}

void FMetalStateCache::SetViewport(const mtlpp::Viewport& InViewport)
{
	if (!MTLViewportEqual(Viewport[0], InViewport))
	{
		Viewport[0] = InViewport;
	
		RasterBits |= EMetalRenderFlagViewport;
	}
	
	ActiveViewports = 1;
	
	if (!bScissorRectEnabled)
	{
		mtlpp::ScissorRect Rect;
		Rect.x = InViewport.originX;
		Rect.y = InViewport.originY;
		Rect.width = InViewport.width;
		Rect.height = InViewport.height;
		SetScissorRect(false, Rect);
	}
}

void FMetalStateCache::SetViewport(uint32 Index, const mtlpp::Viewport& InViewport)
{
	check(Index < ML_MaxViewports);
	
	if (!MTLViewportEqual(Viewport[Index], InViewport))
	{
		Viewport[Index] = InViewport;
		
		RasterBits |= EMetalRenderFlagViewport;
	}
	
	// There may not be gaps in the viewport array.
	ActiveViewports = Index + 1;
	
	// This always sets the scissor rect because the RHI doesn't bother to expose proper scissor states for multiple viewports.
	// This will have to change if we want to guarantee correctness in the mid to long term.
	{
		mtlpp::ScissorRect Rect;
		Rect.x = InViewport.originX;
		Rect.y = InViewport.originY;
		Rect.width = InViewport.width;
		Rect.height = InViewport.height;
		SetScissorRect(Index, false, Rect);
	}
}

void FMetalStateCache::SetScissorRect(uint32 Index, bool const bEnable, mtlpp::ScissorRect const& Rect)
{
	check(Index < ML_MaxViewports);
	if (!MTLScissorRectEqual(Scissor[Index], Rect))
	{
		// There's no way we can setup the bounds correctly - that must be done by the caller or incorrect rendering & crashes will ensue.
		Scissor[Index] = Rect;
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	ActiveScissors = Index + 1;
}

void FMetalStateCache::SetViewports(const mtlpp::Viewport InViewport[], uint32 Count)
{
	check(Count >= 1 && Count < ML_MaxViewports);
	
	// Check if the count has changed first & if so mark for a rebind
	if (ActiveViewports != Count)
	{
		RasterBits |= EMetalRenderFlagViewport;
		RasterBits |= EMetalRenderFlagScissorRect;
	}
	
	for (uint32 i = 0; i < Count; i++)
	{
		SetViewport(i, InViewport[i]);
	}
	
	ActiveViewports = Count;
}

void FMetalStateCache::SetVertexStream(uint32 const Index, FMetalBuffer* Buffer, FMetalBufferData* Bytes, uint32 const Offset, uint32 const Length)
{
	check(Index < MaxVertexElementCount);
	check(UNREAL_TO_METAL_BUFFER_INDEX(Index) < MaxMetalStreams);

	if (Buffer)
	{
		VertexBuffers[Index].Buffer = *Buffer;
	}
	else
	{
		VertexBuffers[Index].Buffer = nil;
	}
	VertexBuffers[Index].Offset = 0;
	VertexBuffers[Index].Bytes = Bytes;
	VertexBuffers[Index].Length = Length;
	
	SetShaderBuffer(EMetalShaderStages::Vertex, VertexBuffers[Index].Buffer, Bytes, Offset, Length, UNREAL_TO_METAL_BUFFER_INDEX(Index), mtlpp::ResourceUsage::Read);
}

uint32 FMetalStateCache::GetVertexBufferSize(uint32 const Index)
{
	check(Index < MaxVertexElementCount);
	check(UNREAL_TO_METAL_BUFFER_INDEX(Index) < MaxMetalStreams);
	return VertexBuffers[Index].Length;
}

void FMetalStateCache::SetGraphicsPipelineState(FMetalGraphicsPipelineState* State)
{
	if (GraphicsPSO != State)
	{
		GraphicsPSO = State;
				
		DirtyUniformBuffers[EMetalShaderStages::Vertex] = 0xffffffff;
		DirtyUniformBuffers[EMetalShaderStages::Pixel] = 0xffffffff;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		DirtyUniformBuffers[EMetalShaderStages::Geometry] = 0xffffffff;
#endif
		
		PipelineBits |= EMetalPipelineFlagPipelineState;
		
        if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelResetOnBind)
        {
            for (uint32 i = 0; i < EMetalShaderStages::Num; i++)
            {
                ShaderBuffers[i].Bound = UINT32_MAX;
                ShaderTextures[i].Bound = FMETALTEXTUREMASK_MAX;
                ShaderSamplers[i].Bound = UINT16_MAX;
            }
        }
		
		SetDepthStencilState(State->DepthStencilState);
		SetRasterizerState(State->RasterizerState);

		for (const auto& PackedGlobalArray : State->VertexShader->Bindings.PackedGlobalArrays)
		{
			ShaderParameters[EMetalShaderStages::Vertex].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
		}

		if (State->PixelShader)
		{
			for (const auto& PackedGlobalArray : State->PixelShader->Bindings.PackedGlobalArrays)
			{
				ShaderParameters[EMetalShaderStages::Pixel].PrepareGlobalUniforms(CrossCompiler::PackedTypeNameToTypeIndex(PackedGlobalArray.TypeName), PackedGlobalArray.Size);
			}
		}
	}
}

FMetalShaderPipeline* FMetalStateCache::GetPipelineState() const
{
	return GraphicsPSO->GetPipeline();
}

EPrimitiveType FMetalStateCache::GetPrimitiveType()
{
	check(IsValidRef(GraphicsPSO));
	return GraphicsPSO->GetPrimitiveType();
}

void FMetalStateCache::BindUniformBuffer(EMetalShaderStages const Freq, uint32 const BufferIndex, FRHIUniformBuffer* BufferRHI)
{
	check(BufferIndex < ML_MaxBuffers);
	if (BoundUniformBuffers[Freq][BufferIndex] != BufferRHI)
	{
		ActiveUniformBuffers.Add(BufferRHI);
		BoundUniformBuffers[Freq][BufferIndex] = BufferRHI;
		DirtyUniformBuffers[Freq] |= 1 << BufferIndex;
	}
}

void FMetalStateCache::SetDirtyUniformBuffers(EMetalShaderStages const Freq, uint32 const Dirty)
{
	DirtyUniformBuffers[Freq] = Dirty;
}

void FMetalStateCache::SetVisibilityResultMode(mtlpp::VisibilityResultMode const Mode, NSUInteger const Offset)
{
	if (VisibilityMode != Mode || VisibilityOffset != Offset)
	{
		VisibilityMode = Mode;
		VisibilityOffset = Offset;
		
		RasterBits |= EMetalRenderFlagVisibilityResultMode;
	}
}

void FMetalStateCache::ConditionalUpdateBackBuffer(FMetalSurface& Surface)
{
	// are we setting the back buffer? if so, make sure we have the drawable
	if (EnumHasAnyFlags(Surface.GetDesc().Flags, TexCreate_Presentable))
	{
		// update the back buffer texture the first time used this frame
		if (Surface.Texture.GetPtr() == nil)
		{
			// set the texture into the backbuffer
			Surface.GetDrawableTexture();
		}
#if PLATFORM_MAC
		check (Surface.Texture);
#endif
	}
}

bool FMetalStateCache::NeedsToSetRenderTarget(const FRHIRenderPassInfo& InRenderPassInfo)
{
	// see if our new Info matches our previous Info
	uint32 CurrentNumColorRenderTargets = RenderPassInfo.GetNumColorRenderTargets();
	uint32 NewNumColorRenderTargets = InRenderPassInfo.GetNumColorRenderTargets();
	
	// basic checks
	bool bAllChecksPassed = GetHasValidRenderTarget() && bIsRenderTargetActive && CurrentNumColorRenderTargets == NewNumColorRenderTargets &&
		(InRenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget == RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget);

	// now check each color target if the basic tests passe
	if (bAllChecksPassed)
	{
		for (int32 RenderTargetIndex = 0; RenderTargetIndex < NewNumColorRenderTargets; RenderTargetIndex++)
		{
			const FRHIRenderPassInfo::FColorEntry& RenderTargetView = InRenderPassInfo.ColorRenderTargets[RenderTargetIndex];
			const FRHIRenderPassInfo::FColorEntry& PreviousRenderTargetView = RenderPassInfo.ColorRenderTargets[RenderTargetIndex];

			// handle simple case of switching textures or mip/slice
			if (RenderTargetView.RenderTarget != PreviousRenderTargetView.RenderTarget ||
				RenderTargetView.ResolveTarget != PreviousRenderTargetView.ResolveTarget ||
				RenderTargetView.MipIndex != PreviousRenderTargetView.MipIndex ||
				RenderTargetView.ArraySlice != PreviousRenderTargetView.ArraySlice)
			{
				bAllChecksPassed = false;
				break;
			}
			
			// it's non-trivial when we need to switch based on load/store action:
			// LoadAction - it only matters what we are switching to in the new one
			//    If we switch to Load, no need to switch as we can re-use what we already have
			//    If we switch to Clear, we have to always switch to a new RT to force the clear
			//    If we switch to DontCare, there's definitely no need to switch
			//    If we switch *from* Clear then we must change target as we *don't* want to clear again.
            if (GetLoadAction(RenderTargetView.Action) == ERenderTargetLoadAction::EClear)
            {
                bAllChecksPassed = false;
                break;
            }
            // StoreAction - this matters what the previous one was **In Spirit**
            //    If we come from Store, we need to switch to a new RT to force the store
            //    If we come from DontCare, then there's no need to switch
            //    @todo metal: However, we basically only use Store now, and don't
            //        care about intermediate results, only final, so we don't currently check the value
            //			if (PreviousRenderTargetView.StoreAction == ERenderTTargetStoreAction::EStore)
            //			{
            //				bAllChecksPassed = false;
            //				break;
            //			}
        }
        
        if (InRenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget && (GetLoadAction(GetDepthActions(InRenderPassInfo.DepthStencilRenderTarget.Action)) == ERenderTargetLoadAction::EClear || GetLoadAction(GetStencilActions(InRenderPassInfo.DepthStencilRenderTarget.Action)) == ERenderTargetLoadAction::EClear))
        {
            bAllChecksPassed = false;
		}
		
		if (InRenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget && (GetStoreAction(GetDepthActions(InRenderPassInfo.DepthStencilRenderTarget.Action)) > GetStoreAction(GetDepthActions(RenderPassInfo.DepthStencilRenderTarget.Action)) || GetStoreAction(GetStencilActions(InRenderPassInfo.DepthStencilRenderTarget.Action)) > GetStoreAction(GetStencilActions(RenderPassInfo.DepthStencilRenderTarget.Action))))
		{
			// Don't break the encoder if we can just change the store actions.
			mtlpp::StoreAction NewDepthStore = DepthStore;
			mtlpp::StoreAction NewStencilStore = StencilStore;
			if (GetStoreAction(GetDepthActions(InRenderPassInfo.DepthStencilRenderTarget.Action)) > GetStoreAction(GetDepthActions(RenderPassInfo.DepthStencilRenderTarget.Action)))
			{
				if (RenderPassDesc.GetDepthAttachment().GetTexture())
				{
					FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget);
					
					const uint32 DepthSampleCount = (Surface.MSAATexture ? Surface.MSAATexture.GetSampleCount() : Surface.Texture.GetSampleCount());
					bool const bDepthStencilSampleCountMismatchFixup = (SampleCount != DepthSampleCount);

					ERenderTargetStoreAction HighLevelStoreAction = (Surface.MSAATexture && !bDepthStencilSampleCountMismatchFixup) ? ERenderTargetStoreAction::EMultisampleResolve : GetStoreAction(GetDepthActions(RenderPassInfo.DepthStencilRenderTarget.Action));
					
#if PLATFORM_IOS
					FMetalTexture& Tex = Surface.MSAATexture ? Surface.MSAATexture : Surface.Texture;
					if (Tex.GetStorageMode() == mtlpp::StorageMode::Memoryless)
					{
						HighLevelStoreAction = ERenderTargetStoreAction::ENoAction;
					}
#endif
					
					NewDepthStore = GetMetalRTStoreAction(HighLevelStoreAction);
				}
				else
				{
					bAllChecksPassed = false;
				}
			}
			
			if (GetStoreAction(GetStencilActions(InRenderPassInfo.DepthStencilRenderTarget.Action)) > GetStoreAction(GetStencilActions(RenderPassInfo.DepthStencilRenderTarget.Action)))
			{
				if (RenderPassDesc.GetStencilAttachment().GetTexture())
				{
					NewStencilStore = GetMetalRTStoreAction(GetStoreAction(GetStencilActions(RenderPassInfo.DepthStencilRenderTarget.Action)));
#if PLATFORM_IOS
					if (RenderPassDesc.GetStencilAttachment().GetTexture().GetStorageMode() == mtlpp::StorageMode::Memoryless)
					{
						NewStencilStore = GetMetalRTStoreAction(ERenderTargetStoreAction::ENoAction);
					}
#endif
				}
				else
				{
					bAllChecksPassed = false;
				}
			}
			
			if (bAllChecksPassed)
			{
				DepthStore = NewDepthStore;
				StencilStore = NewStencilStore;
			}
		}
	}

	// if we are setting them to nothing, then this is probably end of frame, and we can't make a framebuffer
	// with nothng, so just abort this (only need to check on single MRT case)
	if (NewNumColorRenderTargets == 1 && InRenderPassInfo.ColorRenderTargets[0].RenderTarget == nullptr &&
		InRenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget == nullptr)
	{
		bAllChecksPassed = true;
	}

	return bAllChecksPassed == false;
}

void FMetalStateCache::SetShaderBuffer(EMetalShaderStages const Frequency, FMetalBuffer const& Buffer, FMetalBufferData* const Bytes, NSUInteger const Offset, NSUInteger const Length, NSUInteger const Index, mtlpp::ResourceUsage const Usage, EPixelFormat const Format, NSUInteger const ElementRowPitch)
{
	check(Frequency < EMetalShaderStages::Num);
	check(Index < ML_MaxBuffers);
	
	if (ShaderBuffers[Frequency].Buffers[Index].Buffer != Buffer ||
		ShaderBuffers[Frequency].Buffers[Index].Bytes != Bytes ||
		ShaderBuffers[Frequency].Buffers[Index].Offset != Offset ||
		ShaderBuffers[Frequency].Buffers[Index].Length != Length ||
		ShaderBuffers[Frequency].Buffers[Index].ElementRowPitch != ElementRowPitch ||
		!(ShaderBuffers[Frequency].Buffers[Index].Usage & Usage) ||
		ShaderBuffers[Frequency].Formats[Index] != Format)
	{
		ShaderBuffers[Frequency].Buffers[Index].Buffer = Buffer;
		ShaderBuffers[Frequency].Buffers[Index].Bytes = Bytes;
		ShaderBuffers[Frequency].Buffers[Index].Offset = Offset;
		ShaderBuffers[Frequency].Buffers[Index].Length = Length;
		ShaderBuffers[Frequency].Buffers[Index].ElementRowPitch = ElementRowPitch;
		ShaderBuffers[Frequency].Buffers[Index].Usage = Usage;
		
		ShaderBuffers[Frequency].Formats[Index] = Format;
		
		if (Buffer || Bytes)
		{
			ShaderBuffers[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderBuffers[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetShaderTexture(EMetalShaderStages const Frequency, FMetalTexture const& Texture, NSUInteger const Index, mtlpp::ResourceUsage const Usage)
{
	check(Frequency < EMetalShaderStages::Num);
	check(Index < ML_MaxTextures);

#if (PLATFORM_IOS || PLATFORM_TVOS)
    UE_CLOG([Texture.GetPtr() storageMode] == MTLStorageModeMemoryless, LogMetal, Fatal, TEXT("FATAL: Attempting to bind a memoryless texture. Stage %u Index %u Texture %@"), Frequency, Index, Texture.GetPtr());
#endif
	
	if (ShaderTextures[Frequency].Textures[Index] != Texture
		|| ShaderTextures[Frequency].Usage[Index] != Usage)
	{
		ShaderTextures[Frequency].Textures[Index] = Texture;
		ShaderTextures[Frequency].Usage[Index] = Usage;
		
		if (Texture)
		{
			ShaderTextures[Frequency].Bound |= (FMetalTextureMask(1) << FMetalTextureMask(Index));
		}
		else
		{
			ShaderTextures[Frequency].Bound &= ~(FMetalTextureMask(1) << FMetalTextureMask(Index));
		}
	}
}

void FMetalStateCache::SetShaderSamplerState(EMetalShaderStages const Frequency, FMetalSamplerState* const Sampler, NSUInteger const Index)
{
	check(Frequency < EMetalShaderStages::Num);
	check(Index < ML_MaxSamplers);
	
	if (ShaderSamplers[Frequency].Samplers[Index].GetPtr() != (Sampler ? Sampler->State.GetPtr() : nil))
	{
		if (Sampler)
		{
#if !PLATFORM_MAC
			ShaderSamplers[Frequency].Samplers[Index] = ((Frequency == EMetalShaderStages::Vertex || Frequency == EMetalShaderStages::Compute) && Sampler->NoAnisoState) ? Sampler->NoAnisoState : Sampler->State;
#else
			ShaderSamplers[Frequency].Samplers[Index] = Sampler->State;
#endif
			ShaderSamplers[Frequency].Bound |= (1 << Index);
		}
		else
		{
			ShaderSamplers[Frequency].Samplers[Index] = nil;
			ShaderSamplers[Frequency].Bound &= ~(1 << Index);
		}
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI, float CurrentTime)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	ns::AutoReleased<FMetalTexture> Texture;
	mtlpp::ResourceUsage Usage = (mtlpp::ResourceUsage)0;
	if (Surface != nullptr)
	{
		TextureRHI->SetLastRenderTime(CurrentTime);
		Texture = Surface->Texture;
		Usage = mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read|mtlpp::ResourceUsage::Sample);
	}
	
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderTexture(EMetalShaderStages::Pixel, Texture, BindIndex, Usage);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderTexture(EMetalShaderStages::Vertex, Texture, BindIndex, Usage);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderTexture(EMetalShaderStages::Compute, Texture, BindIndex, Usage);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetShaderResourceView(FMetalContext* Context, EMetalShaderStages ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV)
{
	if (SRV)
	{
		if (SRV->bTexture)
		{
			FMetalTexture const& View = SRV->GetTextureView();
			if (View)
			{
				SetShaderTexture(ShaderStage, View, BindIndex, mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read | mtlpp::ResourceUsage::Sample));
			}
			else
			{
				SetShaderTexture(ShaderStage, nil, BindIndex, mtlpp::ResourceUsage(0));
			}
		}
		else
		{
			if (IsLinearBuffer(ShaderStage, BindIndex) && SRV->GetLinearTexture())
			{
				ns::AutoReleased<FMetalTexture> Tex;
				Tex = SRV->GetLinearTexture();

				SetShaderTexture(ShaderStage, Tex, BindIndex, mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read | mtlpp::ResourceUsage::Sample));
			}
			else
			{
				FMetalResourceMultiBuffer* Buffer = SRV->GetSourceBuffer();
				if(Buffer != nullptr)
				{
					SetShaderBuffer(ShaderStage, Buffer->GetCurrentBufferOrNil(), Buffer->Data, SRV->Offset, Buffer->GetSize(), BindIndex, mtlpp::ResourceUsage::Read, (EPixelFormat)SRV->Format);
				}
				else
				{
					SetShaderBuffer(ShaderStage, nil, nullptr, 0, 0, BindIndex, mtlpp::ResourceUsage(0));
				}
			}
		}
	}
}

bool FMetalStateCache::IsLinearBuffer(EMetalShaderStages ShaderStage, uint32 BindIndex)
{
    switch (ShaderStage)
    {
        case EMetalShaderStages::Vertex:
        {
            return (GraphicsPSO->VertexShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
            break;
        }
        case EMetalShaderStages::Pixel:
        {
            return (GraphicsPSO->PixelShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
            break;
        }
        case EMetalShaderStages::Compute:
        {
            return (ComputeShader->Bindings.LinearBuffer & (1 << BindIndex)) != 0;
        }
        default:
        {
            check(false);
            return false;
        }
    }
}

void FMetalStateCache::SetShaderUnorderedAccessView(EMetalShaderStages ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV)
{
	if (UAV)
	{
		if (UAV->bTexture)
		{
			FMetalSurface* Surface = UAV->GetSourceTexture();
			FMetalTexture const& View = UAV->GetTextureView();

			if (View)
			{
				FPlatformAtomics::InterlockedExchange(&Surface->Written, 1);

				SetShaderTexture(ShaderStage, View, BindIndex, mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read | mtlpp::ResourceUsage::Write));

				if (Surface->Texture.GetBuffer() && (EnumHasAllFlags(Surface->GetDesc().Flags, TexCreate_UAV | TexCreate_NoTiling) || EnumHasAllFlags(Surface->GetDesc().Flags, TexCreate_AtomicCompatible)))
				{
					uint32 BytesPerRow = Surface->Texture.GetBufferBytesPerRow();
					uint32 ElementsPerRow = BytesPerRow / GPixelFormats[(EPixelFormat)Surface->GetFormat()].BlockBytes;

					FMetalBuffer Buffer(Surface->Texture.GetBuffer(), false);
					const uint32 BufferOffset = Surface->Texture.GetBufferOffset();
					const uint32 BufferSize = Surface->Texture.GetBuffer().GetLength();
					SetShaderBuffer(ShaderStage, Buffer, nullptr, BufferOffset, BufferSize, BindIndex, mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read | mtlpp::ResourceUsage::Write), static_cast<EPixelFormat>(UAV->Format), ElementsPerRow);
				}
			}
			else
			{
				SetShaderTexture(ShaderStage, nil, BindIndex, mtlpp::ResourceUsage(0));
			}
		}
		else
		{
			FMetalResourceMultiBuffer* Buffer = UAV->GetSourceBuffer();
			check(!Buffer->Data && Buffer->GetCurrentBufferOrNil());

			if (IsLinearBuffer(ShaderStage, BindIndex) && UAV->GetLinearTexture())
			{
				ns::AutoReleased<FMetalTexture> Tex;
				Tex = UAV->GetLinearTexture();
				SetShaderTexture(ShaderStage, Tex, BindIndex, mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read | mtlpp::ResourceUsage::Write));
			}

			SetShaderBuffer(ShaderStage, Buffer->GetCurrentBufferOrNil(), Buffer->Data, 0, Buffer->GetSize(), BindIndex, mtlpp::ResourceUsage(mtlpp::ResourceUsage::Read | mtlpp::ResourceUsage::Write), (EPixelFormat)UAV->Format);
		}
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderResourceView(nullptr, EMetalShaderStages::Pixel, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderResourceView(nullptr, EMetalShaderStages::Vertex, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderResourceView(nullptr, EMetalShaderStages::Compute, BindIndex, SRV);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState, float CurrentTime)
{
	check(SamplerState->State);
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderSamplerState(EMetalShaderStages::Pixel, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderSamplerState(EMetalShaderStages::Vertex, SamplerState, BindIndex);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderSamplerState(EMetalShaderStages::Compute, SamplerState, BindIndex);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalStateCache::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderUnorderedAccessView(EMetalShaderStages::Pixel, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderUnorderedAccessView(EMetalShaderStages::Vertex, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderUnorderedAccessView(EMetalShaderStages::Compute, BindIndex, UAV);
			break;
			
		default:
			check(0);
			break;
	}
}


template <typename MetalResourceType>
inline int32 FMetalStateCache::SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex, float CurrentTime)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);
			
			MetalResourceType* ResourcePtr = (MetalResourceType*)Resources[ResourceIndex].GetReference();
			
			// todo: could coalesce adjacent bound resources.
			SetResource(ShaderStage, BindIndex, ResourcePtr, CurrentTime);
			
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <class ShaderType>
void FMetalStateCache::SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage)
{
	checkSlow(Shader);
	
	EMetalShaderStages Frequency;
	switch(ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_VERTEX:
			Frequency = EMetalShaderStages::Vertex;
			break;
		case CrossCompiler::SHADER_STAGE_PIXEL:
			Frequency = EMetalShaderStages::Pixel;
			break;
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			Frequency = EMetalShaderStages::Compute;
			break;
		default:
			Frequency = EMetalShaderStages::Num; //Silence a compiler warning/error
			check(false);
			break;
	}

	float CurrentTime = FPlatformTime::Seconds();

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->Bindings.ShaderResourceTable.ResourceTableBits & GetDirtyUniformBuffers(Frequency);
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FMetalUniformBuffer* Buffer = (FMetalUniformBuffer*)GetBoundUniformBuffers(Frequency)[BufferIndex];
		if (Buffer && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs))
		{
			check(BufferIndex < Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes.Num());
			check(Buffer->GetLayout().GetHash() == Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);
			
			// todo: could make this two pass: gather then set
			SetShaderResourcesFromBuffer<FRHITexture>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.TextureMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalShaderResourceView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalSamplerState>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.SamplerMap.GetData(), BufferIndex, CurrentTime);
			SetShaderResourcesFromBuffer<FMetalUnorderedAccessView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.UnorderedAccessViewMap.GetData(), BufferIndex, CurrentTime);
		}
	}
	SetDirtyUniformBuffers(Frequency, 0);
}

void FMetalStateCache::CommitRenderResources(FMetalCommandEncoder* Raster)
{
	check(IsValidRef(GraphicsPSO));
    
    SetResourcesFromTables(GraphicsPSO->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX);
    GetShaderParameters(EMetalShaderStages::Vertex).CommitPackedGlobals(this, Raster, EMetalShaderStages::Vertex, GraphicsPSO->VertexShader->Bindings);
	
    if (IsValidRef(GraphicsPSO->PixelShader))
    {
    	SetResourcesFromTables(GraphicsPSO->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL);
        GetShaderParameters(EMetalShaderStages::Pixel).CommitPackedGlobals(this, Raster, EMetalShaderStages::Pixel, GraphicsPSO->PixelShader->Bindings);
    }
}

void FMetalStateCache::CommitComputeResources(FMetalCommandEncoder* Compute)
{
	check(IsValidRef(ComputeShader));
	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);
	
	GetShaderParameters(EMetalShaderStages::Compute).CommitPackedGlobals(this, Compute, EMetalShaderStages::Compute, ComputeShader->Bindings);
}

bool FMetalStateCache::PrepareToRestart(bool const bCurrentApplied)
{
	if(CanRestartRenderPass())
	{
		return true;
	}
	else
	{
		FRHIRenderPassInfo Info = GetRenderPassInfo();
		
		ERenderTargetActions DepthActions = GetDepthActions(Info.DepthStencilRenderTarget.Action);
		ERenderTargetActions StencilActions = GetStencilActions(Info.DepthStencilRenderTarget.Action);
		ERenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
		ERenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);
		ERenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
		ERenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);

		if (Info.DepthStencilRenderTarget.DepthStencilTarget)
		{
			if(bCurrentApplied && Info.DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite() && DepthStoreAction == ERenderTargetStoreAction::ENoAction)
			{
				return false;
			}
			if (bCurrentApplied && Info.DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite() && StencilStoreAction == ERenderTargetStoreAction::ENoAction)
			{
				return false;
			}
		
			if (bCurrentApplied || DepthLoadAction != ERenderTargetLoadAction::EClear)
			{
				DepthLoadAction = ERenderTargetLoadAction::ELoad;
			}
			if (Info.DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite())
			{
				DepthStoreAction = ERenderTargetStoreAction::EStore;
			}

			if (bCurrentApplied || StencilLoadAction != ERenderTargetLoadAction::EClear)
			{
				StencilLoadAction = ERenderTargetLoadAction::ELoad;
			}
			if (Info.DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite())
			{
				StencilStoreAction = ERenderTargetStoreAction::EStore;
			}
			
			DepthActions = MakeRenderTargetActions(DepthLoadAction, DepthStoreAction);
			StencilActions = MakeRenderTargetActions(StencilLoadAction, StencilStoreAction);
			Info.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(DepthActions, StencilActions);
		}
		
		for (int32 RenderTargetIndex = 0; RenderTargetIndex < Info.GetNumColorRenderTargets(); RenderTargetIndex++)
		{
			FRHIRenderPassInfo::FColorEntry& RenderTargetView = Info.ColorRenderTargets[RenderTargetIndex];
			ERenderTargetLoadAction LoadAction = GetLoadAction(RenderTargetView.Action);
			ERenderTargetStoreAction StoreAction = GetStoreAction(RenderTargetView.Action);
			
			if(bCurrentApplied && StoreAction == ERenderTargetStoreAction::ENoAction)
			{
				return false;
			}
			
			if (!bCurrentApplied && LoadAction == ERenderTargetLoadAction::EClear)
			{
				StoreAction == ERenderTargetStoreAction::EStore;
			}
			else
			{
				LoadAction = ERenderTargetLoadAction::ELoad;
			}
			RenderTargetView.Action = MakeRenderTargetActions(LoadAction, StoreAction);
			check(RenderTargetView.RenderTarget == nil || GetStoreAction(RenderTargetView.Action) != ERenderTargetStoreAction::ENoAction);
		}
		
		InvalidateRenderTargets();
		return SetRenderPassInfo(Info, GetVisibilityResultsBuffer(), true) && CanRestartRenderPass();
	}
}

void FMetalStateCache::SetStateDirty(void)
{	
	RasterBits = UINT32_MAX;
    PipelineBits = EMetalPipelineFlagMask;
	for (uint32 i = 0; i < EMetalShaderStages::Num; i++)
	{
		ShaderBuffers[i].Bound = UINT32_MAX;
		ShaderTextures[i].Bound = FMETALTEXTUREMASK_MAX;
		ShaderSamplers[i].Bound = UINT16_MAX;
	}
}

void FMetalStateCache::SetShaderBufferDirty(EMetalShaderStages const Frequency, NSUInteger const Index)
{
	ShaderBuffers[Frequency].Bound |= (1 << Index);
}

void FMetalStateCache::SetRenderStoreActions(FMetalCommandEncoder& CommandEncoder, bool const bConditionalSwitch)
{
	check(CommandEncoder.IsRenderCommandEncoderActive())
	{
		if (bConditionalSwitch)
		{
			ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> ColorAttachments = RenderPassDesc.GetColorAttachments();
			for (int32 RenderTargetIndex = 0; RenderTargetIndex < RenderPassInfo.GetNumColorRenderTargets(); RenderTargetIndex++)
			{
				FRHIRenderPassInfo::FColorEntry& RenderTargetView = RenderPassInfo.ColorRenderTargets[RenderTargetIndex];
				if(RenderTargetView.RenderTarget != nil)
				{
					const bool bMultiSampled = (ColorAttachments[RenderTargetIndex].GetTexture().GetSampleCount() > 1);
					ColorStore[RenderTargetIndex] = GetConditionalMetalRTStoreAction(bMultiSampled);
				}
			}
			
			if (RenderPassInfo.DepthStencilRenderTarget.DepthStencilTarget)
			{
				const bool bMultiSampled = RenderPassDesc.GetDepthAttachment().GetTexture() && (RenderPassDesc.GetDepthAttachment().GetTexture().GetSampleCount() > 1);
				DepthStore = GetConditionalMetalRTStoreAction(bMultiSampled);
				StencilStore = GetConditionalMetalRTStoreAction(false);
			}
		}
		CommandEncoder.SetRenderPassStoreActions(ColorStore, DepthStore, StencilStore);
	}
}

void FMetalStateCache::FlushVisibilityResults(FMetalCommandEncoder& CommandEncoder)
{
#if PLATFORM_MAC
	if(VisibilityResults && VisibilityResults->Buffer && VisibilityResults->Buffer.GetStorageMode() == mtlpp::StorageMode::Managed && VisibilityWritten && CommandEncoder.IsRenderCommandEncoderActive())
	{
		TRefCountPtr<FMetalFence> Fence = CommandEncoder.EndEncoding();
		
        CommandEncoder.BeginBlitCommandEncoding();
		CommandEncoder.WaitForFence(Fence);
		
		mtlpp::BlitCommandEncoder& Encoder = CommandEncoder.GetBlitCommandEncoder();

		// METAL_GPUPROFILE(FMetalProfiler::GetProfiler()->EncodeBlit(CommandEncoder.GetCommandBufferStats(), __FUNCTION__));
		MTLPP_VALIDATE(mtlpp::BlitCommandEncoder, Encoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Synchronize(VisibilityResults->Buffer));
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, CommandEncoder.GetBlitCommandEncoderDebugging().Synchronize(VisibilityResults->Buffer));
		
		VisibilityWritten = 0;
	}
#endif
}

void FMetalStateCache::SetRenderState(FMetalCommandEncoder& CommandEncoder, FMetalCommandEncoder* PrologueEncoder)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalSetRenderStateTime);
	
	if (RasterBits)
	{
		if (RasterBits & EMetalRenderFlagViewport)
		{
			CommandEncoder.SetViewport(Viewport, ActiveViewports);
		}
		if (RasterBits & EMetalRenderFlagFrontFacingWinding)
		{
			CommandEncoder.SetFrontFacingWinding(mtlpp::Winding::CounterClockwise);
		}
		if (RasterBits & EMetalRenderFlagCullMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetCullMode(TranslateCullMode(RasterizerState->State.CullMode));
		}
		if (RasterBits & EMetalRenderFlagDepthBias)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetDepthBias(RasterizerState->State.DepthBias, RasterizerState->State.SlopeScaleDepthBias, FLT_MAX);
		}
		if ((RasterBits & EMetalRenderFlagScissorRect))
		{
			CommandEncoder.SetScissorRect(Scissor, ActiveScissors);
		}
		if (RasterBits & EMetalRenderFlagTriangleFillMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetTriangleFillMode(TranslateFillMode(RasterizerState->State.FillMode));
		}
		if (RasterBits & EMetalRenderFlagBlendColor)
		{
			CommandEncoder.SetBlendColor(BlendFactor.R, BlendFactor.G, BlendFactor.B, BlendFactor.A);
		}
		if (RasterBits & EMetalRenderFlagDepthStencilState)
		{
			check(IsValidRef(DepthStencilState));
            
            if (DepthStencilState && RenderPassDesc && SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
            {
                METAL_FATAL_ASSERT(DepthStencilState->bIsDepthWriteEnabled == false || (RenderPassDesc.GetDepthAttachment() && RenderPassDesc.GetDepthAttachment().GetTexture()) , TEXT("Attempting to set a depth-stencil state that writes depth but no depth texture is configured!\nState: %s\nRender Pass: %s"), *FString([DepthStencilState->State.GetPtr() description]), *FString([RenderPassDesc.GetPtr() description]));
                METAL_FATAL_ASSERT(DepthStencilState->bIsStencilWriteEnabled == false || (RenderPassDesc.GetStencilAttachment() && RenderPassDesc.GetStencilAttachment().GetTexture()), TEXT("Attempting to set a depth-stencil state that writes stencil but no stencil texture is configured!\nState: %s\nRender Pass: %s"), *FString([DepthStencilState->State.GetPtr() description]), *FString([RenderPassDesc.GetPtr() description]));
            }
            
			CommandEncoder.SetDepthStencilState(DepthStencilState ? DepthStencilState->State : nil);
		}
		if (RasterBits & EMetalRenderFlagStencilReferenceValue)
		{
			CommandEncoder.SetStencilReferenceValue(StencilRef);
		}
		if (RasterBits & EMetalRenderFlagVisibilityResultMode)
		{
			CommandEncoder.SetVisibilityResultMode(VisibilityMode, VisibilityOffset);
			if (VisibilityMode != mtlpp::VisibilityResultMode::Disabled)
			{
            	VisibilityWritten = VisibilityOffset + FMetalQueryBufferPool::EQueryResultMaxSize;
			}
		}
		if (RasterBits & EMetalRenderFlagDepthClipMode)
		{
			check(IsValidRef(RasterizerState));
			CommandEncoder.SetDepthClipMode(TranslateDepthClipMode(RasterizerState->State.DepthClipMode));
		}
		RasterBits = 0;
	}
}

void FMetalStateCache::EnsureTextureAndType(EMetalShaderStages Stage, uint32 Index, const TMap<uint8, uint8>& TexTypes) const
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (ShaderTextures[Stage].Textures[Index])
	{
		if (ShaderTextures[Stage].Textures[Index].GetTextureType() != (mtlpp::TextureType)TexTypes.FindRef(Index))
		{
			ensureMsgf(0, TEXT("Mismatched texture type: EMetalShaderStages %d, Index %d, ShaderTextureType %d != TexTypes %d"), (uint32)Stage, Index, (uint32)ShaderTextures[Stage].Textures[Index].GetTextureType(), (uint32)TexTypes.FindRef(Index));
		}
	}
	else
	{
		ensureMsgf(0, TEXT("NULL texture: EMetalShaderStages %d, Index %d"), (uint32)Stage, Index);
	}
#endif
}

void FMetalStateCache::SetRenderPipelineState(FMetalCommandEncoder& CommandEncoder, FMetalCommandEncoder* PrologueEncoder)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalSetRenderPipelineStateTime);
	
    if ((PipelineBits & EMetalPipelineFlagRasterMask) != 0)
    {
    	// Some Intel drivers need RenderPipeline state to be set after DepthStencil state to work properly
    	FMetalShaderPipeline* Pipeline = GetPipelineState();

		check(Pipeline);
        CommandEncoder.SetRenderPipelineState(Pipeline);
        if (Pipeline->ComputePipelineState)
        {
            check(PrologueEncoder);
            PrologueEncoder->SetComputePipelineState(Pipeline);
        }
        
        PipelineBits &= EMetalPipelineFlagComputeMask;
    }
	
#if METAL_DEBUG_OPTIONS
	if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
	{
		FMetalShaderPipeline* Pipeline = GetPipelineState();
		EMetalShaderStages VertexStage = EMetalShaderStages::Vertex;
		
		FMetalDebugShaderResourceMask VertexMask = Pipeline->ResourceMask[EMetalShaderVertex];
		TArray<uint32>& MinVertexBufferSizes = Pipeline->BufferDataSizes[EMetalShaderVertex];
		const TMap<uint8, uint8>& VertexTexTypes = Pipeline->TextureTypes[EMetalShaderVertex];
		while(VertexMask.BufferMask)
		{
			uint32 Index = __builtin_ctz(VertexMask.BufferMask);
			VertexMask.BufferMask &= ~(1 << Index);
			
			if (VertexStage == EMetalShaderStages::Vertex)
			{
				FMetalBufferBinding const& Binding = ShaderBuffers[VertexStage].Buffers[Index];
				ensure(Binding.Buffer || Binding.Bytes);
				ensure(MinVertexBufferSizes.Num() > Index);
				ensure(Binding.Length >= MinVertexBufferSizes[Index]);
			}
		}
#if PLATFORM_MAC
		{
			uint64 LoTextures = (uint64)VertexMask.TextureMask;
			while(LoTextures)
			{
				uint32 Index = __builtin_ctzll(LoTextures);
				LoTextures &= ~(uint64(1) << uint64(Index));
				EnsureTextureAndType(VertexStage, Index, VertexTexTypes);
			}
			
			uint64 HiTextures = (uint64)(VertexMask.TextureMask >> FMetalTextureMask(64));
			while(HiTextures)
			{
				uint32 Index = __builtin_ctzll(HiTextures);
				HiTextures &= ~(uint64(1) << uint64(Index));
				EnsureTextureAndType(VertexStage, Index + 64, VertexTexTypes);
			}
		}
#else
		while(VertexMask.TextureMask)
		{
			uint32 Index = __builtin_ctz(VertexMask.TextureMask);
			VertexMask.TextureMask &= ~(1 << Index);
			
			EnsureTextureAndType(VertexStage, Index, VertexTexTypes);
		}
#endif
		while(VertexMask.SamplerMask)
		{
			uint32 Index = __builtin_ctz(VertexMask.SamplerMask);
			VertexMask.SamplerMask &= ~(1 << Index);
			ensure(ShaderSamplers[VertexStage].Samplers[Index]);
		}
		
		FMetalDebugShaderResourceMask FragmentMask = Pipeline->ResourceMask[EMetalShaderFragment];
		TArray<uint32>& MinFragmentBufferSizes = Pipeline->BufferDataSizes[EMetalShaderFragment];
		const TMap<uint8, uint8>& FragmentTexTypes = Pipeline->TextureTypes[EMetalShaderFragment];
		while(FragmentMask.BufferMask)
		{
			uint32 Index = __builtin_ctz(FragmentMask.BufferMask);
			FragmentMask.BufferMask &= ~(1 << Index);
			
			FMetalBufferBinding const& Binding = ShaderBuffers[EMetalShaderStages::Pixel].Buffers[Index];
			ensure(Binding.Buffer || Binding.Bytes);
			ensure(MinFragmentBufferSizes.Num() > Index);
			ensure(Binding.Length >= MinFragmentBufferSizes[Index]);
		}
#if PLATFORM_MAC
		{
			uint64 LoTextures = (uint64)FragmentMask.TextureMask;
			while(LoTextures)
			{
				uint32 Index = __builtin_ctzll(LoTextures);
				LoTextures &= ~(uint64(1) << uint64(Index));
				EnsureTextureAndType(EMetalShaderStages::Pixel, Index, FragmentTexTypes);
			}
			
			uint64 HiTextures = (uint64)(FragmentMask.TextureMask >> FMetalTextureMask(64));
			while(HiTextures)
			{
				uint32 Index = __builtin_ctzll(HiTextures);
				HiTextures &= ~(uint64(1) << uint64(Index));
				EnsureTextureAndType(EMetalShaderStages::Pixel, Index + 64, FragmentTexTypes);
			}
		}
#else
		while(FragmentMask.TextureMask)
		{
			uint32 Index = __builtin_ctz(FragmentMask.TextureMask);
			FragmentMask.TextureMask &= ~(1 << Index);
			
			EnsureTextureAndType(EMetalShaderStages::Pixel, Index, FragmentTexTypes);
		}
#endif
		while(FragmentMask.SamplerMask)
		{
			uint32 Index = __builtin_ctz(FragmentMask.SamplerMask);
			FragmentMask.SamplerMask &= ~(1 << Index);
			ensure(ShaderSamplers[EMetalShaderStages::Pixel].Samplers[Index]);
		}
	}
#endif // METAL_DEBUG_OPTIONS
}

void FMetalStateCache::SetComputePipelineState(FMetalCommandEncoder& CommandEncoder)
{
	if ((PipelineBits & EMetalPipelineFlagComputeMask) != 0)
	{
		FMetalShaderPipeline* Pipeline = ComputeShader->GetPipeline();
	    check(Pipeline);
	    CommandEncoder.SetComputePipelineState(Pipeline);
        
        PipelineBits &= EMetalPipelineFlagRasterMask;
    }
	
	if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
	{
		FMetalShaderPipeline* Pipeline = ComputeShader->GetPipeline();
		check(Pipeline);
		
		FMetalDebugShaderResourceMask ComputeMask = Pipeline->ResourceMask[EMetalShaderCompute];
		TArray<uint32>& MinComputeBufferSizes = Pipeline->BufferDataSizes[EMetalShaderCompute];
		const TMap<uint8, uint8>& ComputeTexTypes = Pipeline->TextureTypes[EMetalShaderCompute];
		while(ComputeMask.BufferMask)
		{
			uint32 Index = __builtin_ctz(ComputeMask.BufferMask);
			ComputeMask.BufferMask &= ~(1 << Index);
			
			FMetalBufferBinding const& Binding = ShaderBuffers[EMetalShaderStages::Compute].Buffers[Index];
			ensure(Binding.Buffer || Binding.Bytes);
			ensure(MinComputeBufferSizes.Num() > Index);
			ensure(Binding.Length >= MinComputeBufferSizes[Index]);
		}
#if PLATFORM_MAC
		{
			uint64 LoTextures = (uint64)ComputeMask.TextureMask;
			while(LoTextures)
			{
				uint32 Index = __builtin_ctzll(LoTextures);
				LoTextures &= ~(uint64(1) << uint64(Index));
				EnsureTextureAndType(EMetalShaderStages::Compute, Index, ComputeTexTypes);
			}
			
			uint64 HiTextures = (uint64)(ComputeMask.TextureMask >> FMetalTextureMask(64));
			while(HiTextures)
			{
				uint32 Index = __builtin_ctzll(HiTextures);
				HiTextures &= ~(uint64(1) << uint64(Index));
				EnsureTextureAndType(EMetalShaderStages::Compute, Index + 64, ComputeTexTypes);
			}
		}
#else
		while(ComputeMask.TextureMask)
		{
			uint32 Index = __builtin_ctz(ComputeMask.TextureMask);
			ComputeMask.TextureMask &= ~(1 << Index);
			
			EnsureTextureAndType(EMetalShaderStages::Compute, Index, ComputeTexTypes);
		}
#endif
		while(ComputeMask.SamplerMask)
		{
			uint32 Index = __builtin_ctz(ComputeMask.SamplerMask);
			ComputeMask.SamplerMask &= ~(1 << Index);
			ensure(ShaderSamplers[EMetalShaderStages::Compute].Samplers[Index]);
		}
	}
}

void FMetalStateCache::CommitResourceTable(EMetalShaderStages const Frequency, mtlpp::FunctionType const Type, FMetalCommandEncoder& CommandEncoder)
{
	FMetalBufferBindings& BufferBindings = ShaderBuffers[Frequency];
	while(BufferBindings.Bound)
	{
		uint32 Index = __builtin_ctz(BufferBindings.Bound);
		BufferBindings.Bound &= ~(1 << Index);
		
		if (Index < ML_MaxBuffers)
		{
			FMetalBufferBinding& Binding = BufferBindings.Buffers[Index];
			if (Binding.Buffer)
			{
				CommandEncoder.SetShaderBuffer(Type, Binding.Buffer, Binding.Offset, Binding.Length, Index, Binding.Usage, BufferBindings.Formats[Index], Binding.ElementRowPitch);
				
				if (Binding.Buffer.IsSingleUse())
				{
					Binding.Buffer = nil;
				}
			}
			else if (Binding.Bytes)
			{
				CommandEncoder.SetShaderData(Type, Binding.Bytes, Binding.Offset, Index, BufferBindings.Formats[Index], Binding.ElementRowPitch);
			}
		}
	}
	
	FMetalTextureBindings& TextureBindings = ShaderTextures[Frequency];
#if PLATFORM_MAC
	uint64 LoTextures = (uint64)TextureBindings.Bound;
	while(LoTextures)
	{
		uint32 Index = __builtin_ctzll(LoTextures);
		LoTextures &= ~(uint64(1) << uint64(Index));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index, TextureBindings.Usage[Index]);
		}
	}
	
	uint64 HiTextures = (uint64)(TextureBindings.Bound >> FMetalTextureMask(64));
	while(HiTextures)
	{
		uint32 Index = __builtin_ctzll(HiTextures);
		HiTextures &= ~(uint64(1) << uint64(Index));
		Index += 64;
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index, TextureBindings.Usage[Index]);
		}
	}
	
	TextureBindings.Bound = FMetalTextureMask(LoTextures) | (FMetalTextureMask(HiTextures) << FMetalTextureMask(64));
	check(TextureBindings.Bound == 0);
#else
	while(TextureBindings.Bound)
	{
		uint32 Index = __builtin_ctz(TextureBindings.Bound);
		TextureBindings.Bound &= ~(FMetalTextureMask(FMetalTextureMask(1) << FMetalTextureMask(Index)));
		
		if (Index < ML_MaxTextures && TextureBindings.Textures[Index])
		{
			CommandEncoder.SetShaderTexture(Type, TextureBindings.Textures[Index], Index, TextureBindings.Usage[Index]);
		}
	}
#endif
	
    FMetalSamplerBindings& SamplerBindings = ShaderSamplers[Frequency];
	while(SamplerBindings.Bound)
	{
		uint32 Index = __builtin_ctz(SamplerBindings.Bound);
		SamplerBindings.Bound &= ~(1 << Index);
		
		if (Index < ML_MaxSamplers && SamplerBindings.Samplers[Index])
		{
			CommandEncoder.SetShaderSamplerState(Type, SamplerBindings.Samplers[Index], Index);
		}
	}
}

FMetalBuffer& FMetalStateCache::GetDebugBuffer()
{
    if (!DebugBuffer)
    {
        // Assume worst case tiling (16x16) and render-target size (4096x4096) on iOS for now
        uint32 NumTiles = PLATFORM_MAC ? 1 : 65536;
        DebugBuffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), NumTiles * sizeof(FMetalDebugInfo), BUF_Dynamic, mtlpp::StorageMode::Shared));
    }
    return DebugBuffer;
}

FTexture2DRHIRef FMetalStateCache::CreateFallbackDepthStencilSurface(uint32 Width, uint32 Height)
{
#if PLATFORM_MAC
	if (!IsValidRef(FallbackDepthStencilSurface) || FallbackDepthStencilSurface->GetSizeX() < Width || FallbackDepthStencilSurface->GetSizeY() < Height)
#else
	if (!IsValidRef(FallbackDepthStencilSurface) || FallbackDepthStencilSurface->GetSizeX() != Width || FallbackDepthStencilSurface->GetSizeY() != Height)
#endif
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FallbackDepthStencilSurface"), Width, Height, PF_DepthStencil)
			.SetFlags(ETextureCreateFlags::DepthStencilTargetable);

		FallbackDepthStencilSurface = RHICreateTexture(Desc);
	}
	check(IsValidRef(FallbackDepthStencilSurface));
	return FallbackDepthStencilSurface;
}

void FMetalStateCache::DiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	if (Depth)
	{
		switch (DepthStore)
		{
			case mtlpp::StoreAction::Unknown:
			case mtlpp::StoreAction::Store:
				DepthStore = mtlpp::StoreAction::DontCare;
				break;
			case mtlpp::StoreAction::StoreAndMultisampleResolve:
				DepthStore = mtlpp::StoreAction::MultisampleResolve;
				break;
			default:
				break;
		}
	}

	if (Stencil)
	{
		StencilStore = mtlpp::StoreAction::DontCare;
	}

	for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		if ((ColorBitMask & (1u << Index)) != 0)
		{
			switch (ColorStore[Index])
			{
				case mtlpp::StoreAction::Unknown:
				case mtlpp::StoreAction::Store:
					ColorStore[Index] = mtlpp::StoreAction::DontCare;
					break;
				case mtlpp::StoreAction::StoreAndMultisampleResolve:
					ColorStore[Index] = mtlpp::StoreAction::MultisampleResolve;
					break;
				default:
					break;
			}
		}
	}
}
