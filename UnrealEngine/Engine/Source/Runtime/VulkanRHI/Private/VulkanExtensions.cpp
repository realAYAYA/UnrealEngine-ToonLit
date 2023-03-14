// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanRHIPrivate.h"
#include "VulkanExtensions.h"

#include "IHeadMountedDisplayModule.h"
#include "IHeadMountedDisplayVulkanExtensions.h"


// ADDING A NEW EXTENSION:
// 
// A - If the extension simply needs to be queried for driver support and added at device creation (and set a flag):
//     Add an FVulkanDeviceExtension/FVulkanInstanceExtension directly in the array returned by GetUESupportedDeviceExtensions/GetUESupportedInstanceExtensions.
// 
// B - The extension requires the use of specialized Feature and/or Property structures, or other extended processing:
//     * Add a [instance/device] class for your extension, try to call it FVulkan[ExtensionNameInCamelCaps]Extension so we have consistency
//     * Feed it the extension name string, use the SDK's define if possible : VK_[BLA_BLA_BLA]_EXTENSION_NAME
//     * If there is a VULKAN_SUPPORTS_* define that enables/disables your extension in code, provide it so that the extension knows if it should be used or not.
//       We don't IFDEF the extensions code so that we still know of its existence and we can report warnings if someone tries to use a disabled extension.
//       If there is no define to enable/disable the extension in code, then simply use VULKAN_EXTENSION_ENABLED.
// 
// Tips:
// - Feature and Property structures specific to extensions that won't be needed beyond init should be included in the extension's class instead of the device.
// - To add engine support for a complex extension, but require some kind of external activation (eg plugin) you can use its EExtensionActivation state (see header definition).
// - If an extension is supported on multiple platforms, it may be cleaner to include it here and simply disable its VULKAN_SUPPORTS_* value in the Vulkan platform header where it's not supported.



TAutoConsoleVariable<int32> GRHIAllow64bitShaderAtomicsCvar(
	TEXT("r.Vulkan.Allow64bitShaderAtomics"),
	1,
	TEXT("Whether to enable 64bit buffer/image atomics required by Nanite\n")
	TEXT("0 to disable 64bit atomics\n")
	TEXT("1 to enable (default)"),
	ECVF_ReadOnly
);

TAutoConsoleVariable<int32> GVulkanRayTracingCVar(
	TEXT("r.Vulkan.RayTracing"),
	1,
	TEXT("0: Do not enable Vulkan ray tracing extensions\n")
	TEXT("1: Enable experimental ray tracing support (default)"),
	ECVF_ReadOnly
);

TAutoConsoleVariable<int32> GVulkanAllowHostQueryResetCVar(
	TEXT("r.Vulkan.AllowHostQueryReset"),
	1,
	TEXT("0: Do not enable support for Host Query Reset extension\n")
	TEXT("1: Enable Host Query Reset (default)"),
	ECVF_ReadOnly
);


#if VULKAN_HAS_DEBUGGING_ENABLED
extern TAutoConsoleVariable<int32> GGPUValidationCvar;
#endif

TSharedPtr<IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe> FVulkanDynamicRHI::HMDVulkanExtensions;
TArray<const ANSICHAR*> FVulkanDeviceExtension::ExternalExtensions;
TArray<const ANSICHAR*> FVulkanInstanceExtension::ExternalExtensions;


template <typename ExistingChainType, typename NewStructType>
static void AddToPNext(ExistingChainType& Existing, NewStructType& Added)
{
	Added.pNext = (void*)Existing.pNext;
	Existing.pNext = (void*)&Added;
}


#define VERIFYVULKANRESULT_INIT(VkFunction)	{ const VkResult ScopedResult = VkFunction; \
												if (ScopedResult == VK_ERROR_INITIALIZATION_FAILED) { \
													UE_LOG(LogVulkanRHI, Error, \
													TEXT("%s failed\n at %s:%u\nThis typically means Vulkan is not properly set up in your system; try running vulkaninfo from the Vulkan SDK."), \
													ANSI_TO_TCHAR(#VkFunction), ANSI_TO_TCHAR(__FILE__), __LINE__); } \
												else if (ScopedResult < VK_SUCCESS) { \
													VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); }}


TArray<VkExtensionProperties> FVulkanDeviceExtension::GetDriverSupportedDeviceExtensions(VkPhysicalDevice Gpu, const ANSICHAR* LayerName)
{
	TArray<VkExtensionProperties> OutDeviceExtensions;
	uint32 Count = 0;
	VERIFYVULKANRESULT_INIT(VulkanRHI::vkEnumerateDeviceExtensionProperties(Gpu, LayerName, &Count, nullptr));
	if (Count > 0)
	{
		OutDeviceExtensions.AddZeroed(Count);
		VERIFYVULKANRESULT_INIT(VulkanRHI::vkEnumerateDeviceExtensionProperties(Gpu, LayerName, &Count, OutDeviceExtensions.GetData()));
	}
	OutDeviceExtensions.Sort([](const VkExtensionProperties& A, const VkExtensionProperties& B) { return FCStringAnsi::Strcmp(A.extensionName, B.extensionName) < 0; });
	return OutDeviceExtensions;
}

TArray<VkExtensionProperties> FVulkanInstanceExtension::GetDriverSupportedInstanceExtensions(const ANSICHAR* LayerName)
{
	TArray<VkExtensionProperties> OutInstanceExtensions;
	uint32 Count = 0;
	VERIFYVULKANRESULT_INIT(VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, nullptr));
	if (Count > 0)
	{
		OutInstanceExtensions.AddZeroed(Count);
		VERIFYVULKANRESULT_INIT(VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, OutInstanceExtensions.GetData()));
	}
	OutInstanceExtensions.Sort([](const VkExtensionProperties& A, const VkExtensionProperties& B) { return FCStringAnsi::Strcmp(A.extensionName, B.extensionName) < 0; });
	return OutInstanceExtensions;
}

#undef VERIFYVULKANRESULT_INIT




// *** Vulkan Device Extension support ***
// Typical flow:
// 1- For the selected rendering device, the engine will query the supported extensions (FVulkanDeviceExtension are constructed and support is queried)
// 2- Followed by a query of Physical Device Features (PrePhysicalDeviceFeatures, PostPhysicalDeviceFeatures)
// 3- Followed by a query of Physical Device Properties (PrePhysicalDeviceProperties, PostPhysicalDeviceProperties)
// 4- Finally, the device is created (PreCreateDevice)



// ***** VK_KHR_maintenance4
class FVulkanKHRMaintenance4Extension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRMaintenance4Extension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VK_API_VERSION_1_3)
	{}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(Maintenance4Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR);
		AddToPNext(PhysicalDeviceFeatures2, Maintenance4Features);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasKHRMaintenance4 = (Maintenance4Features.maintenance4 == VK_TRUE) ? 1 : 0; 
	}

private:
	VkPhysicalDeviceMaintenance4FeaturesKHR Maintenance4Features;
};



// ***** VK_KHR_driver_properties
class FVulkanKHRDriverPropertiesExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRDriverPropertiesExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, VULKAN_SUPPORTS_DRIVER_PROPERTIES, VK_API_VERSION_1_2)
	{}

	virtual void PrePhysicalDeviceProperties(VkPhysicalDeviceProperties2KHR& PhysicalDeviceProperties2) override final
	{
		ZeroVulkanStruct(PhysicalDeviceDriverProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR);
		AddToPNext(PhysicalDeviceProperties2, PhysicalDeviceDriverProperties);
	}

	virtual void PostPhysicalDeviceProperties() override final
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Vulkan Physical Device Driver Properties:"));
		UE_LOG(LogVulkanRHI, Display, TEXT("- driverName: %s"), ANSI_TO_TCHAR(PhysicalDeviceDriverProperties.driverName));
		UE_LOG(LogVulkanRHI, Display, TEXT("- driverInfo: %s"), ANSI_TO_TCHAR(PhysicalDeviceDriverProperties.driverInfo));
	}

private:
	VkPhysicalDeviceDriverPropertiesKHR PhysicalDeviceDriverProperties;
};



// ***** VK_KHR_shader_atomic_int64
class FVulkanShaderAtomicInt64Extension : public FVulkanDeviceExtension
{
public:

	FVulkanShaderAtomicInt64Extension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME, VULKAN_SUPPORTS_BUFFER_64BIT_ATOMICS, VK_API_VERSION_1_2)
	{
		bEnabledInCode = bEnabledInCode && (GRHIAllow64bitShaderAtomicsCvar.GetValueOnAnyThread() != 0);
	}

#if VULKAN_SUPPORTS_BUFFER_64BIT_ATOMICS
	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(BufferAtomicFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR);
		AddToPNext(PhysicalDeviceFeatures2, BufferAtomicFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasKHRShaderAtomicInt64 = BufferAtomicFeatures.shaderBufferInt64Atomics;
		GRHISupportsAtomicUInt64 = (BufferAtomicFeatures.shaderBufferInt64Atomics == VK_TRUE);
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (BufferAtomicFeatures.shaderBufferInt64Atomics == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, BufferAtomicFeatures);
		}
		else
		{
			GRHISupportsAtomicUInt64 = false;
		}
	}

	VkPhysicalDeviceShaderAtomicInt64Features BufferAtomicFeatures;
#endif // VULKAN_SUPPORTS_BUFFER_64BIT_ATOMICS
};



// ***** VK_EXT_shader_image_atomic_int64
class FVulkanShaderImageAtomicInt64Extension : public FVulkanDeviceExtension
{
public:

	FVulkanShaderImageAtomicInt64Extension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, VULKAN_SUPPORTS_IMAGE_64BIT_ATOMICS)
	{
		bEnabledInCode = bEnabledInCode && (GRHIAllow64bitShaderAtomicsCvar.GetValueOnAnyThread() != 0);
	}

#if VULKAN_SUPPORTS_IMAGE_64BIT_ATOMICS
	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(ImageAtomicFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT);
		AddToPNext(PhysicalDeviceFeatures2, ImageAtomicFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasImageAtomicInt64 = ImageAtomicFeatures.shaderImageInt64Atomics;
		GRHISupportsAtomicUInt64 = GRHISupportsAtomicUInt64 && (ImageAtomicFeatures.shaderImageInt64Atomics == VK_TRUE); // GRHISupportsAtomicUInt64 already contains FVulkanShaderAtomicInt64Extension support
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		const bool bEnable64bitAtomics = (GRHIAllow64bitShaderAtomicsCvar.GetValueOnAnyThread() != 0);
		if (bEnable64bitAtomics && (ImageAtomicFeatures.shaderImageInt64Atomics == VK_TRUE))
		{
			ImageAtomicFeatures.pNext = (void*)DeviceCreateInfo.pNext;
			DeviceCreateInfo.pNext = &ImageAtomicFeatures;
		}
		else
		{
			GRHISupportsAtomicUInt64 = false;
		}
	}

	VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT ImageAtomicFeatures;
#endif // VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2
};



// ***** VK_EXT_shader_viewport_index_layer
class FVulkanEXTShaderViewportIndexLayerExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTShaderViewportIndexLayerExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME, VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER, VK_API_VERSION_1_2)
	{}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasEXTShaderViewportIndexLayer = 1; 

		GRHISupportsArrayIndexFromAnyShader = true;
	}
};



// ***** VK_KHR_separate_depth_stencil_layouts
class FVulkanKHRSeparateDepthStencilLayoutsExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRSeparateDepthStencilLayoutsExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME, VULKAN_SUPPORTS_SEPARATE_DEPTH_STENCIL_LAYOUTS, VK_API_VERSION_1_2)
	{
		// Disabled but kept for reference. The barriers code doesn't currently use separate transitions for depth and stencil.
		bEnabledInCode = false;
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(SeparateDepthStencilLayoutsFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR);
		AddToPNext(PhysicalDeviceFeatures2, SeparateDepthStencilLayoutsFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasSeparateDepthStencilLayouts = SeparateDepthStencilLayoutsFeatures.separateDepthStencilLayouts;
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (SeparateDepthStencilLayoutsFeatures.separateDepthStencilLayouts == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, SeparateDepthStencilLayoutsFeatures);
		}
	}

private:
	VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR SeparateDepthStencilLayoutsFeatures;
};



// ***** VK_KHR_multiview
class FVulkanKHRMultiviewExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRMultiviewExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_MULTIVIEW_EXTENSION_NAME, VULKAN_SUPPORTS_MULTIVIEW, VK_API_VERSION_1_1)
	{}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(DeviceMultiviewFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
		AddToPNext(PhysicalDeviceFeatures2, DeviceMultiviewFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final 
	{
		ExtensionFlags.HasKHRMultiview = DeviceMultiviewFeatures.multiview; 
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (DeviceMultiviewFeatures.multiview == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, DeviceMultiviewFeatures);
		}
	}

private:
	VkPhysicalDeviceMultiviewFeatures DeviceMultiviewFeatures;
};



// ***** VK_EXT_scalar_block_layout
class FVulkanEXTScalarBlockLayoutExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTScalarBlockLayoutExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT, VK_API_VERSION_1_2)
	{}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(ScalarBlockLayoutFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT);
		AddToPNext(PhysicalDeviceFeatures2, ScalarBlockLayoutFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final 
	{
		ExtensionFlags.HasEXTScalarBlockLayout = ScalarBlockLayoutFeatures.scalarBlockLayout; 
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (ScalarBlockLayoutFeatures.scalarBlockLayout == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, ScalarBlockLayoutFeatures);
		}
	}

private:
	VkPhysicalDeviceScalarBlockLayoutFeaturesEXT ScalarBlockLayoutFeatures;
};



// ***** VK_EXT_descriptor_indexing
class FVulkanEXTDescriptorIndexingExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTDescriptorIndexingExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VULKAN_SUPPORTS_DESCRIPTOR_INDEXING, VK_API_VERSION_1_2)
	{}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(DescriptorIndexingFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
		AddToPNext(PhysicalDeviceFeatures2, DescriptorIndexingFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasEXTDescriptorIndexing = DescriptorIndexingFeatures.runtimeDescriptorArray; // :todo-jn: add resource specific checks
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (DescriptorIndexingFeatures.runtimeDescriptorArray == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, DescriptorIndexingFeatures);
		}
	}

private:
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT DescriptorIndexingFeatures;
};



// ***** VK_KHR_fragment_shading_rate
class FVulkanKHRFragmentShadingRateExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRFragmentShadingRateExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE)
	{
	}

	virtual void PrePhysicalDeviceProperties(VkPhysicalDeviceProperties2KHR& PhysicalDeviceProperties2) override final
	{
		ZeroVulkanStruct(FragmentShadingRateProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR);
		AddToPNext(PhysicalDeviceProperties2, FragmentShadingRateProperties);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasKHRFragmentShadingRate = FragmentShadingRateFeatures.attachmentFragmentShadingRate;

		GRHISupportsAttachmentVariableRateShading = FragmentShadingRateFeatures.attachmentFragmentShadingRate ? true : false;
		GRHISupportsPipelineVariableRateShading = FragmentShadingRateFeatures.pipelineFragmentShadingRate ? true : false;

		if (FragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE)
		{
			GRHIVariableRateShadingImageDataType = VRSImage_Palette;
			GRHIVariableRateShadingImageFormat = PF_R8_UINT;
		}
		else
		{
			GRHIVariableRateShadingImageDataType = VRSImage_NotSupported;
			GRHIVariableRateShadingImageFormat = PF_Unknown;
		}
	}

	virtual void PostPhysicalDeviceProperties() override final
	{
		GRHIVariableRateShadingImageTileMinWidth = FragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.width;
		GRHIVariableRateShadingImageTileMinHeight = FragmentShadingRateProperties.minFragmentShadingRateAttachmentTexelSize.height;
		GRHIVariableRateShadingImageTileMaxWidth = FragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.width;
		GRHIVariableRateShadingImageTileMaxHeight = FragmentShadingRateProperties.maxFragmentShadingRateAttachmentTexelSize.height;

		if (FragmentShadingRateProperties.maxFragmentSize.width >= 4 && FragmentShadingRateProperties.maxFragmentSize.height >= 4)
		{
			// FYI FVulkanDevice::GetBestMatchedShadingRateExtents does extent filtering
			GRHISupportsLargerVariableRateShadingSizes = GRHISupportsPipelineVariableRateShading;
		}

		// todo: We don't currently care much about the other properties here, but at some point in the future we probably will.

		UE_LOG(LogVulkanRHI, Verbose, TEXT("Image-based Variable Rate Shading supported via KHRFragmentShadingRate extension. Selected VRS tile size %u by %u pixels per VRS image texel."), GRHIVariableRateShadingImageTileMinWidth, GRHIVariableRateShadingImageTileMinHeight);
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(FragmentShadingRateFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR);
		AddToPNext(PhysicalDeviceFeatures2, FragmentShadingRateFeatures);
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (FragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, FragmentShadingRateFeatures);
		}
	}

private:
	VkPhysicalDeviceFragmentShadingRatePropertiesKHR FragmentShadingRateProperties;
	VkPhysicalDeviceFragmentShadingRateFeaturesKHR FragmentShadingRateFeatures;
};



// ***** VK_EXT_fragment_density_map
class FVulkanEXTFragmentDensityMapExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTFragmentDensityMapExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP)
	{
	}

	virtual void PrePhysicalDeviceProperties(VkPhysicalDeviceProperties2KHR& PhysicalDeviceProperties2) override final
	{
		ZeroVulkanStruct(FragmentDensityMapProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT);
		AddToPNext(PhysicalDeviceProperties2, FragmentDensityMapProperties);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasEXTFragmentDensityMap = 1;

		// Use the Fragment Density Map extension if and only if the Fragment Shading Rate extension is not available.
		// NOTE: FVulkanKHRFragmentShadingRateExtension must be placed before FVulkanEXTFragmentDensityMapExtension for this to work!
		if (!GRHISupportsAttachmentVariableRateShading && (FragmentDensityMapFeatures.fragmentDensityMap == VK_TRUE))
		{
			GRHISupportsAttachmentVariableRateShading = true;
			GRHISupportsPipelineVariableRateShading = false;

			// Go with the smallest tile size for now, and also force to square, since this seems to be standard.
			// TODO: Eventually we may want to surface the range of possible tile sizes depending on end use cases, but for now this is being used for foveated rendering and smallest tile size
			// is preferred.

			GRHIVariableRateShadingImageTileMinWidth = FragmentDensityMapProperties.minFragmentDensityTexelSize.width;
			GRHIVariableRateShadingImageTileMinHeight = FragmentDensityMapProperties.minFragmentDensityTexelSize.height;
			GRHIVariableRateShadingImageTileMaxWidth = FragmentDensityMapProperties.maxFragmentDensityTexelSize.width;
			GRHIVariableRateShadingImageTileMaxHeight = FragmentDensityMapProperties.maxFragmentDensityTexelSize.height;

			GRHIVariableRateShadingImageDataType = VRSImage_Fractional;
			GRHIVariableRateShadingImageFormat = PF_R8G8;

			// UE_LOG(LogVulkanRHI, Display, TEXT("Image-based Variable Rate Shading supported via EXTFragmentDensityMap extension. Selected VRS tile size %u by %u pixels per VRS image texel."), GRHIVariableRateShadingImageTileMinWidth, GRHIVariableRateShadingImageTileMinHeight);
		}
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(FragmentDensityMapFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT);
		AddToPNext(PhysicalDeviceFeatures2, FragmentDensityMapFeatures);
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (FragmentDensityMapFeatures.fragmentDensityMap == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, FragmentDensityMapFeatures);
		}
	}

private:
	VkPhysicalDeviceFragmentDensityMapPropertiesEXT FragmentDensityMapProperties;
	VkPhysicalDeviceFragmentDensityMapFeaturesEXT FragmentDensityMapFeatures;
};



// ***** VK_EXT_fragment_density_map2
class FVulkanEXTFragmentDensityMap2Extension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTFragmentDensityMap2Extension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME, VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP2)
	{
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(FragmentDensityMap2Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT);
		AddToPNext(PhysicalDeviceFeatures2, FragmentDensityMap2Features);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasEXTFragmentDensityMap2 = FragmentDensityMap2Features.fragmentDensityMapDeferred;

		GRHISupportsLateVariableRateShadingUpdate = (FragmentDensityMap2Features.fragmentDensityMapDeferred == VK_TRUE);
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (FragmentDensityMap2Features.fragmentDensityMapDeferred == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, FragmentDensityMap2Features);
		}
	}

private:
	VkPhysicalDeviceFragmentDensityMap2FeaturesEXT FragmentDensityMap2Features;
};



// ***** VK_KHR_get_memory_requirements2
class FVulkanKHRGetMemoryRequirements2Extension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRGetMemoryRequirements2Extension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VK_API_VERSION_1_1)
	{}
};



// ***** VK_KHR_buffer_device_address
class FVulkanKHRBufferDeviceAddressExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRBufferDeviceAddressExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VULKAN_RHI_RAYTRACING, VK_API_VERSION_1_2) {}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(BufferDeviceAddressFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
		AddToPNext(PhysicalDeviceFeatures2, BufferDeviceAddressFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final 
	{
		ExtensionFlags.HasBufferDeviceAddress = BufferDeviceAddressFeatures.bufferDeviceAddress; 
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (BufferDeviceAddressFeatures.bufferDeviceAddress == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, BufferDeviceAddressFeatures);
		}
	}

private:
	VkPhysicalDeviceBufferDeviceAddressFeaturesKHR BufferDeviceAddressFeatures;
};



// ***** VK_KHR_acceleration_structure
class FVulkanKHRAccelerationStructureExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRAccelerationStructureExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VULKAN_RHI_RAYTRACING)
	{
		bEnabledInCode = bEnabledInCode && GVulkanRayTracingCVar.GetValueOnAnyThread() && !FParse::Param(FCommandLine::Get(), TEXT("noraytracing"));
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(AccelerationStructureFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);;
		AddToPNext(PhysicalDeviceFeatures2, AccelerationStructureFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasAccelerationStructure = 
			AccelerationStructureFeatures.accelerationStructure & 
			AccelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind;
	}

	virtual void PrePhysicalDeviceProperties(VkPhysicalDeviceProperties2KHR& PhysicalDeviceProperties2) override final
	{
#if VULKAN_RHI_RAYTRACING
		const FRayTracingProperties& RayTracingProperties = Device->GetRayTracingProperties();
		VkPhysicalDeviceAccelerationStructurePropertiesKHR& AccelerationStructure = const_cast<VkPhysicalDeviceAccelerationStructurePropertiesKHR&>(RayTracingProperties.AccelerationStructure);
		ZeroVulkanStruct(AccelerationStructure, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR);
		AddToPNext(PhysicalDeviceProperties2, AccelerationStructure);
#endif
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if ((AccelerationStructureFeatures.accelerationStructure == VK_TRUE) &&
			(AccelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE))
		{
			AddToPNext(DeviceCreateInfo, AccelerationStructureFeatures);
		}
	}

private:
	VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures;
};



// ***** VK_KHR_ray_tracing_pipeline
class FVulkanKHRRayTracingPipelineExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRRayTracingPipelineExtension(FVulkanDevice* InDevice) 
		: FVulkanDeviceExtension(InDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VULKAN_RHI_RAYTRACING)
	{
		bEnabledInCode = bEnabledInCode && GVulkanRayTracingCVar.GetValueOnAnyThread() && !FParse::Param(FCommandLine::Get(), TEXT("noraytracing"));
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(RayTracingPipelineFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
		AddToPNext(PhysicalDeviceFeatures2, RayTracingPipelineFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasRayTracingPipeline = RayTracingPipelineFeatures.rayTracingPipeline & RayTracingPipelineFeatures.rayTraversalPrimitiveCulling;
	}

	virtual void PrePhysicalDeviceProperties(VkPhysicalDeviceProperties2KHR& PhysicalDeviceProperties2) override final
	{
#if VULKAN_RHI_RAYTRACING
		const FRayTracingProperties& RayTracingProperties = Device->GetRayTracingProperties();
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR& RayTracingPipeline = const_cast<VkPhysicalDeviceRayTracingPipelinePropertiesKHR&>(RayTracingProperties.RayTracingPipeline);
		ZeroVulkanStruct(RayTracingPipeline, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR);
		AddToPNext(PhysicalDeviceProperties2, RayTracingPipeline);
#endif
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if ((RayTracingPipelineFeatures.rayTracingPipeline == VK_TRUE) &&
			(RayTracingPipelineFeatures.rayTraversalPrimitiveCulling == VK_TRUE))
		{
			AddToPNext(DeviceCreateInfo, RayTracingPipelineFeatures);
		}
	}

private:
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR RayTracingPipelineFeatures;
};



// ***** VK_KHR_ray_query
class FVulkanKHRRayQueryExtension : public FVulkanDeviceExtension
{
public:

	FVulkanKHRRayQueryExtension(FVulkanDevice* InDevice) 
		: FVulkanDeviceExtension(InDevice, VK_KHR_RAY_QUERY_EXTENSION_NAME, VULKAN_RHI_RAYTRACING)
	{
		bEnabledInCode = bEnabledInCode && GVulkanRayTracingCVar.GetValueOnAnyThread() && !FParse::Param(FCommandLine::Get(), TEXT("noraytracing"));
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(RayQueryFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR);
		AddToPNext(PhysicalDeviceFeatures2, RayQueryFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasRayQuery = RayQueryFeatures.rayQuery; 
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (RayQueryFeatures.rayQuery == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, RayQueryFeatures);
		}
	}

private:
	VkPhysicalDeviceRayQueryFeaturesKHR RayQueryFeatures;
};



// ***** VK_EXT_debug_marker
class FVulkanEXTDebugMarkerExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTDebugMarkerExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_DEBUG_MARKER_EXTENSION_NAME, (VULKAN_ENABLE_DRAW_MARKERS & VULKAN_HAS_DEBUGGING_ENABLED))
	{
#if VULKAN_HAS_DEBUGGING_ENABLED
		const int32 VulkanValidationOption = GValidationCvar.GetValueOnAnyThread();
		bEnabledInCode = bEnabledInCode && ((GRenderDocFound || VulkanValidationOption == 0) || FVulkanPlatform::ForceEnableDebugMarkers());
#endif
	}

	virtual void PostPhysicalDeviceProperties() override final
	{
		Device->SetDebugMarkersFound();
	}
};



// ***** VK_AMD_buffer_marker (vendor)
class FVulkanAMDBufferMarkerExtension : public FVulkanDeviceExtension
{
public:

	FVulkanAMDBufferMarkerExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_AMD_BUFFER_MARKER_EXTENSION_NAME, VULKAN_SUPPORTS_AMD_BUFFER_MARKER)
	{
		const bool bAllowVendorDevice = !FParse::Param(FCommandLine::Get(), TEXT("novendordevice"));
		bEnabledInCode = bEnabledInCode && GGPUCrashDebuggingEnabled && bAllowVendorDevice;
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasAMDBufferMarker = 1; 
	}
};



// ***** VK_NV_device_diagnostic_checkpoints (vendor)
class FVulkanNVDeviceDiagnosticCheckpointsExtension : public FVulkanDeviceExtension
{
public:

	FVulkanNVDeviceDiagnosticCheckpointsExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME, VULKAN_SUPPORTS_NV_DIAGNOSTICS)
	{
		const bool bAllowVendorDevice = !FParse::Param(FCommandLine::Get(), TEXT("novendordevice"));
		bEnabledInCode = bEnabledInCode && GGPUCrashDebuggingEnabled && bAllowVendorDevice;
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasNVDiagnosticCheckpoints = 1; 
	}
};



// ***** VK_NV_device_diagnostics_config (vendor)
class FVulkanNVDeviceDiagnosticConfigExtension : public FVulkanDeviceExtension
{
public:

	FVulkanNVDeviceDiagnosticConfigExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME, VULKAN_SUPPORTS_NV_DIAGNOSTICS)
	{
		const bool bAllowVendorDevice = !FParse::Param(FCommandLine::Get(), TEXT("novendordevice"));
		bEnabledInCode = bEnabledInCode && GGPUCrashDebuggingEnabled && bAllowVendorDevice;
	}


	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(DeviceDiagnosticsConfigFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV);
		AddToPNext(PhysicalDeviceFeatures2, DeviceDiagnosticsConfigFeaturesNV);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final 
	{
		ExtensionFlags.HasNVDeviceDiagnosticConfig = DeviceDiagnosticsConfigFeaturesNV.diagnosticsConfig; 
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (DeviceDiagnosticsConfigFeaturesNV.diagnosticsConfig == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, DeviceDiagnosticsConfigFeaturesNV);

			ZeroVulkanStruct(DeviceDiagnosticsConfigCreateInfoNV, VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV);
			DeviceDiagnosticsConfigCreateInfoNV.flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV;
			AddToPNext(DeviceCreateInfo, DeviceDiagnosticsConfigCreateInfoNV);
		}
	}

private:
	VkPhysicalDeviceDiagnosticsConfigFeaturesNV DeviceDiagnosticsConfigFeaturesNV;
	VkDeviceDiagnosticsConfigCreateInfoNV DeviceDiagnosticsConfigCreateInfoNV;
};


// ***** VK_EXT_host_query_reset
class FVulkanEXTHostQueryResetExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTHostQueryResetExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VK_API_VERSION_1_2)
	{
		bEnabledInCode = bEnabledInCode && (GVulkanAllowHostQueryResetCVar.GetValueOnAnyThread() != 0);
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(HostQueryResetFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES);
		AddToPNext(PhysicalDeviceFeatures2, HostQueryResetFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
		ExtensionFlags.HasEXTHostQueryReset = HostQueryResetFeatures.hostQueryReset;
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (HostQueryResetFeatures.hostQueryReset == VK_TRUE)
		{
			AddToPNext(DeviceCreateInfo, HostQueryResetFeatures);
		}
	}

private:
	VkPhysicalDeviceHostQueryResetFeaturesEXT HostQueryResetFeatures;
};


// ***** VK_EXT_subgroup_size_control
class FVulkanEXTSubgroupSizeControlExtension : public FVulkanDeviceExtension
{
public:

	FVulkanEXTSubgroupSizeControlExtension(FVulkanDevice* InDevice)
		: FVulkanDeviceExtension(InDevice, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VK_API_VERSION_1_3)
	{
	}

	virtual void PrePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2KHR& PhysicalDeviceFeatures2) override final
	{
		ZeroVulkanStruct(SubgroupSizeControlFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES);
		AddToPNext(PhysicalDeviceFeatures2, SubgroupSizeControlFeatures);
	}

	virtual void PostPhysicalDeviceFeatures(FOptionalVulkanDeviceExtensions& ExtensionFlags) override final
	{
 		bSupportsSubgroupSizeControl = (SubgroupSizeControlFeatures.subgroupSizeControl == VK_TRUE);
	}

	virtual void PrePhysicalDeviceProperties(VkPhysicalDeviceProperties2KHR& PhysicalDeviceProperties2) override final
	{
		if (bSupportsSubgroupSizeControl)
		{
			ZeroVulkanStruct(SubgroupSizeControlProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES);
			AddToPNext(PhysicalDeviceProperties2, SubgroupSizeControlProperties);
		}
	}

	virtual void PreCreateDevice(VkDeviceCreateInfo& DeviceCreateInfo) override final
	{
		if (bSupportsSubgroupSizeControl)
		{
			GRHIMinimumWaveSize = SubgroupSizeControlProperties.minSubgroupSize;
			GRHIMaximumWaveSize = SubgroupSizeControlProperties.maxSubgroupSize;
		}
	}

private:
	VkPhysicalDeviceSubgroupSizeControlFeaturesEXT SubgroupSizeControlFeatures;
	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT SubgroupSizeControlProperties;
	bool bSupportsSubgroupSizeControl = false;
};



template <typename ExtensionType>
static void FlagExtensionSupport(const TArray<VkExtensionProperties>& ExtensionProperties, TArray<TUniquePtr<ExtensionType>>& UEExtensions, const TCHAR* ExtensionTypeName)
{
	// Flag the extension support
	UE_LOG(LogVulkanRHI, Display, TEXT("Found %d available %s extensions :"), ExtensionProperties.Num(), ExtensionTypeName);
	for (const VkExtensionProperties& Extension : ExtensionProperties)
	{
		const int32 ExtensionIndex = ExtensionType::FindExtension(UEExtensions, Extension.extensionName);
		const bool bFound = (ExtensionIndex != INDEX_NONE);
		if (bFound)
		{
			UEExtensions[ExtensionIndex]->SetSupported();
		}
		UE_LOG(LogVulkanRHI, Display, TEXT("  %s %s"), bFound ? TEXT("+") : TEXT("-"), ANSI_TO_TCHAR(Extension.extensionName));
	}
}



FVulkanDeviceExtensionArray FVulkanDeviceExtension::GetUESupportedDeviceExtensions(FVulkanDevice* InDevice)
{
	FVulkanDeviceExtensionArray OutUEDeviceExtensions;

	#define ADD_SIMPLE_EXTENSION(EXTENSION_NAME, ENABLED_IN_CODE, PROMOTED_VER, FLAG_SETTER) \
		OutUEDeviceExtensions.Add(MakeUnique<FVulkanDeviceExtension>(InDevice, EXTENSION_NAME, ENABLED_IN_CODE, PROMOTED_VER, FLAG_SETTER, FVulkanExtensionBase::AutoActivate))

	#define ADD_EXTERNAL_EXTENSION(EXTENSION_NAME, ENABLED_IN_CODE, PROMOTED_VER, FLAG_SETTER) \
		OutUEDeviceExtensions.Add(MakeUnique<FVulkanDeviceExtension>(InDevice, EXTENSION_NAME, ENABLED_IN_CODE, PROMOTED_VER, FLAG_SETTER, FVulkanExtensionBase::ManuallyActivate))

	#define ADD_CUSTOM_EXTENSION(EXTENSION_CLASS) \
		OutUEDeviceExtensions.Add(MakeUnique<EXTENSION_CLASS>(InDevice));


	// Generic simple extensions :

	ADD_SIMPLE_EXTENSION(VK_KHR_SWAPCHAIN_EXTENSION_NAME,                VULKAN_EXTENSION_ENABLED,             VULKAN_EXTENSION_NOT_PROMOTED, nullptr);
	ADD_SIMPLE_EXTENSION(VK_KHR_MAINTENANCE1_EXTENSION_NAME,             VULKAN_EXTENSION_ENABLED,             VK_API_VERSION_1_1,            DEVICE_EXT_FLAG_SETTER(HasKHRMaintenance1));
	ADD_SIMPLE_EXTENSION(VK_KHR_MAINTENANCE2_EXTENSION_NAME,             VULKAN_EXTENSION_ENABLED,             VK_API_VERSION_1_1,            DEVICE_EXT_FLAG_SETTER(HasKHRMaintenance2));
	ADD_SIMPLE_EXTENSION(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,            VULKAN_SUPPORTS_MEMORY_BUDGET,        VULKAN_EXTENSION_NOT_PROMOTED, DEVICE_EXT_FLAG_SETTER(HasMemoryBudget));
	ADD_SIMPLE_EXTENSION(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,          VULKAN_SUPPORTS_MEMORY_PRIORITY,      VULKAN_EXTENSION_NOT_PROMOTED, DEVICE_EXT_FLAG_SETTER(HasMemoryPriority));
	ADD_SIMPLE_EXTENSION(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,      VULKAN_SUPPORTS_RENDERPASS2,          VK_API_VERSION_1_2,            DEVICE_EXT_FLAG_SETTER(HasKHRRenderPass2));
	ADD_SIMPLE_EXTENSION(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,     VULKAN_SUPPORTS_DEDICATED_ALLOCATION, VK_API_VERSION_1_1,            DEVICE_EXT_FLAG_SETTER(HasKHRDedicatedAllocation));
	ADD_SIMPLE_EXTENSION(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VULKAN_RHI_RAYTRACING,                VULKAN_EXTENSION_NOT_PROMOTED, DEVICE_EXT_FLAG_SETTER(HasDeferredHostOperations));
	ADD_SIMPLE_EXTENSION(VK_KHR_SPIRV_1_4_EXTENSION_NAME,                VULKAN_RHI_RAYTRACING,                VK_API_VERSION_1_2,            DEVICE_EXT_FLAG_SETTER(HasSPIRV_14));
	ADD_SIMPLE_EXTENSION(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,    VULKAN_RHI_RAYTRACING,                VK_API_VERSION_1_2,            DEVICE_EXT_FLAG_SETTER(HasShaderFloatControls));
	ADD_SIMPLE_EXTENSION(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,        VULKAN_EXTENSION_ENABLED,             VK_API_VERSION_1_2,            DEVICE_EXT_FLAG_SETTER(HasKHRImageFormatList));
	ADD_SIMPLE_EXTENSION(VK_EXT_VALIDATION_CACHE_EXTENSION_NAME,         VULKAN_SUPPORTS_VALIDATION_CACHE,     VULKAN_EXTENSION_NOT_PROMOTED, DEVICE_EXT_FLAG_SETTER(HasEXTValidationCache));


	// Externally activated extensions (supported by the engine, but enabled externally by plugin or other) :

	ADD_EXTERNAL_EXTENSION(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VULKAN_SUPPORTS_EXTERNAL_MEMORY, VK_API_VERSION_1_1, nullptr);


	// Extensions with custom classes :

	ADD_CUSTOM_EXTENSION(FVulkanKHRDriverPropertiesExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRMaintenance4Extension);
	ADD_CUSTOM_EXTENSION(FVulkanShaderAtomicInt64Extension);      // must be kept BEFORE ShaderImageAtomicInt64!
	ADD_CUSTOM_EXTENSION(FVulkanShaderImageAtomicInt64Extension); // must be kept AFTER ShaderAtomicInt64!
	ADD_CUSTOM_EXTENSION(FVulkanEXTScalarBlockLayoutExtension);
	ADD_CUSTOM_EXTENSION(FVulkanEXTShaderViewportIndexLayerExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRSeparateDepthStencilLayoutsExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRFragmentShadingRateExtension); // must be kept BEFORE DensityMap!
	ADD_CUSTOM_EXTENSION(FVulkanEXTFragmentDensityMapExtension);  // must be kept AFTER ShadingRate!
	ADD_CUSTOM_EXTENSION(FVulkanEXTFragmentDensityMap2Extension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRMultiviewExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRGetMemoryRequirements2Extension);
	ADD_CUSTOM_EXTENSION(FVulkanEXTDescriptorIndexingExtension);
	ADD_CUSTOM_EXTENSION(FVulkanEXTHostQueryResetExtension);
	ADD_CUSTOM_EXTENSION(FVulkanEXTSubgroupSizeControlExtension);

	// Needed for Raytracing
	ADD_CUSTOM_EXTENSION(FVulkanKHRBufferDeviceAddressExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRAccelerationStructureExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRRayTracingPipelineExtension);
	ADD_CUSTOM_EXTENSION(FVulkanKHRRayQueryExtension);

	// Vendor extensions
	ADD_CUSTOM_EXTENSION(FVulkanAMDBufferMarkerExtension);
	ADD_CUSTOM_EXTENSION(FVulkanNVDeviceDiagnosticCheckpointsExtension);
	ADD_CUSTOM_EXTENSION(FVulkanNVDeviceDiagnosticConfigExtension);

	// Debug
	ADD_CUSTOM_EXTENSION(FVulkanEXTDebugMarkerExtension);

	// Add in platform specific extensions
	FVulkanPlatform::GetDeviceExtensions(InDevice, OutUEDeviceExtensions);

	// Helper function to go through a list of extensions and activate them (or add them)
	auto ActivateExternalExtensions = [&](TArray<const ANSICHAR*> Extensions, const TCHAR* Requester) {
		for (const ANSICHAR* ExtensionName : Extensions)
		{
			const int32 ExtensionIndex = FindExtension(OutUEDeviceExtensions, ExtensionName);
			if (ExtensionIndex == INDEX_NONE)
			{
				OutUEDeviceExtensions.Add(MakeUnique<FVulkanDeviceExtension>(InDevice, ExtensionName, VULKAN_EXTENSION_ENABLED));
				UE_LOG(LogVulkanRHI, Warning, TEXT("%s requested device extension [%s] isn't part of the engine's core extension list. Adding it on-the-fly..."), Requester, ANSI_TO_TCHAR(ExtensionName));
			}
			else
			{
				OutUEDeviceExtensions[ExtensionIndex]->SetActivated();
			}
		}
	};

	// Add HMD requested extensions
	{
		if (IHeadMountedDisplayModule::IsAvailable())
		{
			FVulkanDynamicRHI::HMDVulkanExtensions = IHeadMountedDisplayModule::Get().GetVulkanExtensions();
		}
		if (FVulkanDynamicRHI::HMDVulkanExtensions.IsValid())
		{
			TArray<const ANSICHAR*> HMDExtensions;
			FVulkanDynamicRHI::HMDVulkanExtensions->GetVulkanDeviceExtensionsRequired(InDevice->GetPhysicalHandle(), HMDExtensions);
			ActivateExternalExtensions(HMDExtensions, TEXT("HMD"));
		}
	}

	// Add extensions added outside the RHI (eg plugins)
	ActivateExternalExtensions(ExternalExtensions, TEXT("Externally"));

	// Now that all the extensions are listed, update their support flags
	FlagExtensionSupport(GetDriverSupportedDeviceExtensions(InDevice->GetPhysicalHandle()), OutUEDeviceExtensions, TEXT("device"));

	#undef ADD_SIMPLE_EXTENSION
	#undef ADD_EXTERNAL_EXTENSION
	#undef ADD_CUSTOM_EXTENSION

	return OutUEDeviceExtensions;
}











// *** Vulkan Instance Extension support ***
// Typical flow:
// 1- The engine will query the supported extensions at RHI creation (FVulkanDeviceExtension are constructed and support is queried)
// 2- The supported extensions are added at VkInstance creation (PreCreateInstance)



// ***** VK_EXT_validation_features
class FVulkanEXTValidationFeaturesExtension : public FVulkanInstanceExtension
{
public:

	FVulkanEXTValidationFeaturesExtension()
		: FVulkanInstanceExtension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, VULKAN_HAS_DEBUGGING_ENABLED & VULKAN_HAS_VALIDATION_FEATURES, VULKAN_EXTENSION_NOT_PROMOTED, nullptr, FVulkanExtensionBase::ManuallyActivate)
	{}

	virtual void PreCreateInstance(VkInstanceCreateInfo& InstanceCreateInfo, FOptionalVulkanInstanceExtensions& ExtensionFlags) override final 
	{
#if VULKAN_HAS_DEBUGGING_ENABLED
		check(GValidationCvar.GetValueOnAnyThread() > 0);

		auto GetValidationFeaturesEnabled = []()
		{
			TArray<VkValidationFeatureEnableEXT> Features;
			const int32 GPUValidationValue = GGPUValidationCvar.GetValueOnAnyThread();
			if (GPUValidationValue > 0)
			{
				Features.Add(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
				if (GPUValidationValue > 1)
				{
					Features.Add(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
				}
			}

			if (FParse::Param(FCommandLine::Get(), TEXT("vulkanbestpractices")))
			{
				Features.Add(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
			}

			if (FParse::Param(FCommandLine::Get(), TEXT("vulkandebugsync")))
			{
				Features.Add(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
			}

			return Features;
		};

		ValidationFeaturesEnabled = GetValidationFeaturesEnabled();

		if (ValidationFeaturesEnabled.Num())
		{
			ZeroVulkanStruct(ValidationFeatures, VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
			ValidationFeatures.enabledValidationFeatureCount = (uint32)ValidationFeaturesEnabled.Num();
			ValidationFeatures.pEnabledValidationFeatures = ValidationFeaturesEnabled.GetData();
			AddToPNext(InstanceCreateInfo, ValidationFeatures);
		}
#endif 
	}

private:
	VkValidationFeaturesEXT ValidationFeatures;
	TArray<VkValidationFeatureEnableEXT> ValidationFeaturesEnabled;
};




FVulkanInstanceExtensionArray FVulkanInstanceExtension::GetUESupportedInstanceExtensions()
{
	FVulkanInstanceExtensionArray OutUEInstanceExtensions;

	// Generic simple extensions :
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2, VK_API_VERSION_1_1, INSTANCE_EXT_FLAG_SETTER(HasKHRGetPhysicalDeviceProperties2)));
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME, VULKAN_SUPPORTS_EXTERNAL_MEMORY, VK_API_VERSION_1_1, INSTANCE_EXT_FLAG_SETTER(HasKHRExternalMemoryCapabilities)));
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_KHR_SURFACE_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED));
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED));

	// Debug extensions :
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VULKAN_HAS_DEBUGGING_ENABLED & VULKAN_SUPPORTS_DEBUG_UTILS, VULKAN_EXTENSION_NOT_PROMOTED, nullptr, FVulkanExtensionBase::ManuallyActivate));
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, VULKAN_HAS_DEBUGGING_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED, nullptr, FVulkanExtensionBase::ManuallyActivate));

	// Extensions with custom classes :
	OutUEInstanceExtensions.Add(MakeUnique<FVulkanEXTValidationFeaturesExtension>());


	// Add in platform specific extensions
	FVulkanPlatform::GetInstanceExtensions(OutUEInstanceExtensions);

	// Helper function to go through a list of extensions and activate them (or add them)
	auto ActivateExternalExtensions = [&] (TArray<const ANSICHAR*> Extensions, const TCHAR* Requester) {
		for (const ANSICHAR* ExtensionName : Extensions)
		{
			const int32 ExtensionIndex = FindExtension(OutUEInstanceExtensions, ExtensionName);
			if (ExtensionIndex == INDEX_NONE)
			{
				OutUEInstanceExtensions.Add(MakeUnique<FVulkanInstanceExtension>(ExtensionName, VULKAN_EXTENSION_ENABLED));
				UE_LOG(LogVulkanRHI, Warning, TEXT("%s requested instance extension [%s] isn't part of the engine's core extension list. Adding it on-the-fly..."), Requester, ANSI_TO_TCHAR(ExtensionName));
			}
			else
			{
				OutUEInstanceExtensions[ExtensionIndex]->SetActivated();
			}
		}
	};


	// Add HMD requested extensions
	{
		if (IHeadMountedDisplayModule::IsAvailable())
		{
			FVulkanDynamicRHI::HMDVulkanExtensions = IHeadMountedDisplayModule::Get().GetVulkanExtensions();
		}
		if (FVulkanDynamicRHI::HMDVulkanExtensions.IsValid())
		{
			TArray<const ANSICHAR*> HMDExtensions;
			FVulkanDynamicRHI::HMDVulkanExtensions->GetVulkanInstanceExtensionsRequired(HMDExtensions);
			ActivateExternalExtensions(HMDExtensions, TEXT("HMD"));
		}
	}

	// Add extensions added outside the RHI (eg plugins)
	ActivateExternalExtensions(ExternalExtensions, TEXT("Externally"));

	// Now that all the extensions are listed, update their support flags
	FlagExtensionSupport(GetDriverSupportedInstanceExtensions(), OutUEInstanceExtensions, TEXT("instance"));

	return OutUEInstanceExtensions;
}




