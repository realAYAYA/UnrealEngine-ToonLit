// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRHMD_RenderBridge.h"
#include "OpenXRHMD.h"
#include "OpenXRHMD_Swapchain.h"
#include "OpenXRCore.h"

bool FOpenXRRenderBridge::Present(int32& InOutSyncInterval)
{
	bool bNeedsNativePresent = true;

	if (OpenXRHMD)
	{
		OpenXRHMD->OnFinishRendering_RHIThread();
		bNeedsNativePresent = !OpenXRHMD->IsStandaloneStereoOnlyDevice() && bNativePresent_RHIThread;
	}

	InOutSyncInterval = 0; // VSync off

	return bNeedsNativePresent;
}

#ifdef XR_USE_GRAPHICS_API_D3D11
class FD3D11RenderBridge : public FOpenXRRenderBridge
{
public:
	FD3D11RenderBridge(XrInstance InInstance, XrSystemId InSystem)
		: Binding()
	{
		PFN_xrGetD3D11GraphicsRequirementsKHR GetD3D11GraphicsRequirementsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&GetD3D11GraphicsRequirementsKHR));

		XrGraphicsRequirementsD3D11KHR Requirements;
		Requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
		Requirements.next = nullptr;
		if (XR_ENSURE(GetD3D11GraphicsRequirementsKHR(InInstance, InSystem, &Requirements)))
		{
			AdapterLuid = reinterpret_cast<uint64&>(Requirements.adapterLuid);
		}
	}

	virtual void* GetGraphicsBinding() override
	{
		Binding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
		Binding.next = nullptr;
		Binding.device = GetID3D11DynamicRHI()->RHIGetDevice();
		return &Binding;
	}

	virtual FXRSwapChainPtr CreateSwapchain(XrSession InSession, uint8 Format, uint8& OutActualFormat, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags CreateFlags, const FClearValueBinding& ClearValueBinding) override final
	{
		return CreateSwapchain_D3D11(InSession, Format, OutActualFormat, SizeX, SizeY, ArraySize, NumMips, NumSamples, CreateFlags, ClearValueBinding);
	}

private:
	XrGraphicsBindingD3D11KHR Binding;
};

FOpenXRRenderBridge* CreateRenderBridge_D3D11(XrInstance InInstance, XrSystemId InSystem) { return new FD3D11RenderBridge(InInstance, InSystem); }
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
class FD3D12RenderBridge : public FOpenXRRenderBridge
{
public:
	FD3D12RenderBridge(XrInstance InInstance, XrSystemId InSystem)
		: Binding()
	{
		PFN_xrGetD3D12GraphicsRequirementsKHR GetD3D12GraphicsRequirementsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&GetD3D12GraphicsRequirementsKHR));

		XrGraphicsRequirementsD3D12KHR Requirements;
		Requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR;
		Requirements.next = nullptr;
		if (XR_ENSURE(GetD3D12GraphicsRequirementsKHR(InInstance, InSystem, &Requirements)))
		{
			AdapterLuid = reinterpret_cast<uint64&>(Requirements.adapterLuid);
		}
	}

	virtual void* GetGraphicsBinding() override
	{
		Binding.type = XR_TYPE_GRAPHICS_BINDING_D3D12_KHR;
		Binding.next = nullptr;
		Binding.device = GetID3D12DynamicRHI()->RHIGetDevice(0);
		Binding.queue = GetID3D12DynamicRHI()->RHIGetCommandQueue();
		return &Binding;
	}

	virtual FXRSwapChainPtr CreateSwapchain(XrSession InSession, uint8 Format, uint8& OutActualFormat, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags CreateFlags, const FClearValueBinding& ClearValueBinding) override final
	{
		return CreateSwapchain_D3D12(InSession, Format, OutActualFormat, SizeX, SizeY, ArraySize, NumMips, NumSamples, CreateFlags, ClearValueBinding);
	}

private:
	XrGraphicsBindingD3D12KHR Binding;
};

FOpenXRRenderBridge* CreateRenderBridge_D3D12(XrInstance InInstance, XrSystemId InSystem) { return new FD3D12RenderBridge(InInstance, InSystem); }
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
class FOpenGLRenderBridge : public FOpenXRRenderBridge
{
public:
	FOpenGLRenderBridge(XrInstance InInstance, XrSystemId InSystem)
		: Binding()
	{
		PFN_xrGetOpenGLGraphicsRequirementsKHR GetOpenGLGraphicsRequirementsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&GetOpenGLGraphicsRequirementsKHR));

		XrGraphicsRequirementsOpenGLKHR Requirements;
		Requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
		Requirements.next = nullptr;
		Requirements.minApiVersionSupported = 0;
		Requirements.maxApiVersionSupported = 0;
		XR_ENSURE(GetOpenGLGraphicsRequirementsKHR(InInstance, InSystem, &Requirements));

		IOpenGLDynamicRHI* RHI = GetIOpenGLDynamicRHI();
		XrVersion RHIVersion = XR_MAKE_VERSION(RHI->RHIGetGLMajorVersion(), RHI->RHIGetGLMinorVersion(), 0);
		if (RHIVersion < Requirements.minApiVersionSupported) //-V547
		{
			UE_LOG(LogHMD, Fatal, TEXT("The OpenGL API version does not meet the minimum version required by the OpenXR runtime"));
		}

		if (RHIVersion > Requirements.maxApiVersionSupported) //-V547
		{
			UE_LOG(LogHMD, Warning, TEXT("The OpenGL API version has not been tested with the OpenXR runtime"));
		}
	}

	virtual void* GetGraphicsBinding() override
	{
#if PLATFORM_WINDOWS
		Binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
		Binding.next = nullptr;
		Binding.hDC = wglGetCurrentDC();
		Binding.hGLRC = wglGetCurrentContext();
		return &Binding;
#endif
		return nullptr;
	}

	virtual FXRSwapChainPtr CreateSwapchain(XrSession InSession, uint8 Format, uint8& OutActualFormat, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags CreateFlags, const FClearValueBinding& ClearValueBinding) override final
	{
		return CreateSwapchain_OpenGL(InSession, Format, OutActualFormat, SizeX, SizeY, ArraySize, NumMips, NumSamples, CreateFlags, ClearValueBinding);
	}

private:
#if PLATFORM_WINDOWS
	XrGraphicsBindingOpenGLWin32KHR Binding;
#endif
};

FOpenXRRenderBridge* CreateRenderBridge_OpenGL(XrInstance InInstance, XrSystemId InSystem) { return new FOpenGLRenderBridge(InInstance, InSystem); }
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
class FOpenGLESRenderBridge : public FOpenXRRenderBridge
{
public:
	FOpenGLESRenderBridge(XrInstance InInstance, XrSystemId InSystem)
		: Binding()
	{
		PFN_xrGetOpenGLESGraphicsRequirementsKHR GetOpenGLESGraphicsRequirementsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&GetOpenGLESGraphicsRequirementsKHR));

		XrGraphicsRequirementsOpenGLESKHR Requirements;
		Requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
		Requirements.next = nullptr;
		Requirements.minApiVersionSupported = 0;
		Requirements.maxApiVersionSupported = 0;
		XR_ENSURE(GetOpenGLESGraphicsRequirementsKHR(InInstance, InSystem, &Requirements));

#if PLATFORM_ANDROID
		IOpenGLDynamicRHI* RHI = GetIOpenGLDynamicRHI();
		XrVersion RHIVersion = XR_MAKE_VERSION(RHI->RHIGetGLMajorVersion(), RHI->RHIGetGLMinorVersion(), 0);
		if (RHIVersion < Requirements.minApiVersionSupported) //-V547
		{
			UE_LOG(LogHMD, Fatal, TEXT("The OpenGLES API version does not meet the minimum version required by the OpenXR runtime"));
		}

		if (RHIVersion > Requirements.maxApiVersionSupported) //-V547
		{
			UE_LOG(LogHMD, Warning, TEXT("The OpenGLES API version has not been tested with the OpenXR runtime"));
		}
#endif
	}

	virtual void* GetGraphicsBinding() override
	{
#if PLATFORM_ANDROID
		IOpenGLDynamicRHI* RHI = GetIOpenGLDynamicRHI();
		Binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
		Binding.next = nullptr;
		Binding.display = RHI->RHIGetEGLDisplay();
		Binding.config = RHI->RHIGetEGLConfig();
		Binding.context = RHI->RHIGetEGLContext();
		return &Binding;
#endif
		return nullptr;
	}

	virtual FXRSwapChainPtr CreateSwapchain(XrSession InSession, uint8 Format, uint8& OutActualFormat, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags CreateFlags, const FClearValueBinding& ClearValueBinding) override final
	{
		return CreateSwapchain_OpenGLES(InSession, Format, OutActualFormat, SizeX, SizeY, ArraySize, NumMips, NumSamples, CreateFlags, ClearValueBinding);
	}

private:
#if PLATFORM_ANDROID
	XrGraphicsBindingOpenGLESAndroidKHR Binding;
#endif
};

FOpenXRRenderBridge* CreateRenderBridge_OpenGLES(XrInstance InInstance, XrSystemId InSystem) { return new FOpenGLESRenderBridge(InInstance, InSystem); }
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
class FVulkanRenderBridge : public FOpenXRRenderBridge
{
public:
	FVulkanRenderBridge(XrInstance InInstance, XrSystemId InSystem)
		: Binding()
		, Instance(InInstance)
		, System(InSystem)
	{
		PFN_xrGetVulkanGraphicsRequirementsKHR GetVulkanGraphicsRequirementsKHR;
		XR_ENSURE(xrGetInstanceProcAddr(InInstance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&GetVulkanGraphicsRequirementsKHR));

		XrGraphicsRequirementsVulkanKHR Requirements;
		Requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
		Requirements.next = nullptr;
		Requirements.minApiVersionSupported = 0;
		Requirements.maxApiVersionSupported = 0;
		XR_ENSURE(GetVulkanGraphicsRequirementsKHR(InInstance, InSystem, &Requirements));

		IVulkanDynamicRHI* VulkanRHI = GetIVulkanDynamicRHI();
		const uint32 VulkanVersion = VulkanRHI->RHIGetVulkanVersion();

		// The extension uses the OpenXR version format instead of the Vulkan one
		XrVersion RHIVersion = XR_MAKE_VERSION(
			VK_VERSION_MAJOR(VulkanVersion),
			VK_VERSION_MINOR(VulkanVersion),
			VK_VERSION_PATCH(VulkanVersion)
		);
		if (RHIVersion < Requirements.minApiVersionSupported) //-V547
		{
			UE_LOG(LogHMD, Fatal, TEXT("The Vulkan API version does not meet the minimum version required by the OpenXR runtime"));
		}

		if (RHIVersion > Requirements.maxApiVersionSupported) //-V547
		{
			UE_LOG(LogHMD, Warning, TEXT("The Vulkan API version has not been tested with the OpenXR runtime"));
		}

		PFN_xrGetVulkanGraphicsDeviceKHR GetVulkanGraphicsDeviceKHR;
		XR_ENSURE(xrGetInstanceProcAddr(Instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&GetVulkanGraphicsDeviceKHR));
		XR_ENSURE(GetVulkanGraphicsDeviceKHR(Instance, System, VulkanRHI->RHIGetVkInstance(), &Gpu));
	}

	virtual void* GetGraphicsBinding() override
	{
		IVulkanDynamicRHI* VulkanRHI = GetIVulkanDynamicRHI();

		Binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
		Binding.next = nullptr;
		Binding.instance = VulkanRHI->RHIGetVkInstance();
		Binding.physicalDevice = VulkanRHI->RHIGetVkPhysicalDevice();
		Binding.device = VulkanRHI->RHIGetVkDevice();
		Binding.queueFamilyIndex = VulkanRHI->RHIGetGraphicsQueueFamilyIndex();
		Binding.queueIndex = 0;
		return &Binding;
	}

	virtual uint64 GetGraphicsAdapterLuid() override
	{
		if (!AdapterLuid)
		{
			AdapterLuid = GetIVulkanDynamicRHI()->RHIGetGraphicsAdapterLUID(Gpu);
		}

		return AdapterLuid;
	}

	virtual FXRSwapChainPtr CreateSwapchain(XrSession InSession, uint8 Format, uint8& OutActualFormat, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags CreateFlags, const FClearValueBinding& ClearValueBinding) override final
	{
		return CreateSwapchain_Vulkan(InSession, Format, OutActualFormat, SizeX, SizeY, ArraySize, NumMips, NumSamples, CreateFlags, ClearValueBinding);
	}

private:
	XrGraphicsBindingVulkanKHR Binding;
	XrInstance Instance;
	XrSystemId System;
	VkPhysicalDevice Gpu;
};

FOpenXRRenderBridge* CreateRenderBridge_Vulkan(XrInstance InInstance, XrSystemId InSystem) { return new FVulkanRenderBridge(InInstance, InSystem); }
#endif
