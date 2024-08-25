// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanLinuxPlatform.h"
#include "../VulkanRHIPrivate.h"
#include "../VulkanRayTracing.h"
#include "../VulkanExtensions.h"
#include <dlfcn.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include "Linux/LinuxPlatformApplicationMisc.h"

// Vulkan function pointers
#define DEFINE_VK_ENTRYPOINTS(Type,Func) Type VulkanDynamicAPI::Func = NULL;
ENUM_VK_ENTRYPOINTS_ALL(DEFINE_VK_ENTRYPOINTS)

static bool GRenderOffScreen = false;

void* FVulkanLinuxPlatform::VulkanLib = nullptr;
bool FVulkanLinuxPlatform::bAttemptedLoad = false;

bool FVulkanLinuxPlatform::IsSupported()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")))
	{
		// If we're not running offscreen mode, make sure we have a display envvar set
		bool bHasX11Display = (getenv("DISPLAY") != nullptr);

		if (!bHasX11Display)
		{
			// check Wayland
			bool bHasWaylandSession = (getenv("WAYLAND_DISPLAY") != nullptr);

			if (!bHasWaylandSession)
			{
				UE_LOG(LogRHI, Warning, TEXT("Could not detect DISPLAY or WAYLAND_DISPLAY environment variables"));
				return false;
			}
		}
	}

	// Attempt to load the library
	return LoadVulkanLibrary();
}

bool FVulkanLinuxPlatform::LoadVulkanLibrary()
{
	if (bAttemptedLoad)
	{
		return (VulkanLib != nullptr);
	}
	bAttemptedLoad = true;

#if VULKAN_HAS_DEBUGGING_ENABLED
	const FString VulkanSDK = FPlatformMisc::GetEnvironmentVariable(TEXT("VULKAN_SDK"));
	UE_LOG(LogVulkanRHI, Display, TEXT("Found VULKAN_SDK=%s"), *VulkanSDK);
	const bool bHasVulkanSDK = !VulkanSDK.IsEmpty();

	UE_LOG(LogVulkanRHI, Display, TEXT("Registering provided Vulkan validation layers"));

	// if vulkan SDK is installed, we'll append our built-in validation layers to VK_ADD_LAYER_PATH,
	// otherwise we append to VK_LAYER_PATH (which is probably empty)

	// Change behavior of loading Vulkan layers by setting environment variable "VarToUse" to UE specific directory
	FString VarToUse = (bHasVulkanSDK)?TEXT("VK_ADD_LAYER_PATH"):TEXT("VK_LAYER_PATH");
	FString PreviousEnvVar = FPlatformMisc::GetEnvironmentVariable(*VarToUse);
	FString UELayerPath = FPaths::EngineDir();
	UELayerPath.Append(TEXT("Binaries/ThirdParty/Vulkan/Linux"));
	
	if(!PreviousEnvVar.IsEmpty())
	{
		PreviousEnvVar.Append(TEXT(":"));
	}

	PreviousEnvVar.Append(*UELayerPath);
	FPlatformMisc::SetEnvironmentVar(*VarToUse, *PreviousEnvVar);
	UE_LOG(LogVulkanRHI, Display, TEXT("Updated %s=%s"), *VarToUse, *PreviousEnvVar);

	FString PreviousLibPath = FPlatformMisc::GetEnvironmentVariable(TEXT("LD_LIBRARY_PATH"));
	if (!PreviousLibPath.IsEmpty())
	{
		PreviousLibPath.Append(TEXT(":"));
	}

	PreviousLibPath.Append(*UELayerPath);
	FPlatformMisc::SetEnvironmentVar(TEXT("LD_LIBRARY_PATH"), *PreviousLibPath);
	UE_LOG(LogVulkanRHI, Display, TEXT("Updated LD_LIBRARY_PATH=%s"), *PreviousLibPath);
#endif // VULKAN_HAS_DEBUGGING_ENABLED

	// try to load libvulkan.so
	VulkanLib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);

	if (VulkanLib == nullptr)
	{
		// be more verbose on Linux
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, 
			TEXT("Unable to load Vulkan library and/or acquire the necessary function pointers. Make sure an up-to-date libvulkan.so.1 is installed."),
			TEXT("Unable to initialize Vulkan."));

		return false;
	}

	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

	// Initialize all of the entry points we have to query manually
#define GET_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = (Type)dlsym(VulkanLib, #Func);
	ENUM_VK_ENTRYPOINTS_BASE(GET_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_BASE(CHECK_VK_ENTRYPOINTS);
	if (!bFoundAllEntryPoints)
	{
		dlclose(VulkanLib);
		VulkanLib = nullptr;
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

bool FVulkanLinuxPlatform::LoadVulkanInstanceFunctions(VkInstance inInstance)
{
	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

#define GETINSTANCE_VK_ENTRYPOINTS(Type, Func) VulkanDynamicAPI::Func = (Type)VulkanDynamicAPI::vkGetInstanceProcAddr(inInstance, #Func);
	ENUM_VK_ENTRYPOINTS_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_INSTANCE(CHECK_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(CHECK_VK_ENTRYPOINTS);

	if (!bFoundAllEntryPoints && !FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")))
	{
		return false;
	}

#if VULKAN_RHI_RAYTRACING
	const bool bFoundRayTracingEntries = FVulkanRayTracingPlatform::CheckVulkanInstanceFunctions(inInstance);
	if (!bFoundRayTracingEntries)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Vulkan RHI ray tracing is enabled, but failed to load instance functions."));
	}
#endif

	ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
#if UE_BUILD_DEBUG
	ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(CHECK_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);
#endif

	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);
#undef GET_VK_ENTRYPOINTS

	return true;
}

void FVulkanLinuxPlatform::FreeVulkanLibrary()
{
	if (VulkanLib != nullptr)
	{
#define CLEAR_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = nullptr;
		ENUM_VK_ENTRYPOINTS_ALL(CLEAR_VK_ENTRYPOINTS);

		dlclose(VulkanLib);
		VulkanLib = nullptr;
	}
	bAttemptedLoad = false;
}

namespace
{
	void EnsureSDLIsInited()
	{
		if (!FLinuxPlatformApplicationMisc::InitSDL()) //   will not initialize more than once
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Vulkan InitSDL() failed, cannot initialize SDL."), TEXT("InitSDL Failed"));
			UE_LOG(LogInit, Error, TEXT("Vulkan InitSDL() failed, cannot initialize SDL."));
		}
	}
}

void FVulkanLinuxPlatform::GetInstanceExtensions(FVulkanInstanceExtensionArray& OutExtensions)
{
	EnsureSDLIsInited();

	// We only support Xlib and Wayland, so check the video driver and hardcode each.
	// See FVulkanLinuxPlatform::IsSupported for the one other spot where support is hardcoded!
	//
	// Long-term, it'd be nice to replace dlopen with SDL_Vulkan_LoadLibrary so we can use
	// SDL_Vulkan_GetInstanceExtensions, but this requires moving vkGetDeviceProcAddr out of
	// the base entry points and allocating vkInstance to get all the non-global functions.
	//
	// Previously there was an Epic extension called SDL_Vulkan_GetRequiredInstanceExtensions,
	// but this effectively did what we're doing here (including depending on Xlib without a
	// fallback for xcb-only situations). Hardcoding is actually _better_ because the extension
	// broke the SDL_dynapi function table, making third-party SDL updates much harder to do.

	const char *SDLDriver = SDL_GetCurrentVideoDriver();
	if (SDLDriver == NULL)
	{
		// This should never happen if EnsureSDLIsInited passed!
		return;
	}

	if (strcmp(SDLDriver, "x11") == 0)
	{
		OutExtensions.Add(MakeUnique<FVulkanInstanceExtension>("VK_KHR_xlib_surface", VULKAN_EXTENSION_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED));
	}
	else if (strcmp(SDLDriver, "wayland") == 0)
	{
		OutExtensions.Add(MakeUnique<FVulkanInstanceExtension>("VK_KHR_wayland_surface", VULKAN_EXTENSION_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED));
	}
	// dummy is when we render offscreen, so ignore warning here
	else if (strcmp(SDLDriver, "dummy") != 0)
	{
		UE_LOG(LogRHI, Warning, TEXT("Could not detect SDL video driver!"));
	}
}

void FVulkanLinuxPlatform::GetDeviceExtensions(FVulkanDevice* Device, FVulkanDeviceExtensionArray& OutExtensions)
{
	// Manually activated extensions
	OutExtensions.Add(MakeUnique<FVulkanDeviceExtension>(Device, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VULKAN_EXTENSION_ENABLED, VULKAN_EXTENSION_NOT_PROMOTED, nullptr, FVulkanExtensionBase::ManuallyActivate));
}

void FVulkanLinuxPlatform::CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface)
{
	EnsureSDLIsInited();

	if (SDL_Vulkan_CreateSurface((SDL_Window*)WindowHandle, Instance, OutSurface) == SDL_FALSE)
	{
		UE_LOG(LogInit, Error, TEXT("Error initializing SDL Vulkan Surface: %hs"), SDL_GetError());
		check(0);
	}
}

void FVulkanLinuxPlatform::WriteCrashMarker(const FOptionalVulkanDeviceExtensions& OptionalExtensions, VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding)
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

	if (OptionalExtensions.HasNVDiagnosticCheckpoints)
	{
		if (bAdding)
		{
			int32 LastIndex = Entries.Num() - 1;
			uint32 Value = Entries[LastIndex];
			VulkanDynamicAPI::vkCmdSetCheckpointNV(CmdBuffer, (void*)(size_t)Value);
		}
	}
}
