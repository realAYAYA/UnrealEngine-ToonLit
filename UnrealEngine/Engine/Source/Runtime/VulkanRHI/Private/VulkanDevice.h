// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanDevice.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanMemory.h"

class FVulkanDescriptorSetCache;
class FVulkanDescriptorPool;
class FVulkanDescriptorPoolsManager;
class FVulkanBindlessDescriptorManager;
class FVulkanCommandListContextImmediate;
class FVulkanTransientHeapCache;
class FVulkanDeviceExtension;
class FVulkanOcclusionQueryPool;
class FVulkanRenderPassManager;

#if VULKAN_RHI_RAYTRACING
class FVulkanRayTracingCompactionRequestHandler;
#endif

// HOTFIX for UE-218250: Disable vulkan debug names to get around crash/performance issues
#define VULKAN_USE_DEBUG_NAMES 0

#if VULKAN_USE_DEBUG_NAMES
#define VULKAN_SET_DEBUG_NAME(Device, Type, Handle, Format, ...) Device.VulkanSetObjectName(Type, (uint64)Handle, *FString::Printf(Format, __VA_ARGS__))
#else
#define VULKAN_SET_DEBUG_NAME(Device, Type, Handle, Format, ...) do{}while(0)
#endif

struct FOptionalVulkanDeviceExtensions
{
	union
	{
		struct
		{
			// Optional Extensions
			uint64 HasEXTValidationCache : 1;
			uint64 HasMemoryPriority : 1;
			uint64 HasMemoryBudget : 1;
			uint64 HasEXTASTCDecodeMode : 1;
			uint64 HasEXTFragmentDensityMap : 1;
			uint64 HasEXTFragmentDensityMap2 : 1;
			uint64 HasKHRFragmentShadingRate : 1;
			uint64 HasEXTFullscreenExclusive : 1;
			uint64 HasImageAtomicInt64 : 1;
			uint64 HasAccelerationStructure : 1;
			uint64 HasRayTracingPipeline : 1;
			uint64 HasRayQuery : 1;
			uint64 HasDeferredHostOperations : 1;
			uint64 HasEXTCalibratedTimestamps : 1;
			uint64 HasEXTDescriptorBuffer : 1;
			uint64 HasEXTDeviceFault : 1;

			// Vendor specific
			uint64 HasAMDBufferMarker : 1;
			uint64 HasNVDiagnosticCheckpoints : 1;
			uint64 HasNVDeviceDiagnosticConfig : 1;
			uint64 HasQcomRenderPassTransform : 1;

			// Promoted to 1.1
			uint64 HasKHRMultiview : 1;
			uint64 HasKHR16bitStorage : 1;

			// Promoted to 1.2
			uint64 HasKHRRenderPass2 : 1;
			uint64 HasKHRImageFormatList : 1;
			uint64 HasKHRShaderAtomicInt64 : 1;
			uint64 HasEXTScalarBlockLayout : 1;
			uint64 HasBufferDeviceAddress : 1;
			uint64 HasSPIRV_14 : 1;
			uint64 HasShaderFloatControls : 1;
			uint64 HasKHRShaderFloat16 : 1;
			uint64 HasEXTDescriptorIndexing : 1;
			uint64 HasEXTShaderViewportIndexLayer : 1;
			uint64 HasSeparateDepthStencilLayouts : 1;
			uint64 HasEXTHostQueryReset : 1;
			uint64 HasQcomRenderPassShaderResolve : 1;

			// Promoted to 1.3
			uint64 HasEXTTextureCompressionASTCHDR : 1;
			uint64 HasKHRMaintenance4 : 1;
			uint64 HasKHRSynchronization2 : 1;
			uint64 HasEXTSubgroupSizeControl : 1;
			uint64 HasEXTPipelineCreationCacheControl : 1;
		};
		uint64 Packed;
	};

	FOptionalVulkanDeviceExtensions()
	{
		static_assert(sizeof(Packed) == sizeof(FOptionalVulkanDeviceExtensions), "More bits needed for Packed!");
		Packed = 0;
	}

	inline bool HasGPUCrashDumpExtensions() const
	{
		return HasAMDBufferMarker || HasNVDiagnosticCheckpoints;
	}

#if VULKAN_RHI_RAYTRACING
	inline bool HasRaytracingExtensions() const
	{
		return 
			HasAccelerationStructure && 
			(HasRayTracingPipeline || HasRayQuery) &&
			HasEXTDescriptorIndexing &&
			HasBufferDeviceAddress && 
			HasDeferredHostOperations && 
			HasSPIRV_14 && 
			HasShaderFloatControls;
	}
#endif
};

// All the features and properties we need to keep around from extension initialization
struct FOptionalVulkanDeviceExtensionProperties
{
	FOptionalVulkanDeviceExtensionProperties()
	{
		FMemory::Memzero(*this);
	}

	VkPhysicalDeviceDescriptorBufferPropertiesEXT DescriptorBufferProps;
	VkPhysicalDeviceSubgroupSizeControlPropertiesEXT SubgroupSizeControlProperties;

#if VULKAN_RHI_RAYTRACING
	VkPhysicalDeviceAccelerationStructurePropertiesKHR AccelerationStructureProps;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProps;
#endif // VULKAN_RHI_RAYTRACING

	VkPhysicalDeviceFragmentShadingRateFeaturesKHR FragmentShadingRateFeatures;
	VkPhysicalDeviceFragmentDensityMapFeaturesEXT FragmentDensityMapFeatures;
	VkPhysicalDeviceFragmentDensityMap2FeaturesEXT FragmentDensityMap2Features;
};

class FVulkanPhysicalDeviceFeatures
{
public:
	FVulkanPhysicalDeviceFeatures()
	{
		FMemory::Memzero(*this);
	}

	void Query(VkPhysicalDevice PhysicalDevice, uint32 APIVersion);

	VkPhysicalDeviceFeatures	     Core_1_0;
	VkPhysicalDeviceVulkan11Features Core_1_1;
private:
	// Anything above Core 1.1 cannot be assumed, they should only be used by the device at init time
	VkPhysicalDeviceVulkan12Features Core_1_2;
	VkPhysicalDeviceVulkan13Features Core_1_3;

	friend class FVulkanDevice;
};


namespace VulkanRHI
{
	class FDeferredDeletionQueue2 : public FDeviceChild
	{

	public:
		FDeferredDeletionQueue2(FVulkanDevice* InDevice);
		~FDeferredDeletionQueue2();

		enum class EType
		{
			RenderPass,
			Buffer,
			BufferView,
			Image,
			ImageView,
			Pipeline,
			PipelineLayout,
			Framebuffer,
			DescriptorSetLayout,
			Sampler,
			Semaphore,
			ShaderModule,
			Event,
			ResourceAllocation,
			DeviceMemoryAllocation,
			BufferSuballocation,
			AccelerationStructure,
			BindlessHandle,
		};

		template <typename T>
		inline void EnqueueResource(EType Type, T Handle)
		{
			static_assert(sizeof(T) <= sizeof(uint64), "Vulkan resource handle type size too large.");
			EnqueueGenericResource(Type, (uint64)Handle);
		}

		inline void EnqueueBindlessHandle(FRHIDescriptorHandle DescriptorHandle)
		{
			if (DescriptorHandle.IsValid())
			{
				const uint64 Type = (uint64)DescriptorHandle.GetRawType();
				const uint64 Index = (uint64)DescriptorHandle.GetIndex();
				const uint64 AsUInt64 = (Type << 32) | Index;
				EnqueueResource(EType::BindlessHandle, AsUInt64);
			}
		}

		void EnqueueResourceAllocation(FVulkanAllocation& Allocation);
		void EnqueueDeviceAllocation(FDeviceMemoryAllocation* DeviceMemoryAllocation);

		void ReleaseResources(bool bDeleteImmediately = false);

		inline void Clear()
		{
			ReleaseResources(true);
		}

		void OnCmdBufferDeleted(FVulkanCmdBuffer* CmdBuffer);
	private:
		void EnqueueGenericResource(EType Type, uint64 Handle);

		struct FEntry
		{
			EType StructureType;
			uint32 FrameNumber;
			uint64 FenceCounter;
			FVulkanCmdBuffer* CmdBuffer;

			uint64 Handle;
			FVulkanAllocation Allocation;
			FDeviceMemoryAllocation* DeviceMemoryAllocation;
		};
		FCriticalSection CS;
		TArray<FEntry> Entries;
	};
}


class FVulkanDevice
{
public:
	FVulkanDevice(FVulkanDynamicRHI* InRHI, VkPhysicalDevice Gpu);

	~FVulkanDevice();

	void InitGPU();

	void CreateDevice(TArray<const ANSICHAR*>& DeviceLayers, FVulkanDeviceExtensionArray& UEExtensions);
	void ChooseVariableRateShadingMethod();

	void PrepareForDestroy();
	void Destroy();

	void WaitUntilIdle();

	inline EGpuVendorId GetVendorId() const
	{
		return VendorId;
	}

	inline bool HasAsyncComputeQueue() const
	{
		return bAsyncComputeQueue;
	}

	inline bool CanPresentOnComputeQueue() const
	{
		return bPresentOnComputeQueue;
	}

	inline bool IsRealAsyncComputeContext(const FVulkanCommandListContext* InContext) const
	{
		if (bAsyncComputeQueue)
		{
			ensure((FVulkanCommandListContext*)ImmediateContext != ComputeContext);
			return InContext == ComputeContext;
		}
	
		return false;
	}

	inline FVulkanQueue* GetGraphicsQueue()
	{
		return GfxQueue;
	}

	inline FVulkanQueue* GetComputeQueue()
	{
		return ComputeQueue;
	}

	inline FVulkanQueue* GetTransferQueue()
	{
		return TransferQueue;
	}

	inline FVulkanQueue* GetPresentQueue()
	{
		return PresentQueue;
	}

	inline VkPhysicalDevice GetPhysicalHandle() const
	{
		return Gpu;
	}

	inline const VkPhysicalDeviceProperties& GetDeviceProperties() const
	{
		return GpuProps;
	}

	inline VkExtent2D GetBestMatchedFragmentSize(EVRSShadingRate Rate) const
	{
		return FragmentSizeMap[Rate];
	}

	inline const VkPhysicalDeviceLimits& GetLimits() const
	{
		return GpuProps.limits;
	}

	inline const VkPhysicalDeviceIDPropertiesKHR& GetDeviceIdProperties() const
	{
		return GpuIdProps;
	}

	inline const VkPhysicalDeviceSubgroupProperties& GetDeviceSubgroupProperties() const
	{
		return GpuSubgroupProps;
	}

#if VULKAN_RHI_RAYTRACING
	FVulkanRayTracingCompactionRequestHandler* GetRayTracingCompactionRequestHandler() { return RayTracingCompactionRequestHandler; }

	void InitializeRayTracing();
	void CleanUpRayTracing();
#endif // VULKAN_RHI_RAYTRACING

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	inline VkValidationCacheEXT GetValidationCache() const
	{
		return ValidationCache;
	}
#endif

	inline const FVulkanPhysicalDeviceFeatures& GetPhysicalDeviceFeatures() const
	{
		return PhysicalDeviceFeatures;
	}

	inline bool HasUnifiedMemory() const
	{
		return DeviceMemoryManager.HasUnifiedMemory();
	}

	bool SupportsBindless() const;

	inline uint64 GetTimestampValidBitsMask() const
	{
		return TimestampValidBitsMask;
	}

	const VkComponentMapping& GetFormatComponentMapping(EPixelFormat UEFormat) const;

	inline VkDevice GetInstanceHandle() const
	{
		return Device;
	}

	inline const FVulkanSamplerState& GetDefaultSampler() const
	{
		return *DefaultSampler;
	}

	inline const FVulkanView::FTextureView& GetDefaultImageView() const
	{
		return DefaultTexture->DefaultView->GetTextureView();
	}

	const VkFormatProperties& GetFormatProperties(VkFormat InFormat) const;

	inline VulkanRHI::FDeviceMemoryManager& GetDeviceMemoryManager()
	{
		return DeviceMemoryManager;
	}

	inline const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const
	{
		return DeviceMemoryManager.GetMemoryProperties();
	}

	inline VulkanRHI::FMemoryManager& GetMemoryManager()
	{
		return MemoryManager;
	}

	inline VulkanRHI::FDeferredDeletionQueue2& GetDeferredDeletionQueue()
	{
		return DeferredDeletionQueue;
	}

	inline VulkanRHI::FStagingManager& GetStagingManager()
	{
		return StagingManager;
	}

	inline VulkanRHI::FFenceManager& GetFenceManager()
	{
		return FenceManager;
	}

	inline FVulkanRenderPassManager& GetRenderPassManager()
	{
		return *RenderPassManager;
	}

	inline FVulkanDescriptorSetCache& GetDescriptorSetCache()
	{
		return *DescriptorSetCache;
	}

	inline FVulkanDescriptorPoolsManager& GetDescriptorPoolsManager()
	{
		return *DescriptorPoolsManager;
	}

	inline FVulkanBindlessDescriptorManager* GetBindlessDescriptorManager()
	{
		return BindlessDescriptorManager;
	}

	inline TMap<uint32, FSamplerStateRHIRef>& GetSamplerMap()
	{
		return SamplerMap;
	}

	inline FVulkanShaderFactory& GetShaderFactory()
	{
		return ShaderFactory;
	}

	FVulkanCommandListContextImmediate& GetImmediateContext();

	inline FVulkanCommandListContext& GetImmediateComputeContext()
	{
		return *ComputeContext;
	}

	void NotifyDeletedImage(VkImage Image, bool bRenderTarget);

#if VULKAN_ENABLE_DRAW_MARKERS
	inline PFN_vkCmdBeginDebugUtilsLabelEXT GetCmdBeginDebugLabel() const
	{
		return DebugMarkers.CmdBeginDebugLabel;
	}

	inline PFN_vkCmdEndDebugUtilsLabelEXT GetCmdEndDebugLabel() const
	{
		return DebugMarkers.CmdEndDebugLabel;
	}

	inline PFN_vkSetDebugUtilsObjectNameEXT GetSetDebugName() const
	{
		return DebugMarkers.SetDebugName;
	}
#endif

	void PrepareForCPURead();

	void SubmitCommandsAndFlushGPU();

	FVulkanOcclusionQueryPool* AcquireOcclusionQueryPool(FVulkanCommandBufferManager* CommandBufferManager, uint32 NumQueries);
	void ReleaseUnusedOcclusionQueryPools();

	inline class FVulkanPipelineStateCacheManager* GetPipelineStateCache()
	{
		return PipelineStateCache;
	}

	void NotifyDeletedGfxPipeline(class FVulkanRHIGraphicsPipelineState* Pipeline);
	void NotifyDeletedComputePipeline(class FVulkanComputePipeline* Pipeline);

	FVulkanCommandListContext* AcquireDeferredContext();
	void ReleaseDeferredContext(FVulkanCommandListContext* InContext);
	void VulkanSetObjectName(VkObjectType Type, uint64_t Handle, const TCHAR* Name);
	inline const FOptionalVulkanDeviceExtensions& GetOptionalExtensions() const
	{
		return OptionalDeviceExtensions;
	}

	inline const FOptionalVulkanDeviceExtensionProperties& GetOptionalExtensionProperties() const
	{
		return OptionalDeviceExtensionProperties;
	}

	inline bool SupportsParallelRendering() const
	{
		return OptionalDeviceExtensions.HasSeparateDepthStencilLayouts && OptionalDeviceExtensions.HasKHRSynchronization2 && OptionalDeviceExtensions.HasKHRRenderPass2;
	}

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	VkBuffer GetCrashMarkerBuffer() const
	{
		return CrashMarker.Buffer;
	}

	void* GetCrashMarkerMappedPointer() const
	{
		return (CrashMarker.Allocation != nullptr) ? CrashMarker.Allocation->GetMappedPointer() : nullptr;
	}
#endif

	void SetupPresentQueue(VkSurfaceKHR Surface);

	inline const TArray<VkQueueFamilyProperties>& GetQueueFamilyProps()
	{
		return QueueFamilyProps;
	}

	FVulkanTransientHeapCache& GetOrCreateTransientHeapCache();

	const TArray<const ANSICHAR*>& GetDeviceExtensions() { return DeviceExtensions; }

	// Performs a GPU and CPU timestamp at nearly the same time.
	// This allows aligning GPU and CPU events on the same timeline in profile visualization.
	FGPUTimingCalibrationTimestamp GetCalibrationTimestamp();

private:
	void MapBufferFormatSupport(FPixelFormatInfo& PixelFormatInfo, EPixelFormat UEFormat, VkFormat VulkanFormat);
	void MapImageFormatSupport(FPixelFormatInfo& PixelFormatInfo, const TArrayView<const VkFormat>& PrioritizedFormats, EPixelFormatCapabilities RequiredCapabilities);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, EPixelFormatCapabilities RequiredCapabilities, int32 BlockBytes);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, int32 BlockBytes);
	void MapFormatSupport(EPixelFormat UEFormat, std::initializer_list<VkFormat> PrioritizedFormats, const VkComponentMapping& ComponentMapping, EPixelFormatCapabilities RequiredCapabilities);

	void SubmitCommands(FVulkanCommandListContext* Context);


	VkDevice Device;

	VulkanRHI::FDeviceMemoryManager DeviceMemoryManager;

	VulkanRHI::FMemoryManager MemoryManager;

	VulkanRHI::FDeferredDeletionQueue2 DeferredDeletionQueue;

	VulkanRHI::FStagingManager StagingManager;

	VulkanRHI::FFenceManager FenceManager;

	FVulkanRenderPassManager* RenderPassManager;

	FVulkanTransientHeapCache* TransientHeapCache = nullptr;

	// Active on ES3.1
	FVulkanDescriptorSetCache* DescriptorSetCache = nullptr;
	// Active on >= SM4
	FVulkanDescriptorPoolsManager* DescriptorPoolsManager = nullptr;

	FVulkanBindlessDescriptorManager* BindlessDescriptorManager = nullptr;

	FVulkanShaderFactory ShaderFactory;

	FVulkanSamplerState* DefaultSampler;
	FVulkanTexture* DefaultTexture;

	VkPhysicalDevice Gpu;
	VkPhysicalDeviceProperties GpuProps;

	TArray<VkPhysicalDeviceFragmentShadingRateKHR> FragmentShadingRates;
	TStaticArray<VkExtent2D, (EVRSShadingRate::VRSSR_Last+1)> FragmentSizeMap;

	// Extension specific properties
	VkPhysicalDeviceIDPropertiesKHR GpuIdProps;
	VkPhysicalDeviceSubgroupProperties GpuSubgroupProps;

#if VULKAN_RHI_RAYTRACING
	FVulkanRayTracingCompactionRequestHandler* RayTracingCompactionRequestHandler = nullptr;
#endif // VULKAN_RHI_RAYTRACING

	FVulkanPhysicalDeviceFeatures PhysicalDeviceFeatures;

	TArray<VkQueueFamilyProperties> QueueFamilyProps;
	VkFormatProperties FormatProperties[VK_FORMAT_RANGE_SIZE];
	// Info for formats that are not in the core Vulkan spec (i.e. extensions)
	mutable TMap<VkFormat, VkFormatProperties> ExtensionFormatProperties;

	TArray<FVulkanOcclusionQueryPool*> UsedOcclusionQueryPools;
	TArray<FVulkanOcclusionQueryPool*> FreeOcclusionQueryPools;

	uint64 TimestampValidBitsMask = 0;

	FVulkanQueue* GfxQueue;
	FVulkanQueue* ComputeQueue;
	FVulkanQueue* TransferQueue;
	FVulkanQueue* PresentQueue;
	bool bAsyncComputeQueue = false;
	bool bPresentOnComputeQueue = false;

	EGpuVendorId VendorId = EGpuVendorId::NotQueried;

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	struct
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		VulkanRHI::FDeviceMemoryAllocation* Allocation = nullptr;
	} CrashMarker;
#endif

	VkComponentMapping PixelFormatComponentMapping[PF_MAX];

	TMap<uint32, FSamplerStateRHIRef> SamplerMap;

	FVulkanCommandListContextImmediate* ImmediateContext;
	FVulkanCommandListContext* ComputeContext;
	TArray<FVulkanCommandListContext*> CommandContexts;

	FVulkanDynamicRHI* RHI = nullptr;
	bool bDebugMarkersFound = false;
	
	static TArray<const ANSICHAR*> SetupDeviceLayers(VkPhysicalDevice Gpu, FVulkanDeviceExtensionArray& UEExtensions);

	FOptionalVulkanDeviceExtensions	OptionalDeviceExtensions;
	FOptionalVulkanDeviceExtensionProperties OptionalDeviceExtensionProperties;
	TArray<const ANSICHAR*>			DeviceExtensions;

	void SetupFormats();

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VkValidationCacheEXT ValidationCache = VK_NULL_HANDLE;
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
	struct
	{
		PFN_vkSetDebugUtilsObjectNameEXT	SetDebugName = nullptr;
		PFN_vkCmdBeginDebugUtilsLabelEXT	CmdBeginDebugLabel = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT		CmdEndDebugLabel = nullptr;
	} DebugMarkers;
	friend class FVulkanCommandListContext;
#endif
	void SetupDrawMarkers();

	class FVulkanPipelineStateCacheManager* PipelineStateCache;
	friend class FVulkanDynamicRHI;
	friend class FVulkanRHIGraphicsPipelineState;
};
