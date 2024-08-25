// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanSwapChain.h"
#include "VulkanPlatform.h"
#include "Engine/RendererSettings.h"
#include "HAL/PlatformFramePacer.h"
#include "IHeadMountedDisplayModule.h"
#include "IHeadMountedDisplayVulkanExtensions.h"
#include "Misc/CommandLine.h"
#include "RHIUtilities.h"

#if PLATFORM_ANDROID
// this path crashes within libvulkan during vkDestroySwapchainKHR on some versions of Android. See FORT-250079
int32 GVulkanKeepSwapChain = 0;
#else
int32 GVulkanKeepSwapChain = 1;
#endif
static FAutoConsoleVariableRef CVarVulkanKeepSwapChain(
	TEXT("r.Vulkan.KeepSwapChain"),
	GVulkanKeepSwapChain,
	TEXT("Whether to keep old swap chain to pass through when creating the next one"),
	ECVF_RenderThreadSafe
);

int32 GVulkanSwapChainIgnoreExtraImages = 0;
static FAutoConsoleVariableRef CVarVulkanSwapChainIgnoreExtraImages(
	TEXT("r.Vulkan.SwapChainIgnoreExtraImages"),
	GVulkanSwapChainIgnoreExtraImages,
	TEXT("Whether to ignore extra images created in swapchain and stick with a requested number of images"),
	ECVF_ReadOnly
);

int32 GShouldCpuWaitForFence = 1;
static FAutoConsoleVariableRef CVarCpuWaitForFence(
	TEXT("r.Vulkan.CpuWaitForFence"),
	GShouldCpuWaitForFence,
	TEXT("Whether to have the Cpu wait for the fence in AcquireImageIndex"),
	ECVF_RenderThreadSafe
);

//disabled by default in swapchain creation if the extension frame pacer is available
int32 GVulkanCPURenderThreadFramePacer = 1;
static FAutoConsoleVariableRef CVarVulkanCPURenderThreadFramePacer(
	TEXT("r.Vulkan.CPURenderthreadFramePacer"),
	GVulkanCPURenderThreadFramePacer,
	TEXT("Whether to enable the simple Render thread CPU Framepacer for Vulkan"),
	ECVF_RenderThreadSafe
);

int32 GVulkanCPURHIFramePacer = 1;
static FAutoConsoleVariableRef CVarVulkanCPURHIFramePacer(
	TEXT("r.Vulkan.CPURHIThreadFramePacer"),
	GVulkanCPURHIFramePacer,
	TEXT("Whether to enable the simple RHI thread CPU Framepacer for Vulkan"),
	ECVF_RenderThreadSafe
);

int32 GVulkanForcePacingWithoutVSync = 0;
static FAutoConsoleVariableRef CVarVulkanForcePacingWithoutVSync(
	TEXT("r.Vulkan.ForcePacingWithoutVSync"),
	GVulkanForcePacingWithoutVSync,
	TEXT("Whether to CPU pacers remain enabled even if VSync is off"),
	ECVF_RenderThreadSafe
);

int32 GPrintVulkanVsyncDebug = 0;
#if !(UE_BUILD_SHIPPING)
static FAutoConsoleVariableRef CVarVulkanDebugVsync(
	TEXT("r.Vulkan.DebugVsync"),
	GPrintVulkanVsyncDebug,
	TEXT("Whether to print vulkan vsync data"),
	ECVF_RenderThreadSafe
);
#endif

#if !UE_BUILD_SHIPPING

bool GSimulateLostSurfaceInNextTick = false;
bool GSimulateSuboptimalSurfaceInNextTick = false;

// A self registering exec helper to check for the VULKAN_* commands.
class FVulkanCommandsHelper : public FSelfRegisteringExec
{
	virtual bool Exec_Dev(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		if (FParse::Command(&Cmd, TEXT("VULKAN_SIMULATE_LOST_SURFACE")))
		{
			GSimulateLostSurfaceInNextTick = true;
			Ar.Log(FString::Printf(TEXT("Vulkan: simulating lost surface next frame")));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("VULKAN_SIMULATE_SUBOPTIMAL_SURFACE")))
		{
			GSimulateSuboptimalSurfaceInNextTick = true;
			Ar.Log(FString::Printf(TEXT("Vulkan: simulating suboptimal surface next frame")));
			return true;
		}
		else
		{
			return false;
		}
	}
};
static FVulkanCommandsHelper GVulkanCommandsHelper;

VkResult SimulateErrors(VkResult Result)
{
	if (GSimulateLostSurfaceInNextTick)
	{
		GSimulateLostSurfaceInNextTick = false;
		return VK_ERROR_SURFACE_LOST_KHR;
	}

	if (GSimulateSuboptimalSurfaceInNextTick)
	{
		GSimulateSuboptimalSurfaceInNextTick = false;
		return VK_SUBOPTIMAL_KHR;
	}

	return Result;
}

#endif

extern TAutoConsoleVariable<int32> GAllowPresentOnComputeQueue;
static TSet<EPixelFormat> GPixelFormatNotSupportedWarning;

FVulkanSwapChain::FVulkanSwapChain(VkInstance InInstance, FVulkanDevice& InDevice, void* InWindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height, bool bIsFullScreen,
	uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages, int8 InLockToVsync, FVulkanSwapChainRecreateInfo* RecreateInfo)
	: SwapChain(VK_NULL_HANDLE)
	, Device(InDevice)
	, Surface(VK_NULL_HANDLE)
	, WindowHandle(InWindowHandle)
	, CurrentImageIndex(-1)
	, SemaphoreIndex(0)
	, NumPresentCalls(0)
	, NumAcquireCalls(0)
	, Instance(InInstance)
	, LockToVsync(InLockToVsync)
{
	NextPresentTargetTime = (FPlatformTime::Seconds() - GStartTime);

	if (RecreateInfo != nullptr && RecreateInfo->SwapChain != VK_NULL_HANDLE)
	{
		check(RecreateInfo->Surface != VK_NULL_HANDLE);
		Surface = RecreateInfo->Surface;
		RecreateInfo->Surface = VK_NULL_HANDLE;
	}
	else
	{
		// let the platform create the surface
		FVulkanPlatform::CreateSurface(WindowHandle, Instance, &Surface);
	}

	uint32 NumFormats;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalHandle(), Surface, &NumFormats, nullptr));
	check(NumFormats > 0);

	TArray<VkSurfaceFormatKHR> Formats;
	Formats.AddZeroed(NumFormats);
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalHandle(), Surface, &NumFormats, Formats.GetData()));

	VkColorSpaceKHR RequestedColorSpace = Formats[0].colorSpace;

	// If multiple colorspaces are possible, then use CVarHDROutputDevice to narrow it down
	for (int32 Index = 1; Index < Formats.Num(); ++Index)
	{
		if (Formats[Index].colorSpace != RequestedColorSpace)
		{
			static const auto CVarHDROutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
			EDisplayOutputFormat OutputDevice = CVarHDROutputDevice ? (EDisplayOutputFormat)CVarHDROutputDevice->GetValueOnAnyThread() : EDisplayOutputFormat::SDR_sRGB;
			switch (OutputDevice)
			{
			case EDisplayOutputFormat::SDR_sRGB:
				RequestedColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				break;
			case EDisplayOutputFormat::SDR_Rec709:
				RequestedColorSpace = VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
				break;
			case EDisplayOutputFormat::HDR_ACES_1000nit_ST2084:
			case EDisplayOutputFormat::HDR_ACES_2000nit_ST2084:
				RequestedColorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
				break;
			default:
				UE_LOG(LogVulkanRHI, Warning, TEXT("Requested color format %d not supported in Vulkan, falling back to sRGB. Please check the value of r.HDR.Display.OutputDevice."), int(OutputDevice));
				RequestedColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				break;
			}

			break;
		}
	}

	// Find Pixel format for presentable images
	VkSurfaceFormatKHR CurrFormat;
	FMemory::Memzero(CurrFormat);
	{
		if (InOutPixelFormat == PF_Unknown)
		{
			static const auto* CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
			InOutPixelFormat = CVarDefaultBackBufferPixelFormat ? EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread())) : PF_Unknown;
		}

		if (InOutPixelFormat != PF_Unknown)
		{
			bool bFound = false;
			if (GPixelFormats[InOutPixelFormat].Supported)
			{
				VkFormat Requested = (VkFormat)GPixelFormats[InOutPixelFormat].PlatformFormat;
				for (int32 Index = 0; Index < Formats.Num(); ++Index)
				{
					if (Formats[Index].format == Requested)
					{
						CurrFormat = Formats[Index];
						bFound = true;

						// We stop the search if both the pixel format and color space are a match. However, if we can't find a matching color space, we'll still use one of the
						// formats that at least matches the pixel format.
						if (CurrFormat.colorSpace == RequestedColorSpace)
						{
							break;
						}
					}
				}

				if (!bFound)
				{
					if (!GPixelFormatNotSupportedWarning.Contains(InOutPixelFormat))
					{
						GPixelFormatNotSupportedWarning.Add(InOutPixelFormat);
						UE_LOG(LogVulkanRHI, Display, TEXT("Requested PixelFormat %d not supported by this swapchain! Falling back to supported swapchain format..."), (uint32)InOutPixelFormat);
					}
					InOutPixelFormat = PF_Unknown;
				}
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Requested PixelFormat %d not supported by this Vulkan implementation!"), (uint32)InOutPixelFormat);
				InOutPixelFormat = PF_Unknown;
			}
		}

		if (InOutPixelFormat == PF_Unknown)
		{
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				// Reverse lookup
				check(Formats[Index].format != VK_FORMAT_UNDEFINED);
				for (int32 PFIndex = 0; PFIndex < PF_MAX; ++PFIndex)
				{
					if (Formats[Index].format == GPixelFormats[PFIndex].PlatformFormat && Formats[Index].colorSpace == RequestedColorSpace)
					{
						InOutPixelFormat = (EPixelFormat)PFIndex;
						CurrFormat = Formats[Index];
						UE_LOG(LogVulkanRHI, Verbose, TEXT("No swapchain format requested, picking up VulkanFormat %s"), VK_TYPE_TO_STRING(VkFormat, CurrFormat.format));
						break;
					}
				}

				if (InOutPixelFormat != PF_Unknown)
				{
					break;
				}
			}
		}

		if (InOutPixelFormat == PF_Unknown)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Can't find a proper pixel format for the swapchain, trying to pick up the first available"));
			VkFormat PlatformFormat = UEToVkTextureFormat(InOutPixelFormat, false);
			bool bSupported = false;
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				if (Formats[Index].format == PlatformFormat && Formats[Index].colorSpace == RequestedColorSpace)
				{
					bSupported = true;
					CurrFormat = Formats[Index];
					break;
				}
			}

			check(bSupported);
		}

		if (InOutPixelFormat == PF_Unknown)
		{
			FString Msg;
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				if (Index == 0)
				{
					Msg += TEXT("(");
				}
				else
				{
					Msg += TEXT(", ");
				}
				Msg += FString::Printf(TEXT("%d/%d"), (int32)Formats[Index].format, (int32)Formats[Index].colorSpace);
			}
			if (Formats.Num())
			{
				Msg += TEXT(")");
			}
			UE_LOG(LogVulkanRHI, Fatal, TEXT("Unable to find a pixel format for the swapchain; swapchain returned %d Vulkan formats %s"), Formats.Num(), *Msg);
		}
	}

	if (CurrFormat.colorSpace != RequestedColorSpace)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Requested color format %d not supported by this Vulkan implementation, falling back to %d. Please check the value of r.HDR.Display.OutputDevice."), (int32)RequestedColorSpace, (int32)CurrFormat.colorSpace);
	}

	VkFormat PlatformFormat = UEToVkTextureFormat(InOutPixelFormat, false);

	Device.SetupPresentQueue(Surface);

	// Fetch present mode
	VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (FVulkanPlatform::SupportsQuerySurfaceProperties())
	{
		// Only dump the present modes the very first time they are queried
		static bool bFirstTimeLog = !!VULKAN_HAS_DEBUGGING_ENABLED;

		uint32 NumFoundPresentModes = 0;
		VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfacePresentModesKHR(Device.GetPhysicalHandle(), Surface, &NumFoundPresentModes, nullptr));
		check(NumFoundPresentModes > 0);

		TArray<VkPresentModeKHR> FoundPresentModes;
		FoundPresentModes.AddZeroed(NumFoundPresentModes);
		VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfacePresentModesKHR(Device.GetPhysicalHandle(), Surface, &NumFoundPresentModes, FoundPresentModes.GetData()));

		UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("Found %d Surface present modes:"), NumFoundPresentModes);

		bool bFoundPresentModeMailbox = false;
		bool bFoundPresentModeImmediate = false;
		bool bFoundPresentModeFIFO = false;

		for (size_t i = 0; i < NumFoundPresentModes; i++)
		{
			switch (FoundPresentModes[i])
			{
			case VK_PRESENT_MODE_MAILBOX_KHR:
				bFoundPresentModeMailbox = true;
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_MAILBOX_KHR (%d)"), (int32)VK_PRESENT_MODE_MAILBOX_KHR);
				break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				bFoundPresentModeImmediate = true;
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_IMMEDIATE_KHR (%d)"), (int32)VK_PRESENT_MODE_IMMEDIATE_KHR);
				break;
			case VK_PRESENT_MODE_FIFO_KHR:
				bFoundPresentModeFIFO = true;
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_FIFO_KHR (%d)"), (int32)VK_PRESENT_MODE_FIFO_KHR);
				break;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_FIFO_RELAXED_KHR (%d)"), (int32)VK_PRESENT_MODE_FIFO_RELAXED_KHR);
				break;
			default:
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VkPresentModeKHR %d"), (int32)FoundPresentModes[i]);
				break;
			}
		}

		int32 RequestedPresentMode = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("vulkanpresentmode="), RequestedPresentMode))
		{
			bool bRequestSuccessful = false;
			switch (RequestedPresentMode)
			{
			case VK_PRESENT_MODE_MAILBOX_KHR:
				if (bFoundPresentModeMailbox)
				{
					PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					bRequestSuccessful = true;
				}
				break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				if (bFoundPresentModeImmediate)
				{
					PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
					bRequestSuccessful = true;
				}
				break;
			case VK_PRESENT_MODE_FIFO_KHR:
				if (bFoundPresentModeFIFO)
				{
					PresentMode = VK_PRESENT_MODE_FIFO_KHR;
					bRequestSuccessful = true;
				}
				break;
			default:
				break;
			}

			if (!bRequestSuccessful)
			{
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Warning, TEXT("Requested PresentMode (%d) is not handled or available, ignoring..."), RequestedPresentMode);
				RequestedPresentMode = -1;
			}
		}

		if (RequestedPresentMode == -1)
		{
			// Until FVulkanViewport::Present honors SyncInterval, we need to disable vsync for the spectator window if using an HMD.
			const bool bDisableVsyncForHMD = (FVulkanDynamicRHI::HMDVulkanExtensions.IsValid()) ? FVulkanDynamicRHI::HMDVulkanExtensions->ShouldDisableVulkanVSync() : false;

			if (bFoundPresentModeImmediate && (bDisableVsyncForHMD || !LockToVsync))
			{
				PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
			else if (bFoundPresentModeMailbox)
			{
				PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
			else if (bFoundPresentModeFIFO)
			{
				PresentMode = VK_PRESENT_MODE_FIFO_KHR;
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Couldn't find desired PresentMode! Using %s"), VK_TYPE_TO_STRING(VkPresentModeKHR, FoundPresentModes[0]));
				PresentMode = FoundPresentModes[0];
			}
		}

		UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("Selected VkPresentModeKHR mode %s"), VK_TYPE_TO_STRING(VkPresentModeKHR, PresentMode));
		bFirstTimeLog = false;
	}

	// Check the surface properties and formats

	VkSurfaceCapabilitiesKHR SurfProperties;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device.GetPhysicalHandle(),
		Surface,
		&SurfProperties));
	VkSurfaceTransformFlagBitsKHR PreTransform;
	if (SurfProperties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		PreTransform = SurfProperties.currentTransform;
	}

	VkCompositeAlphaFlagBitsKHR CompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	if (SurfProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
	{
		CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}

	// 0 means no limit, so use the requested number
	uint32 DesiredNumBuffers = SurfProperties.maxImageCount > 0 ? FMath::Clamp(*InOutDesiredNumBackBuffers, SurfProperties.minImageCount, SurfProperties.maxImageCount) : *InOutDesiredNumBackBuffers;

	uint32 SizeX = FVulkanPlatform::SupportsQuerySurfaceProperties() ? (SurfProperties.currentExtent.width == 0xFFFFFFFF ? Width : SurfProperties.currentExtent.width) : Width;
	uint32 SizeY = FVulkanPlatform::SupportsQuerySurfaceProperties() ? (SurfProperties.currentExtent.height == 0xFFFFFFFF ? Height : SurfProperties.currentExtent.height) : Height;

	VkSwapchainCreateInfoKHR SwapChainInfo;
	ZeroVulkanStruct(SwapChainInfo, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
	SwapChainInfo.surface = Surface;
	SwapChainInfo.minImageCount = DesiredNumBuffers;
	SwapChainInfo.imageFormat = CurrFormat.format;
	SwapChainInfo.imageColorSpace = CurrFormat.colorSpace;
	SwapChainInfo.imageExtent.width = SizeX;
	SwapChainInfo.imageExtent.height = SizeY;
	SwapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	SwapChainInfo.preTransform = PreTransform;
	SwapChainInfo.imageArrayLayers = 1;
	SwapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	SwapChainInfo.presentMode = PresentMode;
	SwapChainInfo.oldSwapchain = VK_NULL_HANDLE;
	if(RecreateInfo != nullptr)
	{
		SwapChainInfo.oldSwapchain = RecreateInfo->SwapChain;
	}
	
	SwapChainInfo.clipped = VK_TRUE;
	SwapChainInfo.compositeAlpha = CompositeAlpha;
	
	{
		//#todo-rco: Crappy workaround
		if (SwapChainInfo.imageExtent.width == 0)
		{
			SwapChainInfo.imageExtent.width = Width;
		}
		if (SwapChainInfo.imageExtent.height == 0)
		{
			SwapChainInfo.imageExtent.height = Height;
		}
	}

	if (Device.GetOptionalExtensions().HasQcomRenderPassTransform)
	{
		QCOMRenderPassTransform = SurfProperties.currentTransform;
		SwapChainInfo.preTransform = QCOMRenderPassTransform;
		ImageFormat = SwapChainInfo.imageFormat;
		if (SwapChainInfo.preTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
			SwapChainInfo.preTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
		{
			Swap(SwapChainInfo.imageExtent.width, SwapChainInfo.imageExtent.height);
		}
	}

	VkBool32 bSupportsPresent;
	VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfaceSupportKHR(Device.GetPhysicalHandle(), Device.GetPresentQueue()->GetFamilyIndex(), Surface, &bSupportsPresent));
	ensure(bSupportsPresent);

	//ensure(SwapChainInfo.imageExtent.width >= SurfProperties.minImageExtent.width && SwapChainInfo.imageExtent.width <= SurfProperties.maxImageExtent.width);
	//ensure(SwapChainInfo.imageExtent.height >= SurfProperties.minImageExtent.height && SwapChainInfo.imageExtent.height <= SurfProperties.maxImageExtent.height);
	static bool bPrintSwapchainCreationInfo = true;
	if (bPrintSwapchainCreationInfo)
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("Creating new VK swapchain with %s, %s, %s, num images %d"), 
			VK_TYPE_TO_STRING(VkPresentModeKHR, SwapChainInfo.presentMode), VK_TYPE_TO_STRING(VkFormat, SwapChainInfo.imageFormat), 
			VK_TYPE_TO_STRING(VkColorSpaceKHR, SwapChainInfo.imageColorSpace), static_cast<uint32>(SwapChainInfo.minImageCount));
#if WITH_EDITOR
		bPrintSwapchainCreationInfo = false;
#endif
	}

#if VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
	VkSurfaceFullScreenExclusiveInfoEXT FullScreenInfo;
	ZeroVulkanStruct(FullScreenInfo, VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT);
	if (Device.GetOptionalExtensions().HasEXTFullscreenExclusive)
	{
		FullScreenInfo.fullScreenExclusive = bIsFullScreen ? VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT : VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
		FullScreenInfo.pNext = (void*)SwapChainInfo.pNext;
		SwapChainInfo.pNext = &FullScreenInfo;
	}
#endif

	VkResult Result = FVulkanPlatform::CreateSwapchainKHR(WindowHandle, Device.GetPhysicalHandle(), Device.GetInstanceHandle(), &SwapChainInfo, VULKAN_CPU_ALLOCATOR, &SwapChain);
#if VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
	if (Device.GetOptionalExtensions().HasEXTFullscreenExclusive && Result == VK_ERROR_INITIALIZATION_FAILED)
	{
		// Unlink fullscreen
		UE_LOG(LogVulkanRHI, Warning, TEXT("Create swapchain failed with Initialization error; removing FullScreen extension..."));
		SwapChainInfo.pNext = FullScreenInfo.pNext;
		Result = FVulkanPlatform::CreateSwapchainKHR(WindowHandle, Device.GetPhysicalHandle(), Device.GetInstanceHandle(), &SwapChainInfo, VULKAN_CPU_ALLOCATOR, &SwapChain);
	}
#endif
	VERIFYVULKANRESULT_EXPANDED(Result);

	if (RecreateInfo != nullptr)
	{
		if (RecreateInfo->SwapChain != VK_NULL_HANDLE)
		{
			FVulkanPlatform::DestroySwapchainKHR(Device.GetInstanceHandle(), RecreateInfo->SwapChain, VULKAN_CPU_ALLOCATOR);
			RecreateInfo->SwapChain = VK_NULL_HANDLE;
		}
		if (RecreateInfo->Surface != VK_NULL_HANDLE)
		{
			VulkanRHI::vkDestroySurfaceKHR(Instance, RecreateInfo->Surface, VULKAN_CPU_ALLOCATOR);
			RecreateInfo->Surface = VK_NULL_HANDLE;
		}
	}

	InternalWidth = FMath::Min(Width, SwapChainInfo.imageExtent.width);
	InternalHeight = FMath::Min(Height, SwapChainInfo.imageExtent.height);
	bInternalFullScreen = bIsFullScreen;

	uint32 NumSwapChainImages;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetSwapchainImagesKHR(Device.GetInstanceHandle(), SwapChain, &NumSwapChainImages, nullptr));

	if (GVulkanSwapChainIgnoreExtraImages != 0)
	{
		NumSwapChainImages = DesiredNumBuffers;
	}

	OutImages.AddUninitialized(NumSwapChainImages);
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetSwapchainImagesKHR(Device.GetInstanceHandle(), SwapChain, &NumSwapChainImages, OutImages.GetData()));

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	ImageAcquiredFences.AddUninitialized(NumSwapChainImages);
	VulkanRHI::FFenceManager& FenceMgr = Device.GetFenceManager();
	for (uint32 BufferIndex = 0; BufferIndex < NumSwapChainImages; ++BufferIndex)
	{
		ImageAcquiredFences[BufferIndex] = Device.GetFenceManager().AllocateFence(true);
	}
#endif
	ImageAcquiredSemaphore.AddUninitialized(NumSwapChainImages);
	for (uint32 BufferIndex = 0; BufferIndex < NumSwapChainImages; ++BufferIndex)
	{
		ImageAcquiredSemaphore[BufferIndex] = new VulkanRHI::FSemaphore(Device);
		ImageAcquiredSemaphore[BufferIndex]->AddRef();
	}

	*InOutDesiredNumBackBuffers = NumSwapChainImages;

	PresentID = 0;
}

void FVulkanSwapChain::Destroy(FVulkanSwapChainRecreateInfo* RecreateInfo)
{
	// We could be responding to an OUT_OF_DATE event and the GPU might not be done with swapchain image, so wait for idle.
	// Alternatively could also check on the fence(s) for the image(s) from the swapchain but then timing out/waiting could become an issue.
	Device.WaitUntilIdle();

	bool bRecreate = RecreateInfo && GVulkanKeepSwapChain;
	if (bRecreate)
	{
		RecreateInfo->SwapChain = SwapChain;
		RecreateInfo->Surface = Surface;
	}
	else
	{
		FVulkanPlatform::DestroySwapchainKHR(Device.GetInstanceHandle(), SwapChain, VULKAN_CPU_ALLOCATOR);
	}
	SwapChain = VK_NULL_HANDLE;

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	VulkanRHI::FFenceManager& FenceMgr = Device.GetFenceManager();
	for (int32 Index = 0; Index < ImageAcquiredFences.Num(); ++Index)
	{
		FenceMgr.ReleaseFence(ImageAcquiredFences[Index]);
	}
#endif

	//#todo-rco: Enqueue for deletion as we first need to destroy the cmd buffers and queues otherwise validation fails
	for (int BufferIndex = 0; BufferIndex < ImageAcquiredSemaphore.Num(); ++BufferIndex)
	{
		ImageAcquiredSemaphore[BufferIndex]->Release();
	}

	if(!bRecreate)
	{
		VulkanRHI::vkDestroySurfaceKHR(Instance, Surface, VULKAN_CPU_ALLOCATOR);
	}

	if (QCOMDepthView && QCOMDepthView != QCOMDepthStencilView)
	{
		delete QCOMDepthView;
		QCOMDepthView = nullptr;
	}

	if (QCOMDepthStencilView)
	{
		delete QCOMDepthStencilView;
		QCOMDepthStencilView = nullptr;
		QCOMDepthView = nullptr;
	}

	Surface = VK_NULL_HANDLE;
}

int32 FVulkanSwapChain::AcquireImageIndex(VulkanRHI::FSemaphore** OutSemaphore)
{
	check(CurrentImageIndex == -1);

	// Get the index of the next swapchain image we should render to.
	// We'll wait with an "infinite" timeout, the function will block until an image is ready.
	// The ImageAcquiredSemaphore[ImageAcquiredSemaphoreIndex] will get signaled when the image is ready (upon function return).
	uint32 ImageIndex = 0;
	const int32 PrevSemaphoreIndex = SemaphoreIndex;
	SemaphoreIndex = (SemaphoreIndex + 1) % ImageAcquiredSemaphore.Num();

	// If we have not called present for any of the swapchain images, it will cause a crash/hang
	checkf(!(NumAcquireCalls == ImageAcquiredSemaphore.Num() - 1 && NumPresentCalls == 0), TEXT("vkAcquireNextImageKHR will fail as no images have been presented before acquiring all of them"));
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	VulkanRHI::FFenceManager& FenceMgr = Device.GetFenceManager();
	FenceMgr.ResetFence(ImageAcquiredFences[SemaphoreIndex]);
	const VkFence AcquiredFence = ImageAcquiredFences[SemaphoreIndex]->GetHandle();
#else
	const VkFence AcquiredFence = VK_NULL_HANDLE;
#endif
	VkResult Result;
	{
		const uint32 MaxImageIndex = ImageAcquiredSemaphore.Num() - 1;

		SCOPE_CYCLE_COUNTER(STAT_VulkanAcquireBackBuffer);
		FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUPresent);

		Result = VulkanRHI::vkAcquireNextImageKHR(
			Device.GetInstanceHandle(),
			SwapChain,
			UINT64_MAX,
			ImageAcquiredSemaphore[SemaphoreIndex]->GetHandle(),
			AcquiredFence,
			&ImageIndex);

		// The swapchain may have more images than we have requested on creating it. Ignore all extra images
		while (ImageIndex > MaxImageIndex && (Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR))
		{
			Result = VulkanRHI::vkAcquireNextImageKHR(
				Device.GetInstanceHandle(),
				SwapChain,
				UINT64_MAX,
				ImageAcquiredSemaphore[SemaphoreIndex]->GetHandle(),
				AcquiredFence,
				&ImageIndex);
		}
	}

	if (Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		SemaphoreIndex = PrevSemaphoreIndex;
		return (int32)EStatus::OutOfDate;
	}

	if (Result == VK_ERROR_SURFACE_LOST_KHR)
	{
		SemaphoreIndex = PrevSemaphoreIndex;
		return (int32)EStatus::SurfaceLost;
	}

	++NumAcquireCalls;
	*OutSemaphore = ImageAcquiredSemaphore[SemaphoreIndex];

#if VULKAN_HAS_DEBUGGING_ENABLED
	if (Result == VK_ERROR_VALIDATION_FAILED_EXT)
	{
		extern TAutoConsoleVariable<int32> GValidationCvar;
		if (GValidationCvar.GetValueOnRenderThread() == 0)
		{
			UE_LOG(LogVulkanRHI, Fatal, TEXT("vkAcquireNextImageKHR failed with Validation error. Try running with r.Vulkan.EnableValidation=1 to get information from the driver"));
		}
	}
	else
#endif
	{
		checkf(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR, TEXT("vkAcquireNextImageKHR failed Result = %d"), int32(Result));
	}
	CurrentImageIndex = (int32)ImageIndex;

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanWaitSwapchain);
		bool bResult = FenceMgr.WaitForFence(ImageAcquiredFences[SemaphoreIndex], UINT64_MAX);
		ensure(bResult);
	}
#endif
	return CurrentImageIndex;
}

void FVulkanSwapChain::RenderThreadPacing()
{
	check(IsInRenderingThread());
	const int32 SyncInterval = (LockToVsync || GVulkanForcePacingWithoutVSync) ? RHIGetSyncInterval() : 0;

	//very naive CPU side frame pacer.
	if (GVulkanCPURenderThreadFramePacer && SyncInterval > 0)
	{
		double NowCPUTime = FPlatformTime::Seconds();
		double DeltaCPUPresentTimeMS = (NowCPUTime - RTPacingPreviousFrameCPUTime) * 1000.0;


		double TargetIntervalWithEpsilonMS = (double)SyncInterval * (1.0 / 60.0) * 1000.0;
		const double IntervalThresholdMS = TargetIntervalWithEpsilonMS * 0.1;

		RTPacingSampledDeltaTimeMS += DeltaCPUPresentTimeMS; RTPacingSampleCount++;

		double SampledDeltaMS = (RTPacingSampledDeltaTimeMS / (double)RTPacingSampleCount) + IntervalThresholdMS;

		if (RTPacingSampleCount > 1000)
		{
			RTPacingSampledDeltaTimeMS = SampledDeltaMS;
			RTPacingSampleCount = 1;
		}

		if (SampledDeltaMS < (TargetIntervalWithEpsilonMS))
		{
			FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUPresent);
			QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);

			FPlatformProcess::SleepNoStats((TargetIntervalWithEpsilonMS - SampledDeltaMS) * 0.001f);
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CPU RT delta: %f, TargetWEps: %f, sleepTime: %f "), SampledDeltaMS, TargetIntervalWithEpsilonMS, TargetIntervalWithEpsilonMS - DeltaCPUPresentTimeMS);
			}
		}
		else
		{
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CPU RT delta: %f"), DeltaCPUPresentTimeMS);
			}
		}
		RTPacingPreviousFrameCPUTime = NowCPUTime;
	}
}

FVulkanSwapChain::EStatus FVulkanSwapChain::Present(FVulkanQueue* GfxQueue, FVulkanQueue* PresentQueue, VulkanRHI::FSemaphore* BackBufferRenderingDoneSemaphore)
{
	check(CurrentImageIndex != -1);

	//ensure(GfxQueue == PresentQueue);

	VkPresentInfoKHR Info;
	ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
	VkSemaphore Semaphore = VK_NULL_HANDLE;
	if (BackBufferRenderingDoneSemaphore)
	{
		Info.waitSemaphoreCount = 1;
		Semaphore = BackBufferRenderingDoneSemaphore->GetHandle();
		Info.pWaitSemaphores = &Semaphore;
	}
	Info.swapchainCount = 1;
	Info.pSwapchains = &SwapChain;
	Info.pImageIndices = (uint32*)&CurrentImageIndex;

	bool bPlatformHandlesFramePacing = FVulkanPlatform::FramePace(Device, WindowHandle, SwapChain, PresentID, Info);

	if (!bPlatformHandlesFramePacing)
	{
		const int32 FramePace = (LockToVsync || GVulkanForcePacingWithoutVSync) ? FPlatformRHIFramePacer::GetFramePace() : 0;

		//very naive CPU side frame pacer.
		if (GVulkanCPURHIFramePacer && FramePace > 0)
		{
			const double NowCPUTime = (FPlatformTime::Seconds() - GStartTime);

			const double TimeToSleep = (NextPresentTargetTime - NowCPUTime);
			const double TargetIntervalWithEpsilon = 1.0 / (double)FramePace;

			if (TimeToSleep > 0.0)
			{
				FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUPresent);

				QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);
				FPlatformProcess::SleepNoStats(static_cast<float>(TimeToSleep));
				if (GPrintVulkanVsyncDebug)
				{
					UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, CPU TimeToSleep: %f, TargetWEps: %f"), PresentID, TimeToSleep * 1000.0, TargetIntervalWithEpsilon * 1000.0);
				}
			}
			else
			{
				if (GPrintVulkanVsyncDebug)
				{
					UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, CPU TimeToSleep: %f"), PresentID, TimeToSleep * 1000.0);
				}
			}
			NextPresentTargetTime = FMath::Max(NextPresentTargetTime + TargetIntervalWithEpsilon, NowCPUTime);
		}
	}
	PresentID++;

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanQueuePresent);

		VkResult PresentResult;
		{
			FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUPresent);
			PresentResult = FVulkanPlatform::Present(PresentQueue->GetHandle(), Info);
		}

		CurrentImageIndex = -1;

#if !UE_BUILD_SHIPPING
		PresentResult = SimulateErrors(PresentResult);
#endif

		if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return EStatus::OutOfDate;
		}

		if (PresentResult == VK_ERROR_SURFACE_LOST_KHR)
		{
			return EStatus::SurfaceLost;
		}

		if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR)
		{
			VERIFYVULKANRESULT(PresentResult);
		}
	}

	++NumPresentCalls;

	return EStatus::Healthy;
}

void FVulkanSwapChain::CreateQCOMDepthStencil(const FVulkanTexture& InSurface) const
{
	check(!QCOMDepthStencilSurface);
	check(!QCOMDepthStencilView);
	check(!QCOMDepthView);

	const FRHITextureDesc& Desc = InSurface.GetDesc();
	const ETextureCreateFlags UEFlags = Desc.Flags;
	check(UEFlags & TexCreate_DepthStencilTargetable);
	const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

	const FRHITextureCreateDesc CreateDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FVulkanSwapChainQCOM"), Desc.Extent.Y, Desc.Extent.X, Desc.Format) // Desc.Extent.X and Desc.Extent.Y are intentionally swapped.
		.SetClearValue(FClearValueBinding::None)
		.SetFlags(UEFlags)
		.SetNumMips(Desc.NumMips)
		.SetNumSamples(Desc.NumSamples)
		.DetermineInititialState();

	QCOMDepthStencilSurface = new FVulkanTexture(Device, CreateDesc, nullptr);

	check(QCOMDepthStencilSurface->GetViewType() == VK_IMAGE_VIEW_TYPE_2D);
	check(QCOMDepthStencilSurface->Image != VK_NULL_HANDLE);

	QCOMDepthStencilView = new FVulkanView(*QCOMDepthStencilSurface->Device, DescriptorType);
	QCOMDepthStencilView->InitAsTextureView(
		  QCOMDepthStencilSurface->Image
		, QCOMDepthStencilSurface->GetViewType()
		, QCOMDepthStencilSurface->GetFullAspectMask()
		, QCOMDepthStencilSurface->GetDesc().Format
		, QCOMDepthStencilSurface->ViewFormat
		, 0
		, FMath::Max(QCOMDepthStencilSurface->GetNumMips(), 1u)
		, 0
		, 1u
		, false
	);

	if (QCOMDepthStencilSurface->GetFullAspectMask() == QCOMDepthStencilSurface->GetPartialAspectMask())
	{
		QCOMDepthView = QCOMDepthStencilView;
	}
	else
	{
		QCOMDepthView = new FVulkanView(*QCOMDepthStencilSurface->Device, DescriptorType);
		QCOMDepthView->InitAsTextureView(
			  QCOMDepthStencilSurface->Image
			, QCOMDepthStencilSurface->GetViewType()
			, QCOMDepthStencilSurface->GetPartialAspectMask()
			, QCOMDepthStencilSurface->GetDesc().Format
			, QCOMDepthStencilSurface->ViewFormat
			, 0
			, FMath::Max(QCOMDepthStencilSurface->GetNumMips(), 1u)
			, 0
			, 1u
			, false
		);
	}
}

const FVulkanView* FVulkanSwapChain::GetOrCreateQCOMDepthStencilView(const FVulkanTexture& InSurface) const
{
	if (QCOMDepthStencilView)
	{
		return QCOMDepthStencilView;
	}

	CreateQCOMDepthStencil(InSurface);

	return QCOMDepthStencilView;
}

const FVulkanView* FVulkanSwapChain::GetOrCreateQCOMDepthView(const FVulkanTexture& InSurface) const
{
	if (QCOMDepthView)
	{
		return QCOMDepthView;
	}

	CreateQCOMDepthStencil(InSurface);

	return QCOMDepthView;
}

const FVulkanTexture* FVulkanSwapChain::GetQCOMDepthStencilSurface() const
{
	return QCOMDepthStencilSurface;
}

void FVulkanDevice::SetupPresentQueue(VkSurfaceKHR Surface)
{
	if (!PresentQueue)
	{
		const auto SupportsPresent = [Surface](VkPhysicalDevice PhysicalDevice, FVulkanQueue* Queue)
		{
			VkBool32 bSupportsPresent = VK_FALSE;
			const uint32 FamilyIndex = Queue->GetFamilyIndex();
			VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, FamilyIndex, Surface, &bSupportsPresent));
			if (bSupportsPresent)
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("Queue Family %d: Supports Present"), FamilyIndex);
			}
			return (bSupportsPresent == VK_TRUE);
		};

		bool bGfx = SupportsPresent(Gpu, GfxQueue);
		if (!bGfx)
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Cannot find a compatible Vulkan device that supports surface presentation.\n\n"), TEXT("Vulkan device not available"));
			FPlatformMisc::RequestExitWithStatus(true, 1);
		}

		bool bCompute = SupportsPresent(Gpu, ComputeQueue);
		if (TransferQueue->GetFamilyIndex() != GfxQueue->GetFamilyIndex() && TransferQueue->GetFamilyIndex() != ComputeQueue->GetFamilyIndex())
		{
			SupportsPresent(Gpu, TransferQueue);
		}
		if (GAllowPresentOnComputeQueue.GetValueOnAnyThread() != 0 && ComputeQueue->GetFamilyIndex() != GfxQueue->GetFamilyIndex() && bCompute)
		{
			//#todo-rco: Do other IHVs have a fast path here?
			bPresentOnComputeQueue = (VendorId == EGpuVendorId::Amd);
			PresentQueue = ComputeQueue;
		}
		else
		{
			PresentQueue = GfxQueue;
		}
	}
}
