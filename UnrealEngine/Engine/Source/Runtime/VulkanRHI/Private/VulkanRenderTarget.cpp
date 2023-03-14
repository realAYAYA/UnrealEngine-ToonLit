// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRenderTarget.cpp: Vulkan render target implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanSwapChain.h"
#include "SceneUtils.h"
#include "RHISurfaceDataConversion.h"

// Debug mode used as workaround when a DEVICE LOST occurs on alt+tab on some platforms
// This is a workaround and may end up causing some hitches on the rendering thread
static int32 GVulkanFlushOnMapStaging = 0;
static FAutoConsoleVariableRef CVarGVulkanFlushOnMapStaging(
	TEXT("r.Vulkan.FlushOnMapStaging"),
	GVulkanFlushOnMapStaging,
	TEXT("Flush GPU on MapStagingSurface calls without any fence.\n")
	TEXT(" 0: Do not Flush (default)\n")
	TEXT(" 1: Flush"),
	ECVF_Default
);


static int32 GSubmitOnCopyToResolve = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitOnCopyToResolve(
	TEXT("r.Vulkan.SubmitOnCopyToResolve"),
	GSubmitOnCopyToResolve,
	TEXT("Submits the Queue to the GPU on every RHICopyToResolveTarget call.\n")
	TEXT(" 0: Do not submit (default)\n")
	TEXT(" 1: Submit"),
	ECVF_Default
	);

static int32 GIgnoreCPUReads = 0;
static FAutoConsoleVariableRef CVarVulkanIgnoreCPUReads(
	TEXT("r.Vulkan.IgnoreCPUReads"),
	GIgnoreCPUReads,
	TEXT("Debugging utility for GPU->CPU reads.\n")
	TEXT(" 0 will read from the GPU (default).\n")
	TEXT(" 1 will NOT read from the GPU and fill with zeros.\n"),
	ECVF_Default
	);

static FCriticalSection GStagingMapLock;
static TMap<FVulkanTexture*, VulkanRHI::FStagingBuffer*> GPendingLockedStagingBuffers;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
TAutoConsoleVariable<int32> CVarVulkanDebugBarrier(
	TEXT("r.Vulkan.DebugBarrier"),
	0,
	TEXT("Forces a full barrier for debugging. This is a mask/bitfield (so add up the values)!\n")
	TEXT(" 0: Don't (default)\n")
	TEXT(" 1: Enable heavy barriers after EndRenderPass()\n")
	TEXT(" 2: Enable heavy barriers after every dispatch\n")
	TEXT(" 4: Enable heavy barriers after upload cmd buffers\n")
	TEXT(" 8: Enable heavy barriers after active cmd buffers\n")
	TEXT(" 16: Enable heavy buffer barrier after uploads\n")
	TEXT(" 32: Enable heavy buffer barrier between acquiring back buffer and blitting into swapchain\n"),
	ECVF_Default
);
#endif

FVulkanRenderPass* FVulkanCommandListContext::PrepareRenderPassForPSOCreation(const FGraphicsPipelineStateInitializer& Initializer)
{
	FVulkanRenderTargetLayout RTLayout(Initializer);
	return PrepareRenderPassForPSOCreation(RTLayout);
}

FVulkanRenderPass* FVulkanCommandListContext::PrepareRenderPassForPSOCreation(const FVulkanRenderTargetLayout& RTLayout)
{
	FVulkanRenderPass* RenderPass = nullptr;
	RenderPass = LayoutManager.GetOrCreateRenderPass(*Device, RTLayout);
	return RenderPass;
}

template<typename RegionType>
static void SetupCopyOrResolveRegion(RegionType& Region, const FVulkanTexture& SrcSurface, const FVulkanTexture& DstSurface, const VkImageSubresourceRange& SrcRange, const VkImageSubresourceRange& DstRange, const FResolveParams& ResolveParams)
{
	FMemory::Memzero(Region);

	const FRHITextureDesc& SrcDesc = SrcSurface.GetDesc();
	const FRHITextureDesc& DstDesc = DstSurface.GetDesc();

	bool bCopySrcSubRect = ResolveParams.Rect.IsValid()     && (ResolveParams.Rect.X1 != 0     || ResolveParams.Rect.Y1 != 0     || ResolveParams.Rect.X2 != SrcDesc.Extent.X || ResolveParams.Rect.Y2 != SrcDesc.Extent.Y);
	bool bCopyDstSubRect = ResolveParams.DestRect.IsValid() && (ResolveParams.DestRect.X1 != 0 || ResolveParams.DestRect.Y1 != 0 || ResolveParams.DestRect.X2 != DstDesc.Extent.X || ResolveParams.DestRect.Y2 != DstDesc.Extent.Y);
	if(bCopySrcSubRect || bCopyDstSubRect)
	{
		const FResolveRect& SrcSubRect = ResolveParams.Rect.IsValid()     ? ResolveParams.Rect     : FResolveRect(0, 0, SrcDesc.Extent.X, SrcDesc.Extent.Y);
		const FResolveRect& DstSubRect = ResolveParams.DestRect.IsValid() ? ResolveParams.DestRect : FResolveRect(0, 0, DstDesc.Extent.X, DstDesc.Extent.Y);

		const uint32 SrcSubRectWidth  = SrcSubRect.X2 - SrcSubRect.X1;
		const uint32 SrcSubRectHeight = SrcSubRect.Y2 - SrcSubRect.Y1;

		const uint32 DstSubRectWidth  = DstSubRect.X2 - DstSubRect.X1;
		const uint32 DstSubRectHeight = DstSubRect.Y2 - DstSubRect.Y1;

		ensure(SrcSubRectWidth == DstSubRectWidth && SrcSubRectHeight == DstSubRectHeight);

		Region.srcOffset.x = SrcSubRect.X1;
		Region.srcOffset.y = SrcSubRect.Y1;
		Region.dstOffset.x = DstSubRect.X1;
		Region.dstOffset.y = DstSubRect.Y1;

		Region.extent.width = FMath::Max(1u, SrcSubRectWidth >> ResolveParams.MipIndex);
		Region.extent.height = FMath::Max(1u, SrcSubRectHeight >> ResolveParams.MipIndex);
	}
	else
	{
		ensure(SrcDesc.Extent.X == DstDesc.Extent.X && SrcDesc.Extent.Y == DstDesc.Extent.Y);

		Region.extent.width = FMath::Max(1, SrcDesc.Extent.X >> ResolveParams.MipIndex);
		Region.extent.height = FMath::Max(1, SrcDesc.Extent.Y >> ResolveParams.MipIndex);
	}

	Region.extent.depth = 1;
	Region.srcSubresource.aspectMask = SrcSurface.GetFullAspectMask();
	Region.srcSubresource.baseArrayLayer = SrcRange.baseArrayLayer;
	Region.srcSubresource.layerCount = 1;
	Region.srcSubresource.mipLevel = ResolveParams.MipIndex;
	Region.dstSubresource.aspectMask = DstSurface.GetFullAspectMask();
	Region.dstSubresource.baseArrayLayer = DstRange.baseArrayLayer;
	Region.dstSubresource.layerCount = 1;
	Region.dstSubresource.mipLevel = ResolveParams.MipIndex;
}

void FVulkanCommandListContext::RHICopyToResolveTarget(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FResolveParams& InResolveParams)
{
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (silently ignored)
		return;
	}

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(CmdBuffer->IsOutsideRenderPass());

	const FVulkanTexture& SrcSurface = *FVulkanTexture::Cast(SourceTextureRHI);
	const FVulkanTexture& DstSurface = *FVulkanTexture::Cast(DestTextureRHI);

	const FRHITextureDesc& SrcDesc = SrcSurface.GetDesc();
	const FRHITextureDesc& DstDesc = DstSurface.GetDesc();

	uint32 SrcNumLayers, DstNumLayers, SrcCubeFace = 0, DstCubeFace = 0;

	switch (SourceTextureRHI->GetDesc().Dimension)
	{
	case ETextureDimension::Texture2D:
		SrcNumLayers = 1;
		if (DestTextureRHI->GetDesc().Dimension == ETextureDimension::Texture2D)
		{
			DstNumLayers = 1;
		}
		else
		{
			// Allow copying a 2D texture to a face of a cube texture.
			check(DestTextureRHI->GetDesc().Dimension == ETextureDimension::TextureCube);
			DstNumLayers = 6;
			DstCubeFace = (uint32)InResolveParams.CubeFace;
		}
		break;

	case ETextureDimension::Texture2DArray:
		check(DestTextureRHI->GetDesc().Dimension == ETextureDimension::Texture2DArray);
		SrcNumLayers = SrcDesc.ArraySize;
		DstNumLayers = DstDesc.ArraySize;
		break;

	case ETextureDimension::Texture3D:
		check(DestTextureRHI->GetDesc().Dimension == ETextureDimension::Texture3D);
		SrcNumLayers = DstNumLayers = 1;
		break;

	case ETextureDimension::TextureCube:
		SrcNumLayers = 6;
		SrcCubeFace = DstCubeFace = (uint32)InResolveParams.CubeFace;
		if (DestTextureRHI->GetDesc().Dimension == ETextureDimension::TextureCube)
		{
			DstNumLayers = 6;
		}
		else
		{
			// Allow copying a cube texture to a slice of a cube texture array.
			check(DestTextureRHI->GetDesc().Dimension == ETextureDimension::TextureCubeArray);
			DstNumLayers = 6 * DstDesc.ArraySize;
		}
		break;

	case ETextureDimension::TextureCubeArray:
		check(DestTextureRHI->GetDesc().Dimension == ETextureDimension::TextureCubeArray);
		SrcNumLayers = 6 * SrcDesc.ArraySize;
		DstNumLayers = 6 * DstDesc.ArraySize;
		SrcCubeFace = DstCubeFace = (uint32)InResolveParams.CubeFace;
		break;

	default:
		checkNoEntry();
		return;
	}

	VkImageSubresourceRange SrcRange;
	SrcRange.aspectMask = SrcSurface.GetFullAspectMask();
	SrcRange.baseMipLevel = InResolveParams.MipIndex;
	SrcRange.levelCount = 1;
	SrcRange.baseArrayLayer = InResolveParams.SourceArrayIndex * SrcNumLayers + SrcCubeFace;
	SrcRange.layerCount = 1;

	VkImageSubresourceRange DstRange;
	DstRange.aspectMask = DstSurface.GetFullAspectMask();
	DstRange.baseMipLevel = InResolveParams.MipIndex;
	DstRange.levelCount = 1;
	DstRange.baseArrayLayer = InResolveParams.DestArrayIndex * DstNumLayers + DstCubeFace;
	DstRange.layerCount = 1;

	ERHIAccess SrcCurrentAccess, DstCurrentAccess;

	check(!EnumHasAnyFlags(SrcSurface.GetDesc().Flags, TexCreate_CPUReadback));
	VkImageLayout& SrcLayout = LayoutManager.FindOrAddLayoutRW(SrcSurface, VK_IMAGE_LAYOUT_UNDEFINED);

	if(EnumHasAnyFlags(DstSurface.GetDesc().Flags, TexCreate_CPUReadback))
	{
		//Readback textures are represented as a buffer, so we can support miplevels on hardware that does not expose it.
		FVulkanPipelineBarrier BarrierBefore;
		// We'll transition the entire resources to the correct copy states, so we don't need to worry about sub-resource states.
		if (SrcLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			BarrierBefore.AddImageLayoutTransition(SrcSurface.Image, SrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FVulkanPipelineBarrier::MakeSubresourceRange(SrcSurface.GetFullAspectMask()));
			SrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
		BarrierBefore.Execute(CmdBuffer->GetHandle());
		const FVulkanCpuReadbackBuffer* CpuReadbackBuffer = DstSurface.GetCpuReadbackBuffer();
		check(DstRange.baseArrayLayer == 0);
		VkBufferImageCopy CopyRegion;
		FMemory::Memzero(CopyRegion);
		uint32 MipLevel = InResolveParams.MipIndex;
		uint32 SizeX = FMath::Max(SrcDesc.Extent.X >> MipLevel, 1);
		uint32 SizeY = FMath::Max(SrcDesc.Extent.Y >> MipLevel, 1);
		CopyRegion.bufferOffset = CpuReadbackBuffer->MipOffsets[MipLevel];
		CopyRegion.bufferRowLength = SizeX;
		CopyRegion.bufferImageHeight = SizeY;
		CopyRegion.imageSubresource.aspectMask = SrcSurface.GetFullAspectMask();
		CopyRegion.imageSubresource.mipLevel = MipLevel;
		CopyRegion.imageSubresource.baseArrayLayer = 0;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageExtent.width = SizeX;
		CopyRegion.imageExtent.height = SizeY;
		CopyRegion.imageExtent.depth = 1;
		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CpuReadbackBuffer->Buffer, 1, &CopyRegion);

		{
			FVulkanPipelineBarrier BarrierAfter;
			BarrierAfter.AddMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT);

			SrcCurrentAccess = ERHIAccess::CopySrc;
			if(SrcCurrentAccess != InResolveParams.SourceAccessFinal && InResolveParams.SourceAccessFinal != ERHIAccess::Unknown)
			{
				BarrierAfter.AddImageAccessTransition(SrcSurface, SrcCurrentAccess, InResolveParams.SourceAccessFinal, FVulkanPipelineBarrier::MakeSubresourceRange(SrcSurface.GetFullAspectMask()), SrcLayout);
			}

			BarrierAfter.Execute(CmdBuffer->GetHandle());
		}
	}
	else
	{
		VkImageLayout& DstLayout = LayoutManager.FindOrAddLayoutRW(DstSurface, VK_IMAGE_LAYOUT_UNDEFINED);
		if (SrcSurface.Image != DstSurface.Image)
		{
			const bool bIsResolve = SrcSurface.GetNumSamples() > DstSurface.GetNumSamples();
			checkf(!bIsResolve || !DstSurface.IsDepthOrStencilAspect(), TEXT("Vulkan does not support multisample depth resolve."));

			FVulkanPipelineBarrier BarrierBefore;
		
			// We'll transition the entire resources to the correct copy states, so we don't need to worry about sub-resource states.
			if (SrcLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				BarrierBefore.AddImageLayoutTransition(SrcSurface.Image, SrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FVulkanPipelineBarrier::MakeSubresourceRange(SrcSurface.GetFullAspectMask()));
				SrcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}

			if (DstLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				BarrierBefore.AddImageLayoutTransition(DstSurface.Image, DstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FVulkanPipelineBarrier::MakeSubresourceRange(DstSurface.GetFullAspectMask()));
				DstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}

			BarrierBefore.Execute(CmdBuffer->GetHandle());

			if (!bIsResolve)
			{
				VkImageCopy Region;
				SetupCopyOrResolveRegion(Region, SrcSurface, DstSurface, SrcRange, DstRange, InResolveParams);
				VulkanRHI::vkCmdCopyImage(CmdBuffer->GetHandle(),
					SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &Region);
			}
			else
			{
				check(DstSurface.GetNumSamples() == 1);
				VkImageResolve Region;
				SetupCopyOrResolveRegion(Region, SrcSurface, DstSurface, SrcRange, DstRange, InResolveParams);
				VulkanRHI::vkCmdResolveImage(CmdBuffer->GetHandle(),
					SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &Region);
			}

			SrcCurrentAccess = ERHIAccess::CopySrc;
			DstCurrentAccess = ERHIAccess::CopyDest;
		}
		else
		{
			SrcCurrentAccess = ERHIAccess::Unknown;
			DstCurrentAccess = ERHIAccess::Unknown;
		}

		if (InResolveParams.SourceAccessFinal != ERHIAccess::Unknown && InResolveParams.DestAccessFinal != ERHIAccess::Unknown)
		{
			FVulkanPipelineBarrier BarrierAfter;
			if (SrcSurface.Image != DstSurface.Image && SrcCurrentAccess != InResolveParams.SourceAccessFinal && InResolveParams.SourceAccessFinal != ERHIAccess::Unknown)
			{
				BarrierAfter.AddImageAccessTransition(SrcSurface, SrcCurrentAccess, InResolveParams.SourceAccessFinal, FVulkanPipelineBarrier::MakeSubresourceRange(SrcSurface.GetFullAspectMask()), SrcLayout);
			}

			if (DstCurrentAccess != InResolveParams.DestAccessFinal && InResolveParams.DestAccessFinal != ERHIAccess::Unknown)
			{
				BarrierAfter.AddImageAccessTransition(DstSurface, DstCurrentAccess, InResolveParams.DestAccessFinal, FVulkanPipelineBarrier::MakeSubresourceRange(DstSurface.GetFullAspectMask()), DstLayout);
			}

			BarrierAfter.Execute(CmdBuffer->GetHandle());
		}
	}

	if (GSubmitOnCopyToResolve)
	{
		InternalSubmitActiveCmdBuffer();
	}
}


static void ConvertRawDataToFColor(VkFormat VulkanFormat, uint32 DestWidth, uint32 DestHeight, uint8* In, uint32 SrcPitch, FColor* Dest, const FReadSurfaceDataFlags& InFlags)
{
	const bool bLinearToGamma = InFlags.GetLinearToGamma();
	switch (VulkanFormat)
	{
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		ConvertRawR32G32B32A32DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest, bLinearToGamma);
		break;

	case VK_FORMAT_R16G16B16A16_SFLOAT:
		ConvertRawR16G16B16A16FDataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest, bLinearToGamma);
		break;

	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		ConvertRawR10G10B10A2DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	case VK_FORMAT_R8G8B8A8_UNORM:
		ConvertRawR8G8B8A8DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	case VK_FORMAT_R16G16B16A16_UNORM:
		ConvertRawR16G16B16A16DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	case VK_FORMAT_B8G8R8A8_UNORM:
		ConvertRawB8G8R8A8DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	case VK_FORMAT_R8_UNORM:
		ConvertRawR8DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	case VK_FORMAT_R16_UNORM:
		ConvertRawR16DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	case VK_FORMAT_R16G16_UNORM:
		ConvertRawR16G16DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
		break;

	default:
		checkf(false, TEXT("Unsupported format [%d] for conversion to FColor!"), (uint32)VulkanFormat);
		break;
	}

}

void FVulkanDynamicRHI::RHIReadSurfaceData(FRHITexture* TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	const uint32 DestWidth = Rect.Max.X - Rect.Min.X;
	const uint32 DestHeight = Rect.Max.Y - Rect.Min.Y;
	const uint32 NumRequestedPixels = DestWidth * DestHeight;
	if (GIgnoreCPUReads)
	{
		// Debug: Fill with CPU
		OutData.Empty(0);
		OutData.AddZeroed(NumRequestedPixels);
		return;
	}

	const FRHITextureDesc& Desc = TextureRHI->GetDesc();
	switch (Desc.Dimension)
	{
	case ETextureDimension::Texture2D:
	case ETextureDimension::Texture2DArray:
		// In VR, the high level code calls this function on the viewport render target, without knowing that it's
		// actually a texture array created and managed by the VR runtime. In that case we'll just read the first
		// slice of the array, which corresponds to one of the eyes.
		break;

	default:
		// Just return black for texture types we don't support.
		OutData.Empty(0);
		OutData.AddZeroed(NumRequestedPixels);
		return;
	}

	FVulkanTexture& Surface = *FVulkanTexture::Cast(TextureRHI);

	Device->PrepareForCPURead();

	// Figure out the size of the buffer required to hold the requested pixels
	const uint32 PixelByteSize = GetNumBitsPerPixel(Surface.StorageFormat) / 8;
	checkf(GPixelFormats[TextureRHI->GetFormat()].Supported && (PixelByteSize > 0), TEXT("Trying to read from unsupported format."));
	const uint32 BufferSize = NumRequestedPixels * PixelByteSize;

	// Validate that the Rect is within the texture
	const uint32 MipLevel = InFlags.GetMip();
	const uint32 MipSizeX = FMath::Max(Desc.Extent.X >> MipLevel, 1);
	const uint32 MipSizeY = FMath::Max(Desc.Extent.Y >> MipLevel, 1);
	checkf((Rect.Max.X <= (int32)MipSizeX) && (Rect.Max.Y <= (int32)MipSizeY), TEXT("The specified Rect [%dx%d] extends beyond this Mip [%dx%d]."), Rect.Max.X, Rect.Max.Y, MipSizeX, MipSizeY);

	FVulkanCommandListContext& ImmediateContext = Device->GetImmediateContext();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	FVulkanCmdBuffer* CmdBuffer = nullptr;
	const bool bCPUReadback = EnumHasAllFlags(Surface.GetDesc().Flags, TexCreate_CPUReadback);
	if (!bCPUReadback) //this function supports reading back arbitrary rendertargets, so if its not a cpu readback surface, we do a copy.
	{
		CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();
		StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		VkBufferImageCopy CopyRegion;
		FMemory::Memzero(CopyRegion);
		// Leave bufferRowLength/bufferImageHeight at 0 for tightly packed
		CopyRegion.imageSubresource.aspectMask = Surface.GetFullAspectMask();
		CopyRegion.imageSubresource.mipLevel = MipLevel;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageOffset.x = Rect.Min.X;
		CopyRegion.imageOffset.y = Rect.Min.Y;
		CopyRegion.imageExtent.width = DestWidth;
		CopyRegion.imageExtent.height = DestHeight;
		CopyRegion.imageExtent.depth = 1;

		VkImageLayout& CurrentLayout = Device->GetImmediateContext().GetLayoutManager().FindOrAddLayoutRW(Surface, VK_IMAGE_LAYOUT_UNDEFINED);
		const bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
		if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			VulkanSetImageLayoutAllMips(CmdBuffer->GetHandle(), Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);
		if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			VulkanSetImageLayoutAllMips(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout);
		}
		else
		{
			CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
		ensure(StagingBuffer->GetSize() >= BufferSize);

		VkMemoryBarrier Barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER , nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &Barrier, 0, nullptr, 0, nullptr);

		// Force upload
		ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer();
	}

	Device->WaitUntilIdle();

	uint8* In;
	uint32 SrcPitch;
	if (bCPUReadback)
	{
		// If the text was bCPUReadback, then we have to deal with our Rect potentially being a subset of the total texture
		In = (uint8*)Surface.GetMappedPointer() + ((Rect.Min.Y * MipSizeX + Rect.Min.X) * PixelByteSize);
		SrcPitch = MipSizeX * PixelByteSize;
	}
	else
	{
		// If the text was NOT bCPUReadback, the buffer contains only the (tightly packed) Rect we requested
		StagingBuffer->InvalidateMappedMemory();
		In = (uint8*)StagingBuffer->GetMappedPointer();
		SrcPitch = DestWidth * PixelByteSize;
	}

	OutData.SetNum(NumRequestedPixels);
	FColor* Dest = OutData.GetData();
	ConvertRawDataToFColor(Surface.StorageFormat, DestWidth, DestHeight, In, SrcPitch, Dest, InFlags);

	if (!bCPUReadback)
	{
		Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
	}

	ImmediateContext.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
}

void FVulkanDynamicRHI::RHIReadSurfaceData(FRHITexture* TextureRHI, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	TArray<FColor> FromColorData;
	RHIReadSurfaceData(TextureRHI, Rect, FromColorData, InFlags);
	for (FColor& From : FromColorData)
	{
		OutData.Emplace(FLinearColor(From));
	}
}

void FVulkanDynamicRHI::RHIMapStagingSurface(FRHITexture* TextureRHI, FRHIGPUFence* FenceRHI, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex)
{
	check(TextureRHI->GetDesc().Dimension == ETextureDimension::Texture2D);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);

	if (FenceRHI && !FenceRHI->Poll())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		Device->SubmitCommandsAndFlushGPU();

		// SubmitCommandsAndFlushGPU might update fence state if it was tied to a previously submitted command buffer.
		// Its state will have been updated from Submitted to NeedReset, and would assert in WaitForCmdBuffer (which is not needed in such a case)
		if (!FenceRHI->Poll())
		{
			FVulkanGPUFence* Fence = ResourceCast(FenceRHI);
			Device->GetImmediateContext().GetCommandBufferManager()->WaitForCmdBuffer(Fence->GetCmdBuffer());
		}
	}
	else
	{
		if(GVulkanFlushOnMapStaging)
		{
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			Device->WaitUntilIdle();
		}
	}


	check(EnumHasAllFlags(Texture->GetDesc().Flags, TexCreate_CPUReadback));
	OutData = Texture->GetMappedPointer();
	Texture->InvalidateMappedMemory();
	OutWidth = Texture->GetSizeX();
	OutHeight = Texture->GetSizeY();
}

void FVulkanDynamicRHI::RHIUnmapStagingSurface(FRHITexture* TextureRHI, uint32 GPUIndex)
{
}

void FVulkanDynamicRHI::RHIReadSurfaceFloatData(FRHITexture* TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex)
{
	auto DoCopyFloat = [](FVulkanDevice* InDevice, FVulkanCmdBuffer* InCmdBuffer, const FVulkanTexture& Surface, uint32 InMipIndex, uint32 SrcBaseArrayLayer, FIntRect InRect, TArray<FFloat16Color>& OutputData)
	{
		ensure(Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);

		const FRHITextureDesc& Desc = Surface.GetDesc();

		uint32 NumPixels = (Desc.Extent.X >> InMipIndex) * (Desc.Extent.Y >> InMipIndex);
		const uint32 Size = NumPixels * sizeof(FFloat16Color);
		VulkanRHI::FStagingBuffer* StagingBuffer = InDevice->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		if (GIgnoreCPUReads == 0)
		{
			VkBufferImageCopy CopyRegion;
			FMemory::Memzero(CopyRegion);
			//Region.bufferOffset = 0;
			CopyRegion.bufferRowLength = Desc.Extent.X >> InMipIndex;
			CopyRegion.bufferImageHeight = Desc.Extent.Y >> InMipIndex;
			CopyRegion.imageSubresource.aspectMask = Surface.GetFullAspectMask();
			CopyRegion.imageSubresource.mipLevel = InMipIndex;
			CopyRegion.imageSubresource.baseArrayLayer = SrcBaseArrayLayer;
			CopyRegion.imageSubresource.layerCount = 1;
			CopyRegion.imageExtent.width = Desc.Extent.X >> InMipIndex;
			CopyRegion.imageExtent.height = Desc.Extent.Y >> InMipIndex;
			CopyRegion.imageExtent.depth = 1;

			//#todo-rco: Multithreaded!
			VkImageLayout& CurrentLayout = InDevice->GetImmediateContext().GetLayoutManager().FindOrAddLayoutRW(Surface, VK_IMAGE_LAYOUT_UNDEFINED);
			bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			}

			VulkanRHI::vkCmdCopyImageToBuffer(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

			if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout);
			}
			else
			{
				CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
		}
		else
		{
			VulkanRHI::vkCmdFillBuffer(InCmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
		}

		// the staging buffer size may be bigger then the size due to alignment, etc. but it must not be smaller!
		ensure(StagingBuffer->GetSize() >= Size);

		VkMemoryBarrier Barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER , nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
		VulkanRHI::vkCmdPipelineBarrier(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &Barrier, 0, nullptr, 0, nullptr);

		// Force upload
		InDevice->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();
		InDevice->WaitUntilIdle();

		StagingBuffer->InvalidateMappedMemory();

		uint32 OutWidth = InRect.Max.X - InRect.Min.X;
		uint32 OutHeight= InRect.Max.Y - InRect.Min.Y;
		OutputData.SetNum(OutWidth * OutHeight);
		uint32 OutIndex = 0;
		FFloat16Color* Dest = OutputData.GetData();
		for (int32 Row = InRect.Min.Y; Row < InRect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Row * (Desc.Extent.X >> InMipIndex) + InRect.Min.X;
			for (int32 Col = InRect.Min.X; Col < InRect.Max.X; ++Col)
			{
				OutputData[OutIndex++] = *Src++;
			}
		}
		InDevice->GetStagingManager().ReleaseBuffer(InCmdBuffer, StagingBuffer);
	};

	FVulkanTexture& Surface = *FVulkanTexture::Cast(TextureRHI);
	const FRHITextureDesc& Desc = Surface.GetDesc();

	if (GIgnoreCPUReads == 1)
	{
		// Debug: Fill with CPU
		uint32 NumPixels = 0;
		switch(Desc.Dimension)
		{
		case ETextureDimension::TextureCubeArray:
		case ETextureDimension::TextureCube:
			NumPixels = (Desc.Extent.X >> MipIndex) * (Desc.Extent.Y >> MipIndex);
			break;

		case ETextureDimension::Texture2DArray:
		case ETextureDimension::Texture2D:
			NumPixels = (Desc.Extent.X >> MipIndex) * (Desc.Extent.Y >> MipIndex);
			break;

		default:
			checkNoEntry();
			break;
		}

		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
	}
	else
	{
		Device->PrepareForCPURead();

		FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
		switch (TextureRHI->GetDesc().Dimension)
		{
			case ETextureDimension::TextureCubeArray:
			case ETextureDimension::TextureCube:
				DoCopyFloat(Device, CmdBuffer, Surface, MipIndex, CubeFace + 6 * ArrayIndex, Rect, OutData);
				break;

			case ETextureDimension::Texture2DArray:
			case ETextureDimension::Texture2D:
				DoCopyFloat(Device, CmdBuffer, Surface, MipIndex, ArrayIndex, Rect, OutData);
				break;

			default:
				checkNoEntry();
				break;
		}
		Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
	}
}

void FVulkanDynamicRHI::RHIRead3DSurfaceFloatData(FRHITexture* TextureRHI, FIntRect InRect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData)
{
	FVulkanTexture& Surface = *FVulkanTexture::Cast(TextureRHI);
	const FRHITextureDesc& Desc = Surface.GetDesc();

	uint32 SizeX = InRect.Width();
	uint32 SizeY = InRect.Height();
	uint32 SizeZ = ZMinMax.Y - ZMinMax.X;
	uint32 NumPixels = SizeX * SizeY * SizeZ;
	const uint32 Size = NumPixels * sizeof(FFloat16Color);

	// Allocate the output buffer.
	OutData.Reserve(Size);

	if (GIgnoreCPUReads == 1)
	{
		// Debug: Fill with CPU
		OutData.AddZeroed(Size);
		return;
	}

	Device->PrepareForCPURead();
	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();

	ensure(Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);

	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	if (GIgnoreCPUReads == 0)
	{
		VkBufferImageCopy CopyRegion;
		FMemory::Memzero(CopyRegion);
		//Region.bufferOffset = 0;
		CopyRegion.bufferRowLength = Desc.Extent.X;
		CopyRegion.bufferImageHeight = Desc.Extent.Y;
		CopyRegion.imageSubresource.aspectMask = Surface.GetFullAspectMask();
		//CopyRegion.imageSubresource.mipLevel = 0;
		//CopyRegion.imageSubresource.baseArrayLayer = 0;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageOffset.x = InRect.Min.X;
		CopyRegion.imageOffset.y = InRect.Min.Y;
		CopyRegion.imageOffset.z = ZMinMax.X;
		CopyRegion.imageExtent.width = SizeX;
		CopyRegion.imageExtent.height = SizeY;
		CopyRegion.imageExtent.depth = SizeZ;

		//#todo-rco: Multithreaded!
		VkImageLayout& CurrentLayout = Device->GetImmediateContext().GetLayoutManager().FindOrAddLayoutRW(Surface, VK_IMAGE_LAYOUT_UNDEFINED);
		bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
		if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

		if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout);
		}
		else
		{
			CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
	}
	else
	{
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
	}

	// the staging buffer size may be bigger then the size due to alignment, etc. but it must not be smaller!
	ensure(StagingBuffer->GetSize() >= Size);

	VkMemoryBarrier Barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER , nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &Barrier, 0, nullptr, 0, nullptr);

	// Force upload
	Device->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();
	Device->WaitUntilIdle();

	StagingBuffer->InvalidateMappedMemory();

	OutData.SetNum(NumPixels);
	FFloat16Color* Dest = OutData.GetData();
	for (int32 Layer = ZMinMax.X; Layer < ZMinMax.Y; ++Layer)
	{
		for (int32 Row = InRect.Min.Y; Row < InRect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Layer * SizeX * SizeY + Row * Desc.Extent.X + InRect.Min.X;
			for (int32 Col = InRect.Min.X; Col < InRect.Max.X; ++Col)
			{
				*Dest++ = *Src++;
			}
		}
	}
	FFloat16Color* End = OutData.GetData() + OutData.Num();
	checkf(Dest <= End, TEXT("Memory overwrite! Calculated total size %d: SizeX %d SizeY %d SizeZ %d; InRect(%d, %d, %d, %d) InZ(%d, %d)"),
		Size, SizeX, SizeY, SizeZ, InRect.Min.X, InRect.Min.Y, InRect.Max.X, InRect.Max.Y, ZMinMax.X, ZMinMax.Y);
	Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
	Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
}

VkSurfaceTransformFlagBitsKHR FVulkanCommandListContext::GetSwapchainQCOMRenderPassTransform() const
{
	TArray<FVulkanViewport*>& viewports = RHI->GetViewports();
	if (viewports.Num() == 0)
	{
		return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}

	// check(viewports.Num() == 1);
	return viewports[0]->GetSwapchainQCOMRenderPassTransform();
}

VkFormat FVulkanCommandListContext::GetSwapchainImageFormat() const
{
	TArray<FVulkanViewport*>& viewports = RHI->GetViewports();
	if (viewports.Num() == 0)
	{
		return VK_FORMAT_UNDEFINED;
	}

	return viewports[0]->GetSwapchainImageFormat();
}

FVulkanSwapChain* FVulkanCommandListContext::GetSwapChain() const
{
	TArray<FVulkanViewport*>& viewports = RHI->GetViewports();
	uint32 numViewports = viewports.Num();

	if (viewports.Num() == 0)
	{
		return nullptr;
	}

	return viewports[0]->GetSwapChain();
}

bool FVulkanCommandListContext::IsSwapchainImage(FRHITexture* InTexture) const
{
	TArray<FVulkanViewport*>& Viewports = RHI->GetViewports();
	uint32 NumViewports = Viewports.Num();

	for (uint32 i = 0; i < NumViewports; i++)
	{
		VkImage Image = FVulkanTexture::Cast(InTexture)->Image;
		uint32 BackBufferImageCount = Viewports[i]->GetBackBufferImageCount();

		for (uint32 SwapchainImageIdx = 0; SwapchainImageIdx < BackBufferImageCount; SwapchainImageIdx++)
		{
			if (Image == Viewports[i]->GetBackBufferImage(SwapchainImageIdx))
			{
				return true;
			}
		}
	}
	return false;
}

void FVulkanCommandListContext::RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	if (GVulkanSubmitAfterEveryEndRenderPass)
	{
		CommandBufferManager->SubmitActiveCmdBuffer();
		CommandBufferManager->PrepareForNewActiveCommandBuffer();
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}
	else if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}

	RenderPassInfo = InInfo;
	RHIPushEvent(InName ? InName : TEXT("<unnamed RenderPass>"), FColor::Green);
	if (InInfo.NumOcclusionQueries > 0)
	{
		BeginOcclusionQueryBatch(CmdBuffer, InInfo.NumOcclusionQueries);
	}

	FRHITexture* DSTexture = InInfo.DepthStencilRenderTarget.DepthStencilTarget;
	VkImageLayout CurrentDSLayout;
	if (DSTexture)
	{
		FVulkanTexture& Surface = *FVulkanTexture::Cast(DSTexture);
		CurrentDSLayout = LayoutManager.FindLayoutChecked(Surface.Image);
	}
	else
	{
		CurrentDSLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	FVulkanRenderTargetLayout RTLayout(*Device, InInfo, CurrentDSLayout);
	check(RTLayout.GetExtent2D().width != 0 && RTLayout.GetExtent2D().height != 0);

	FVulkanRenderPass* RenderPass = LayoutManager.GetOrCreateRenderPass(*Device, RTLayout);
	FRHISetRenderTargetsInfo RTInfo;
	InInfo.ConvertToRenderTargetsInfo(RTInfo);

	FVulkanFramebuffer* Framebuffer = LayoutManager.GetOrCreateFramebuffer(*Device, RTInfo, RTLayout, RenderPass);
	checkf(RenderPass != nullptr && Framebuffer != nullptr, TEXT("RenderPass not started! Bad combination of values? Depth %p #Color %d Color0 %p"), (void*)InInfo.DepthStencilRenderTarget.DepthStencilTarget, InInfo.GetNumColorRenderTargets(), (void*)InInfo.ColorRenderTargets[0].RenderTarget);
	LayoutManager.BeginRenderPass(*this, *Device, CmdBuffer, InInfo, RTLayout, RenderPass, Framebuffer);
}

void FVulkanCommandListContext::RHIEndRenderPass()
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (RenderPassInfo.NumOcclusionQueries > 0)
	{
		EndOcclusionQueryBatch(CmdBuffer);
	}
	else
	{
		LayoutManager.EndRenderPass(CmdBuffer);
	}

	RHIPopEvent();
}

void FVulkanCommandListContext::RHINextSubpass()
{
	check(LayoutManager.CurrentRenderPass);
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer Cmd = CmdBuffer->GetHandle();
	VulkanRHI::vkCmdNextSubpass(Cmd, VK_SUBPASS_CONTENTS_INLINE);
}

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct FRenderPassCompatibleHashableStruct
{
	FRenderPassCompatibleHashableStruct()
	{
		FMemory::Memzero(*this);
	}

	uint8							NumAttachments;
	uint8							MultiViewCount;
	uint8							NumSamples;
	uint8							SubpassHint;
	VkSurfaceTransformFlagBitsKHR	QCOMRenderPassTransform;
	// +1 for DepthStencil, +1 for Fragment Density
	VkFormat						Formats[MaxSimultaneousRenderTargets + 2];
	uint16							AttachmentsToResolve;
};

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct FRenderPassFullHashableStruct
{
	FRenderPassFullHashableStruct()
	{
		FMemory::Memzero(*this);
	}

	// +1 for Depth, +1 for Stencil, +1 for Fragment Density
	TEnumAsByte<VkAttachmentLoadOp>		LoadOps[MaxSimultaneousRenderTargets + 3];
	TEnumAsByte<VkAttachmentStoreOp>	StoreOps[MaxSimultaneousRenderTargets + 3];
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
	// If the initial != final we need to add FinalLayout and potentially RefLayout
	VkImageLayout						InitialLayout[MaxSimultaneousRenderTargets + 2];
	//VkImageLayout						FinalLayout[MaxSimultaneousRenderTargets + 2];
	//VkImageLayout						RefLayout[MaxSimultaneousRenderTargets + 2];
#endif
};

VkImageLayout FVulkanRenderTargetLayout::GetVRSImageLayout() const
{
	if (ValidateShadingRateDataType())
	{
#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		}
#endif
#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
		if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}
#endif
	}

	return VK_IMAGE_LAYOUT_UNDEFINED;
}

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RTInfo)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, MultiViewCount(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(FragmentDensityReference);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(InputAttachments);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
	{
		const FRHIRenderTargetView& RTView = RTInfo.ColorRenderTarget[Index];
		if (RTView.Texture)
		{
			FVulkanTexture* Texture = FVulkanTexture::Cast(RTView.Texture);
			check(Texture);
			const FRHITextureDesc& TextureDesc = Texture->GetDesc();

			if (InDevice.GetImmediateContext().IsSwapchainImage(RTView.Texture))
			{
				QCOMRenderPassTransform = InDevice.GetImmediateContext().GetSwapchainQCOMRenderPassTransform();
			}

			if (bSetExtent)
			{
				ensure(Extent.Extent3D.width == FMath::Max(1, TextureDesc.Extent.X >> RTView.MipIndex));
				ensure(Extent.Extent3D.height == FMath::Max(1, TextureDesc.Extent.Y >> RTView.MipIndex));
				ensure(Extent.Extent3D.depth == TextureDesc.Depth);
			}
			else
			{
				bSetExtent = true;
				Extent.Extent3D.width = FMath::Max(1, TextureDesc.Extent.X >> RTView.MipIndex);
				Extent.Extent3D.height = FMath::Max(1, TextureDesc.Extent.Y >> RTView.MipIndex);
				Extent.Extent3D.depth = TextureDesc.Depth;
			}

			ensure(!NumSamples || NumSamples == Texture->GetNumSamples());
			NumSamples = Texture->GetNumSamples();
		
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkTextureFormat(RTView.Texture->GetFormat(), EnumHasAllFlags(TextureDesc.Flags, TexCreate_SRGB));
			CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTView.LoadAction);
			bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			// Removed this temporarily as we need a way to determine if the target is actually memoryless
			/*if (EnumHasAllFlags(Texture->UEFlags, TexCreate_Memoryless))
			{
				ensure(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			}*/

			// If the initial != final we need to change the FullHashInfo and use FinalLayout
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			const bool bHasValidResolveAttachment = RTInfo.bHasResolveAttachments && RTInfo.ColorResolveRenderTarget[Index].Texture;
			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT && bHasValidResolveAttachment)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				CompatibleHashInfo.AttachmentsToResolve |= (uint16)(1 << NumColorAttachments);
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
			FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
			FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
			FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
#endif
			++CompatibleHashInfo.NumAttachments;

			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	VkImageLayout DepthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (RTInfo.DepthStencilRenderTarget.Texture)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTexture* Texture = FVulkanTexture::Cast(RTInfo.DepthStencilRenderTarget.Texture);
		check(Texture);
		const FRHITextureDesc& TextureDesc = Texture->GetDesc();

		ensure(!NumSamples || NumSamples == Texture->GetNumSamples());
		NumSamples = TextureDesc.NumSamples;

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkTextureFormat(RTInfo.DepthStencilRenderTarget.Texture->GetFormat(), false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.StencilLoadAction);
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.GetStencilStoreAction());

			// Removed this temporarily as we need a way to determine if the target is actually memoryless
			/*if (EnumHasAllFlags(Texture->UEFlags, TexCreate_Memoryless))
			{
				ensure(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
				ensure(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			}*/
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}

		DepthStencilLayout = VulkanRHI::GetDepthStencilLayout(RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess(), InDevice);

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = DepthStencilLayout;
		CurrDesc.finalLayout = DepthStencilLayout;

		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = DepthStencilLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = DepthStencilLayout;
#endif
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;

		bHasDepthStencil = true;

		if (bSetExtent)
		{
			// Depth can be greater or equal to color. Clamp to the smaller size.
			Extent.Extent3D.width = FMath::Min<uint32>(Extent.Extent3D.width, TextureDesc.Extent.X);
			Extent.Extent3D.height = FMath::Min<uint32>(Extent.Extent3D.height, TextureDesc.Extent.Y);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = TextureDesc.Extent.X;
			Extent.Extent3D.height = TextureDesc.Extent.Y;
			Extent.Extent3D.depth = Texture->GetNumberOfArrayLevels();
		}
	}

	if (GRHISupportsAttachmentVariableRateShading && GRHIVariableRateShadingEnabled && GRHIAttachmentVariableRateShadingEnabled && RTInfo.ShadingRateTexture)
	{
		FVulkanTexture* Texture = FVulkanTexture::Cast(RTInfo.ShadingRateTexture);
		check(Texture->GetFormat() == GRHIVariableRateShadingImageFormat);

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		const VkImageLayout VRSLayout = GetVRSImageLayout();
		
		CurrDesc.flags = 0;
		CurrDesc.format = UEToVkTextureFormat(RTInfo.ShadingRateTexture->GetFormat(), false);
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RTInfo.ShadingRateTexture->GetNumSamples());
		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = VRSLayout;
		CurrDesc.finalLayout = VRSLayout;

		FragmentDensityReference.attachment = NumAttachmentDescriptions;
		FragmentDensityReference.layout = VRSLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = VRSLayout;
#endif
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets + 1] = CurrDesc.format;

		++NumAttachmentDescriptions;
		bHasFragmentDensityAttachment = true;
	}

	SubpassHint = ESubpassHint::None;
	CompatibleHashInfo.SubpassHint = 0;

	CompatibleHashInfo.QCOMRenderPassTransform = QCOMRenderPassTransform;

	CompatibleHashInfo.NumSamples = NumSamples;
	CompatibleHashInfo.MultiViewCount = MultiViewCount;

	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}


FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(FVulkanDevice& InDevice, const FRHIRenderPassInfo& RPInfo, VkImageLayout CurrentDSLayout)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, MultiViewCount(RPInfo.MultiViewCount)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(FragmentDensityReference);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(InputAttachments);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Offset);
	FMemory::Memzero(Extent);

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	bool bMultiviewRenderTargets = false;

	int32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
	for (int32 Index = 0; Index < NumColorRenderTargets; ++Index)
	{
		const FRHIRenderPassInfo::FColorEntry& ColorEntry = RPInfo.ColorRenderTargets[Index];
		FVulkanTexture* Texture = FVulkanTexture::Cast(ColorEntry.RenderTarget);
		check(Texture);
		const FRHITextureDesc& TextureDesc = Texture->GetDesc();

		if (InDevice.GetImmediateContext().IsSwapchainImage(ColorEntry.RenderTarget))
		{
			QCOMRenderPassTransform = InDevice.GetImmediateContext().GetSwapchainQCOMRenderPassTransform();
		}
		check(QCOMRenderPassTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR || NumAttachmentDescriptions == 0);

		if (bSetExtent)
		{
			ensure(Extent.Extent3D.width == FMath::Max(1, TextureDesc.Extent.X >> ColorEntry.MipIndex));
			ensure(Extent.Extent3D.height == FMath::Max(1, TextureDesc.Extent.Y >> ColorEntry.MipIndex));
			ensure(Extent.Extent3D.depth == TextureDesc.Depth);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = FMath::Max(1, TextureDesc.Extent.X >> ColorEntry.MipIndex);
			Extent.Extent3D.height = FMath::Max(1, TextureDesc.Extent.Y >> ColorEntry.MipIndex);
			Extent.Extent3D.depth = TextureDesc.Depth;
		}

		ensure(!NumSamples || NumSamples == ColorEntry.RenderTarget->GetNumSamples());
		NumSamples = ColorEntry.RenderTarget->GetNumSamples();

		ensure(!GetIsMultiView() || !bMultiviewRenderTargets || Texture->GetNumberOfArrayLevels() > 1);
		bMultiviewRenderTargets = Texture->GetNumberOfArrayLevels() > 1;

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkTextureFormat(ColorEntry.RenderTarget->GetFormat(), EnumHasAllFlags(Texture->GetDesc().Flags, TexCreate_SRGB));
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(ColorEntry.Action));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(ColorEntry.Action));
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if (EnumHasAnyFlags(Texture->GetDesc().Flags, TexCreate_Memoryless))
		{
			ensure(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
		}

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
		ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT && ColorEntry.ResolveTarget)
		{
			Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
			Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
			ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CompatibleHashInfo.AttachmentsToResolve |= (uint16)(1 << NumColorAttachments);
			++NumAttachmentDescriptions;
			bHasResolveAttachments = true;
		}

		CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
		FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
#endif
		FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
		++CompatibleHashInfo.NumAttachments;

		++NumAttachmentDescriptions;
		++NumColorAttachments;
	}

	VkImageLayout DepthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (RPInfo.DepthStencilRenderTarget.DepthStencilTarget)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTexture* Texture = FVulkanTexture::Cast(RPInfo.DepthStencilRenderTarget.DepthStencilTarget);
		check(Texture);
		const FRHITextureDesc& TextureDesc = Texture->GetDesc();

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples());
		ensure(!NumSamples || CurrDesc.samples == NumSamples);
		NumSamples = CurrDesc.samples;
		CurrDesc.format = UEToVkTextureFormat(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetFormat(), false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		if (CurrDesc.samples != VK_SAMPLE_COUNT_1_BIT)
		{
			// Can't resolve MSAA depth/stencil
			ensure(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)) != ERenderTargetStoreAction::EMultisampleResolve);
			ensure(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)) != ERenderTargetStoreAction::EMultisampleResolve);
		}

		CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
		CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));

		if (EnumHasAnyFlags(TextureDesc.Flags, TexCreate_Memoryless))
		{
			ensure(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			ensure(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
		}

		FExclusiveDepthStencil ExclusiveDepthStencil = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
		if (FVulkanPlatform::RequiresDepthWriteOnStencilClear() &&
			RPInfo.DepthStencilRenderTarget.Action == EDepthStencilTargetActions::LoadDepthClearStencil_StoreDepthStencil)
		{
			ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
			CurrentDSLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		// Make sure that the requested depth-stencil access is compatible with the current layout of the DS target.
		const bool bWritableDepth = ExclusiveDepthStencil.IsDepthWrite();
		const bool bWritableStencil = ExclusiveDepthStencil.IsStencilWrite();
		switch (CurrentDSLayout)
		{
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Writable depth-stencil is compatible with all the requested modes.
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
				// Read-only on both aspects requires the requested access to be read-only.
				ensureMsgf(!bWritableDepth && !bWritableStencil, TEXT("Both aspects of the DS target are read-only, but the requested mode requires write access: D=%s S=%s."),
					ExclusiveDepthStencil.IsUsingDepth() ? (ExclusiveDepthStencil.IsDepthWrite() ? TEXT("Write") : TEXT("Read")) : TEXT("Nop"),
					ExclusiveDepthStencil.IsUsingStencil() ? (ExclusiveDepthStencil.IsStencilWrite() ? TEXT("Write") : TEXT("Read")) : TEXT("Nop")
				);
				break;

			case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
				// If only stencil is writable, the requested depth access must be read-only.
				ensureMsgf(!bWritableDepth, TEXT("The depth aspect is read-only, but the requested mode requires depth writes: D=%s S=%s."),
					ExclusiveDepthStencil.IsUsingDepth() ? (ExclusiveDepthStencil.IsDepthWrite() ? TEXT("Write") : TEXT("Read")) : TEXT("Nop"),
					ExclusiveDepthStencil.IsUsingStencil() ? (ExclusiveDepthStencil.IsStencilWrite() ? TEXT("Write") : TEXT("Read")) : TEXT("Nop")
				);
				break;

			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
				// If only depth is writable, the requested stencil access must be read-only.
				ensureMsgf(!bWritableStencil, TEXT("The stencil aspect is read-only, but the requested mode requires stencil writes: D=%s S=%s."),
					ExclusiveDepthStencil.IsUsingDepth() ? (ExclusiveDepthStencil.IsDepthWrite() ? TEXT("Write") : TEXT("Read")) : TEXT("Nop"),
					ExclusiveDepthStencil.IsUsingStencil() ? (ExclusiveDepthStencil.IsStencilWrite() ? TEXT("Write") : TEXT("Read")) : TEXT("Nop")
				);
				break;

			default:
				// Any other layout is invalid when starting a render pass.
				ensureMsgf(false, TEXT("Depth target is in layout %u, which is invalid for a render pass."), CurrentDSLayout);
				break;
		}

		DepthStencilLayout = CurrentDSLayout;

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = DepthStencilLayout;
		CurrDesc.finalLayout = DepthStencilLayout;
		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = DepthStencilLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = DepthStencilLayout;
#endif
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;

		bHasDepthStencil = true;

		if (bSetExtent)
		{
			// Depth can be greater or equal to color. Clamp to the smaller size.
			Extent.Extent3D.width = FMath::Min<uint32>(Extent.Extent3D.width, TextureDesc.Extent.X);
			Extent.Extent3D.height = FMath::Min<uint32>(Extent.Extent3D.height, TextureDesc.Extent.Y);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = TextureDesc.Extent.X;
			Extent.Extent3D.height = TextureDesc.Extent.Y;
			Extent.Extent3D.depth = TextureDesc.Depth;
		}
	}
	else if (NumColorRenderTargets == 0)
	{
		// No Depth and no color, it's a raster-only pass so make sure the renderArea will be set up properly
		checkf(RPInfo.ResolveRect.IsValid(), TEXT("For raster-only passes without render targets, ResolveRect has to contain the render area"));
		bSetExtent = true;
		Offset.Offset3D.x = RPInfo.ResolveRect.X1;
		Offset.Offset3D.y = RPInfo.ResolveRect.Y1;
		Offset.Offset3D.z = 0;
		Extent.Extent3D.width = RPInfo.ResolveRect.X2 - RPInfo.ResolveRect.X1;
		Extent.Extent3D.height = RPInfo.ResolveRect.Y2 - RPInfo.ResolveRect.Y1;
		Extent.Extent3D.depth = 1;
	}

	if (GRHISupportsAttachmentVariableRateShading && GRHIVariableRateShadingEnabled && GRHIAttachmentVariableRateShadingEnabled && RPInfo.ShadingRateTexture)
	{
		FVulkanTexture* Texture = FVulkanTexture::Cast(RPInfo.ShadingRateTexture);
		check(Texture->GetFormat() == GRHIVariableRateShadingImageFormat);

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		const VkImageLayout VRSLayout = GetVRSImageLayout();

		CurrDesc.flags = 0;
		CurrDesc.format = UEToVkTextureFormat(RPInfo.ShadingRateTexture->GetFormat(), false);
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.ShadingRateTexture->GetNumSamples());
		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = VRSLayout;
		CurrDesc.finalLayout = VRSLayout;

		FragmentDensityReference.attachment = NumAttachmentDescriptions;
		FragmentDensityReference.layout = VRSLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = VRSLayout;
#endif
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets + 1] = CurrDesc.format;

		++NumAttachmentDescriptions;
		bHasFragmentDensityAttachment = true;
	}

	SubpassHint = RPInfo.SubpassHint;
	CompatibleHashInfo.SubpassHint = (uint8)RPInfo.SubpassHint;

	CompatibleHashInfo.QCOMRenderPassTransform = QCOMRenderPassTransform;

	CompatibleHashInfo.NumSamples = NumSamples;
	CompatibleHashInfo.MultiViewCount = MultiViewCount;

	if (MultiViewCount > 1 && !bMultiviewRenderTargets)
	{
		UE_LOG(LogVulkan, Error, TEXT("Non multiview textures on a multiview layout!"));
	}

	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FGraphicsPipelineStateInitializer& Initializer)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, MultiViewCount(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(FragmentDensityReference);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(InputAttachments);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bFoundClearOp = false;
	MultiViewCount = Initializer.MultiViewCount;
	NumSamples = Initializer.NumSamples;
	for (uint32 Index = 0; Index < Initializer.RenderTargetsEnabled; ++Index)
	{
		EPixelFormat UEFormat = (EPixelFormat)Initializer.RenderTargetFormats[Index];
		if (UEFormat != PF_Unknown)
		{
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkTextureFormat(UEFormat, EnumHasAllFlags(Initializer.RenderTargetFlags[Index], TexCreate_SRGB));
			CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			// If the initial != final we need to change the FullHashInfo and use FinalLayout
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				Desc[NumAttachmentDescriptions + 1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				Desc[NumAttachmentDescriptions + 1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				CompatibleHashInfo.AttachmentsToResolve |= (uint16)(1 << NumColorAttachments);
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
			FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
			FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
			FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
#endif
			++CompatibleHashInfo.NumAttachments;

			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	if (Initializer.DepthStencilTargetFormat != PF_Unknown)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkTextureFormat(Initializer.DepthStencilTargetFormat, false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(Initializer.DepthTargetLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(Initializer.StencilTargetLoadAction);
		if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			bFoundClearOp = true;
		}
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(Initializer.DepthTargetStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(Initializer.StencilTargetStoreAction);
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
#endif
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;
		bHasDepthStencil = true;
	}

	if (Initializer.bHasFragmentDensityAttachment)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		const VkImageLayout VRSLayout = GetVRSImageLayout();

		check(GRHIVariableRateShadingImageFormat != PF_Unknown);

		CurrDesc.flags = 0;
		CurrDesc.format = UEToVkTextureFormat(GRHIVariableRateShadingImageFormat, false);
		CurrDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = VRSLayout;
		CurrDesc.finalLayout = VRSLayout;

		FragmentDensityReference.attachment = NumAttachmentDescriptions;
		FragmentDensityReference.layout = VRSLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
#if VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = VRSLayout;
#endif
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets + 1] = CurrDesc.format;

		++NumAttachmentDescriptions;
		bHasFragmentDensityAttachment = true;
	}

	SubpassHint = Initializer.SubpassHint;
	CompatibleHashInfo.SubpassHint = (uint8)Initializer.SubpassHint;

	FVulkanCommandListContext& ImmediateContext = GVulkanRHI->GetDevice()->GetImmediateContext();

	if (GVulkanRHI->GetDevice()->GetOptionalExtensions().HasQcomRenderPassTransform)
	{
		VkFormat SwapchainImageFormat = ImmediateContext.GetSwapchainImageFormat();
		if (Desc[0].format == SwapchainImageFormat)
		{
			// Potential Swapchain RenderPass
			QCOMRenderPassTransform = ImmediateContext.GetSwapchainQCOMRenderPassTransform();
		}
		// TODO: add some checks to detect potential Swapchain pass
		else if (SwapchainImageFormat == VK_FORMAT_UNDEFINED)
		{
			// WA: to have compatible RP created with VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM flag
			QCOMRenderPassTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
		}
	}

	CompatibleHashInfo.QCOMRenderPassTransform = QCOMRenderPassTransform;

	CompatibleHashInfo.NumSamples = NumSamples;
	CompatibleHashInfo.MultiViewCount = MultiViewCount;

	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}
