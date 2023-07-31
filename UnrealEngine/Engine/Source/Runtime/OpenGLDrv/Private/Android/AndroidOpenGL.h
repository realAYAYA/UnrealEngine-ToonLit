// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOpenGL.h: Public OpenGL ES definitions for Android-specific functionality
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "RenderingThread.h"
#include "RHI.h"

#if PLATFORM_ANDROID

#include "AndroidEGL.h"

#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2ext.h>
	
typedef GLsync			UGLsync;
#define GLdouble		GLfloat
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;
#define GL_CLAMP		GL_CLAMP_TO_EDGE

#include "OpenGLES.h"

typedef khronos_stime_nanoseconds_t EGLnsecsANDROID;

typedef GLboolean(GL_APIENTRYP PFNeglPresentationTimeANDROID) (EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time);
typedef GLboolean(GL_APIENTRYP PFNeglGetNextFrameIdANDROID) (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR *frameId);
typedef GLboolean(GL_APIENTRYP PFNeglGetCompositorTimingANDROID) (EGLDisplay dpy, EGLSurface surface, EGLint numTimestamps, const EGLint *names, EGLnsecsANDROID *values);
typedef GLboolean(GL_APIENTRYP PFNeglGetFrameTimestampsANDROID) (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR frameId, EGLint numTimestamps, const EGLint *timestamps, EGLnsecsANDROID *values);
typedef GLboolean(GL_APIENTRYP PFNeglQueryTimestampSupportedANDROID) (EGLDisplay dpy, EGLSurface surface, EGLint timestamp);

#define EGL_TIMESTAMPS_ANDROID 0x3430
#define EGL_COMPOSITE_DEADLINE_ANDROID 0x3431
#define EGL_COMPOSITE_INTERVAL_ANDROID 0x3432
#define EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID 0x3433
#define EGL_REQUESTED_PRESENT_TIME_ANDROID 0x3434
#define EGL_RENDERING_COMPLETE_TIME_ANDROID 0x3435
#define EGL_COMPOSITION_LATCH_TIME_ANDROID 0x3436
#define EGL_FIRST_COMPOSITION_START_TIME_ANDROID 0x3437
#define EGL_LAST_COMPOSITION_START_TIME_ANDROID 0x3438
#define EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID 0x3439
#define EGL_DISPLAY_PRESENT_TIME_ANDROID 0x343A
#define EGL_DEQUEUE_READY_TIME_ANDROID 0x343B
#define EGL_READS_DONE_TIME_ANDROID 0x343C


extern "C"
{
	extern PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV_p;
	extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_p;
	extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_p;
	extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_p;
	extern PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_p;

	extern PFNeglPresentationTimeANDROID eglPresentationTimeANDROID_p;
	extern PFNeglGetNextFrameIdANDROID eglGetNextFrameIdANDROID_p;
	extern PFNeglGetCompositorTimingANDROID eglGetCompositorTimingANDROID_p;
	extern PFNeglGetFrameTimestampsANDROID eglGetFrameTimestampsANDROID_p;
	extern PFNeglQueryTimestampSupportedANDROID eglQueryTimestampSupportedANDROID_p;
	extern PFNeglQueryTimestampSupportedANDROID eglGetCompositorTimingSupportedANDROID_p;
	extern PFNeglQueryTimestampSupportedANDROID eglGetFrameTimestampsSupportedANDROID_p;
}

namespace GLFuncPointers
{
	// GL_QCOM_shader_framebuffer_fetch_noncoherent
	extern PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC	glFramebufferFetchBarrierQCOM;
}

struct FAndroidOpenGL : public FOpenGLES
{
	static FORCEINLINE bool HasHardwareHiddenSurfaceRemoval() { return bHasHardwareHiddenSurfaceRemoval; };

	// Optional:
	static void QueryTimestampCounter(GLuint QueryID) 
	{
	};

	static GLuint MakeVirtualQueryReal(GLuint QueryID);

	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs)
	{
		*(char*)3 = 0; // this is virtualized and should not be called
	}

	static void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult);

	static void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint64* OutResult);

	static void BeginQuery(GLenum QueryType, GLuint QueryId);

	static void EndQuery(GLenum QueryType);

	static bool SupportsFramebufferSRGBEnable();

	static FORCEINLINE void DeleteSync(UGLsync Sync)
	{
		if (GUseThreadedRendering)
		{
			glDeleteSync( Sync );
		}
	}

	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags)
	{
		return GUseThreadedRendering ? glFenceSync( Condition, Flags ) : 0;
	}

	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		if (GUseThreadedRendering)
		{
			return (glIsSync( Sync ) == GL_TRUE) ? true : false;
		}
		return true;
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		if (GUseThreadedRendering)
		{
			GLenum Result = glClientWaitSync( Sync, Flags, Timeout );
			switch (Result)
			{
				case GL_ALREADY_SIGNALED:		return FR_AlreadySignaled;
				case GL_TIMEOUT_EXPIRED:		return FR_TimeoutExpired;
				case GL_CONDITION_SATISFIED:	return FR_ConditionSatisfied;
			}
			return FR_WaitFailed;
		}
		return FR_ConditionSatisfied;
	}
	
	// Disable all queries except occlusion
	// Query is a limited resource on Android and we better spent them all on occlusion
	static FORCEINLINE bool SupportsTimestampQueries()					{ return false; }
	static FORCEINLINE bool SupportsDisjointTimeQueries()				{ return false; }

	enum class EImageExternalType : uint8
	{
		None,
		ImageExternal100,
		ImageExternal300,
		ImageExternalESSL300
	};

	static FORCEINLINE bool SupportsImageExternal() { return bSupportsImageExternal; }

	static FORCEINLINE EImageExternalType GetImageExternalType() { return ImageExternalType; }

	static FORCEINLINE GLint GetMaxComputeUniformComponents() { check(MaxComputeUniformComponents != -1); return MaxComputeUniformComponents; }
	static FORCEINLINE GLint GetFirstComputeUAVUnit()			{ return 0; }
	static FORCEINLINE GLint GetMaxComputeUAVUnits()			{ check(MaxComputeUAVUnits != -1); return MaxComputeUAVUnits; }
	static FORCEINLINE GLint GetFirstVertexUAVUnit()			{ return 0; }
	static FORCEINLINE GLint GetFirstPixelUAVUnit()				{ return 0; }
	static FORCEINLINE GLint GetMaxPixelUAVUnits()				{ check(MaxPixelUAVUnits != -1); return MaxPixelUAVUnits; }
	static FORCEINLINE GLint GetMaxCombinedUAVUnits()			{ return MaxCombinedUAVUnits; }
		
	static FORCEINLINE void FrameBufferFetchBarrier()
	{
		if (glFramebufferFetchBarrierQCOM)
		{
			glFramebufferFetchBarrierQCOM();
		}
	}
		
	static void ProcessExtensions(const FString& ExtensionsString);
	static void SetupDefaultGLContextState(const FString& ExtensionsString);

	static bool RequiresAdrenoTilingModeHint();
	static void EnableAdrenoTilingModeHint(bool bEnable);
	static bool bRequiresAdrenoTilingHint;

	/** supported OpenGL ES version queried from the system */
	static int32 GLMajorVerion;
	static int32 GLMinorVersion;

	static FORCEINLINE GLuint GetMajorVersion()
	{
		return GLMajorVerion;
	}

	static FORCEINLINE GLuint GetMinorVersion()
	{
		return GLMinorVersion;
	}

	/** Whether device supports image external */
	static bool bSupportsImageExternal;

	/** Type of image external supported */
	static EImageExternalType ImageExternalType;

	/* interface to remote GLES program compiler */
	static TArray<uint8> DispatchAndWaitForRemoteGLProgramCompile(const TArrayView<uint8> ContextData, const TArray<ANSICHAR>& VertexGlslCode, const TArray<ANSICHAR>& PixelGlslCode, const TArray<ANSICHAR>& ComputeGlslCode, FString& FailureMessageOUT);
	static bool AreRemoteCompileServicesActive();
	static bool StartAndWaitForRemoteCompileServices(int NumServices);
	static void StopRemoteCompileServices();
};

typedef FAndroidOpenGL FOpenGL;


/** Unreal tokens that maps to different OpenGL tokens by platform. */
#undef UGL_DRAW_FRAMEBUFFER
#define UGL_DRAW_FRAMEBUFFER	GL_DRAW_FRAMEBUFFER_NV
#undef UGL_READ_FRAMEBUFFER
#define UGL_READ_FRAMEBUFFER	GL_READ_FRAMEBUFFER_NV

#endif // PLATFORM_ANDROID
