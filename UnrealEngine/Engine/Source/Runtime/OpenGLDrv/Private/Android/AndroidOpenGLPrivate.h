// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOpenGLPrivate.h: Code shared betweeen AndroidOpenGL and AndroidESDeferredOpenGL (Removed)
=============================================================================*/
#pragma once

#include "Android/AndroidApplication.h"

bool GAndroidGPUInfoReady = false;

// call out to JNI to see if the application was packaged for Oculus Mobile
extern bool AndroidThunkCpp_IsOculusMobileApplication();
extern bool ShouldUseGPUFencesToLimitLatency();


class FAndroidGPUInfo
{
public:
	static FAndroidGPUInfo& Get()
	{
		static FAndroidGPUInfo This;
		return This;
	}

	FString GPUFamily;
	FString GLVersion;
	FString VendorName;
	bool bSupportsFloatingPointRenderTargets;
	bool bSupportsFrameBufferFetch;
	TArray<FString> TargetPlatformNames;

	void RemoveTargetPlatform(FString PlatformName)
	{
		TargetPlatformNames.Remove(PlatformName);
	}

private:
	FAndroidGPUInfo()
	{
		// this is only valid in the game thread, make sure we are initialized there before being called on other threads!
		check(IsInGameThread())

		// make sure GL is started so we can get the supported formats
		AndroidEGL* EGL = AndroidEGL::GetInstance();

		if (!EGL->IsInitialized())
		{
			FAndroidAppEntry::PlatformInit();
		}

		// Do not create a window surface if the app is for Oculus Mobile (use small buffer)
		bool bCreateSurface = !AndroidThunkCpp_IsOculusMobileApplication();
		FPlatformMisc::LowLevelOutputDebugString(TEXT("FAndroidGPUInfo"));
		EGL->InitSurface(false, bCreateSurface);
		EGL->SetCurrentSharedContext();

		// get extensions
		// Process the extension caps directly here, as FOpenGL might not yet be setup
		// Do not process extensions here, because extension pointers may not be setup
		const ANSICHAR* GlGetStringOutput = (const ANSICHAR*) glGetString(GL_EXTENSIONS);
		FString ExtensionsString = GlGetStringOutput;

		GPUFamily = (const ANSICHAR*)glGetString(GL_RENDERER);
		check(!GPUFamily.IsEmpty());

		GLVersion = (const ANSICHAR*)glGetString(GL_VERSION);

		// highest priority is the per-texture version
		if (ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr")))
		{
			TargetPlatformNames.Add(TEXT("Android_ASTC"));
		}
		if (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")))
		{
			TargetPlatformNames.Add(TEXT("Android_DXT"));
		}
		
		TargetPlatformNames.Add(TEXT("Android_ETC2"));

		// finally, generic Android
		TargetPlatformNames.Add(TEXT("Android"));

		bSupportsFloatingPointRenderTargets = 
			ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float")) 
			// According to https://www.khronos.org/registry/gles/extensions/EXT/EXT_color_buffer_float.txt
			|| (ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_float")));

		bSupportsFrameBufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch")) 
			|| ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch ")); // has space at the end to exclude GL_ARM_shader_framebuffer_fetch_depth_stencil match

		GAndroidGPUInfoReady = true;

		VendorName = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VENDOR)));
	}
};
