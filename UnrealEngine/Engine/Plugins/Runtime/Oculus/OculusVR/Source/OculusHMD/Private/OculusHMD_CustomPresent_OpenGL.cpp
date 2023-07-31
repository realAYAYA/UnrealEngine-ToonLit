// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "OculusHMD.h"

#if PLATFORM_WINDOWS
#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresentGL
//-------------------------------------------------------------------------------------------------

class FOpenGLCustomPresent : public FCustomPresent
{
public:
	FOpenGLCustomPresent(FOculusHMD* InOculusHMD, bool srgbSupport);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual int GetLayerFlags() const override;
	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, ETextureCreateFlags InTexCreateFlags) override;
	virtual void SubmitGPUFrameTime(float GPUFrameTime) override;
};


FOpenGLCustomPresent::FOpenGLCustomPresent(FOculusHMD* InOculusHMD, bool srgbSupport) :
	FCustomPresent(InOculusHMD, ovrpRenderAPI_OpenGL, PF_R8G8B8A8, srgbSupport)
{
}


int FOpenGLCustomPresent::GetLayerFlags() const
{
#if PLATFORM_ANDROID
	return ovrpLayerFlag_TextureOriginAtBottomLeft;
#else
	return 0;
#endif
}


FTextureRHIRef FOpenGLCustomPresent::CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, ETextureCreateFlags InTexCreateFlags)
{
	CheckInRenderThread();

	IOpenGLDynamicRHI* DynamicRHI = GetIOpenGLDynamicRHI();

	switch (InResourceType)
	{
	case RRT_Texture2D:
		return DynamicRHI->RHICreateTexture2DFromResource(InFormat, InSizeX, InSizeY, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	case RRT_Texture2DArray:
		return DynamicRHI->RHICreateTexture2DArrayFromResource(InFormat, InSizeX, InSizeY, 2, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	case RRT_TextureCube:
		return DynamicRHI->RHICreateTextureCubeFromResource(InFormat, InSizeX, false, 1, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	default:
		return nullptr;
	}
}

void FOpenGLCustomPresent::SubmitGPUFrameTime(float GPUFrameTime)
{
	GetIOpenGLDynamicRHI()->RHISetExternalGPUTime(GPUFrameTime * 1000);
}


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_OpenGL(FOculusHMD* InOculusHMD)
{
	return new FOpenGLCustomPresent(InOculusHMD, GetIOpenGLDynamicRHI()->RHISupportsFramebufferSRGBEnable());
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
