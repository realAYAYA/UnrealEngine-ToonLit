// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayBase.h"
#include "XRRenderBridge.h"
#include "XRSwapChain.h"
#include "OpenXRPlatformRHI.h"

#include <openxr/openxr.h>

class FOpenXRHMD;
class FRHICommandListImmediate;

class FOpenXRRenderBridge : public FXRRenderBridge
{
public:
	FOpenXRRenderBridge(XrInstance InInstance)
		: Instance(InInstance)
		, OpenXRHMD(nullptr)
	{ }

	void SetOpenXRHMD(FOpenXRHMD* InHMD) { OpenXRHMD = InHMD; }
	virtual uint64 GetGraphicsAdapterLuid(XrSystemId InSystem) { return 0; };
	virtual void* GetGraphicsBinding(XrSystemId InSystem) = 0;

	virtual FXRSwapChainPtr CreateSwapchain(XrSession InSession, uint8 Format, uint8& OutActualFormat, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, ETextureCreateFlags CreateFlags, const FClearValueBinding& ClearValueBinding, ETextureCreateFlags AuxiliaryCreateFlags = ETextureCreateFlags::None) = 0;

	FXRSwapChainPtr CreateSwapchain(XrSession InSession, FRHITexture2D* Template, ETextureCreateFlags CreateFlags)
	{
		if (!Template)
		{
			return nullptr;
		}

		uint8 UnusedOutFormat = 0;
		return CreateSwapchain(InSession,
			Template->GetFormat(),
			UnusedOutFormat,
			Template->GetSizeX(),
			Template->GetSizeY(),
			1,
			Template->GetNumMips(),
			Template->GetNumSamples(),
			Template->GetFlags() | CreateFlags,
			Template->GetClearBinding());
	}

	/** FRHICustomPresent */
	virtual bool Present(int32& InOutSyncInterval) override;

	virtual bool Support10BitSwapchain() const { return false; }

	virtual bool HDRGetMetaDataForStereo(EDisplayOutputFormat& OutDisplayOutputFormat, EDisplayColorGamut& OutDisplayColorGamut, bool& OutbHDRSupported) { return false; }

	virtual void SetSkipRate(uint32 SkipRate) {}

	virtual void HMDOnFinishRendering_RHIThread();

protected:
	XrInstance Instance;
	FOpenXRHMD* OpenXRHMD;

private:
};

#ifdef XR_USE_GRAPHICS_API_D3D11
FOpenXRRenderBridge* CreateRenderBridge_D3D11(XrInstance InInstance);
#endif
#ifdef XR_USE_GRAPHICS_API_D3D12
FOpenXRRenderBridge* CreateRenderBridge_D3D12(XrInstance InInstance);
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
FOpenXRRenderBridge* CreateRenderBridge_OpenGLES(XrInstance InInstance);
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
FOpenXRRenderBridge* CreateRenderBridge_OpenGL(XrInstance InInstance);
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
FOpenXRRenderBridge* CreateRenderBridge_Vulkan(XrInstance InInstance);
#endif
