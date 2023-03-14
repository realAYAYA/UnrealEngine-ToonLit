// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalRHIStagingBuffer.h"
#include "MetalCommandBuffer.h"
#include "RenderUtils.h"
#include "ClearReplacementShaders.h"
#include "MetalTransitionData.h"

// Constructor for buffers
FMetalResourceViewBase::FMetalResourceViewBase(FRHIBuffer* InBuffer, uint32 InStartOffsetBytes, uint32 InNumElements, EPixelFormat InFormat)
	: SourceBuffer(ResourceCast(InBuffer))
	, bTexture         (false)
	, bSRGBForceDisable(false)
	, MipLevel         (0)
	, Reserved         (0)
	, NumMips          (0)
	, Format           (InFormat)
	, Stride           (0)
	, Offset           (InBuffer ? InStartOffsetBytes : 0)
{
	check(!bTexture);

	if (SourceBuffer)
	{
		SourceBuffer->AddRef();

		EBufferUsageFlags Usage = SourceBuffer->GetUsage();
		if (EnumHasAnyFlags(Usage, BUF_VertexBuffer))
		{
			if (!SourceBuffer)
			{
				Stride = 0;
			}
			else
			{
				check(SourceBuffer->GetUsage() & BUF_ShaderResource);
				Stride = GPixelFormats[Format].BlockBytes;

				LinearTextureDesc = MakeUnique<FMetalLinearTextureDescriptor>(InStartOffsetBytes, InNumElements, Stride);
				SourceBuffer->CreateLinearTexture((EPixelFormat)Format, SourceBuffer, LinearTextureDesc.Get());
			}
		}
		else if (EnumHasAnyFlags(Usage, BUF_IndexBuffer))
		{
			if (!SourceBuffer)
			{
				Format = PF_R16_UINT;
				Stride = 0;
			}
			else
			{
				Format = (SourceBuffer->IndexType == mtlpp::IndexType::UInt16) ? PF_R16_UINT : PF_R32_UINT;
				Stride = SourceBuffer->GetStride();

				check(Stride == ((Format == PF_R16_UINT) ? 2 : 4));

				LinearTextureDesc = MakeUnique<FMetalLinearTextureDescriptor>(InStartOffsetBytes, InNumElements, Stride);
				SourceBuffer->CreateLinearTexture((EPixelFormat)Format, SourceBuffer, LinearTextureDesc.Get());
			}
		}
		else
		{
			check(EnumHasAnyFlags(Usage, BUF_StructuredBuffer));

			Format = PF_Unknown;
			Stride = SourceBuffer->GetStride();
		}
	}
}

// Constructor for textures
FMetalResourceViewBase::FMetalResourceViewBase(
	  FRHITexture* InTexture
	, EPixelFormat InFormat
	, uint8 InMipLevel
	, uint8 InNumMipLevels
	, ERHITextureSRVOverrideSRGBType InSRGBOverride
	, uint32 InFirstArraySlice
	, uint32 InNumArraySlices
	, bool bInUAV
	)
	: SourceTexture(ResourceCast(InTexture))
	, bTexture(true)
	, bSRGBForceDisable(InSRGBOverride == SRGBO_ForceDisable)
	, MipLevel(InMipLevel)
	, Reserved(0)
	, NumMips(InNumMipLevels)
	, Format((InTexture && InFormat == PF_Unknown) ? InTexture->GetDesc().Format : InFormat)
	, Stride(0)
	, Offset(0)
{
	if (SourceTexture)
	{
		SourceTexture->AddRef();

#if PLATFORM_IOS
		// Memoryless targets can't have texture views (SRVs or UAVs)
		if (SourceTexture->Texture.GetStorageMode() != mtlpp::StorageMode::Memoryless)
#endif
		{
			// Determine the appropriate metal format for the view.
			// This format will be non-sRGB. We convert to sRGB below if required.
			mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[Format].PlatformFormat;

			if (Format == PF_X24_G8)
			{
				// Stencil buffer view of a depth texture
				check(SourceTexture->GetDesc().Format == PF_DepthStencil);
				switch (SourceTexture->Texture.GetPixelFormat())
				{
				default: checkNoEntry(); break;
#if PLATFORM_MAC
				case mtlpp::PixelFormat::Depth24Unorm_Stencil8: MetalFormat = mtlpp::PixelFormat::X24_Stencil8; break;
#endif
				case mtlpp::PixelFormat::Depth32Float_Stencil8: MetalFormat = mtlpp::PixelFormat::X32_Stencil8; break;
				}
			}
			else
			{
				// Override the format's sRGB setting if appropriate
				if (EnumHasAnyFlags(SourceTexture->GetDesc().Flags, TexCreate_SRGB))
				{
					if (bSRGBForceDisable)
					{
#if PLATFORM_MAC
						// R8Unorm has been expanded in the source surface for sRGBA support - we need to expand to RGBA to enable compatible texture format view for non apple silicon macs
						if (Format == PF_G8 && SourceTexture->Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
						{
							MetalFormat = mtlpp::PixelFormat::RGBA8Unorm;
						}
#endif
					}
					else
					{
						// Ensure we have the correct sRGB target format if we create a new texture view rather than using the source texture
						MetalFormat = ToSRGBFormat(MetalFormat);
					}
				}
			}

			// We can use the source texture directly if the view's format / mip count etc matches.
			bool bUseSourceTex =
				MipLevel == 0
				&& NumMips == SourceTexture->Texture.GetMipmapLevelCount()
				&& MetalFormat == SourceTexture->Texture.GetPixelFormat()
				&& !(bInUAV && SourceTexture->GetDesc().IsTextureCube())		// @todo: Remove this once Cube UAV supported for all Metal Devices
				&& InFirstArraySlice == 0
				&& InNumArraySlices == 0;

			if (bUseSourceTex)
			{
				// SRV is exactly compatible with the original texture.
				TextureView = SourceTexture->Texture;
			}
			else
			{
				// Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
				// @todo recreating resources like this will likely prevent us from making view creation multi-threaded.
				// @todo RW: Flag usage creation logic has changed: This should be a check(PixelFormatView) now: SourceTexture should have been created with correct flags
				if (!(SourceTexture->Texture.GetUsage() & mtlpp::TextureUsage::PixelFormatView))
				{
					SourceTexture->PrepareTextureView();
				}

				const uint32 TextureSliceCount = SourceTexture->Texture.GetArrayLength();
				const uint32 CubeSliceMultiplier = SourceTexture->GetDesc().IsTextureCube() ? 6 : 1;
				const uint32 NumArraySlices = (InNumArraySlices > 0 ? InNumArraySlices : TextureSliceCount) * CubeSliceMultiplier;

				// @todo: Remove this type swizzle once Cube UAV supported for all Metal Devices - SRV seem to want to stay as cube but UAV are expected to be 2DArray
				mtlpp::TextureType TextureType = bInUAV && SourceTexture->GetDesc().IsTextureCube() ? mtlpp::TextureType::Texture2DArray : SourceTexture->Texture.GetTextureType();

				// Assume a texture view of 1 slice into a multislice texture wants to be the non-array texture type
				// This doesn't really matter to Metal but will be very important when this texture is bound in the shader
				if (InNumArraySlices == 1)
				{
					switch (TextureType)
					{
					case mtlpp::TextureType::Texture2DArray:
						TextureType = mtlpp::TextureType::Texture2D;
						break;
					case mtlpp::TextureType::TextureCubeArray:
						TextureType = mtlpp::TextureType::TextureCube;
						break;
					default:
						// NOP
						break;
					}
				}

				TextureView = SourceTexture->Texture.NewTextureView(
					MetalFormat,
					TextureType,
					ns::Range(MipLevel, NumMips),
					ns::Range(InFirstArraySlice, NumArraySlices));

#if METAL_DEBUG_OPTIONS
				TextureView.SetLabel([SourceTexture->Texture.GetLabel() stringByAppendingString:@"_TextureView"]);
#endif
			}
		}
	}
	else
	{
		TextureView = {};
	}
}

FMetalResourceViewBase::~FMetalResourceViewBase()
{
	if (TextureView)
	{
		SafeReleaseMetalTexture(TextureView);
		TextureView = nil;
	}
	
	if (bTexture)
	{
		if (SourceTexture)
			SourceTexture->Release();
	}
	else
	{
		if (SourceBuffer)
			SourceBuffer->Release();
	}
}

ns::AutoReleased<FMetalTexture> FMetalResourceViewBase::GetLinearTexture()
{
	ns::AutoReleased<FMetalTexture> NewLinearTexture;
	if (SourceBuffer)
	{
		NewLinearTexture = SourceBuffer->GetLinearTexture((EPixelFormat)Format, LinearTextureDesc.Get());
	}
	return NewLinearTexture;
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, bool bUseUAVCounter, bool bAppendBuffer)
{
	return this->RHICreateUnorderedAccessView(Buffer, bUseUAVCounter, bAppendBuffer);
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);

	// The FMetalResourceViewBase constructor for textures currently modifies the underlying texture object via FMetalSurface::PrepareTextureView() to add PixelFormatView support if it was not already created with it.
	// Because of this, the following RHI thread stall is necessary. We will need to clean this up in future before RHI functions can be completely thread safe.
	if (!(Surface->Texture.GetUsage() & mtlpp::TextureUsage::PixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return this->RHICreateUnorderedAccessView(Texture, MipLevel, FirstArraySlice, NumArraySlices);
	}
	else
	{
		return this->RHICreateUnorderedAccessView(Texture, MipLevel, FirstArraySlice, NumArraySlices);
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint8 Format)
{
	FUnorderedAccessViewRHIRef Result = this->RHICreateUnorderedAccessView(Buffer, Format);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FRHIBuffer* BufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	@autoreleasepool {
		return new FMetalUnorderedAccessView(BufferRHI, bUseUAVCounter, bAppendBuffer);
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	@autoreleasepool {
		return new FMetalUnorderedAccessView(TextureRHI, MipLevel, FirstArraySlice, NumArraySlices);
	}
}

FUnorderedAccessViewRHIRef FMetalDynamicRHI::RHICreateUnorderedAccessView(FRHIBuffer* BufferRHI, uint8 Format)
{
	@autoreleasepool {
		return new FMetalUnorderedAccessView(BufferRHI, (EPixelFormat)Format);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
{
	FShaderResourceViewRHIRef Result = this->RHICreateShaderResourceView(Buffer, Stride, (EPixelFormat)Format);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer)
{
	FShaderResourceViewRHIRef Result = this->RHICreateShaderResourceView(Initializer);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer)
{
	FShaderResourceViewRHIRef Result = this->RHICreateShaderResourceView(Buffer);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
{
	FShaderResourceViewRHIRef Result = this->RHICreateShaderResourceView(Buffer, Stride, Format);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer)
{
	FShaderResourceViewRHIRef Result = this->RHICreateShaderResourceView(Initializer);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer)
{
	FShaderResourceViewRHIRef Result = this->RHICreateShaderResourceView(Buffer);
	if (IsRunningRHIInSeparateThread() && !RHICmdList.Bypass())
	{
		RHICmdList.RHIThreadFence(true);
	}
	return Result;
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture* Texture2DRHI, const FRHITextureSRVCreateInfo& CreateInfo)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture2DRHI);

	// The FMetalResourceViewBase constructor for textures currently modifies the underlying texture object via FMetalSurface::PrepareTextureView() to add PixelFormatView support if it was not already created with it.
	// Because of this, the following RHI thread stall is necessary. We will need to clean this up in future before RHI functions can be completely thread safe.
	if (!(Surface->Texture.GetUsage() & mtlpp::TextureUsage::PixelFormatView))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return this->RHICreateShaderResourceView(Texture2DRHI, CreateInfo);
	}
	else
	{
		return this->RHICreateShaderResourceView(Texture2DRHI, CreateInfo);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FRHITexture* Texture2DRHI, const FRHITextureSRVCreateInfo& CreateInfo)
{
	@autoreleasepool {
		return new FMetalShaderResourceView(Texture2DRHI, CreateInfo);
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FRHIBuffer* BufferRHI)
{
	@autoreleasepool {
		return this->RHICreateShaderResourceView(FShaderResourceViewInitializer(BufferRHI));
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	@autoreleasepool {
		check(GPixelFormats[Format].BlockBytes == Stride);
		return this->RHICreateShaderResourceView(FShaderResourceViewInitializer(BufferRHI, EPixelFormat(Format)));
	}
}

FShaderResourceViewRHIRef FMetalDynamicRHI::RHICreateShaderResourceView(const FShaderResourceViewInitializer& Initializer)
{
	@autoreleasepool {
		return new FMetalShaderResourceView(Initializer);
	}
}

void FMetalDynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRVRHI, FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	check(SRVRHI);
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	check(!SRV->bTexture);

	FMetalResourceMultiBuffer* OldBuffer = SRV->SourceBuffer;

	SRV->SourceBuffer = ResourceCast(BufferRHI);
	SRV->Stride = Stride;
	SRV->Format = Format;

	if (SRV->SourceBuffer)
		SRV->SourceBuffer->AddRef();

	if (OldBuffer)
		OldBuffer->Release();
}

void FMetalDynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRVRHI, FRHIBuffer* BufferRHI)
{
	check(SRVRHI);
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	check(!SRV->bTexture);

	FMetalResourceMultiBuffer* OldBuffer = SRV->SourceBuffer;

	SRV->SourceBuffer = ResourceCast(BufferRHI);
	SRV->Stride = 0;

	SRV->Format = SRV->SourceBuffer && SRV->SourceBuffer->IndexType != mtlpp::IndexType::UInt16
		? PF_R32_UINT
		: PF_R16_UINT;

	if (SRV->SourceBuffer)
		SRV->SourceBuffer->AddRef();

	if (OldBuffer)
		OldBuffer->Release();
}

#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
void FMetalRHICommandContext::ClearUAVWithBlitEncoder(FRHIUnorderedAccessView* UnorderedAccessViewRHI, EMetalRHIClearUAVType Type, uint32 Pattern)
{
	SCOPED_AUTORELEASE_POOL;

	FMetalResourceMultiBuffer* SourceBuffer = ResourceCast(UnorderedAccessViewRHI)->GetSourceBuffer();
	FMetalBuffer Buffer = SourceBuffer->GetCurrentBuffer();
	uint32 Size = SourceBuffer->GetSize();

	check(Type != EMetalRHIClearUAVType::VertexBuffer || EnumHasAnyFlags(SourceBuffer->GetUsage(), BUF_ByteAddressBuffer));

	uint32 AlignedSize = Align(Size, BufferOffsetAlignment);
	FMetalPooledBufferArgs Args(GetMetalDeviceContext().GetDevice(), AlignedSize, BUF_Dynamic, mtlpp::StorageMode::Shared);
	FMetalBuffer Temp = GetMetalDeviceContext().CreatePooledBuffer(Args);
	uint32* ContentBytes = (uint32*)Temp.GetContents();
	for (uint32 Element = 0; Element < (AlignedSize >> 2); ++Element)
	{
		ContentBytes[Element] = Pattern;
	}
	Context->CopyFromBufferToBuffer(Temp, 0, Buffer, 0, Size);
	GetMetalDeviceContext().ReleaseBuffer(Temp);
}
#endif // UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER

void FMetalRHICommandContext::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values)
{
#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
	FMetalUnorderedAccessView* UAV = ResourceCast(UnorderedAccessViewRHI);
	if (!UAV->bTexture && EnumHasAnyFlags(UAV->GetSourceBuffer()->GetUsage(), BUF_StructuredBuffer))
	{
		ClearUAVWithBlitEncoder(UnorderedAccessViewRHI, EMetalRHIClearUAVType::StructuredBuffer, *(uint32*)&Values.X);
	}
	else
#endif // UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
	{
		TRHICommandList_RecursiveHazardous<FMetalRHICommandContext> RHICmdList(this);
		ClearUAV(RHICmdList, ResourceCast(UnorderedAccessViewRHI), &Values, true);
	}
}

void FMetalRHICommandContext::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
{
#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
	FMetalUnorderedAccessView* UAV = ResourceCast(UnorderedAccessViewRHI);
	if (!UAV->bTexture && EnumHasAnyFlags(UAV->GetSourceBuffer()->GetUsage(), BUF_StructuredBuffer))
	{
		ClearUAVWithBlitEncoder(UnorderedAccessViewRHI, EMetalRHIClearUAVType::StructuredBuffer, *(uint32*)&Values.X);
	}
	else
#endif // UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
	{
		TRHICommandList_RecursiveHazardous<FMetalRHICommandContext> RHICmdList(this);
		ClearUAV(RHICmdList, ResourceCast(UnorderedAccessViewRHI), &Values, false);
	}
}

void FMetalRHICommandContext::ClearUAV(TRHICommandList_RecursiveHazardous<FMetalRHICommandContext>& RHICmdList, FMetalUnorderedAccessView* UnorderedAccessView, const void* ClearValue, bool bFloat)
{
	@autoreleasepool {
		EClearReplacementValueType ValueType = bFloat ? EClearReplacementValueType::Float : EClearReplacementValueType::Uint32;

		// The Metal validation layer will complain about resources with a
		// signed format bound against an unsigned data format type as the
		// shader parameter.
		switch (GPixelFormats[UnorderedAccessView->Format].UnrealFormat)
		{
			case PF_R32_SINT:
			case PF_R16_SINT:
			case PF_R16G16B16A16_SINT:
				ValueType = EClearReplacementValueType::Int32;
				break;
				
			default:
				break;
		}

		if (UnorderedAccessView->bTexture)
		{
			FMetalSurface* Texture = UnorderedAccessView->GetSourceTexture();
			FIntVector SizeXYZ = Texture->GetSizeXYZ();

			if (FRHITexture2D* Texture2D = Texture->GetTexture2D())
			{
				ClearUAVShader_T<EClearReplacementResourceType::Texture2D, 4, false>(RHICmdList, UnorderedAccessView, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, ValueType);
			}
			else if (FRHITexture2DArray* Texture2DArray = Texture->GetTexture2DArray())
			{
				ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, UnorderedAccessView, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, ValueType);
			}
			else if (FRHITexture3D* Texture3D = Texture->GetTexture3D())
			{
				ClearUAVShader_T<EClearReplacementResourceType::Texture3D, 4, false>(RHICmdList, UnorderedAccessView, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, ValueType);
			}
			else if (FRHITextureCube* TextureCube = Texture->GetTextureCube())
			{
				ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, UnorderedAccessView, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, ValueType);
			}
			else
			{
				ensure(0);
			}
		}
		else
		{
			FMetalResourceMultiBuffer* SourceBuffer = UnorderedAccessView->GetSourceBuffer();

#if UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
			if (EnumHasAnyFlags(SourceBuffer->GetUsage(), BUF_ByteAddressBuffer))
			{
				ClearUAVWithBlitEncoder(UnorderedAccessView, EMetalRHIClearUAVType::VertexBuffer, *(const uint32*)ClearValue);
			}
			else
#endif // UE_METAL_RHI_SUPPORT_CLEAR_UAV_WITH_BLIT_ENCODER
			{
				uint32 NumElements = SourceBuffer->GetSize() / GPixelFormats[UnorderedAccessView->Format].BlockBytes;
				ClearUAVShader_T<EClearReplacementResourceType::Buffer, 4, false>(RHICmdList, UnorderedAccessView, NumElements, 1, 1, ClearValue, ValueType);
			}
		}
	} // @autoreleasepool
}

void FMetalRHICommandContext::RHIBeginTransitions(TArrayView<const FRHITransition*> Transitions)
{
	for (auto Transition : Transitions)
	{
		Transition->GetPrivateData<FMetalTransitionData>()->BeginResourceTransitions();
	}
}

void FMetalRHICommandContext::RHIEndTransitions(TArrayView<const FRHITransition*> Transitions)
{
	for (auto Transition : Transitions)
	{
		Transition->GetPrivateData<FMetalTransitionData>()->EndResourceTransitions();
	}
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
		FMetalVertexBuffer* SourceBuffer = ResourceCast(SourceBufferRHI);
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
