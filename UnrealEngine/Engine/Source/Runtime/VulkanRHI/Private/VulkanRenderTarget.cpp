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
#include "VulkanRenderpass.h"

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

static int32 GIgnoreCPUReads = 0;
static FAutoConsoleVariableRef CVarVulkanIgnoreCPUReads(
	TEXT("r.Vulkan.IgnoreCPUReads"),
	GIgnoreCPUReads,
	TEXT("Debugging utility for GPU->CPU reads.\n")
	TEXT(" 0 will read from the GPU (default).\n")
	TEXT(" 1 will NOT read from the GPU and fill with zeros.\n"),
	ECVF_Default
	);

TAutoConsoleVariable<int32> GSubmitOcclusionBatchCmdBufferCVar(
	TEXT("r.Vulkan.SubmitOcclusionBatchCmdBuffer"),
	1,
	TEXT("1 to submit the cmd buffer after end occlusion query batch (default)"),
	ECVF_RenderThreadSafe
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
	RenderPass = Device->GetRenderPassManager().GetOrCreateRenderPass(RTLayout);
	return RenderPass;
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

	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		ConvertRawR11G11B10DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest, bLinearToGamma);
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

	case VK_FORMAT_R8G8_UNORM:
		ConvertRawR8G8DataToFColor(DestWidth, DestHeight, In, SrcPitch, Dest);
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
	checkf((!TextureRHI->GetDesc().IsTextureCube()) || (InFlags.GetCubeFace() == CubeFace_MAX), TEXT("Cube faces not supported yet."));

	const uint32 DestWidth = Rect.Max.X - Rect.Min.X;
	const uint32 DestHeight = Rect.Max.Y - Rect.Min.Y;
	const uint32 NumRequestedPixels = DestWidth * DestHeight;
	OutData.SetNumUninitialized(NumRequestedPixels);
	if (GIgnoreCPUReads)
	{
		// Debug: Fill with CPU
		FMemory::Memzero(OutData.GetData(), NumRequestedPixels * sizeof(FColor));
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
		FMemory::Memzero(OutData.GetData(), NumRequestedPixels * sizeof(FColor));
		return;
	}

	FVulkanTexture& Surface = *ResourceCast(TextureRHI);

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
		CopyRegion.imageSubresource.baseArrayLayer = InFlags.GetArrayIndex();
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageOffset.x = Rect.Min.X;
		CopyRegion.imageOffset.y = Rect.Min.Y;
		CopyRegion.imageExtent.width = DestWidth;
		CopyRegion.imageExtent.height = DestHeight;
		CopyRegion.imageExtent.depth = 1;

		const VkImageLayout PreviousLayout = FVulkanLayoutManager::SetExpectedLayout(CmdBuffer, Surface, ERHIAccess::CopySrc);

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

		FVulkanPipelineBarrier AfterBarrier;
		if ((PreviousLayout != VK_IMAGE_LAYOUT_UNDEFINED) && (PreviousLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
		{
			AfterBarrier.AddImageLayoutTransition(Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, PreviousLayout, FVulkanPipelineBarrier::MakeSubresourceRange(Surface.GetFullAspectMask()));
		}
		else
		{
			CmdBuffer->GetLayoutManager().SetFullLayout(Surface, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
		ensure(StagingBuffer->GetSize() >= BufferSize);

		AfterBarrier.AddMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
		AfterBarrier.Execute(CmdBuffer);

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
	OutData.SetNumUninitialized(FromColorData.Num());
	for (int Index = 0, Num = FromColorData.Num(); Index < Num; Index++)
	{
		OutData[Index] = FLinearColor(FromColorData[Index]);
	}
}

void FVulkanDynamicRHI::RHIMapStagingSurface(FRHITexture* TextureRHI, FRHIGPUFence* FenceRHI, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex)
{
	FVulkanTexture* Texture = ResourceCast(TextureRHI);

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

		const uint32 NumPixels = (Desc.Extent.X >> InMipIndex) * (Desc.Extent.Y >> InMipIndex);
		const uint32 Size = NumPixels * sizeof(FFloat16Color);
		VulkanRHI::FStagingBuffer* StagingBuffer = InDevice->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		FVulkanPipelineBarrier AfterBarrier;
		if (GIgnoreCPUReads == 0)
		{
			VkBufferImageCopy CopyRegion;
			FMemory::Memzero(CopyRegion);
			//Region.bufferOffset = 0;
			CopyRegion.bufferRowLength = FMath::Max(1, Desc.Extent.X >> InMipIndex);
			CopyRegion.bufferImageHeight = FMath::Max(1, Desc.Extent.Y >> InMipIndex);
			CopyRegion.imageSubresource.aspectMask = Surface.GetFullAspectMask();
			CopyRegion.imageSubresource.mipLevel = InMipIndex;
			CopyRegion.imageSubresource.baseArrayLayer = SrcBaseArrayLayer;
			CopyRegion.imageSubresource.layerCount = 1;
			CopyRegion.imageExtent.width = FMath::Max(1, Desc.Extent.X >> InMipIndex);
			CopyRegion.imageExtent.height = FMath::Max(1, Desc.Extent.Y >> InMipIndex);
			CopyRegion.imageExtent.depth = 1;

			const VkImageLayout OriginalLayout = FVulkanLayoutManager::SetExpectedLayout(InCmdBuffer, Surface, ERHIAccess::CopySrc);

			VulkanRHI::vkCmdCopyImageToBuffer(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

			if ((OriginalLayout != VK_IMAGE_LAYOUT_UNDEFINED) && (OriginalLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
			{
				const VkImageSubresourceRange ImageSubresource = FVulkanPipelineBarrier::MakeSubresourceRange(Surface.GetFullAspectMask());
				AfterBarrier.AddImageLayoutTransition(Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, OriginalLayout, ImageSubresource);
			}
			else
			{
				InCmdBuffer->GetLayoutManager().SetFullLayout(Surface, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			}
		}
		else
		{
			VulkanRHI::vkCmdFillBuffer(InCmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
		}

		// the staging buffer size may be bigger then the size due to alignment, etc. but it must not be smaller!
		ensure(StagingBuffer->GetSize() >= Size);

		AfterBarrier.AddMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
		AfterBarrier.Execute(InCmdBuffer);

		// Force upload
		InDevice->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();
		InDevice->WaitUntilIdle();

		StagingBuffer->InvalidateMappedMemory();

		uint32 OutWidth = InRect.Max.X - InRect.Min.X;
		uint32 OutHeight= InRect.Max.Y - InRect.Min.Y;
		OutputData.SetNumUninitialized(OutWidth * OutHeight);
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

	FVulkanTexture& Surface = *ResourceCast(TextureRHI);
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
	FVulkanTexture& Surface = *ResourceCast(TextureRHI);
	const FRHITextureDesc& Desc = Surface.GetDesc();

	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();
	const uint32 SizeZ = ZMinMax.Y - ZMinMax.X;
	const uint32 NumPixels = SizeX * SizeY * SizeZ;
	const uint32 Size = NumPixels * sizeof(FFloat16Color);

	// Allocate the output buffer.
	OutData.SetNumUninitialized(Size);

	if (GIgnoreCPUReads == 1)
	{
		// Debug: Fill with CPU
		FMemory::Memzero(OutData.GetData(), Size * sizeof(FFloat16Color));
		return;
	}

	Device->PrepareForCPURead();
	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();

	ensure(Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);

	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

	FVulkanPipelineBarrier AfterBarrier;
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
		const VkImageLayout OriginalLayout = FVulkanLayoutManager::SetExpectedLayout(CmdBuffer, Surface, ERHIAccess::CopySrc);

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

		if ((OriginalLayout != VK_IMAGE_LAYOUT_UNDEFINED) && (OriginalLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
		{
			AfterBarrier.AddImageLayoutTransition(Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, OriginalLayout, FVulkanPipelineBarrier::MakeSubresourceRange(Surface.GetFullAspectMask()));
		}
		else
		{
			CmdBuffer->GetLayoutManager().SetFullLayout(Surface, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
	}
	else
	{
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
	}

	// the staging buffer size may be bigger then the size due to alignment, etc. but it must not be smaller!
	ensure(StagingBuffer->GetSize() >= Size);

	AfterBarrier.AddMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
	AfterBarrier.Execute(CmdBuffer);

	// Force upload
	Device->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();
	Device->WaitUntilIdle();

	StagingBuffer->InvalidateMappedMemory();

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
		VkImage Image = ResourceCast(InTexture)->Image;
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
	VkImageLayout CurrentDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout CurrentStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (DSTexture)
	{
		FVulkanTexture& VulkanTexture = *ResourceCast(DSTexture);
		const VkImageAspectFlags AspectFlags = VulkanTexture.GetFullAspectMask();

		if (GetDevice()->SupportsParallelRendering())
		{
			const FExclusiveDepthStencil ExclusiveDepthStencil = InInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
			if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_DEPTH_BIT))
			{
				if (ExclusiveDepthStencil.IsDepthWrite())
				{
					CurrentDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				}
				else
				{
					// todo-jn: temporarily use tracking illegally because sometimes depth is left in ATTACHMENT_OPTIMAL for Read passes
					CurrentDepthLayout = CmdBuffer->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_DEPTH_BIT);
					if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
					{
						CurrentDepthLayout = GetQueue()->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_DEPTH_BIT);
						if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
						{
							if (ExclusiveDepthStencil.IsDepthRead())
							{
								CurrentDepthLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
							}
							else
							{
								UE_LOG(LogVulkanRHI, Warning, TEXT("Render pass [%s] used an unknown depth state!"), InName ? InName : TEXT("unknown"));
							}
						}
					}
				}
			}

			if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_STENCIL_BIT))
			{
				if (ExclusiveDepthStencil.IsStencilWrite())
				{
					CurrentStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
				}
				else if (ExclusiveDepthStencil.IsStencilRead())
				{
					CurrentStencilLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
				}
				else
				{
					// todo-jn: temporarily use tracking illegally because stencil is no-op so we can't know expected layout
					CurrentStencilLayout = CmdBuffer->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_STENCIL_BIT);
					if (CurrentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
					{
						CurrentStencilLayout = GetQueue()->GetLayoutManager().GetDepthStencilHint(VulkanTexture, VK_IMAGE_ASPECT_STENCIL_BIT);
						// If the layout is still UNDEFINED, FVulkanRenderTargetLayout will force it into a known state
					}
				}
			}
		}
		else
		{
			const FVulkanImageLayout* FullLayout = CmdBuffer->GetLayoutManager().GetFullLayout(VulkanTexture);
			check(FullLayout);
			if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_DEPTH_BIT))
			{
				CurrentDepthLayout = FullLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_DEPTH_BIT);
			}
			if (VKHasAnyFlags(AspectFlags, VK_IMAGE_ASPECT_STENCIL_BIT))
			{
				CurrentStencilLayout = FullLayout->GetSubresLayout(0, 0, VK_IMAGE_ASPECT_STENCIL_BIT);
			}
		}
	}

	FVulkanRenderTargetLayout RTLayout(*Device, InInfo, CurrentDepthLayout, CurrentStencilLayout);
	check(RTLayout.GetExtent2D().width != 0 && RTLayout.GetExtent2D().height != 0);

	FVulkanRenderPass* RenderPass = Device->GetRenderPassManager().GetOrCreateRenderPass(RTLayout);
	FRHISetRenderTargetsInfo RTInfo;
	InInfo.ConvertToRenderTargetsInfo(RTInfo);

	FVulkanFramebuffer* Framebuffer = Device->GetRenderPassManager().GetOrCreateFramebuffer(RTInfo, RTLayout, RenderPass);
	checkf(RenderPass != nullptr && Framebuffer != nullptr, TEXT("RenderPass not started! Bad combination of values? Depth %p #Color %d Color0 %p"), (void*)InInfo.DepthStencilRenderTarget.DepthStencilTarget, InInfo.GetNumColorRenderTargets(), (void*)InInfo.ColorRenderTargets[0].RenderTarget);
	Device->GetRenderPassManager().BeginRenderPass(*this, *Device, CmdBuffer, InInfo, RTLayout, RenderPass, Framebuffer);

	check(!CurrentRenderPass);
	CurrentRenderPass = RenderPass;
	CurrentFramebuffer = Framebuffer;
}

void FVulkanCommandListContext::RHIEndRenderPass()
{
	const bool bHasOcclusionQueries = (RenderPassInfo.NumOcclusionQueries > 0);
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	if (bHasOcclusionQueries)
	{
		EndOcclusionQueryBatch(CmdBuffer);
	}
	
	Device->GetRenderPassManager().EndRenderPass(CmdBuffer);

	check(CurrentRenderPass);
	CurrentRenderPass = nullptr;

	RHIPopEvent();

	// Sync point for passes with occlusion queries
	if (bHasOcclusionQueries && GSubmitOcclusionBatchCmdBufferCVar.GetValueOnAnyThread())
	{
		RequestSubmitCurrentCommands();
		SafePointSubmit();
	}
}

void FVulkanCommandListContext::RHINextSubpass()
{
	check(CurrentRenderPass);
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
	// +1 for Depth, +1 for Stencil, +1 for Fragment Density
	VkFormat						Formats[MaxSimultaneousRenderTargets + 3];
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
	// If the initial != final we need to add FinalLayout and potentially RefLayout
	VkImageLayout						InitialLayout[MaxSimultaneousRenderTargets + 3];
	//VkImageLayout						FinalLayout[MaxSimultaneousRenderTargets + 3];
	//VkImageLayout						RefLayout[MaxSimultaneousRenderTargets + 3];
};

VkImageLayout FVulkanRenderTargetLayout::GetVRSImageLayout() const
{
	if (ValidateShadingRateDataType())
	{
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
		}
		if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
		}
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
	ResetAttachments();

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
	{
		const FRHIRenderTargetView& RTView = RTInfo.ColorRenderTarget[Index];
		if (RTView.Texture)
		{
			FVulkanTexture* Texture = ResourceCast(RTView.Texture);
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
			FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			++CompatibleHashInfo.NumAttachments;

			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	if (RTInfo.DepthStencilRenderTarget.Texture)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTexture* Texture = ResourceCast(RTInfo.DepthStencilRenderTarget.Texture);
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

		const VkImageLayout DepthLayout = RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsDepthWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		const VkImageLayout StencilLayout = RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess().IsStencilWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = DepthLayout;
		CurrDesc.finalLayout = DepthLayout;
		StencilDesc.stencilInitialLayout = StencilLayout;
		StencilDesc.stencilFinalLayout = StencilLayout;

		DepthReference.attachment = NumAttachmentDescriptions;
		DepthReference.layout = DepthLayout;
		StencilReference.stencilLayout = StencilLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = DepthLayout;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = StencilLayout;
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
		FVulkanTexture* Texture = ResourceCast(RTInfo.ShadingRateTexture);
		check(Texture->GetFormat() == GRHIVariableRateShadingImageFormat);

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		const VkImageLayout VRSLayout = GetVRSImageLayout();
		
		CurrDesc.flags = 0;
		CurrDesc.format = UEToVkTextureFormat(RTInfo.ShadingRateTexture->GetFormat(), false);
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RTInfo.ShadingRateTexture->GetNumSamples());
		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = VRSLayout;
		CurrDesc.finalLayout = VRSLayout;

		FragmentDensityReference.attachment = NumAttachmentDescriptions;
		FragmentDensityReference.layout = VRSLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 2] = VRSLayout;
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


FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(FVulkanDevice& InDevice, const FRHIRenderPassInfo& RPInfo, VkImageLayout CurrentDepthLayout, VkImageLayout CurrentStencilLayout)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, bHasFragmentDensityAttachment(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
	, MultiViewCount(RPInfo.MultiViewCount)
{
	ResetAttachments();

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	bool bMultiviewRenderTargets = false;

	int32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
	for (int32 Index = 0; Index < NumColorRenderTargets; ++Index)
	{
		const FRHIRenderPassInfo::FColorEntry& ColorEntry = RPInfo.ColorRenderTargets[Index];
		FVulkanTexture* Texture = ResourceCast(ColorEntry.RenderTarget);
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

		// CustomResolveSubpass can have targets with a different NumSamples
		ensure(!NumSamples || NumSamples == ColorEntry.RenderTarget->GetNumSamples() || RPInfo.SubpassHint == ESubpassHint::CustomResolveSubpass);
		NumSamples = ColorEntry.RenderTarget->GetNumSamples();

		ensure(!GetIsMultiView() || !bMultiviewRenderTargets || Texture->GetNumberOfArrayLevels() > 1);
		bMultiviewRenderTargets = Texture->GetNumberOfArrayLevels() > 1;
		// With a CustomResolveSubpass last color attachment is a resolve target
		bool bCustomResolveAttachment = (Index == (NumColorRenderTargets - 1)) && RPInfo.SubpassHint == ESubpassHint::CustomResolveSubpass;

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
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
		FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
		++CompatibleHashInfo.NumAttachments;

		++NumAttachmentDescriptions;
		++NumColorAttachments;
	}

	if (RPInfo.DepthStencilRenderTarget.DepthStencilTarget)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTexture* Texture = ResourceCast(RPInfo.DepthStencilRenderTarget.DepthStencilTarget);
		check(Texture);
		const FRHITextureDesc& TextureDesc = Texture->GetDesc();

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples());
		// CustomResolveSubpass can have targets with a different NumSamples
		ensure(!NumSamples || CurrDesc.samples == NumSamples || RPInfo.SubpassHint == ESubpassHint::CustomResolveSubpass);
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
			CurrentDepthLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
			CurrentStencilLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}

		// If the initial != final we need to change the FullHashInfo and use FinalLayout
		CurrDesc.initialLayout = CurrentDepthLayout;
		CurrDesc.finalLayout = CurrentDepthLayout;
		StencilDesc.stencilInitialLayout = CurrentStencilLayout;
		StencilDesc.stencilFinalLayout = CurrentStencilLayout;

		// We can't have the final layout be UNDEFINED, but it's possible that we get here from a transient texture
		// where the stencil was never used yet.  We can set the layout to whatever we want, the next transition will
		// happen from UNDEFINED anyhow.
		if (CurrentDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			check(CurrDesc.storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}
		if (CurrentStencilLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			check(CurrDesc.stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
			StencilDesc.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		}

		DepthReference.attachment = NumAttachmentDescriptions;
		DepthReference.layout = CurrentDepthLayout;
		StencilReference.stencilLayout = CurrentStencilLayout;

		++NumAttachmentDescriptions;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = CurrentDepthLayout;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = CurrentStencilLayout;
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;


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
		FVulkanTexture* Texture = ResourceCast(RPInfo.ShadingRateTexture);
		check(Texture->GetFormat() == GRHIVariableRateShadingImageFormat);

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		const VkImageLayout VRSLayout = GetVRSImageLayout();

		CurrDesc.flags = 0;
		CurrDesc.format = UEToVkTextureFormat(RPInfo.ShadingRateTexture->GetFormat(), false);
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.ShadingRateTexture->GetNumSamples());
		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = VRSLayout;
		CurrDesc.finalLayout = VRSLayout;

		FragmentDensityReference.attachment = NumAttachmentDescriptions;
		FragmentDensityReference.layout = VRSLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 2] = VRSLayout;
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
	ResetAttachments();

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
			// With a CustomResolveSubpass last color attachment is a resolve target
			bool bCustomResolveAttachment = (Index == (Initializer.RenderTargetsEnabled - 1)) && Initializer.SubpassHint == ESubpassHint::CustomResolveSubpass;
			
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = bCustomResolveAttachment ? VK_SAMPLE_COUNT_1_BIT : static_cast<VkSampleCountFlagBits>(NumSamples);
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
			FullHashInfo.InitialLayout[NumColorAttachments] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		StencilDesc.stencilInitialLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		StencilDesc.stencilFinalLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

		DepthReference.attachment = NumAttachmentDescriptions;
		DepthReference.layout = Initializer.DepthStencilAccess.IsDepthWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		StencilReference.stencilLayout = Initializer.DepthStencilAccess.IsStencilWrite() ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 1] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
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
		CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = VRSLayout;
		CurrDesc.finalLayout = VRSLayout;

		FragmentDensityReference.attachment = NumAttachmentDescriptions;
		FragmentDensityReference.layout = VRSLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 2] = CurrDesc.stencilStoreOp;
		FullHashInfo.InitialLayout[MaxSimultaneousRenderTargets + 2] = VRSLayout;
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
