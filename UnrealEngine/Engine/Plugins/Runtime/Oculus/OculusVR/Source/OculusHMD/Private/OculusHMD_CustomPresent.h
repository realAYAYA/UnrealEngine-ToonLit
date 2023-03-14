// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_Settings.h"
#include "OculusHMD_GameFrame.h"
#include "XRSwapChain.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "IStereoLayers.h"
#include "XRRenderBridge.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

DECLARE_STATS_GROUP(TEXT("OculusHMD"), STATGROUP_OculusHMD, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("BeginRendering"), STAT_BeginRendering, STATGROUP_OculusHMD);
DECLARE_CYCLE_STAT(TEXT("FinishRendering"), STAT_FinishRendering, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyRender"), STAT_LatencyRender, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyTimewarp"), STAT_LatencyTimewarp, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("LatencyPostPresent"), STAT_LatencyPostPresent, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("ErrorRender"), STAT_ErrorRender, STATGROUP_OculusHMD);
DECLARE_FLOAT_COUNTER_STAT(TEXT("ErrorTimewarp"), STAT_ErrorTimewarp, STATGROUP_OculusHMD);

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresent
//-------------------------------------------------------------------------------------------------

class FCustomPresent : public FXRRenderBridge
{
public:
	FCustomPresent(class FOculusHMD* InOculusHMD, ovrpRenderAPIType InRenderAPI, EPixelFormat InDefaultPixelFormat, bool InSupportsSRGB);

	// FXRRenderBridge/FRHICustomPresent
	virtual bool NeedsNativePresent() override;
	virtual bool Present(int32& SyncInterval) override;
	virtual void FinishRendering_RHIThread();

	ovrpRenderAPIType GetRenderAPI() const { return RenderAPI; }
	virtual bool IsUsingCorrectDisplayAdapter() const { return true; }

	void UpdateMirrorTexture_RenderThread();
	void ReleaseResources_RHIThread();
	void Shutdown();

	FTexture2DRHIRef GetMirrorTexture() { return MirrorTextureRHI; }

	virtual void* GetOvrpInstance() const { return nullptr; }
	virtual void* GetOvrpPhysicalDevice() const { return nullptr; }
	virtual void* GetOvrpDevice() const { return nullptr; }
	virtual void* GetOvrpCommandQueue() const { return nullptr; }
	EPixelFormat GetPixelFormat(EPixelFormat InFormat) const;
	EPixelFormat GetPixelFormat(ovrpTextureFormat InFormat) const;
	EPixelFormat GetDefaultPixelFormat() const { return DefaultPixelFormat; }
	ovrpTextureFormat GetOvrpTextureFormat(EPixelFormat InFormat, bool usesRGB = true) const;
	ovrpTextureFormat GetDefaultOvrpTextureFormat() const { return DefaultOvrpTextureFormat; }
	ovrpTextureFormat GetDefaultDepthOvrpTextureFormat() const { return DefaultDepthOvrpTextureFormat; }
	static bool IsSRGB(ovrpTextureFormat InFormat);
	virtual int GetSystemRecommendedMSAALevel() const;
	virtual int GetLayerFlags() const { return 0; }

	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, ETextureCreateFlags TexCreateFlags) = 0;
	FXRSwapChainPtr CreateSwapChain_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, const TArray<ovrpTextureHandle>& InTextures, ETextureCreateFlags InTexCreateFlags, const TCHAR* DebugName);
	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* DstTexture, FRHITexture* SrcTexture, FIntRect DstRect = FIntRect(), FIntRect SrcRect = FIntRect(), bool bAlphaPremultiply = false, bool bNoAlphaWrite = false, bool bInvertY = true, bool sRGBSource = false) const;
	void SubmitGPUCommands_RenderThread(FRHICommandListImmediate& RHICmdList);
	virtual void SubmitGPUFrameTime(float GPUFrameTime) { }

	bool SupportsSRGB() { return bSupportsSRGB; }
	bool SupportsSubsampled() { return bSupportsSubsampled; }

protected:
	FOculusHMD* OculusHMD;
	ovrpRenderAPIType RenderAPI;
	EPixelFormat DefaultPixelFormat;
	bool bSupportsSRGB;
	bool bSupportsSubsampled;
	ovrpTextureFormat DefaultOvrpTextureFormat;
	ovrpTextureFormat DefaultDepthOvrpTextureFormat;
	IRendererModule* RendererModule;
	FTexture2DRHIRef MirrorTextureRHI;
	bool bIsStandaloneStereoDevice;
};


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
FCustomPresent* CreateCustomPresent_D3D11(FOculusHMD* InOculusHMD);
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
FCustomPresent* CreateCustomPresent_D3D12(FOculusHMD* InOculusHMD);
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
FCustomPresent* CreateCustomPresent_OpenGL(FOculusHMD* InOculusHMD);
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
FCustomPresent* CreateCustomPresent_Vulkan(FOculusHMD* InOculusHMD);
#endif


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
