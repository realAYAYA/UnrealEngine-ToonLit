// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreOpenGLContext.h"


DEFINE_LOG_CATEGORY_STATIC(LogARCoreOpenGL, Log, All);

TSharedPtr<FGoogleARCoreOpenGLContext, ESPMode::ThreadSafe> FGoogleARCoreOpenGLContext::CreateContext()
{
#if PLATFORM_ANDROID
	if (FAndroidMisc::ShouldUseVulkan())
	{
		auto Context = MakeShared<FGoogleARCoreOpenGLContext, ESPMode::ThreadSafe>();
		Context->InitContext();
		return Context;
	}
#endif
	
	return nullptr;
}

FGoogleARCoreOpenGLContext::~FGoogleARCoreOpenGLContext()
{
	ReleaseContext();
	UE_LOG(LogARCoreOpenGL, Log, TEXT("context destroyed"));
}

void FGoogleARCoreOpenGLContext::InitContext()
{
#if PLATFORM_ANDROID
	UE_LOG(LogARCoreOpenGL, Log, TEXT("FGoogleARCoreOpenGLContext::InitContext"));
	
	IOpenGLDynamicRHI* RHI = GetIOpenGLDynamicRHI();

	RHI->RHIInitEGLInstanceGLES2();
	SaveContext();
	MakeCurrent();
	RHI->RHIInitEGLBackBuffer(); //can be done only after context is made current.
	glGenTextures(1, &PassthroughCameraTextureId);
	RestoreContext();
	
	UE_LOG(LogARCoreOpenGL, Log, TEXT("PassthroughCameraTextureId: %d"), PassthroughCameraTextureId);
#endif
}

void FGoogleARCoreOpenGLContext::ReleaseContext()
{
#if PLATFORM_ANDROID
	glDeleteTextures(1, &PassthroughCameraTextureId);
	PassthroughCameraTextureId = 0;
	GetIOpenGLDynamicRHI()->RHIEGLTerminateContext();
#endif
}

void FGoogleARCoreOpenGLContext::MakeCurrent()
{
#if PLATFORM_ANDROID
	GetIOpenGLDynamicRHI()->RHIEGLSetCurrentRenderingContext();
#endif
}

void FGoogleARCoreOpenGLContext::SaveContext()
{
#if PLATFORM_ANDROID
	SavedDisplay = eglGetCurrentDisplay();
	SavedContext = eglGetCurrentContext();
	SavedDrawSurface = eglGetCurrentSurface(EGL_DRAW);
	SavedReadSurface = eglGetCurrentSurface(EGL_READ);
#endif
}

void FGoogleARCoreOpenGLContext::RestoreContext()
{
#if PLATFORM_ANDROID
	eglMakeCurrent(SavedDisplay, SavedDrawSurface, SavedReadSurface, SavedContext);
	
	SavedDisplay = EGL_NO_DISPLAY;
	SavedContext = EGL_NO_CONTEXT;
	SavedDrawSurface = EGL_NO_SURFACE;
	SavedReadSurface = EGL_NO_SURFACE;
#endif
}

uint32 FGoogleARCoreOpenGLContext::GetCameraTextureId() const
{
#if PLATFORM_ANDROID
	return PassthroughCameraTextureId;
#else
	return 0;
#endif
}
