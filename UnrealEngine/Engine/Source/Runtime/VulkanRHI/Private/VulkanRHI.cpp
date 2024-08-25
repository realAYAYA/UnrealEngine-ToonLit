// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRHI.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "BuildSettings.h"
#include "HardwareInfo.h"
#include "VulkanShaderResources.h"
#include "VulkanResources.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanBarriers.h"
#include "Misc/CommandLine.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "Modules/ModuleManager.h"
#include "VulkanPipelineState.h"
#include "Misc/FileHelper.h"
#include "VulkanLLM.h"
#include "Misc/EngineVersion.h"
#include "GlobalShader.h"
#include "RHIValidation.h"
#include "RHIUtilities.h"
#include "IHeadMountedDisplayModule.h"
#include "VulkanRenderpass.h"
#include "VulkanTransientResourceAllocator.h"
#include "VulkanExtensions.h"
#include "VulkanRayTracing.h"
#include "VulkanChunkedPipelineCache.h"
#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
#endif

// Use Vulkan Profiles to verify feature level support on startup
void VulkanProfilePrint(const char* Msg)
{
	UE_LOG(LogVulkanRHI, Log, TEXT("   - %s"), ANSI_TO_TCHAR(Msg));
}
#define VP_DEBUG_MESSAGE_CALLBACK VulkanProfilePrint
#include "vulkan_profiles_ue.h"
#undef VP_DEBUG_MESSAGE_CALLBACK


static_assert(sizeof(VkStructureType) == sizeof(int32), "ZeroVulkanStruct() assumes VkStructureType is int32!");

#if NV_AFTERMATH
bool GVulkanNVAftermathModuleLoaded = false;
#endif

TAtomic<uint64> GVulkanBufferHandleIdCounter{ 0 };
TAtomic<uint64> GVulkanBufferViewHandleIdCounter{ 0 };
TAtomic<uint64> GVulkanImageViewHandleIdCounter{ 0 };
TAtomic<uint64> GVulkanSamplerHandleIdCounter{ 0 };
TAtomic<uint64> GVulkanDSetLayoutHandleIdCounter{ 0 };

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
#include "IHeadMountedDisplayModule.h"
#endif

#define LOCTEXT_NAMESPACE "VulkanRHI"


///////////////////////////////////////////////////////////////////////////////

TAutoConsoleVariable<int32> GRHIThreadCvar(
	TEXT("r.Vulkan.RHIThread"),
	1,
	TEXT("0 to only use Render Thread\n")
	TEXT("1 to use ONE RHI Thread\n")
	TEXT("2 to use multiple RHI Thread\n")
);

int32 GVulkanInputAttachmentShaderRead = 0;
static FAutoConsoleVariableRef GCVarInputAttachmentShaderRead(
	TEXT("r.Vulkan.InputAttachmentShaderRead"),
	GVulkanInputAttachmentShaderRead,
	TEXT("Whether to use VK_ACCESS_SHADER_READ_BIT an input attachments to workaround rendering issues\n")
	TEXT("0 use: VK_ACCESS_INPUT_ATTACHMENT_READ_BIT (default)\n")
	TEXT("1 use: VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT\n"),
	ECVF_ReadOnly
);

int32 GVulkanEnableTransientResourceAllocator = 1;
static FAutoConsoleVariableRef GCVarEnableTransientResourceAllocator(
	TEXT("r.Vulkan.EnableTransientResourceAllocator"),
	GVulkanEnableTransientResourceAllocator,
	TEXT("Whether to enable the TransientResourceAllocator to reduce memory usage\n")
	TEXT("0 to disabled (default)\n")
	TEXT("1 to enable\n"),
	ECVF_ReadOnly
);

int32 GVulkanAllowVariableRateShading = 1;
static FAutoConsoleVariableRef CVarVulkanVariableRateShading(
	TEXT("r.Vulkan.AllowVariableRateShading"),
	GVulkanAllowVariableRateShading,
	TEXT("0 to disable variable rate shading")
	TEXT("1 to allow use of variable rate shading if available (default)"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<bool> CVarAllowVulkanPSOPrecache(
	TEXT("r.Vulkan.AllowPSOPrecaching"),
	false,
	TEXT("true: if r.PSOPrecaching=1 Vulkan RHI will use precaching.\n")
	TEXT("false: Vulkan RHI will disable precaching (even if r.PSOPrecaching=1). (default)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

// If precaching is active we should not need the file cache.
// however, precaching and filecache are compatible with each other, there maybe some scenarios in which both could be used.
static TAutoConsoleVariable<bool> CVarEnableVulkanPSOFileCacheWhenPrecachingActive(
	TEXT("r.Vulkan.EnablePSOFileCacheWhenPrecachingActive"),
	false,
	TEXT("false: If precaching is available (r.PSOPrecaching=1, r.Vulkan.UseChunkedPSOCache=1) then disable the PSO filecache. (default)\n")
	TEXT("true: Allow both PSO file cache and precaching."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

bool GGPUCrashDebuggingEnabled = false;

extern TAutoConsoleVariable<int32> GVulkanRayTracingCVar;

// All shader stages supported by VK device - VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, FRAGMENT etc
uint32 GVulkanDevicePipelineStageBits = 0;

DEFINE_LOG_CATEGORY(LogVulkan)

// Selects the device to us for the provided instance
static VkPhysicalDevice SelectPhysicalDevice(VkInstance InInstance)
{
	VkResult Result;

	uint32 PhysicalDeviceCount = 0;
	Result = VulkanRHI::vkEnumeratePhysicalDevices(InInstance, &PhysicalDeviceCount, nullptr);
	if ((Result != VK_SUCCESS) || (PhysicalDeviceCount == 0))
	{
		UE_LOG(LogVulkanRHI, Log, 
			TEXT("SelectPhysicalDevice could not find a compatible Vulkan device or driver (EnumeratePhysicalDevices returned '%s' and %d devices).  ")
			TEXT("Make sure your video card supports Vulkan and try updating your video driver to a more recent version (proceed with any pending reboots).")
			, VK_TYPE_TO_STRING(VkResult, Result), PhysicalDeviceCount
		);

		return VK_NULL_HANDLE;
	}

	TArray<VkPhysicalDevice> PhysicalDevices;
	PhysicalDevices.AddZeroed(PhysicalDeviceCount);
	VERIFYVULKANRESULT(VulkanRHI::vkEnumeratePhysicalDevices(InInstance, &PhysicalDeviceCount, PhysicalDevices.GetData()));
	checkf(PhysicalDeviceCount >= 1, TEXT("Couldn't enumerate physical devices on second attempt! Make sure your drivers are up to date and that you are not pending a reboot."));

	struct FPhysicalDeviceInfo
	{
		FPhysicalDeviceInfo() = delete;
		FPhysicalDeviceInfo(uint32 InOriginalIndex, VkPhysicalDevice InPhysicalDevice)
			: OriginalIndex(InOriginalIndex)
			, PhysicalDevice(InPhysicalDevice)
		{
			ZeroVulkanStruct(PhysicalDeviceProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
			ZeroVulkanStruct(PhysicalDeviceIDProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
			PhysicalDeviceProperties2.pNext = &PhysicalDeviceIDProperties;
			VulkanRHI::vkGetPhysicalDeviceProperties2(PhysicalDevice, &PhysicalDeviceProperties2);
		}

		uint32 OriginalIndex;
		VkPhysicalDevice PhysicalDevice;
		VkPhysicalDeviceProperties2 PhysicalDeviceProperties2;
		VkPhysicalDeviceIDProperties PhysicalDeviceIDProperties;
	};
	TArray<FPhysicalDeviceInfo> PhysicalDeviceInfos;
	PhysicalDeviceInfos.Reserve(PhysicalDeviceCount);

	// Fill the array with each devices properties
	for (uint32 Index = 0; Index < PhysicalDeviceCount; ++Index)
	{
		PhysicalDeviceInfos.Emplace(Index, PhysicalDevices[Index]);
	}

	// Allow HMD to override which graphics adapter is chosen, so we pick the adapter where the HMD is connected
#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	if (IHeadMountedDisplayModule::IsAvailable())
	{
		static_assert(sizeof(uint64) == VK_LUID_SIZE);
		const uint64 HmdGraphicsAdapterLuid = IHeadMountedDisplayModule::Get().GetGraphicsAdapterLuid();

		for (int32 Index = 0; Index < PhysicalDeviceInfos.Num(); ++Index)
		{
			if (FMemory::Memcmp(&HmdGraphicsAdapterLuid, &PhysicalDeviceInfos[Index].PhysicalDeviceIDProperties.deviceLUID, VK_LUID_SIZE_KHR) == 0)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("HMD device at index %d of %u being used as default..."), Index, PhysicalDeviceCount);
				return PhysicalDeviceInfos[Index].PhysicalDevice;
			}
		}
	}
#endif

	// Use the device as forced by CVar or CommandLine arg
	auto* CVarGraphicsAdapter = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GraphicsAdapter"));
	int32 ExplicitAdapterValue = CVarGraphicsAdapter ? CVarGraphicsAdapter->GetValueOnAnyThread() : -1;
	const bool bUsingCmdLine = FParse::Value(FCommandLine::Get(), TEXT("graphicsadapter="), ExplicitAdapterValue);
	const TCHAR* GraphicsAdapterOriginTxt = bUsingCmdLine ? TEXT("command line") : TEXT("'r.GraphicsAdapter'");
	if (ExplicitAdapterValue >= 0)  // Use adapter at the specified index
	{
		if (ExplicitAdapterValue >= PhysicalDeviceInfos.Num())
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Tried to use graphics adapter at index %d as specified by %s, but only %d Adapter(s) found. Falling back to first device..."), 
				ExplicitAdapterValue, GraphicsAdapterOriginTxt, PhysicalDeviceInfos.Num());
			ExplicitAdapterValue = 0;
		}

		UE_LOG(LogVulkanRHI, Log, TEXT("Using device at index %d of %u as specfified by %s..."), ExplicitAdapterValue, PhysicalDeviceCount, GraphicsAdapterOriginTxt);
		return PhysicalDeviceInfos[ExplicitAdapterValue].PhysicalDevice;
	}
	else if (ExplicitAdapterValue == -2)  // Take the first one that fulfills the criteria
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("Using first device (of %u) without any sorting as specfified by %s..."), PhysicalDeviceCount, GraphicsAdapterOriginTxt);
		return PhysicalDeviceInfos[0].PhysicalDevice;
	}
	else if (ExplicitAdapterValue == -1)  // Favour non integrated because there are usually faster
	{
		// Reoreder the list to place discrete adapters first
		PhysicalDeviceInfos.Sort([](const FPhysicalDeviceInfo& Lhs, const FPhysicalDeviceInfo& Rhs)
			{
				// For devices of the same type, jsut keep the original order
				if (Lhs.PhysicalDeviceProperties2.properties.deviceType == Rhs.PhysicalDeviceProperties2.properties.deviceType)
				{
					return Lhs.OriginalIndex < Rhs.OriginalIndex;
				}
				
				// Prefer discrete GPUs first, then integrated, then CPU
				return (Lhs.PhysicalDeviceProperties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) || 
					(Rhs.PhysicalDeviceProperties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
			});
	}


	// If a preferred vendor is specified, return the first device from that vendor
	const EGpuVendorId PreferredVendor = RHIGetPreferredAdapterVendor();
	if (PreferredVendor != EGpuVendorId::Unknown)
	{
		// Check for preferred
		for (int32 Index = 0; Index < PhysicalDeviceInfos.Num(); ++Index)
		{
			if (RHIConvertToGpuVendorId(PhysicalDeviceInfos[Index].PhysicalDeviceProperties2.properties.vendorID) == PreferredVendor)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("Using preferred vendor device at index %d of %u..."), Index, PhysicalDeviceCount);
				return PhysicalDeviceInfos[Index].PhysicalDevice;
			}
		}
	}

	// Skip all CPU devices if they aren't permitted
	const bool bAllowCPUDevices = FParse::Param(FCommandLine::Get(), TEXT("AllowCPUDevices"));
	for (int32 Index = 0; Index < PhysicalDeviceInfos.Num(); ++Index)
	{
		if (!bAllowCPUDevices && (PhysicalDeviceInfos[Index].PhysicalDeviceProperties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU))
		{
			continue;
		}

		return PhysicalDeviceInfos[Index].PhysicalDevice;
	}

	UE_LOG(LogVulkanRHI, Warning, TEXT("None of the %u devices meet all the criteria!"), PhysicalDeviceCount);
	return VK_NULL_HANDLE;
}

static uint32 GetVulkanApiVersionForFeatureLevel(ERHIFeatureLevel::Type FeatureLevel, bool bRaytracing)
{
	const FString ProfileName = FVulkanPlatform::GetVulkanProfileNameForFeatureLevel(FeatureLevel, bRaytracing);
	const detail::VpProfileDesc* ProfileDesc = detail::vpGetProfileDesc(TCHAR_TO_ANSI(*ProfileName));
	if (ProfileDesc)
	{
		return ProfileDesc->minApiVersion;
	}

	UE_LOG(LogVulkanRHI, Log, TEXT("Using default apiVersion for platform..."));
	return UE_VK_API_VERSION;
}

// Returns the API version for the provided feautre level, returns 0 if not supported
static bool CheckVulkanProfile(ERHIFeatureLevel::Type FeatureLevel, bool bRaytracing)
{
	const FString ProfileName = FVulkanPlatform::GetVulkanProfileNameForFeatureLevel(FeatureLevel, bRaytracing);

	if (!FVulkanGenericPlatform::SupportsProfileChecks())
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("Skipping Vulkan Profile check for %s:"), *ProfileName);
		return true;
	}

	UE_LOG(LogVulkanRHI, Log, TEXT("Starting Vulkan Profile check for %s:"), *ProfileName);
	ON_SCOPE_EXIT
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("Vulkan Profile check complete."));
	};

	VpProfileProperties ProfileProperties;
	FMemory::Memzero(ProfileProperties);
	FCStringAnsi::Strcpy(ProfileProperties.profileName, VP_MAX_PROFILE_NAME_SIZE, TCHAR_TO_ANSI(*ProfileName));

	VkBool32 bInstanceSupported = VK_FALSE;
	VkResult InstanceResult = vpGetInstanceProfileSupport(nullptr, &ProfileProperties, &bInstanceSupported);  // :todo-jn: no VERIFYVULKANRESULT, this can fail and it's fine
	if ((InstanceResult == VK_SUCCESS) && bInstanceSupported)
	{
		VkInstanceCreateInfo InstanceCreateInfo;
		ZeroVulkanStruct(InstanceCreateInfo, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);

		VpInstanceCreateInfo ProfileInstanceCreateInfo;
		FMemory::Memzero(ProfileInstanceCreateInfo);
		ProfileInstanceCreateInfo.pProfile = &ProfileProperties;
		ProfileInstanceCreateInfo.pCreateInfo = &InstanceCreateInfo;

		VkInstance TempInstance = VK_NULL_HANDLE;
		VERIFYVULKANRESULT(vpCreateInstance(&ProfileInstanceCreateInfo, VULKAN_CPU_ALLOCATOR, &TempInstance));

		// Use FVulkanGenericPlatform on purpose here, we only want basic common functionality (no platform specific stuff)
		FVulkanGenericPlatform::LoadVulkanInstanceFunctions(TempInstance);

		ON_SCOPE_EXIT
		{
			// Keep nothing around from the temporary instance we created
			if (TempInstance)
			{
				VulkanRHI::vkDestroyInstance(TempInstance, VULKAN_CPU_ALLOCATOR);
				TempInstance = VK_NULL_HANDLE;
				FVulkanPlatform::ClearVulkanInstanceFunctions();
			}
		};

		// Pick the device we would use on this instance
		const VkPhysicalDevice PhysicalDevice = SelectPhysicalDevice(TempInstance);
		if (PhysicalDevice)
		{
			VkBool32 bDeviceSupported = VK_FALSE;
			VERIFYVULKANRESULT(vpGetPhysicalDeviceProfileSupport(TempInstance, PhysicalDevice, &ProfileProperties, &bDeviceSupported));
			if (bDeviceSupported)
			{
				return true;
			}
		}
	}

	return false;
}

void FVulkanDynamicRHIModule::StartupModule()
{
#if VULKAN_USE_LLM
	LLM(VulkanLLM::Initialize());
#endif
}

bool FVulkanDynamicRHIModule::IsSupported()
{
	if (FVulkanPlatform::IsSupported())
	{
		return FVulkanPlatform::LoadVulkanLibrary();
	}
	return false;
}

bool FVulkanDynamicRHIModule::IsSupported(ERHIFeatureLevel::Type FeatureLevel)
{
	if (IsSupported())
	{
		if (FeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			return !GIsEditor;
		}
		else if (!FVulkanPlatform::SupportsProfileChecks())
		{
			return true;
		}
		else
		{
			return CheckVulkanProfile(FeatureLevel, false);
		}
	}

	return false;
}

FDynamicRHI* FVulkanDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type InRequestedFeatureLevel)
{
	GMaxRHIFeatureLevel = FVulkanPlatform::GetFeatureLevel(InRequestedFeatureLevel);
	checkf(GMaxRHIFeatureLevel != ERHIFeatureLevel::Num, TEXT("Invalid feature level requested!"));

	EShaderPlatform ShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num];
	FVulkanPlatform::SetupFeatureLevels(ShaderPlatformForFeatureLevel);
	GMaxRHIShaderPlatform = ShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];
	checkf(GMaxRHIShaderPlatform != SP_NumPlatforms, TEXT("Requested feature level [%s] mapped to unsupported shader platform!"), *LexToString(InRequestedFeatureLevel));

	GVulkanRHI = new FVulkanDynamicRHI();
	FDynamicRHI* FinalRHI = GVulkanRHI;

#if ENABLE_RHI_VALIDATION
	if (FParse::Param(FCommandLine::Get(), TEXT("RHIValidation")))
	{
		FinalRHI = new FValidationRHI(FinalRHI);
	}
#endif

	for (int32 Index = 0; Index < ERHIFeatureLevel::Num; ++Index)
	{
		if (ShaderPlatformForFeatureLevel[Index] != SP_NumPlatforms)
		{
			check(GMaxTextureSamplers >= (int32)FDataDrivenShaderPlatformInfo::GetMaxSamplers(ShaderPlatformForFeatureLevel[Index]));
			if (GMaxTextureSamplers < (int32)FDataDrivenShaderPlatformInfo::GetMaxSamplers(ShaderPlatformForFeatureLevel[Index]))
			{
				UE_LOG(LogVulkanRHI, Error, TEXT("Shader platform requires at least: %d samplers, device supports: %d."), FDataDrivenShaderPlatformInfo::GetMaxSamplers(ShaderPlatformForFeatureLevel[Index]), GMaxTextureSamplers);
			}
		}
	}

	return FinalRHI;
}

IMPLEMENT_MODULE(FVulkanDynamicRHIModule, VulkanRHI);


FVulkanCommandListContext::FVulkanCommandListContext(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, FVulkanQueue* InQueue, FVulkanCommandListContext* InImmediate)
	: RHI(InRHI)
	, Immediate(InImmediate)
	, Device(InDevice)
	, Queue(InQueue)
	, bSubmitAtNextSafePoint(false)
	, UniformBufferUploader(nullptr)
	, TempFrameAllocationBuffer(InDevice)
	, CommandBufferManager(nullptr)
	, PendingGfxState(nullptr)
	, PendingComputeState(nullptr)
	, FrameCounter(0)
	, GpuProfiler(this, InDevice)
{
	FrameTiming = new FVulkanGPUTiming(this, InDevice);

	// Create CommandBufferManager, contain all active buffers
	CommandBufferManager = new FVulkanCommandBufferManager(InDevice, this);
	CommandBufferManager->Init(this);
	FrameTiming->Initialize();
	if (IsImmediate())
	{
		// Insert the Begin frame timestamp query. On EndDrawingViewport() we'll insert the End and immediately after a new Begin()
		WriteBeginTimestamp(CommandBufferManager->GetActiveCmdBuffer());

		// Flush the cmd buffer immediately to ensure a valid
		// 'Last submitted' cmd buffer exists at frame 0.
		CommandBufferManager->SubmitActiveCmdBuffer();
		CommandBufferManager->PrepareForNewActiveCommandBuffer();
	}

	// Create Pending state, contains pipeline states such as current shader and etc..
	PendingGfxState = new FVulkanPendingGfxState(Device, *this);
	PendingComputeState = new FVulkanPendingComputeState(Device, *this);

	UniformBufferUploader = new FVulkanUniformBufferUploader(Device);

	GlobalUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
}

void FVulkanCommandListContext::ReleasePendingState()
{
	delete PendingGfxState;
	PendingGfxState = nullptr;
	
	delete PendingComputeState;
	PendingComputeState = nullptr;
}

FVulkanCommandListContext::~FVulkanCommandListContext()
{
	if (FVulkanPlatform::SupportsTimestampRenderQueries())
	{
		FrameTiming->Release();
		delete FrameTiming;
		FrameTiming = nullptr;
	}

	check(CommandBufferManager != nullptr);
	delete CommandBufferManager;
	CommandBufferManager = nullptr;

	delete UniformBufferUploader;
	delete PendingGfxState;
	delete PendingComputeState;

	TempFrameAllocationBuffer.Destroy();
}


FVulkanCommandListContextImmediate::FVulkanCommandListContextImmediate(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, FVulkanQueue* InQueue)
	: FVulkanCommandListContext(InRHI, InDevice, InQueue, nullptr)
{
}


FVulkanDynamicRHI::FVulkanDynamicRHI()
	: Instance(VK_NULL_HANDLE)
	, Device(nullptr)
	, DrawingViewport(nullptr)
{
	// This should be called once at the start 
	check(IsInGameThread());
	check(!GIsThreadedRendering);

	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GRHISupportsMultithreading = true;
	GRHISupportsMultithreadedResources = true;
	GRHISupportsPipelineFileCache = true;
	GRHIVariableRateShadingEnabled &= (GVulkanAllowVariableRateShading != 0); // before extensions setup
	GRHITransitionPrivateData_SizeInBytes = sizeof(FVulkanPipelineBarrier);
	GRHITransitionPrivateData_AlignInBytes = alignof(FVulkanPipelineBarrier);
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);

	GRHIGlobals.SupportsBarycentricsSemantic = true;

	static const auto CVarPSOPrecaching = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PSOPrecaching"));

	GRHISupportsPSOPrecaching = FVulkanChunkedPipelineCacheManager::IsEnabled() && (CVarPSOPrecaching && CVarPSOPrecaching->GetInt() != 0) && CVarAllowVulkanPSOPrecache.GetValueOnAnyThread();
	GRHISupportsPipelineFileCache = !GRHISupportsPSOPrecaching || CVarEnableVulkanPSOFileCacheWhenPrecachingActive.GetValueOnAnyThread();
	UE_LOG(LogVulkanRHI, Log, TEXT("Vulkan PSO Precaching = %d, PipelineFileCache = %d"), GRHISupportsPSOPrecaching, GRHISupportsPipelineFileCache);

	// Copy source requires its own image layout.
	EnumRemoveFlags(GRHIMergeableAccessMask, ERHIAccess::CopySrc);

	// Setup the validation requests ready before we load dlls
	SetupValidationRequests();

	UE_LOG(LogVulkanRHI, Display, TEXT("Built with Vulkan header version %u.%u.%u"), VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE), VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE), VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE));

	{
		IConsoleVariable* GPUCrashDebuggingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging"));
		GGPUCrashDebuggingEnabled = (GPUCrashDebuggingCVar && GPUCrashDebuggingCVar->GetInt() != 0) || FParse::Param(FCommandLine::Get(), TEXT("gpucrashdebugging"));
	}



	CreateInstance();
	SelectDevice();
}

void FVulkanDynamicRHI::Init()
{
	InitInstance();

	bIsStandaloneStereoDevice = IHeadMountedDisplayModule::IsAvailable() && IHeadMountedDisplayModule::Get().IsStandaloneStereoOnlyDevice();

	static const auto CVarStreamingTexturePoolSize = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.PoolSize"));
	int32 StreamingPoolSizeValue = CVarStreamingTexturePoolSize->GetValueOnAnyThread();
			
	if (GPoolSizeVRAMPercentage > 0)
	{
		const uint64 TotalGPUMemory = Device->GetDeviceMemoryManager().GetTotalMemory(true);

		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(TotalGPUMemory);

		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;

		UE_LOG(LogRHI, Log, TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
			GTexturePoolSize / 1024 / 1024,
			GPoolSizeVRAMPercentage,
			TotalGPUMemory / 1024 / 1024);
	}
	else if (StreamingPoolSizeValue > 0)
	{
		GTexturePoolSize = (int64)StreamingPoolSizeValue * 1024 * 1024;

		const uint64 TotalGPUMemory = Device->GetDeviceMemoryManager().GetTotalMemory(true);
		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (of %llu MB total graphics mem)"),
				GTexturePoolSize / 1024 / 1024,
				TotalGPUMemory / 1024 / 1024);
	}
}

void FVulkanDynamicRHI::PostInit()
{
#if VULKAN_RHI_RAYTRACING
	if (GRHISupportsRayTracing)
	{
		Device->InitializeRayTracing();
	}
#endif // VULKAN_RHI_RAYTRACING
}

void FVulkanDynamicRHI::Shutdown()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("savevulkanpsocacheonexit")))
	{
		SavePipelineCache();
	}

	check(IsInGameThread() && IsInRenderingThread());
	check(Device);

	Device->PrepareForDestroy();

	EmptyCachedBoundShaderStates();

	FVulkanVertexDeclaration::EmptyCache();

	if (GIsRHIInitialized)
	{
		// Reset the RHI initialized flag.
		GIsRHIInitialized = false;

		FVulkanPlatform::OverridePlatformHandlers(false);

		GRHINeedsExtraDeletionLatency = false;

		check(!GIsCriticalError);

		// Ask all initialized FRenderResources to release their RHI resources.
		FRenderResource::ReleaseRHIForAllResources();

		{
			for (auto& Pair : Device->SamplerMap)
			{
				FVulkanSamplerState* SamplerState = (FVulkanSamplerState*)Pair.Value.GetReference();
				VulkanRHI::vkDestroySampler(Device->GetInstanceHandle(), SamplerState->Sampler, VULKAN_CPU_ALLOCATOR);
			}
			Device->SamplerMap.Empty();
		}

#if VULKAN_RHI_RAYTRACING
		Device->CleanUpRayTracing();
#endif // VULKAN_RHI_RAYTRACING

		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// Flush all pending deletes before destroying the device.
		RHICmdList.FlushPendingDeletes();
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

		// And again since some might get on a pending queue
		RHICmdList.FlushPendingDeletes();
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}

	Device->Destroy();

	delete Device;
	Device = nullptr;

	// Release the early HMD interface used to query extra extensions - if any was used
	HMDVulkanExtensions = nullptr;

#if VULKAN_HAS_DEBUGGING_ENABLED
	RemoveDebugLayerCallback();
#endif

	VulkanRHI::vkDestroyInstance(Instance, VULKAN_CPU_ALLOCATOR);

	IConsoleManager::Get().UnregisterConsoleObject(SavePipelineCacheCmd);
	IConsoleManager::Get().UnregisterConsoleObject(RebuildPipelineCacheCmd);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	IConsoleManager::Get().UnregisterConsoleObject(DumpMemoryCmd);
	IConsoleManager::Get().UnregisterConsoleObject(DumpMemoryFullCmd);
	IConsoleManager::Get().UnregisterConsoleObject(DumpStagingMemoryCmd);
	IConsoleManager::Get().UnregisterConsoleObject(DumpLRUCmd);
	IConsoleManager::Get().UnregisterConsoleObject(TrimLRUCmd);
#endif

	FVulkanPlatform::FreeVulkanLibrary();

#if VULKAN_ENABLE_DUMP_LAYER
	VulkanRHI::FlushDebugWrapperLog();
#endif
}

void FVulkanDynamicRHI::CreateInstance()
{
	// Engine registration can be disabled via console var. Also disable automatically if ShaderDevelopmentMode is on.
	auto* CVarShaderDevelopmentMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderDevelopmentMode"));
	auto* CVarDisableEngineAndAppRegistration = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DisableEngineAndAppRegistration"));
	bool bDisableEngineRegistration = (CVarDisableEngineAndAppRegistration && CVarDisableEngineAndAppRegistration->GetValueOnAnyThread() != 0) ||
		(CVarShaderDevelopmentMode && CVarShaderDevelopmentMode->GetValueOnAnyThread() != 0);

	// Use the API version stored in the profile
	ApiVersion = GetVulkanApiVersionForFeatureLevel(GMaxRHIFeatureLevel, false);

#if VULKAN_RHI_RAYTRACING
	// Run a profile check to see if this device can support our raytacing requirements since it might change the required API version of the instance
	if (FVulkanPlatform::SupportsProfileChecks() && GVulkanRayTracingCVar.GetValueOnAnyThread())
	{
		static IConsoleVariable* RequireSM6CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.RequireSM6"));
		const bool bRequireSM6 = RequireSM6CVar && RequireSM6CVar->GetBool();
		const bool bRayTracingAllowedOnCurrentShaderPlatform = (bRequireSM6 == false) || (GMaxRHIShaderPlatform == SP_VULKAN_SM6 || IsVulkanMobileSM5Platform(GMaxRHIShaderPlatform));

		if (CheckVulkanProfile(GMaxRHIFeatureLevel, true) && bRayTracingAllowedOnCurrentShaderPlatform)
		{
			// Raytracing is supported, update the required API version
			ApiVersion = GetVulkanApiVersionForFeatureLevel(GMaxRHIFeatureLevel, true);
		}
		else
		{
			// Raytracing is not supported, disable it completely instead of only loading parts of it
			GVulkanRayTracingCVar->Set(0, ECVF_SetByCode);

			if (!bRayTracingAllowedOnCurrentShaderPlatform)
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("Vulkan RayTracing disabled because SM6 shader platform is required (r.RayTracing.RequireSM6=1)."));
			}
			else
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("Vulkan RayTracing disabled because of failed profile check."));
			}
		}
	}
#endif // VULKAN_RHI_RAYTRACING

	UE_LOG(LogVulkanRHI, Log, TEXT("Using API Version %u.%u."), VK_API_VERSION_MAJOR(ApiVersion), VK_API_VERSION_MINOR(ApiVersion));

	// EngineName will be of the form "UnrealEngine4.21", with the minor version ("21" in this example)
	// updated with every quarterly release
	FString EngineName = FApp::GetEpicProductIdentifier() + FEngineVersion::Current().ToString(EVersionComponent::Minor);
	FTCHARToUTF8 EngineNameConverter(*EngineName);
	FTCHARToUTF8 ProjectNameConverter(FApp::GetProjectName());

	VkApplicationInfo AppInfo;
	ZeroVulkanStruct(AppInfo, VK_STRUCTURE_TYPE_APPLICATION_INFO);
	AppInfo.pApplicationName = bDisableEngineRegistration ? nullptr : ProjectNameConverter.Get();
	AppInfo.applicationVersion = static_cast<uint32>(BuildSettings::GetCurrentChangelist()) | (BuildSettings::IsLicenseeVersion() ? 0x80000000 : 0);
	AppInfo.pEngineName = bDisableEngineRegistration ? nullptr : EngineNameConverter.Get();
	AppInfo.engineVersion = FEngineVersion::Current().GetMinor();
	AppInfo.apiVersion = ApiVersion;

	VkInstanceCreateInfo InstInfo;
	ZeroVulkanStruct(InstInfo, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
	InstInfo.pApplicationInfo = &AppInfo;

	FVulkanInstanceExtensionArray UEInstanceExtensions = FVulkanInstanceExtension::GetUESupportedInstanceExtensions(ApiVersion);
	InstanceLayers = SetupInstanceLayers(UEInstanceExtensions);
	for (TUniquePtr<FVulkanInstanceExtension>& Extension : UEInstanceExtensions)
	{
		if (Extension->InUse())
		{
			InstanceExtensions.Add(Extension->GetExtensionName());
			Extension->PreCreateInstance(InstInfo, OptionalInstanceExtensions);
		}
	}

	InstInfo.enabledExtensionCount = InstanceExtensions.Num();
	InstInfo.ppEnabledExtensionNames = InstInfo.enabledExtensionCount > 0 ? (const ANSICHAR* const*)InstanceExtensions.GetData() : nullptr;
	
	InstInfo.enabledLayerCount = InstanceLayers.Num();
	InstInfo.ppEnabledLayerNames = InstInfo.enabledLayerCount > 0 ? InstanceLayers.GetData() : nullptr;

	VkResult Result = VulkanRHI::vkCreateInstance(&InstInfo, VULKAN_CPU_ALLOCATOR, &Instance);
	
	FVulkanPlatform::NotifyFoundInstanceLayersAndExtensions(InstanceLayers, InstanceExtensions);

	if (Result == VK_ERROR_INCOMPATIBLE_DRIVER)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Cannot find a compatible Vulkan driver (ICD).\n\nPlease look at the Getting Started guide for "
			"additional information."), TEXT("Incompatible Vulkan driver found!"));
		FPlatformMisc::RequestExitWithStatus(true, 1);
		// unreachable
		return;
	}
	else if(Result == VK_ERROR_EXTENSION_NOT_PRESENT)
	{
		// Check for missing extensions 
		FString MissingExtensions;

		uint32_t PropertyCount;
		VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &PropertyCount, nullptr);

		TArray<VkExtensionProperties> Properties;
		Properties.SetNum(PropertyCount);
		VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &PropertyCount, Properties.GetData());

		for (const ANSICHAR* Extension : InstanceExtensions)
		{
			bool bExtensionFound = false;

			for (uint32_t PropertyIndex = 0; PropertyIndex < PropertyCount; PropertyIndex++)
			{
				const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

				if (!FCStringAnsi::Strcmp(PropertyExtensionName, Extension))
				{
					bExtensionFound = true;
					break;
				}
			}

			if (!bExtensionFound)
			{
				FString ExtensionStr = ANSI_TO_TCHAR(Extension);
				UE_LOG(LogVulkanRHI, Error, TEXT("Missing required Vulkan extension: %s"), *ExtensionStr);
				MissingExtensions += ExtensionStr + TEXT("\n");
			}
		}

		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT(
			"Vulkan driver doesn't contain specified extensions:\n%s;\n\
			make sure your layers path is set appropriately."), *MissingExtensions), TEXT("Incomplete Vulkan driver found!"));
	}
	else if (Result != VK_SUCCESS)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Vulkan failed to create instance (apiVersion=0x%x)\n\nDo you have a compatible Vulkan "
			 "driver (ICD) installed?\nPlease look at "
			 "the Getting Started guide for additional information."), TEXT("No Vulkan driver found!"));
		FPlatformMisc::RequestExitWithStatus(true, 1);
		// unreachable
		return;
	}

	VERIFYVULKANRESULT(Result);

	if (!FVulkanPlatform::LoadVulkanInstanceFunctions(Instance))
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT(
			"Failed to find all required Vulkan entry points! Try updating your driver."), TEXT("No Vulkan entry points found!"));
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	SetupDebugLayerCallback();
#endif
}

void FVulkanDynamicRHI::SelectDevice()
{
	VkPhysicalDevice PhysicalDevice = SelectPhysicalDevice(Instance);
	if (PhysicalDevice == VK_NULL_HANDLE)
	{
		// Shouldn't be possible if profile checks passed prior to this
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, 
			TEXT("Vulkan failed to select physical device after passing profile checks."), TEXT("No Vulkan driver found!"));
		FPlatformMisc::RequestExitWithStatus(true, 1);
		return;
	}

	UE_LOG(LogVulkanRHI, Log, TEXT("Creating Vulkan Device using VkPhysicalDevice 0x%x."), reinterpret_cast<const void*>(PhysicalDevice));
	Device = new FVulkanDevice(this, PhysicalDevice);

	const VkPhysicalDeviceProperties& Props = Device->GetDeviceProperties();
	bool bUseVendorIdAsIs = true;
	if (Props.vendorID > 0xffff)
	{
		bUseVendorIdAsIs = false;
		VkVendorId VendorId = (VkVendorId)Props.vendorID;
		switch (VendorId)
		{
		case VK_VENDOR_ID_VIV:		GRHIVendorId = (uint32)EGpuVendorId::Vivante; break;
		case VK_VENDOR_ID_VSI:		GRHIVendorId = (uint32)EGpuVendorId::VeriSilicon; break;
		case VK_VENDOR_ID_KAZAN:	GRHIVendorId = (uint32)EGpuVendorId::Kazan; break;
		case VK_VENDOR_ID_CODEPLAY:	GRHIVendorId = (uint32)EGpuVendorId::Codeplay; break;
		case VK_VENDOR_ID_MESA:		GRHIVendorId = (uint32)EGpuVendorId::Mesa; break;
		default:
			// Unhandled case
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unhandled VkVendorId %d"), (int32)VendorId);
			bUseVendorIdAsIs = true;
			break;
		}
	}

	if (bUseVendorIdAsIs)
	{
		GRHIVendorId = Props.vendorID;
	}
	GRHIAdapterName = ANSI_TO_TCHAR(Props.deviceName);

	if (PLATFORM_ANDROID)
	{
		GRHIAdapterName.Append(TEXT(" Vulkan"));
		// On Android GL version string often contains extra information such as an actual driver version on the device.
#if PLATFORM_ANDROID
		FString GLVersion = FAndroidMisc::GetGLVersion();
#else
		FString GLVersion = "";
#endif
		GRHIAdapterInternalDriverVersion = FString::Printf(TEXT("%d.%d.%d|%s"), VK_VERSION_MAJOR(Props.apiVersion), VK_VERSION_MINOR(Props.apiVersion), VK_VERSION_PATCH(Props.apiVersion), *GLVersion);
		UE_LOG(LogVulkanRHI, Log, TEXT("API Version: %s"), *GRHIAdapterInternalDriverVersion);
	}
	else if (PLATFORM_WINDOWS)
	{
		GRHIDeviceId = Props.deviceID;
		UE_LOG(LogVulkanRHI, Log, TEXT("API Version: %d.%d.%d"), VK_VERSION_MAJOR(Props.apiVersion), VK_VERSION_MINOR(Props.apiVersion), VK_VERSION_PATCH(Props.apiVersion));
	}
	else if(PLATFORM_UNIX)
	{
		if (Device->GetVendorId() == EGpuVendorId::Nvidia)
		{
			UNvidiaDriverVersion NvidiaVersion;
			static_assert(sizeof(NvidiaVersion) == sizeof(Props.driverVersion), "Mismatched Nvidia pack driver version!");
			NvidiaVersion.Packed = Props.driverVersion;
			GRHIAdapterUserDriverVersion = FString::Printf(TEXT("%d.%02d"), NvidiaVersion.Major, NvidiaVersion.Minor);
		}
		else
		{
			GRHIAdapterUserDriverVersion = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(Props.driverVersion), VK_VERSION_MINOR(Props.driverVersion), VK_VERSION_PATCH(Props.driverVersion), Props.driverVersion);
		}

		GRHIDeviceId = Props.deviceID;
		GRHIAdapterInternalDriverVersion = GRHIAdapterUserDriverVersion;
		GRHIAdapterDriverDate = TEXT("01-01-01");  // Unused on unix systems, pick a date that will fail test if compared but passes IsValid() check
		UE_LOG(LogVulkanRHI, Log, TEXT("     API Version: %d.%d.%d"), VK_VERSION_MAJOR(Props.apiVersion), VK_VERSION_MINOR(Props.apiVersion), VK_VERSION_PATCH(Props.apiVersion));
	}

	GRHIPersistentThreadGroupCount = 1440; // TODO: Revisit based on vendor/adapter/perf query
}

void FVulkanDynamicRHI::InitInstance()
{
	check(IsInGameThread());

	if (!GIsRHIInitialized)
	{
		// Wait for the rendering thread to go idle.
		FlushRenderingCommands();

		FVulkanPlatform::OverridePlatformHandlers(true);

		GRHISupportsAsyncTextureCreation = false;
		GEnableAsyncCompute = false;

		Device->InitGPU();

#if VULKAN_HAS_DEBUGGING_ENABLED
		if (GRenderDocFound)
		{
			EnableIdealGPUCaptureOptions(true);
		}
#endif

		const VkPhysicalDeviceProperties& Props = Device->GetDeviceProperties();
		const VkPhysicalDeviceLimits& Limits = Device->GetLimits();

		// Initialize the RHI capabilities.
		GRHISupportsFirstInstance = true;
		GRHISupportsDynamicResolution = FVulkanPlatform::SupportsDynamicResolution();
		GRHISupportsFrameCyclesBubblesRemoval = true;
		GSupportsDepthBoundsTest = Device->GetPhysicalDeviceFeatures().Core_1_0.depthBounds != 0;
		GSupportsRenderTargetFormat_PF_G8 = false;	// #todo-rco
		GRHISupportsTextureStreaming = true;
		GSupportsTimestampRenderQueries = FVulkanPlatform::SupportsTimestampRenderQueries();
		GSupportsMobileMultiView = Device->GetOptionalExtensions().HasKHRMultiview ? true : false;
		GRHISupportsMSAAShaderResolve = Device->GetOptionalExtensions().HasQcomRenderPassShaderResolve ? true : false;
#if VULKAN_RHI_RAYTRACING
		GRHISupportsRayTracing = RHISupportsRayTracing(GMaxRHIShaderPlatform) && Device->GetOptionalExtensions().HasRaytracingExtensions();

		if (GRHISupportsRayTracing)
		{
			GRHISupportsRayTracingShaders = RHISupportsRayTracingShaders(GMaxRHIShaderPlatform);
			GRHISupportsInlineRayTracing = RHISupportsInlineRayTracing(GMaxRHIShaderPlatform) && Device->GetOptionalExtensions().HasRayQuery;

			GRHIRayTracingAccelerationStructureAlignment = 256; // TODO (currently handled by FVulkanAccelerationStructureBuffer)
			//Some devices have 64 for min AS offset alignment meanwhile engine AS alignment is 256. hence using round up value
			GRHIRayTracingScratchBufferAlignment = FPlatformMath::Max<uint32>(GRHIRayTracingAccelerationStructureAlignment, 
													Device->GetOptionalExtensionProperties().AccelerationStructureProps.minAccelerationStructureScratchOffsetAlignment);

			GRHIRayTracingInstanceDescriptorSize = uint32(sizeof(VkAccelerationStructureInstanceKHR));
		}
#endif
#if VULKAN_ENABLE_DUMP_LAYER
		// Disable RHI thread by default if the dump layer is enabled
		GRHISupportsRHIThread = false;
		GRHISupportsParallelRHIExecute = false;
#else
		GRHISupportsRHIThread = GRHIThreadCvar->GetInt() != 0;
		GRHISupportsParallelRHIExecute = GRHIThreadCvar->GetInt() > 1;
#endif

		// Some platforms might only have CPU for an RHI thread, but not for parallel tasks
		GSupportsParallelRenderingTasksWithSeparateRHIThread = GRHISupportsRHIThread ? FVulkanPlatform::SupportParallelRenderingTasks() : false;

		//#todo-rco: Add newer Nvidia also
		GSupportsEfficientAsyncCompute = (Device->ComputeContext != Device->ImmediateContext) && ((Device->GetVendorId() == EGpuVendorId::Amd) || FParse::Param(FCommandLine::Get(), TEXT("ForceAsyncCompute")));

		GSupportsVolumeTextureRendering = FVulkanPlatform::SupportsVolumeTextureRendering();

		// Indicate that the RHI needs to use the engine's deferred deletion queue.
		GRHINeedsExtraDeletionLatency = true;

		GMaxShadowDepthBufferSizeX =  FPlatformMath::Min<int32>(Props.limits.maxImageDimension2D, GMaxShadowDepthBufferSizeX);
		GMaxShadowDepthBufferSizeY =  FPlatformMath::Min<int32>(Props.limits.maxImageDimension2D, GMaxShadowDepthBufferSizeY);
		GMaxTextureDimensions = Props.limits.maxImageDimension2D;
		GMaxBufferDimensions = Props.limits.maxTexelBufferElements;
		GMaxComputeSharedMemory = Props.limits.maxComputeSharedMemorySize;
		GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
		GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
		GMaxCubeTextureDimensions = Props.limits.maxImageDimensionCube;
		GMaxVolumeTextureDimensions = Props.limits.maxImageDimension3D;
		GMaxWorkGroupInvocations = Props.limits.maxComputeWorkGroupInvocations;
		GMaxTextureArrayLayers = Props.limits.maxImageArrayLayers;
		GRHISupportsBaseVertexIndex = true;
		GSupportsSeparateRenderTargetBlendState = true;
		GSupportsDualSrcBlending = Device->GetPhysicalDeviceFeatures().Core_1_0.dualSrcBlend == VK_TRUE;
		GRHISupportsSeparateDepthStencilCopyAccess = Device->SupportsParallelRendering();
		GRHIBindlessSupport = Device->SupportsBindless() ? RHIGetBindlessSupport(GMaxRHIShaderPlatform) : ERHIBindlessSupport::Unsupported;
		GMaxTextureSamplers = Props.limits.maxPerStageDescriptorSamplers;
		
		GRHIMaxDispatchThreadGroupsPerDimension.X = FMath::Min<uint32>(Limits.maxComputeWorkGroupCount[0], 0x7fffffff);
		GRHIMaxDispatchThreadGroupsPerDimension.Y = FMath::Min<uint32>(Limits.maxComputeWorkGroupCount[1], 0x7fffffff);
		GRHIMaxDispatchThreadGroupsPerDimension.Z = FMath::Min<uint32>(Limits.maxComputeWorkGroupCount[2], 0x7fffffff);

		FVulkanPlatform::SetupFeatureLevels(GRHIGlobals.ShaderPlatformForFeatureLevel);

		GRHIRequiresRenderTargetForPixelShaderUAVs = true;

		GUseTexture3DBulkDataRHI = false;

		// these are supported by all devices
		GVulkanDevicePipelineStageBits = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		VkShaderStageFlags VulkanDeviceShaderStageBits = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		// optional shader stages
		if (Device->GetPhysicalDeviceFeatures().Core_1_0.geometryShader)
		{
			GVulkanDevicePipelineStageBits |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
			VulkanDeviceShaderStageBits |= VK_SHADER_STAGE_GEOMETRY_BIT;
		}

		const VkShaderStageFlags RequiredSubgroupShaderStageFlags = FVulkanPlatform::RequiredWaveOpsShaderStageFlags(VulkanDeviceShaderStageBits);
		
		// Check for wave ops support (only filled on platforms creating Vulkan 1.1 or greater instances)
		const VkSubgroupFeatureFlags RequiredSubgroupFlags =	VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT | 
																VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT | 
																VK_SUBGROUP_FEATURE_SHUFFLE_BIT;
		GRHISupportsWaveOperations = VKHasAllFlags(Device->GetDeviceSubgroupProperties().supportedStages, RequiredSubgroupShaderStageFlags) &&
			VKHasAllFlags(Device->GetDeviceSubgroupProperties().supportedOperations, RequiredSubgroupFlags);

		if (GRHISupportsWaveOperations)
		{
			// Use default size if VK_EXT_subgroup_size_control didn't fill them
			if (!GRHIMinimumWaveSize || !GRHIMaximumWaveSize)
			{
				GRHIMinimumWaveSize = GRHIMaximumWaveSize = Device->GetDeviceSubgroupProperties().subgroupSize;
			}

			UE_LOG(LogVulkanRHI, Display, TEXT("Wave Operations have been ENABLED (wave size: min=%d max=%d)."), GRHIMinimumWaveSize, GRHIMaximumWaveSize);
		}
		else
		{
			const uint32 MissingStageFlags = (Device->GetDeviceSubgroupProperties().supportedStages & RequiredSubgroupShaderStageFlags) ^ RequiredSubgroupShaderStageFlags;
			const uint32 MissingOperationFlags = (Device->GetDeviceSubgroupProperties().supportedOperations & RequiredSubgroupFlags) ^ RequiredSubgroupFlags;
			UE_LOG(LogVulkanRHI, Display, TEXT("Wave Operations have been DISABLED (missing stages=0x%x operations=0x%x)."), MissingStageFlags, MissingOperationFlags);
		}


		if (GGPUCrashDebuggingEnabled && !Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Tried to enable GPU crash debugging but no extension found! Will use local tracepoints."));
		}

		FHardwareInfo::RegisterHardwareInfo(NAME_RHI, TEXT("Vulkan"));

		SavePipelineCacheCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.SavePipelineCache"),
			TEXT("Save pipeline cache."),
			FConsoleCommandDelegate::CreateStatic(SavePipelineCache),
			ECVF_Default
			);

		RebuildPipelineCacheCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.RebuildPipelineCache"),
			TEXT("Rebuilds pipeline cache."),
			FConsoleCommandDelegate::CreateStatic(RebuildPipelineCache),
			ECVF_Default
			);

#if VULKAN_SUPPORTS_VALIDATION_CACHE
#if VULKAN_HAS_DEBUGGING_ENABLED
		if (GValidationCvar.GetValueOnAnyThread() > 0)
		{
			SaveValidationCacheCmd = IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("r.Vulkan.SaveValidationCache"),
				TEXT("Save validation cache."),
				FConsoleCommandDelegate::CreateStatic(SaveValidationCache),
				ECVF_Default
				);
		}
#endif
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		DumpMemoryCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.DumpMemory"),
			TEXT("Dumps memory map."),
			FConsoleCommandDelegate::CreateStatic(DumpMemory),
			ECVF_Default
			);
		DumpMemoryFullCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.DumpMemoryFull"),
			TEXT("Dumps full memory map."),
			FConsoleCommandDelegate::CreateStatic(DumpMemoryFull),
			ECVF_Default
		);
		DumpStagingMemoryCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.DumpStagingMemory"),
			TEXT("Dumps staging memory map."),
			FConsoleCommandDelegate::CreateStatic(DumpStagingMemory),
			ECVF_Default
		);

		DumpLRUCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.DumpPSOLRU"),
			TEXT("Dumps Vulkan PSO LRU."),
			FConsoleCommandDelegate::CreateStatic(DumpLRU),
			ECVF_Default
		);
		TrimLRUCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.Vulkan.TrimPSOLRU"),
			TEXT("Trim Vulkan PSO LRU."),
			FConsoleCommandDelegate::CreateStatic(TrimLRU),
			ECVF_Default
		);

#endif

		GRHICommandList.GetImmediateCommandList().InitializeImmediateContexts();

		FRenderResource::InitPreRHIResources();
		GIsRHIInitialized = true;
	}
}

void FVulkanCommandListContext::RHIBeginFrame()
{
	check(IsImmediate());

	extern uint32 GVulkanRHIDeletionFrameNumber;
	++GVulkanRHIDeletionFrameNumber;

	GpuProfiler.BeginFrame();

#if VULKAN_RHI_RAYTRACING
	if (GRHISupportsRayTracing)
	{
		Device->GetRayTracingCompactionRequestHandler()->Update(*this);
	}
#endif
}


void FVulkanCommandListContext::RHIBeginScene()
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIBeginScene()")));
}

void FVulkanCommandListContext::RHIEndScene()
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIEndScene()")));
}

void FVulkanCommandListContext::RHIBeginDrawingViewport(FRHIViewport* ViewportRHI, FRHITexture* RenderTargetRHI)
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIBeginDrawingViewport\n")));
	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
	RHI->DrawingViewport = Viewport;

	FRHICustomPresent* CustomPresent = Viewport->GetCustomPresent();
	if (CustomPresent)
	{
		CustomPresent->BeginDrawing();
	}
}

void FVulkanCommandListContext::RHIEndDrawingViewport(FRHIViewport* ViewportRHI, bool bPresent, bool bLockToVsync)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanMisc);
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIEndDrawingViewport()")));
	check(IsImmediate());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
	check(Viewport == RHI->DrawingViewport);

	//#todo-rco: Unbind all pending state
/*
	check(bPresent);
	RHI->Present();
*/
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(!CmdBuffer->HasEnded() && !CmdBuffer->IsInsideRenderPass());

	WriteEndTimestamp(CmdBuffer);

	bool bNativePresent = Viewport->Present(this, CmdBuffer, Queue, Device->GetPresentQueue(), bLockToVsync);
	if (bNativePresent)
	{
		//#todo-rco: Check for r.FinishCurrentFrame
	}

	RHI->DrawingViewport = nullptr;

	WriteBeginTimestamp(CommandBufferManager->GetActiveCmdBuffer());
}

void FVulkanCommandListContext::RHIEndFrame()
{
	check(IsImmediate());
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanCommandListContext::RHIEndFrame()")));
	
	ReadAndCalculateGPUFrameTime();

	GetGPUProfiler().EndFrame();

	bool bTrimMemory = false;
	GetCommandBufferManager()->FreeUnusedCmdBuffers(bTrimMemory);

	Device->GetStagingManager().ProcessPendingFree(false, true);
	Device->GetMemoryManager().ReleaseFreedPages(*this);
	Device->GetDeferredDeletionQueue().ReleaseResources();

	if (UseVulkanDescriptorCache())
	{
		Device->GetDescriptorSetCache().GC();
	}
	Device->GetDescriptorPoolsManager().GC();

	Device->ReleaseUnusedOcclusionQueryPools();

	Device->GetPipelineStateCache()->TickLRU();

	++FrameCounter;
}

void FVulkanCommandListContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if VULKAN_ENABLE_DRAW_MARKERS
	if (auto CmdBeginLabel = Device->GetCmdBeginDebugLabel())
	{
		FTCHARToUTF8 Converter(Name);
		VkDebugUtilsLabelEXT Label;
		ZeroVulkanStruct(Label, VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
		Label.pLabelName = Converter.Get();
		FLinearColor LColor(Color);
		Label.color[0] = LColor.R;
		Label.color[1] = LColor.G;
		Label.color[2] = LColor.B;
		Label.color[3] = LColor.A;
		CmdBeginLabel(GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), &Label);
	}
#endif

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GpuProfiler.bTrackingGPUCrashData)
	{
		GpuProfiler.PushMarkerForCrash(GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), Device->GetCrashMarkerBuffer(), Name);
	}
#endif

	//only valid on immediate context currently.  needs to be fixed for parallel rhi execute
	if (IsImmediate())
	{
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::DumpLayerPushMarker(Name);
#endif

		GpuProfiler.PushEvent(Name, Color);
	}
}

void FVulkanCommandListContext::RHIPopEvent()
{
#if VULKAN_ENABLE_DRAW_MARKERS
	if (auto CmdEndLabel = Device->GetCmdEndDebugLabel())
	{
		CmdEndLabel(GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle());
	}
#endif

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GpuProfiler.bTrackingGPUCrashData)
	{
		GpuProfiler.PopMarkerForCrash(GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), Device->GetCrashMarkerBuffer());
	}
#endif

	//only valid on immediate context currently.  needs to be fixed for parallel rhi execute
	if (IsImmediate())
	{
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::DumpLayerPopMarker();
#endif

		GpuProfiler.PopEvent();
	}
}

void FVulkanDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
}

bool FVulkanDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return false;
}

void FVulkanDynamicRHI::RHIFlushResources()
{
	FVulkanCommandListContextImmediate& ImmediateContext = GetDevice()->GetImmediateContext();
	bool bTrimMemory = true;
	ImmediateContext.GetCommandBufferManager()->FreeUnusedCmdBuffers(bTrimMemory);
}

void FVulkanDynamicRHI::RHIAcquireThreadOwnership()
{
}

void FVulkanDynamicRHI::RHIReleaseThreadOwnership()
{
}

// IVulkanDynamicRHI interface

uint32 FVulkanDynamicRHI::RHIGetVulkanVersion() const
{
	return ApiVersion;
}

VkInstance FVulkanDynamicRHI::RHIGetVkInstance() const
{
	return GetInstance();
}

VkDevice FVulkanDynamicRHI::RHIGetVkDevice() const
{
	return Device->GetInstanceHandle();
}

const uint8* FVulkanDynamicRHI::RHIGetVulkanDeviceUUID() const
{
	return GetDevice()->GetDeviceIdProperties().deviceUUID;
}

VkPhysicalDevice FVulkanDynamicRHI::RHIGetVkPhysicalDevice() const
{
	return Device->GetPhysicalHandle();
}

const VkAllocationCallbacks* FVulkanDynamicRHI::RHIGetVkAllocationCallbacks()
{
	return VULKAN_CPU_ALLOCATOR;
}

VkQueue FVulkanDynamicRHI::RHIGetGraphicsVkQueue() const
{
	return GetDevice()->GetGraphicsQueue()->GetHandle();
}

uint32 FVulkanDynamicRHI::RHIGetGraphicsQueueIndex() const
{
	return GetDevice()->GetGraphicsQueue()->GetQueueIndex();
}

uint32 FVulkanDynamicRHI::RHIGetGraphicsQueueFamilyIndex() const
{
	return GetDevice()->GetGraphicsQueue()->GetFamilyIndex();
}

VkCommandBuffer FVulkanDynamicRHI::RHIGetActiveVkCommandBuffer()
{
	FVulkanCommandListContextImmediate& ImmediateContext = GetDevice()->GetImmediateContext();
	VkCommandBuffer VulkanCommandBuffer = ImmediateContext.GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle();
	return VulkanCommandBuffer;
}

uint64 FVulkanDynamicRHI::RHIGetGraphicsAdapterLUID(VkPhysicalDevice InPhysicalDevice) const
{
	uint64 AdapterLUID = 0;
#if VULKAN_SUPPORTS_DRIVER_PROPERTIES

	VkPhysicalDeviceProperties2KHR GpuProps2;
	ZeroVulkanStruct(GpuProps2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);

	VkPhysicalDeviceIDPropertiesKHR GpuIdProps;
	ZeroVulkanStruct(GpuIdProps, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);

	GpuProps2.pNext = &GpuIdProps;

	VulkanRHI::vkGetPhysicalDeviceProperties2(InPhysicalDevice, &GpuProps2);
	check(GpuIdProps.deviceLUIDValid);
	AdapterLUID = reinterpret_cast<const uint64&>(GpuIdProps.deviceLUID);
#endif
	return AdapterLUID;
}

bool FVulkanDynamicRHI::RHIDoesAdapterMatchDevice(const void* InAdapterId) const
{
#if PLATFORM_WINDOWS
	const VkPhysicalDeviceIDPropertiesKHR& vkPhysicalDeviceIDProperties = GetDevice()->GetDeviceIdProperties();
	if (vkPhysicalDeviceIDProperties.deviceLUIDValid)
	{
		return FMemory::Memcmp(InAdapterId, &vkPhysicalDeviceIDProperties.deviceLUID, sizeof(LUID)) == 0;
	}
#endif

	// Not enough information.  Assume the adapter matches
	return true;
}

void* FVulkanDynamicRHI::RHIGetVkDeviceProcAddr(const char* InName) const
{
	return (void*)VulkanRHI::vkGetDeviceProcAddr(Device->GetInstanceHandle(), InName);
}

void* FVulkanDynamicRHI::RHIGetVkInstanceProcAddr(const char* InName) const
{
	return (void*)VulkanRHI::vkGetInstanceProcAddr(Instance, InName);
}

VkFormat FVulkanDynamicRHI::RHIGetSwapChainVkFormat(EPixelFormat InFormat) const
{
	// UE renders a gamma-corrected image so we need to use an sRGB format if available
	return UEToVkTextureFormat(GPixelFormats[InFormat].UnrealFormat, true);
}

bool FVulkanDynamicRHI::RHISupportsEXTFragmentDensityMap2() const
{
	return GetDevice()->GetOptionalExtensions().HasEXTFragmentDensityMap2;
}

TArray<VkExtensionProperties> FVulkanDynamicRHI::RHIGetAllInstanceExtensions() const
{
	uint32_t ExtensionCount = 0;
	VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);

	TArray<VkExtensionProperties> Extensions;
	Extensions.SetNumUninitialized(ExtensionCount);

	VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.GetData());

	return Extensions;
}

TArray<VkExtensionProperties> FVulkanDynamicRHI::RHIGetAllDeviceExtensions(VkPhysicalDevice InPhysicalDevice) const
{
	uint32_t ExtensionCount = 0;
	VulkanRHI::vkEnumerateDeviceExtensionProperties(InPhysicalDevice, nullptr, &ExtensionCount, nullptr);

	TArray<VkExtensionProperties> Extensions;
	Extensions.SetNumUninitialized(ExtensionCount);

	VulkanRHI::vkEnumerateDeviceExtensionProperties(InPhysicalDevice, nullptr, &ExtensionCount, Extensions.GetData());

	return Extensions;
}

VkImage FVulkanDynamicRHI::RHIGetVkImage(FRHITexture* InTexture) const
{
	FVulkanTexture* VulkanTexture = ResourceCast(InTexture);
	return VulkanTexture->Image;
}

VkFormat FVulkanDynamicRHI::RHIGetViewVkFormat(FRHITexture* InTexture) const
{
	FVulkanTexture* VulkanTexture = ResourceCast(InTexture);
	return VulkanTexture->ViewFormat;
}

FVulkanRHIAllocationInfo FVulkanDynamicRHI::RHIGetAllocationInfo(FRHITexture* InTexture) const
{
	FVulkanTexture* VulkanTexture = ResourceCast(InTexture);

	FVulkanRHIAllocationInfo NewInfo{};
	NewInfo.Handle = VulkanTexture->GetAllocationHandle();
	NewInfo.Offset = VulkanTexture->GetAllocationOffset();
	NewInfo.Size = VulkanTexture->GetMemorySize();

	return NewInfo;
}

FVulkanRHIImageViewInfo FVulkanDynamicRHI::RHIGetImageViewInfo(FRHITexture* InTexture) const
{
	FVulkanTexture* VulkanTexture = ResourceCast(InTexture);

	const FRHITextureDesc& Desc = InTexture->GetDesc();

	FVulkanRHIImageViewInfo Info{};
	Info.ImageView = VulkanTexture->DefaultView->GetTextureView().View;
	Info.Image = VulkanTexture->DefaultView->GetTextureView().Image;
	Info.Format = VulkanTexture->ViewFormat;
	Info.Width = Desc.Extent.X;
	Info.Height = Desc.Extent.Y;
	Info.Depth = Desc.Depth;
	Info.UEFlags = Desc.Flags;

	Info.SubresourceRange.aspectMask = VulkanTexture->GetFullAspectMask();
	Info.SubresourceRange.layerCount = VulkanTexture->GetNumberOfArrayLevels();
	Info.SubresourceRange.levelCount = Desc.NumMips;

	// TODO: do we need these?
	Info.SubresourceRange.baseMipLevel = 0;
	Info.SubresourceRange.baseArrayLayer = 0;

	return Info;
}

void FVulkanDynamicRHI::RHISetImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange)
{
	FVulkanCommandListContext& ImmediateContext = GetDevice()->GetImmediateContext();
	FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetActiveCmdBuffer();
	VulkanSetImageLayout(CmdBuffer->GetHandle(), Image, OldLayout, NewLayout, SubresourceRange);
}

void FVulkanDynamicRHI::RHISetUploadImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange)
{
	FVulkanCommandListContext& ImmediateContext = GetDevice()->GetImmediateContext();
	FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();
	VulkanSetImageLayout(CmdBuffer->GetHandle(), Image, OldLayout, NewLayout, SubresourceRange);
}

void FVulkanDynamicRHI::RHIFinishExternalComputeWork(VkCommandBuffer InCommandBuffer)
{
	FVulkanCommandListContextImmediate& ImmediateContext = GetDevice()->GetImmediateContext();
	check(InCommandBuffer == ImmediateContext.GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle());

	ImmediateContext.GetPendingComputeState()->Reset();
	ImmediateContext.GetPendingGfxState()->Reset();
}

void FVulkanDynamicRHI::RHIRegisterWork(uint32 NumPrimitives)
{
	FVulkanCommandListContextImmediate& ImmediateContext = GetDevice()->GetImmediateContext();
	if (FVulkanPlatform::RegisterGPUWork() && ImmediateContext.IsImmediate())
	{
		ImmediateContext.GetGPUProfiler().RegisterGPUWork(1);
	}
}

void FVulkanDynamicRHI::RHISubmitUploadCommandBuffer()
{
	FVulkanCommandListContext& ImmediateContext = GetDevice()->GetImmediateContext();
	ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer();
}

void FVulkanDynamicRHI::RHIVerifyResult(VkResult Result, const ANSICHAR* VkFuntion, const ANSICHAR* Filename, uint32 Line)
{
	VulkanRHI::VerifyVulkanResult(Result, VkFuntion, Filename, Line);
}

void* FVulkanDynamicRHI::RHIGetNativeDevice()
{
	return (void*)Device->GetInstanceHandle();
}

void* FVulkanDynamicRHI::RHIGetNativePhysicalDevice()
{
	return (void*)Device->GetPhysicalHandle();
}

void* FVulkanDynamicRHI::RHIGetNativeGraphicsQueue()
{
	return (void*)Device->GetGraphicsQueue()->GetHandle();
}

void* FVulkanDynamicRHI::RHIGetNativeComputeQueue()
{
	return (void*)Device->GetComputeQueue()->GetHandle();
}

void* FVulkanDynamicRHI::RHIGetNativeInstance()
{
	return (void*)GetInstance();
}

IRHICommandContext* FVulkanDynamicRHI::RHIGetDefaultContext()
{
	return &Device->GetImmediateContext();
}

IRHIComputeContext* FVulkanDynamicRHI::RHIGetDefaultAsyncComputeContext()
{
	return &Device->GetImmediateComputeContext();
}

uint64 FVulkanDynamicRHI::RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format)
{
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	return Limits.minTexelBufferOffsetAlignment;
}

void FVulkanDynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	Device->SubmitCommandsAndFlushGPU();
}

FTexture2DRHIRef FVulkanDynamicRHI::RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding)
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("VulkanTexture2DFromResource"), SizeX, SizeY, Format)
		.SetClearValue(ClearValueBinding)
		.SetFlags(Flags)
		.SetNumMips(NumMips)
		.SetNumSamples(NumSamples)
		.DetermineInititialState();

	return new FVulkanTexture(*Device, Desc, Resource, false);
}

FTexture2DArrayRHIRef FVulkanDynamicRHI::RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding)
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2DArray(TEXT("VulkanTextureArrayFromResource"), SizeX, SizeY, ArraySize, Format)
		.SetClearValue(ClearValueBinding)
		.SetFlags(Flags)
		.SetNumMips(NumMips)
		.SetNumSamples(NumSamples)
		.DetermineInititialState();

	return new FVulkanTexture(*Device, Desc, Resource, false);
}

FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Resource, ETextureCreateFlags Flags, const FClearValueBinding& ClearValueBinding)
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create(TEXT("VulkanTextureCubeFromResource"), ArraySize > 1 ? ETextureDimension::TextureCubeArray : ETextureDimension::TextureCube)
		.SetExtent(Size)
		.SetArraySize(ArraySize)
		.SetFormat(Format)
		.SetClearValue(ClearValueBinding)
		.SetFlags(Flags)
		.SetNumMips(NumMips)
		.DetermineInititialState();

	return new FVulkanTexture(*Device, Desc, Resource, false);
}

void FVulkanDynamicRHI::RHIAliasTextureResources(FTextureRHIRef& DestTextureRHI, FTextureRHIRef& SrcTextureRHI)
{
	if (DestTextureRHI && SrcTextureRHI)
	{
		FVulkanTexture* DestTexture = ResourceCast(DestTextureRHI);
		DestTexture->AliasTextureResources(SrcTextureRHI);
	}
}

FTextureRHIRef FVulkanDynamicRHI::RHICreateAliasedTexture(FTextureRHIRef& SourceTextureRHI)
{
	const FString Name = SourceTextureRHI->GetName().ToString() + TEXT("Alias");
	FRHITextureCreateDesc Desc(SourceTextureRHI->GetDesc(), ERHIAccess::SRVMask, *Name);
	return new FVulkanTexture(*Device, Desc, SourceTextureRHI);
}

FVulkanDescriptorSetsLayout::FVulkanDescriptorSetsLayout(FVulkanDevice* InDevice) :
	Device(InDevice)
{
}

FVulkanDescriptorSetsLayout::~FVulkanDescriptorSetsLayout()
{
	// Handles are owned by FVulkanPipelineStateCacheManager
	LayoutHandles.Reset(0);
}

// Increments a value and asserts on overflow.
// FSetInfo uses narrow integer types for descriptor counts,
// which may feasibly overflow one day (for example if we add bindless resources).
template <typename T> 
static void IncrementChecked(T& Value)
{
	check(Value < TNumericLimits<T>::Max());
	++Value;
}

void FVulkanDescriptorSetsLayoutInfo::AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor)
{
	// Increment type usage
	if (LayoutTypes.Contains(Descriptor.descriptorType))
	{
		LayoutTypes[Descriptor.descriptorType]++;
	}
	else
	{
		LayoutTypes.Add(Descriptor.descriptorType, 1);
	}

	if (DescriptorSetIndex >= SetLayouts.Num())
	{
		SetLayouts.SetNum(DescriptorSetIndex + 1, EAllowShrinking::No);
	}

	FSetLayout& DescSetLayout = SetLayouts[DescriptorSetIndex];

	VkDescriptorSetLayoutBinding* Binding = new(DescSetLayout.LayoutBindings) VkDescriptorSetLayoutBinding;
	*Binding = Descriptor;

	const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = RemappingInfo.SetInfos[DescriptorSetIndex];
	check(SetInfo.Types[Descriptor.binding] == Descriptor.descriptorType);
	switch (Descriptor.descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		IncrementChecked(RemappingInfo.SetInfos[DescriptorSetIndex].NumImageInfos);
		break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		IncrementChecked(RemappingInfo.SetInfos[DescriptorSetIndex].NumBufferInfos);
		break;
#if VULKAN_RHI_RAYTRACING
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		IncrementChecked(RemappingInfo.SetInfos[DescriptorSetIndex].NumAccelerationStructures);
		break;
#endif
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		break;
	default:
		checkf(0, TEXT("Unsupported descriptor type %d"), (int32)Descriptor.descriptorType);
		break;
	}
}

void FVulkanDescriptorSetsLayoutInfo::GenerateHash(const TArrayView<FRHISamplerState*>& InImmutableSamplers, VkPipelineBindPoint InBindPoint)
{
	const int32 LayoutCount = SetLayouts.Num();
	Hash = FCrc::MemCrc32(&TypesUsageID, sizeof(uint32), LayoutCount);

	for (int32 layoutIndex = 0; layoutIndex < LayoutCount; ++layoutIndex)
	{
		SetLayouts[layoutIndex].GenerateHash();
		Hash = FCrc::MemCrc32(&SetLayouts[layoutIndex].Hash, sizeof(uint32), Hash);
	}

	for (uint32 RemapingIndex = 0; RemapingIndex < ShaderStage::NumStages; ++RemapingIndex)
	{
		Hash = FCrc::MemCrc32(&RemappingInfo.StageInfos[RemapingIndex].PackedUBDescriptorSet, sizeof(uint16), Hash);
		Hash = FCrc::MemCrc32(&RemappingInfo.StageInfos[RemapingIndex].Pad0, sizeof(uint16), Hash);

		TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& Globals = RemappingInfo.StageInfos[RemapingIndex].Globals;
		Hash = FCrc::MemCrc32(Globals.GetData(), sizeof(FDescriptorSetRemappingInfo::FRemappingInfo) * Globals.Num(), Hash);

		TArray<FDescriptorSetRemappingInfo::FUBRemappingInfo>& UniformBuffers = RemappingInfo.StageInfos[RemapingIndex].UniformBuffers;
		Hash = FCrc::MemCrc32(UniformBuffers.GetData(), sizeof(FDescriptorSetRemappingInfo::FUBRemappingInfo) * UniformBuffers.Num(), Hash);

		TArray<uint16>& PackedUBBindingIndices = RemappingInfo.StageInfos[RemapingIndex].PackedUBBindingIndices;
		Hash = FCrc::MemCrc32(PackedUBBindingIndices.GetData(), sizeof(uint16) * PackedUBBindingIndices.Num(), Hash);
	}

	// It would be better to store this when the object is created, but it's not available at that time, so we'll do it here.
	BindPoint = InBindPoint;

	// Include the bind point in the hash, because we can have graphics and compute PSOs with the same descriptor info, and we don't want them to collide.
	Hash = FCrc::MemCrc32(&BindPoint, sizeof(BindPoint), Hash);
}

static FCriticalSection GTypesUsageCS;
void FVulkanDescriptorSetsLayoutInfo::CompileTypesUsageID()
{
	FScopeLock ScopeLock(&GTypesUsageCS);

	static TMap<uint32, uint32> GTypesUsageHashMap;
	static uint32 GUniqueID = 1;

	LayoutTypes.KeySort([](VkDescriptorType A, VkDescriptorType B)
	{
		return static_cast<uint32>(A) < static_cast<uint32>(B);
	});

	uint32 TypesUsageHash = 0;
	for (const auto& Elem : LayoutTypes)
	{
		TypesUsageHash = FCrc::MemCrc32(&Elem.Value, sizeof(uint32), TypesUsageHash);
	}

	uint32* UniqueID = GTypesUsageHashMap.Find(TypesUsageHash);
	if (UniqueID == nullptr)
	{
		TypesUsageID = GTypesUsageHashMap.Add(TypesUsageHash, GUniqueID++);
	}
	else
	{
		TypesUsageID = *UniqueID;
	}
}

void FVulkanDescriptorSetsLayout::Compile(FVulkanDescriptorSetLayoutMap& DSetLayoutMap)
{
	check(LayoutHandles.Num() == 0);

	// Check if we obey limits
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	
	// Check for maxDescriptorSetSamplers
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_SAMPLER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]
			<=	Limits.maxDescriptorSetSamplers);

	// Check for maxDescriptorSetUniformBuffers
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC]
			<=	Limits.maxDescriptorSetUniformBuffers);

	// Check for maxDescriptorSetUniformBuffersDynamic
	check(Device->GetVendorId() == EGpuVendorId::Amd ||
				LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC]
			<=	Limits.maxDescriptorSetUniformBuffersDynamic);

	// Check for maxDescriptorSetStorageBuffers
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC]
			<=	Limits.maxDescriptorSetStorageBuffers);

	// Check for maxDescriptorSetStorageBuffersDynamic
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC]
			<=	Limits.maxDescriptorSetStorageBuffersDynamic);

	// Check for maxDescriptorSetSampledImages
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER]
			<=	Limits.maxDescriptorSetSampledImages);

	// Check for maxDescriptorSetStorageImages
	check(		LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE]
			+	LayoutTypes[VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER]
			<=	Limits.maxDescriptorSetStorageImages);

	check(LayoutTypes[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] <= Limits.maxDescriptorSetInputAttachments);

#if VULKAN_RHI_RAYTRACING
	if (GRHISupportsRayTracing)
	{
		check(LayoutTypes[VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR] < Device->GetOptionalExtensionProperties().AccelerationStructureProps.maxDescriptorSetAccelerationStructures);
	}
#endif
	
	LayoutHandles.Empty(SetLayouts.Num());

	if (UseVulkanDescriptorCache())
	{
		LayoutHandleIds.Empty(SetLayouts.Num());
	}
				
	for (FSetLayout& Layout : SetLayouts)
	{
		VkDescriptorSetLayout* LayoutHandle = new(LayoutHandles) VkDescriptorSetLayout;

		uint32* LayoutHandleId = nullptr;
		if (UseVulkanDescriptorCache())
		{
			LayoutHandleId = new(LayoutHandleIds) uint32;
		}
			
		if (FVulkanDescriptorSetLayoutEntry* Found = DSetLayoutMap.Find(Layout))
		{
			*LayoutHandle = Found->Handle;
			if (LayoutHandleId)
			{
				*LayoutHandleId = Found->HandleId;
			}
			continue;
		}

		VkDescriptorSetLayoutCreateInfo DescriptorLayoutInfo;
		ZeroVulkanStruct(DescriptorLayoutInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
		DescriptorLayoutInfo.bindingCount = Layout.LayoutBindings.Num();
		DescriptorLayoutInfo.pBindings = Layout.LayoutBindings.GetData();

		VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorSetLayout(Device->GetInstanceHandle(), &DescriptorLayoutInfo, VULKAN_CPU_ALLOCATOR, LayoutHandle));

		if (LayoutHandleId)
		{
			*LayoutHandleId = ++GVulkanDSetLayoutHandleIdCounter;
		}

		FVulkanDescriptorSetLayoutEntry DescriptorSetLayoutEntry;
		DescriptorSetLayoutEntry.Handle = *LayoutHandle;
		DescriptorSetLayoutEntry.HandleId = LayoutHandleId ? *LayoutHandleId : 0;
				
		DSetLayoutMap.Add(Layout, DescriptorSetLayoutEntry);
	}

	if (TypesUsageID == ~0)
	{
		CompileTypesUsageID();
	}

	ZeroVulkanStruct(DescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
	DescriptorSetAllocateInfo.descriptorSetCount = LayoutHandles.Num();
	DescriptorSetAllocateInfo.pSetLayouts = LayoutHandles.GetData();
}

FVulkanRenderPass::FVulkanRenderPass(FVulkanDevice& InDevice, const FVulkanRenderTargetLayout& InRTLayout) :
	Layout(InRTLayout),
	RenderPass(VK_NULL_HANDLE),
	NumUsedClearValues(InRTLayout.GetNumUsedClearValues()),
	Device(InDevice)
{
	INC_DWORD_STAT(STAT_VulkanNumRenderPasses);
	RenderPass = CreateVulkanRenderPass(InDevice, InRTLayout);
}

FVulkanRenderPass::~FVulkanRenderPass()
{
	DEC_DWORD_STAT(STAT_VulkanNumRenderPasses);

	Device.GetDeferredDeletionQueue().EnqueueResource(FDeferredDeletionQueue2::EType::RenderPass, RenderPass);
	RenderPass = VK_NULL_HANDLE;
}

void FVulkanDynamicRHI::SavePipelineCache()
{
	FString CacheFile = GetPipelineCacheFilename();

	GVulkanRHI->Device->PipelineStateCache->Save(CacheFile);
}

void FVulkanDynamicRHI::RebuildPipelineCache()
{
	GVulkanRHI->Device->PipelineStateCache->RebuildCache();
}

#if VULKAN_SUPPORTS_VALIDATION_CACHE
void FVulkanDynamicRHI::SaveValidationCache()
{
	VkValidationCacheEXT ValidationCache = GVulkanRHI->Device->GetValidationCache();
	if (ValidationCache != VK_NULL_HANDLE)
	{
		VkDevice Device = GVulkanRHI->Device->GetInstanceHandle();
		PFN_vkGetValidationCacheDataEXT vkGetValidationCacheData = (PFN_vkGetValidationCacheDataEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkGetValidationCacheDataEXT");
		check(vkGetValidationCacheData);
		size_t CacheSize = 0;
		VkResult Result = vkGetValidationCacheData(Device, ValidationCache, &CacheSize, nullptr);
		if (Result == VK_SUCCESS)
		{
			if (CacheSize > 0)
			{
				TArray<uint8> Data;
				Data.AddUninitialized(CacheSize);
				Result = vkGetValidationCacheData(Device, ValidationCache, &CacheSize, Data.GetData());
				if (Result == VK_SUCCESS)
				{
					FString CacheFilename = GetValidationCacheFilename();
					if (FFileHelper::SaveArrayToFile(Data, *CacheFilename))
					{
						UE_LOG(LogVulkanRHI, Display, TEXT("Saved validation cache file '%s', %d bytes"), *CacheFilename, Data.Num());
					}
				}
				else
				{
					UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to query Vulkan validation cache data, VkResult=%d"), Result);
				}
			}
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to query Vulkan validation cache size, VkResult=%d"), Result);
		}
	}
}
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
void FVulkanDynamicRHI::DumpMemory()
{
	GVulkanRHI->Device->GetMemoryManager().DumpMemory(false);
}
void FVulkanDynamicRHI::DumpMemoryFull()
{
	GVulkanRHI->Device->GetMemoryManager().DumpMemory(true);
}
void FVulkanDynamicRHI::DumpStagingMemory()
{
	GVulkanRHI->Device->GetStagingManager().DumpMemory();
}
void FVulkanDynamicRHI::DumpLRU()
{
	GVulkanRHI->Device->PipelineStateCache->LRUDump();
}
void FVulkanDynamicRHI::TrimLRU()
{
	GVulkanRHI->Device->PipelineStateCache->LRUDebugEvictAll();
}
#endif

void FVulkanDynamicRHI::DestroySwapChain()
{
	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}

	TArray<FVulkanViewport*> Viewports = GVulkanRHI->Viewports;
	ENQUEUE_RENDER_COMMAND(VulkanDestroySwapChain)(
		[Viewports](FRHICommandListImmediate& RHICmdList)
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("Destroy swapchain ... "));
		
		for (FVulkanViewport* Viewport : Viewports)
		{
			Viewport->DestroySwapchain(nullptr);
		}
	});

	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}
}

void FVulkanDynamicRHI::RecreateSwapChain(void* NewNativeWindow)
{
	if (NewNativeWindow)
	{
		if (IsInGameThread())
		{
			FlushRenderingCommands();
		}

		TArray<FVulkanViewport*> Viewports = GVulkanRHI->Viewports;
		ENQUEUE_RENDER_COMMAND(VulkanRecreateSwapChain)(
			[Viewports, NewNativeWindow](FRHICommandListImmediate& RHICmdList)
		{
			UE_LOG(LogVulkanRHI, Log, TEXT("Recreate swapchain ... "));
			
			for (FVulkanViewport* Viewport : Viewports)
			{
				Viewport->RecreateSwapchain(NewNativeWindow);
			}
		});

		if (IsInGameThread())
		{
			FlushRenderingCommands();
		}
	}
}

void FVulkanDynamicRHI::VulkanSetImageLayout(VkCommandBuffer CmdBuffer, VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, const VkImageSubresourceRange& SubresourceRange)
{
	FVulkanPipelineBarrier Barrier;
	Barrier.AddImageLayoutTransition(Image, OldLayout, NewLayout, SubresourceRange);
	Barrier.Execute(CmdBuffer);
}

IRHITransientResourceAllocator* FVulkanDynamicRHI::RHICreateTransientResourceAllocator()
{
#if VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR
	// Only use transient heap on desktop platforms for now
	// Not compatible with VulkanDescriptorCache for now because it hashes using the 32bit BufferId instead of the VulkanHandle.
	if (GVulkanEnableTransientResourceAllocator && IsPCPlatform(GMaxRHIShaderPlatform) && !UseVulkanDescriptorCache())
	{
		return new FVulkanTransientResourceAllocator(Device->GetOrCreateTransientHeapCache());
	}
#endif
	return nullptr;
}

uint32 FVulkanDynamicRHI::GetPrecachePSOHashVersion()
{
	static const uint32 PrecacheHashVersion = 2;
	return PrecacheHashVersion;
}

// If you modify this function bump then GetPrecachePSOHashVersion, this will invalidate any previous uses of the hash.
// i.e. pre-existing PSO caches must be rebuilt.
uint64 FVulkanDynamicRHI::RHIComputeStatePrecachePSOHash(const FGraphicsPipelineStateInitializer& Initializer)
{
	struct FHashKey
	{
		uint32 VertexDeclaration;
		uint32 VertexShader;
		uint32 PixelShader;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		uint32 GeometryShader;
#endif // PLATFORM_SUPPORTS_GEOMETRY_SHADERS
#if PLATFORM_SUPPORTS_MESH_SHADERS
		uint32 MeshShader;
#endif // PLATFORM_SUPPORTS_MESH_SHADERS
		uint32 BlendState;
		uint32 RasterizerState;
		uint32 DepthStencilState;
		uint32 ImmutableSamplerState;

		uint32 MultiViewCount : 8;
		uint32 DrawShadingRate : 8;
		uint32 PrimitiveType : 8;
		uint32 bDepthBounds : 1;
		uint32 bHasFragmentDensityAttachment : 1;
		uint32 Unused : 6;
	} HashKey;

	FMemory::Memzero(&HashKey, sizeof(FHashKey));

	HashKey.VertexDeclaration = Initializer.BoundShaderState.VertexDeclarationRHI ? Initializer.BoundShaderState.VertexDeclarationRHI->GetPrecachePSOHash() : 0;
	HashKey.VertexShader = Initializer.BoundShaderState.GetVertexShader() ? GetTypeHash(Initializer.BoundShaderState.GetVertexShader()->GetHash()) : 0;
	HashKey.PixelShader = Initializer.BoundShaderState.GetPixelShader() ? GetTypeHash(Initializer.BoundShaderState.GetPixelShader()->GetHash()) : 0;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
	HashKey.GeometryShader = Initializer.BoundShaderState.GetGeometryShader() ? GetTypeHash(Initializer.BoundShaderState.GetGeometryShader()->GetHash()) : 0;
#endif
#if PLATFORM_SUPPORTS_MESH_SHADERS
	HashKey.MeshShader = Initializer.BoundShaderState.GetMeshShader() ? GetTypeHash(Initializer.BoundShaderState.GetMeshShader()->GetHash()) : 0;
#endif

	FBlendStateInitializerRHI BlendStateInitializerRHI;
	if (Initializer.BlendState && Initializer.BlendState->GetInitializer(BlendStateInitializerRHI))
	{
		HashKey.BlendState = GetTypeHash(BlendStateInitializerRHI);
	}
	FRasterizerStateInitializerRHI RasterizerStateInitializerRHI;
	if (Initializer.RasterizerState && Initializer.RasterizerState->GetInitializer(RasterizerStateInitializerRHI))
	{
		HashKey.RasterizerState = GetTypeHash(RasterizerStateInitializerRHI);
	}
	FDepthStencilStateInitializerRHI DepthStencilStateInitializerRHI;
	if (Initializer.DepthStencilState && Initializer.DepthStencilState->GetInitializer(DepthStencilStateInitializerRHI))
	{
		HashKey.DepthStencilState = GetTypeHash(DepthStencilStateInitializerRHI);
	}

	// Ignore immutable samplers for now
	//HashKey.ImmutableSamplerState = GetTypeHash(ImmutableSamplerState);

	HashKey.MultiViewCount = Initializer.MultiViewCount;
	HashKey.DrawShadingRate = Initializer.ShadingRate;
	HashKey.PrimitiveType = Initializer.PrimitiveType;
	HashKey.bDepthBounds = Initializer.bDepthBounds;
	HashKey.bHasFragmentDensityAttachment = Initializer.bHasFragmentDensityAttachment;

	uint64 PrecachePSOHash = CityHash64((const char*)&HashKey, sizeof(FHashKey));

	return PrecachePSOHash;
}

// If you modify this function bump then GetPrecachePSOHashVersion, this will invalidate any previous uses of the hash.
// i.e. pre-existing PSO caches must be rebuilt.
uint64 FVulkanDynamicRHI::RHIComputePrecachePSOHash(const FGraphicsPipelineStateInitializer& Initializer)
{

	// When compute precache PSO hash we assume a valid state precache PSO hash is already provided
	uint64 StatePrecachePSOHash = Initializer.StatePrecachePSOHash;
	if (StatePrecachePSOHash == 0)
	{
		StatePrecachePSOHash = RHIComputeStatePrecachePSOHash(Initializer);
	}

	// All members which are not part of the state objects
	struct FNonStateHashKey
	{
		uint64							StatePrecachePSOHash;

		EPrimitiveType					PrimitiveType;
		uint32							RenderTargetsEnabled;
		FGraphicsPipelineStateInitializer::TRenderTargetFormats	RenderTargetFormats;
		FGraphicsPipelineStateInitializer::TRenderTargetFlags RenderTargetFlags;
// AJB: temporarily disabling depth stencil properties as they do not appear to be required and it causes us to miss some permutations.
//		EPixelFormat					DepthStencilTargetFormat;
//		ETextureCreateFlags				DepthStencilTargetFlag;
		uint16							NumSamples;
		ESubpassHint					SubpassHint;
		uint8							SubpassIndex;
		EConservativeRasterization		ConservativeRasterization;
		uint8							MultiViewCount;
		EVRSShadingRate					ShadingRate;
		bool							bDepthBounds;
		bool							bHasFragmentDensityAttachment;
	} HashKey;

	FMemory::Memzero(&HashKey, sizeof(FNonStateHashKey));

	HashKey.StatePrecachePSOHash = StatePrecachePSOHash;

	HashKey.PrimitiveType = Initializer.PrimitiveType;
	HashKey.RenderTargetsEnabled = Initializer.RenderTargetsEnabled;
	HashKey.RenderTargetFormats = Initializer.RenderTargetFormats;
	HashKey.RenderTargetFlags = Initializer.RenderTargetFlags;
//	HashKey.DepthStencilTargetFormat = Initializer.DepthStencilTargetFormat;
//	HashKey.DepthStencilTargetFlag = Initializer.DepthStencilTargetFlag;
	HashKey.NumSamples = Initializer.NumSamples;
	HashKey.SubpassHint = Initializer.SubpassHint;
	HashKey.SubpassIndex = Initializer.SubpassIndex;
	HashKey.ConservativeRasterization = Initializer.ConservativeRasterization;
	HashKey.MultiViewCount = Initializer.MultiViewCount;
	HashKey.ShadingRate = Initializer.ShadingRate;
	HashKey.bDepthBounds = Initializer.bDepthBounds;
	HashKey.bHasFragmentDensityAttachment = Initializer.bHasFragmentDensityAttachment;

	// TODO: check if any RT flags actually affect PSO in VK
	for (ETextureCreateFlags& Flags : HashKey.RenderTargetFlags)
	{
		Flags = Flags & FGraphicsPipelineStateInitializer::RelevantRenderTargetFlagMask;
	}
// 	HashKey.DepthStencilTargetFlag = (HashKey.DepthStencilTargetFlag & FGraphicsPipelineStateInitializer::RelevantDepthStencilFlagMask);

	return CityHash64((const char*)&HashKey, sizeof(FNonStateHashKey));
}

bool FVulkanDynamicRHI::RHIMatchPrecachePSOInitializers(const FGraphicsPipelineStateInitializer& LHS, const FGraphicsPipelineStateInitializer& RHS)
{
	// first check non pointer objects
	if (LHS.ImmutableSamplerState != RHS.ImmutableSamplerState ||
		LHS.PrimitiveType != RHS.PrimitiveType ||
		LHS.bDepthBounds != RHS.bDepthBounds ||
		LHS.MultiViewCount != RHS.MultiViewCount ||
		LHS.ShadingRate != RHS.ShadingRate ||
		LHS.bHasFragmentDensityAttachment != RHS.bHasFragmentDensityAttachment ||
		LHS.RenderTargetsEnabled != RHS.RenderTargetsEnabled ||
		LHS.RenderTargetFormats != RHS.RenderTargetFormats ||
		!FGraphicsPipelineStateInitializer::RelevantRenderTargetFlagsEqual(LHS.RenderTargetFlags, RHS.RenderTargetFlags) ||
		LHS.DepthStencilTargetFormat != RHS.DepthStencilTargetFormat ||
		!FGraphicsPipelineStateInitializer::RelevantDepthStencilFlagsEqual(LHS.DepthStencilTargetFlag, RHS.DepthStencilTargetFlag) ||
		LHS.NumSamples != RHS.NumSamples ||
		LHS.SubpassHint != RHS.SubpassHint ||
		LHS.SubpassIndex != RHS.SubpassIndex ||
		LHS.StatePrecachePSOHash != RHS.StatePrecachePSOHash ||
		LHS.ConservativeRasterization != RHS.ConservativeRasterization)
	{
		return false;
	}

	// check the RHI shaders (pointer check for shaders should be fine)
	if (LHS.BoundShaderState.VertexShaderRHI != RHS.BoundShaderState.VertexShaderRHI ||
		LHS.BoundShaderState.PixelShaderRHI != RHS.BoundShaderState.PixelShaderRHI ||
		LHS.BoundShaderState.GetMeshShader() != RHS.BoundShaderState.GetMeshShader() ||
		LHS.BoundShaderState.GetAmplificationShader() != RHS.BoundShaderState.GetAmplificationShader() ||
		LHS.BoundShaderState.GetGeometryShader() != RHS.BoundShaderState.GetGeometryShader())
	{
		return false;
	}

	// Full compare the of the vertex declaration
	if (!MatchRHIState<FRHIVertexDeclaration, FVertexDeclarationElementList>(LHS.BoundShaderState.VertexDeclarationRHI, RHS.BoundShaderState.VertexDeclarationRHI))
	{
		return false;
	}

	// Check actual state content (each initializer can have it's own state and not going through a factory)
	if (!MatchRHIState<FRHIBlendState, FBlendStateInitializerRHI>(LHS.BlendState, RHS.BlendState)
		|| !MatchRHIState<FRHIRasterizerState, FRasterizerStateInitializerRHI>(LHS.RasterizerState, RHS.RasterizerState)
		|| !MatchRHIState<FRHIDepthStencilState, FDepthStencilStateInitializerRHI>(LHS.DepthStencilState, RHS.DepthStencilState)
		)
	{
		return false;
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
