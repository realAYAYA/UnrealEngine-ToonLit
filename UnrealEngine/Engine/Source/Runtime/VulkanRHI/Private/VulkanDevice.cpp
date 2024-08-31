// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanDevice.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanDevice.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "VulkanPlatform.h"
#include "VulkanLLM.h"
#include "VulkanTransientResourceAllocator.h"
#include "VulkanExtensions.h"
#include "VulkanRenderpass.h"
#include "VulkanRayTracing.h"
#include "VulkanDescriptorSets.h"
#include "VulkanChunkedPipelineCache.h"

TAutoConsoleVariable<int32> GRHIAllowAsyncComputeCvar(
	TEXT("r.Vulkan.AllowAsyncCompute"),
	0,
	TEXT("0 to disable async compute queue(if available)")
	TEXT("1 to allow async compute queue")
);

TAutoConsoleVariable<int32> GAllowPresentOnComputeQueue(
	TEXT("r.Vulkan.AllowPresentOnComputeQueue"),
	0,
	TEXT("0 to present on the graphics queue")
	TEXT("1 to allow presenting on the compute queue if available")
);

TAutoConsoleVariable<int32> GCVarRobustBufferAccess(
	TEXT("r.Vulkan.RobustBufferAccess"),
	1,
	TEXT("0 to disable robust buffer access")
	TEXT("1 to enable (default)"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarVulkanUseD24(
	TEXT("r.Vulkan.Depth24Bit"),
	0,
	TEXT("0: Use 32-bit float depth buffer (default)\n1: Use 24-bit fixed point depth buffer\n"),
	ECVF_ReadOnly
);


#if NV_AFTERMATH
#include "GFSDK_Aftermath_GpuCrashDump.h"
void AftermathGpuCrashDumpCallback(const void* pGpuCrashDump, const uint32 gpuCrashDumpSize, void* pUserData);
void AftermathShaderDebugInfoCallback(const void* pShaderDebugInfo, const uint32 shaderDebugInfoSize, void* pUserData);
void AftermathCrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* pUserData);
void AftermathResolveMarkerCallback(const void* pMarker, void* pUserData, void** resolvedMarkerData, uint32_t* markerSize);
#endif

// Mirror GPixelFormats with format information for buffers
VkFormat GVulkanBufferFormat[PF_MAX];

// Mirror GPixelFormats with format information for buffers
VkFormat GVulkanSRGBFormat[PF_MAX];

EDelayAcquireImageType GVulkanDelayAcquireImage = EDelayAcquireImageType::DelayAcquire;

TAutoConsoleVariable<int32> CVarDelayAcquireBackBuffer(
	TEXT("r.Vulkan.DelayAcquireBackBuffer"),
	1,
	TEXT("Whether to delay acquiring the back buffer \n")
	TEXT(" 0: acquire next image on frame start \n")
	TEXT(" 1: acquire next image just before presenting, rendering is done to intermediate image which is then copied to a real backbuffer (default) \n")
	TEXT(" 2: acquire next image on first use"),
	ECVF_ReadOnly
);

static EDelayAcquireImageType DelayAcquireBackBuffer()
{
	int32 DelayType = CVarDelayAcquireBackBuffer.GetValueOnAnyThread();
	switch (DelayType)
	{
	case 1:
		return EDelayAcquireImageType::DelayAcquire;
	case 2:
		return EDelayAcquireImageType::LazyAcquire;
	}
	return EDelayAcquireImageType::None;
}

static void EnableDrawMarkers()
{
	static IConsoleVariable* ShowMaterialDrawEventVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShowMaterialDrawEvents"));

	const bool bDrawEvents = GetEmitDrawEvents() != 0;
	const bool bMaterialDrawEvents = ShowMaterialDrawEventVar ? ShowMaterialDrawEventVar->GetInt() != 0 : false;

	UE_LOG(LogRHI, Display, TEXT("Setting GPU Capture Options: 1"));
	if (!bDrawEvents)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling draw events: 1"));
		SetEmitDrawEvents(true);
	}
	if (!bMaterialDrawEvents && ShowMaterialDrawEventVar)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling showmaterialdrawevents: 1"));
		ShowMaterialDrawEventVar->Set(-1);
	}
}

#if VULKAN_SUPPORTS_VALIDATION_CACHE
static void LoadValidationCache(VkDevice Device, VkValidationCacheEXT& OutValidationCache)
{
	VkValidationCacheCreateInfoEXT ValidationCreateInfo;
	ZeroVulkanStruct(ValidationCreateInfo, VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT);
	TArray<uint8> InData;

	const FString& CacheFilename = VulkanRHI::GetValidationCacheFilename();
	UE_LOG(LogVulkanRHI, Display, TEXT("Trying validation cache file %s"), *CacheFilename);
	if (FFileHelper::LoadFileToArray(InData, *CacheFilename, FILEREAD_Silent) && InData.Num() > 0)
	{
		// The code below supports SDK 1.0.65 Vulkan spec, which contains the following table:
		//
		// Offset	 Size            Meaning
		// ------    ------------    ------------------------------------------------------------------
		//      0               4    length in bytes of the entire validation cache header written as a
		//                           stream of bytes, with the least significant byte first
		//      4               4    a VkValidationCacheHeaderVersionEXT value written as a stream of
		//                           bytes, with the least significant byte first
		//      8    VK_UUID_SIZE    a layer commit ID expressed as a UUID, which uniquely identifies
		//                           the version of the validation layers used to generate these
		//                           validation results
		int32* DataPtr = (int32*)InData.GetData();
		if (*DataPtr > 0)
		{
			++DataPtr;
			int32 Version = *DataPtr++;
			if (Version == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
			{
				DataPtr += VK_UUID_SIZE / sizeof(int32);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Bad validation cache file %s, version=%d, expected %d"), *CacheFilename, Version, VK_PIPELINE_CACHE_HEADER_VERSION_ONE);
				InData.Reset(0);
			}
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Bad validation cache file %s, header size=%d"), *CacheFilename, *DataPtr);
			InData.Reset(0);
		}
	}

	ValidationCreateInfo.initialDataSize = InData.Num();
	ValidationCreateInfo.pInitialData = InData.Num() > 0 ? InData.GetData() : nullptr;
	//ValidationCreateInfo.flags = 0;
	PFN_vkCreateValidationCacheEXT vkCreateValidationCache = (PFN_vkCreateValidationCacheEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkCreateValidationCacheEXT");
	if (vkCreateValidationCache)
	{
		VkResult Result = vkCreateValidationCache(Device, &ValidationCreateInfo, VULKAN_CPU_ALLOCATOR, &OutValidationCache);
		if (Result != VK_SUCCESS)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to create Vulkan validation cache, VkResult=%d"), Result);
		}
	}
}
#endif

static VkExtent2D GetBestMatchedShadingRateExtents(uint32 ShadingRate, const TArray<VkPhysicalDeviceFragmentShadingRateKHR>& FragmentShadingRates)
{
	// Given that for Vulkan we need to query available device shading rates, we're not guaranteed to have everything that's in our enum;
	// This function walks the list of supported fragment rates returned by the device, and returns the closest match to the rate requested.
	const VkExtent2D DirectMappedExtent = { 
		1u << (ShadingRate >> 2), 
		1u << (ShadingRate & 0x03) 
	};
	VkExtent2D BestMatchedExtent = { 1, 1 };

	if (BestMatchedExtent.width != DirectMappedExtent.width || 
		BestMatchedExtent.height != DirectMappedExtent.height)
	{
		for (auto const& Rate : FragmentShadingRates)
		{
			if (Rate.fragmentSize.width == DirectMappedExtent.width && 
				Rate.fragmentSize.height == DirectMappedExtent.height)
			{
				BestMatchedExtent = DirectMappedExtent;
				break;
			}

			if ((Rate.fragmentSize.width >= BestMatchedExtent.width && Rate.fragmentSize.width <= DirectMappedExtent.width && Rate.fragmentSize.height <= DirectMappedExtent.height && Rate.fragmentSize.height >= BestMatchedExtent.height) ||
				(Rate.fragmentSize.height >= BestMatchedExtent.height && Rate.fragmentSize.height <= DirectMappedExtent.height && Rate.fragmentSize.width <= DirectMappedExtent.width && Rate.fragmentSize.width >= BestMatchedExtent.width))
			{
				BestMatchedExtent = Rate.fragmentSize;
			}
		}
	}

	return BestMatchedExtent;
}


void FVulkanPhysicalDeviceFeatures::Query(VkPhysicalDevice PhysicalDevice, uint32 APIVersion)
{
	VkPhysicalDeviceFeatures2 PhysicalDeviceFeatures2;
	ZeroVulkanStruct(PhysicalDeviceFeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);

	PhysicalDeviceFeatures2.pNext = &Core_1_1;
	Core_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

	if (APIVersion >= VK_API_VERSION_1_2)
	{
		Core_1_1.pNext = &Core_1_2;
		Core_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	}

	if (APIVersion >= VK_API_VERSION_1_3)
	{
		Core_1_2.pNext = &Core_1_3;
		Core_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	}

	VulkanRHI::vkGetPhysicalDeviceFeatures2(PhysicalDevice, &PhysicalDeviceFeatures2);

	// Copy features into old struct for convenience
	Core_1_0 = PhysicalDeviceFeatures2.features;

	// Apply config modifications
	Core_1_0.robustBufferAccess = GCVarRobustBufferAccess.GetValueOnAnyThread() > 0 ? VK_TRUE : VK_FALSE;

	// Apply platform restrictions
	FVulkanPlatform::RestrictEnabledPhysicalDeviceFeatures(this);
}



FVulkanDevice::FVulkanDevice(FVulkanDynamicRHI* InRHI, VkPhysicalDevice InGpu)
	: Device(VK_NULL_HANDLE)
	, MemoryManager(this)
	, DeferredDeletionQueue(this)
	, DefaultSampler(nullptr)
	, DefaultTexture(nullptr)
	, Gpu(InGpu)
	, GfxQueue(nullptr)
	, ComputeQueue(nullptr)
	, TransferQueue(nullptr)
	, PresentQueue(nullptr)
	, ImmediateContext(nullptr)
	, ComputeContext(nullptr)
	, PipelineStateCache(nullptr)
{
	RHI = InRHI;
	FMemory::Memzero(GpuProps);
	FMemory::Memzero(FormatProperties);
	FMemory::Memzero(PixelFormatComponentMapping);

	ZeroVulkanStruct(GpuIdProps, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR);
	ZeroVulkanStruct(GpuSubgroupProps, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);

	{
		VkPhysicalDeviceProperties2KHR PhysicalDeviceProperties2;
		ZeroVulkanStruct(PhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR);
		PhysicalDeviceProperties2.pNext = &GpuIdProps;
		GpuIdProps.pNext = &GpuSubgroupProps;
		VulkanRHI::vkGetPhysicalDeviceProperties2(Gpu, &PhysicalDeviceProperties2);
		GpuProps = PhysicalDeviceProperties2.properties;
	}

	// First get the VendorId. We'll have to get properties again after finding out which extensions we want to use
	VendorId = RHIConvertToGpuVendorId(GpuProps.vendorID);

	UE_LOG(LogVulkanRHI, Display, TEXT("- DeviceName: %s"), ANSI_TO_TCHAR(GpuProps.deviceName));
	UE_LOG(LogVulkanRHI, Display, TEXT("- API=%d.%d.%d (0x%x) Driver=0x%x VendorId=0x%x"), VK_VERSION_MAJOR(GpuProps.apiVersion), VK_VERSION_MINOR(GpuProps.apiVersion), VK_VERSION_PATCH(GpuProps.apiVersion), GpuProps.apiVersion, GpuProps.driverVersion, GpuProps.vendorID);
	UE_LOG(LogVulkanRHI, Display, TEXT("- DeviceID=0x%x Type=%s"), GpuProps.deviceID, VK_TYPE_TO_STRING(VkPhysicalDeviceType, GpuProps.deviceType));
	UE_LOG(LogVulkanRHI, Display, TEXT("- Max Descriptor Sets Bound %d, Timestamps %d"), GpuProps.limits.maxBoundDescriptorSets, GpuProps.limits.timestampComputeAndGraphics);

	ensureMsgf(VendorId != EGpuVendorId::Unknown, TEXT("Unknown vendor ID 0x%x"), GpuProps.vendorID);
}

FVulkanDevice::~FVulkanDevice()
{
	if (Device != VK_NULL_HANDLE)
	{
		Destroy();
		Device = VK_NULL_HANDLE;
	}
}

static inline FString GetQueueInfoString(const VkQueueFamilyProperties& Props)
{
	FString Info;
	if ((Props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
	{
		Info += TEXT(" Gfx");
	}
	if ((Props.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
	{
		Info += TEXT(" Compute");
	}
	if ((Props.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
	{
		Info += TEXT(" Xfer");
	}
	if ((Props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == VK_QUEUE_SPARSE_BINDING_BIT)
	{
		Info += TEXT(" Sparse");
	}

	return Info;
};

void FVulkanDevice::CreateDevice(TArray<const ANSICHAR*>& DeviceLayers, FVulkanDeviceExtensionArray& UEExtensions)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanMisc);
	check(Device == VK_NULL_HANDLE);

	// Setup extension and layer info
	VkDeviceCreateInfo DeviceInfo;
	ZeroVulkanStruct(DeviceInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

	DeviceInfo.pEnabledFeatures = &PhysicalDeviceFeatures.Core_1_0;

	for (TUniquePtr<FVulkanDeviceExtension>& UEExtension : UEExtensions)
	{
		if (UEExtension->InUse())
		{
			DeviceExtensions.Add(UEExtension->GetExtensionName());
			UEExtension->PreCreateDevice(DeviceInfo);
		}
	}

	DeviceInfo.enabledExtensionCount = DeviceExtensions.Num();
	DeviceInfo.ppEnabledExtensionNames = DeviceExtensions.GetData();

	DeviceInfo.enabledLayerCount = DeviceLayers.Num();
	DeviceInfo.ppEnabledLayerNames = (DeviceInfo.enabledLayerCount > 0) ? DeviceLayers.GetData() : nullptr;

	// Setup Queue info
	TArray<VkDeviceQueueCreateInfo> QueueFamilyInfos;
	int32 GfxQueueFamilyIndex = -1;
	int32 ComputeQueueFamilyIndex = -1;
	int32 TransferQueueFamilyIndex = -1;
	UE_LOG(LogVulkanRHI, Display, TEXT("Found %d Queue Families"), QueueFamilyProps.Num());
	uint32 NumPriorities = 0;
	for (int32 FamilyIndex = 0; FamilyIndex < QueueFamilyProps.Num(); ++FamilyIndex)
	{
		const VkQueueFamilyProperties& CurrProps = QueueFamilyProps[FamilyIndex];

		bool bIsValidQueue = false;
		if ((CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
		{
			if (GfxQueueFamilyIndex == -1)
			{
				GfxQueueFamilyIndex = FamilyIndex;
				bIsValidQueue = true;
			}
			else
			{
				//#todo-rco: Support for multi-queue/choose the best queue!
			}
		}

		if ((CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
		{
			if (ComputeQueueFamilyIndex == -1 &&
				(GRHIAllowAsyncComputeCvar.GetValueOnAnyThread() != 0 || GAllowPresentOnComputeQueue.GetValueOnAnyThread() != 0) && GfxQueueFamilyIndex != FamilyIndex)
			{
				ComputeQueueFamilyIndex = FamilyIndex;
				bIsValidQueue = true;
			}
		}

		if ((CurrProps.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
		{
			// Prefer a non-gfx transfer queue
			if (TransferQueueFamilyIndex == -1 && (CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != VK_QUEUE_GRAPHICS_BIT && (CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != VK_QUEUE_COMPUTE_BIT)
			{
				TransferQueueFamilyIndex = FamilyIndex;
				bIsValidQueue = true;
			}
		}

		if (!bIsValidQueue)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Skipping unnecessary Queue Family %d: %d queues%s"), FamilyIndex, CurrProps.queueCount, *GetQueueInfoString(CurrProps));
			continue;
		}

		int32 QueueIndex = QueueFamilyInfos.Num();
		QueueFamilyInfos.AddZeroed(1);
		VkDeviceQueueCreateInfo& CurrQueue = QueueFamilyInfos[QueueIndex];
		CurrQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		CurrQueue.queueFamilyIndex = FamilyIndex;
		CurrQueue.queueCount = CurrProps.queueCount;
		NumPriorities += CurrProps.queueCount;
		UE_LOG(LogVulkanRHI, Display, TEXT("Initializing Queue Family %d: %d queues%s"), FamilyIndex, CurrProps.queueCount, *GetQueueInfoString(CurrProps));
	}

	TArray<float> QueuePriorities;
	QueuePriorities.AddUninitialized(NumPriorities);
	float* CurrentPriority = QueuePriorities.GetData();
	for (int32 Index = 0; Index < QueueFamilyInfos.Num(); ++Index)
	{
		VkDeviceQueueCreateInfo& CurrQueue = QueueFamilyInfos[Index];
		CurrQueue.pQueuePriorities = CurrentPriority;

		const VkQueueFamilyProperties& CurrProps = QueueFamilyProps[CurrQueue.queueFamilyIndex];
		for (int32 QueueIndex = 0; QueueIndex < (int32)CurrProps.queueCount; ++QueueIndex)
		{
			*CurrentPriority++ = 1.0f;
		}
	}

	DeviceInfo.queueCreateInfoCount = QueueFamilyInfos.Num();
	DeviceInfo.pQueueCreateInfos = QueueFamilyInfos.GetData();

#if NV_AFTERMATH && VULKAN_SUPPORTS_NV_DIAGNOSTICS
	if (GGPUCrashDebuggingEnabled && GVulkanNVAftermathModuleLoaded)
	{
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_EnableGpuCrashDumps(GFSDK_Aftermath_Version_API, 
			GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
			GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks, 
			&AftermathGpuCrashDumpCallback,
			&AftermathShaderDebugInfoCallback,
			&AftermathCrashDumpDescriptionCallback,
			&AftermathResolveMarkerCallback,
			this);
		if (Result != GFSDK_Aftermath_Result_Success)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to initialize Aftermath crash dumps (Result %d)"), (int32)Result);
		}
	}
#endif

	// Create the device
	VkResult Result = VulkanRHI::vkCreateDevice(Gpu, &DeviceInfo, VULKAN_CPU_ALLOCATOR, &Device);
	if (Result == VK_ERROR_INITIALIZATION_FAILED)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Cannot create a Vulkan device. Try updating your video driver to a more recent version.\n"), TEXT("Vulkan device creation failed"));
		FPlatformMisc::RequestExitWithStatus(true, 1);
	}
	VERIFYVULKANRESULT_EXPANDED(Result);

	FVulkanPlatform::NotifyFoundDeviceLayersAndExtensions(Gpu, DeviceLayers, DeviceExtensions);

	// Create Graphics Queue, here we submit command buffers for execution
	GfxQueue = new FVulkanQueue(this, GfxQueueFamilyIndex);
	if (ComputeQueueFamilyIndex == -1)
	{
		// If we didn't find a dedicated Queue, use the default one
		ComputeQueueFamilyIndex = GfxQueueFamilyIndex;
	}
	else
	{
		// Dedicated queue
		if (GRHIAllowAsyncComputeCvar.GetValueOnAnyThread() != 0)
		{
			bAsyncComputeQueue = true;
		}
	}
	ComputeQueue = new FVulkanQueue(this, ComputeQueueFamilyIndex);
	if (TransferQueueFamilyIndex == -1)
	{
		// If we didn't find a dedicated Queue, use the default one
		TransferQueueFamilyIndex = ComputeQueueFamilyIndex;
	}
	TransferQueue = new FVulkanQueue(this, TransferQueueFamilyIndex);

	uint64 NumBits = QueueFamilyProps[GfxQueueFamilyIndex].timestampValidBits;
	if (NumBits > 0)
	{
		ensure(NumBits == QueueFamilyProps[ComputeQueueFamilyIndex].timestampValidBits);
		if (NumBits == 64)
		{
			// Undefined behavior trying to 1 << 64 on uint64
			TimestampValidBitsMask = UINT64_MAX;
		}
		else
		{
			TimestampValidBitsMask = ((uint64)1 << (uint64)NumBits) - (uint64)1;
		}
	}

	// Enumerate the available shading rates
	if (OptionalDeviceExtensions.HasKHRFragmentShadingRate)
	{
		uint32 FragmentShadingRateCount = 0;
		VulkanRHI::vkGetPhysicalDeviceFragmentShadingRatesKHR(Gpu, &FragmentShadingRateCount, nullptr);
		if (FragmentShadingRateCount != 0)
		{
			FragmentShadingRates.SetNum(FragmentShadingRateCount);
			for (uint32 i = 0; i < FragmentShadingRateCount; ++i)
			{
				ZeroVulkanStruct(FragmentShadingRates[i], VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR);
			}
			VulkanRHI::vkGetPhysicalDeviceFragmentShadingRatesKHR(Gpu, &FragmentShadingRateCount, FragmentShadingRates.GetData());

			// Build a map from EVRSShadingRate to fragment size
			for (uint32 ShadingRate = 0u; ShadingRate < (uint32)FragmentSizeMap.Num(); ++ShadingRate)
			{
				FragmentSizeMap[ShadingRate] = GetBestMatchedShadingRateExtents(ShadingRate, FragmentShadingRates);
			}
		}
	}

	UE_LOG(LogVulkanRHI, Display, TEXT("Using %d device layers%s"), DeviceLayers.Num(), DeviceLayers.Num() ? TEXT(":") : TEXT("."));
	for (const ANSICHAR* Layer : DeviceLayers)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
	}

	UE_LOG(LogVulkanRHI, Display, TEXT("Using %d device extensions:"), DeviceExtensions.Num());
	for (const ANSICHAR* Extension : DeviceExtensions)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
	}

	GVulkanDelayAcquireImage = DelayAcquireBackBuffer();

	SetupDrawMarkers();
}

void FVulkanDevice::SetupDrawMarkers()
{
#if VULKAN_ENABLE_DRAW_MARKERS
	if (RHI->SupportsDebugUtilsExt())
	{
		// HOTFIX for UE-218250: Disable vulkan draw markers to get around crash/performance issues
		if (FParse::Param(FCommandLine::Get(), TEXT("forcevulkanddrawmarkers")))
		{
			DebugMarkers.CmdBeginDebugLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)(void*)VulkanRHI::vkGetInstanceProcAddr(RHI->GetInstance(), "vkCmdBeginDebugUtilsLabelEXT");
			DebugMarkers.CmdEndDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)(void*)VulkanRHI::vkGetInstanceProcAddr(RHI->GetInstance(), "vkCmdEndDebugUtilsLabelEXT");
			DebugMarkers.SetDebugName = (PFN_vkSetDebugUtilsObjectNameEXT)(void*)VulkanRHI::vkGetInstanceProcAddr(RHI->GetInstance(), "vkSetDebugUtilsObjectNameEXT");
		}

		if (DebugMarkers.CmdBeginDebugLabel && DebugMarkers.CmdEndDebugLabel && DebugMarkers.SetDebugName)
		{
			bDebugMarkersFound = true;
		}
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	if (bDebugMarkersFound && GRenderDocFound)
	{
		// We're running under RenderDoc or other trace tool, so enable capturing mode
		EnableDrawMarkers();
	}
#endif
#endif

#if VULKAN_ENABLE_DUMP_LAYER
	EnableDrawMarkers();
#endif
}

void FVulkanDevice::SetupFormats()
{
	for (uint32 Index = 0; Index < VK_FORMAT_RANGE_SIZE; ++Index)
	{
		const VkFormat Format = (VkFormat)Index;
		FMemory::Memzero(FormatProperties[Index]);
		VulkanRHI::vkGetPhysicalDeviceFormatProperties(Gpu, Format, &FormatProperties[Index]);
	}

	static_assert(sizeof(VkFormat) <= sizeof(GPixelFormats[0].PlatformFormat), "PlatformFormat must be increased!");

	// Create shortcuts for the possible component mappings
	const VkComponentMapping ComponentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	const VkComponentMapping ComponentMappingRGB1 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
	const VkComponentMapping ComponentMappingRG01 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE };
	const VkComponentMapping ComponentMappingR001 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE };
	const VkComponentMapping ComponentMappingRIII = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	const VkComponentMapping ComponentMapping000R = { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R };
	const VkComponentMapping ComponentMappingR000 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO };
	const VkComponentMapping ComponentMappingRR01 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE };


	// Initialize the platform pixel format map.
	for (int32 Index = 0; Index < PF_MAX; ++Index)
	{
		GPixelFormats[Index].PlatformFormat = VK_FORMAT_UNDEFINED;
		GPixelFormats[Index].Supported = false;
		GVulkanBufferFormat[Index] = VK_FORMAT_UNDEFINED;
		
		// Set default component mapping
		PixelFormatComponentMapping[Index] = ComponentMappingRGBA;
	}

	const EPixelFormatCapabilities ColorRenderTargetRequiredCapabilities = (EPixelFormatCapabilities::TextureSample | EPixelFormatCapabilities::RenderTarget);

	// Default formats
	MapFormatSupport(PF_B8G8R8A8, { VK_FORMAT_B8G8R8A8_UNORM }, ComponentMappingRGBA);
	MapFormatSupport(PF_G8, { VK_FORMAT_R8_UNORM }, ComponentMappingR001);
	MapFormatSupport(PF_FloatRGB, { VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT }, ComponentMappingRGB1, ColorRenderTargetRequiredCapabilities);
	MapFormatSupport(PF_FloatRGBA, { VK_FORMAT_R16G16B16A16_SFLOAT }, ComponentMappingRGBA, 8);
	MapFormatSupport(PF_ShadowDepth, { VK_FORMAT_D16_UNORM }, ComponentMappingRIII);
	MapFormatSupport(PF_G32R32F, { VK_FORMAT_R32G32_SFLOAT }, ComponentMappingRG01, 8);  // Requirement for GPU particles
	MapFormatSupport(PF_A32B32G32R32F, { VK_FORMAT_R32G32B32A32_SFLOAT }, ComponentMappingRGBA, 16);
	MapFormatSupport(PF_G16R16, { VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16_SFLOAT }, ComponentMappingRG01);
	MapFormatSupport(PF_G16R16F, { VK_FORMAT_R16G16_SFLOAT }, ComponentMappingRG01);
	MapFormatSupport(PF_G16R16F_FILTER, { VK_FORMAT_R16G16_SFLOAT }, ComponentMappingRG01);
	MapFormatSupport(PF_R16_UINT, { VK_FORMAT_R16_UINT }, ComponentMappingR001);
	MapFormatSupport(PF_R16_SINT, { VK_FORMAT_R16_SINT }, ComponentMappingR001);
	MapFormatSupport(PF_R32_UINT, { VK_FORMAT_R32_UINT }, ComponentMappingR001);
	MapFormatSupport(PF_R32_SINT, { VK_FORMAT_R32_SINT }, ComponentMappingR001);
	MapFormatSupport(PF_R8_UINT, { VK_FORMAT_R8_UINT }, ComponentMappingR001);
	MapFormatSupport(PF_D24, { VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT }, ComponentMappingR000);
	MapFormatSupport(PF_R16F, { VK_FORMAT_R16_SFLOAT }, ComponentMappingR001);
	MapFormatSupport(PF_R16F_FILTER, { VK_FORMAT_R16_SFLOAT }, ComponentMappingR001);
	MapFormatSupport(PF_FloatR11G11B10, { VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT }, ComponentMappingRGB1, ColorRenderTargetRequiredCapabilities);
	MapFormatSupport(PF_A2B10G10R10, { VK_FORMAT_A2B10G10R10_UNORM_PACK32 }, ComponentMappingRGBA, 4);
	MapFormatSupport(PF_A16B16G16R16, { VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT }, ComponentMappingRGBA, 8);
	MapFormatSupport(PF_A8, { VK_FORMAT_R8_UNORM }, ComponentMapping000R);
	MapFormatSupport(PF_R5G6B5_UNORM, { VK_FORMAT_R5G6B5_UNORM_PACK16 }, ComponentMappingRGBA);
	MapFormatSupport(PF_B5G5R5A1_UNORM, { VK_FORMAT_A1R5G5B5_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16, VK_FORMAT_B8G8R8A8_UNORM }, ComponentMappingRGBA);
	MapFormatSupport(PF_R8G8B8A8, { VK_FORMAT_R8G8B8A8_UNORM }, ComponentMappingRGBA);
	MapFormatSupport(PF_R8G8B8A8_UINT, { VK_FORMAT_R8G8B8A8_UINT }, ComponentMappingRGBA);
	MapFormatSupport(PF_R8G8B8A8_SNORM, { VK_FORMAT_R8G8B8A8_SNORM }, ComponentMappingRGBA);
	MapFormatSupport(PF_R16G16_UINT, { VK_FORMAT_R16G16_UINT }, ComponentMappingRG01);
	MapFormatSupport(PF_R16G16B16A16_UINT, { VK_FORMAT_R16G16B16A16_UINT }, ComponentMappingRGBA);
	MapFormatSupport(PF_R16G16B16A16_SINT, { VK_FORMAT_R16G16B16A16_SINT }, ComponentMappingRGBA);
	MapFormatSupport(PF_R32G32_UINT, { VK_FORMAT_R32G32_UINT }, ComponentMappingRG01);
	MapFormatSupport(PF_R32G32B32A32_UINT, { VK_FORMAT_R32G32B32A32_UINT }, ComponentMappingRGBA);
	MapFormatSupport(PF_R16G16B16A16_SNORM, { VK_FORMAT_R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_SFLOAT }, ComponentMappingRGBA);
	MapFormatSupport(PF_R16G16B16A16_UNORM, { VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT }, ComponentMappingRGBA);
	MapFormatSupport(PF_R8G8, { VK_FORMAT_R8G8_UNORM }, ComponentMappingRG01);
	MapFormatSupport(PF_V8U8, { VK_FORMAT_R8G8_UNORM }, ComponentMappingRG01);
	MapFormatSupport(PF_R32_FLOAT, { VK_FORMAT_R32_SFLOAT }, ComponentMappingR001);
	MapFormatSupport(PF_R8, { VK_FORMAT_R8_UNORM }, ComponentMappingR001);
	MapFormatSupport(PF_G16R16_SNORM, { VK_FORMAT_R16G16_SNORM }, ComponentMappingRG01);
	MapFormatSupport(PF_R8G8_UINT, { VK_FORMAT_R8G8_UINT }, ComponentMappingRG01);
	MapFormatSupport(PF_R32G32B32_UINT, { VK_FORMAT_R32G32B32_UINT }, ComponentMappingRGB1);
	MapFormatSupport(PF_R32G32B32_SINT, { VK_FORMAT_R32G32B32_SINT }, ComponentMappingRGB1);
	MapFormatSupport(PF_R32G32B32F, { VK_FORMAT_R32G32B32_SFLOAT }, ComponentMappingRGB1);
	MapFormatSupport(PF_R8_SINT, { VK_FORMAT_R8_SINT }, ComponentMappingR001);

	// This will be the format used for 64bit image atomics
	// This format is SM5 only, skip it for mobile to not confuse QA with a logged error about missing pixel format
	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
#if VULKAN_HAS_DEBUGGING_ENABLED
		const EPixelFormatCapabilities RequiredCaps64U = GRenderDocFound ? EPixelFormatCapabilities::UAV : (EPixelFormatCapabilities::UAV | EPixelFormatCapabilities::TextureAtomics);
#else
		const EPixelFormatCapabilities RequiredCaps64U = (EPixelFormatCapabilities::UAV | EPixelFormatCapabilities::TextureAtomics);
#endif
		MapFormatSupport(PF_R64_UINT, { VK_FORMAT_R64_UINT, VK_FORMAT_R32G32_UINT }, ComponentMappingR001, RequiredCaps64U);
		// Shaders were patched to use UAV, make sure we don't expose texture sampling
		GPixelFormats[PF_R64_UINT].Capabilities &= ~(EPixelFormatCapabilities::AnyTexture | EPixelFormatCapabilities::TextureSample);
		if (GRHISupportsAtomicUInt64 && !EnumHasAnyFlags(GPixelFormats[PF_R64_UINT].Capabilities, EPixelFormatCapabilities::UAV))
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("64bit image atomics were enabled, but the R64 format does not have UAV capabilities.  Disabling support."));
			GRHISupportsAtomicUInt64 = false;
		}
	}

	if (CVarVulkanUseD24.GetValueOnAnyThread() != 0)
	{
		// prefer VK_FORMAT_D24_UNORM_S8_UINT
		MapFormatSupport(PF_DepthStencil, { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT }, ComponentMappingRIII);
		MapFormatSupport(PF_X24_G8, { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT }, ComponentMappingRR01);
		GPixelFormats[PF_DepthStencil].bIs24BitUnormDepthStencil = true;
	}
	else
	{
		// prefer VK_FORMAT_D32_SFLOAT_S8_UINT
		MapFormatSupport(PF_DepthStencil, { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT }, ComponentMappingRIII);
		MapFormatSupport(PF_X24_G8, { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT }, ComponentMappingRR01);
	}

	if (FVulkanPlatform::SupportsBCTextureFormats())
	{
		MapFormatSupport(PF_DXT1, { VK_FORMAT_BC1_RGB_UNORM_BLOCK }, ComponentMappingRGB1);	// Also what OpenGL expects (RGBA instead RGB, but not SRGB)
		MapFormatSupport(PF_DXT3, { VK_FORMAT_BC2_UNORM_BLOCK },     ComponentMappingRGBA);
		MapFormatSupport(PF_DXT5, { VK_FORMAT_BC3_UNORM_BLOCK },     ComponentMappingRGBA);
		MapFormatSupport(PF_BC4,  { VK_FORMAT_BC4_UNORM_BLOCK },     ComponentMappingRGBA);
		MapFormatSupport(PF_BC5,  { VK_FORMAT_BC5_UNORM_BLOCK },     ComponentMappingRGBA);
		MapFormatSupport(PF_BC6H, { VK_FORMAT_BC6H_UFLOAT_BLOCK },   ComponentMappingRGBA);
		MapFormatSupport(PF_BC7,  { VK_FORMAT_BC7_UNORM_BLOCK },     ComponentMappingRGBA);
	}

	if (FVulkanPlatform::SupportsASTCTextureFormats())
	{
		MapFormatSupport(PF_ASTC_4x4,   { VK_FORMAT_ASTC_4x4_UNORM_BLOCK },   ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_6x6,   { VK_FORMAT_ASTC_6x6_UNORM_BLOCK },   ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_8x8,   { VK_FORMAT_ASTC_8x8_UNORM_BLOCK },   ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_10x10, { VK_FORMAT_ASTC_10x10_UNORM_BLOCK }, ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_12x12, { VK_FORMAT_ASTC_12x12_UNORM_BLOCK }, ComponentMappingRGBA);
	}

	if (FVulkanPlatform::SupportsETC2TextureFormats())
	{
		MapFormatSupport(PF_ETC2_RGB,      { VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK },   ComponentMappingRGB1);
		MapFormatSupport(PF_ETC2_RGBA,     { VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK }, ComponentMappingRGBA);
		MapFormatSupport(PF_ETC2_R11_EAC,  { VK_FORMAT_EAC_R11_UNORM_BLOCK },       ComponentMappingR001);
		MapFormatSupport(PF_ETC2_RG11_EAC, { VK_FORMAT_EAC_R11G11_UNORM_BLOCK },    ComponentMappingRG01);
	}
	if (FVulkanPlatform::SupportsR16UnormTextureFormat())
	{
		MapFormatSupport(PF_G16, { VK_FORMAT_R16_UNORM, VK_FORMAT_R16_SFLOAT }, ComponentMappingR001);
	}
	else
	{
		MapFormatSupport(PF_G16, { VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16_UNORM }, ComponentMappingR001);
	}

	if (GetOptionalExtensions().HasEXTTextureCompressionASTCHDR)
	{
		MapFormatSupport(PF_ASTC_4x4_HDR,   { VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT },   ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_6x6_HDR,   { VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT },   ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_8x8_HDR,   { VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT },   ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_10x10_HDR, { VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT }, ComponentMappingRGBA);
		MapFormatSupport(PF_ASTC_12x12_HDR, { VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT }, ComponentMappingRGBA);
	}


	// Verify available Vertex Formats
	{
		static_assert(VET_None == 0, "Change loop below to skip VET_None");
		for (int32 VETIndex = (int32)VET_None + 1; VETIndex < VET_MAX; ++VETIndex)
		{
			const EVertexElementType UEType = (EVertexElementType)VETIndex;
			const VkFormat VulkanFormat = UEToVkBufferFormat(UEType);
			const VkFormatProperties& VertexFormatProperties = GetFormatProperties(VulkanFormat);
			if (VertexFormatProperties.bufferFeatures == 0)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("EVertexElementType(%d) is not supported with VkFormat %d"), (int32)UEType, (int32)VulkanFormat);
			}
		}
	}

	// Verify the potential SRGB formats and fill GVulkanSRGBFormat
	{
		auto GetSRGBMapping = [this](const VkFormat InFormat)
		{
			VkFormat SRGBFormat = InFormat;
			switch (InFormat)
			{
			case VK_FORMAT_B8G8R8A8_UNORM:				SRGBFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
			case VK_FORMAT_A8B8G8R8_UNORM_PACK32:		SRGBFormat = VK_FORMAT_A8B8G8R8_SRGB_PACK32; break;
			case VK_FORMAT_R8_UNORM:					SRGBFormat = ((GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1) ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8_SRGB); break;
			case VK_FORMAT_R8G8_UNORM:					SRGBFormat = VK_FORMAT_R8G8_SRGB; break;
			case VK_FORMAT_R8G8B8_UNORM:				SRGBFormat = VK_FORMAT_R8G8B8_SRGB; break;
			case VK_FORMAT_R8G8B8A8_UNORM:				SRGBFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
			case VK_FORMAT_BC1_RGB_UNORM_BLOCK:			SRGBFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK; break;
			case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_BC1_RGBA_SRGB_BLOCK; break;
			case VK_FORMAT_BC2_UNORM_BLOCK:				SRGBFormat = VK_FORMAT_BC2_SRGB_BLOCK; break;
			case VK_FORMAT_BC3_UNORM_BLOCK:				SRGBFormat = VK_FORMAT_BC3_SRGB_BLOCK; break;
			case VK_FORMAT_BC7_UNORM_BLOCK:				SRGBFormat = VK_FORMAT_BC7_SRGB_BLOCK; break;
			case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK; break;
			case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:	SRGBFormat = VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK; break;
			case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:	SRGBFormat = VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_4x4_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_5x4_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_5x5_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_6x5_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_6x6_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_8x5_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_8x6_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_8x8_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_10x5_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_10x6_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_10x8_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_10x10_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_12x10_SRGB_BLOCK; break;
			case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:		SRGBFormat = VK_FORMAT_ASTC_12x12_SRGB_BLOCK; break;
				//		case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG; break;
				//		case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG; break;
				//		case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG; break;
				//		case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:	Format = VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG; break;
			default:	break;
			}

			// If we're introducing a new format, make sure it's supported
			if (InFormat != SRGBFormat)
			{
				const VkFormatProperties& SRGBFormatProperties = GetFormatProperties(SRGBFormat);
				if (!VKHasAnyFlags(SRGBFormatProperties.optimalTilingFeatures, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
				{
					// If we can't even sample from it, then reject the suggested SRGB format
					SRGBFormat = InFormat;
				}
			}

			return SRGBFormat;
		};

		for (int32 PixelFormatIndex = 0; PixelFormatIndex < PF_MAX; ++PixelFormatIndex)
		{
			const FPixelFormatInfo& PixelFormatInfo = GPixelFormats[PixelFormatIndex];
			if (PixelFormatInfo.Supported)
			{
				const VkFormat OriginalFormat = (VkFormat)PixelFormatInfo.PlatformFormat;
				GVulkanSRGBFormat[PixelFormatIndex] = GetSRGBMapping(OriginalFormat);
			}
			else
			{
				GVulkanSRGBFormat[PixelFormatIndex] = VK_FORMAT_UNDEFINED;
			}
		}
	}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT

	// Print the resulting pixel format support
	if (FParse::Param(FCommandLine::Get(), TEXT("PrintVulkanPixelFormatMappings")))
	{
		auto GetFormatCapabilities = [](EPixelFormatCapabilities FormatCapabilities)
		{
#define VULKAN_CHECK_FORMAT_CAPABILITY(PF_Name) if (EnumHasAllFlags(FormatCapabilities, EPixelFormatCapabilities::PF_Name)) { CapabilitiesString += TEXT(#PF_Name) TEXT(", ");}
			FString CapabilitiesString;

			VULKAN_CHECK_FORMAT_CAPABILITY(TextureSample);
			VULKAN_CHECK_FORMAT_CAPABILITY(TextureCube);
			VULKAN_CHECK_FORMAT_CAPABILITY(RenderTarget);
			VULKAN_CHECK_FORMAT_CAPABILITY(DepthStencil);
			VULKAN_CHECK_FORMAT_CAPABILITY(TextureBlendable);
			VULKAN_CHECK_FORMAT_CAPABILITY(TextureAtomics);

			VULKAN_CHECK_FORMAT_CAPABILITY(Buffer);
			VULKAN_CHECK_FORMAT_CAPABILITY(VertexBuffer);
			VULKAN_CHECK_FORMAT_CAPABILITY(IndexBuffer);
			VULKAN_CHECK_FORMAT_CAPABILITY(BufferAtomics);

			VULKAN_CHECK_FORMAT_CAPABILITY(UAV);

			return CapabilitiesString;
#undef VULKAN_CHECK_FORMAT_CAPABILITY
		};

		UE_LOG(LogVulkanRHI, Warning, TEXT("Pixel Format Mappings for Vulkan:"));
		UE_LOG(LogVulkanRHI, Warning, TEXT("%24s | %24s | BlockBytes | Components | ComponentMapping | BufferFormat | Capabilities | SRGBFormat"), 
			TEXT("PixelFormatName"), TEXT("VulkanFormat"));
		for (int32 PixelFormatIndex = 0; PixelFormatIndex < PF_MAX; ++PixelFormatIndex)
		{
			if (GPixelFormats[PixelFormatIndex].Supported)
			{
				const VkComponentMapping& ComponentMapping = PixelFormatComponentMapping[PixelFormatIndex];

				const VkFormat VulkanFormat = (VkFormat)GPixelFormats[PixelFormatIndex].PlatformFormat;
				FString VulkanFormatStr(VK_TYPE_TO_STRING(VkFormat, VulkanFormat));
				VulkanFormatStr.RightChopInline(10);  // Chop the VK_FORMAT_

				FString SRGBFormat;
				if (VulkanFormat != GVulkanSRGBFormat[PixelFormatIndex])
				{
					SRGBFormat = VK_TYPE_TO_STRING(VkFormat, GVulkanSRGBFormat[PixelFormatIndex]);
					SRGBFormat.RightChopInline(10);  // Chop the VK_FORMAT_
				}

				UE_LOG(LogVulkanRHI, Warning, TEXT("%24s | %24s | %10d | %10d | %10d,%d,%d,%d | %12d |  0x%08X  | %s"),
					GPixelFormats[PixelFormatIndex].Name,
					*VulkanFormatStr,
					GPixelFormats[PixelFormatIndex].BlockBytes,
					GPixelFormats[PixelFormatIndex].NumComponents,
					ComponentMapping.r, ComponentMapping.g, ComponentMapping.b, ComponentMapping.a,
					(int32)GVulkanBufferFormat[PixelFormatIndex],
					(uint32)GPixelFormats[PixelFormatIndex].Capabilities,
					*SRGBFormat
					);
			}
		}

		UE_LOG(LogVulkanRHI, Warning, TEXT("Pixel Format Capabilities for Vulkan:"));
		for (int32 PixelFormatIndex = 0; PixelFormatIndex < PF_MAX; ++PixelFormatIndex)
		{
			if (GPixelFormats[PixelFormatIndex].Supported)
			{
				const FString CapabilitiesString = GetFormatCapabilities(GPixelFormats[PixelFormatIndex].Capabilities);
				UE_LOG(LogVulkanRHI, Warning, TEXT("%24s : %s"), GPixelFormats[PixelFormatIndex].Name, *CapabilitiesString);
			}
		}
	}
#endif  // UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
}

const VkFormatProperties& FVulkanDevice::GetFormatProperties(VkFormat InFormat) const
{
	if (InFormat >= 0 && InFormat < VK_FORMAT_RANGE_SIZE)
	{
		return FormatProperties[InFormat];
	}

	// Check for extension formats
	const VkFormatProperties* FoundProperties = ExtensionFormatProperties.Find(InFormat);
	if (FoundProperties)
	{
		return *FoundProperties;
	}

	// Add it for faster caching next time
	VkFormatProperties& NewProperties = ExtensionFormatProperties.Add(InFormat);
	FMemory::Memzero(NewProperties);
	VulkanRHI::vkGetPhysicalDeviceFormatProperties(Gpu, InFormat, &NewProperties);
	return NewProperties;
}

void FVulkanDevice::MapBufferFormatSupport(FPixelFormatInfo& PixelFormatInfo, EPixelFormat UEFormat, VkFormat VulkanFormat)
{
	check(GVulkanBufferFormat[UEFormat] == VK_FORMAT_UNDEFINED);

	const VkFormatProperties& LocalFormatProperties = GetFormatProperties(VulkanFormat);
	EPixelFormatCapabilities Capabilities = EPixelFormatCapabilities::None;

	auto ConvertBufferCap = [&Capabilities, &LocalFormatProperties](EPixelFormatCapabilities UnrealCap, VkFormatFeatureFlags InFlag)
	{
		const bool HasBufferFeature = VKHasAllFlags(LocalFormatProperties.bufferFeatures, InFlag);
		if (HasBufferFeature)
		{
			EnumAddFlags(Capabilities, UnrealCap);
		}

		// Make sure we aren't looking in the wrong place for a bit
		check(!VKHasAnyFlags(LocalFormatProperties.linearTilingFeatures, InFlag));
		check(!VKHasAnyFlags(LocalFormatProperties.optimalTilingFeatures, InFlag));
	};

	// Check for buffer caps, use the first one with any caps
	if (LocalFormatProperties.bufferFeatures != 0)
	{
		EnumAddFlags(Capabilities, EPixelFormatCapabilities::Buffer);

		ConvertBufferCap(EPixelFormatCapabilities::VertexBuffer, VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT);
		ConvertBufferCap(EPixelFormatCapabilities::BufferLoad, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT);
		ConvertBufferCap(EPixelFormatCapabilities::BufferStore, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT);
		ConvertBufferCap(EPixelFormatCapabilities::BufferAtomics, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT);

		// Vulkan index buffers aren't tied to formats, so any 16 or 32 bit UINT format with a single component will do...
		// But because we can't check for uint vs float, hardcode supported formats for now
		if (EnumHasAllFlags(Capabilities, (EPixelFormatCapabilities::BufferLoad | EPixelFormatCapabilities::BufferStore)) &&
			((VulkanFormat == VK_FORMAT_R16_UINT) || (VulkanFormat == VK_FORMAT_R32_UINT)))
		{
			EnumAddFlags(Capabilities, EPixelFormatCapabilities::IndexBuffer);
		}

		GVulkanBufferFormat[UEFormat] = VulkanFormat;
		PixelFormatInfo.Capabilities |= Capabilities;
	}
}

void FVulkanDevice::MapImageFormatSupport(FPixelFormatInfo& PixelFormatInfo, const TArrayView<const VkFormat>& PrioritizedFormats, EPixelFormatCapabilities RequiredCapabilities)
{
	// Query for MipMap support with typical parameters
	auto SupportsMipMap = [this](VkFormat InFormat)
	{
		VkImageFormatProperties ImageFormatProperties;
		VkResult RetVal = VulkanRHI::vkGetPhysicalDeviceImageFormatProperties(Gpu, InFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &ImageFormatProperties);
		return (RetVal == VK_SUCCESS) && (ImageFormatProperties.maxMipLevels > 1);
	};

	EPixelFormatCapabilities Capabilities = EPixelFormatCapabilities::None;
	auto ConvertImageCap = [&Capabilities](const VkFormatProperties& InFormatProperties, EPixelFormatCapabilities UnrealCap, VkFormatFeatureFlags InFlag)
	{
		// Do not distinguish between Linear and Optimal for now.
		const bool HasImageFeature = VKHasAllFlags(InFormatProperties.linearTilingFeatures, InFlag) || VKHasAllFlags(InFormatProperties.optimalTilingFeatures, InFlag);
		if (HasImageFeature)
		{
			EnumAddFlags(Capabilities, UnrealCap);
		}

		// Make sure we aren't looking in the wrong place for a bit
		check(!VKHasAnyFlags(InFormatProperties.bufferFeatures, InFlag));
	};

	// Go through the PrioritizedFormats and use the first one that meets RequiredCapabilities
	for (int32 FormatIndex = 0; FormatIndex < PrioritizedFormats.Num(); ++FormatIndex)
	{
		Capabilities = EPixelFormatCapabilities::None;

		const VkFormat VulkanFormat = PrioritizedFormats[FormatIndex];
		const VkFormatProperties& LocalFormatProperties = GetFormatProperties(VulkanFormat);

		// Check for individual texture caps
		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::AnyTexture | EPixelFormatCapabilities::TextureSample | EPixelFormatCapabilities::TextureLoad, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::DepthStencil, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::RenderTarget, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::TextureBlendable, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::AllUAVFlags | EPixelFormatCapabilities::TextureStore, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::TextureAtomics, VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT);
		ConvertImageCap(LocalFormatProperties, EPixelFormatCapabilities::TextureFilterable, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

		if (EnumHasAllFlags(Capabilities, EPixelFormatCapabilities::AnyTexture))
		{
			// We support gather, but some of our shaders assume offsets so check against features
			if (GetPhysicalDeviceFeatures().Core_1_0.shaderImageGatherExtended)
			{
				EnumAddFlags(Capabilities, EPixelFormatCapabilities::TextureGather);
			}

			if (SupportsMipMap(VulkanFormat))
			{
				EnumAddFlags(Capabilities, EPixelFormatCapabilities::TextureMipmaps);
			}
		}

		if (EnumHasAllFlags(Capabilities, RequiredCapabilities))
		{
			PixelFormatInfo.PlatformFormat = VulkanFormat;
			PixelFormatInfo.Capabilities |= Capabilities;

			if (FormatIndex > 0)
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("MapImageFormatSupport: %s is not supported with VkFormat %d, falling back to VkFormat %d"), PixelFormatInfo.Name, (int32)PrioritizedFormats[0], (int32)PrioritizedFormats[FormatIndex]);
			}

			break;
		}
	}
}

// Minimum capabilities required for a Vulkan format to be considered as supported
static constexpr EPixelFormatCapabilities kDefaultTextureCapabilities = EPixelFormatCapabilities::TextureSample;
// Passthrough to specify we want to keep the initial BlockBytes value set in the PixelFormat
static constexpr int32 kDefaultBlockBytes = -1;

void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> InPrioritizedFormats, const VkComponentMapping& ComponentMapping, EPixelFormatCapabilities RequiredCapabilities, int32 BlockBytes)
{
	TArrayView<const VkFormat> PrioritizedFormats = MakeArrayView(InPrioritizedFormats);
	FPixelFormatInfo& PixelFormatInfo = GPixelFormats[UEFormat];

	check(PrioritizedFormats.Num() > 0);
	check(!PixelFormatInfo.Supported);
	check(PixelFormatInfo.Capabilities == EPixelFormatCapabilities::None);

	MapBufferFormatSupport(PixelFormatInfo, UEFormat, PrioritizedFormats[0]);
	MapImageFormatSupport(PixelFormatInfo, PrioritizedFormats, RequiredCapabilities);

	// Flag the pixel format as supported if we can do anything with it
	PixelFormatInfo.Supported = EnumHasAllFlags(PixelFormatInfo.Capabilities, RequiredCapabilities) || EnumHasAnyFlags(PixelFormatInfo.Capabilities, EPixelFormatCapabilities::Buffer);
	if (PixelFormatInfo.Supported)
	{
		PixelFormatComponentMapping[UEFormat] = ComponentMapping;
		if (BlockBytes > 0)
		{
			PixelFormatInfo.BlockBytes = BlockBytes;
		}
	}
	else
	{
		UE_LOG(LogVulkanRHI, Error, TEXT("MapFormatSupport: %s is not supported with VkFormat %d"), PixelFormatInfo.Name, (int32)PrioritizedFormats[0]);
	}
}

void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping)
{
	MapFormatSupport(UEFormat, PrioritizedFormats, ComponentMapping, kDefaultTextureCapabilities, kDefaultBlockBytes);
}
void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, int32 BlockBytes)
{
	MapFormatSupport(UEFormat, PrioritizedFormats, ComponentMapping, kDefaultTextureCapabilities, BlockBytes);
}
void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, EPixelFormatCapabilities RequiredCapabilities)
{
	MapFormatSupport(UEFormat, PrioritizedFormats, ComponentMapping, RequiredCapabilities, kDefaultBlockBytes);
}

bool FVulkanDevice::SupportsBindless() const
{
	checkSlow(BindlessDescriptorManager != nullptr);
	return BindlessDescriptorManager->IsSupported();
}

void FVulkanDevice::ChooseVariableRateShadingMethod()
{
	auto IsFragmentShadingRateAvailable = [](VkPhysicalDeviceFragmentShadingRateFeaturesKHR& FragmentShadingRateFeatures)
	{
		return FragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE;
	};

	auto IsFragmentDensityMapAvailable = [](FOptionalVulkanDeviceExtensions& ExtensionFlags)
	{
		return ExtensionFlags.HasEXTFragmentDensityMap;
	};

	auto TurnOffFragmentShadingRate = [](VkPhysicalDeviceFragmentShadingRateFeaturesKHR& FragmentShadingRateFeatures)
	{
		FragmentShadingRateFeatures.primitiveFragmentShadingRate = VK_FALSE;
		FragmentShadingRateFeatures.attachmentFragmentShadingRate = VK_FALSE;
		FragmentShadingRateFeatures.pipelineFragmentShadingRate = VK_FALSE;
		GRHISupportsPipelineVariableRateShading = false;
		GRHISupportsLargerVariableRateShadingSizes = false;
	};

	auto TurnOffFragmentDensityMap = [](FOptionalVulkanDeviceExtensions& ExtensionFlags, VkPhysicalDeviceFragmentDensityMapFeaturesEXT& FragmentDensityMapFeatures, VkPhysicalDeviceFragmentDensityMap2FeaturesEXT& FragmentDensityMap2Features)
	{
		ExtensionFlags.HasEXTFragmentDensityMap = 0;
		FragmentDensityMapFeatures.fragmentDensityMap = VK_FALSE;
		FragmentDensityMapFeatures.fragmentDensityMapDynamic = VK_FALSE;
		FragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages = VK_FALSE;
		ExtensionFlags.HasEXTFragmentDensityMap2 = 0;
		FragmentDensityMap2Features.fragmentDensityMapDeferred = VK_FALSE;
	};

	int32 VRSFormatPreference = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.VRSFormat"))->GetValueOnAnyThread();
	UE_LOG(LogVulkanRHI, Display, TEXT("Vulkan Variable Rate Shading choice: %d."), VRSFormatPreference);

	// If both FSR and FDM are available we turn off the one that we're not using to prevent Vulkan validation layers warnings.
	if (IsFragmentDensityMapAvailable(OptionalDeviceExtensions) && IsFragmentShadingRateAvailable(OptionalDeviceExtensionProperties.FragmentShadingRateFeatures))
	{
		if (VRSFormatPreference <= (uint8)EVulkanVariableRateShadingPreference::RequireFSR)
		{
			TurnOffFragmentDensityMap(OptionalDeviceExtensions, OptionalDeviceExtensionProperties.FragmentDensityMapFeatures, OptionalDeviceExtensionProperties.FragmentDensityMap2Features);
		}
		else
		{
			TurnOffFragmentShadingRate(OptionalDeviceExtensionProperties.FragmentShadingRateFeatures);
		}
		return;
	}
	// When only FSR is available.
	if (IsFragmentShadingRateAvailable(OptionalDeviceExtensionProperties.FragmentShadingRateFeatures))
	{
		if (VRSFormatPreference == (uint8)EVulkanVariableRateShadingPreference::UseFDMOnlyIfAvailable)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Fragment Density Map was requested but is not available."));
		}
		else if (VRSFormatPreference == (uint8)EVulkanVariableRateShadingPreference::RequireFDM)
		{
			UE_LOG(LogVulkanRHI, Error, TEXT("Fragment Density Map was required but is not available."));
		}
		TurnOffFragmentDensityMap(OptionalDeviceExtensions, OptionalDeviceExtensionProperties.FragmentDensityMapFeatures, OptionalDeviceExtensionProperties.FragmentDensityMap2Features);
	}
	// When only FDM is available.
	if (IsFragmentDensityMapAvailable(OptionalDeviceExtensions))
	{
		if (VRSFormatPreference == (uint8)EVulkanVariableRateShadingPreference::UseFSROnlyIfAvailable)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Fragment Shading Rate was requested but is not available."));
		}
		else if (VRSFormatPreference == (uint8)EVulkanVariableRateShadingPreference::RequireFSR)
		{
			UE_LOG(LogVulkanRHI, Error, TEXT("Fragment Shading Rate was required but is not available."));
		}
		TurnOffFragmentShadingRate(OptionalDeviceExtensionProperties.FragmentShadingRateFeatures);
	}
}

void FVulkanDevice::InitGPU()
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanMisc);

	uint32 QueueCount = 0;
	VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &QueueCount, nullptr);
	check(QueueCount >= 1);

	QueueFamilyProps.AddUninitialized(QueueCount);
	VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &QueueCount, QueueFamilyProps.GetData());

	// Query base features
	PhysicalDeviceFeatures.Query(Gpu, RHI->GetApiVersion());

	// Setup layers and extensions
	FVulkanDeviceExtensionArray UEExtensions = FVulkanDeviceExtension::GetUESupportedDeviceExtensions(this, RHI->GetApiVersion());
	TArray<const ANSICHAR*> DeviceLayers = FVulkanDevice::SetupDeviceLayers(Gpu, UEExtensions);

	// Query advanced features
	{
		VkPhysicalDeviceFeatures2 PhysicalDeviceFeatures2;
		ZeroVulkanStruct(PhysicalDeviceFeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);

		for (TUniquePtr<FVulkanDeviceExtension>& UEExtension : UEExtensions)
		{
			if (UEExtension->InUse())
			{
				UEExtension->PrePhysicalDeviceFeatures(PhysicalDeviceFeatures2);
			}
		}

		VulkanRHI::vkGetPhysicalDeviceFeatures2(Gpu, &PhysicalDeviceFeatures2);

		for (TUniquePtr<FVulkanDeviceExtension>& UEExtension : UEExtensions)
		{
			if (UEExtension->InUse())
			{
				UEExtension->PostPhysicalDeviceFeatures(OptionalDeviceExtensions);
			}
		}
	}

	// Query advances properties
	{
		VkPhysicalDeviceProperties2 PhysicalDeviceProperties2;
		ZeroVulkanStruct(PhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
		PhysicalDeviceProperties2.pNext = &GpuIdProps;
		ZeroVulkanStruct(GpuIdProps, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);

		for (TUniquePtr<FVulkanDeviceExtension>& UEExtension : UEExtensions)
		{
			if (UEExtension->InUse())
			{
				UEExtension->PrePhysicalDeviceProperties(PhysicalDeviceProperties2);
			}
		}

		VulkanRHI::vkGetPhysicalDeviceProperties2(Gpu, &PhysicalDeviceProperties2);

		for (TUniquePtr<FVulkanDeviceExtension>& UEExtension : UEExtensions)
		{
			if (UEExtension->InUse())
			{
				UEExtension->PostPhysicalDeviceProperties();
			}
		}
	}

	ChooseVariableRateShadingMethod();

	UE_LOG(LogVulkanRHI, Display, TEXT("Device properties: Geometry %d BufferAtomic64 %d ImageAtomic64 %d"), 
		PhysicalDeviceFeatures.Core_1_0.geometryShader, OptionalDeviceExtensions.HasKHRShaderAtomicInt64, OptionalDeviceExtensions.HasImageAtomicInt64);

	CreateDevice(DeviceLayers, UEExtensions);

	FVulkanPlatform::InitDevice(this);

	SetupFormats();

	DeviceMemoryManager.Init(this);

	MemoryManager.Init();

	FenceManager.Init(this);

	StagingManager.Init(this);

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GGPUCrashDebuggingEnabled)
	{
		if (OptionalDeviceExtensions.HasAMDBufferMarker)
		{
			VkBufferCreateInfo CreateInfo;
			ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
			CreateInfo.size = GMaxCrashBufferEntries * sizeof(uint32_t);
			CreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VERIFYVULKANRESULT(VulkanRHI::vkCreateBuffer(Device, &CreateInfo, VULKAN_CPU_ALLOCATOR, &CrashMarker.Buffer));

			VkMemoryRequirements MemReq;
			FMemory::Memzero(MemReq);
			VulkanRHI::vkGetBufferMemoryRequirements(Device, CrashMarker.Buffer, &MemReq);

			CrashMarker.Allocation = DeviceMemoryManager.Alloc(false, CreateInfo.size, MemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nullptr, VULKAN_MEMORY_MEDIUM_PRIORITY, false, __FILE__, __LINE__);

			uint32* Entry = (uint32*)CrashMarker.Allocation->Map(VK_WHOLE_SIZE, 0);
			check(Entry);
			// Start with 0 entries
			*Entry = 0;
			VERIFYVULKANRESULT(VulkanRHI::vkBindBufferMemory(Device, CrashMarker.Buffer, CrashMarker.Allocation->GetHandle(), 0));
		}
	}
#endif

	RenderPassManager = new FVulkanRenderPassManager(this);

	if (UseVulkanDescriptorCache())
	{
		DescriptorSetCache = new FVulkanDescriptorSetCache(this);
	}
	
	DescriptorPoolsManager = new FVulkanDescriptorPoolsManager();
	DescriptorPoolsManager->Init(this);

	BindlessDescriptorManager = new FVulkanBindlessDescriptorManager(this);
	BindlessDescriptorManager->Init();

	PipelineStateCache = new FVulkanPipelineStateCacheManager(this);

	TArray<FString> CacheFilenames = FVulkanPlatform::GetPSOCacheFilenames();

	// always look in the saved directory (for the cache from previous run that wasn't moved over to stage directory)
	CacheFilenames.Add(VulkanRHI::GetPipelineCacheFilename());

	ImmediateContext = new FVulkanCommandListContextImmediate(GVulkanRHI, this, GfxQueue);

	if (GfxQueue->GetFamilyIndex() != ComputeQueue->GetFamilyIndex() && GRHIAllowAsyncComputeCvar.GetValueOnAnyThread() != 0)
	{
		ComputeContext = new FVulkanCommandListContextImmediate(GVulkanRHI, this, ComputeQueue);
		GEnableAsyncCompute = true;
	}
	else
	{
		ComputeContext = ImmediateContext;
	}

	extern TAutoConsoleVariable<int32> GRHIThreadCvar;
	if (GRHIThreadCvar->GetInt() > 1)
	{
		int32 Num = FTaskGraphInterface::Get().GetNumWorkerThreads();
		for (int32 Index = 0; Index < Num; Index++)
		{
			FVulkanCommandListContext* CmdContext = new FVulkanCommandListContext(GVulkanRHI, this, GfxQueue, ImmediateContext);
			CommandContexts.Add(CmdContext);
		}
	}

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	if (OptionalDeviceExtensions.HasEXTValidationCache)
	{
		LoadValidationCache(Device, ValidationCache);
	}
#endif

	FVulkanChunkedPipelineCacheManager::Init();

	PipelineStateCache->InitAndLoad(CacheFilenames);

	// Setup default resource
	{
		FSamplerStateInitializerRHI Default(SF_Point);
		DefaultSampler = ResourceCast(RHICreateSamplerState(Default).GetReference());

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FVulkanDevice_DefaultImage"), 1, 1, PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
			.SetInitialState(ERHIAccess::SRVMask);

		DefaultTexture = new FVulkanTexture(*this, Desc, nullptr);
	}

#if VULKAN_RHI_RAYTRACING
	if (RHISupportsRayTracing(GMaxRHIShaderPlatform) && GetOptionalExtensions().HasRaytracingExtensions())
	{
		check(RayTracingCompactionRequestHandler == nullptr);
		RayTracingCompactionRequestHandler = new FVulkanRayTracingCompactionRequestHandler(this);
	}
#endif

	FVulkanPlatform::PostInitGPU(*this);
}

void FVulkanDevice::PrepareForDestroy()
{
	WaitUntilIdle();
}

void FVulkanDevice::Destroy()
{
#if VULKAN_SUPPORTS_VALIDATION_CACHE
	if (ValidationCache != VK_NULL_HANDLE)
	{
		PFN_vkDestroyValidationCacheEXT vkDestroyValidationCache = (PFN_vkDestroyValidationCacheEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkDestroyValidationCacheEXT");
		if (vkDestroyValidationCache)
		{
			vkDestroyValidationCache(Device, ValidationCache, VULKAN_CPU_ALLOCATOR);
		}
	}
#endif

	// Release pending state that might hold references to RHI resources before we do final FlushPendingDeletes
	ImmediateContext->ReleasePendingState();
	if (ComputeContext && ComputeContext != ImmediateContext)
	{
		ComputeContext->ReleasePendingState();
	}

	if (TransientHeapCache)
	{
		delete TransientHeapCache;
		TransientHeapCache = nullptr;
	}

	// Flush all pending deletes before destroying the device and any Vulkan context objects.
	// Repeat until no new deletes are added
	int32 NumDeletes = 0;
	do
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		NumDeletes = RHICmdList.FlushPendingDeletes();
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	} while (NumDeletes > 0);

	delete DescriptorSetCache;
	DescriptorSetCache = nullptr;
	
	delete DescriptorPoolsManager;
	DescriptorPoolsManager = nullptr;

	// No need to delete as it's stored in SamplerMap
	DefaultSampler = nullptr;

	delete DefaultTexture;
	DefaultTexture = nullptr;

	for (int32 Index = CommandContexts.Num() - 1; Index >= 0; --Index)
	{
		delete CommandContexts[Index];
	}
	CommandContexts.Reset();

	if (ComputeContext != ImmediateContext)
	{
		delete ComputeContext;
	}
	ComputeContext = nullptr;

	delete ImmediateContext;
	ImmediateContext = nullptr;

	delete RenderPassManager;
	RenderPassManager = nullptr;

	for (FVulkanOcclusionQueryPool* Pool : UsedOcclusionQueryPools)
	{
		delete Pool;
	}
	UsedOcclusionQueryPools.SetNum(0, EAllowShrinking::No);
	for (FVulkanOcclusionQueryPool* Pool : FreeOcclusionQueryPools)
	{
		delete Pool;
	}
	FreeOcclusionQueryPools.SetNum(0, EAllowShrinking::No);

	delete PipelineStateCache;
	PipelineStateCache = nullptr;
	StagingManager.Deinit();

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GGPUCrashDebuggingEnabled)
	{
		if (CrashMarker.Buffer != VK_NULL_HANDLE)
		{
			VulkanRHI::vkDestroyBuffer(Device, CrashMarker.Buffer, VULKAN_CPU_ALLOCATOR);
			CrashMarker.Buffer = VK_NULL_HANDLE;
		}

		if (CrashMarker.Allocation)
		{
			CrashMarker.Allocation->Unmap();
			DeviceMemoryManager.Free(CrashMarker.Allocation);
		}
	}
#endif // VULKAN_SUPPORTS_GPU_CRASH_DUMPS

	DeferredDeletionQueue.Clear();

	BindlessDescriptorManager->Deinit();
	delete BindlessDescriptorManager;
	BindlessDescriptorManager = nullptr;

	MemoryManager.Deinit();

	delete TransferQueue;
	delete ComputeQueue;
	delete GfxQueue;

	FenceManager.Deinit();
	DeviceMemoryManager.Deinit();
	FVulkanChunkedPipelineCacheManager::Shutdown();

	VulkanRHI::vkDestroyDevice(Device, VULKAN_CPU_ALLOCATOR);
	Device = VK_NULL_HANDLE;
}

void FVulkanDevice::WaitUntilIdle()
{
	VERIFYVULKANRESULT(VulkanRHI::vkDeviceWaitIdle(Device));

	//#todo-rco: Loop through all contexts!
	GetImmediateContext().GetCommandBufferManager()->RefreshFenceStatus();
}

const VkComponentMapping& FVulkanDevice::GetFormatComponentMapping(EPixelFormat UEFormat) const
{
	check(GPixelFormats[UEFormat].Supported);
	return PixelFormatComponentMapping[UEFormat];
}

void FVulkanDevice::NotifyDeletedImage(VkImage Image, bool bRenderTarget)
{
	if (bRenderTarget)
	{
		// Contexts first, as it may clear the current framebuffer
		GetImmediateContext().NotifyDeletedRenderTarget(Image);
		// Delete framebuffers using this image
		GetRenderPassManager().NotifyDeletedRenderTarget(Image);
	}

	//#todo-jn: Loop through all contexts!  And all queues!
	GetImmediateContext().NotifyDeletedImage(Image);
}

void FVulkanDevice::PrepareForCPURead()
{
	//#todo-rco: Process other contexts first!
	ImmediateContext->PrepareForCPURead();
}

void FVulkanDevice::SubmitCommands(FVulkanCommandListContext* Context)
{
	FVulkanCommandBufferManager* CmdMgr = Context->GetCommandBufferManager();
	if (CmdMgr->HasPendingUploadCmdBuffer())
	{
		CmdMgr->SubmitUploadCmdBuffer();
	}
	if (CmdMgr->HasPendingActiveCmdBuffer())
	{
		CmdMgr->SubmitActiveCmdBuffer();
	}
	CmdMgr->PrepareForNewActiveCommandBuffer();
}

void FVulkanDevice::SubmitCommandsAndFlushGPU()
{
	if (ComputeContext != ImmediateContext)
	{
		SubmitCommands(ComputeContext);
	}

	SubmitCommands(ImmediateContext);

	//#todo-rco: Process other contexts first!
}

void FVulkanDevice::NotifyDeletedGfxPipeline(class FVulkanRHIGraphicsPipelineState* Pipeline)
{
	if (ComputeContext != ImmediateContext)
	{
		//ensure(0);
	}

	//#todo-rco: Loop through all contexts!
	if (ImmediateContext && ImmediateContext->PendingGfxState)
	{
		ImmediateContext->PendingGfxState->NotifyDeletedPipeline(Pipeline);
	}
}

void FVulkanDevice::NotifyDeletedComputePipeline(class FVulkanComputePipeline* Pipeline)
{
	if (ComputeContext && ComputeContext != ImmediateContext && ComputeContext->PendingComputeState)
	{
		ComputeContext->PendingComputeState->NotifyDeletedPipeline(Pipeline);
	}

	//#todo-rco: Loop through all contexts!
	if (ImmediateContext && ImmediateContext->PendingComputeState)
	{
		ImmediateContext->PendingComputeState->NotifyDeletedPipeline(Pipeline);
	}

	if (PipelineStateCache)
	{
		PipelineStateCache->NotifyDeletedComputePipeline(Pipeline);
	}
}

static FCriticalSection GContextCS;
FVulkanCommandListContext* FVulkanDevice::AcquireDeferredContext()
{
	FScopeLock Lock(&GContextCS);
	if (CommandContexts.Num() == 0)
	{
		return new FVulkanCommandListContext(GVulkanRHI, this, GfxQueue, ImmediateContext);
	}
	return CommandContexts.Pop(EAllowShrinking::No);
}

void FVulkanDevice::ReleaseDeferredContext(FVulkanCommandListContext* InContext)
{
	check(InContext);
	{
		FScopeLock Lock(&GContextCS);
		CommandContexts.Push(InContext);
	}
}

void FVulkanDevice::VulkanSetObjectName(VkObjectType Type, uint64_t Handle, const TCHAR* Name)
{
#if VULKAN_ENABLE_DRAW_MARKERS
	if(DebugMarkers.SetDebugName)
	{
		FTCHARToUTF8 Converter(Name);
		VkDebugUtilsObjectNameInfoEXT Info;
		ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
		Info.objectType = Type;
		Info.objectHandle = Handle;
		Info.pObjectName = Converter.Get();
		DebugMarkers.SetDebugName(Device, &Info);
	}
#endif // VULKAN_ENABLE_DRAW_MARKERS
}

FVulkanTransientHeapCache& FVulkanDevice::GetOrCreateTransientHeapCache()
{
	if (!TransientHeapCache)
	{
		TransientHeapCache = FVulkanTransientHeapCache::Create(this);
	}
	return *TransientHeapCache;
}

FGPUTimingCalibrationTimestamp FVulkanDevice::GetCalibrationTimestamp()
{
	auto ToMicroseconds = [](uint64_t Timestamp)
	{
		const double Frequency = double(FVulkanGPUTiming::GetTimingFrequency());
		uint64 Microseconds = (uint64)((double(Timestamp) / Frequency) * 1000.0 * 1000.0);
		return Microseconds;
	};

	FGPUTimingCalibrationTimestamp CalibrationTimestamp;
	if (OptionalDeviceExtensions.HasEXTCalibratedTimestamps)
	{
		VkCalibratedTimestampInfoEXT TimestampInfo;
		ZeroVulkanStruct(TimestampInfo, VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT);
		TimestampInfo.timeDomain = VK_TIME_DOMAIN_DEVICE_EXT;

		uint64_t GPUTimestamp = 0;
		uint64_t MaxDeviation = 0;
		VERIFYVULKANRESULT(VulkanRHI::vkGetCalibratedTimestampsEXT(Device, 1, &TimestampInfo, &GPUTimestamp, &MaxDeviation));
		CalibrationTimestamp.GPUMicroseconds = ToMicroseconds(GPUTimestamp);

		const uint64 CPUTimestamp = FPlatformTime::Cycles64();
		CalibrationTimestamp.CPUMicroseconds = uint64(FPlatformTime::ToSeconds64(CPUTimestamp) * 1e6);
	}
	return CalibrationTimestamp;
}
