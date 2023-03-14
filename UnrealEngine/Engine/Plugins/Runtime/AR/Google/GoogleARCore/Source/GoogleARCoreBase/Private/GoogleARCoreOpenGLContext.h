// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Misc/Build.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

#if PLATFORM_ANDROID
#include "IOpenGLDynamicRHI.h"
#endif

class FGoogleARCoreOpenGLContext
{
public:
	static TSharedPtr<FGoogleARCoreOpenGLContext, ESPMode::ThreadSafe> CreateContext();
	~FGoogleARCoreOpenGLContext();
	
	void InitContext();
	void ReleaseContext();
	
	void SaveContext();
	void MakeCurrent();
	void RestoreContext();
	
	uint32 GetCameraTextureId() const;
	
private:
#if PLATFORM_ANDROID
	EGLDisplay SavedDisplay = EGL_NO_DISPLAY;
	EGLContext SavedContext = EGL_NO_CONTEXT;
	EGLSurface SavedDrawSurface = EGL_NO_SURFACE;
	EGLSurface SavedReadSurface = EGL_NO_SURFACE;
	uint32 PassthroughCameraTextureId = 0;
#endif
};
