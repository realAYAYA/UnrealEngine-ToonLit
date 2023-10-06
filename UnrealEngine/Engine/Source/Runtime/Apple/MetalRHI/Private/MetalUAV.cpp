// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalRHIStagingBuffer.h"
#include "MetalCommandBuffer.h"
#include "RenderUtils.h"
#include "ClearReplacementShaders.h"
#include "MetalTransitionData.h"

void FMetalViewableResource::UpdateLinkedViews()
{
	for (FMetalResourceViewBase* View = LinkedViews; View; View = View->Next())
	{
		View->UpdateView();
	}
}


FMetalResourceViewBase::~FMetalResourceViewBase()
{
	Invalidate();
	Unlink();
}

void FMetalResourceViewBase::Invalidate()
{
	if (bOwnsResource)
	{
		// @todo - SRV/UAV refactor - is releasing objects like this safe / correct?
		switch (GetMetalType())
		{
		case EMetalType::TextureView:
			SafeReleaseMetalTexture(Storage.Get<FMetalTexture>()); 
			break;

		case EMetalType::BufferView:
			SafeReleaseMetalBuffer(Storage.Get<FBufferView>().Buffer);
			break;
                
        case EMetalType::TextureBufferBacked:
            FTextureBufferBacked & View = Storage.Get<FTextureBufferBacked>();
            SafeReleaseMetalTexture(View.Texture);
            break;
		}
	}

	Storage.Emplace<FEmptyVariantState>();
	bOwnsResource = true;
}

FMetalTexture& FMetalResourceViewBase::InitAsTextureView()
{
	check(GetMetalType() == EMetalType::Null);
	Storage.Emplace<FMetalTexture>();
	return Storage.Get<FMetalTexture>();
}

void FMetalResourceViewBase::InitAsBufferView(FMetalBuffer& Buffer, uint32 Offset, uint32 Size)
{
	check(GetMetalType() == EMetalType::Null);
	Storage.Emplace<FBufferView>(Buffer, Offset, Size);
	bOwnsResource = false;
}

void FMetalResourceViewBase::InitAsTextureBufferBacked(FMetalTexture& Texture, FMetalBuffer& Buffer, uint32 Offset, uint32 Size, EPixelFormat Format = EPixelFormat::PF_Unknown)
{
    check(GetMetalType() == EMetalType::Null);
    Storage.Emplace<FTextureBufferBacked>(Texture, Buffer, Offset, Size, Format);
}

FMetalShaderResourceView::FMetalShaderResourceView(FRHICommandListBase& RHICmdList, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
	: FRHIShaderResourceView(InResource, InViewDesc)
{
	RHICmdList.EnqueueLambda([this](FRHICommandListBase&)
	{
		LinkHead(GetBaseResource()->LinkedViews);
		UpdateView();
	});
}

FMetalViewableResource* FMetalShaderResourceView::GetBaseResource() const
{
	return IsBuffer()
		? static_cast<FMetalViewableResource*>(ResourceCast(GetBuffer()))
		: static_cast<FMetalViewableResource*>(ResourceCast(GetTexture()));
}

mtlpp::TextureType UAVDimensionToMetalTextureType(FRHIViewDesc::EDimension Dimension)
{
    switch (Dimension)
    {
        case FRHIViewDesc::EDimension::Texture2D:
            return mtlpp::TextureType::Texture2D;
        case FRHIViewDesc::EDimension::Texture2DArray:
        case FRHIViewDesc::EDimension::TextureCube:
        case FRHIViewDesc::EDimension::TextureCubeArray:
            return mtlpp::TextureType::Texture2DArray;
        case FRHIViewDesc::EDimension::Texture3D:
            return mtlpp::TextureType::Texture3D;
        default:
            checkNoEntry();
    }
    
    return mtlpp::TextureType::Texture2D;
}

mtlpp::TextureType SRVDimensionToMetalTextureType(FRHIViewDesc::EDimension Dimension)
{
    switch (Dimension)
    {
        case FRHIViewDesc::EDimension::Texture2D:
            return mtlpp::TextureType::Texture2D;
        case FRHIViewDesc::EDimension::Texture2DArray:
            return mtlpp::TextureType::Texture2DArray;
        case FRHIViewDesc::EDimension::TextureCube:
            return mtlpp::TextureType::TextureCube;
        case FRHIViewDesc::EDimension::TextureCubeArray:
            return mtlpp::TextureType::TextureCubeArray;
        case FRHIViewDesc::EDimension::Texture3D:
            return mtlpp::TextureType::Texture3D;
        default:
            checkNoEntry();
    }
    
    return mtlpp::TextureType::Texture2D;
}

void FMetalShaderResourceView::UpdateView()
{
	Invalidate();

	if (IsBuffer())
	{
		FMetalRHIBuffer* Buffer = ResourceCast(GetBuffer());
		auto const Info = ViewDesc.Buffer.SRV.GetViewInfo(Buffer);

		if (!Info.bNullView)
		{
			switch (Info.BufferType)
			{
			case FRHIViewDesc::EBufferType::Typed:
				{
					check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesTextureBuffers));

					mtlpp::PixelFormat Format = (mtlpp::PixelFormat)GMetalBufferFormats[Info.Format].LinearTextureFormat;
					NSUInteger Options = ((NSUInteger)Buffer->Mode) << mtlpp::ResourceStorageModeShift;

                    const uint32 MinimumByteAlignment = GetMetalDeviceContext().GetDevice().GetMinimumLinearTextureAlignmentForPixelFormat(Format);
                    const uint32 MinimumElementAlignment = MinimumByteAlignment / Info.StrideInBytes;
                    uint32 NumElements = Align(Info.NumElements, MinimumElementAlignment);
                    uint32 SizeInBytes = NumElements * Info.StrideInBytes;
                    
					auto Desc = mtlpp::TextureDescriptor::TextureBufferDescriptor(
						  Format
						, NumElements
						, mtlpp::ResourceOptions(Options)
						, mtlpp::TextureUsage::ShaderRead
					);

					Desc.SetAllowGPUOptimisedContents(false);

					FMetalTexture& View = InitAsTextureView();
					View = Buffer->GetCurrentBuffer().NewTexture(Desc, Info.OffsetInBytes, SizeInBytes);
				}
				break;

			case FRHIViewDesc::EBufferType::Raw:
			case FRHIViewDesc::EBufferType::Structured:
				{
					InitAsBufferView(Buffer->GetCurrentBuffer(), Info.OffsetInBytes, Info.SizeInBytes);
				}
				break;

			default:
				checkNoEntry();
				break;
			}
		}
	}
	else
	{
		FMetalTexture& View = InitAsTextureView();

		FMetalSurface* Texture = ResourceCast(GetTexture());
		auto const Info = ViewDesc.Texture.SRV.GetViewInfo(Texture);

		// Texture must have been created with view support.
		check(Texture->Texture.GetUsage() & mtlpp::TextureUsage::PixelFormatView);

#if PLATFORM_IOS
		// Memoryless targets can't have texture views (SRVs or UAVs)
		check(Texture->Texture.GetStorageMode() != mtlpp::StorageMode::Memoryless);
#endif

		mtlpp::PixelFormat MetalFormat = UEToMetalFormat(Info.Format, Info.bSRGB);
        mtlpp::TextureType TextureType = Texture->Texture.GetTextureType();

        if (EnumHasAnyFlags(Texture->GetDesc().Flags, TexCreate_SRGB) && !Info.bSRGB)
        {
#if PLATFORM_MAC
            // R8Unorm has been expanded in the source surface for sRGBA support - we need to expand to RGBA to enable compatible texture format view for non apple silicon macs
            if (Info.Format == PF_G8 && Texture->Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
            {
                MetalFormat = mtlpp::PixelFormat::RGBA8Unorm;
            }
#endif
        }

        if (Info.Format == PF_X24_G8)
        {
            // Stencil buffer view of a depth texture
            check(Texture->GetDesc().Format == PF_DepthStencil);
            switch (Texture->Texture.GetPixelFormat())
            {
            default: checkNoEntry(); break;
#if PLATFORM_MAC
            case mtlpp::PixelFormat::Depth24Unorm_Stencil8: MetalFormat = mtlpp::PixelFormat::X24_Stencil8; break;
#endif
            case mtlpp::PixelFormat::Depth32Float_Stencil8: MetalFormat = mtlpp::PixelFormat::X32_Stencil8; break;
            }
        }
        
        bool bUseSourceTexture = Info.bAllMips && Info.bAllSlices && MetalFormat == Texture->Texture.GetPixelFormat() &&
                                SRVDimensionToMetalTextureType(Info.Dimension) == TextureType;
        
		// We can use the source texture directly if the view's format / mip count etc matches.
		if (bUseSourceTexture)
		{
			// View is exactly compatible with the original texture.
			View = Texture->Texture;
			bOwnsResource = false;
		}
		else
		{
            TextureType = SRVDimensionToMetalTextureType(Info.Dimension);

            uint32_t ArrayStart = Info.ArrayRange.First;
            uint32_t ArraySize = Info.ArrayRange.Num;
            
            if (Info.Dimension == FRHIViewDesc::EDimension::TextureCube ||
                Info.Dimension == FRHIViewDesc::EDimension::TextureCubeArray)
            {
                ArrayStart = Info.ArrayRange.First * 6;
                ArraySize = Info.ArrayRange.Num * 6;
            }
            
			View = Texture->Texture.NewTextureView(
				MetalFormat,
                TextureType,
				ns::Range(Info.MipRange.First, Info.MipRange.Num),
				ns::Range(ArrayStart, ArraySize)
			);

#if METAL_DEBUG_OPTIONS
			View.SetLabel([Texture->Texture.GetLabel() stringByAppendingString:@"_TextureView"]);
#endif
		}
	}
}




FMetalUnorderedAccessView::FMetalUnorderedAccessView(FRHICommandListBase& RHICmdList, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
	: FRHIUnorderedAccessView(InResource, InViewDesc)
{
	RHICmdList.EnqueueLambda([this](FRHICommandListBase&)
	{
		LinkHead(GetBaseResource()->LinkedViews);
		UpdateView();
	});
}

FMetalViewableResource* FMetalUnorderedAccessView::GetBaseResource() const
{
	return IsBuffer()
		? static_cast<FMetalViewableResource*>(ResourceCast(GetBuffer()))
		: static_cast<FMetalViewableResource*>(ResourceCast(GetTexture()));
}

void FMetalUnorderedAccessView::UpdateView()
{
	Invalidate();

	if (IsBuffer())
	{
		FMetalRHIBuffer* Buffer = ResourceCast(GetBuffer());
		auto const Info = ViewDesc.Buffer.UAV.GetViewInfo(Buffer);

		checkf(!Info.bAtomicCounter && !Info.bAppendBuffer, TEXT("UAV counters not implemented."));

		if (!Info.bNullView)
		{
			switch (Info.BufferType)
			{
			case FRHIViewDesc::EBufferType::Typed:
			{
				check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesTextureBuffers));

				mtlpp::PixelFormat Format = (mtlpp::PixelFormat)GMetalBufferFormats[Info.Format].LinearTextureFormat;
				NSUInteger Options = ((NSUInteger)Buffer->Mode) << mtlpp::ResourceStorageModeShift;

                const uint32 MinimumByteAlignment = GetMetalDeviceContext().GetDevice().GetMinimumLinearTextureAlignmentForPixelFormat(Format);
                const uint32 MinimumElementAlignment = MinimumByteAlignment / Info.StrideInBytes;
                uint32 NumElements = Align(Info.NumElements, MinimumElementAlignment);
                uint32 SizeInBytes = NumElements * Info.StrideInBytes;
                
				auto Desc = mtlpp::TextureDescriptor::TextureBufferDescriptor(
					Format
					, NumElements
					, mtlpp::ResourceOptions(Options)
					, mtlpp::TextureUsage(mtlpp::TextureUsage::ShaderRead | mtlpp::TextureUsage::ShaderWrite)
				);

				Desc.SetAllowGPUOptimisedContents(false);

                FMetalTexture MetalTexture(Buffer->GetCurrentBuffer().NewTexture(Desc, Info.OffsetInBytes, SizeInBytes));
                InitAsTextureBufferBacked(MetalTexture, Buffer->GetCurrentBuffer(), Info.OffsetInBytes, SizeInBytes, Info.Format);
			}
			break;

			case FRHIViewDesc::EBufferType::Raw:
			case FRHIViewDesc::EBufferType::Structured:
			{
				InitAsBufferView(Buffer->GetCurrentBuffer(), Info.OffsetInBytes, Info.SizeInBytes);
			}
			break;

			default:
				checkNoEntry();
				break;
			}
		}
	}
	else
	{
		FMetalSurface* Texture = ResourceCast(GetTexture());
		auto const Info = ViewDesc.Texture.UAV.GetViewInfo(Texture);

		// Texture must have been created with view support.
		check(Texture->Texture.GetUsage() & mtlpp::TextureUsage::PixelFormatView);

#if PLATFORM_IOS
		// Memoryless targets can't have texture views (SRVs or UAVs)
		check(Texture->Texture.GetStorageMode() != mtlpp::StorageMode::Memoryless);
#endif

		mtlpp::PixelFormat MetalFormat = UEToMetalFormat(Info.Format, false);
        mtlpp::TextureType TextureType = Texture->Texture.GetTextureType();

        if (EnumHasAnyFlags(Texture->GetDesc().Flags, TexCreate_SRGB))
        {
#if PLATFORM_MAC
            // R8Unorm has been expanded in the source surface for sRGBA support - we need to expand to RGBA to enable compatible texture format view for non apple silicon macs
            if (Info.Format == PF_G8 && Texture->Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
            {
                MetalFormat = mtlpp::PixelFormat::RGBA8Unorm;
            }
#endif
        }
        
        if (Info.Format == PF_X24_G8)
        {
            // Stencil buffer view of a depth texture
            check(Texture->GetDesc().Format == PF_DepthStencil);
            switch (Texture->Texture.GetPixelFormat())
            {
            default: checkNoEntry(); break;
#if PLATFORM_MAC
            case mtlpp::PixelFormat::Depth24Unorm_Stencil8: MetalFormat = mtlpp::PixelFormat::X24_Stencil8; break;
#endif
            case mtlpp::PixelFormat::Depth32Float_Stencil8: MetalFormat = mtlpp::PixelFormat::X32_Stencil8; break;
            }
        }
        
        bool bUseSourceTexture = Info.bAllMips && Info.bAllSlices &&
                                UAVDimensionToMetalTextureType(Info.Dimension) == TextureType && MetalFormat == Texture->Texture.GetPixelFormat();
        
        bool bIsAtomicCompatible = EnumHasAllFlags(Texture->GetDesc().Flags, TexCreate_AtomicCompatible) ||
                                            EnumHasAllFlags(Texture->GetDesc().Flags, ETextureCreateFlags::Atomic64Compatible);
        
        // We can use the source texture directly if the view's format / mip count etc matches.
        if (bUseSourceTexture)
		{
            // If we are using texture atomics then we need to bind them as buffers because Metal lacks texture atomics
            if((EnumHasAllFlags(Texture->GetDesc().Flags, TexCreate_UAV | TexCreate_NoTiling) || bIsAtomicCompatible) && Texture->Texture.GetBuffer())
            {
                FMetalBuffer MetalBuffer(Texture->Texture.GetBuffer(), false);
                InitAsTextureBufferBacked(Texture->Texture, MetalBuffer,
                                        Texture->Texture.GetBufferOffset(),
                                        Texture->Texture.GetBuffer().GetLength(), Info.Format);
            }
            else
            {
                FMetalTexture& View = InitAsTextureView();
                View = Texture->Texture;
            }
            bOwnsResource = false;
		}
		else
		{
            uint32_t ArrayStart = Info.ArrayRange.First;
            uint32_t ArraySize = Info.ArrayRange.Num;
            TextureType = UAVDimensionToMetalTextureType(Info.Dimension);
            
            if (Info.Dimension == FRHIViewDesc::EDimension::TextureCube ||
                Info.Dimension == FRHIViewDesc::EDimension::TextureCubeArray)
            {
                ArrayStart = Info.ArrayRange.First * 6;
                ArraySize = Info.ArrayRange.Num * 6;
            }
            
            // Metal doesn't support atomic Texture2DArray
            if(bIsAtomicCompatible && Info.Dimension == FRHIViewDesc::EDimension::Texture2DArray)
            {
                TextureType = mtlpp::TextureType::Texture2D;
            }
            
			FMetalTexture MetalTexture(Texture->Texture.NewTextureView(
				MetalFormat,
                TextureType,
				ns::Range(Info.MipLevel, 1),
                ns::Range(ArrayStart, ArraySize))
			);
            
            // If we are using texture atomics then we need to bind them as buffers because Metal lacks texture atomics
            if((EnumHasAllFlags(Texture->GetDesc().Flags, TexCreate_UAV | TexCreate_NoTiling) || bIsAtomicCompatible)
				 && Texture->Texture.GetBuffer())
            {
                FMetalBuffer MetalBuffer(Texture->Texture.GetBuffer(), false);
                InitAsTextureBufferBacked(MetalTexture, MetalBuffer,
                                          Texture->Texture.GetBufferOffset(),
                                          Texture->Texture.GetBuffer().GetLength(), Info.Format);
            }
            else
            {
                FMetalTexture& View = InitAsTextureView();
                View = MetalTexture;
            }
            
#if METAL_DEBUG_OPTIONS
            MetalTexture.SetLabel([Texture->Texture.GetLabel() stringByAppendingString:@"_TextureView"]);
#endif
        }
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc)
{
	@autoreleasepool {
		return new FMetalShaderResourceView(RHICmdList, Resource, ViewDesc);
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc)
{
	@autoreleasepool {
		return new FMetalUnorderedAccessView(RHICmdList, Resource, ViewDesc);
	}
}


#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
void FMetalUnorderedAccessView::ClearUAVWithBlitEncoder(TRHICommandList_RecursiveHazardous<FMetalRHICommandContext>& RHICmdList, uint32 Pattern)
{
	RHICmdList.RunOnContext([this, Pattern](FMetalRHICommandContext& Context)
	{
		SCOPED_AUTORELEASE_POOL;

		FMetalRHIBuffer* SourceBuffer = ResourceCast(GetBuffer());
        auto const &Info = ViewDesc.Buffer.UAV.GetViewInfo(SourceBuffer);
		FMetalBuffer Buffer = SourceBuffer->GetCurrentBuffer();
		uint32 Size = Info.SizeInBytes;
		uint32 AlignedSize = Align(Size, BufferOffsetAlignment);
		FMetalPooledBufferArgs Args(Context.GetInternalContext().GetDevice(), AlignedSize, BUF_Dynamic, mtlpp::StorageMode::Shared);

		FMetalBuffer Temp = Context.GetInternalContext().CreatePooledBuffer(Args);

		uint32* ContentBytes = (uint32*)Temp.GetContents();
		for (uint32 Element = 0; Element < (AlignedSize >> 2); ++Element)
		{
			ContentBytes[Element] = Pattern;
		}

		Context.GetInternalContext().CopyFromBufferToBuffer(Temp, 0, Buffer, Info.OffsetInBytes, Size);
		Context.GetInternalContext().ReleaseBuffer(Temp);
	});
}
#endif // UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER

void FMetalRHICommandContext::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values)
{
	TRHICommandList_RecursiveHazardous<FMetalRHICommandContext> RHICmdList(this);
	ResourceCast(UnorderedAccessViewRHI)->ClearUAV(RHICmdList, &Values, true);
}

void FMetalRHICommandContext::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
{
	TRHICommandList_RecursiveHazardous<FMetalRHICommandContext> RHICmdList(this);
	ResourceCast(UnorderedAccessViewRHI)->ClearUAV(RHICmdList, &Values, false);
}

void FMetalUnorderedAccessView::ClearUAV(TRHICommandList_RecursiveHazardous<FMetalRHICommandContext>& RHICmdList, const void* ClearValue, bool bFloat)
{
	@autoreleasepool {
		auto GetValueType = [&](EPixelFormat InFormat)
		{
			if (bFloat)
				return EClearReplacementValueType::Float;

			// The Metal validation layer will complain about resources with a
			// signed format bound against an unsigned data format type as the
			// shader parameter.
			switch (InFormat)
			{
			case PF_R32_SINT:
			case PF_R16_SINT:
			case PF_R16G16B16A16_SINT:
				return EClearReplacementValueType::Int32;
			}

			return EClearReplacementValueType::Uint32;
		};

		if (IsBuffer())
		{
			FMetalRHIBuffer* Buffer = ResourceCast(GetBuffer());
			auto const Info = ViewDesc.Buffer.UAV.GetViewInfo(Buffer);

			switch (Info.BufferType)
			{
#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
			case FRHIViewDesc::EBufferType::Raw:
				ClearUAVWithBlitEncoder(RHICmdList, *(const uint32*)ClearValue);
				break;

			case FRHIViewDesc::EBufferType::Structured:
				ClearUAVWithBlitEncoder(RHICmdList, *(const uint32*)ClearValue);
				break;
#endif // UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER

			default:
				ClearUAVShader_T<EClearReplacementResourceType::Buffer, 4, false>(RHICmdList, this, Info.NumElements, 1, 1, ClearValue, GetValueType(Info.Format));
				break;
			}
		}
		else
		{
			FMetalSurface* Texture = ResourceCast(GetTexture());
			auto const Info = ViewDesc.Texture.UAV.GetViewInfo(Texture);

			FIntVector SizeXYZ = Texture->GetMipDimensions(Info.MipLevel);

			switch (Texture->GetDesc().Dimension)
			{
			case ETextureDimension::Texture2D:
				ClearUAVShader_T<EClearReplacementResourceType::Texture2D, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, GetValueType(Info.Format));
				break;

			case ETextureDimension::Texture2DArray:
				ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, Info.ArrayRange.Num, ClearValue, GetValueType(Info.Format));
				break;

			case ETextureDimension::TextureCube:
			case ETextureDimension::TextureCubeArray:
				ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, Info.ArrayRange.Num * 6, ClearValue, GetValueType(Info.Format));
				break;

			case ETextureDimension::Texture3D:
				ClearUAVShader_T<EClearReplacementResourceType::Texture3D, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, GetValueType(Info.Format));
				break;

			default:
				checkNoEntry();
				break;
			}
		}
	} // @autoreleasepool
}

void FMetalGPUFence::WriteInternal(mtlpp::CommandBuffer& CmdBuffer)
{
	Fence = CmdBuffer.GetCompletionFence();
	check(Fence);
}

void FMetalRHICommandContext::RHICopyToStagingBuffer(FRHIBuffer* SourceBufferRHI, FRHIStagingBuffer* DestinationStagingBufferRHI, uint32 Offset, uint32 NumBytes)
{
	@autoreleasepool {
		check(DestinationStagingBufferRHI);

		FMetalRHIStagingBuffer* MetalStagingBuffer = ResourceCast(DestinationStagingBufferRHI);
		ensureMsgf(!MetalStagingBuffer->bIsLocked, TEXT("Attempting to Copy to a locked staging buffer. This may have undefined behavior"));
		FMetalRHIBuffer* SourceBuffer = ResourceCast(SourceBufferRHI);
		FMetalBuffer& ReadbackBuffer = MetalStagingBuffer->ShadowBuffer;

		// Need a shadow buffer for this read. If it hasn't been allocated in our FStagingBuffer or if
		// it's not big enough to hold our readback we need to allocate.
		if (!ReadbackBuffer || ReadbackBuffer.GetLength() < NumBytes)
		{
			if (ReadbackBuffer)
			{
				SafeReleaseMetalBuffer(ReadbackBuffer);
			}
			FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), NumBytes, BUF_Dynamic, mtlpp::StorageMode::Shared);
			ReadbackBuffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
		}

		// Inline copy from the actual buffer to the shadow
		GetMetalDeviceContext().CopyFromBufferToBuffer(SourceBuffer->GetCurrentBuffer(), Offset, ReadbackBuffer, 0, NumBytes);
	}
}

void FMetalRHICommandContext::RHIWriteGPUFence(FRHIGPUFence* FenceRHI)
{
	@autoreleasepool {
		check(FenceRHI);
		FMetalGPUFence* Fence = ResourceCast(FenceRHI);
		Fence->WriteInternal(Context->GetCurrentCommandBuffer());
	}
}

FGPUFenceRHIRef FMetalDynamicRHI::RHICreateGPUFence(const FName &Name)
{
	@autoreleasepool {
	return new FMetalGPUFence(Name);
	}
}

void FMetalGPUFence::Clear()
{
	Fence = mtlpp::CommandBufferFence();
}

bool FMetalGPUFence::Poll() const
{
	if (Fence)
	{
		return Fence.Wait(0);
	}
	else
	{
		return false;
	}
}
