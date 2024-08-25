// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidEGL.h: Private EGL definitions for Android-specific functionality
=============================================================================*/
#pragma once

#include "Android/AndroidPlatform.h"

#if USE_ANDROID_OPENGL

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

struct AndroidESPImpl;
struct ANativeWindow;

#ifndef USE_ANDROID_EGL_NO_ERROR_CONTEXT
#if UE_BUILD_SHIPPING
#define USE_ANDROID_EGL_NO_ERROR_CONTEXT 1
#else
#define USE_ANDROID_EGL_NO_ERROR_CONTEXT 0
#endif
#endif // USE_ANDROID_EGL_NO_ERROR_CONTEXT

DECLARE_LOG_CATEGORY_EXTERN(LogEGL, Log, All);

struct FPlatformOpenGLContext
{
	EGLContext	eglContext;
	GLuint		ViewportFramebuffer;
	EGLSurface	eglSurface;
	GLuint		DefaultVertexArrayObject;
	GLuint		BackBufferResource;
	GLenum		BackBufferTarget;
	GLuint		DummyFrameBuffer;

	FPlatformOpenGLContext()
	{
		Reset();
	}

	void Reset()
	{
		eglContext = EGL_NO_CONTEXT;
		eglSurface = EGL_NO_SURFACE;
		ViewportFramebuffer = 0;
		DefaultVertexArrayObject = 0;
		BackBufferResource = 0;
		BackBufferTarget = 0;
		DummyFrameBuffer = 0;
	}
};


class AndroidEGL
{
public:
	enum APIVariant
	{
		AV_OpenGLES,
		AV_OpenGLCore
	};

	static AndroidEGL* GetInstance();
	~AndroidEGL();	

	bool IsInitialized();
	void InitBackBuffer();
	void DestroyBackBuffer();
	void Init( APIVariant API, uint32 MajorVersion, uint32 MinorVersion);
	void ReInit();
	void UnBind();
	void UnBindRender();
	void UnBindShared();
	bool SwapBuffers();
	void Terminate();
	void InitSurface(bool bUseSmallSurface, bool bCreateWndSurface);
	void InitRenderSurface(bool bUseSmallSurface, bool bCreateWndSurface);
	void InitSharedSurface(bool bUseSmallSurface);
	void UpdateBuffersTransform();
	bool IsOfflineSurfaceRequired();

	void GetDimensions(uint32& OutWidth, uint32& OutHeight);
	bool IsUsingRobustContext() const { return bIsEXTRobustContextActive; }

	EGLDisplay GetDisplay() const;
	EGLSurface GetSurface() const;
	EGLConfig GetConfig() const;
	ANativeWindow* GetNativeWindow() const;
	void GetSwapIntervalRange(EGLint& OutMinSwapInterval, EGLint& OutMaxSwapInterval) const;

	EGLContext CreateContext(EGLContext InSharedContext = EGL_NO_CONTEXT);
	int32 GetError();
	EGLBoolean SetCurrentContext(EGLContext InContext, EGLSurface InSurface);

	void AcquireCurrentRenderingContext();
	void ReleaseContextOwnership();

	GLuint GetResolveFrameBuffer();
	bool IsCurrentContextValid();
	EGLContext  GetCurrentContext(  );
	void SetCurrentSharedContext();
	void SetCurrentRenderingContext();
	uint32_t GetCurrentContextType();
	FPlatformOpenGLContext* GetRenderingContext();
	FPlatformOpenGLContext* GetSharedContext();
	bool GetSupportsNoErrorContext();

	// recreate the EGL surface for the current hardware window.
	void SetRenderContextWindowSurface();

	// Called from game thread when a window is reinited.
	void RefreshWindowSize();

protected:
	AndroidEGL();
	static AndroidEGL* Singleton ;

private:
	void InitEGL(APIVariant API);
	void TerminateEGL();

	void CreateEGLRenderSurface(ANativeWindow* InWindow, bool bCreateWndSurface);
	void CreateEGLSharedSurface();
	void DestroyRenderSurface();
	void DestroySharedSurface();

	bool InitContexts();
	void DestroyContext(EGLContext InContext);

	void ResetDisplay();

	AndroidESPImpl* PImplData;

	void ResetInternal();
	void LogConfigInfo(EGLConfig  EGLConfigInfo);

	// Actual Update to the egl surface to match the GT's requested size.
	void ResizeRenderContextSurface();
	void ResizeSharedContextSurface();

	bool bSupportsKHRCreateContext;
	bool bSupportsKHRSurfacelessContext;
	bool bSupportsKHRNoErrorContext;
	bool bSupportsEXTRobustContext;
	bool bIsEXTRobustContextActive = false;

	int *ContextAttributes;
};

#define ENABLE_CONFIG_FILTER 1
#define ENABLE_EGL_DEBUG 0
#define ENABLE_VERIFY_EGL 0
#define ENABLE_VERIFY_EGL_TRACE 0

#if ENABLE_VERIFY_EGL

#define VERIFY_EGL(msg) { VerifyEGLResult(eglGetError(),TEXT(#msg),TEXT(""),TEXT(__FILE__),__LINE__); }

void VerifyEGLResult(EGLint ErrorCode, const TCHAR* Msg1, const TCHAR* Msg2, const TCHAR* Filename, uint32 Line)
{
	if (ErrorCode != EGL_SUCCESS)
	{
		static const TCHAR* EGLErrorStrings[] =
		{
			TEXT("EGL_NOT_INITIALIZED"),
			TEXT("EGL_BAD_ACCESS"),
			TEXT("EGL_BAD_ALLOC"),
			TEXT("EGL_BAD_ATTRIBUTE"),
			TEXT("EGL_BAD_CONFIG"),
			TEXT("EGL_BAD_CONTEXT"),
			TEXT("EGL_BAD_CURRENT_SURFACE"),
			TEXT("EGL_BAD_DISPLAY"),
			TEXT("EGL_BAD_MATCH"),
			TEXT("EGL_BAD_NATIVE_PIXMAP"),
			TEXT("EGL_BAD_NATIVE_WINDOW"),
			TEXT("EGL_BAD_PARAMETER"),
			TEXT("EGL_BAD_SURFACE"),
			TEXT("EGL_CONTEXT_LOST"),
			TEXT("UNKNOWN EGL ERROR")
		};

		uint32 ErrorIndex = FMath::Min<uint32>(ErrorCode - EGL_SUCCESS, UE_ARRAY_COUNT(EGLErrorStrings) - 1);
		UE_LOG(LogRHI, Warning, TEXT("%s(%u): %s%s failed with error %s (0x%x)"),
			Filename, Line, Msg1, Msg2, EGLErrorStrings[ErrorIndex], ErrorCode);
		check(0);
	}
}

class FEGLErrorScope
{
public:
	FEGLErrorScope(
		const TCHAR* InFunctionName,
		const TCHAR* InFilename,
		const uint32 InLine)
		: FunctionName(InFunctionName)
		, Filename(InFilename)
		, Line(InLine)
	{
#if ENABLE_VERIFY_EGL_TRACE
		UE_LOG(LogRHI, Log, TEXT("EGL log before %s(%d): %s"), InFilename, InLine, InFunctionName);
#endif
		CheckForErrors(TEXT("Before "));
	}

	~FEGLErrorScope()
	{
#if ENABLE_VERIFY_EGL_TRACE
		UE_LOG(LogRHI, Log, TEXT("EGL log after  %s(%d): %s"), Filename, Line, FunctionName);
#endif
		CheckForErrors(TEXT("After "));
	}

private:
	const TCHAR* FunctionName;
	const TCHAR* Filename;
	const uint32 Line;

	void CheckForErrors(const TCHAR* PrefixString)
	{
		VerifyEGLResult(eglGetError(), PrefixString, FunctionName, Filename, Line);
	}
};

#define MACRO_TOKENIZER(IdentifierName, Msg, FileName, LineNumber) FEGLErrorScope IdentifierName_ ## LineNumber (Msg, FileName, LineNumber)
#define MACRO_TOKENIZER2(IdentifierName, Msg, FileName, LineNumber) MACRO_TOKENIZER(IdentiferName, Msg, FileName, LineNumber)
#define VERIFY_EGL_SCOPE_WITH_MSG_STR(MsgStr) MACRO_TOKENIZER2(ErrorScope_, MsgStr, TEXT(__FILE__), __LINE__)
#define VERIFY_EGL_SCOPE() VERIFY_EGL_SCOPE_WITH_MSG_STR(ANSI_TO_TCHAR(__FUNCTION__))
#define VERIFY_EGL_FUNC(Func, ...) { VERIFY_EGL_SCOPE_WITH_MSG_STR(TEXT(#Func)); Func(__VA_ARGS__); }
#else
#define VERIFY_EGL(...)
#define VERIFY_EGL_SCOPE(...)
#endif



#endif
