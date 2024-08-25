// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLViewport.cpp: OpenGL viewport RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

static int32 GOpenGLPerFrameErrorCheck = 1;
static FAutoConsoleVariableRef CVarPerFrameGLErrorCheck(
	TEXT("r.OpenGL.PerFrameErrorCheck"),
	GOpenGLPerFrameErrorCheck,
	TEXT("When no other GL debugging is in use, check for GL errors once per frame.\nNot active in shipping builds.\n")
	TEXT("0: GL errors not be checked.\n")
	TEXT("1: any GL errors will be logged as errors. (default)\n")
	TEXT("2: any GL errors will be fatal.\n")
	,
	ECVF_RenderThreadSafe
);

static void CheckForGLErrors()
{
#if !UE_BUILD_SHIPPING 
	if (GOpenGLPerFrameErrorCheck && IsOGLDebugOutputEnabled() == false && !ENABLE_VERIFY_GL)
	{
		int32 Error = PlatformGlGetError();
		if (Error != GL_NO_ERROR)
		{
			switch (GOpenGLPerFrameErrorCheck)
			{
			case 1:
				UE_LOG(LogRHI, Error, TEXT("GL Error encountered during frame %d, glerror=0x%x. Set command line arg -OpenGLDebugLevel=1 for detailed debugging."), GFrameNumber, Error);
				break;
			default: checkNoEntry(); [[fallthrough]];
			case 2:
				UE_LOG(LogRHI, Fatal, TEXT("GL Error encountered during frame %d, glerror=0x%x. Set command line arg -OpenGLDebugLevel=1 for detailed debugging."), GFrameNumber, Error);
				break;
			}
		}
	}
#endif
}

void FOpenGLDynamicRHI::RHIGetSupportedResolution(uint32 &Width, uint32 &Height)
{
	PlatformGetSupportedResolution(Width, Height);
}

bool FOpenGLDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	const bool Result = PlatformGetAvailableResolutions(Resolutions, bIgnoreRefreshRate);
	if (Result)
	{
		Resolutions.Sort([](const FScreenResolutionRHI& L, const FScreenResolutionRHI& R)
		{
			if (L.Width != R.Width)
			{
				return L.Width < R.Width;
			}
			else if (L.Height != R.Height)
			{
				return L.Height < R.Height;
			}
			else
			{
				return L.RefreshRate < R.RefreshRate;
			}
		});
	}
	return Result;
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FOpenGLDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat PreferredPixelFormat)
{
	check(IsInGameThread());

//	SCOPED_SUSPEND_RENDERING_THREAD(true);

	// Use a default pixel format if none was specified	
	PreferredPixelFormat = RHIPreferredPixelFormatHint(PreferredPixelFormat);

	return new FOpenGLViewport(this,WindowHandle,SizeX,SizeY,bIsFullscreen,PreferredPixelFormat);
}

void FOpenGLDynamicRHI::RHIResizeViewport(FRHIViewport* ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	FOpenGLViewport* Viewport = ResourceCast(ViewportRHI);
	check( IsInGameThread() );

//	SCOPED_SUSPEND_RENDERING_THREAD(true);

	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

EPixelFormat FOpenGLDynamicRHI::RHIPreferredPixelFormatHint(EPixelFormat PreferredPixelFormat)
{
	return FOpenGL::PreferredPixelFormatHint(PreferredPixelFormat);
}

void FOpenGLDynamicRHI::RHITick( float DeltaTime )
{
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FOpenGLDynamicRHI::RHIBeginDrawingViewport(FRHIViewport* ViewportRHI, FRHITexture* RenderTarget)
{
	VERIFY_GL_SCOPE();

	FOpenGLViewport* Viewport = ResourceCast(ViewportRHI);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLPresentTime);

	check(!DrawingViewport);
	DrawingViewport = Viewport;

	bRevertToSharedContextAfterDrawingViewport = false;
	EOpenGLCurrentContext CurrentContext = PlatformOpenGLCurrentContext( PlatformDevice );
	if( CurrentContext != CONTEXT_Rendering )
	{
		check(CurrentContext == CONTEXT_Shared);
		check(!bIsRenderingContextAcquired || !GUseThreadedRendering);

		bRevertToSharedContextAfterDrawingViewport = true;
		PlatformRenderingContextSetup(PlatformDevice);
	}

	// Set the render target and viewport.
	if( RenderTarget )
	{
		FRHIRenderTargetView RTV(RenderTarget, ERenderTargetLoadAction::ELoad);
		SetRenderTargets(1, &RTV, nullptr);
	}
	else
	{
		FRHIRenderTargetView RTV(DrawingViewport->GetBackBuffer(), ERenderTargetLoadAction::ELoad);
		SetRenderTargets(1, &RTV, nullptr);
	}

	if (IsValidRef(CustomPresent))
	{
		CustomPresent->BeginDrawing();
	}
}

void FOpenGLDynamicRHI::RHIEndDrawingViewport(FRHIViewport* ViewportRHI,bool bPresent,bool bLockToVsync)
{
	VERIFY_GL_SCOPE();

	FOpenGLViewport* Viewport = ResourceCast(ViewportRHI);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLPresentTime);
	{
		FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUPresent);

		check(DrawingViewport.GetReference() == Viewport);

		FOpenGLTexture* BackBuffer = Viewport->GetBackBuffer();

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();

		if (ContextState.bScissorEnabled)
		{
			ContextState.bScissorEnabled = false;
			glDisable(GL_SCISSOR_TEST);
		}

		bool bNeedFinishFrame = PlatformBlitToViewport(PlatformDevice,
			*Viewport, 
			BackBuffer->GetSizeX(),
			BackBuffer->GetSizeY(),
			bPresent,
			bLockToVsync
		);

		// Always consider the Framebuffer in the rendering context dirty after the blit
		RenderingContextState.Framebuffer = -1;

		DrawingViewport = NULL;

		if (bNeedFinishFrame)
		{
			static const auto CFinishFrameVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FinishCurrentFrame"));
			if (!CFinishFrameVar->GetValueOnRenderThread())
			{
				// Wait for the GPU to finish rendering the previous frame before finishing this frame.
				Viewport->WaitForFrameEventCompletion();
				Viewport->IssueFrameEvent();
			}
			else
			{
				// Finish current frame immediately to reduce latency
				Viewport->IssueFrameEvent();
				Viewport->WaitForFrameEventCompletion();
			}
		}
		
		// If the input latency timer has been triggered, block until the GPU is completely
		// finished displaying this frame and calculate the delta time.
		if ( GInputLatencyTimer.RenderThreadTrigger )
		{
			Viewport->WaitForFrameEventCompletion();
			uint32 EndTime = FPlatformTime::Cycles();
			GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
			GInputLatencyTimer.RenderThreadTrigger = false;
		}

		if (bRevertToSharedContextAfterDrawingViewport)
		{
			PlatformSharedContextSetup(PlatformDevice);
			bRevertToSharedContextAfterDrawingViewport = false;
		}
	}

	EndFrameTick();

	CheckForGLErrors();
}


FTexture2DRHIRef FOpenGLDynamicRHI::RHIGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	FOpenGLViewport* Viewport = ResourceCast(ViewportRHI);
	return Viewport->GetBackBuffer();
}

void FOpenGLDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* Viewport)
{
}


FOpenGLViewport::FOpenGLViewport(FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen,EPixelFormat PreferredPixelFormat)
	: OpenGLRHI(InOpenGLRHI)
	, OpenGLContext(NULL)
	, SizeX(0)
	, SizeY(0)
	, bIsFullscreen(false)
	, PixelFormat(PreferredPixelFormat)
	, bIsValid(true)
{
	check(OpenGLRHI);
	// @todo lumin: Add a "PLATFORM_HAS_NO_NATIVE_WINDOW" or something
#if !PLATFORM_ANDROID
	check(InWindowHandle);
#endif
	check(IsInGameThread());

	// flush out old errors.
	PlatformGlGetError();	

	OpenGLRHI->Viewports.Add(this);

	check(PlatformOpenGLCurrentContext(OpenGLRHI->PlatformDevice) == CONTEXT_Shared);

	OpenGLContext = PlatformCreateOpenGLContext(OpenGLRHI->PlatformDevice, InWindowHandle);
	Resize(InSizeX, InSizeY, bInIsFullscreen);

	check(PlatformOpenGLCurrentContext(OpenGLRHI->PlatformDevice) == CONTEXT_Shared);

	ENQUEUE_RENDER_COMMAND(CreateFrameSyncEvent)([this](FRHICommandListImmediate& RHICmdList)
	{
		RunOnGLRenderContextThread([this]()
		{
			FrameSyncEvent = MakeUnique<FOpenGLEventQuery>();
		});
	});
}

FOpenGLViewport::~FOpenGLViewport()
{
	check(IsInRenderingThread() || IsInRHIThread());

	if (bIsFullscreen)
	{
		PlatformRestoreDesktopDisplayMode();
	}

	// Release back buffer, before OpenGL context becomes invalid, making it impossible
	BackBuffer.SafeRelease();
	check(!IsValidRef(BackBuffer));

	RunOnGLRenderContextThread([&]()
	{
		FrameSyncEvent = nullptr;
		PlatformDestroyOpenGLContext(OpenGLRHI->PlatformDevice, OpenGLContext);
	}, true);

	OpenGLContext = NULL;
	OpenGLRHI->Viewports.Remove(this);
}

void FOpenGLViewport::WaitForFrameEventCompletion()
{
	VERIFY_GL_SCOPE();
	FrameSyncEvent->WaitForCompletion();
}

void FOpenGLViewport::IssueFrameEvent()
{
	VERIFY_GL_SCOPE();
	FrameSyncEvent->IssueEvent();
}

void FOpenGLViewport::Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{
	check(IsInGameThread());
	if ((InSizeX == SizeX) && (InSizeY == SizeY) && (bInIsFullscreen == bIsFullscreen))
	{
		return;
	}

	SizeX = InSizeX;
	SizeY = InSizeY;
	bool bWasFullscreen = bIsFullscreen;
	bIsFullscreen = bInIsFullscreen;

	ENQUEUE_RENDER_COMMAND(ResizeViewport)([this, InSizeX, InSizeY, bInIsFullscreen, bWasFullscreen](FRHICommandListImmediate& RHICmdList)
	{
		if (IsValidRef(CustomPresent))
		{
			CustomPresent->OnBackBufferResize();
		}

		BackBuffer.SafeRelease();	// when the rest of the engine releases it, its framebuffers will be released too (those the engine knows about)

		BackBuffer = PlatformCreateBuiltinBackBuffer(OpenGLRHI, InSizeX, InSizeY);
		if (!BackBuffer)
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("FOpenGLViewport"), InSizeX, InSizeY, PixelFormat)
				.SetClearValue(FClearValueBinding::Transparent)
				.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ResolveTargetable)
				.DetermineInititialState();

			BackBuffer = new FOpenGLTexture(RHICmdList, Desc);
		}

		RHICmdList.EnqueueLambda([this, InSizeX, InSizeY, bInIsFullscreen, bWasFullscreen](FRHICommandListImmediate&)
		{
			PlatformResizeGLContext(OpenGLRHI->PlatformDevice, OpenGLContext, InSizeX, InSizeY, bInIsFullscreen, bWasFullscreen, BackBuffer->Target, BackBuffer->GetResource());
		});
	});	
}

void* FOpenGLViewport::GetNativeWindow(void** AddParam) const
{
	return PlatformGetWindow(OpenGLContext, AddParam);
}

