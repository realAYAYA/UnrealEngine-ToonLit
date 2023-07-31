// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanWindowsPlatform.h"
#include "../VulkanRHIPrivate.h"
#include "../VulkanDevice.h"
#include "../VulkanRayTracing.h"
#include "../VulkanExtensions.h"

// Disable warning about forward declared enumeration without a type, since the D3D specific enums are not used in this translation unit
#pragma warning(push)
#pragma warning(disable : 4471)
#include "amd_ags.h"
#pragma warning(pop)

#include "Windows/AllowWindowsPlatformTypes.h"
static HMODULE GVulkanDLLModule = nullptr;

static PFN_vkGetInstanceProcAddr GGetInstanceProcAddr = nullptr;

// Vulkan function pointers
#define DEFINE_VK_ENTRYPOINTS(Type,Func) VULKANRHI_API Type VulkanDynamicAPI::Func = NULL;
ENUM_VK_ENTRYPOINTS_ALL(DEFINE_VK_ENTRYPOINTS)

#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

#pragma warning(push)
#pragma warning(disable : 4191) // warning C4191: 'type cast': unsafe conversion
bool FVulkanWindowsPlatform::LoadVulkanLibrary()
{
#if NV_AFTERMATH
	GVulkanNVAftermathModuleLoaded = false;
	const bool bAllowVendorDevice = !FParse::Param(FCommandLine::Get(), TEXT("novendordevice"));
	if (bAllowVendorDevice)
	{
		// Note - can't check device type here, we'll check for that before actually initializing Aftermath
		const FString AftermathBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/");

		FPlatformProcess::PushDllDirectory(*AftermathBinariesRoot);
		void* Handle = FPlatformProcess::GetDllHandle(TEXT("GFSDK_Aftermath_Lib.x64.dll"));
		FPlatformProcess::PopDllDirectory(*AftermathBinariesRoot);

		if (Handle == nullptr)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to load GFSDK_Aftermath_Lib.x64.dll"));
		}
		else
		{
			UE_LOG(LogVulkanRHI, Log, TEXT("Loaded GFSDK_Aftermath_Lib.x64.dll"));
			GVulkanNVAftermathModuleLoaded = true;
		}
	}
#endif

#if VULKAN_HAS_DEBUGGING_ENABLED
	if (GValidationCvar->GetInt() > 0)
	{
		const FString VulkanSDK = FPlatformMisc::GetEnvironmentVariable(TEXT("VULKAN_SDK"));
		const bool bHasVulkanSDK = !VulkanSDK.IsEmpty();
		// Only editor builds can use the redist libs currently
		//#todo-rco: Package the DLLs next to the exe; if so then change this check
		if (!bHasVulkanSDK && GIsEditor)
		{
			const FString PreviousEnvVar = FPlatformMisc::GetEnvironmentVariable(TEXT("VK_LAYER_PATH"));
			if (PreviousEnvVar.IsEmpty())
			{
				// Change behavior of loading Vulkan layers by setting environment variable "VK_LAYER_PATH" to UE specific directory
				FString VulkanLayerPath = FPaths::EngineDir();
#if PLATFORM_64BITS
				VulkanLayerPath.Append(TEXT("Binaries/ThirdParty/Windows/Vulkan/Win64"));
#else
				VulkanLayerPath.Append(TEXT("Binaries/ThirdParty/Windows/Vulkan/Win32"));
#endif
				FPlatformMisc::SetEnvironmentVar(TEXT("VK_LAYER_PATH"), *VulkanLayerPath);
			}
		}
	}
#endif // VULKAN_HAS_DEBUGGING_ENABLED

	// The vulkan dll must exist, otherwise the driver doesn't support Vulkan
	GVulkanDLLModule = ::LoadLibraryW(TEXT("vulkan-1.dll"));

	if (GVulkanDLLModule)
	{
#define GET_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = (Type)FPlatformProcess::GetDllExport(GVulkanDLLModule, L""#Func);
		ENUM_VK_ENTRYPOINTS_BASE(GET_VK_ENTRYPOINTS);

		bool bFoundAllEntryPoints = true;
		ENUM_VK_ENTRYPOINTS_BASE(CHECK_VK_ENTRYPOINTS);
		if (!bFoundAllEntryPoints)
		{
			FreeVulkanLibrary();
			return false;
		}

		ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(GET_VK_ENTRYPOINTS);
#if UE_BUILD_DEBUG
		ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(CHECK_VK_ENTRYPOINTS);
#endif

		ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(GET_VK_ENTRYPOINTS);
		ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(CHECK_VK_ENTRYPOINTS);

#undef GET_VK_ENTRYPOINTS

		return true;
	}

	return false;
}

bool FVulkanWindowsPlatform::LoadVulkanInstanceFunctions(VkInstance inInstance)
{
	if (!GVulkanDLLModule)
	{
		return false;
	}

	GGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)FPlatformProcess::GetDllExport(GVulkanDLLModule, TEXT("vkGetInstanceProcAddr"));

	if (!GGetInstanceProcAddr)
	{
		return false;
	}

	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

	// Initialize all of the entry points we have to query manually
#define GETINSTANCE_VK_ENTRYPOINTS(Type, Func) VulkanDynamicAPI::Func = (Type)VulkanDynamicAPI::vkGetInstanceProcAddr(inInstance, #Func);
	ENUM_VK_ENTRYPOINTS_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_INSTANCE(CHECK_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(CHECK_VK_ENTRYPOINTS);
	if (!bFoundAllEntryPoints)
	{
		FreeVulkanLibrary();
		return false;
	}

	ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
#if UE_BUILD_DEBUG
	ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(CHECK_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);
#endif

#if VULKAN_RHI_RAYTRACING
	const bool bFoundRayTracingEntries = FVulkanRayTracingPlatform::CheckVulkanInstanceFunctions(inInstance);
	if (!bFoundRayTracingEntries)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Vulkan RHI ray tracing is enabled, but failed to load instance functions."));
	}
#endif
	
	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);
#undef GET_VK_ENTRYPOINTS

	return true;
}
#pragma warning(pop) // restore 4191

void FVulkanWindowsPlatform::FreeVulkanLibrary()
{
	if (GVulkanDLLModule != nullptr)
	{
		::FreeLibrary(GVulkanDLLModule);
		GVulkanDLLModule = nullptr;
	}
}

#include "Windows/HideWindowsPlatformTypes.h"

void FVulkanWindowsPlatform::GetInstanceExtensions(FVulkanInstanceExtensionArray& OutExtensions)
{
	OutExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED));
	OutExtensions.Add(MakeUnique<FVulkanInstanceExtension>(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE, VULKAN_EXTENSION_NOT_PROMOTED));
}


void FVulkanWindowsPlatform::GetDeviceExtensions(FVulkanDevice* Device, FVulkanDeviceExtensionArray& OutExtensions)
{
	OutExtensions.Add(MakeUnique<FVulkanDeviceExtension>(Device, VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME, VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE, 
														VULKAN_EXTENSION_NOT_PROMOTED, DEVICE_EXT_FLAG_SETTER(HasEXTFullscreenExclusive)));

	// Manually activated extensions
	OutExtensions.Add(MakeUnique<FVulkanDeviceExtension>(Device, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, VULKAN_SUPPORTS_EXTERNAL_MEMORY, 
														VULKAN_EXTENSION_NOT_PROMOTED, nullptr, FVulkanExtensionBase::ManuallyActivate));
}

void FVulkanWindowsPlatform::CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface)
{
	VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo;
	ZeroVulkanStruct(SurfaceCreateInfo, VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);
	SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	SurfaceCreateInfo.hwnd = (HWND)WindowHandle;
	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, VULKAN_CPU_ALLOCATOR, OutSurface));
}

bool FVulkanWindowsPlatform::SupportsDeviceLocalHostVisibleWithNoPenalty(EGpuVendorId VendorId)
{
	static bool bIsWin10 = FPlatformMisc::VerifyWindowsVersion(10, 0) /*Win10*/;
	return (VendorId == EGpuVendorId::Amd && bIsWin10);
}


void FVulkanWindowsPlatform::WriteCrashMarker(const FOptionalVulkanDeviceExtensions& OptionalExtensions, VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding)
{
	ensure(Entries.Num() <= GMaxCrashBufferEntries);

	if (OptionalExtensions.HasAMDBufferMarker)
	{
		// AMD API only allows updating one entry at a time. Assume buffer has entry 0 as num entries
		VulkanDynamicAPI::vkCmdWriteBufferMarkerAMD(CmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, DestBuffer, 0, Entries.Num());
		if (bAdding)
		{
			int32 LastIndex = Entries.Num() - 1;
			// +1 size as entries start at index 1
			VulkanDynamicAPI::vkCmdWriteBufferMarkerAMD(CmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, DestBuffer, (1 + LastIndex) * sizeof(uint32), Entries[LastIndex]);
		}
	}
	else if (OptionalExtensions.HasNVDiagnosticCheckpoints)
	{
		if (bAdding)
		{
			int32 LastIndex = Entries.Num() - 1;
			uint32 Value = Entries[LastIndex];
			VulkanDynamicAPI::vkCmdSetCheckpointNV(CmdBuffer, (void*)(size_t)Value);
		}
	}
}
