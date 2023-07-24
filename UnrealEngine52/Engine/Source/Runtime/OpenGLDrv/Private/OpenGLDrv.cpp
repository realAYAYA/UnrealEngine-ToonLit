// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLDrv.cpp: Unreal OpenGL RHI library implementation.
=============================================================================*/

#include "OpenGLDrv.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "StaticBoundShaderState.h"
#include "RHIStaticStates.h"
#include "Engine/Engine.h"
#include "OpenGLDrvPrivate.h"
#include "PipelineStateCache.h"
#include "Engine/GameViewportClient.h"
#include "DataDrivenShaderPlatformInfo.h"


IMPLEMENT_MODULE(FOpenGLDynamicRHIModule, OpenGLDrv);

#include "Shader.h"
#include "OneColorShader.h"
#include "OpenGLShaders.h"

/** OpenGL Logging. */
DEFINE_LOG_CATEGORY(LogOpenGL);

#define LOCTEXT_NAMESPACE "OpenGLDrv"

ERHIFeatureLevel::Type GRequestedFeatureLevel = ERHIFeatureLevel::Num;


int32 FOpenGLDynamicRHI::RHIGetGLMajorVersion() const
{
	return FOpenGL::GetMajorVersion();
}

int32 FOpenGLDynamicRHI::RHIGetGLMinorVersion() const
{
	return FOpenGL::GetMinorVersion();
}

bool FOpenGLDynamicRHI::RHISupportsFramebufferSRGBEnable() const
{
	return FOpenGL::SupportsFramebufferSRGBEnable();
}

GLuint FOpenGLDynamicRHI::RHIGetResource(FRHITexture* InTexture) const
{
	FOpenGLTexture* GLTexture = GetOpenGLTextureFromRHITexture(InTexture);
	return GLTexture->GetResource();
}

bool FOpenGLDynamicRHI::RHIIsValidTexture(GLuint InTexture) const
{
	return glIsTexture(InTexture) == GL_TRUE;
}

void FOpenGLDynamicRHI::RHISetExternalGPUTime(uint32 InExternalGPUTime)
{
	GetGPUProfilingData().ExternalGPUTime = InExternalGPUTime;
}

#if PLATFORM_ANDROID

EGLDisplay FOpenGLDynamicRHI::RHIGetEGLDisplay() const
{
	return AndroidEGL::GetInstance()->GetDisplay();
}

EGLSurface FOpenGLDynamicRHI::RHIGetEGLSurface() const
{
	return AndroidEGL::GetInstance()->GetSurface();
}

EGLConfig FOpenGLDynamicRHI::RHIGetEGLConfig() const
{
	return AndroidEGL::GetInstance()->GetConfig();
}

EGLContext FOpenGLDynamicRHI::RHIGetEGLContext() const
{
	return AndroidEGL::GetInstance()->GetRenderingContext()->eglContext;
}

ANativeWindow* FOpenGLDynamicRHI::RHIGetEGLNativeWindow() const
{
	return AndroidEGL::GetInstance()->GetNativeWindow();
}

bool FOpenGLDynamicRHI::RHIEGLSupportsNoErrorContext() const
{
	return AndroidEGL::GetInstance()->GetSupportsNoErrorContext();
}

void FOpenGLDynamicRHI::RHIInitEGLInstanceGLES2()
{
	AndroidEGL::GetInstance()->Init(AndroidEGL::AV_OpenGLES, 2, 0, false);
	AndroidEGL::GetInstance()->InitSurface(false, false);
}

void FOpenGLDynamicRHI::RHIInitEGLBackBuffer()
{
	AndroidEGL::GetInstance()->InitBackBuffer();
}

void FOpenGLDynamicRHI::RHIEGLSetCurrentRenderingContext()
{
	AndroidEGL::GetInstance()->SetCurrentRenderingContext();
}

void FOpenGLDynamicRHI::RHIEGLTerminateContext()
{
	AndroidEGL::GetInstance()->Terminate();
}
#endif


void FOpenGLDynamicRHI::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if ENABLE_OPENGL_DEBUG_GROUPS
	// @todo-mobile: Fix string conversion ASAP!
	FOpenGL::PushGroupMarker(TCHAR_TO_ANSI(Name));
#endif
	GPUProfilingData.PushEvent(Name, Color);
}

void FOpenGLGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	FGPUProfiler::PushEvent(Name, Color);
}

void FOpenGLDynamicRHI::RHIPopEvent()
{
#if ENABLE_OPENGL_DEBUG_GROUPS
	FOpenGL::PopGroupMarker();
#endif

	GPUProfilingData.PopEvent();
}

void FOpenGLGPUProfiler::PopEvent()
{
	FGPUProfiler::PopEvent();

}

bool FOpenGLDynamicRHI::RHIRequiresComputeGenerateMips() const
{
	return !FOpenGL::SupportsGenerateMipmap();
};

void FOpenGLGPUProfiler::BeginFrame(FOpenGLDynamicRHI* InRHI)
{
	if (!bIntialized)
	{
		bIntialized = true;
		InitResources();
	}
	if (NestedFrameCount++>0)
	{
		// guard against nested Begin/EndFrame calls.
		return;
	}

	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}
	
	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GetEmitDrawEvents();
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			SetEmitDrawEvents(true);  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			CurrentEventNodeFrame = new FOpenGLEventNodeFrame(InRHI);
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	if (FrameTiming.IsSupported())
	{
		FrameTiming.StartTiming();
	}
	if (FOpenGLDisjointTimeStampQuery::IsSupported())
	{
		CurrentGPUFrameQueryIndex = (CurrentGPUFrameQueryIndex + 1) % MAX_GPUFRAMEQUERIES;
		DisjointGPUFrameTimeQuery[CurrentGPUFrameQueryIndex].StartTracking();
	}

	if (GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FOpenGLGPUProfiler::EndFrame()
{
	if (--NestedFrameCount != 0)
	{
		// ignore endframes calls from nested beginframe calls.
		return;
	}

	if (GetEmitDrawEvents())
	{
		PopEvent();
	}

	if (FrameTiming.IsSupported())
	{
		FrameTiming.EndTiming();
	}
	if (FOpenGLDisjointTimeStampQuery::IsSupported())
	{
		DisjointGPUFrameTimeQuery[CurrentGPUFrameQueryIndex].EndTracking();
	}

	if (FrameTiming.IsSupported())
	{
		uint64 GPUTiming = FrameTiming.GetTiming();
		uint64 GPUFreq = FrameTiming.GetTimingFrequency();
		GGPUFrameTime = FMath::TruncToInt( double(GPUTiming) / double(GPUFreq) / FPlatformTime::GetSecondsPerCycle() );
	}
	else if (FOpenGLDisjointTimeStampQuery::IsSupported())
	{
		static uint32 GLastGPUFrameTime = 0;
		uint64 GPUTiming = 0;
		uint64 GPUFreq = FOpenGLDisjointTimeStampQuery::GetTimingFrequency();
		int OldestQueryIndex = (CurrentGPUFrameQueryIndex + 1) % MAX_GPUFRAMEQUERIES;
		if ( DisjointGPUFrameTimeQuery[OldestQueryIndex].IsResultValid() && DisjointGPUFrameTimeQuery[OldestQueryIndex].GetResult(&GPUTiming) )
		{
			GGPUFrameTime = FMath::TruncToInt( double(GPUTiming) / double(GPUFreq) / FPlatformTime::GetSecondsPerCycle() );
			GLastGPUFrameTime = GGPUFrameTime;
		}
		else
		{
			// Keep the timing of the last frame if the query was disjoint (e.g. GPU frequency changed and the result is undefined)
			GGPUFrameTime = GLastGPUFrameTime;
		}
	}
	else
	{
		GGPUFrameTime = ExternalGPUTime;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			SetEmitDrawEvents(bOriginalGEmitDrawEvents);
			UE_LOG(LogRHI, Warning, TEXT(""));
			UE_LOG(LogRHI, Warning, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();

			// OPENGL_PERFORMANCE_DATA_INVALID is a compile time constant
			bool DebugEnabled = false;
#ifdef GL_ARB_debug_output
			DebugEnabled = GL_TRUE == glIsEnabled( GL_DEBUG_OUTPUT );
#endif
#if !OPENGL_PERFORMANCE_DATA_INVALID
			if( DebugEnabled )
#endif
			{
				UE_LOG(LogRHI, Warning, TEXT(""));
				UE_LOG(LogRHI, Warning, TEXT(""));
				UE_LOG(LogRHI, Warning, TEXT("*********************************************************************************************"));
				UE_LOG(LogRHI, Warning, TEXT("OpenGL perfomance data is potentially invalid because of the following build/runtime options:"));
				
#define LOG_GL_DEBUG_FLAG(a) UE_LOG(LogRHI, Warning, TEXT("   built with %s = %d"), TEXT(#a), a);
				LOG_GL_DEBUG_FLAG(ENABLE_VERIFY_GL);
				LOG_GL_DEBUG_FLAG(ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION);
				LOG_GL_DEBUG_FLAG(ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP);
				LOG_GL_DEBUG_FLAG(DEBUG_GL_SHADERS);
				LOG_GL_DEBUG_FLAG(ENABLE_OPENGL_DEBUG_GROUPS);
				LOG_GL_DEBUG_FLAG(OPENGL_PERFORMANCE_DATA_INVALID);
#undef LOG_GL_DEBUG_FLAG

				UE_LOG(LogRHI, Warning, TEXT("*********************************************************************************************"));
				UE_LOG(LogRHI, Warning, TEXT(""));
				UE_LOG(LogRHI, Warning, TEXT(""));
			}


			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;

			if (RHIConfig::ShouldSaveScreenshotAfterProfilingGPU()
				&& GEngine->GameViewport)
			{
				GEngine->GameViewport->Exec( NULL, TEXT("SCREENSHOT"), *GLog);
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static double LastTime = -1.0;
		double Now = FPlatformTime::Seconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			static const float HitchThreshold = .1f; //100ms
			float ThisTime = Now - LastTime;
			bool bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				UE_LOG(LogRHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogRHI, Warning, TEXT("********** Hitch detected on CPU, frametime = %6.1fms"),ThisTime * 1000.0f);
				UE_LOG(LogRHI, Warning, TEXT("*******************************************************************************"));

				for (int32 Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					UE_LOG(LogRHI, Warning, TEXT(""));
					UE_LOG(LogRHI, Warning, TEXT(""));
					UE_LOG(LogRHI, Warning, TEXT("********** GPU Frame: Current - %d"),GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames[Frame].DumpEventTree();
				}
				UE_LOG(LogRHI, Warning, TEXT(""));
				UE_LOG(LogRHI, Warning, TEXT(""));
				UE_LOG(LogRHI, Warning, TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				UE_LOG(LogRHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogRHI, Warning, TEXT("********** End Hitch GPU Profile"));
				UE_LOG(LogRHI, Warning, TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec( NULL, TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const int32 HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.RemoveAt(0);
				}
				GPUHitchEventNodeFrames.Add((FOpenGLEventNodeFrame*)CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}
	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}

void FOpenGLGPUProfiler::Cleanup()
{
	if (bIntialized)
	{
		for (int32 Index = 0; Index < MAX_GPUFRAMEQUERIES; ++Index)
		{
			DisjointGPUFrameTimeQuery[Index].ReleaseResources();
		}

		FrameTiming.ReleaseResources();
		NestedFrameCount = 0;
		bIntialized = false;
	}
}

/** Start this frame of per tracking */
void FOpenGLEventNodeFrame::StartFrame()
{
	EventTree.Reset();
	DisjointQuery.StartTracking();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FOpenGLEventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
	DisjointQuery.EndTracking();
}


float FOpenGLEventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency();

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

void FOpenGLEventNodeFrame::LogDisjointQuery()
{
	if (DisjointQuery.IsSupported())
	{
		UE_LOG(LogRHI, Warning, TEXT("%s"),
			DisjointQuery.IsResultValid() ?
			TEXT("Profiled range was continuous.") :
			TEXT("Profiled range was disjoint! GPU switched to doing something else while profiling.")
			);
	}
	else
	{
		UE_LOG(LogRHI, Warning, TEXT("Profiled range \"disjoinness\" could not be determined due to lack of disjoint timer query functionality on this platform."));
	}
}

float FOpenGLEventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const uint64 GPUTiming = Timing.GetTiming(true);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

void FOpenGLDynamicRHI::InitializeStateResources()
{
	VERIFY_GL_SCOPE();
	SharedContextState.InitializeResources(FOpenGL::GetMaxCombinedTextureImageUnits(), FOpenGL::GetMaxCombinedUAVUnits());
	RenderingContextState.InitializeResources(FOpenGL::GetMaxCombinedTextureImageUnits(), FOpenGL::GetMaxCombinedUAVUnits());
	PendingState.InitializeResources(FOpenGL::GetMaxCombinedTextureImageUnits(), FOpenGL::GetMaxCombinedUAVUnits());
}

GLint FOpenGLBase::MaxTextureImageUnits = -1;
GLint FOpenGLBase::MaxCombinedTextureImageUnits = -1;
GLint FOpenGLBase::MaxComputeTextureImageUnits = -1;
GLint FOpenGLBase::MaxVertexTextureImageUnits = -1;
GLint FOpenGLBase::MaxGeometryTextureImageUnits = -1;
GLint FOpenGLBase::MaxVaryingVectors = -1;
GLint FOpenGLBase::TextureBufferAlignment = -1;
GLint FOpenGLBase::MaxVertexUniformComponents = -1;
GLint FOpenGLBase::MaxPixelUniformComponents = -1;
GLint FOpenGLBase::MaxGeometryUniformComponents = -1;
bool  FOpenGLBase::bSupportsClipControl = false;
bool  FOpenGLBase::bSupportsASTC = false;
bool  FOpenGLBase::bSupportsASTCHDR = false;
bool  FOpenGLBase::bSupportsSeamlessCubemap = false;
bool  FOpenGLBase::bSupportsVolumeTextureRendering = false;
bool  FOpenGLBase::bSupportsTextureFilterAnisotropic = false;
bool  FOpenGLBase::bSupportsDrawBuffersBlend = false;
bool  FOpenGLBase::bAmdWorkaround = false;

void FOpenGLBase::ProcessQueryGLInt()
{
	LOG_AND_GET_GL_INT(GL_MAX_TEXTURE_IMAGE_UNITS, 0, MaxTextureImageUnits);
	LOG_AND_GET_GL_INT(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, 0, MaxVertexTextureImageUnits);
	LOG_AND_GET_GL_INT(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 0, MaxComputeTextureImageUnits);
	GET_GL_INT(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 0, MaxCombinedTextureImageUnits);
}

void FOpenGLBase::ProcessExtensions( const FString& ExtensionsString )
{
	ProcessQueryGLInt();

	auto CheckAndSetImageUnits = [](GLint& StageImageUnitsINOUT, GLint Limit, const TCHAR* Msg) 
	{
		const bool bUnsupported = StageImageUnitsINOUT < Limit;
		UE_CLOG(bUnsupported, LogRHI, Error, TEXT("GL RHI requires a minimum %s texture unit count of %d, this device reports %d."), Msg, Limit, StageImageUnitsINOUT);
		check(!bUnsupported);
		StageImageUnitsINOUT = Limit;
	};

	static const GLint GLESMaxImageUnitsPerStage = 16; // gles 3 spec is a minimum of 16 per stage. 
	static const GLint MaxCombinedImageUnits = 48;

	if (IsMobilePlatform(GMaxRHIShaderPlatform))
	{
		// clamp things to the levels that the spec is expecting, check the minimum is supported.
		CheckAndSetImageUnits(MaxTextureImageUnits, GLESMaxImageUnitsPerStage, TEXT("pixel stage"));
		CheckAndSetImageUnits(MaxVertexTextureImageUnits, GLESMaxImageUnitsPerStage, TEXT("vertex stage"));
		CheckAndSetImageUnits(MaxGeometryTextureImageUnits, 0, TEXT("geometry stage")); // gles is not expecting this.
		CheckAndSetImageUnits(MaxComputeTextureImageUnits, GLESMaxImageUnitsPerStage, TEXT("compute stage"));
		CheckAndSetImageUnits(MaxCombinedTextureImageUnits, MaxCombinedImageUnits, TEXT("combined"));
	}
	else
	{
		UE_CLOG(MaxCombinedTextureImageUnits<MaxCombinedImageUnits, LogRHI, Fatal, TEXT("GL RHI requires a minimum combined texture unit count of %d, this device reports %d."), MaxCombinedImageUnits, MaxCombinedTextureImageUnits);
	}

	// Check for support for advanced texture compression (desktop and mobile)
	bSupportsASTC = ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));

	bSupportsASTCHDR = bSupportsASTC && ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_hdr"));

	bSupportsSeamlessCubemap = ExtensionsString.Contains(TEXT("GL_ARB_seamless_cube_map"));
	
	bSupportsTextureFilterAnisotropic = ExtensionsString.Contains(TEXT("GL_EXT_texture_filter_anisotropic"));

	bSupportsDrawBuffersBlend = ExtensionsString.Contains(TEXT("GL_ARB_draw_buffers_blend"));

#if PLATFORM_IOS
	GRHIVendorId = 0x1010;
#else
	FString VendorName( ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VENDOR) ) );
	if (VendorName.Contains(TEXT("ATI ")))
	{
		GRHIVendorId = 0x1002;
#if PLATFORM_WINDOWS || PLATFORM_LINUX
		bAmdWorkaround = true;
#endif
	}
#if PLATFORM_LINUX
	else if (VendorName.Contains(TEXT("X.Org")))
	{
		GRHIVendorId = 0x1002;
		bAmdWorkaround = true;
	}
#endif
	else if (VendorName.Contains(TEXT("Intel ")) || VendorName == TEXT("Intel"))
	{
		GRHIVendorId = 0x8086;
#if PLATFORM_WINDOWS || PLATFORM_LINUX
		bAmdWorkaround = true;
#endif
	}
	else if (VendorName.Contains(TEXT("NVIDIA ")))
	{
		GRHIVendorId = 0x10DE;
	}
	else if (VendorName.Contains(TEXT("ImgTec")) || VendorName.Contains(TEXT("Imagination")))
	{
		GRHIVendorId = 0x1010;
	}
	else if (VendorName.Contains(TEXT("ARM")))
	{
		GRHIVendorId = 0x13B5;
	}
	else if (VendorName.Contains(TEXT("Qualcomm")))
	{
		GRHIVendorId = 0x5143;
	}

#if PLATFORM_LINUX
	if (GRHIVendorId == 0x0)
	{
		// Try harder for Mesa
		const ANSICHAR* AnsiVersion = (const ANSICHAR*)glGetString(GL_VERSION);
		const ANSICHAR* AnsiRenderer = (const ANSICHAR*)glGetString(GL_RENDERER);
		if (AnsiVersion && AnsiRenderer)
		{
			if (FCStringAnsi::Strstr(AnsiVersion, "Mesa"))
			{
				if (FCStringAnsi::Strstr(AnsiRenderer, "AMD") || FCStringAnsi::Strstr(AnsiRenderer, "ATI"))
				{
					// Radeon
					GRHIVendorId = 0x1002;
					bAmdWorkaround = true;
				}
				else if (FCStringAnsi::Strstr(AnsiRenderer, "Intel"))
				{
					GRHIVendorId = 0x8086;
					bAmdWorkaround = true;
				}
			}
		}

		// If still not detected, show a message box to the user (editor build only) and
		// set GRHIVendorId to something to avoid crashing in check()s later
		if (GRHIVendorId == 0x0)
		{
			if (WITH_EDITOR != 0 && !IsRunningCommandlet() && !FApp::IsUnattended())
			{
				FString GlRenderer(ANSI_TO_TCHAR(AnsiRenderer));
				FText ErrorMessage = FText::Format(LOCTEXT("CannotDetermineGraphicsDriversVendor", "Unknown graphics drivers '{0}' by '{1}' are installed on this system. You may experience visual artifacts and other problems."),
					FText::FromString(GlRenderer), FText::FromString(VendorName));
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(),
					*LOCTEXT("CannotDetermineGraphicsDriversVendorTitle", "Cannot determine driver vendor.").ToString());
			}

			GRHIVendorId = 0xFFFF;
			bAmdWorkaround = true;	// be conservative here as well.
		}
	}
#endif // PLATFORM_LINUX

#if PLATFORM_WINDOWS
	auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("OpenGL.UseStagingBuffer"));
	if (CVar)
	{
		CVar->Set(false);
	}
#endif
#endif // !PLATFORM_IOS

	// Setup CVars that require the RHI initialized
}

void FOpenGLBase::PE_GetCurrentOpenGLShaderDeviceCapabilities(FOpenGLShaderDeviceCapabilities& Capabilities)
{
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Unknown;
}

void GetExtensionsString( FString& ExtensionsString)
{
	GLint ExtensionCount = 0;
	ExtensionsString = TEXT("");
	if ( FOpenGL::SupportsIndexedExtensions() )
	{
		glGetIntegerv(GL_NUM_EXTENSIONS, &ExtensionCount);
		for (int32 ExtensionIndex = 0; ExtensionIndex < ExtensionCount; ++ExtensionIndex)
		{
			const ANSICHAR* ExtensionString = FOpenGL::GetStringIndexed(GL_EXTENSIONS, ExtensionIndex);

			ExtensionsString += TEXT(" ");
			ExtensionsString += ANSI_TO_TCHAR(ExtensionString);
		}
	}
	else
	{
		const ANSICHAR* GlGetStringOutput = (const ANSICHAR*) glGetString( GL_EXTENSIONS );
		if (GlGetStringOutput)
		{
			ExtensionsString += GlGetStringOutput;
			ExtensionsString += TEXT(" ");
		}
	}
}

namespace OpenGLConsoleVariables
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	int32 bUseGlClipControlIfAvailable = 1;
#else
	int32 bUseGlClipControlIfAvailable = 0;
#endif
	static FAutoConsoleVariableRef CVarUseGlClipControlIfAvailable(
		TEXT("OpenGL.UseGlClipControlIfAvailable"),
		bUseGlClipControlIfAvailable,
		TEXT("If true, the engine trys to use glClipControl if the driver supports it."),
		ECVF_RenderThreadSafe | ECVF_ReadOnly
	);
}

void InitDefaultGLContextState(void)
{
	// NOTE: This function can be called before capabilities setup, so extensions need to be checked directly
	FString ExtensionsString;
	GetExtensionsString(ExtensionsString);

	// Intel HD4000 under <= 10.8.4 requires GL_DITHER disabled or dithering will occur on any channel < 8bits.
	// No other driver does this but we don't need GL_DITHER on anyway.
	glDisable(GL_DITHER);

	if (FOpenGL::SupportsFramebufferSRGBEnable())
	{
		// Render targets with TexCreate_SRGB should do sRGB conversion like in D3D11
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	// Engine always expects seamless cubemap, so enable it if available
	if (ExtensionsString.Contains(TEXT("GL_ARB_seamless_cube_map")))
	{
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

#if PLATFORM_WINDOWS || PLATFORM_LINUX
	if (OpenGLConsoleVariables::bUseGlClipControlIfAvailable && ExtensionsString.Contains(TEXT("GL_ARB_clip_control")) && !FOpenGL::IsAndroidGLESCompatibilityModeEnabled())
	{
		FOpenGL::EnableSupportsClipControl();
		glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
	}
#endif

	// optional per platform setup
	FOpenGL::SetupDefaultGLContextState(ExtensionsString);
}

#undef LOCTEXT_NAMESPACE
