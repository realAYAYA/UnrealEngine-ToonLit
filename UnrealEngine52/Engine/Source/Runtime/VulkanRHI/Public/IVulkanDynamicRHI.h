// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	#include <windows.h>
	#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
	#define VK_NO_PROTOTYPES
	#include <vulkan.h>
THIRD_PARTY_INCLUDES_END

struct FVulkanRHIAllocationInfo
{
	VkDeviceMemory Handle{};
	uint64 Offset{};
	uint64 Size{};
};

struct FVulkanRHIImageViewInfo
{
	VkImageView ImageView;
	VkImage Image;
	VkImageSubresourceRange SubresourceRange;
	VkFormat Format;
	uint32 Width;
	uint32 Height;
	uint32 Depth;
	ETextureCreateFlags UEFlags;
};

struct IVulkanDynamicRHI : public FDynamicRHI
{
	virtual ERHIInterfaceType GetInterfaceType() const override { return ERHIInterfaceType::Vulkan; }

	virtual uint32           RHIGetVulkanVersion() const = 0;
	virtual VkInstance       RHIGetVkInstance() const = 0;
	virtual VkDevice         RHIGetVkDevice() const = 0;
	virtual const uint8*     RHIGetVulkanDeviceUUID() const = 0;
	virtual VkPhysicalDevice RHIGetVkPhysicalDevice() const = 0;
	virtual const VkAllocationCallbacks* RHIGetVkAllocationCallbacks() = 0;

	virtual VkQueue          RHIGetGraphicsVkQueue() const = 0;
	virtual uint32           RHIGetGraphicsQueueIndex() const = 0;
	virtual uint32           RHIGetGraphicsQueueFamilyIndex() const = 0;

	virtual VkCommandBuffer  RHIGetActiveVkCommandBuffer() = 0;

	virtual uint64           RHIGetGraphicsAdapterLUID(VkPhysicalDevice InPhysicalDevice) const = 0;
	virtual bool             RHIDoesAdapterMatchDevice(const void* InAdapterId) const = 0;
	virtual void*            RHIGetVkDeviceProcAddr(const char* InName) const = 0;
	virtual VkFormat         RHIGetSwapChainVkFormat(EPixelFormat InFormat) const = 0;
	virtual bool             RHISupportsEXTFragmentDensityMap2() const = 0;

	virtual TArray<VkExtensionProperties> RHIGetAllInstanceExtensions() const = 0;
	virtual TArray<VkExtensionProperties> RHIGetAllDeviceExtensions(VkPhysicalDevice InPhysicalDevice) const = 0;

	virtual FTexture2DRHIRef      RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding = FClearValueBinding::Transparent) = 0;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding = FClearValueBinding::Transparent) = 0;
	virtual FTextureCubeRHIRef    RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding = FClearValueBinding::Transparent) = 0;

	virtual VkImage                  RHIGetVkImage(FRHITexture* InTexture) const = 0;
	virtual VkFormat                 RHIGetViewVkFormat(FRHITexture* InTexture) const = 0;
	virtual FVulkanRHIAllocationInfo RHIGetAllocationInfo(FRHITexture* InTexture) const = 0;
	virtual FVulkanRHIImageViewInfo  RHIGetImageViewInfo(FRHITexture* InTexture) const = 0;

	UE_DEPRECATED(5.2, "Altering image layout tracking directly is not permitted anymore.")
	virtual VkImageLayout& RHIFindOrAddLayoutRW(FRHITexture* InTexture, VkImageLayout LayoutIfNotFound) = 0;

	virtual void           RHISetImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange) = 0;
	virtual void           RHISetUploadImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange) = 0;

	virtual void           RHIFinishExternalComputeWork(VkCommandBuffer InCommandBuffer) = 0;
	virtual void           RHIRegisterWork(uint32 NumPrimitives) = 0;
	virtual void           RHISubmitUploadCommandBuffer() = 0;

	virtual void           RHIVerifyResult(VkResult Result, const ANSICHAR* VkFuntion, const ANSICHAR* Filename, uint32 Line) = 0;

	static VULKANRHI_API void AddEnabledInstanceExtensionsAndLayers(TArrayView<const ANSICHAR* const> InInstanceExtensions, TArrayView<const ANSICHAR* const> InInstanceLayers);
	static VULKANRHI_API void AddEnabledDeviceExtensionsAndLayers(TArrayView<const ANSICHAR* const> InDeviceExtensions, TArrayView<const ANSICHAR* const> InDeviceLayers);
};

inline IVulkanDynamicRHI* GetIVulkanDynamicRHI()
{
	checkf(GDynamicRHI, TEXT("Tried to fetch RHI too early"));
	check(GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Vulkan);
	return GetDynamicRHI<IVulkanDynamicRHI>();
}

#define VERIFYVULKANRESULT_EXTERNAL(VkFunction) { const VkResult ScopedResult = VkFunction; if (ScopedResult != VK_SUCCESS) { GetIVulkanDynamicRHI()->RHIVerifyResult(ScopedResult, #VkFunction, __FILE__, __LINE__); }}
