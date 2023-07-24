// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanTexture.cpp: Vulkan texture RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "Containers/ResourceArray.h"
#include "VulkanLLM.h"
#include "VulkanBarriers.h"
#include "VulkanTransientResourceAllocator.h"

int32 GVulkanSubmitOnTextureUnlock = 1;
static FAutoConsoleVariableRef CVarVulkanSubmitOnTextureUnlock(
	TEXT("r.Vulkan.SubmitOnTextureUnlock"),
	GVulkanSubmitOnTextureUnlock,
	TEXT("Whether to submit upload cmd buffer on each texture unlock.\n")
	TEXT(" 0: Do not submit\n")
	TEXT(" 1: Submit (default)"),
	ECVF_Default
);

int32 GVulkanDepthStencilForceStorageBit = 0;
static FAutoConsoleVariableRef CVarVulkanDepthStencilForceStorageBit(
	TEXT("r.Vulkan.DepthStencilForceStorageBit"),
	GVulkanDepthStencilForceStorageBit,
	TEXT("Whether to force Image Usage Storage on Depth (can disable framebuffer compression).\n")
	TEXT(" 0: Not enabled\n")
	TEXT(" 1: Enables override for IMAGE_USAGE_STORAGE"),
	ECVF_Default
);

extern int32 GVulkanLogDefrag;

static FCriticalSection GTextureMapLock;

struct FTextureLock
{
	FRHIResource* Texture;
	uint32 MipIndex;
	uint32 LayerIndex;

	FTextureLock(FRHIResource* InTexture, uint32 InMipIndex, uint32 InLayerIndex = 0)
		: Texture(InTexture)
		, MipIndex(InMipIndex)
		, LayerIndex(InLayerIndex)
	{
	}
};

#if VULKAN_USE_LLM
inline ELLMTagVulkan GetMemoryTagForTextureFlags(ETextureCreateFlags UEFlags)
{
	bool bRenderTarget = EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable);
	return bRenderTarget ? ELLMTagVulkan::VulkanRenderTargets : ELLMTagVulkan::VulkanTextures;
}
#endif // VULKAN_USE_LLM

inline bool operator == (const FTextureLock& A, const FTextureLock& B)
{
	return A.Texture == B.Texture && A.MipIndex == B.MipIndex && A.LayerIndex == B.LayerIndex;
}

inline uint32 GetTypeHash(const FTextureLock& Lock)
{
	return GetTypeHash(Lock.Texture) ^ (Lock.MipIndex << 16) ^ (Lock.LayerIndex << 8);
}

static TMap<FTextureLock, VulkanRHI::FStagingBuffer*> GPendingLockedBuffers;

static const VkImageTiling GVulkanViewTypeTilingMode[VK_IMAGE_VIEW_TYPE_RANGE_SIZE] =
{
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_3D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D_ARRAY
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D_ARRAY
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
};

static TStatId GetVulkanStatEnum(bool bIsCube, bool bIs3D, bool bIsRT)
{
#if STATS
	if (bIsRT == false)
	{
		// normal texture
		if (bIsCube)
		{
			return GET_STATID(STAT_TextureMemoryCube);
		}
		else if (bIs3D)
		{
			return GET_STATID(STAT_TextureMemory3D);
		}
		else
		{
			return GET_STATID(STAT_TextureMemory2D);
		}
	}
	else
	{
		// render target
		if (bIsCube)
		{
			return GET_STATID(STAT_RenderTargetMemoryCube);
		}
		else if (bIs3D)
		{
			return GET_STATID(STAT_RenderTargetMemory3D);
		}
		else
		{
			return GET_STATID(STAT_RenderTargetMemory2D);
		}
	}
#endif
	return TStatId();
}

static void UpdateVulkanTextureStats(int64 TextureSize, bool bIsCube, bool bIs3D, bool bIsRT)
{
	const int64 AlignedSize = (TextureSize > 0) ? Align(TextureSize, 1024) / 1024 : -(Align(-TextureSize, 1024) / 1024);
	if (bIsRT == false)
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, AlignedSize);
	}
	else
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentRendertargetMemorySize, AlignedSize);
	}

	INC_MEMORY_STAT_BY_FName(GetVulkanStatEnum(bIsCube, bIs3D, bIsRT).GetName(), TextureSize);
}

static void VulkanTextureAllocated(uint64 Size, VkImageViewType ImageType, bool bIsRT)
{
	bool bIsCube = ImageType == VK_IMAGE_VIEW_TYPE_CUBE || ImageType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	bool bIs3D = ImageType == VK_IMAGE_VIEW_TYPE_3D ;

	UpdateVulkanTextureStats(Size, bIsCube, bIs3D, bIsRT);
}

static void VulkanTextureDestroyed(uint64 Size, VkImageViewType ImageType, bool bIsRT)
{
	bool bIsCube = ImageType == VK_IMAGE_VIEW_TYPE_CUBE || ImageType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	bool bIs3D = ImageType == VK_IMAGE_VIEW_TYPE_3D;

	UpdateVulkanTextureStats(-(int64)Size, bIsCube, bIs3D, bIsRT);
}

inline void FVulkanTexture::InternalLockWrite(FVulkanCommandListContext& Context, FVulkanTexture* Surface, const VkBufferImageCopy& Region, VulkanRHI::FStagingBuffer* StagingBuffer)
{
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	const VkImageSubresourceLayers& ImageSubresource = Region.imageSubresource;
	const VkImageSubresourceRange SubresourceRange = FVulkanPipelineBarrier::MakeSubresourceRange(ImageSubresource.aspectMask, ImageSubresource.mipLevel, 1, ImageSubresource.baseArrayLayer, ImageSubresource.layerCount);

	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(Surface->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
		Barrier.Execute(CmdBuffer);
	}

	VulkanRHI::vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Surface->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(Surface->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Surface->GetDefaultLayout(), SubresourceRange);
		Barrier.Execute(CmdBuffer);
	}

	Surface->Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);

	if (GVulkanSubmitOnTextureUnlock != 0)
	{
		Context.GetCommandBufferManager()->SubmitUploadCmdBuffer();
	}
}

void FVulkanTexture::ErrorInvalidViewType() const
{
	UE_LOG(LogVulkanRHI, Error, TEXT("Invalid ViewType %s"), VK_TYPE_TO_STRING(VkImageViewType, GetViewType()));
}


struct FRHICommandLockWriteTexture final : public FRHICommand<FRHICommandLockWriteTexture>
{
	FVulkanTexture* Surface;
	VkBufferImageCopy Region;
	VulkanRHI::FStagingBuffer* StagingBuffer;

	FRHICommandLockWriteTexture(FVulkanTexture* InSurface, const VkBufferImageCopy& InRegion, VulkanRHI::FStagingBuffer* InStagingBuffer)
		: Surface(InSurface)
		, Region(InRegion)
		, StagingBuffer(InStagingBuffer)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		FVulkanTexture::InternalLockWrite(FVulkanCommandListContext::GetVulkanContext(RHICmdList.GetContext()), Surface, Region, StagingBuffer);
	}
};

void FVulkanTexture::GenerateImageCreateInfo(
	FImageCreateInfo& OutImageCreateInfo,
	FVulkanDevice& InDevice,
	const FRHITextureDesc& InDesc,
	VkFormat* OutStorageFormat,
	VkFormat* OutViewFormat,
	bool bForceLinearTexture)
{
	const VkPhysicalDeviceProperties& DeviceProperties = InDevice.GetDeviceProperties();
	const FPixelFormatInfo& FormatInfo = GPixelFormats[InDesc.Format];
	VkFormat TextureFormat = (VkFormat)FormatInfo.PlatformFormat;

	const ETextureCreateFlags UEFlags = InDesc.Flags;
	if(EnumHasAnyFlags(UEFlags, TexCreate_CPUReadback))
	{
		bForceLinearTexture = true;
	}

	checkf(TextureFormat != VK_FORMAT_UNDEFINED, TEXT("PixelFormat %d, is not supported for images"), (int32)InDesc.Format);
	VkImageCreateInfo& ImageCreateInfo = OutImageCreateInfo.ImageCreateInfo;
	ZeroVulkanStruct(ImageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

	const VkImageViewType ResourceType = UETextureDimensionToVkImageViewType(InDesc.Dimension);
	switch(ResourceType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
		check((uint32)InDesc.Extent.X <= DeviceProperties.limits.maxImageDimension1D);
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		check(InDesc.Extent.X == InDesc.Extent.Y);
		check((uint32)InDesc.Extent.X <= DeviceProperties.limits.maxImageDimensionCube);
		check((uint32)InDesc.Extent.Y <= DeviceProperties.limits.maxImageDimensionCube);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		check((uint32)InDesc.Extent.X <= DeviceProperties.limits.maxImageDimension2D);
		check((uint32)InDesc.Extent.Y <= DeviceProperties.limits.maxImageDimension2D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		check((uint32)InDesc.Extent.Y <= DeviceProperties.limits.maxImageDimension3D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		break;
	default:
		checkf(false, TEXT("Unhandled image type %d"), (int32)ResourceType);
		break;
	}

	VkFormat srgbFormat = UEToVkTextureFormat(InDesc.Format, EnumHasAllFlags(UEFlags, TexCreate_SRGB));
	VkFormat nonSrgbFormat = UEToVkTextureFormat(InDesc.Format, false);

	ImageCreateInfo.format = EnumHasAnyFlags(UEFlags, TexCreate_UAV) ? nonSrgbFormat : srgbFormat;

	checkf(ImageCreateInfo.format != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), (int32)InDesc.Format);
	if (OutViewFormat)
	{
		*OutViewFormat = srgbFormat;
	}
	if (OutStorageFormat)
	{
		*OutStorageFormat = nonSrgbFormat;
	}

	ImageCreateInfo.extent.width = InDesc.Extent.X;
	ImageCreateInfo.extent.height = InDesc.Extent.Y;
	ImageCreateInfo.extent.depth = ResourceType == VK_IMAGE_VIEW_TYPE_3D ? InDesc.Depth : 1;
	ImageCreateInfo.mipLevels = InDesc.NumMips;
	uint32 LayerCount = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? 6 : 1;
	ImageCreateInfo.arrayLayers = InDesc.ArraySize * LayerCount;
	check(ImageCreateInfo.arrayLayers <= DeviceProperties.limits.maxImageArrayLayers);

	ImageCreateInfo.flags = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;


	if(EnumHasAllFlags(UEFlags, TexCreate_SRGB))
	{
		if(InDevice.GetOptionalExtensions().HasKHRImageFormatList)
		{
			VkImageFormatListCreateInfoKHR& ImageFormatListCreateInfo = OutImageCreateInfo.ImageFormatListCreateInfo;
			ZeroVulkanStruct(ImageFormatListCreateInfo, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR);
			ImageFormatListCreateInfo.pNext = ImageCreateInfo.pNext;
			ImageCreateInfo.pNext = &ImageFormatListCreateInfo;
			ImageFormatListCreateInfo.viewFormatCount = 2;
			ImageFormatListCreateInfo.pViewFormats = OutImageCreateInfo.FormatsUsed;
			OutImageCreateInfo.FormatsUsed[0] = nonSrgbFormat;
			OutImageCreateInfo.FormatsUsed[1] = srgbFormat;
		}

		ImageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	if (InDevice.GetOptionalExtensions().HasKHRMaintenance1 && ImageCreateInfo.imageType == VK_IMAGE_TYPE_3D)
	{
		ImageCreateInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;
	}

	ImageCreateInfo.tiling = bForceLinearTexture ? VK_IMAGE_TILING_LINEAR : GVulkanViewTypeTilingMode[ResourceType];

	ImageCreateInfo.usage = 0;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//@TODO: should everything be created with the source bit?
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (EnumHasAnyFlags(UEFlags, TexCreate_Presentable))
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;		
	}
	else if (EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
	{
		if (EnumHasAllFlags(UEFlags, TexCreate_InputAttachmentRead))
		{
			ImageCreateInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		ImageCreateInfo.usage |= (EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		if (EnumHasAllFlags(UEFlags, TexCreate_Memoryless) && InDevice.GetDeviceMemoryManager().SupportsMemoryless())
		{
			ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			// Remove the transfer and sampled bits, as they are incompatible with the transient bit.
			ImageCreateInfo.usage &= ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		}
	}
	else if (EnumHasAnyFlags(UEFlags, TexCreate_DepthStencilResolveTarget))
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (EnumHasAnyFlags(UEFlags, TexCreate_ResolveTargetable))
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}

	if (EnumHasAnyFlags(UEFlags, TexCreate_Foveation) && ValidateShadingRateDataType())
	{
#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
		if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
		{
			ImageCreateInfo.usage |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		}
#endif

#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
		if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
		{
			ImageCreateInfo.usage |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
		}
#endif
	}
	
	if (EnumHasAnyFlags(UEFlags, TexCreate_UAV))
	{
		//cannot have the storage bit on a memoryless texture
		ensure(!EnumHasAnyFlags(UEFlags, TexCreate_Memoryless));
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	#if VULKAN_SUPPORTS_EXTERNAL_MEMORY
	if (EnumHasAnyFlags(UEFlags, TexCreate_External))
	{
		VkExternalMemoryImageCreateInfoKHR& ExternalMemImageCreateInfo = OutImageCreateInfo.ExternalMemImageCreateInfo;
		ZeroVulkanStruct(ExternalMemImageCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR);
#if PLATFORM_WINDOWS
		ExternalMemImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#else
	    ExternalMemImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
#endif
		ExternalMemImageCreateInfo.pNext = ImageCreateInfo.pNext;
    	ImageCreateInfo.pNext = &ExternalMemImageCreateInfo;
	}
#endif // VULKAN_SUPPORTS_EXTERNAL_MEMORY

	//#todo-rco: If using CONCURRENT, make sure to NOT do so on render targets as that kills DCC compression
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.queueFamilyIndexCount = 0;
	ImageCreateInfo.pQueueFamilyIndices = nullptr;

	uint8 NumSamples = InDesc.NumSamples;
	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR && NumSamples > 1)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Not allowed to create Linear textures with %d samples, reverting to 1 sample"), NumSamples);
		NumSamples = 1;
	}

	switch (NumSamples)
	{
	case 1:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		break;
	case 2:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
		break;
	case 4:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
		break;
	case 8:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
		break;
	case 16:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
		break;
	case 32:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_32_BIT;
		break;
	case 64:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_64_BIT;
		break;
	default:
		checkf(0, TEXT("Unsupported number of samples %d"), NumSamples);
		break;
	}

	FVulkanPlatform::SetImageMemoryRequirementWorkaround(ImageCreateInfo);

	const VkFormatProperties& FormatProperties = InDevice.GetFormatProperties(ImageCreateInfo.format);
	const VkFormatFeatureFlags FormatFlags = ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR ? 
		FormatProperties.linearTilingFeatures : 
		FormatProperties.optimalTilingFeatures;

	if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
	{
		// Some formats don't support sampling and that's ok, we'll use a STORAGE_IMAGE
		check(EnumHasAnyFlags(UEFlags, TexCreate_UAV | TexCreate_CPUReadback));
		ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
	{
		ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
		ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
	}

	if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
	{
		ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
		ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
	{
		ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
		ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
	{
		// this flag is used unconditionally, strip it without warnings 
		ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
		
	if (!VKHasAnyFlags(FormatFlags, VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
	{
		// this flag is used unconditionally, strip it without warnings 
		ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	if (EnumHasAnyFlags(UEFlags, TexCreate_DepthStencilTargetable) && GVulkanDepthStencilForceStorageBit)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
}

struct FRHICommandSetInitialImageState final : public FRHICommand<FRHICommandSetInitialImageState>
{
	FVulkanTexture* VulkanTexture;
	VkImageLayout InitialLayout;
	bool bOnlyAddToLayoutManager;
	bool bClear;
	bool bIsTransientResource;
	FClearValueBinding ClearValueBinding;

	FRHICommandSetInitialImageState(FVulkanTexture* InVulkanTexture, VkImageLayout InInitialLayout, bool bInOnlyAddToLayoutManager, bool bInClear, const FClearValueBinding& InClearValueBinding, bool bInIsTransientResource)
		: VulkanTexture(InVulkanTexture)
		, InitialLayout(InInitialLayout)
		, bOnlyAddToLayoutManager(bInOnlyAddToLayoutManager)
		, bClear(bInClear)
		, bIsTransientResource(bInIsTransientResource)
		, ClearValueBinding(InClearValueBinding)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		FVulkanCommandListContext& Context = FVulkanCommandListContext::GetVulkanContext(CmdList.GetContext());
		
		if (bOnlyAddToLayoutManager)
		{
			FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
			CmdBuffer->GetLayoutManager().SetFullLayout(*VulkanTexture, InitialLayout, true);
		}
		else
		{
			VulkanTexture->SetInitialImageState(Context, InitialLayout, bClear, ClearValueBinding, bIsTransientResource);
		}
	}
};

struct FRHICommandOnDestroyImage final : public FRHICommand<FRHICommandOnDestroyImage>
{
	VkImage Image;
	FVulkanDevice* Device;
	bool bRenderTarget;

	FRHICommandOnDestroyImage(VkImage InImage, FVulkanDevice* InDevice, bool bInRenderTarget)
		: Image(InImage)
		, Device(InDevice)
		, bRenderTarget(bInRenderTarget)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		Device->NotifyDeletedImage(Image, bRenderTarget);
	}
};

static VkImageLayout GetInitialLayoutFromRHIAccess(ERHIAccess RHIAccess, bool bIsDepthStencil, bool bSupportReadOnlyOptimal)
{
	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::RTV) || RHIAccess == ERHIAccess::Present)
	{
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVWrite))
	{
		return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::DSVRead))
	{
		return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::SRVMask))
	{
		if (bIsDepthStencil)
		{
			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		}

		return bSupportReadOnlyOptimal ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
	}

	if (EnumHasAnyFlags(RHIAccess, ERHIAccess::UAVMask))
	{
		return VK_IMAGE_LAYOUT_GENERAL;
	}

	switch (RHIAccess)
	{
		case ERHIAccess::Unknown:	return VK_IMAGE_LAYOUT_UNDEFINED;
		case ERHIAccess::Discard:	return VK_IMAGE_LAYOUT_UNDEFINED;
		case ERHIAccess::CopySrc:	return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case ERHIAccess::CopyDest:	return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	checkf(false, TEXT("Invalid initial access %d"), RHIAccess);
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

void FVulkanTexture::InternalMoveSurface(FVulkanDevice& InDevice, FVulkanCommandListContext& Context, FVulkanAllocation& DestAllocation, VkImageLayout OriginalLayout)
{
	FImageCreateInfo ImageCreateInfo;
	const FRHITextureDesc& Desc = GetDesc();
	FVulkanTexture::GenerateImageCreateInfo(ImageCreateInfo, InDevice, Desc, &StorageFormat, &ViewFormat);

	VkImage MovedImage;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(InDevice.GetInstanceHandle(), &ImageCreateInfo.ImageCreateInfo, VULKAN_CPU_ALLOCATOR, &MovedImage));
	checkf(Tiling == ImageCreateInfo.ImageCreateInfo.tiling, TEXT("Move has changed image tiling:  before [%s] != after [%s]"), VK_TYPE_TO_STRING(VkImageTiling, Tiling), VK_TYPE_TO_STRING(VkImageTiling, ImageCreateInfo.ImageCreateInfo.tiling));

	const ETextureCreateFlags UEFlags = Desc.Flags;
	const bool bRenderTarget = EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable);
	const bool bCPUReadback = EnumHasAnyFlags(UEFlags, TexCreate_CPUReadback);
	const bool bMemoryless = EnumHasAnyFlags(UEFlags, TexCreate_Memoryless);
	const bool bExternal = EnumHasAnyFlags(UEFlags, TexCreate_External);
	checkf(!bCPUReadback, TEXT("Move of CPUReadback surfaces not currently supported.   UEFlags=0x%x"), (int32)UEFlags);
	checkf(!bMemoryless || !InDevice.GetDeviceMemoryManager().SupportsMemoryless(), TEXT("Move of Memoryless surfaces not currently supported.   UEFlags=0x%x"), (int32)UEFlags);
	checkf(!bExternal, TEXT("Move of external memory not supported. UEFlags=0x%x"), (int32)UEFlags);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	// This shouldn't change
	VkMemoryRequirements MovedMemReqs;
	VulkanRHI::vkGetImageMemoryRequirements(InDevice.GetInstanceHandle(), MovedImage, &MovedMemReqs);
	checkf((MemoryRequirements.alignment == MovedMemReqs.alignment), TEXT("Memory requirements changed: alignment %d -> %d"), (int32)MemoryRequirements.alignment, (int32)MovedMemReqs.alignment);
	checkf((MemoryRequirements.size == MovedMemReqs.size), TEXT("Memory requirements changed: size %d -> %d"), (int32)MemoryRequirements.size, (int32)MovedMemReqs.size);
	checkf((MemoryRequirements.memoryTypeBits == MovedMemReqs.memoryTypeBits), TEXT("Memory requirements changed: memoryTypeBits %d -> %d"), (int32)MemoryRequirements.memoryTypeBits, (int32)MovedMemReqs.memoryTypeBits);
#endif // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT

	DestAllocation.BindImage(&InDevice, MovedImage);

	// Copy Original -> Moved
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
	VkCommandBuffer VkCmdBuffer = CmdBuffer->GetHandle();
	ensure(CmdBuffer->IsOutsideRenderPass());

	{
		const uint32 NumberOfArrayLevels = GetNumberOfArrayLevels();
		const VkImageSubresourceRange FullSubresourceRange = FVulkanPipelineBarrier::MakeSubresourceRange(FullAspectMask);

		// Transition to copying layouts
		{
			FVulkanPipelineBarrier Barrier;
			Barrier.AddImageLayoutTransition(Image, OriginalLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FullSubresourceRange);
			Barrier.AddImageLayoutTransition(MovedImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FullSubresourceRange);
			Barrier.Execute(CmdBuffer);
		}
		{
			VkImageCopy Regions[MAX_TEXTURE_MIP_COUNT];
			check(Desc.NumMips <= MAX_TEXTURE_MIP_COUNT);
			FMemory::Memzero(Regions);
			for (uint32 i = 0; i < Desc.NumMips; ++i)
			{
				VkImageCopy& Region = Regions[i];
				Region.extent.width = FMath::Max(1, Desc.Extent.X >> i);
				Region.extent.height = FMath::Max(1, Desc.Extent.Y >> i);
				Region.extent.depth = FMath::Max(1, Desc.Depth >> i);
				Region.srcSubresource.aspectMask = FullAspectMask;
				Region.dstSubresource.aspectMask = FullAspectMask;
				Region.srcSubresource.baseArrayLayer = 0;
				Region.dstSubresource.baseArrayLayer = 0;
				Region.srcSubresource.layerCount = NumberOfArrayLevels;
				Region.dstSubresource.layerCount = NumberOfArrayLevels;
				Region.srcSubresource.mipLevel = i;
				Region.dstSubresource.mipLevel = i;
			}
			VulkanRHI::vkCmdCopyImage(VkCmdBuffer,
				Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				MovedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				Desc.NumMips, &Regions[0]);
		}

		// Put the destination image in exactly the same layout the original image was
		{
			FVulkanPipelineBarrier Barrier;
			Barrier.AddImageLayoutTransition(Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, OriginalLayout, FullSubresourceRange);
			Barrier.AddImageLayoutTransition(MovedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, OriginalLayout, FullSubresourceRange);
			Barrier.Execute(CmdBuffer);
		}
	}

	{
		check(Image != VK_NULL_HANDLE);
		InDevice.NotifyDeletedImage(Image, bRenderTarget);
		InDevice.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Image, Image);

		if (GVulkanLogDefrag)
		{
			FGenericPlatformMisc::LowLevelOutputDebugStringf(TEXT("** MOVE IMAGE %p -> %p\n"), Image, MovedImage);
		}
	}

	Image = MovedImage;

	// Move is used for defrag, which uses layouts stored in the queue, update the layout stored there
	Context.GetQueue()->GetLayoutManager().SetFullLayout(*this, OriginalLayout);
}

void FVulkanTexture::DestroySurface()
{
	const bool bIsLocalOwner = (ImageOwnerType == EImageOwnerType::LocalOwner);
	const bool bHasExternalOwner = (ImageOwnerType == EImageOwnerType::ExternalOwner);

	if (CpuReadbackBuffer)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Buffer, CpuReadbackBuffer->Buffer);
		Device->GetMemoryManager().FreeVulkanAllocation(Allocation);
		delete CpuReadbackBuffer;

	}
	else if (bIsLocalOwner || bHasExternalOwner)
	{
		const bool bRenderTarget = EnumHasAnyFlags(GetDesc().Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable);
		FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
		{
			Device->NotifyDeletedImage(Image, bRenderTarget);
		}
		else
		{
			check(IsInRenderingThread());
			new (RHICmdList.AllocCommand<FRHICommandOnDestroyImage>()) FRHICommandOnDestroyImage(Image, Device, bRenderTarget);
		}

		if (bIsLocalOwner)
		{
			// If we don't own the allocation, it's transient memory not included in stats
			if (Allocation.HasAllocation())
			{
				VulkanTextureDestroyed(Allocation.Size, GetViewType(), bRenderTarget);
			}

			if (Image != VK_NULL_HANDLE)
			{
				Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Image, Image);
				Device->GetMemoryManager().FreeVulkanAllocation(Allocation);
				Image = VK_NULL_HANDLE;
			}
		}

		ImageOwnerType = EImageOwnerType::None;
	}
}


void FVulkanTexture::InvalidateMappedMemory()
{
	Allocation.InvalidateMappedMemory(Device);

}
void* FVulkanTexture::GetMappedPointer()
{
	return Allocation.GetMappedPointer(Device);
}



VkDeviceMemory FVulkanTexture::GetAllocationHandle() const
{
	if (Allocation.IsValid())
	{
		return Allocation.GetDeviceMemoryHandle(Device);
	}
	else
	{
		return VK_NULL_HANDLE;
	}
}

uint64 FVulkanTexture::GetAllocationOffset() const
{
	if (Allocation.IsValid())
	{
		return Allocation.Offset;
	}
	else
	{
		return 0;
	}
}

void FVulkanTexture::GetMipStride(uint32 MipIndex, uint32& Stride)
{
	// Calculate the width of the MipMap.
	const FRHITextureDesc& Desc = GetDesc();
	const EPixelFormat PixelFormat = Desc.Format;
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 MipSizeX = FMath::Max<uint32>(Desc.Extent.X >> MipIndex, BlockSizeX);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;

	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
	}

	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;

	Stride = NumBlocksX * BlockBytes;
}

void FVulkanTexture::GetMipOffset(uint32 MipIndex, uint32& Offset)
{
	uint32 offset = Offset = 0;
	for(uint32 i = 0; i < MipIndex; i++)
	{
		GetMipSize(i, offset);
		Offset += offset;
	}
}

void FVulkanTexture::GetMipSize(uint32 MipIndex, uint32& MipBytes)
{
	// Calculate the dimensions of mip-map level.
	const FRHITextureDesc& Desc = GetDesc();
	const EPixelFormat PixelFormat = Desc.Format;
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max<uint32>(Desc.Extent.X >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max<uint32>(Desc.Extent.Y >> MipIndex, BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;

	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}

	// Size in bytes
	MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
/*
#if VULKAN_HAS_DEBUGGING_ENABLED
	VkImageSubresource SubResource;
	FMemory::Memzero(SubResource);
	SubResource.aspectMask = FullAspectMask;
	SubResource.mipLevel = MipIndex;
	//SubResource.arrayLayer = 0;
	VkSubresourceLayout OutLayout;
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &SubResource, &OutLayout);
	ensure(MipBytes >= OutLayout.size);
#endif
*/
}

void FVulkanTexture::SetInitialImageState(FVulkanCommandListContext& Context, VkImageLayout InitialLayout, bool bClear, const FClearValueBinding& ClearValueBinding, bool bIsTransientResource)
{
	// Can't use TransferQueue as Vulkan requires that queue to also have Gfx or Compute capabilities...
	//#todo-rco: This function is only used during loading currently, if used for regular RHIClear then use the ActiveCmdBuffer
	// NOTE: Transient resources' memory might have belonged to another resource earlier in the ActiveCmdBuffer, so we can't use UploadCmdBuffer
	FVulkanCmdBuffer* CmdBuffer = bIsTransientResource ? Context.GetCommandBufferManager()->GetActiveCmdBuffer() : Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VkImageSubresourceRange SubresourceRange = FVulkanPipelineBarrier::MakeSubresourceRange(FullAspectMask);

	VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (bClear && !bIsTransientResource)
	{
		{
			FVulkanPipelineBarrier Barrier;
			Barrier.AddImageLayoutTransition(Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
			Barrier.Execute(CmdBuffer);
		}

		if (FullAspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
		{
			VkClearColorValue Color;
			FMemory::Memzero(Color);
			Color.float32[0] = ClearValueBinding.Value.Color[0];
			Color.float32[1] = ClearValueBinding.Value.Color[1];
			Color.float32[2] = ClearValueBinding.Value.Color[2];
			Color.float32[3] = ClearValueBinding.Value.Color[3];

			VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Color, 1, &SubresourceRange);
		}
		else
		{
			check(IsDepthOrStencilAspect());
			VkClearDepthStencilValue Value;
			FMemory::Memzero(Value);
			Value.depth = ClearValueBinding.Value.DSValue.Depth;
			Value.stencil = ClearValueBinding.Value.DSValue.Stencil;

			VulkanRHI::vkCmdClearDepthStencilImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Value, 1, &SubresourceRange);
		}

		CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	if ((InitialLayout != CurrentLayout) && (InitialLayout != VK_IMAGE_LAYOUT_UNDEFINED))
	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(Image, CurrentLayout, InitialLayout, SubresourceRange);
		Barrier.Execute(CmdBuffer);
	}

	CmdBuffer->GetLayoutManager().SetFullLayout(*this, InitialLayout);
}

void FVulkanTexture::CopyBindlessHandle(FVulkanCommandListContext& Context, FRHIDescriptorHandle DestHandle)
{
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();

	// Copy the our descriptor in the old texture's slot
	checkSlow(DefaultBindlessHandle.IsValid());
	checkSlow(DestHandle.IsValid());
	Device->GetBindlessDescriptorManager()->CopyDescriptor(CmdBuffer->GetHandle(), DestHandle, DefaultBindlessHandle);
}



/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FVulkanDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	check(Device);
	const uint64 TotalGPUMemory = Device->GetDeviceMemoryManager().GetTotalMemory(true);
	const uint64 TotalCPUMemory = Device->GetDeviceMemoryManager().GetTotalMemory(false);

	OutStats.DedicatedVideoMemory = TotalGPUMemory;
	OutStats.DedicatedSystemMemory = TotalCPUMemory;
	OutStats.SharedSystemMemory = -1;
	OutStats.TotalGraphicsMemory = TotalGPUMemory ? TotalGPUMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

bool FVulkanDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	return false;
}

uint32 FVulkanDynamicRHI::RHIComputeMemorySize(FRHITexture* TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return FVulkanTexture::Cast(TextureRHI)->GetMemorySize();
}

void FVulkanDynamicRHI::RHIUpdateTextureReference(FRHITextureReference* TextureRef, FRHITexture* NewTexture)
{
	if (Device->SupportsBindless())
	{
		if (TextureRef->GetReferencedTexture() && NewTexture)
		{
			// Continue using the old slot for the new texture, this way indexes stored in buffers are still valid
			FVulkanTexture* NextTexture = ResourceCast(NewTexture);
			NextTexture->CopyBindlessHandle(Device->GetImmediateContext(), TextureRef->GetDefaultBindlessHandle());
		}
	}

	FDynamicRHI::RHIUpdateTextureReference(TextureRef, NewTexture);
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTextureRHIRef FVulkanDynamicRHI::RHICreateTexture(const FRHITextureCreateDesc& CreateDesc)
{
	LLM_SCOPE_VULKAN(GetMemoryTagForTextureFlags(CreateDesc.Flags));
	return new FVulkanTexture(*Device, CreateDesc, nullptr);
}

FTextureRHIRef FVulkanDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,ETextureCreateFlags Flags, ERHIAccess InResourceState,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogVulkan, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return FTextureRHIRef();
}

static void DoAsyncReallocateTexture2D(FVulkanCommandListContext& Context, FVulkanTexture* OldTexture, FVulkanTexture* NewTexture, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandGnmAsyncReallocateTexture2D_Execute);
	check(Context.IsImmediate());

	// figure out what mips to copy from/to
	const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
	const uint32 SourceFirstMip = OldTexture->GetNumMips() - NumSharedMips;
	const uint32 DestFirstMip = NewTexture->GetNumMips() - NumSharedMips;

	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	VkImageCopy Regions[MAX_TEXTURE_MIP_COUNT];
	check(NumSharedMips <= MAX_TEXTURE_MIP_COUNT);
	FMemory::Memzero(&Regions[0], sizeof(VkImageCopy) * NumSharedMips);
	for (uint32 Index = 0; Index < NumSharedMips; ++Index)
	{
		uint32 MipWidth = FMath::Max<uint32>(NewSizeX >> (DestFirstMip + Index), 1u);
		uint32 MipHeight = FMath::Max<uint32>(NewSizeY >> (DestFirstMip + Index), 1u);

		VkImageCopy& Region = Regions[Index];
		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.srcSubresource.mipLevel = SourceFirstMip + Index;
		Region.srcSubresource.baseArrayLayer = 0;
		Region.srcSubresource.layerCount = 1;
		//Region.srcOffset
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.dstSubresource.mipLevel = DestFirstMip + Index;
		Region.dstSubresource.baseArrayLayer = 0;
		Region.dstSubresource.layerCount = 1;
		//Region.dstOffset
		Region.extent.width = MipWidth;
		Region.extent.height = MipHeight;
		Region.extent.depth = 1;
	}

	const VkImageSubresourceRange SourceSubResourceRange = FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, SourceFirstMip, NumSharedMips);
	const VkImageSubresourceRange DestSubResourceRange = FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, DestFirstMip, NumSharedMips);

	{
		// Pre-copy barriers
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(OldTexture->Image, OldTexture->GetDefaultLayout(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SourceSubResourceRange);
		Barrier.AddImageLayoutTransition(NewTexture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DestSubResourceRange);
		Barrier.Execute(CmdBuffer);
	}

	VulkanRHI::vkCmdCopyImage(StagingCommandBuffer, OldTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, NewTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, NumSharedMips, Regions);

	{
		// Post-copy barriers
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(OldTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, OldTexture->GetDefaultLayout(), SourceSubResourceRange);
		Barrier.AddImageLayoutTransition(NewTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, NewTexture->GetDefaultLayout(), DestSubResourceRange);
		Barrier.Execute(CmdBuffer);

		// Add tracking for the appropriate subresources (intentionally leave added mips in VK_IMAGE_LAYOUT_UNDEFINED)
		// NOTE: Overwriting whatever is contained in the layout manager for the new texture (circumvents issue with stale layouts and pointer reuse)
		CmdBuffer->GetLayoutManager().SetLayout(*NewTexture, DestSubResourceRange, NewTexture->GetDefaultLayout());
	}

	// request is now complete
	RequestStatus->Decrement();

	// the next unlock for this texture can't block the GPU (it's during runtime)
	//NewTexture->bSkipBlockOnUnlock = true;
}

struct FRHICommandVulkanAsyncReallocateTexture2D final : public FRHICommand<FRHICommandVulkanAsyncReallocateTexture2D>
{
	FVulkanTexture* OldTexture;
	FVulkanTexture* NewTexture;
	int32 NewMipCount;
	int32 NewSizeX;
	int32 NewSizeY;
	FThreadSafeCounter* RequestStatus;

	FORCEINLINE_DEBUGGABLE FRHICommandVulkanAsyncReallocateTexture2D(FVulkanTexture* InOldTexture, FVulkanTexture* InNewTexture, int32 InNewMipCount, int32 InNewSizeX, int32 InNewSizeY, FThreadSafeCounter* InRequestStatus)
		: OldTexture(InOldTexture)
		, NewTexture(InNewTexture)
		, NewMipCount(InNewMipCount)
		, NewSizeX(InNewSizeX)
		, NewSizeY(InNewSizeY)
		, RequestStatus(InRequestStatus)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		FVulkanCommandListContext& VulkanContext = (FVulkanCommandListContext&)RHICmdList.GetContext().GetLowestLevelContext();
		check(VulkanContext.IsImmediate());
		DoAsyncReallocateTexture2D(VulkanContext, OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
};

FTexture2DRHIRef FVulkanDynamicRHI::AsyncReallocateTexture2D_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	if (RHICmdList.Bypass())
	{
		return FDynamicRHI::AsyncReallocateTexture2D_RenderThread(RHICmdList, OldTextureRHI, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}

	FVulkanTexture* OldTexture = FVulkanTexture::Cast(OldTextureRHI);
	const FRHITextureDesc& OldDesc = OldTexture->GetDesc();

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("AsyncReallocateTexture2D_RenderThread"), NewSizeX, NewSizeY, OldDesc.Format)
		.SetClearValue(OldDesc.ClearValue)
		.SetFlags(OldDesc.Flags)
		.SetNumMips(NewMipCount)
		.SetNumSamples(OldDesc.NumSamples)
		.DetermineInititialState();

	FVulkanTexture* NewTexture = new FVulkanTexture(*Device, Desc, nullptr);
	ALLOC_COMMAND_CL(RHICmdList, FRHICommandVulkanAsyncReallocateTexture2D)(OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncReallocateTexture2D(FRHITexture2D* OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* OldTexture = FVulkanTexture::Cast(OldTextureRHI);
	const FRHITextureDesc& OldDesc = OldTexture->GetDesc();

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("AsyncReallocateTexture2D_RenderThread"), NewSizeX, NewSizeY, OldDesc.Format)
		.SetClearValue(OldDesc.ClearValue)
		.SetFlags(OldDesc.Flags)
		.SetNumMips(NewMipCount)
		.SetNumSamples(OldDesc.NumSamples)
		.DetermineInititialState();

	FVulkanTexture* NewTexture = new FVulkanTexture(*Device, Desc, nullptr);

	DoAsyncReallocateTexture2D(Device->GetImmediateContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

void* FVulkanDynamicRHI::RHILockTexture2D(FRHITexture2D* TextureRHI, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureRHI, MipIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	// No locks for read allowed yet
	check(LockMode == RLM_WriteOnly);

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->GetMipSize(MipIndex, BufferSize);
	Texture->GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::InternalUnlockTexture2D(bool bFromRenderingThread, FRHITexture2D* TextureRHI, uint32 MipIndex, bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureRHI, MipIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	const FRHITextureDesc& Desc = Texture->GetDesc();
	const EPixelFormat Format = Desc.Format;
	uint32 MipWidth = FMath::Max<uint32>(Desc.Extent.X >> MipIndex, 0);
	uint32 MipHeight = FMath::Max<uint32>(Desc.Extent.Y >> MipIndex, 0);
	ensure(!(MipHeight == 0 && MipWidth == 0));
	MipWidth = FMath::Max<uint32>(MipWidth, 1);
	MipHeight = FMath::Max<uint32>(MipHeight, 1);
	uint32 LayerCount = Texture->GetNumberOfArrayLevels();

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = LayerCount;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanTexture::InternalLockWrite(Device->GetImmediateContext(), Texture, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(Texture, Region, StagingBuffer);
	}
}

void* FVulkanDynamicRHI::RHILockTexture2DArray(FRHITexture2DArray* TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureRHI, MipIndex, TextureIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	// No locks for read allowed yet
	check(LockMode == RLM_WriteOnly);

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->GetMipSize(MipIndex, BufferSize);
	Texture->GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTexture2DArray(FRHITexture2DArray* TextureRHI, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureRHI, MipIndex, TextureIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	const FRHITextureDesc& Desc = Texture->GetDesc();
	const EPixelFormat Format = Desc.Format;
	const uint32 MipWidth = FMath::Max<uint32>(Desc.Extent.X >> MipIndex, GPixelFormats[Format].BlockSizeX);
	const uint32 MipHeight = FMath::Max<uint32>(Desc.Extent.Y >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = Texture->GetPartialAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = TextureIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanTexture::InternalLockWrite(Device->GetImmediateContext(), Texture, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(Texture, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::InternalUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourceRowPitch, const uint8* SourceData)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);

	const EPixelFormat PixelFormat = Texture->GetDesc().Format;
	const int32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const int32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const int32 BlockSizeZ = GPixelFormats[PixelFormat].BlockSizeZ;
	const int32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	VkFormat Format = UEToVkTextureFormat(PixelFormat, false);

	ensure(BlockSizeZ == 1);

	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	const uint32 NumBlocksX = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Width, (uint32)BlockSizeX);
	const uint32 NumBlocksY = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Height, (uint32)BlockSizeY);
	ensure(NumBlocksX * BlockBytes <= SourceRowPitch);

	const uint32 DestRowPitch = NumBlocksX * BlockBytes;
	const uint32 DestSlicePitch = DestRowPitch * NumBlocksY;

	const uint32 BufferSize = Align(DestSlicePitch, Limits.minMemoryMapAlignment);
	StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* RESTRICT Memory = StagingBuffer->GetMappedPointer();

	uint8* RESTRICT DestData = (uint8*)Memory;
	uint8* RESTRICT SourceRowData = (uint8*)SourceData;
	for (uint32 Height = 0; Height < NumBlocksY; ++Height)
	{
		FMemory::Memcpy(DestData, SourceRowData, NumBlocksX * BlockBytes);
		DestData += DestRowPitch;
		SourceRowData += SourceRowPitch;
	}

	//Region.bufferOffset = 0;
	// Set these to zero to assume tightly packed buffer
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.DestX;
	Region.imageOffset.y = UpdateRegion.DestY;
	//Region.imageOffset.z = 0;
	Region.imageExtent.width = UpdateRegion.Width;
	Region.imageExtent.height = UpdateRegion.Height;
	Region.imageExtent.depth = 1;

	if (RHICmdList.IsBottomOfPipe())
	{
		FVulkanTexture::InternalLockWrite(FVulkanCommandListContext::GetVulkanContext(RHICmdList.GetContext()), Texture, Region, StagingBuffer);
	}
	else
	{
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(Texture, Region, StagingBuffer);
	}
}

FUpdateTexture3DData FVulkanDynamicRHI::RHIBeginUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	const int32 FormatSize = PixelFormatBlockBytes[Texture->GetFormat()];
	const int32 RowPitch = UpdateRegion.Width * FormatSize;
	const int32 DepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;

	SIZE_T MemorySize = static_cast<SIZE_T>(DepthPitch)* UpdateRegion.Depth;
	uint8* Data = (uint8*)FMemory::Malloc(MemorySize);

	return FUpdateTexture3DData(Texture, MipIndex, UpdateRegion, RowPitch, DepthPitch, Data, MemorySize, GFrameNumberRenderThread);
}

void FVulkanDynamicRHI::RHIEndUpdateTexture3D(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInParallelRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber);

	InternalUpdateTexture3D(RHICmdList, UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	
	FMemory::Free(UpdateData.Data);
	UpdateData.Data = nullptr;
}

void FVulkanDynamicRHI::InternalUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureRHI);

	const EPixelFormat PixelFormat = Texture->GetDesc().Format;
	const int32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const int32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const int32 BlockSizeZ = GPixelFormats[PixelFormat].BlockSizeZ;
	const int32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const VkFormat Format = UEToVkTextureFormat(PixelFormat, false);

	ensure(BlockSizeZ == 1);

	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	const uint32 NumBlocksX = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Width, (uint32)BlockSizeX);
	const uint32 NumBlocksY = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Height, (uint32)BlockSizeY);
	check(NumBlocksX * BlockBytes <= SourceRowPitch);
	check(NumBlocksX * BlockBytes * NumBlocksY <= SourceDepthPitch);

	const uint32 DestRowPitch = NumBlocksX * BlockBytes;
	const uint32 DestSlicePitch = DestRowPitch * NumBlocksY;

	const uint32 BufferSize = Align(DestSlicePitch * UpdateRegion.Depth, Limits.minMemoryMapAlignment);
	StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* RESTRICT Memory = StagingBuffer->GetMappedPointer();

	ensure(UpdateRegion.SrcX == 0);
	ensure(UpdateRegion.SrcY == 0);

	uint8* RESTRICT DestData = (uint8*)Memory;
	for (uint32 Depth = 0; Depth < UpdateRegion.Depth; Depth++)
	{
		uint8* RESTRICT SourceRowData = (uint8*)SourceData + SourceDepthPitch * Depth;
		for (uint32 Height = 0; Height < NumBlocksY; ++Height)
		{
			FMemory::Memcpy(DestData, SourceRowData, NumBlocksX * BlockBytes);
			DestData += DestRowPitch;
			SourceRowData += SourceRowPitch;
		}
	}
	uint32 TextureSizeX = FMath::Max(1u, TextureRHI->GetSizeX() >> MipIndex);
	uint32 TextureSizeY = FMath::Max(1u, TextureRHI->GetSizeY() >> MipIndex);
	uint32 TextureSizeZ = FMath::Max(1u, TextureRHI->GetSizeZ() >> MipIndex);

	//Region.bufferOffset = 0;
	// Set these to zero to assume tightly packed buffer
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.DestX;
	Region.imageOffset.y = UpdateRegion.DestY;
	Region.imageOffset.z = UpdateRegion.DestZ;
	Region.imageExtent.width = (uint32)FMath::Min((int32)(TextureSizeX-UpdateRegion.DestX), (int32)UpdateRegion.Width);
	Region.imageExtent.height = (uint32)FMath::Min((int32)(TextureSizeY-UpdateRegion.DestY), (int32)UpdateRegion.Height);
	Region.imageExtent.depth = (uint32)FMath::Min((int32)(TextureSizeZ-UpdateRegion.DestZ), (int32)UpdateRegion.Depth);

	if (RHICmdList.IsBottomOfPipe())
	{
		FVulkanTexture::InternalLockWrite(FVulkanCommandListContext::GetVulkanContext(RHICmdList.GetContext()), Texture, Region, StagingBuffer);
	}
	else
	{
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(Texture, Region, StagingBuffer);
	}
}

void FVulkanTextureView::Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle /*= false*/, VkImageUsageFlags ImageUsageFlags /*= 0*/)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	View = VK_NULL_HANDLE;

	VkImageViewCreateInfo ViewInfo;
	ZeroVulkanStruct(ViewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
	ViewInfo.image = InImage;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = Format;

#if VULKAN_SUPPORTS_ASTC_DECODE_MODE
	VkImageViewASTCDecodeModeEXT DecodeMode;
	if (Device.GetOptionalExtensions().HasEXTASTCDecodeMode && IsAstcLdrFormat(Format) && !IsAstcSrgbFormat(Format))
	{
		ZeroVulkanStruct(DecodeMode, VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT);
		DecodeMode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
		DecodeMode.pNext = ViewInfo.pNext;
		ViewInfo.pNext = &DecodeMode;
	}
#endif

	if (bUseIdentitySwizzle)
	{
		// VK_COMPONENT_SWIZZLE_IDENTITY == 0 and this was memzero'd already
	}
	else
	{
		ViewInfo.components = Device.GetFormatComponentMapping(UEFormat);
	}

	ViewInfo.subresourceRange.aspectMask = AspectFlags;
	ViewInfo.subresourceRange.baseMipLevel = FirstMip;
	ensure(NumMips != 0xFFFFFFFF);
	ViewInfo.subresourceRange.levelCount = NumMips;

	auto CheckUseNvidiaWorkaround = [&Device]() -> bool
	{
		if (Device.GetVendorId() == EGpuVendorId::Nvidia)
		{
			// Workaround for 20xx family not copying last mips correctly, so instead the view is created without the last 1x1 and 2x2 mips
			if (GRHIAdapterName.Contains(TEXT("RTX 20")))
			{
				UNvidiaDriverVersion NvidiaVersion;
				const VkPhysicalDeviceProperties& Props = Device.GetDeviceProperties();
				static_assert(sizeof(NvidiaVersion) == sizeof(Props.driverVersion), "Mismatched Nvidia pack driver version!");
				NvidiaVersion.Packed = Props.driverVersion;
				if (NvidiaVersion.Major < 430)
				{
					return true;
				}
			}
		}
		return false;
	};
	static bool bNvidiaWorkaround = CheckUseNvidiaWorkaround();
	if (bNvidiaWorkaround && Format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK && Format <= VK_FORMAT_BC7_SRGB_BLOCK && NumMips > 1)
	{
		ViewInfo.subresourceRange.levelCount = FMath::Max(1, int32(NumMips) - 2);
	}

	ensure(ArraySliceIndex != 0xFFFFFFFF);
	ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
	ensure(NumArraySlices != 0xFFFFFFFF);
	switch (ViewType)
	{
	case VK_IMAGE_VIEW_TYPE_3D:
		ViewInfo.subresourceRange.layerCount = 1;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		ensure(NumArraySlices == 1);
		ViewInfo.subresourceRange.layerCount = 6;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		ViewInfo.subresourceRange.layerCount = 6 * NumArraySlices;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		ViewInfo.subresourceRange.layerCount = NumArraySlices;
		break;
	default:
		ViewInfo.subresourceRange.layerCount = 1;
		break;
	}

	//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	if (UEFormat == PF_X24_G8)
	{
		ensure((ViewInfo.format == (VkFormat)GPixelFormats[PF_DepthStencil].PlatformFormat) && (ViewInfo.format != VK_FORMAT_UNDEFINED));
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	// Inform the driver the view will only be used with a subset of usage flags (to help performance and/or compatibility)
	VkImageViewUsageCreateInfo ImageViewUsageCreateInfo;
	if ((ImageUsageFlags != 0) && Device.GetOptionalExtensions().HasKHRMaintenance2)
	{
		ZeroVulkanStruct(ImageViewUsageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
		ImageViewUsageCreateInfo.usage = ImageUsageFlags;

		ImageViewUsageCreateInfo.pNext = (void*)ViewInfo.pNext;
		ViewInfo.pNext = &ImageViewUsageCreateInfo;
	}

	INC_DWORD_STAT(STAT_VulkanNumImageViews);
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImageView(Device.GetInstanceHandle(), &ViewInfo, VULKAN_CPU_ALLOCATOR, &View));

	Image = InImage;

	if (UseVulkanDescriptorCache())
	{
		ViewId = ++GVulkanImageViewHandleIdCounter;
	}
}

void FVulkanTextureView::Destroy(FVulkanDevice& Device)
{
	if (View)
	{
		DEC_DWORD_STAT(STAT_VulkanNumImageViews);
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::ImageView, View);
		Image = VK_NULL_HANDLE;
		View = VK_NULL_HANDLE;
		ViewId = 0;
	}
}

FVulkanTexture::FVulkanTexture(FVulkanDevice& InDevice, const FRHITextureCreateDesc& InCreateDesc, const FRHITransientHeapAllocation* InTransientHeapAllocation)
	: FRHITexture(InCreateDesc)
	, PartialView(nullptr)
	, Device(&InDevice)
	, Image(VK_NULL_HANDLE)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, FullAspectMask(0)
	, PartialAspectMask(0)
	, CpuReadbackBuffer(nullptr) // for readback textures we use a staging buffer. this is because vulkan only requires implementations to support 1 mip level(which is useless), so we emulate using a buffer
	, DefaultLayout(VK_IMAGE_LAYOUT_UNDEFINED)
{
	VULKAN_TRACK_OBJECT_CREATE(FVulkanTexture, this);

	{
		if (EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_CPUReadback))
		{
			check(InCreateDesc.NumSamples == 1);	//not implemented
			check(InCreateDesc.Depth == 1);		//not implemented
			check(InCreateDesc.ArraySize == 1);	//not implemented
			CpuReadbackBuffer = new FVulkanCpuReadbackBuffer;
			uint32 Size = 0;
			for (uint32 Mip = 0; Mip < InCreateDesc.NumMips; ++Mip)
			{
				uint32 LocalSize;
				GetMipSize(Mip, LocalSize);
				CpuReadbackBuffer->MipOffsets[Mip] = Size;
				CpuReadbackBuffer->MipSize[Mip] = LocalSize;
				Size += LocalSize;
			}

			VkDevice VulkanDevice = InDevice.GetInstanceHandle();
			const VkMemoryPropertyFlags BufferMemFlags = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

			VkBufferCreateInfo BufferCreateInfo;
			ZeroVulkanStruct(BufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
			BufferCreateInfo.size = Size;
			BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			VERIFYVULKANRESULT(VulkanRHI::vkCreateBuffer(VulkanDevice, &BufferCreateInfo, VULKAN_CPU_ALLOCATOR, &CpuReadbackBuffer->Buffer));
			VulkanRHI::vkGetBufferMemoryRequirements(VulkanDevice, CpuReadbackBuffer->Buffer, &MemoryRequirements);
			// Set minimum alignment to 16 bytes, as some buffers are used with CPU SIMD instructions
			MemoryRequirements.alignment = FMath::Max<VkDeviceSize>(16, MemoryRequirements.alignment);
			if (!InDevice.GetMemoryManager().AllocateBufferMemory(Allocation, this, MemoryRequirements, BufferMemFlags, EVulkanAllocationMetaBufferStaging, false, __FILE__, __LINE__))
			{
				InDevice.GetMemoryManager().HandleOOM();
			}
			Allocation.BindBuffer(Device, CpuReadbackBuffer->Buffer);
			void* Memory = Allocation.GetMappedPointer(Device);
			FMemory::Memzero(Memory, MemoryRequirements.size);

			ImageOwnerType = EImageOwnerType::None;
			ViewFormat = StorageFormat = UEToVkTextureFormat(InCreateDesc.Format, false);

			return;
		}

		ImageOwnerType = EImageOwnerType::LocalOwner;

		FImageCreateInfo ImageCreateInfo;
		FVulkanTexture::GenerateImageCreateInfo(ImageCreateInfo, InDevice, InCreateDesc, &StorageFormat, &ViewFormat);

		VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(InDevice.GetInstanceHandle(), &ImageCreateInfo.ImageCreateInfo, VULKAN_CPU_ALLOCATOR, &Image));

		// Fetch image size
		VulkanRHI::vkGetImageMemoryRequirements(InDevice.GetInstanceHandle(), Image, &MemoryRequirements);

		VULKAN_SET_DEBUG_NAME(InDevice, VK_OBJECT_TYPE_IMAGE, Image, TEXT("%s:(FVulkanTexture*)0x%p"), InCreateDesc.DebugName ? InCreateDesc.DebugName : TEXT("?"), this);

		FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, true, true);
		PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, false, true);

		// If VK_IMAGE_TILING_OPTIMAL is specified,
		// memoryTypeBits in vkGetImageMemoryRequirements will become 1
		// which does not support VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
		if (ImageCreateInfo.ImageCreateInfo.tiling != VK_IMAGE_TILING_OPTIMAL)
		{
			MemProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}

		const bool bRenderTarget = EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable);
		const bool bUAV = EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_UAV);
		const bool bDynamic = EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_Dynamic);
		const bool bExternal = EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_External);

		VkMemoryPropertyFlags MemoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		bool bMemoryless = EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_Memoryless) && InDevice.GetDeviceMemoryManager().SupportsMemoryless();
		if (bMemoryless)
		{
			if (ensureMsgf(bRenderTarget, TEXT("Memoryless surfaces can only be used for render targets")))
			{
				MemoryFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
			}
			else
			{
				bMemoryless = false;
			}
		}

		const bool bIsTransientResource = (InTransientHeapAllocation != nullptr);
		if (bIsTransientResource)
		{
			check(!bMemoryless);
			check(InTransientHeapAllocation->Offset % MemoryRequirements.alignment == 0);
			check(InTransientHeapAllocation->Size >= MemoryRequirements.size);
			Allocation = FVulkanTransientHeap::GetVulkanAllocation(*InTransientHeapAllocation);
		}
		else
		{
			EVulkanAllocationMetaType MetaType = (bRenderTarget || bUAV) ? EVulkanAllocationMetaImageRenderTarget : EVulkanAllocationMetaImageOther;
#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
			extern int32 GVulkanEnableDedicatedImageMemory;
			// Per https://developer.nvidia.com/what%E2%80%99s-your-vulkan-memory-type
			VkDeviceSize SizeToBeConsideredForDedicated = 12 * 1024 * 1024;
			if ((bRenderTarget || MemoryRequirements.size >= SizeToBeConsideredForDedicated) && !bMemoryless && InDevice.GetOptionalExtensions().HasKHRDedicatedAllocation && GVulkanEnableDedicatedImageMemory)
			{
				if (!InDevice.GetMemoryManager().AllocateDedicatedImageMemory(Allocation, this, Image, MemoryRequirements, MemoryFlags, MetaType, bExternal, __FILE__, __LINE__))
				{
					checkNoEntry();
				}
			}
			else
#endif
			{
				if (!InDevice.GetMemoryManager().AllocateImageMemory(Allocation, this, MemoryRequirements, MemoryFlags, MetaType, bExternal, __FILE__, __LINE__))
				{
					checkNoEntry();
				}
			}

			// update rhi stats
			VulkanTextureAllocated(Allocation.Size, GetViewType(), bRenderTarget);
		}
		Allocation.BindImage(Device, Image);

		Tiling = ImageCreateInfo.ImageCreateInfo.tiling;
		check(Tiling == VK_IMAGE_TILING_LINEAR || Tiling == VK_IMAGE_TILING_OPTIMAL);

		const VkImageLayout InitialLayout = GetInitialLayoutFromRHIAccess(InCreateDesc.InitialState, IsDepthOrStencilAspect(), SupportsSampling());
		const bool bDoInitialClear = VKHasAnyFlags(ImageCreateInfo.ImageCreateInfo.usage, VK_IMAGE_USAGE_SAMPLED_BIT) && EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable);

		if (InitialLayout != VK_IMAGE_LAYOUT_UNDEFINED || bDoInitialClear)
		{
			FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
			{
				SetInitialImageState(Device->GetImmediateContext(), InitialLayout, bDoInitialClear, InCreateDesc.ClearValue, bIsTransientResource);
			}
			else
			{
				check(IsInRenderingThread());
				ALLOC_COMMAND_CL(RHICmdList, FRHICommandSetInitialImageState)(this, InitialLayout, false, bDoInitialClear, InCreateDesc.ClearValue, bIsTransientResource);
			}
		}

		DefaultLayout = InitialLayout;
	}

	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	const VkImageViewType ViewType = GetViewType();
	const bool bArray = ViewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	if (ViewFormat == VK_FORMAT_UNDEFINED)
	{
		StorageFormat = UEToVkTextureFormat(InCreateDesc.Format, false);
		ViewFormat = UEToVkTextureFormat(InCreateDesc.Format, EnumHasAllFlags(InCreateDesc.Flags, TexCreate_SRGB));
		checkf(StorageFormat != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), (int32)InCreateDesc.Format);
	}

	if (ViewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		DefaultView.Create(InDevice, Image, ViewType, GetFullAspectMask(), InCreateDesc.Format, ViewFormat, 0, FMath::Max(InCreateDesc.NumMips, (uint8)1u), 0, bArray ? FMath::Max((uint16)1u, InCreateDesc.ArraySize) : FMath::Max((uint16)1u, InCreateDesc.Depth), !SupportsSampling());
	}

	if (FullAspectMask == PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(InDevice, Image, ViewType, PartialAspectMask, InCreateDesc.Format, ViewFormat, 0, FMath::Max(InCreateDesc.NumMips, (uint8)1u), 0, bArray ? FMath::Max((uint16)1u, InCreateDesc.ArraySize) : FMath::Max((uint16)1u, InCreateDesc.Depth), false);
	}

	DefaultBindlessHandle = Device->GetBindlessDescriptorManager()->RegisterImage(PartialView->View, SupportsSampling() ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IsDepthOrStencilAspect());

	if (!InCreateDesc.BulkData)
	{
		return;
	}

	// InternalLockWrite leaves the image in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, so make sure the requested resource state is SRV.
	check(EnumHasAnyFlags(InCreateDesc.InitialState, ERHIAccess::SRVMask));
	DefaultLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Transfer bulk data
	VulkanRHI::FStagingBuffer* StagingBuffer = InDevice.GetStagingManager().AcquireBuffer(InCreateDesc.BulkData->GetResourceBulkDataSize());
	void* Data = StagingBuffer->GetMappedPointer();

	// Do copy
	FMemory::Memcpy(Data, InCreateDesc.BulkData->GetResourceBulkData(), InCreateDesc.BulkData->GetResourceBulkDataSize());
	InCreateDesc.BulkData->Discard();

	uint32 LayersPerArrayIndex = (ViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Use real Buffer offset when switching to suballocations!
	Region.bufferOffset = 0;
	Region.bufferRowLength = InCreateDesc.Extent.X;
	Region.bufferImageHeight = InCreateDesc.Extent.Y;
	
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = InCreateDesc.ArraySize * LayersPerArrayIndex;
	Region.imageSubresource.aspectMask = GetFullAspectMask();

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = InCreateDesc.Depth;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanTexture::InternalLockWrite(InDevice.GetImmediateContext(), this, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(this, Region, StagingBuffer);
	}
}

FVulkanTexture::FVulkanTexture(FVulkanDevice& InDevice, const FRHITextureCreateDesc& InCreateDesc, VkImage InImage, bool /*bUnused*/)
	: FRHITexture(InCreateDesc)
	, Device(&InDevice)
	, Image(InImage)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, FullAspectMask(0)
	, PartialAspectMask(0)
	, CpuReadbackBuffer(nullptr)
	, DefaultLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	, ImageOwnerType(EImageOwnerType::ExternalOwner)
{
	VULKAN_TRACK_OBJECT_CREATE(FVulkanTexture, this);

	{
		StorageFormat = UEToVkTextureFormat(InCreateDesc.Format, false);

		checkf(InCreateDesc.Format == PF_Unknown || StorageFormat != VK_FORMAT_UNDEFINED, TEXT("PixelFormat %d, is not supported for images"), (int32)InCreateDesc.Format);

		ViewFormat = UEToVkTextureFormat(InCreateDesc.Format, EnumHasAllFlags(InCreateDesc.Flags, TexCreate_SRGB));
		FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, true, true);
		PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, false, true);

		// Purely informative patching, we know that "TexCreate_Presentable" uses optimal tiling
		if (EnumHasAllFlags(InCreateDesc.Flags, TexCreate_Presentable) && GetTiling() == VK_IMAGE_TILING_MAX_ENUM)
		{
			Tiling = VK_IMAGE_TILING_OPTIMAL;
		}

		if (Image != VK_NULL_HANDLE)
		{
#if VULKAN_ENABLE_WRAP_LAYER
			FImageCreateInfo ImageCreateInfo;
			FVulkanTexture::GenerateImageCreateInfo(ImageCreateInfo, InDevice, InCreateDesc, &StorageFormat, &ViewFormat);
			FWrapLayer::CreateImage(VK_SUCCESS, InDevice.GetInstanceHandle(), &ImageCreateInfo.ImageCreateInfo, &Image);
#endif
			VULKAN_SET_DEBUG_NAME(InDevice, VK_OBJECT_TYPE_IMAGE, Image, TEXT("%s:(FVulkanTexture*)0x%p"), InCreateDesc.DebugName ? InCreateDesc.DebugName : TEXT("?"), this);

			VkImageLayout InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			bool bOnlyAddToLayoutManager, bDoInitialClear;
			if (EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
			{
				InitialLayout = EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_DepthStencilTargetable) ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				bOnlyAddToLayoutManager = false;
				bDoInitialClear = true;
			}
			else if (EnumHasAnyFlags(InCreateDesc.Flags, TexCreate_Foveation) && ValidateShadingRateDataType())
			{
				// If it's a shading rate attachment texture, do not clear but add to layoutmgr, and set correct shading rate layout.
#if VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
				if (GRHIVariableRateShadingImageDataType == VRSImage_Palette)
				{
					InitialLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
				}
#endif

#if VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
				if (GRHIVariableRateShadingImageDataType == VRSImage_Fractional)
				{
					InitialLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
				}
#endif

				bOnlyAddToLayoutManager = true;
				bDoInitialClear = false;
			}
			else
			{
				// If we haven't seen this image before, we assume it's an SRV (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) and the call below
				// tells the layout manager about it. If we've seen it before, the call won't do anything, since the manager already knows the layout.
				InitialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				bOnlyAddToLayoutManager = true;
				bDoInitialClear = false;
			}

			DefaultLayout = InitialLayout;

			FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
			{
				FVulkanCommandListContext& Context = InDevice.GetImmediateContext();
				if (bOnlyAddToLayoutManager)
				{
					FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
					CmdBuffer->GetLayoutManager().SetFullLayout(*this, InitialLayout, true);
				}
				else
				{
					SetInitialImageState(Context, InitialLayout, true, InCreateDesc.ClearValue, false);
				}
			}
			else
			{
				check(IsInRenderingThread());
				ALLOC_COMMAND_CL(RHICmdList, FRHICommandSetInitialImageState)(this, InitialLayout, bOnlyAddToLayoutManager, bDoInitialClear, InCreateDesc.ClearValue, false);
			}
		}
	}

	const VkImageViewType ViewType = GetViewType();
	const bool bArray = ViewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

	if (Image != VK_NULL_HANDLE)
	{
		DefaultView.Create(InDevice, Image, ViewType, GetFullAspectMask(), InCreateDesc.Format, ViewFormat, 0, FMath::Max(InCreateDesc.NumMips, (uint8)1u), 0, bArray ? FMath::Max((uint16)1u, InCreateDesc.ArraySize) : FMath::Max((uint16)1u, InCreateDesc.Depth), !SupportsSampling());
	}

	if (FullAspectMask == PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(InDevice, Image, ViewType, PartialAspectMask, InCreateDesc.Format, ViewFormat, 0, FMath::Max(InCreateDesc.NumMips, (uint8)1u), 0, bArray ? FMath::Max((uint16)1u, InCreateDesc.ArraySize) : FMath::Max((uint16)1u, InCreateDesc.Depth), false);
	}

	if (PartialView->View)
	{
		DefaultBindlessHandle = Device->GetBindlessDescriptorManager()->RegisterImage(PartialView->View, SupportsSampling() ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IsDepthOrStencilAspect());
	}
}

FVulkanTexture::FVulkanTexture(FVulkanDevice& InDevice, const FRHITextureCreateDesc& InCreateDesc, FTextureRHIRef& SrcTextureRHI)
	: FRHITexture(InCreateDesc)
	, Device(&InDevice)
	, Image(VK_NULL_HANDLE)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, FullAspectMask(0)
	, PartialAspectMask(0)
	, CpuReadbackBuffer(nullptr)
	, DefaultLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	, ImageOwnerType(EImageOwnerType::Aliased)
{
	VULKAN_TRACK_OBJECT_CREATE(FVulkanTexture, this);

	{
		StorageFormat = UEToVkTextureFormat(InCreateDesc.Format, false);

		checkf(InCreateDesc.Format == PF_Unknown || StorageFormat != VK_FORMAT_UNDEFINED, TEXT("PixelFormat %d, is not supported for images"), (int32)InCreateDesc.Format);

		ViewFormat = UEToVkTextureFormat(InCreateDesc.Format, EnumHasAllFlags(InCreateDesc.Flags, TexCreate_SRGB));
		FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, true, true);
		PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(InCreateDesc.Format, false, true);

		// Purely informative patching, we know that "TexCreate_Presentable" uses optimal tiling
		if (EnumHasAllFlags(InCreateDesc.Flags, TexCreate_Presentable) && GetTiling() == VK_IMAGE_TILING_MAX_ENUM)
		{
			Tiling = VK_IMAGE_TILING_OPTIMAL;
		}
	}

	if (FullAspectMask == PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		// Skip create, since we're aliasing.
	}

	AliasTextureResources(SrcTextureRHI);
}

FVulkanTexture::~FVulkanTexture()
{
	VULKAN_TRACK_OBJECT_DELETE(FVulkanTexture, this);
	DestroyViews();

	if (PartialView != &DefaultView)
	{
		delete PartialView;
	}

	DestroySurface();
}

void FVulkanTexture::AliasTextureResources(FTextureRHIRef& SrcTextureRHI)
{
	DestroyViews();
	DestroySurface();

	FVulkanTexture* SrcTexture = FVulkanTexture::Cast(SrcTextureRHI);

	Image = SrcTexture->Image;
	DefaultView.View = SrcTexture->DefaultView.View;
	DefaultView.Image = SrcTexture->DefaultView.Image;
	DefaultView.ViewId = SrcTexture->DefaultView.ViewId;

	if (PartialView != &DefaultView)
	{
		PartialView->View = SrcTexture->PartialView->View;
		PartialView->Image = SrcTexture->PartialView->Image;
		PartialView->ViewId = SrcTexture->PartialView->ViewId;
	}

	AliasedTexture = SrcTexture;
	DefaultLayout = SrcTexture->DefaultLayout;
	DefaultBindlessHandle = SrcTexture->DefaultBindlessHandle;
}

void FVulkanTexture::DestroyViews()
{
	if (AliasedTexture == nullptr)
	{
		DefaultView.Destroy(*Device);

		if (PartialView != &DefaultView && PartialView != nullptr)
		{
			PartialView->Destroy(*Device);
		}

		if (DefaultBindlessHandle.IsValid())
		{
			Device->GetDeferredDeletionQueue().EnqueueBindlessHandle(DefaultBindlessHandle);
			DefaultBindlessHandle = FRHIDescriptorHandle();
		}
	}
}

void FVulkanTexture::InvalidateViews(FVulkanDevice& InDevice)
{
	DefaultView.Destroy(InDevice);
	const FRHITextureDesc& Desc = GetDesc();
	const uint32 NumMips = Desc.NumMips;
	const VkImageViewType ViewType = GetViewType();
	const bool bArray = ViewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY || ViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	const uint32 SizeZOrArraySize = bArray ? FMath::Max((uint16)1u, Desc.ArraySize) : FMath::Max((uint16)1u, Desc.Depth);

	if (ViewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		DefaultView.Create(InDevice, Image, ViewType, GetFullAspectMask(), GetDesc().Format, ViewFormat, 0, FMath::Max(NumMips, 1u), 0, SizeZOrArraySize, !SupportsSampling());
	}
	if (PartialView != &DefaultView)
	{
		PartialView->Destroy(*Device);
		PartialView->Create(InDevice, Image, ViewType, PartialAspectMask, GetDesc().Format, ViewFormat, 0, FMath::Max(NumMips, 1u), 0, SizeZOrArraySize, false);
	}

	if (DefaultBindlessHandle.IsValid())
	{
		Device->GetDeferredDeletionQueue().EnqueueBindlessHandle(DefaultBindlessHandle);
	}

	DefaultBindlessHandle = Device->GetBindlessDescriptorManager()->RegisterImage(PartialView->View, SupportsSampling() ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IsDepthOrStencilAspect());

	VulkanRHI::FVulkanViewBase* View = FirstView;
	while (View)
	{
		View->Invalidate();
		View = View->NextView;
	}
}

void FVulkanTexture::Move(FVulkanDevice& InDevice, FVulkanCommandListContext& Context, FVulkanAllocation& NewAllocation)
{
	const uint64 Size = GetMemorySize();
	static uint64 TotalSize = 0;
	TotalSize += Size;
	if (GVulkanLogDefrag)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Moving Surface, %p <<-- %p    :::: %s\n"), NewAllocation.Offset, 42, *GetName().ToString());
		UE_LOG(LogVulkanRHI, Display, TEXT("Moved %8.4fkb %8.4fkb   TB %p  :: IMG %p   %-40s\n"), Size / (1024.f), TotalSize / (1024.f), this, Image, *GetName().ToString());
	}

	// Move is used for defrag, which uses layouts stored in the queue
	const FVulkanImageLayout* OriginalLayout = Context.GetQueue()->GetLayoutManager().GetFullLayout(Image);
	check(OriginalLayout && OriginalLayout->AreAllSubresourcesSameLayout());

	const ETextureCreateFlags UEFlags = GetDesc().Flags;
	const bool bRenderTarget = EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable);
	const bool bUAV = EnumHasAnyFlags(UEFlags, TexCreate_UAV);
	checkf(bRenderTarget || bUAV, TEXT("Surface must be a RenderTarget or a UAV in order to be moved.  UEFlags=0x%x"), (int32)UEFlags);
	checkf(Tiling == VK_IMAGE_TILING_OPTIMAL, TEXT("Tiling [%s] is not supported for move, only VK_IMAGE_TILING_OPTIMAL"), VK_TYPE_TO_STRING(VkImageTiling, Tiling));
	checkf((OriginalLayout->NumMips == GetNumMips()), TEXT("NumMips reported by LayoutManager (%d) differs from surface (%d)"), OriginalLayout->NumMips, GetNumMips());
	checkf((OriginalLayout->NumLayers == GetNumberOfArrayLevels()), TEXT("NumLayers reported by LayoutManager (%d) differs from surface (%d)"), OriginalLayout->NumLayers, GetNumberOfArrayLevels());

	InternalMoveSurface(InDevice, Context, NewAllocation, OriginalLayout->MainLayout);
	
	// Swap in the new allocation for this surface
	Allocation.Swap(NewAllocation);

	InvalidateViews(InDevice);
}

void FVulkanTexture::Evict(FVulkanDevice& InDevice, FVulkanCommandListContext& Context)
{
	check(AliasedTexture == nullptr); //can't evict textures we don't own
	const uint64 Size = GetMemorySize();
	static uint64 TotalSize = 0;
	TotalSize += Size;
	if (GVulkanLogDefrag)
	{
		FGenericPlatformMisc::LowLevelOutputDebugStringf(TEXT("Evicted %8.4fkb %8.4fkb   TB %p  :: IMG %p   %-40s\n"), Size / (1024.f), TotalSize / (1024.f), this, Image, *GetName().ToString());
	}

	// Eviction layouts are read from the queue
	const FVulkanImageLayout* OriginalLayout = Context.GetQueue()->GetLayoutManager().GetFullLayout(Image);
	if (OriginalLayout && OriginalLayout->AreAllSubresourcesSameLayout())
	{
		check(0 == CpuReadbackBuffer);
		checkf(MemProps == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TEXT("Can't evict surface that isn't device local.  MemoryProperties=%s"), VK_FLAGS_TO_STRING(VkMemoryPropertyFlags, MemProps));
		checkf(VulkanRHI::GetAspectMaskFromUEFormat(GetDesc().Format, true, true) == FullAspectMask, TEXT("FullAspectMask (%s) does not match with PixelFormat (%d)"), VK_FLAGS_TO_STRING(VkImageAspectFlags, FullAspectMask), (int32)GetDesc().Format);
		checkf(VulkanRHI::GetAspectMaskFromUEFormat(GetDesc().Format, false, true) == PartialAspectMask, TEXT("PartialAspectMask (%s) does not match with PixelFormat (%d)"), VK_FLAGS_TO_STRING(VkImageAspectFlags, PartialAspectMask), (int32)GetDesc().Format);

		const ETextureCreateFlags UEFlags = GetDesc().Flags;
		const bool bRenderTarget = EnumHasAnyFlags(UEFlags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable);
		const bool bUAV = EnumHasAnyFlags(UEFlags, TexCreate_UAV);
		//none of this is supported for eviction
		checkf(!bRenderTarget, TEXT("RenderTargets do not support evict."));
		checkf(!bUAV, TEXT("UAV do not support evict."));
		checkf((OriginalLayout->NumMips == GetNumMips()), TEXT("NumMips reported by LayoutManager (%d) differs from surface (%d)"), OriginalLayout->NumMips, GetNumMips());
		checkf((OriginalLayout->NumLayers == GetNumberOfArrayLevels()), TEXT("NumLayers reported by LayoutManager (%d) differs from surface (%d)"), OriginalLayout->NumLayers, GetNumberOfArrayLevels());

		MemProps = InDevice.GetDeviceMemoryManager().GetEvictedMemoryProperties();

		// Create a new host allocation to move the surface to
		FVulkanAllocation HostAllocation;
		const EVulkanAllocationMetaType MetaType = EVulkanAllocationMetaImageOther;
		if (!InDevice.GetMemoryManager().AllocateImageMemory(HostAllocation, this, MemoryRequirements, MemProps, MetaType, false, __FILE__, __LINE__))
		{
			InDevice.GetMemoryManager().HandleOOM();
			checkNoEntry();
		}

		InternalMoveSurface(InDevice, Context, HostAllocation, OriginalLayout->MainLayout);

		// Delete the original allocation and swap in the new host allocation
		Device->GetMemoryManager().FreeVulkanAllocation(Allocation);
		Allocation.Swap(HostAllocation);

		VULKAN_SET_DEBUG_NAME(InDevice, VK_OBJECT_TYPE_IMAGE, Image, TEXT("(FVulkanTexture*)0x%p [hostimage]"), this);

		InvalidateViews(InDevice);
	}
}


static FCriticalSection ViewCritSection;
void FVulkanTexture::AttachView(FVulkanViewBase* View)
{
	FScopeLock Lock(&ViewCritSection);
	check(View->NextView == nullptr);
	View->NextView = FirstView;
	FirstView = View;
}

void FVulkanTexture::DetachView(FVulkanViewBase* View)
{
	FScopeLock Lock(&ViewCritSection);
	FVulkanViewBase** NextViewPtr = &FirstView;
	while(*NextViewPtr != View)
	{
		NextViewPtr = &(*NextViewPtr)->NextView;
	}
	*NextViewPtr = View->NextView;
	View->NextView = nullptr;
}

bool FVulkanTexture::GetTextureResourceInfo(FRHIResourceInfo& OutResourceInfo) const
{
	OutResourceInfo = FRHIResourceInfo();
	OutResourceInfo.VRamAllocation.AllocationSize = GetMemorySize();
	return true;
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/

void* FVulkanDynamicRHI::RHILockTextureCubeFace(FRHITextureCube* TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureCubeRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureCubeRHI, MipIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->GetMipSize(MipIndex, BufferSize);
	Texture->GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTextureCubeFace(FRHITextureCube* TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture* Texture = FVulkanTexture::Cast(TextureCubeRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureCubeRHI, MipIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	const FRHITextureDesc& Desc = Texture->GetDesc();
	uint32 MipWidth = FMath::Max<uint32>(Desc.Extent.X >> MipIndex, 0);
	uint32 MipHeight = FMath::Max<uint32>(Desc.Extent.Y >> MipIndex, 0);
	ensure(!(MipHeight == 0 && MipWidth == 0));
	MipWidth = FMath::Max<uint32>(MipWidth, 1);
	MipHeight = FMath::Max<uint32>(MipHeight, 1);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = Texture->GetPartialAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = ArrayIndex * 6 + FaceIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanTexture::InternalLockWrite(Device->GetImmediateContext(), Texture, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(Texture, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FRHITexture* TextureRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_IMAGE_TRACKING_LAYER
	{
		FVulkanTexture* VulkanTexture = FVulkanTexture::Cast(TextureRHI);
		VulkanRHI::BindDebugLabelName(VulkanTexture->Image, Name);
	}
#endif

#if VULKAN_ENABLE_DUMP_LAYER
	{
// TODO: this dies in the printf on android. Needs investigation.
#if !PLATFORM_ANDROID
		FVulkanTexture* VulkanTexture = FVulkanTexture::Cast(TextureRHI);
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::PrintfBegin(*FString::Printf(TEXT("vkDebugMarkerSetObjectNameEXT(0x%p=%s)\n"), VulkanTexture->Image, Name));
#endif
#endif
	}
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
#if 0//VULKAN_SUPPORTS_DEBUG_UTILS
	if (auto* SetDebugName = Device->GetSetDebugName())
	{
		FVulkanTexture* VulkanTexture = FVulkanTexture::Cast(TextureRHI);
		FTCHARToUTF8 Converter(Name);
		VulkanRHI::SetDebugName(SetDebugName, Device->GetInstanceHandle(), VulkanTexture->Image, Converter.Get());
	}
	else
#endif
	if (auto* SetObjectName = Device->GetDebugMarkerSetObjectName())
	{
		FVulkanTexture* VulkanTexture = FVulkanTexture::Cast(TextureRHI);
		FTCHARToUTF8 Converter(Name);
		VulkanRHI::SetDebugMarkerName(SetObjectName, Device->GetInstanceHandle(), VulkanTexture->Image, Converter.Get());
	}
#endif
	FName DebugName(Name);
	TextureRHI->SetName(DebugName);
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_DUMP_LAYER
	//if (Device->SupportsDebugMarkers())
	{
		//if (FRHITexture2D* Tex2d = UnorderedAccessViewRHI->GetTexture2D())
		//{
		//	FVulkanTexture2D* VulkanTexture = (FVulkanTexture2D*)Tex2d;
		//	VkDebugMarkerObjectTagInfoEXT Info;
		//	ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT);
		//	Info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		//	Info.object = VulkanTexture->Image;
		//	vkDebugMarkerSetObjectNameEXT(Device->GetInstanceHandle(), &Info);
		//}
	}
#endif
}


void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FRHITexture2D* TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FRHITexture2D* TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

FDynamicRHI::FRHICalcTextureSizeResult FVulkanDynamicRHI::RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex)
{
	// FIXME: this function ignores FirstMipIndex!

	// Zero out the members which don't affect the size since we'll use this as a key in the map of already computed sizes.
	FRHITextureDesc CleanDesc = Desc;
	CleanDesc.UAVFormat = PF_Unknown;
	CleanDesc.ClearValue = FClearValueBinding::None;
	CleanDesc.ExtData = 0;

	// Adjust number of mips as UTexture can request non-valid # of mips
	CleanDesc.NumMips = (uint8)FMath::Min(FMath::FloorLog2(FMath::Max(CleanDesc.Extent.X, FMath::Max(CleanDesc.Extent.Y, (int32)CleanDesc.Depth))) + 1, (uint32)CleanDesc.NumMips);

	static TMap<FRHITextureDesc, VkMemoryRequirements> TextureSizes;
	static FCriticalSection TextureSizesLock;

	VkMemoryRequirements* Found = nullptr;
	{
		FScopeLock Lock(&TextureSizesLock);
		Found = TextureSizes.Find(CleanDesc);
		if (Found)
		{
			return { (uint64)Found->size, (uint32)Found->alignment };
		}
	}

	// Create temporary image to measure the memory requirements.
	FVulkanTexture::FImageCreateInfo TmpCreateInfo;
	FVulkanTexture::GenerateImageCreateInfo(TmpCreateInfo, *Device, CleanDesc, nullptr, nullptr, false);

	VkMemoryRequirements OutMemReq;

	if (Device->GetOptionalExtensions().HasKHRMaintenance4)
	{
		VkDeviceImageMemoryRequirementsKHR ImageMemReq;
		ZeroVulkanStruct(ImageMemReq, VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS);
		ImageMemReq.pCreateInfo = &TmpCreateInfo.ImageCreateInfo;
		ImageMemReq.planeAspect = (VulkanRHI::GetAspectMaskFromUEFormat(CleanDesc.Format, true, true) == VK_IMAGE_ASPECT_COLOR_BIT) ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;  // should be ignored

		VkMemoryRequirements2KHR MemReq2;
		ZeroVulkanStruct(MemReq2, VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);

		VulkanRHI::vkGetDeviceImageMemoryRequirementsKHR(Device->GetInstanceHandle(), &ImageMemReq, &MemReq2);
		OutMemReq = MemReq2.memoryRequirements;
	}
	else
	{
		VkImage TmpImage;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(Device->GetInstanceHandle(), &TmpCreateInfo.ImageCreateInfo, VULKAN_CPU_ALLOCATOR, &TmpImage));
		VulkanRHI::vkGetImageMemoryRequirements(Device->GetInstanceHandle(), TmpImage, &OutMemReq);
		VulkanRHI::vkDestroyImage(Device->GetInstanceHandle(), TmpImage, VULKAN_CPU_ALLOCATOR);
	}

	{
		FScopeLock Lock(&TextureSizesLock);
		TextureSizes.Add(CleanDesc, OutMemReq);
	}

	return { (uint64)OutMemReq.size, (uint32)OutMemReq.alignment };
}

void FVulkanCommandListContext::RHICopyTexture(FRHITexture* SourceTexture, FRHITexture* DestTexture, const FRHICopyTextureInfo& CopyInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	check(SourceTexture && DestTexture);

	FVulkanTexture* Source = FVulkanTexture::Cast(SourceTexture);
	FVulkanTexture* Dest = FVulkanTexture::Cast(DestTexture);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(CmdBuffer->IsOutsideRenderPass());

	{
		const VkImageLayout ExpectedSrcLayout = FVulkanLayoutManager::SetExpectedLayout(CmdBuffer, *Source, ERHIAccess::CopySrc);
		ensureMsgf((ExpectedSrcLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL), TEXT("Expected source texture to be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, actual layout is %s"), VK_TYPE_TO_STRING(VkImageLayout, ExpectedSrcLayout));
	}

	const FPixelFormatInfo& PixelFormatInfo = GPixelFormats[DestTexture->GetDesc().Format];
	const FRHITextureDesc& SourceDesc = SourceTexture->GetDesc();
	const FRHITextureDesc& DestDesc = DestTexture->GetDesc();
	const FIntVector SourceXYZ = SourceDesc.GetSize();
	const FIntVector DestXYZ = DestDesc.GetSize();

	check(!EnumHasAnyFlags(Source->GetDesc().Flags, TexCreate_CPUReadback));
	if (EnumHasAllFlags(Dest->GetDesc().Flags, TexCreate_CPUReadback))
	{
		checkf(CopyInfo.DestSliceIndex == 0, TEXT("Slices not supported in TexCreate_CPUReadback textures"));
		checkf(CopyInfo.DestPosition.IsZero(), TEXT("Destination position not supported in TexCreate_CPUReadback textures"));
		FIntVector Size = CopyInfo.Size;
		if (Size == FIntVector::ZeroValue)
		{
			ensure(SourceXYZ.X <= DestXYZ.X && SourceXYZ.Y <= DestXYZ.Y);
			Size.X = FMath::Max<uint32>(1u, SourceXYZ.X >> CopyInfo.SourceMipIndex);
			Size.Y = FMath::Max<uint32>(1u, SourceXYZ.Y >> CopyInfo.SourceMipIndex);
			Size.Z = FMath::Max<uint32>(1u, SourceXYZ.Z >> CopyInfo.SourceMipIndex);
		}
		VkBufferImageCopy CopyRegion[MAX_TEXTURE_MIP_COUNT];
		FMemory::Memzero(CopyRegion);

		const FVulkanCpuReadbackBuffer* CpuReadbackBuffer = Dest->GetCpuReadbackBuffer();
		const uint32 SourceSliceIndex = CopyInfo.SourceSliceIndex;
		const uint32 SourceMipIndex = CopyInfo.SourceMipIndex;
		const uint32 DestMipIndex = CopyInfo.DestMipIndex;
		for (uint32 Index = 0; Index < CopyInfo.NumMips; ++Index)
		{
			CopyRegion[Index].bufferOffset = CpuReadbackBuffer->MipOffsets[DestMipIndex + Index];
			CopyRegion[Index].bufferRowLength = Size.X;
			CopyRegion[Index].bufferImageHeight = Size.Y;
			CopyRegion[Index].imageSubresource.aspectMask = Source->GetFullAspectMask();
			CopyRegion[Index].imageSubresource.mipLevel = SourceMipIndex;
			CopyRegion[Index].imageSubresource.baseArrayLayer = SourceSliceIndex;
			CopyRegion[Index].imageSubresource.layerCount = 1;
			CopyRegion[Index].imageOffset.x = CopyInfo.SourcePosition.X;
			CopyRegion[Index].imageOffset.y = CopyInfo.SourcePosition.Y;
			CopyRegion[Index].imageOffset.z = CopyInfo.SourcePosition.Z;
			CopyRegion[Index].imageExtent.width = Size.X;
			CopyRegion[Index].imageExtent.height = Size.Y;
			CopyRegion[Index].imageExtent.depth = Size.Z;

			Size.X = FMath::Max(1, Size.X / 2);
			Size.Y = FMath::Max(1, Size.Y / 2);
			Size.Z = FMath::Max(1, Size.Z / 2);
		}

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Source->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CpuReadbackBuffer->Buffer, CopyInfo.NumMips, &CopyRegion[0]);

		FVulkanPipelineBarrier BarrierMemory;
		BarrierMemory.AddMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT);
		BarrierMemory.Execute(CmdBuffer);
	}
	else
	{
		{
			const VkImageLayout ExpectedDstLayout = FVulkanLayoutManager::SetExpectedLayout(CmdBuffer, *Dest, ERHIAccess::CopyDest);
			ensureMsgf((ExpectedDstLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), TEXT("Expected destination texture to be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, actual layout is %s"), VK_TYPE_TO_STRING(VkImageLayout, ExpectedDstLayout));
		}

		VkImageCopy Region;
		FMemory::Memzero(Region);
		if (CopyInfo.Size == FIntVector::ZeroValue)
		{
			// Copy whole texture when zero vector is specified for region size
			Region.extent.width  = FMath::Max<uint32>(PixelFormatInfo.BlockSizeX, SourceXYZ.X >> CopyInfo.SourceMipIndex);
			Region.extent.height = FMath::Max<uint32>(PixelFormatInfo.BlockSizeY, SourceXYZ.Y >> CopyInfo.SourceMipIndex);
			Region.extent.depth  = FMath::Max<uint32>(PixelFormatInfo.BlockSizeZ, SourceXYZ.Z >> CopyInfo.SourceMipIndex);
			ensure(Region.extent.width <= (uint32)DestXYZ.X && Region.extent.height <= (uint32)DestXYZ.Y);
		}
		else
		{
			ensure(CopyInfo.Size.X > 0 && CopyInfo.Size.X <= DestXYZ.X && CopyInfo.Size.Y > 0 && CopyInfo.Size.Y <= DestXYZ.Y);
			Region.extent.width  = FMath::Max(PixelFormatInfo.BlockSizeX, CopyInfo.Size.X);
			Region.extent.height = FMath::Max(PixelFormatInfo.BlockSizeY, CopyInfo.Size.Y);
			Region.extent.depth  = FMath::Max(PixelFormatInfo.BlockSizeZ, CopyInfo.Size.Z);
		}
		Region.srcSubresource.aspectMask = Source->GetFullAspectMask();
		Region.srcSubresource.baseArrayLayer = CopyInfo.SourceSliceIndex;
		Region.srcSubresource.layerCount = CopyInfo.NumSlices;
		Region.srcSubresource.mipLevel = CopyInfo.SourceMipIndex;
		Region.srcOffset.x = CopyInfo.SourcePosition.X;
		Region.srcOffset.y = CopyInfo.SourcePosition.Y;
		Region.srcOffset.z = CopyInfo.SourcePosition.Z;
		Region.dstSubresource.aspectMask = Dest->GetFullAspectMask();
		Region.dstSubresource.baseArrayLayer = CopyInfo.DestSliceIndex;
		Region.dstSubresource.layerCount = CopyInfo.NumSlices;
		Region.dstSubresource.mipLevel = CopyInfo.DestMipIndex;
		Region.dstOffset.x = CopyInfo.DestPosition.X;
		Region.dstOffset.y = CopyInfo.DestPosition.Y;
		Region.dstOffset.z = CopyInfo.DestPosition.Z;

		for (uint32 Index = 0; Index < CopyInfo.NumMips; ++Index)
		{
			VulkanRHI::vkCmdCopyImage(CmdBuffer->GetHandle(),
				Source->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				Dest->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &Region);

			++Region.srcSubresource.mipLevel;
			++Region.dstSubresource.mipLevel;

			// Scale down the copy region if there is another mip to proceed.
			if (Index != CopyInfo.NumMips - 1)
			{
				Region.srcOffset.x /= 2;
				Region.srcOffset.y /= 2;
				Region.srcOffset.z /= 2;

				Region.dstOffset.x /= 2;
				Region.dstOffset.y /= 2;
				Region.dstOffset.z /= 2;

				Region.extent.width  = FMath::Max<uint32>(Region.extent.width  / 2, PixelFormatInfo.BlockSizeX);
				Region.extent.height = FMath::Max<uint32>(Region.extent.height / 2, PixelFormatInfo.BlockSizeY);
				Region.extent.depth  = FMath::Max<uint32>(Region.extent.depth  / 2, PixelFormatInfo.BlockSizeZ);

				// RHICopyTexture is allowed to copy mip regions only if are aligned on the block size to prevent unexpected / inconsistent results.
				ensure(Region.srcOffset.x % PixelFormatInfo.BlockSizeX == 0 && Region.srcOffset.y % PixelFormatInfo.BlockSizeY == 0 && Region.srcOffset.z % PixelFormatInfo.BlockSizeZ == 0);
				ensure(Region.dstOffset.x % PixelFormatInfo.BlockSizeX == 0 && Region.dstOffset.y % PixelFormatInfo.BlockSizeY == 0 && Region.dstOffset.z % PixelFormatInfo.BlockSizeZ == 0);
				// For extent, the condition is harder to verify since on Vulkan, the extent must not be aligned on block size if it would exceed the surface limit.
			}
		}
	}
}

void FVulkanCommandListContext::RHICopyBufferRegion(FRHIBuffer* DstBuffer, uint64 DstOffset, FRHIBuffer* SrcBuffer, uint64 SrcOffset, uint64 NumBytes)
{
	if (!DstBuffer || !SrcBuffer || DstBuffer == SrcBuffer || !NumBytes)
	{
		return;
	}

	FVulkanResourceMultiBuffer* DstBufferVk = ResourceCast(DstBuffer);
	FVulkanResourceMultiBuffer* SrcBufferVk = ResourceCast(SrcBuffer);

	check(DstBufferVk && SrcBufferVk);
	check(DstOffset + NumBytes <= DstBuffer->GetSize() && SrcOffset + NumBytes <= SrcBuffer->GetSize());

	uint64 DstOffsetVk = DstBufferVk->GetOffset() + DstOffset;
	uint64 SrcOffsetVk = SrcBufferVk->GetOffset() + SrcOffset;

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(CmdBuffer->IsOutsideRenderPass());
	VkCommandBuffer VkCmdBuffer = CmdBuffer->GetHandle();

	VkMemoryBarrier BarrierBefore = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT| VK_ACCESS_TRANSFER_WRITE_BIT };
	VulkanRHI::vkCmdPipelineBarrier(VkCmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &BarrierBefore, 0, nullptr, 0, nullptr);

	VkBufferCopy Region = {};
	Region.srcOffset = SrcOffsetVk;
	Region.dstOffset = DstOffsetVk;
	Region.size = NumBytes;
	VulkanRHI::vkCmdCopyBuffer(VkCmdBuffer, SrcBufferVk->GetHandle(), DstBufferVk->GetHandle(), 1, &Region);

	VkMemoryBarrier BarrierAfter = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
	VulkanRHI::vkCmdPipelineBarrier(VkCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &BarrierAfter, 0, nullptr, 0, nullptr);
}
