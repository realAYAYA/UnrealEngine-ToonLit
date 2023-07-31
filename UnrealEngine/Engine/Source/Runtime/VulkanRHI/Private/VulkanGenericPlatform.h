// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "Containers/ArrayView.h"
#include "RHI.h"	// for GShaderPlatformForFeatureLevel and its friends

#include "VulkanLoader.h"

struct FOptionalVulkanDeviceExtensions;
class FVulkanDevice;
class FVulkanRenderTargetLayout;
struct FGfxPipelineDesc;

using FVulkanDeviceExtensionArray = TArray<TUniquePtr<class FVulkanDeviceExtension>>;
using FVulkanInstanceExtensionArray = TArray<TUniquePtr<class FVulkanInstanceExtension>>;

// the platform interface, and empty implementations for platforms that don't need em
class FVulkanGenericPlatform
{
public:
	static void SetupMaxRHIFeatureLevelAndShaderPlatform(ERHIFeatureLevel::Type InRequestedFeatureLevel);

	static bool IsSupported() { return true; }

	static bool LoadVulkanLibrary() { return true; }
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance) { return true; }
	static void FreeVulkanLibrary() {}

	// Called after querying all the available extensions and layers
	static void NotifyFoundInstanceLayersAndExtensions(const TArray<const ANSICHAR*>& Layers, const TArray<const ANSICHAR*>& Extensions) {}
	static void NotifyFoundDeviceLayersAndExtensions(VkPhysicalDevice PhysicalDevice, const TArray<const ANSICHAR*>& Layers, const TArray<const ANSICHAR*>& Extensions) {}

	// Array of required extensions for the platform (Required!)
	static void GetInstanceExtensions(FVulkanInstanceExtensionArray& OutExtensions);
	static void GetInstanceLayers(TArray<const ANSICHAR*>& OutLayers) {}
	static void GetDeviceExtensions(FVulkanDevice* Device, FVulkanDeviceExtensionArray& OutExtensions);
	static void GetDeviceLayers(TArray<const ANSICHAR*>& OutLayers) {}

	// create the platform-specific surface object - required
	static void CreateSurface(VkSurfaceKHR* OutSurface);

	// most platforms support BC* but not ASTC* or ETC2*
	static bool SupportsBCTextureFormats() { return true; }
	static bool SupportsASTCTextureFormats() { return false; }
	static bool SupportsETC2TextureFormats() { return false; }

	// most platforms can query the surface for the present mode, and size, etc
	static bool SupportsQuerySurfaceProperties() { return true; }

	static void SetupFeatureLevels();

	static bool SupportsTimestampRenderQueries() { return true; }

	static bool RequiresMobileRenderer() { return false; }

	// bInit=1 called at RHI init time, bInit=0 at RHI deinit time
	static void OverridePlatformHandlers(bool bInit) {}

	// Some platforms have issues with the access flags for the Present layout
	static bool RequiresPresentLayoutFix() { return false; }

	static bool ForceEnableDebugMarkers() { return false; }

	static bool SupportsDeviceLocalHostVisibleWithNoPenalty(EGpuVendorId VendorId) { return false; }

	static bool HasUnifiedMemory() { return false; }

	static bool RegisterGPUWork() { return true; }

	static void WriteCrashMarker(const FOptionalVulkanDeviceExtensions& OptionalExtensions, VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding) {}

	// Allow the platform code to restrict the device features
	static void RestrictEnabledPhysicalDeviceFeatures(VkPhysicalDeviceFeatures& InOutFeaturesToEnable)
	{ 
		// Disable everything sparse-related
		InOutFeaturesToEnable.shaderResourceResidency	= VK_FALSE;
		InOutFeaturesToEnable.shaderResourceMinLod		= VK_FALSE;
		InOutFeaturesToEnable.sparseBinding				= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyBuffer		= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyImage2D	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyImage3D	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidency2Samples	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidency4Samples	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidency8Samples	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyAliased	= VK_FALSE;
	}

	static bool SupportParallelRenderingTasks() { return true; }

	/** The status quo is false, so the default is chosen to not change it. As platforms opt in it may be better to flip the default. */
	static bool SupportsDynamicResolution() { return false; }

	static bool SupportsVolumeTextureRendering() { return true; }

	// Allow platforms to add extension features to the DeviceInfo pNext chain
	static void EnablePhysicalDeviceFeatureExtensions(VkDeviceCreateInfo& DeviceInfo, FVulkanDevice& Device) {}

	static bool RequiresSwapchainGeneralInitialLayout() { return false; }

	// Allow platforms to perform their own frame pacing, called before Present. Returns true if the platform has done framepacing, false otherwise.
	static bool FramePace(FVulkanDevice& Device, VkSwapchainKHR Swapchain, uint32 PresentID, VkPresentInfoKHR& Info) { return false; }

	// Allow platforms to do extra work on present
	static VkResult Present(VkQueue Queue, VkPresentInfoKHR& PresentInfo);

	// Allow platforms to track swapchain creation
	static VkResult CreateSwapchainKHR(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain);
	
	// Allow platforms to track swapchain destruction
	static void DestroySwapchainKHR(VkDevice Device, VkSwapchainKHR Swapchain, const VkAllocationCallbacks* Allocator);

	// Whether to attempt recreate swapchain when present or acqurire operations fail
	static bool RecreateSwapchainOnFail() { return true; }

	// Ensure the last frame completed on the GPU
	static bool RequiresWaitingForFrameCompletionEvent() { return true; }

	// Does the platform allow a nullptr Pixelshader on the pipeline
	static bool SupportsNullPixelShader() { return true; }

	// Does the platform require depth to be written on stencil clear
	static bool RequiresDepthWriteOnStencilClear() { return false; }

	// Checks if the PSO cache matches the expected vulkan device properties
	static bool PSOBinaryCacheMatches(FVulkanDevice* Device, const TArray<uint8>& DeviceCache);

	// Will create the correct format from a generic pso filename
	static FString CreatePSOBinaryCacheFilename(FVulkanDevice* Device, FString CacheFilename);

	// Gathers a list of pso cache filenames to attempt to load
	static TArray<FString> GetPSOCacheFilenames();

	// Gives platform a chance to handle precompile of PSOs, returns nullptr if unsupported
	static VkPipelineCache PrecompilePSO(FVulkanDevice* Device, VkGraphicsPipelineCreateInfo* PipelineInfo, FGfxPipelineDesc* GfxEntry, const FVulkanRenderTargetLayout* RTLayout, TArrayView<uint32_t> VS, TArrayView<uint32_t> PS, size_t& AfterSize) { return VK_NULL_HANDLE; }

	// Return VK_FALSE if platform wants to suppress the given debug report from the validation layers, VK_TRUE to print it.
	static VkBool32 DebugReportFunction(VkDebugReportFlagsEXT MsgFlags, VkDebugReportObjectTypeEXT ObjType, uint64_t SrcObject, size_t Location, int32 MsgCode, const ANSICHAR* LayerPrefix, const ANSICHAR* Msg, void* UserData) { return VK_TRUE; }

	// Setup platform to use a workaround to reduce textures memory requirements
	static void SetupImageMemoryRequirementWorkaround(const FVulkanDevice& InDevice) {};
	static void SetImageMemoryRequirementWorkaround(VkImageCreateInfo& ImageCreateInfo) {};
};
