// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RHI.h"

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	THIRD_PARTY_INCLUDES_START
		#include <GL/glcorearb.h>
		#include <GL/glext.h>
		#include <GL/wglext.h>
	THIRD_PARTY_INCLUDES_END
	#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
	THIRD_PARTY_INCLUDES_START
		#include <GL/glcorearb.h>
		#include <GL/glext.h>
	THIRD_PARTY_INCLUDES_END
#elif PLATFORM_ANDROID
	#include "Android/AndroidPlatform.h"
	THIRD_PARTY_INCLUDES_START
		#include <EGL/egl.h>
		#include <EGL/eglext.h>
		#include <GLES3/gl31.h>
	THIRD_PARTY_INCLUDES_END

	#ifndef USE_ANDROID_EGL_NO_ERROR_CONTEXT
		#if UE_BUILD_SHIPPING
			#define USE_ANDROID_EGL_NO_ERROR_CONTEXT 1
		#else
			#define USE_ANDROID_EGL_NO_ERROR_CONTEXT 0
		#endif
	#endif
#endif

struct IOpenGLDynamicRHI : public FDynamicRHIPSOFallback
{
	virtual ERHIInterfaceType GetInterfaceType() const override final { return ERHIInterfaceType::OpenGL; }

	virtual int32 RHIGetGLMajorVersion() const = 0;
	virtual int32 RHIGetGLMinorVersion() const = 0;

	virtual bool RHISupportsFramebufferSRGBEnable() const = 0;

	virtual FTexture2DRHIRef RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags Flags) = 0;
	virtual FTexture2DRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags Flags) = 0;
	virtual FTextureCubeRHIRef RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags Flags) = 0;

	virtual GLuint RHIGetResource(FRHITexture* InTexture) const = 0;
	virtual bool RHIIsValidTexture(GLuint InTexture) const = 0;
	virtual void RHISetExternalGPUTime(uint32 InExternalGPUTime) = 0;

#if PLATFORM_ANDROID
	virtual EGLDisplay RHIGetEGLDisplay() const = 0;
	virtual EGLSurface RHIGetEGLSurface() const = 0;
	virtual EGLConfig  RHIGetEGLConfig() const = 0;
	virtual EGLContext RHIGetEGLContext() const = 0;
	virtual ANativeWindow* RHIGetEGLNativeWindow() const = 0;
	virtual bool RHIEGLSupportsNoErrorContext() const = 0;

	virtual void RHIInitEGLInstanceGLES2() = 0;
	virtual void RHIInitEGLBackBuffer() = 0;
	virtual void RHIEGLSetCurrentRenderingContext() = 0;
	virtual void RHIEGLTerminateContext() = 0;
#endif
};

inline IOpenGLDynamicRHI* GetIOpenGLDynamicRHI()
{
	check(GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::OpenGL);
	return GetDynamicRHI<IOpenGLDynamicRHI>();
}
