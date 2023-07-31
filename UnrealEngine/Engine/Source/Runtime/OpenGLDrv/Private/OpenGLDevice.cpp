// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLDevice.cpp: OpenGL device RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Stats/Stats.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "Containers/List.h"
#include "RenderResource.h"
#include "ShaderCore.h"
#include "Shader.h"
#include "RenderUtils.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"
#include "SceneUtils.h"

#include "HardwareInfo.h"
#include "OpenGLProgramBinaryFileCache.h"

#if PLATFORM_ANDROID
#include <jni.h>
extern bool AndroidThunkCpp_IsOculusMobileApplication();
#endif

#ifndef GL_STEREO
#define GL_STEREO			0x0C33
#endif

extern GLint GMaxOpenGLColorSamples;
extern GLint GMaxOpenGLDepthSamples;
extern GLint GMaxOpenGLIntegerSamples;
extern GLint GMaxOpenGLTextureFilterAnisotropic;
extern GLint GMaxOpenGLDrawBuffers;

/** OpenGL texture format table. */
FOpenGLTextureFormat GOpenGLTextureFormats[PF_MAX];

/** Device is necessary for vertex buffers, so they can reach the global device on destruction, and tell it to reset vertex array caches */
static FOpenGLDynamicRHI* PrivateOpenGLDevicePtr = NULL;

/** true if we're not using UBOs. */
bool GUseEmulatedUniformBuffers;

static TAutoConsoleVariable<int32> CVarAllowRGLHIThread(
	TEXT("r.OpenGL.AllowRHIThread"),
	1,
	TEXT("Toggle OpenGL RHI thread support.\n")
	TEXT("0: GL scene rendering operations are performed on the render thread.\n")
	TEXT("1: GL scene rendering operations are queued onto the RHI thread gaining some parallelism with the render thread. (default, mobile feature levels only)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarGLExtraDeletionLatency(
	TEXT("r.OpenGL.ExtraDeletionLatency"),
	1,
	TEXT("Toggle the engine's deferred deletion queue for RHI resources. (default:1)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

void OnQueryCreation( FOpenGLRenderQuery* Query )
{
	check(PrivateOpenGLDevicePtr);
	PrivateOpenGLDevicePtr->RegisterQuery( Query );
}

void OnQueryDeletion( FOpenGLRenderQuery* Query )
{
	if(PrivateOpenGLDevicePtr)
	{
		PrivateOpenGLDevicePtr->UnregisterQuery( Query );
	}
}

void OnQueryInvalidation( void )
{
	if(PrivateOpenGLDevicePtr)
	{
		PrivateOpenGLDevicePtr->InvalidateQueries();
	}
}

void OnProgramDeletion( GLint ProgramResource )
{
	check(PrivateOpenGLDevicePtr);
	PrivateOpenGLDevicePtr->OnProgramDeletion( ProgramResource );
}

void OnBufferDeletion( GLuint BufferResource )
{
	check(PrivateOpenGLDevicePtr);
	PrivateOpenGLDevicePtr->OnBufferDeletion( BufferResource );
}

void OnPixelBufferDeletion( GLuint PixelBufferResource )
{
	check(PrivateOpenGLDevicePtr);
	PrivateOpenGLDevicePtr->OnPixelBufferDeletion( PixelBufferResource );
}

void OnUniformBufferDeletion( GLuint UniformBufferResource, uint32 AllocatedSize, bool bStreamDraw, uint32 , uint8*  )
{
	check(PrivateOpenGLDevicePtr);
	PrivateOpenGLDevicePtr->OnUniformBufferDeletion( UniformBufferResource, AllocatedSize, bStreamDraw );
}

void CachedBindBuffer( GLenum Type, GLuint Buffer )
{
	check(PrivateOpenGLDevicePtr);
	if (Type == GL_ARRAY_BUFFER)
	{
		PrivateOpenGLDevicePtr->CachedBindArrayBuffer(PrivateOpenGLDevicePtr->GetContextStateForCurrentContext(), Buffer);
	}
	else if (Type == GL_ELEMENT_ARRAY_BUFFER)
	{
		PrivateOpenGLDevicePtr->CachedBindElementArrayBuffer(PrivateOpenGLDevicePtr->GetContextStateForCurrentContext(), Buffer);
	}
	else if (Type == GL_SHADER_STORAGE_BUFFER)
	{
		PrivateOpenGLDevicePtr->CachedBindStorageBuffer(PrivateOpenGLDevicePtr->GetContextStateForCurrentContext(), Buffer);
	}
	else
	{
		checkNoEntry();
	}
}

void CachedBindPixelUnpackBuffer( GLenum Type, GLuint Buffer )
{
	check(PrivateOpenGLDevicePtr);
	PrivateOpenGLDevicePtr->CachedBindPixelUnpackBuffer(PrivateOpenGLDevicePtr->GetContextStateForCurrentContext(),Buffer);
}

void CachedBindUniformBuffer( GLuint Buffer )
{
	check(PrivateOpenGLDevicePtr);
	if (FOpenGL::SupportsUniformBuffers())
	{
		PrivateOpenGLDevicePtr->CachedBindUniformBuffer(PrivateOpenGLDevicePtr->GetContextStateForCurrentContext(),Buffer);
	}
}

bool IsUniformBufferBound( GLuint Buffer )
{
	check(PrivateOpenGLDevicePtr);
	return PrivateOpenGLDevicePtr->IsUniformBufferBound(PrivateOpenGLDevicePtr->GetContextStateForCurrentContext(),Buffer);
}

extern void BeginFrame_UniformBufferPoolCleanup();
extern void BeginFrame_VertexBufferCleanup();
extern void BeginFrame_QueryBatchCleanup();
extern void BeginFrame_PollAllFences();


FOpenGLContextState& FOpenGLDynamicRHI::GetContextStateForCurrentContext(bool bAssertIfInvalid)
{
	// most common case
	if (BeginSceneContextType == CONTEXT_Rendering)
	{
		return RenderingContextState;
	}
	
	int32 ContextType = (int32)PlatformOpenGLCurrentContext(PlatformDevice);
	if (bAssertIfInvalid)
	{
			check(ContextType >= 0);
	}
	else if (ContextType < 0)
	{
		return InvalidContextState;
	}

	if (ContextType == CONTEXT_Rendering)
	{
		return RenderingContextState;
	}
	else
	{
		return SharedContextState;
	}
}

void FOpenGLDynamicRHI::RHIBeginFrame()
{
	GPUProfilingData.BeginFrame(this);

#if PLATFORM_ANDROID //adding #if since not sure if this is required for any other platform.
	PendingState.DepthStencil = 0 ;
#endif

	BeginFrame_PollAllFences();
}

void FOpenGLDynamicRHI::RHIEndFrame()
{
	GPUProfilingData.EndFrame();
}

void FOpenGLDynamicRHI::RHIPerFrameRHIFlushComplete()
{
	BeginFrame_UniformBufferPoolCleanup();
	BeginFrame_VertexBufferCleanup();
	BeginFrame_QueryBatchCleanup();
}


void FOpenGLDynamicRHI::RHIBeginScene()
{
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	SceneFrameCounter++;
	if (SceneFrameCounter == INDEX_NONE)
	{
		SceneFrameCounter++;
	}

	static auto* ResourceTableCachingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.ResourceTableCaching"));
	if (ResourceTableCachingCvar == NULL || ResourceTableCachingCvar->GetValueOnAnyThread() == 1)
	{
		ResourceTableFrameCounter = SceneFrameCounter;
	}

	BeginSceneContextType = (int32)PlatformOpenGLCurrentContext(PlatformDevice);

	// recache NULL shader as it can change with ODSC
	NULLPixelShaderRHI = GetNULLPixelShader();
}

void FOpenGLDynamicRHI::RHIEndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
	BeginSceneContextType = CONTEXT_Other;
	NULLPixelShaderRHI = nullptr;
}

#if PLATFORM_ANDROID

JNI_METHOD void Java_com_epicgames_unreal_MediaPlayer14_nativeClearCachedAttributeState(JNIEnv* jenv, jobject thiz, jint PositionAttrib, jint TexCoordsAttrib)
{
	FOpenGLContextState& ContextState = PrivateOpenGLDevicePtr->GetContextStateForCurrentContext();

	// update vertex attributes state
	ContextState.SetVertexAttrEnabled(PositionAttrib, false);
	ContextState.VertexAttrs[PositionAttrib].Size = -1; // will force attribute update

	ContextState.SetVertexAttrEnabled(TexCoordsAttrib, false);
	ContextState.VertexAttrs[TexCoordsAttrib].Size = -1; // will force attribute update

	// make sure the texture is set again
	ContextState.ActiveTexture = 0;
	ContextState.Textures[0].Texture = nullptr;
	ContextState.Textures[0].Target = 0;

	// restore previous program
	FOpenGL::BindProgramPipeline(ContextState.Program);
}

#endif

bool GDisableOpenGLDebugOutput = false;

#if defined(GL_ARB_debug_output) || defined(GL_KHR_debug)

/**
 * Map GL_DEBUG_SOURCE_*_ARB to a human-readable string.
 */
static const TCHAR* GetOpenGLDebugSourceStringARB(GLenum Source)
{
	static const TCHAR* SourceStrings[] =
	{
		TEXT("API"),
		TEXT("System"),
		TEXT("ShaderCompiler"),
		TEXT("ThirdParty"),
		TEXT("Application"),
		TEXT("Other")
	};

	if (Source >= GL_DEBUG_SOURCE_API_ARB && Source <= GL_DEBUG_SOURCE_OTHER_ARB)
	{
		return SourceStrings[Source - GL_DEBUG_SOURCE_API_ARB];
	}
	return TEXT("Unknown");
}

/**
 * Map GL_DEBUG_TYPE_*_ARB to a human-readable string.
 */
static const TCHAR* GetOpenGLDebugTypeStringARB(GLenum Type)
{
	static const TCHAR* TypeStrings[] =
	{
		TEXT("Error"),
		TEXT("Deprecated"),
		TEXT("UndefinedBehavior"),
		TEXT("Portability"),
		TEXT("Performance"),
		TEXT("Other")
	};

	if (Type >= GL_DEBUG_TYPE_ERROR_ARB && Type <= GL_DEBUG_TYPE_OTHER_ARB)
	{
		return TypeStrings[Type - GL_DEBUG_TYPE_ERROR_ARB];
	}
#ifdef GL_KHR_debug
	{
		static const TCHAR* DebugTypeStrings[] =
		{
			TEXT("Marker"),
			TEXT("PushGroup"),
			TEXT("PopGroup"),
		};

		if (Type >= GL_DEBUG_TYPE_MARKER && Type <= GL_DEBUG_TYPE_POP_GROUP)
		{
			return DebugTypeStrings[Type - GL_DEBUG_TYPE_MARKER];
		}
	}
#endif
	return TEXT("Unknown");
}

/**
 * Map GL_DEBUG_SEVERITY_*_ARB to a human-readable string.
 */
static const TCHAR* GetOpenGLDebugSeverityStringARB(GLenum Severity)
{
	static const TCHAR* SeverityStrings[] =
	{
		TEXT("High"),
		TEXT("Medium"),
		TEXT("Low")
	};

	if (Severity >= GL_DEBUG_SEVERITY_HIGH_ARB && Severity <= GL_DEBUG_SEVERITY_LOW_ARB)
	{
		return SeverityStrings[Severity - GL_DEBUG_SEVERITY_HIGH_ARB];
	}
#ifdef GL_KHR_debug
	if(Severity == GL_DEBUG_SEVERITY_NOTIFICATION)
		return TEXT("Notification");
#endif
	return TEXT("Unknown");
}

/**
 * OpenGL debug message callback. Conforms to GLDEBUGPROCARB.
 */
#if PLATFORM_ANDROID
	#ifndef GL_APIENTRY
	#define GL_APIENTRY APIENTRY
	#endif
static void GL_APIENTRY OpenGLDebugMessageCallbackARB(
#else
static void APIENTRY OpenGLDebugMessageCallbackARB(
#endif
	GLenum Source,
	GLenum Type,
	GLuint Id,
	GLenum Severity,
	GLsizei Length,
	const GLchar* Message,
	GLvoid* UserParam)
{
	if (GDisableOpenGLDebugOutput)
		return;
#if !NO_LOGGING
	const TCHAR* SourceStr = GetOpenGLDebugSourceStringARB(Source);
	const TCHAR* TypeStr = GetOpenGLDebugTypeStringARB(Type);
	const TCHAR* SeverityStr = GetOpenGLDebugSeverityStringARB(Severity);

	ELogVerbosity::Type Verbosity = ELogVerbosity::Warning;
	if (Type == GL_DEBUG_TYPE_ERROR_ARB && Severity == GL_DEBUG_SEVERITY_HIGH_ARB)
	{
		Verbosity = ELogVerbosity::Fatal;
	}

	if ((Verbosity & ELogVerbosity::VerbosityMask) <= FLogCategoryLogRHI::CompileTimeVerbosity)
	{
		if (!LogRHI.IsSuppressed(Verbosity))
		{
			FMsg::Logf(__FILE__, __LINE__, LogRHI.GetCategoryName(), Verbosity,
				TEXT("[%s][%s][%s][%u] %s"),
				SourceStr,
				TypeStr,
				SeverityStr,
				Id,
				ANSI_TO_TCHAR(Message)
				);
		}

		// this is a debugging code to catch VIDEO->HOST copying
		if (Id == 131186)
		{
			int A = 5;
		}
	}
#endif
}

#endif // GL_ARB_debug_output

#ifdef GL_AMD_debug_output

/**
 * Map GL_DEBUG_CATEGORY_*_AMD to a human-readable string.
 */
static const TCHAR* GetOpenGLDebugCategoryStringAMD(GLenum Category)
{
	static const TCHAR* CategoryStrings[] =
	{
		TEXT("API"),
		TEXT("System"),
		TEXT("Deprecation"),
		TEXT("UndefinedBehavior"),
		TEXT("Performance"),
		TEXT("ShaderCompiler"),
		TEXT("Application"),
		TEXT("Other")
	};

	if (Category >= GL_DEBUG_CATEGORY_API_ERROR_AMD && Category <= GL_DEBUG_CATEGORY_OTHER_AMD)
	{
		return CategoryStrings[Category - GL_DEBUG_CATEGORY_API_ERROR_AMD];
	}
	return TEXT("Unknown");
}

/**
 * Map GL_DEBUG_SEVERITY_*_AMD to a human-readable string.
 */
static const TCHAR* GetOpenGLDebugSeverityStringAMD(GLenum Severity)
{
	static const TCHAR* SeverityStrings[] =
	{
		TEXT("High"),
		TEXT("Medium"),
		TEXT("Low")
	};

	if (Severity >= GL_DEBUG_SEVERITY_HIGH_AMD && Severity <= GL_DEBUG_SEVERITY_LOW_AMD)
	{
		return SeverityStrings[Severity - GL_DEBUG_SEVERITY_HIGH_AMD];
	}
	return TEXT("Unknown");
}

/**
 * OpenGL debug message callback. Conforms to GLDEBUGPROCAMD.
 */
static void APIENTRY OpenGLDebugMessageCallbackAMD(
	GLuint Id,
	GLenum Category,
	GLenum Severity,
	GLsizei Length,
	const GLchar* Message,
	GLvoid* UserParam)
{
#if !NO_LOGGING
	const TCHAR* CategoryStr = GetOpenGLDebugCategoryStringAMD(Category);
	const TCHAR* SeverityStr = GetOpenGLDebugSeverityStringAMD(Severity);

	ELogVerbosity::Type Verbosity = ELogVerbosity::Warning;
	if (Severity == GL_DEBUG_SEVERITY_HIGH_AMD)
	{
		Verbosity = ELogVerbosity::Fatal;
	}

	if ((Verbosity & ELogVerbosity::VerbosityMask) <= FLogCategoryLogRHI::CompileTimeVerbosity)
	{
		if (!LogRHI.IsSuppressed(Verbosity))
		{
			FMsg::Logf(__FILE__, __LINE__, LogRHI.GetCategoryName(), Verbosity,
				TEXT("[%s][%s][%u] %s"),
				CategoryStr,
				SeverityStr,
				Id,
				ANSI_TO_TCHAR(Message)
				);
		}
	}
#endif
}

#endif // GL_AMD_debug_output

#if PLATFORM_WINDOWS
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT_ProcAddress = NULL;
#endif


static inline void SetupTextureFormat( EPixelFormat Format, const FOpenGLTextureFormat& GLFormat)
{
	GOpenGLTextureFormats[Format] = GLFormat;
	GPixelFormats[Format].Supported = (GLFormat.Format != GL_NONE && (GLFormat.InternalFormat[0] != GL_NONE || GLFormat.InternalFormat[1] != GL_NONE));
	GPixelFormats[Format].PlatformFormat = GLFormat.InternalFormat[0];
}


void InitDebugContext()
{
	// Set the debug output callback if the driver supports it.
	VERIFY_GL(__FUNCTION__);
	bool bDebugOutputInitialized = false;
#if !ENABLE_VERIFY_GL
	#if defined(GL_ARB_debug_output)
		if (glDebugMessageCallbackARB)
		{
			// Synchronous output can slow things down, but we'll get better callstack if breaking in or crashing in the callback. This is debug only after all.
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallbackARB(GLDEBUGPROCARB(OpenGLDebugMessageCallbackARB), /*UserParam=*/ NULL);
			bDebugOutputInitialized = (glGetError() == GL_NO_ERROR);
		}
	#elif defined(GL_KHR_debug)
		// OpenGLES names the debug functions differently, but they behave the same
		if (glDebugMessageCallbackKHR)
		{
			glDebugMessageCallbackKHR(GLDEBUGPROCKHR(OpenGLDebugMessageCallbackARB), /*UserParam=*/ NULL);
			bDebugOutputInitialized = (glGetError() == GL_NO_ERROR);
		}
	#endif // GL_ARB_debug_output / GL_KHR_debug
	#if defined(GL_AMD_debug_output)
		if (glDebugMessageCallbackAMD && !bDebugOutputInitialized)
		{
			glDebugMessageCallbackAMD(OpenGLDebugMessageCallbackAMD, /*UserParam=*/ NULL);
			bDebugOutputInitialized = (glGetError() == GL_NO_ERROR);
		}
	#endif // GL_AMD_debug_output
#endif // !ENABLE_VERIFY_GL

	if (!bDebugOutputInitialized)
	{
		UE_LOG(LogRHI,Warning,TEXT("OpenGL debug output extension not supported!"));
	}

	// this is to suppress feeding back of the debug markers and groups to the log, since those originate in the app anyways...
#if ENABLE_OPENGL_DEBUG_GROUPS && defined(GL_ARB_debug_output) && GL_ARB_debug_output && GL_KHR_debug
	if(glDebugMessageControlARB && bDebugOutputInitialized)
	{
		glDebugMessageControlARB(GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_MARKER, GL_DONT_CARE, 0, NULL, GL_FALSE);
		glDebugMessageControlARB(GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_PUSH_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
		glDebugMessageControlARB(GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_POP_GROUP, GL_DONT_CARE, 0, NULL, GL_FALSE);
#ifdef GL_KHR_debug
		glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
#endif
		UE_LOG(LogRHI,Verbose,TEXT("disabling reporting back of debug groups and markers to the OpenGL debug output callback"));
	}
#elif ENABLE_OPENGL_DEBUG_GROUPS && !defined(GL_ARB_debug_output) && defined(GL_KHR_debug) && GL_KHR_debug
	if(glDebugMessageControlKHR)
	{
		glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_MARKER_KHR, GL_DONT_CARE, 0, NULL, GL_FALSE);
		glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_PUSH_GROUP_KHR, GL_DONT_CARE, 0, NULL, GL_FALSE);
		glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_POP_GROUP_KHR, GL_DONT_CARE, 0, NULL, GL_FALSE);
		glDebugMessageControlKHR(GL_DEBUG_SOURCE_API_KHR, GL_DEBUG_TYPE_OTHER_KHR, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		UE_LOG(LogRHI,Verbose,TEXT("disabling reporting back of debug groups and markers to the OpenGL debug output callback"));
	}
#endif
}

TAutoConsoleVariable<FString> CVarOpenGLStripExtensions(
	TEXT("r.OpenGL.StripExtensions"),
	TEXT(""),
	TEXT("List of comma separated OpenGL extensions to strip from a driver reported extensions string"),
	ECVF_ReadOnly);

TAutoConsoleVariable<FString> CVarOpenGLAddExtensions(
	TEXT("r.OpenGL.AddExtensions"),
	TEXT(""),
	TEXT("List of comma separated OpenGL extensions to add to a driver reported extensions string"),
	ECVF_ReadOnly);

void ApplyExtensionsOverrides(FString& ExtensionsString)
{
	// Strip extensions
	{
		TArray<FString> ExtList;
		FString ExtString = CVarOpenGLStripExtensions.GetValueOnAnyThread();
		ExtString.ParseIntoArray(ExtList, TEXT(","), /*InCullEmpty=*/true);

		for (FString& ExtName : ExtList)
		{
			ExtName.TrimStartAndEndInline();
			if (ExtensionsString.ReplaceInline(*ExtName, TEXT("")) > 0)
			{
				UE_LOG(LogRHI, Log, TEXT("Stripped extension: %s"), *ExtName);
			}
		}
	}

	// Add extensions
	{
		TArray<FString> ExtList;
		FString ExtString = CVarOpenGLAddExtensions.GetValueOnAnyThread();
		ExtString.ParseIntoArray(ExtList, TEXT(","), /*InCullEmpty=*/true);

		for (FString& ExtName : ExtList)
		{
			ExtName.TrimStartAndEndInline();
			if (!ExtensionsString.Contains(ExtName))
			{
				ExtensionsString.Append(TEXT(" ")); // extensions delimiter
				ExtensionsString.Append(ExtName);
				UE_LOG(LogRHI, Log, TEXT("Added extension: %s"), *ExtName);
			}
		}
	}
}


/**
 * Initialize RHI capabilities for the current OpenGL context.
 */
static void InitRHICapabilitiesForGL()
{
	VERIFY_GL_SCOPE();

	GTexturePoolSize = 0;
	GPoolSizeVRAMPercentage = 0;
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	GConfig->GetInt( TEXT( "TextureStreaming" ), TEXT( "PoolSizeVRAMPercentage" ), GPoolSizeVRAMPercentage, GEngineIni );
#endif

	// GL vendor and version information.
#if !defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !(defined(_MSC_VER) && _MSC_VER >= 1900)
	#define LOG_GL_STRING(StringEnum) UE_LOG(LogRHI, Log, TEXT("  ") ## TEXT(#StringEnum) ## TEXT(": %s"), ANSI_TO_TCHAR((const ANSICHAR*)glGetString(StringEnum)))
#else
	#define LOG_GL_STRING(StringEnum) UE_LOG(LogRHI, Log, TEXT("  " #StringEnum ": %s"), ANSI_TO_TCHAR((const ANSICHAR*)glGetString(StringEnum)))
#endif
	UE_LOG(LogRHI, Log, TEXT("Initializing OpenGL RHI"));
	LOG_GL_STRING(GL_VENDOR);
	LOG_GL_STRING(GL_RENDERER);
	LOG_GL_STRING(GL_VERSION);
	LOG_GL_STRING(GL_SHADING_LANGUAGE_VERSION);
	#undef LOG_GL_STRING

	GRHIAdapterName = FOpenGL::GetAdapterName();
	GRHIAdapterInternalDriverVersion = ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION));

	// Shader platform & RHI feature level
	GMaxRHIFeatureLevel = FOpenGL::GetFeatureLevel();
	GMaxRHIShaderPlatform = FOpenGL::GetShaderPlatform();

	// Log all supported extensions.
#if PLATFORM_WINDOWS
	bool bWindowsSwapControlExtensionPresent = false;
#endif
	{
		extern void GetExtensionsString( FString& ExtensionsString);
		FString ExtensionsString;

		GetExtensionsString(ExtensionsString);

#if PLATFORM_WINDOWS
		if (ExtensionsString.Contains(TEXT("WGL_EXT_swap_control")))
		{
			bWindowsSwapControlExtensionPresent = true;
		}
#endif

		// Log supported GL extensions
		UE_LOG(LogRHI, Log, TEXT("OpenGL Extensions:"));
		TArray<FString> GLExtensionArray;
		ExtensionsString.ParseIntoArray(GLExtensionArray, TEXT(" "), true);
		for (int ExtIndex = 0; ExtIndex < GLExtensionArray.Num(); ExtIndex++)
		{
			UE_LOG(LogRHI, Log, TEXT("  %s"), *GLExtensionArray[ExtIndex]);
		}

		ApplyExtensionsOverrides(ExtensionsString);

		FOpenGL::ProcessExtensions(ExtensionsString);
	}

#if PLATFORM_WINDOWS
	#pragma warning(push)
	#pragma warning(disable:4191)
	if (!bWindowsSwapControlExtensionPresent)
	{
		// Disable warning C4191: 'type cast' : unsafe conversion from 'PROC' to 'XXX' while getting GL entry points.
		PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtensionsStringEXT_ProcAddress =
			(PFNWGLGETEXTENSIONSSTRINGEXTPROC) wglGetProcAddress("wglGetExtensionsStringEXT");

		if (strstr(wglGetExtensionsStringEXT_ProcAddress(), "WGL_EXT_swap_control") != NULL)
		{
			bWindowsSwapControlExtensionPresent = true;
		}
	}

	if (bWindowsSwapControlExtensionPresent)
	{
		wglSwapIntervalEXT_ProcAddress = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
	}
	#pragma warning(pop)
#endif

	// Set debug flag if context was setup with debugging
	FOpenGL::InitDebugContext();

	// Log and get various limits.
#if !defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !(defined(_MSC_VER) && _MSC_VER >= 1900)
#define LOG_AND_GET_GL_INT_TEMP(IntEnum,Default) GLint Value_##IntEnum = Default; if (IntEnum) {glGetIntegerv(IntEnum, &Value_##IntEnum); glGetError();} else {Value_##IntEnum = Default;} UE_LOG(LogRHI, Log, TEXT("  ") ## TEXT(#IntEnum) ## TEXT(": %d"), Value_##IntEnum)
#else
#define LOG_AND_GET_GL_INT_TEMP(IntEnum,Default) GLint Value_##IntEnum = Default; if (IntEnum) {glGetIntegerv(IntEnum, &Value_##IntEnum); glGetError();} else {Value_##IntEnum = Default;} UE_LOG(LogRHI, Log, TEXT("  " #IntEnum ": %d"), Value_##IntEnum)
#endif
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_TEXTURE_SIZE, 0);
#if defined(GL_MAX_TEXTURE_BUFFER_SIZE)
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_TEXTURE_BUFFER_SIZE, 0);
#endif
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_CUBE_MAP_TEXTURE_SIZE, 0);
#if defined(GL_MAX_ARRAY_TEXTURE_LAYERS) && GL_MAX_ARRAY_TEXTURE_LAYERS
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_ARRAY_TEXTURE_LAYERS, 0);
#endif
#if GL_MAX_3D_TEXTURE_SIZE
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_3D_TEXTURE_SIZE, 0);
#endif
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_RENDERBUFFER_SIZE, 0);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_TEXTURE_IMAGE_UNITS, 0);

	LOG_AND_GET_GL_INT_TEMP(GL_MAX_DRAW_BUFFERS, 1);
	GMaxOpenGLDrawBuffers = FMath::Min(Value_GL_MAX_DRAW_BUFFERS, (GLint)MaxSimultaneousRenderTargets);

	LOG_AND_GET_GL_INT_TEMP(GL_MAX_COLOR_ATTACHMENTS, 1);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_SAMPLES, 1);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_COLOR_TEXTURE_SAMPLES, 1);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_DEPTH_TEXTURE_SAMPLES, 1);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_INTEGER_SAMPLES, 1);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 0);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_VERTEX_ATTRIBS, 0);

	LOG_AND_GET_GL_INT_TEMP(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, 0);
	LOG_AND_GET_GL_INT_TEMP(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, 0);
	

	if (FParse::Param(FCommandLine::Get(), TEXT("quad_buffer_stereo")))
	{
		GLboolean Result = GL_FALSE;
		glGetBooleanv(GL_STEREO, &Result);
		// Skip any errors if any were generated
		glGetError();
		GSupportsQuadBufferStereo = (Result == GL_TRUE);
	}

	if (FOpenGL::SupportsTextureFilterAnisotropic())
	{
		LOG_AND_GET_GL_INT_TEMP(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, 0);
		GMaxOpenGLTextureFilterAnisotropic = Value_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT;
	}

	if (FOpenGL::SupportsPixelLocalStorage())
	{
		LOG_AND_GET_GL_INT_TEMP(GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_SIZE_EXT, 0);
	}
#undef LOG_AND_GET_GL_INT_TEMP

	GMaxOpenGLColorSamples = Value_GL_MAX_COLOR_TEXTURE_SAMPLES;
	GMaxOpenGLDepthSamples = Value_GL_MAX_DEPTH_TEXTURE_SAMPLES;
	GMaxOpenGLIntegerSamples = Value_GL_MAX_INTEGER_SAMPLES;

	// Verify some assumptions.
	// Android seems like reports one color attachment even when it supports MRT
#if !PLATFORM_ANDROID
	check(Value_GL_MAX_COLOR_ATTACHMENTS >= MaxSimultaneousRenderTargets);
#endif

	// We don't check for compressed formats right now because vendors have not
	// done a great job reporting what is actually supported:
	//   OSX/NVIDIA doesn't claim to support SRGB DXT formats even though they work correctly.
	//   Windows/AMD sometimes claim to support no compressed formats even though they all work correctly.
#if 0
	// Check compressed texture formats.
	bool bSupportsCompressedTextures = true;
	{
		FString CompressedFormatsString = TEXT("  GL_COMPRESSED_TEXTURE_FORMATS:");
		TArray<GLint> CompressedFormats;
		GLint NumCompressedFormats = 0;
		glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &NumCompressedFormats);
		CompressedFormats.Empty(NumCompressedFormats);
		CompressedFormats.AddZeroed(NumCompressedFormats);
		glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, CompressedFormats.GetTypedData());

		#define CHECK_COMPRESSED_FORMAT(GLName) if (CompressedFormats.Contains(GLName)) { CompressedFormatsString += TEXT(" ") TEXT(#GLName); } else { bSupportsCompressedTextures = false; }
		CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
		CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
		CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
		CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT);
		CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
		CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
		//CHECK_COMPRESSED_FORMAT(GL_COMPRESSED_RG_RGTC2);
		#undef CHECK_COMPRESSED_FORMAT

		// ATI does not report that it supports RGTC2, but the 3.2 core profile mandates it.
		// For now assume it is supported and bring it up with ATI if it really isn't(?!)
		CompressedFormatsString += TEXT(" GL_COMPRESSED_RG_RGTC2");

		UE_LOG(LogRHI, Log, *CompressedFormatsString);
	}
	check(bSupportsCompressedTextures);
#endif

	// Set capabilities.
	const GLint MajorVersion = FOpenGL::GetMajorVersion();
	const GLint MinorVersion = FOpenGL::GetMinorVersion();
 
	// Enable the OGL rhi thread if explicitly requested.
	GRHISupportsRHIThread = (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && CVarAllowRGLHIThread.GetValueOnAnyThread())
#if WITH_EDITOR
		&& !IsPCPlatform(GMaxRHIShaderPlatform)
#endif
		;

	GRHISupportsMultithreadedResources = GRHISupportsRHIThread;
	
	// By default use emulated UBs on mobile
	GUseEmulatedUniformBuffers = IsUsingEmulatedUniformBuffers(GMaxRHIShaderPlatform);

	FString FeatureLevelName;
	GetFeatureLevelName(GMaxRHIFeatureLevel, FeatureLevelName);
	FString ShaderPlatformName = LegacyShaderPlatformToShaderFormat(GMaxRHIShaderPlatform).ToString();

	UE_LOG(LogRHI, Log, TEXT("OpenGL MajorVersion = %d, MinorVersion = %d, ShaderPlatform = %s, FeatureLevel = %s"), MajorVersion, MinorVersion, *ShaderPlatformName, *FeatureLevelName);
#if PLATFORM_ANDROID
	UE_LOG(LogRHI, Log, TEXT("PLATFORM_ANDROID"));
#endif

	GMaxTextureSamplers = Value_GL_MAX_TEXTURE_IMAGE_UNITS;
	GMaxTextureMipCount = FMath::CeilLogTwo(Value_GL_MAX_TEXTURE_SIZE) + 1;
	GMaxTextureMipCount = FMath::Min<int32>(MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount);
	GMaxTextureDimensions = Value_GL_MAX_TEXTURE_SIZE;
	
#if defined(GL_MAX_TEXTURE_BUFFER_SIZE)
	GMaxBufferDimensions = Value_GL_MAX_TEXTURE_BUFFER_SIZE;
#endif

	GMaxCubeTextureDimensions = Value_GL_MAX_CUBE_MAP_TEXTURE_SIZE;
#if defined(GL_MAX_ARRAY_TEXTURE_LAYERS) && GL_MAX_ARRAY_TEXTURE_LAYERS
	GMaxTextureArrayLayers = Value_GL_MAX_ARRAY_TEXTURE_LAYERS;
#endif

#if defined(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE)
	GMaxComputeSharedMemory = Value_GL_MAX_COMPUTE_SHARED_MEMORY_SIZE;
#endif

	GMaxWorkGroupInvocations = Value_GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS;

	GSupportsParallelRenderingTasksWithSeparateRHIThread = false; // hopefully this is just temporary
	GRHIThreadNeedsKicking = true; // GL is SLOW
	GRHISupportsExactOcclusionQueries = FOpenGL::SupportsExactOcclusionQueries();

	GSupportsVolumeTextureRendering = FOpenGL::SupportsVolumeTextureRendering();
	GSupportsRenderDepthTargetableShaderResources = true;
	GSupportsRenderTargetFormat_PF_G8 = true;
	GSupportsSeparateRenderTargetBlendState = FOpenGL::SupportsSeparateAlphaBlend();
	GSupportsDepthBoundsTest = FOpenGL::SupportsDepthBoundsTest();

	GSupportsRenderTargetFormat_PF_FloatRGBA = FOpenGL::SupportsColorBufferHalfFloat();

	GSupportsWideMRT = FOpenGL::SupportsWideMRT();
	GSupportsTexture3D = FOpenGL::SupportsTexture3D();
	GSupportsMobileMultiView = FOpenGL::SupportsMobileMultiView();
	GSupportsImageExternal = FOpenGL::SupportsImageExternal();

	GSupportsShaderFramebufferFetch = FOpenGL::SupportsShaderFramebufferFetch();
	GSupportsShaderMRTFramebufferFetch = FOpenGL::SupportsShaderMRTFramebufferFetch();
	GSupportsShaderDepthStencilFetch = FOpenGL::SupportsShaderDepthStencilFetch();
	GSupportsPixelLocalStorage = FOpenGL::SupportsPixelLocalStorage();

	GMaxShadowDepthBufferSizeX = FMath::Min<int32>(Value_GL_MAX_RENDERBUFFER_SIZE, 4096); // Limit to the D3D11 max.
	GMaxShadowDepthBufferSizeY = FMath::Min<int32>(Value_GL_MAX_RENDERBUFFER_SIZE, 4096);
	GHardwareHiddenSurfaceRemoval = FOpenGL::HasHardwareHiddenSurfaceRemoval();
	GSupportsTimestampRenderQueries = FOpenGL::SupportsTimestampQueries();

	GRHIMaxDispatchThreadGroupsPerDimension.X = MAX_uint16;
	GRHIMaxDispatchThreadGroupsPerDimension.Y = MAX_uint16;
	GRHIMaxDispatchThreadGroupsPerDimension.Z = MAX_uint16;

	// It's not possible to create a framebuffer with the backbuffer as the color attachment and a custom renderbuffer as the depth/stencil surface.
	GRHISupportsBackBufferWithCustomDepthStencil = false;

	GRHISupportsLazyShaderCodeLoading = true;

	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1) ? GMaxRHIShaderPlatform : SP_OPENGL_PCES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4_REMOVED] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_NumPlatforms;

	// Disable texture streaming on devices with ES3.1 or lower, unless we have the GL_APPLE_copy_texture_levels extension or support for glCopyImageSubData
	if (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		GRHISupportsTextureStreaming = FOpenGL::SupportsCopyImage();
	}
	else
	{
		GRHISupportsTextureStreaming = true;
	}

	// Disable texture streaming if forced off by r.OpenGL.DisableTextureStreamingSupport
	static const auto CVarDisableOpenGLTextureStreamingSupport = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OpenGL.DisableTextureStreamingSupport"));
	if (CVarDisableOpenGLTextureStreamingSupport->GetValueOnAnyThread())
	{
		GRHISupportsTextureStreaming = false;
	}

	for (int32 PF = 0; PF < PF_MAX; ++PF)
	{
		SetupTextureFormat(EPixelFormat(PF), FOpenGLTextureFormat());
	}

	GLenum DepthFormat = FOpenGL::GetDepthFormat();
	GLenum ShadowDepthFormat = FOpenGL::GetShadowDepthFormat();

	// Initialize the platform pixel format map.					InternalFormat				InternalFormatSRGB		Format				Type							bCompressed		bBGRA
	SetupTextureFormat( PF_Unknown,				FOpenGLTextureFormat( ));
	SetupTextureFormat( PF_A32B32G32R32F,		FOpenGLTextureFormat( GL_RGBA32F,				GL_RGBA32F,				GL_RGBA,			GL_FLOAT,						false,			false));
	SetupTextureFormat( PF_UYVY,				FOpenGLTextureFormat( ));
	SetupTextureFormat( PF_DepthStencil,		FOpenGLTextureFormat( GL_DEPTH24_STENCIL8,		GL_NONE,				GL_DEPTH_STENCIL,	GL_UNSIGNED_INT_24_8,			false,			false));
	SetupTextureFormat( PF_ShadowDepth,			FOpenGLTextureFormat( ShadowDepthFormat,		ShadowDepthFormat,		GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT,				false,			false));
	SetupTextureFormat( PF_D24,					FOpenGLTextureFormat( DepthFormat,				DepthFormat,			GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT,				false,			false));
	SetupTextureFormat( PF_A16B16G16R16,		FOpenGLTextureFormat( GL_RGBA16F,				GL_RGBA16F,				GL_RGBA,			GL_HALF_FLOAT,					false,			false));
	SetupTextureFormat( PF_A1,					FOpenGLTextureFormat( ));
	SetupTextureFormat( PF_R16G16B16A16_UINT,	FOpenGLTextureFormat( GL_RGBA16UI,				GL_RGBA16UI,			GL_RGBA_INTEGER,	GL_UNSIGNED_SHORT,				false,			false));
	SetupTextureFormat( PF_R16G16B16A16_SINT,	FOpenGLTextureFormat( GL_RGBA16I,				GL_RGBA16I,				GL_RGBA_INTEGER,	GL_SHORT,						false,			false));
	SetupTextureFormat( PF_R32G32B32A32_UINT,	FOpenGLTextureFormat( GL_RGBA32UI,				GL_RGBA32UI,			GL_RGBA_INTEGER,	GL_UNSIGNED_INT,				false,			false));
	SetupTextureFormat( PF_R16G16B16A16_UNORM,	FOpenGLTextureFormat( GL_RGBA16,				GL_RGBA16,				GL_RGBA,			GL_UNSIGNED_SHORT,				false,			false));
	SetupTextureFormat( PF_R16G16B16A16_SNORM,	FOpenGLTextureFormat( GL_RGBA16,				GL_RGBA16,				GL_RGBA,			GL_SHORT,						false,			false));
	
	SetupTextureFormat( PF_R16G16_UINT,			FOpenGLTextureFormat( GL_RG16UI,				GL_RG16UI,				GL_RG_INTEGER,		GL_UNSIGNED_SHORT,				false,			false));
	SetupTextureFormat( PF_R8,					FOpenGLTextureFormat( GL_R8,					GL_R8,					GL_RED,				GL_UNSIGNED_BYTE,				false,			false));

	SetupTextureFormat( PF_R5G6B5_UNORM,        FOpenGLTextureFormat( GL_RGB565,                GL_RGB565,              GL_RGB,             GL_UNSIGNED_SHORT_5_6_5,        false,          false));

	//GL_UNSIGNED_SHORT_1_5_5_5_REV is not defined in gles
	GLenum GL5551Format = FOpenGL::GetPlatfrom5551Format();
	//the last 5 bits of GL_UNSIGNED_SHORT_1_5_5_5_REV is Red, while the last 5 bits of DXGI_FORMAT_B5G5R5A1_UNORM is Blue
	bool bNeedsToSwizzleRedBlue = GL5551Format != GL_UNSIGNED_SHORT_5_5_5_1;
	SetupTextureFormat( PF_B5G5R5A1_UNORM,		FOpenGLTextureFormat( GL_RGB5_A1,               GL_RGB5_A1,             GL_RGBA,			GL5551Format,					false,          bNeedsToSwizzleRedBlue));


	SetupTextureFormat( PF_G16,					FOpenGLTextureFormat( GL_R16,					GL_R16,					GL_RED,				GL_UNSIGNED_SHORT,					false,	false));
	SetupTextureFormat( PF_R32_FLOAT,			FOpenGLTextureFormat( GL_R32F,					GL_R32F,				GL_RED,				GL_FLOAT,							false,	false));
	SetupTextureFormat( PF_G16R16F,				FOpenGLTextureFormat( GL_RG16F,					GL_RG16F,				GL_RG,				GL_HALF_FLOAT,						false,	false));
	SetupTextureFormat( PF_G16R16F_FILTER,		FOpenGLTextureFormat( GL_RG16F,					GL_RG16F,				GL_RG,				GL_HALF_FLOAT,						false,	false));
	SetupTextureFormat( PF_G32R32F,				FOpenGLTextureFormat( GL_RG32F,					GL_RG32F,				GL_RG,				GL_FLOAT,							false,	false));
	SetupTextureFormat( PF_A2B10G10R10,			FOpenGLTextureFormat( GL_RGB10_A2,				GL_RGB10_A2,			GL_RGBA,			GL_UNSIGNED_INT_2_10_10_10_REV,		false,	false));
	SetupTextureFormat( PF_R16F,				FOpenGLTextureFormat( GL_R16F,					GL_R16F,				GL_RED,				GL_HALF_FLOAT,						false,	false));
	SetupTextureFormat( PF_R16F_FILTER,			FOpenGLTextureFormat( GL_R16F,					GL_R16F,				GL_RED,				GL_HALF_FLOAT,						false,	false));
	SetupTextureFormat( PF_A8,					FOpenGLTextureFormat( GL_R8,					GL_R8,					GL_RED,				GL_UNSIGNED_BYTE,					false,	false));
	SetupTextureFormat( PF_R32_UINT,			FOpenGLTextureFormat( GL_R32UI,					GL_R32UI,				GL_RED_INTEGER,		GL_UNSIGNED_INT,					false,	false));
	SetupTextureFormat( PF_R32_SINT,			FOpenGLTextureFormat( GL_R32I,					GL_R32I,				GL_RED_INTEGER,		GL_INT,								false,	false));
	SetupTextureFormat( PF_R16_UINT,			FOpenGLTextureFormat( GL_R16UI,					GL_R16UI,				GL_RED_INTEGER,		GL_UNSIGNED_SHORT,					false,	false));
	SetupTextureFormat( PF_R16_SINT,			FOpenGLTextureFormat( GL_R16I,					GL_R16I,				GL_RED_INTEGER,		GL_SHORT,							false,	false));
	SetupTextureFormat( PF_R8_UINT,				FOpenGLTextureFormat( GL_R8UI,					GL_R8UI,				GL_RED_INTEGER,		GL_UNSIGNED_BYTE,					false,  false));
	SetupTextureFormat( PF_R8G8,				FOpenGLTextureFormat( GL_RG8,					GL_RG8,					GL_RG,				GL_UNSIGNED_BYTE,					false,	false));
	SetupTextureFormat( PF_R32G32_UINT,			FOpenGLTextureFormat( GL_RG32UI,				GL_RG32UI,				GL_RG_INTEGER,		GL_UNSIGNED_INT,					false,	false));
	// Should be GL_RGBA8_SNORM but it doesn't work with glTexBuffer -> mapping to Unorm and unpacking in the shader
	SetupTextureFormat( PF_R8G8B8A8_SNORM,		FOpenGLTextureFormat( GL_RGBA8,					GL_RGBA8,				GL_RGBA,			GL_UNSIGNED_BYTE,					false, false));
	
	SetupTextureFormat( PF_FloatRGB,			FOpenGLTextureFormat( GL_R11F_G11F_B10F,		GL_R11F_G11F_B10F,		GL_RGB,				GL_UNSIGNED_INT_10F_11F_11F_REV,	false,  false));
	SetupTextureFormat( PF_FloatR11G11B10,		FOpenGLTextureFormat( GL_R11F_G11F_B10F,		GL_R11F_G11F_B10F,		GL_RGB,				GL_UNSIGNED_INT_10F_11F_11F_REV,	false,  false));
	SetupTextureFormat( PF_FloatRGBA,			FOpenGLTextureFormat( GL_RGBA16F,				GL_RGBA16F,				GL_RGBA,			GL_HALF_FLOAT,						false,	false));

	SetupTextureFormat( PF_R8G8_UINT,			FOpenGLTextureFormat( GL_RG8UI,					GL_RG8UI,				GL_RG_INTEGER,		GL_UNSIGNED_BYTE,					false, false));
	SetupTextureFormat( PF_R32G32B32_UINT,		FOpenGLTextureFormat( GL_RGB32UI,				GL_RGB32UI,				GL_RGB_INTEGER,		GL_UNSIGNED_INT,					false, false));
	SetupTextureFormat( PF_R32G32B32_SINT,		FOpenGLTextureFormat( GL_RGB32I,				GL_RGB32I,				GL_RGB_INTEGER,		GL_INT,								false, false));
	SetupTextureFormat( PF_R32G32B32F,			FOpenGLTextureFormat( GL_RGB32F,				GL_RGB32F,				GL_RGB,				GL_FLOAT,							false, false));
	SetupTextureFormat( PF_R8_SINT,				FOpenGLTextureFormat( GL_R8I,					GL_R8I,					GL_RED_INTEGER,		GL_BYTE,							false, false));

	SetupTextureFormat( PF_B8G8R8A8,			FOpenGLTextureFormat( GL_RGBA8,					GL_SRGB8_ALPHA8,		GL_RGBA,			GL_UNSIGNED_BYTE,					false, true));
	SetupTextureFormat( PF_R8G8B8A8,			FOpenGLTextureFormat( GL_RGBA8,					GL_SRGB8_ALPHA8,		GL_RGBA,			GL_UNSIGNED_BYTE,					false, false));
	SetupTextureFormat( PF_R8G8B8A8_UINT,		FOpenGLTextureFormat( GL_RGBA8UI,				GL_RGBA8UI,				GL_RGBA_INTEGER,	GL_UNSIGNED_BYTE,					false, false));
	SetupTextureFormat( PF_G8,					FOpenGLTextureFormat( GL_R8,					GL_R8,					GL_RED,				GL_UNSIGNED_BYTE,					false, false));

#if PLATFORM_DESKTOP
	CA_SUPPRESS(6286);
	if (PLATFORM_DESKTOP || FOpenGL::GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		SetupTextureFormat( PF_V8U8,			FOpenGLTextureFormat( GL_RG8_SNORM,				GL_NONE,				GL_RG,			GL_BYTE,							false, false));
		SetupTextureFormat( PF_BC5,				FOpenGLTextureFormat( GL_COMPRESSED_RG_RGTC2,	GL_COMPRESSED_RG_RGTC2,	GL_RG,			GL_UNSIGNED_BYTE,					true,	false));
		SetupTextureFormat( PF_BC4,				FOpenGLTextureFormat( GL_COMPRESSED_RED_RGTC1,	GL_COMPRESSED_RED_RGTC1,GL_RED,			GL_UNSIGNED_BYTE,					true,	false));

		SetupTextureFormat( PF_G16R16,			FOpenGLTextureFormat(GL_RG16, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, false, false));
		SetupTextureFormat( PF_G16R16_SNORM,	FOpenGLTextureFormat(GL_RG16_SNORM, GL_RG16_SNORM, GL_RG, GL_SHORT, false, false));
	}
	else
#endif // PLATFORM_DESKTOP
	{
#if !PLATFORM_DESKTOP
		FOpenGL::PE_SetupTextureFormat(&SetupTextureFormat); // platform extension
#endif // !PLATFORM_DESKTOP
	}

	if (FOpenGL::SupportsDXT())
	{
		SetupTextureFormat( PF_DXT1,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,	GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false));
		SetupTextureFormat( PF_DXT3,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,	GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false));
		SetupTextureFormat( PF_DXT5,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,	GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false));
	}
#if PLATFORM_ANDROID
	if ( FOpenGL::SupportsETC2() )
	{
		SetupTextureFormat( PF_ETC2_RGB,		FOpenGLTextureFormat(GL_COMPRESSED_RGB8_ETC2,		GL_COMPRESSED_SRGB8_ETC2,					GL_RGBA,		GL_UNSIGNED_BYTE,	true,		false));
		SetupTextureFormat( PF_ETC2_RGBA,		FOpenGLTextureFormat(GL_COMPRESSED_RGBA8_ETC2_EAC,	GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,		GL_RGBA,		GL_UNSIGNED_BYTE,	true,		false));
		SetupTextureFormat( PF_ETC2_R11_EAC,	FOpenGLTextureFormat(GL_COMPRESSED_R11_EAC,			GL_COMPRESSED_R11_EAC,						GL_RED,			GL_UNSIGNED_BYTE,	true,		false));
		SetupTextureFormat( PF_ETC2_RG11_EAC,	FOpenGLTextureFormat(GL_COMPRESSED_RG11_EAC,		GL_COMPRESSED_RG11_EAC,						GL_RG,			GL_UNSIGNED_BYTE,	true,		false));
	}
#endif
	if (FOpenGL::SupportsASTC())
	{
		SetupTextureFormat( PF_ASTC_4x4,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_4x4_KHR,	GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false) );
		SetupTextureFormat( PF_ASTC_6x6,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_6x6_KHR,	GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false) );
		SetupTextureFormat( PF_ASTC_8x8,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_8x8_KHR,	GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false) );
		SetupTextureFormat( PF_ASTC_10x10,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x10_KHR,	GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false) );
		SetupTextureFormat( PF_ASTC_12x12,	FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_12x12_KHR,	GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR,	GL_RGBA,	GL_UNSIGNED_BYTE,	true,	false) );
	}
	if (FOpenGL::SupportsASTCHDR())
	{
		SetupTextureFormat(PF_ASTC_4x4_HDR, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, GL_RGBA, GL_HALF_FLOAT, true, false));
		SetupTextureFormat(PF_ASTC_6x6_HDR, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, GL_RGBA, GL_HALF_FLOAT, true, false));
		SetupTextureFormat(PF_ASTC_8x8_HDR, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, GL_RGBA, GL_HALF_FLOAT, true, false));
		SetupTextureFormat(PF_ASTC_10x10_HDR, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x10_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, GL_RGBA, GL_HALF_FLOAT, true, false));
		SetupTextureFormat(PF_ASTC_12x12_HDR, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_12x12_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, GL_RGBA, GL_HALF_FLOAT, true, false));
	}
	// Some formats need to know how large a block is.
	GPixelFormats[ PF_DepthStencil		].BlockBytes	 = 4;
	GPixelFormats[ PF_FloatRGB			].BlockBytes	 = 4;
	GPixelFormats[ PF_FloatRGBA			].BlockBytes	 = 8;

	// Temporary fix for nvidia driver issue with non-power-of-two shadowmaps (9/8/2016) UE-35312
	// @TODO revisit this with newer drivers
	GRHINeedsUnatlasedCSMDepthsWorkaround = true;

	GRHISupportsPipelineFileCache = true;
}

FDynamicRHI* FOpenGLDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type InRequestedFeatureLevel)
{
	GRequestedFeatureLevel = InRequestedFeatureLevel;
	return new FOpenGLDynamicRHI();
}


FOpenGLDynamicRHI::FOpenGLDynamicRHI()
:	SceneFrameCounter(0)
,	ResourceTableFrameCounter(INDEX_NONE)
,	bRevertToSharedContextAfterDrawingViewport(false)
,	bIsRenderingContextAcquired(false)
,   BeginSceneContextType(CONTEXT_Other)
,	PlatformDevice(NULL)
,	GPUProfilingData(this)
,	NULLPixelShaderRHI(nullptr)
{
	check(Singleton == nullptr);
	Singleton = this;

	// This should be called once at the start
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

#if PLATFORM_ANDROID
	GRHINeedsExtraDeletionLatency = CVarGLExtraDeletionLatency.GetValueOnAnyThread() == 1;
#endif

	// Ogl must have all programs created on the context owning thread.
	// when GRHISupportsAsyncPipelinePrecompile is true pipelinefilecache will call RHICreateGraphicsPipelineState from any thread and
	// RHISetGraphicsPipelineState will not be called for precompiled programs.
	GRHISupportsAsyncPipelinePrecompile = false;

	PlatformInitOpenGL();
	PlatformDevice = PlatformCreateOpenGLDevice();
	VERIFY_GL_SCOPE();
	InitRHICapabilitiesForGL();

	check(PlatformOpenGLCurrentContext(PlatformDevice) == CONTEXT_Shared);

	if (PlatformCanEnableGPUCapture())
	{
		EnableIdealGPUCaptureOptions(true);

		// Disable persistent mapping
		{
			auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("OpenGL.UBODirectWrite"));
			if (CVar)
			{
				CVar->Set(false);
			}
		}
		{
			auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("OpenGL.UseStagingBuffer"));
			if (CVar)
			{
				CVar->Set(false);
			}
		}
	}

	{
		// Temp disable gpusorting for Opengl because of issues on Adreno and Mali devices
		auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("FX.AllowGPUSorting"));
		if (CVar)
		{
#if PLATFORM_ANDROID
			if(!AndroidThunkCpp_IsOculusMobileApplication())
#endif
			{
				CVar->Set(false);
			}
		}
	}

	PrivateOpenGLDevicePtr = this;
	GlobalUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
}

extern void DestroyShadersAndPrograms();

#if PLATFORM_ANDROID
extern bool GOpenGLShaderHackLastCompileSuccess;

// only used to test for shader compatibility issues
static bool VerifyCompiledShader(GLuint Shader, const ANSICHAR* GlslCode, bool IsFatal)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileVerifyTime);

	if (!GOpenGLShaderHackLastCompileSuccess)
	{
#if DEBUG_GL_SHADERS
		if (GlslCode)
		{
			UE_LOG(LogRHI, Warning, TEXT("Shader:\n%s"), ANSI_TO_TCHAR(GlslCode));

			const ANSICHAR *Temp = GlslCode;

			for (int i = 0; i < 30 && (*Temp != '\0'); ++i)
			{
				FString Converted = ANSI_TO_TCHAR(Temp);
				Converted = Converted.LeftChop(256);

				UE_LOG(LogRHI, Display, TEXT("%s"), *Converted);
				Temp += Converted.Len();
			}
		}
#endif
		UE_LOG(LogRHI, Warning, TEXT("Failed to compile shader."));

		return false;
	}
	return true;

}
#endif

void FOpenGLDynamicRHI::Init()
{
	check(!GIsRHIInitialized);
	VERIFY_GL_SCOPE();

	GRHISupportsMultithreadedShaderCreation = false;

	FOpenGLProgramBinaryCache::Initialize();
	RegisterSharedShaderCodeDelegates();
	InitializeStateResources();

	// Create a default point sampler state for internal use.
	FSamplerStateInitializerRHI PointSamplerStateParams(SF_Point,AM_Clamp,AM_Clamp,AM_Clamp);
	PointSamplerState = this->RHICreateSamplerState(PointSamplerStateParams);

#if PLATFORM_WINDOWS || PLATFORM_LINUX

	extern int64 GOpenGLDedicatedVideoMemory;
	extern int64 GOpenGLTotalGraphicsMemory;

	GOpenGLDedicatedVideoMemory = FOpenGL::GetVideoMemorySize();

	if ( GOpenGLDedicatedVideoMemory != 0)
	{
		GOpenGLTotalGraphicsMemory = GOpenGLDedicatedVideoMemory;

		if ( GPoolSizeVRAMPercentage > 0 )
		{
			float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(GOpenGLTotalGraphicsMemory);

			// Truncate GTexturePoolSize to MB (but still counted in bytes)
			GTexturePoolSize = int64(FGenericPlatformMath::TruncToInt(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;

			UE_LOG(LogRHI, Log, TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
				GTexturePoolSize / 1024 / 1024,
				GPoolSizeVRAMPercentage,
				GOpenGLTotalGraphicsMemory / 1024 / 1024);
		}
	}

#else

	static const auto CVarStreamingTexturePoolSize = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.PoolSize"));
	GTexturePoolSize = (int64)CVarStreamingTexturePoolSize->GetValueOnAnyThread() * 1024 * 1024;

	UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (of %llu MB total graphics mem)"),
			GTexturePoolSize / 1024 / 1024,
			FOpenGL::GetVideoMemorySize());

#endif

	// Flush here since we might be switching to a different context/thread for rendering
	FOpenGL::Flush();

	FHardwareInfo::RegisterHardwareInfo( NAME_RHI, TEXT( "OpenGL" ) );

	GRHICommandList.GetImmediateCommandList().InitializeImmediateContexts();

	FRenderResource::InitPreRHIResources();
	GIsRHIInitialized = true;
}

void FOpenGLDynamicRHI::PostInit()
{
	if (GRHISupportsRHIThread)
	{
		SetupRecursiveResources();
	}
}

void FOpenGLDynamicRHI::Shutdown()
{
	check(IsInGameThread() && IsInRenderingThread()); // require that the render thread has been shut down

	Cleanup();

	DestroyShadersAndPrograms();
	PlatformDestroyOpenGLDevice(PlatformDevice);

	UnregisterSharedShaderCodeDelegates();

	PrivateOpenGLDevicePtr = NULL;
}

void FOpenGLDynamicRHI::Cleanup()
{
	if(GIsRHIInitialized)
	{
		FOpenGLProgramBinaryCache::Shutdown();

		// Reset the RHI initialized flag.
		GIsRHIInitialized = false;

		GPUProfilingData.Cleanup();

		// Ask all initialized FRenderResources to release their RHI resources.
		FRenderResource::ReleaseRHIForAllResources();
	}

	// Release the point sampler state.
	PointSamplerState.SafeRelease();

	extern void EmptyGLSamplerStateCache();
	EmptyGLSamplerStateCache();

	// Release zero-filled dummy uniform buffer, if it exists.
	if (PendingState.ZeroFilledDummyUniformBuffer)
	{
		FOpenGL::DeleteBuffers(1, &PendingState.ZeroFilledDummyUniformBuffer);
		PendingState.ZeroFilledDummyUniformBuffer = 0;
		DecrementBufferMemory(GL_UNIFORM_BUFFER, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
	}

	// Release pending shader
	PendingState.BoundShaderState.SafeRelease();
	check(!IsValidRef(PendingState.BoundShaderState));

	VERIFY_GL_SCOPE();
	PendingState.CleanupResources();
	SharedContextState.CleanupResources();
	RenderingContextState.CleanupResources();
}

void FOpenGLDynamicRHI::RHIFlushResources()
{
	PlatformFlushIfNeeded();
}

void FOpenGLDynamicRHI::RHIAcquireThreadOwnership()
{
	check(!bRevertToSharedContextAfterDrawingViewport);	// if this is true, then main thread is rendering using our context right now.
	PlatformRenderingContextSetup(PlatformDevice);
	PlatformRebindResources(PlatformDevice);
	bIsRenderingContextAcquired = true;
	VERIFY_GL(RHIAcquireThreadOwnership);
	{
		FScopeLock lock(&CustomPresentSection);
		if (CustomPresent)
		{
			CustomPresent->OnAcquireThreadOwnership();
		}
	}
}

void FOpenGLDynamicRHI::RHIReleaseThreadOwnership()
{
	{
		FScopeLock lock(&CustomPresentSection);
		if (CustomPresent)
		{
			CustomPresent->OnReleaseThreadOwnership();
		}
	}
	VERIFY_GL(RHIReleaseThreadOwnership);
	bIsRenderingContextAcquired = false;
	PlatformNULLContextSetup();
}

void FOpenGLDynamicRHI::RegisterQuery( FOpenGLRenderQuery* Query )
{
	FScopeLock Lock(&QueriesListCriticalSection);
	Queries.Add(Query);
}

void FOpenGLDynamicRHI::UnregisterQuery( FOpenGLRenderQuery* Query )
{
	FScopeLock Lock(&QueriesListCriticalSection);
	Queries.RemoveSingleSwap(Query);
}

void* FOpenGLDynamicRHI::RHIGetNativeDevice()
{
	return PlatformDevice;
}

void* FOpenGLDynamicRHI::RHIGetNativeInstance()
{
	return nullptr;
}

void FOpenGLDynamicRHI::InvalidateQueries( void )
{
	{
		FScopeLock Lock(&QueriesListCriticalSection);
		PendingState.RunningOcclusionQuery = 0;
		for( int32 Index = 0; Index < Queries.Num(); ++Index )
		{
			Queries[Index]->bInvalidResource = true;
		}
	}
}

void* FOpenGLDynamicRHI::GetOpenGLCurrentContextHandle()
{
	return PlatformOpenGLCurrentContextHandle(PlatformDevice);
}

void FOpenGLDynamicRHI::SetCustomPresent(FRHICustomPresent* InCustomPresent)
{
	FScopeLock lock(&CustomPresentSection);
	CustomPresent = InCustomPresent;
}

uint64 FOpenGLDynamicRHI::RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format)
{
	check(FOpenGL::GetTextureBufferAlignment()>=0);
	return FOpenGL::GetTextureBufferAlignment();
}

bool FOpenGLDynamicRHIModule::IsSupported()
{
	return true;
}
