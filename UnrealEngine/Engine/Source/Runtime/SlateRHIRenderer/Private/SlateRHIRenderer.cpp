// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRenderer.h"
#include "Fonts/FontCache.h"
#include "SlateRHIRenderingPolicy.h"
#include "SlateRHIRendererSettings.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineGlobals.h"
#include "Engine/AssetManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "FX/SlateFXSubsystem.h"
#include "FX/SlateRHIPostBufferProcessor.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "SceneInterface.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "UnrealEngine.h"
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "SlateShaders.h"
#include "Rendering/ElementBatcher.h"
#include "Rendering/SlateRenderer.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "RHIResources.h"
#include "RHIUtilities.h"
#include "StereoRendering.h"
#include "SlateNativeTextureResource.h"
#include "SceneUtils.h"
#include "TextureResource.h"
#include "VolumeRendering.h"
#include "PipelineStateCache.h"
#include "EngineModule.h"
#include "Interfaces/ISlate3DRenderer.h"
#include "SlateRHIRenderingPolicy.h"
#include "Interfaces/SlateRHIRenderingPolicyInterface.h"
#include "Slate/SlateTextureAtlasInterface.h"
#include "Types/ReflectionMetadata.h"
#include "CommonRenderResources.h"
#include "RenderTargetPool.h"
#include "RendererUtils.h"
#include "HAL/LowLevelMemTracker.h"
#include "Rendering/RenderingCommon.h"
#include "IHeadMountedDisplayModule.h"
#include "HDRHelper.h"
#include "RenderCore.h"
#include "DataDrivenShaderPlatformInfo.h"

#if WITH_EDITORONLY_DATA
#include "ShaderCompiler.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Slate RT: Rendering"), STAT_SlateRenderingRTTime, STATGROUP_Slate);

DECLARE_CYCLE_STAT(TEXT("Slate RT: Draw Batches"), STAT_SlateRTDrawBatches, STATGROUP_Slate);

DECLARE_CYCLE_STAT(TEXT("Total Render Thread time including dependent waits"), STAT_RenderThreadCriticalPath, STATGROUP_Threading);

CSV_DEFINE_CATEGORY(RenderThreadIdle, true);

DECLARE_GPU_DRAWCALL_STAT_NAMED(SlateUI, TEXT("Slate UI"));

// Defines the maximum size that a slate viewport will create
#define MIN_VIEWPORT_SIZE 8
#define MAX_VIEWPORT_SIZE 16384

static TAutoConsoleVariable<float> CVarUILevel(
	TEXT("r.HDR.UI.Level"),
	1.0f,
	TEXT("Luminance level for UI elements when compositing into HDR framebuffer (default: 1.0)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarHDRUILuminance(
	TEXT("r.HDR.UI.Luminance"),
	300.0f,
	TEXT("Base Luminance in nits for UI elements when compositing into HDR framebuffer. Gets multiplied by r.HDR.UI.Level"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarUICompositeMode(
	TEXT("r.HDR.UI.CompositeMode"),
	1,
	TEXT("Mode used when compositing the UI layer:\n")
	TEXT("0: Standard compositing\n")
	TEXT("1: Shader pass to improve HDR blending\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDrawToVRRenderTarget(
	TEXT("Slate.DrawToVRRenderTarget"),
	1,
	TEXT("If enabled while in VR. Slate UI will be drawn into the render target texture where the VR imagery for either eye was rendered, allow the viewer of the HMD to see the UI (for better or worse.)  This render target will then be cropped/scaled into the back buffer, if mirroring is enabled.  When disabled, Slate UI will be drawn on top of the backbuffer (not to the HMD) after the mirror texture has been cropped/scaled into the backbuffer."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMemorylessDepthStencil(
	TEXT("Slate.MemorylessDepthStencil"),
	0,
	TEXT("Whether to use memoryless DepthStencil target for Slate. Reduces memory usage and implies that DepthStencil state can't be preserved between Slate renderpasses"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCopyBackbufferToSlatePostRenderTargets(
	TEXT("Slate.CopyBackbufferToSlatePostRenderTargets"),
	0,
	TEXT("Experimental. Set true to copy final backbuffer into slate RTs for slate post processing / material usage"),
	ECVF_RenderThreadSafe);

#if WITH_SLATE_VISUALIZERS

TAutoConsoleVariable<int32> CVarShowSlateOverdraw(
	TEXT("Slate.ShowOverdraw"),
	0,
	TEXT("0: Don't show overdraw, 1: Show Overdraw"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarShowSlateBatching(
	TEXT("Slate.ShowBatching"),
	0,
	TEXT("0: Don't show batching, 1: Show Batching"),
	ECVF_Default
);
#endif

// RT stat including waits toggle. Off by default for historical tracking reasons
TAutoConsoleVariable<int32> CVarRenderThreadTimeIncludesDependentWaits(
	TEXT("r.RenderThreadTimeIncludesDependentWaits"),
	0,
	TEXT("0: RT stat only includes non-idle time, 1: RT stat includes dependent waits (matching RenderThreadTime_CriticalPath)"),
	ECVF_Default
);


struct FSlateDrawWindowCommandParams
{
	FSlateRHIRenderer* Renderer;
	FSlateWindowElementList* WindowElementList;
	SWindow* Window;
	FIntRect ViewRect;
	ESlatePostRT UsedSlatePostBuffers;
#if WANTS_DRAW_MESH_EVENTS
	FString WindowTitle;
#endif
	FGameTime Time;
	bool bLockToVsync;
	bool bClear;
};

void FViewportInfo::InitRHI(FRHICommandListBase&)
{
	// Viewport RHI is created on the game thread
	// Create the depth-stencil surface if needed.
	RecreateDepthBuffer_RenderThread();
}

void FViewportInfo::ReleaseRHI()
{
	// Some RHIs delete resources when the command list completes on the GPU, but they don't take Present calls into account due to
	// API limitations. Sync the GPU here, to make sure nothing is using the backbuffer anymore.
	FRHICommandListExecutor::GetImmediateCommandList().BlockUntilGPUIdle();
	DepthStencil.SafeRelease();
	ViewportRHI.SafeRelease();
}

void FViewportInfo::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	UITargetRT.SafeRelease();
}

void FViewportInfo::ConditionallyUpdateDepthBuffer(bool bInRequiresStencilTest, uint32 InWidth, uint32 InHeight)
{
	check(IsInRenderingThread());

	bool bWantsMemorylessDepthStencil = (CVarMemorylessDepthStencil.GetValueOnAnyThread() != 0);

	bool bDepthStencilStale =
		bInRequiresStencilTest &&
		(!bRequiresStencilTest ||
		(DepthStencil.IsValid() && (DepthStencil->GetSizeX() != InWidth || DepthStencil->GetSizeY() != InHeight || IsMemorylessTexture(DepthStencil) != bWantsMemorylessDepthStencil)));

	bRequiresStencilTest = bInRequiresStencilTest;

	// Allocate a stencil buffer if needed and not already allocated
	if (bDepthStencilStale)
	{
		RecreateDepthBuffer_RenderThread();
	}
}

void FViewportInfo::RecreateDepthBuffer_RenderThread()
{
	check(IsInRenderingThread());
	DepthStencil.SafeRelease();
	if (bRequiresStencilTest)
	{
		ETextureCreateFlags TargetableTextureFlags = TexCreate_DepthStencilTargetable;
		if (CVarMemorylessDepthStencil.GetValueOnAnyThread() != 0)
		{
			// Use Memoryless target, expecting that DepthStencil content is intermediate and can't be preserved between renderpasses
			TargetableTextureFlags|= TexCreate_Memoryless;
		}
		
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("SlateViewportDepthStencil"))
			.SetExtent(Width, Height)
			.SetFormat(PF_DepthStencil)
			.SetFlags(TargetableTextureFlags | ETextureCreateFlags::ShaderResource)
			.SetInitialState(ERHIAccess::SRVMask)
			.SetClearValue(FClearValueBinding::DepthZero);

		DepthStencil = RHICreateTexture(Desc);
		check(IsValidRef(DepthStencil));
	}
}

bool IsMemorylessTexture(const FTexture2DRHIRef& Tex)
{
	if (Tex)
	{
		return EnumHasAnyFlags(Tex->GetFlags(), TexCreate_Memoryless);
	}
	return false;
}


FSlateRHIRenderer::FSlateRHIRenderer(TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager)
	: FSlateRenderer(InSlateFontServices)
	, EnqueuedWindowDrawBuffer(NULL)
	, FreeBufferIndex(0)
	, FastPathRenderingDataCleanupList(nullptr)
	, bUpdateHDRDisplayInformation(false)
	, CurrentSceneIndex(-1)
	, ResourceVersion(0)
{
	ResourceManager = InResourceManager;

	ViewMatrix = FMatrix(FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 1, 0),
		FPlane(0, 0, 0, 1));

	for (uint64& LastFramePostBufferUsed : LastFramesPostBufferUsed)
	{
		LastFramePostBufferUsed = 0;
	}

	bTakingAScreenShot = false;
	OutScreenshotData = NULL;
	OutHDRScreenshotData = NULL;
	ScreenshotViewportInfo = nullptr;
	bIsStandaloneStereoOnlyDevice = IHeadMountedDisplayModule::IsAvailable() && IHeadMountedDisplayModule::Get().IsStandaloneStereoOnlyDevice();
	bShrinkPostBufferRequested = ESlatePostRT::None;
}

FSlateRHIRenderer::~FSlateRHIRenderer()
{
}

FMatrix FSlateRHIRenderer::CreateProjectionMatrix(uint32 Width, uint32 Height)
{
	// Create ortho projection matrix
	const float Left = 0;
	const float Right = Left + Width;
	const float Top = 0;
	const float Bottom = Top + Height;
	const float ZNear = -100.0f;
	const float ZFar = 100.0f;
	return AdjustProjectionMatrixForRHI(
		FMatrix(
			FPlane(2.0f / (Right - Left), 0, 0, 0),
			FPlane(0, 2.0f / (Top - Bottom), 0, 0),
			FPlane(0, 0, 1 / (ZNear - ZFar), 0),
			FPlane((Left + Right) / (Left - Right), (Top + Bottom) / (Bottom - Top), ZNear / (ZNear - ZFar), 1)
		)
	);
}

int32 FSlateRHIRenderer::GetDrawToVRRenderTarget()
{
	return CVarDrawToVRRenderTarget->GetInt();
}

int32 FSlateRHIRenderer::GetProcessSlatePostBuffers()
{
	return CVarCopyBackbufferToSlatePostRenderTargets->GetInt();
}

bool FSlateRHIRenderer::Initialize()
{
	LoadUsedTextures();

	RenderingPolicy = MakeShareable(new FSlateRHIRenderingPolicy(SlateFontServices.ToSharedRef(), ResourceManager.ToSharedRef()));

	ElementBatcher = MakeUnique<FSlateElementBatcher>(RenderingPolicy.ToSharedRef());

	CurrentSceneIndex = -1;
	ActiveScenes.Empty();
	return true;
}

void FSlateRHIRenderer::Destroy()
{
	RenderingPolicy->ReleaseResources();
	ResourceManager->ReleaseResources();
	SlateFontServices->ReleaseResources();

	for (TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It)
	{
		BeginReleaseResource(It.Value());
	}

	if (FastPathRenderingDataCleanupList)
	{
		FastPathRenderingDataCleanupList->Cleanup();
		FastPathRenderingDataCleanupList = nullptr;
	}

	FlushRenderingCommands();

	ElementBatcher.Reset();
	RenderingPolicy.Reset();
	ResourceManager.Reset();
	SlateFontServices.Reset();

	DeferredUpdateContexts.Empty();

	for (TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It)
	{
		FViewportInfo* ViewportInfo = It.Value();
		delete ViewportInfo;
	}

	WindowToViewportInfo.Empty();
	CurrentSceneIndex = -1;
	ActiveScenes.Empty();
}

/** Returns a draw buffer that can be used by Slate windows to draw window elements */
FSlateDrawBuffer& FSlateRHIRenderer::AcquireDrawBuffer()
{
	FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;

	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];

	while (!Buffer->Lock())
	{
		// If the buffer cannot be locked then the buffer is still in use.  If we are here all buffers are in use
		// so wait until one is free.
		if (IsInSlateThread())
		{
			// We can't flush commands on the slate thread, so simply spinlock until we're done
			// this happens if the render thread becomes completely blocked by expensive tasks when the Slate thread is running
			// in this case we cannot tick Slate.
			FPlatformProcess::Sleep(0.001f);
		}
		else
		{
			FlushCommands();
			UE_LOG(LogSlate, Warning, TEXT("Slate: Had to block on waiting for a draw buffer"));
			FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
		}


		Buffer = &DrawBuffers[FreeBufferIndex];
	}

	// Safely remove brushes by emptying the array and releasing references
	DynamicBrushesToRemove[FreeBufferIndex].Empty();

	Buffer->ClearBuffer();
	Buffer->UpdateResourceVersion(ResourceVersion);
	return *Buffer;
}

void FSlateRHIRenderer::ReleaseDrawBuffer(FSlateDrawBuffer& InWindowDrawBuffer)
{
#if DO_CHECK
	bool bFound = false;
	for (int32 Index = 0; Index < NumDrawBuffers; ++Index)
	{
		if (&DrawBuffers[Index] == &InWindowDrawBuffer)
		{
			bFound = true;
			break;
		}
	}
	ensureMsgf(bFound, TEXT("It release a DrawBuffer that is not a member of the SlateRHIRenderer"));
#endif

	FSlateDrawBuffer* DrawBuffer = &InWindowDrawBuffer;
	ENQUEUE_RENDER_COMMAND(SlateReleaseDrawBufferCommand)(
		[DrawBuffer](FRHICommandListImmediate& RHICmdList)
		{
			FSlateReleaseDrawBufferCommand::ReleaseDrawBuffer(RHICmdList, DrawBuffer);
		}
	);
}

void FSlateRHIRenderer::CreateViewport(const TSharedRef<SWindow> Window)
{
	FlushRenderingCommands();

	if (!WindowToViewportInfo.Contains(&Window.Get()))
	{
		const FVector2f WindowSize = UE::Slate::CastToVector2f(Window->GetViewportSize());

		// Clamp the window size to a reasonable default anything below 8 is a d3d warning and 8 is used anyway.
		// @todo Slate: This is a hack to work around menus being summoned with 0,0 for window size until they are ticked.
		int32 Width = FMath::Max(MIN_VIEWPORT_SIZE, FMath::CeilToInt(WindowSize.X));
		int32 Height = FMath::Max(MIN_VIEWPORT_SIZE, FMath::CeilToInt(WindowSize.Y));

		// Sanity check dimensions
		if (!ensureMsgf(Width <= MAX_VIEWPORT_SIZE && Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), Width, Height))
		{
			Width = FMath::Clamp(Width, MIN_VIEWPORT_SIZE, MAX_VIEWPORT_SIZE);
			Height = FMath::Clamp(Height, MIN_VIEWPORT_SIZE, MAX_VIEWPORT_SIZE);
		}


		FViewportInfo* NewInfo = new FViewportInfo();
		// Create Viewport RHI if it doesn't exist (this must be done on the game thread)
		TSharedRef<FGenericWindow> NativeWindow = Window->GetNativeWindow().ToSharedRef();
		NewInfo->OSWindow = NativeWindow->GetOSWindowHandle();
		NewInfo->Width = Width;
		NewInfo->Height = Height;
		NewInfo->DesiredWidth = Width;
		NewInfo->DesiredHeight = Height;
		NewInfo->ProjectionMatrix = CreateProjectionMatrix( Width, Height );
		// In MobileLDR case backbuffer format should match or be compatible with a SceneColor format in FSceneRenderTargets::GetDesiredMobileSceneColorFormat()
		if (bIsStandaloneStereoOnlyDevice || (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1 && !IsMobileHDR()))
		{
			NewInfo->PixelFormat = GetSlateRecommendedColorFormat();
		}
#if ALPHA_BLENDED_WINDOWS		
		if (Window->GetTransparencySupport() == EWindowTransparency::PerPixel)
		{
			NewInfo->PixelFormat = GetSlateRecommendedColorFormat();
		}
#endif

		// SDR format holds the requested format in non HDR mode
		NewInfo->SDRPixelFormat = NewInfo->PixelFormat;
		HDRGetMetaData(NewInfo->HDRDisplayOutputFormat, NewInfo->HDRDisplayColorGamut, NewInfo->bSceneHDREnabled, Window->GetPositionInScreen(), Window->GetPositionInScreen() + Window->GetSizeInScreen(), NewInfo->OSWindow);

		if (NewInfo->bSceneHDREnabled)
		{
			NewInfo->PixelFormat = GRHIHDRDisplayOutputFormat;
		}

		// Sanity check dimensions
		checkf(Width <= MAX_VIEWPORT_SIZE && Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), Width, Height);

		bool bFullscreen = IsViewportFullscreen( *Window );
		NewInfo->ViewportRHI = RHICreateViewport( NewInfo->OSWindow, Width, Height, bFullscreen, NewInfo->PixelFormat );
		NewInfo->bFullscreen = bFullscreen;

		// Was the window created on a HDR compatible display?
		NewInfo->bHDREnabled = RHIGetColorSpace(NewInfo->ViewportRHI) != EColorSpaceAndEOTF::ERec709_sRGB ;
		Window->SetIsHDR(NewInfo->bHDREnabled);

		WindowToViewportInfo.Add(&Window.Get(), NewInfo);

		BeginInitResource(NewInfo);
	}
}

void FSlateRHIRenderer::ConditionalResizeViewport(FViewportInfo* ViewInfo, uint32 Width, uint32 Height, bool bFullscreen, SWindow* Window)
{
	checkSlow(IsThreadSafeForSlateRendering());

	// Force update if HDR output state changes

	bool bHDREnabled = IsHDREnabled();
	EDisplayColorGamut HDRColorGamut = HDRGetDefaultDisplayColorGamut();
	EDisplayOutputFormat HDROutputDevice = HDRGetDefaultDisplayOutputFormat();

	bool bHDRStale = false;
	if (ViewInfo)
	{
		HDRGetMetaData(HDROutputDevice, HDRColorGamut, bHDREnabled, Window->GetPositionInScreen(), Window->GetPositionInScreen() + Window->GetSizeInScreen(), ViewInfo->OSWindow);
		bHDRStale |= HDROutputDevice != ViewInfo->HDRDisplayOutputFormat;
		bHDRStale |= HDRColorGamut != ViewInfo->HDRDisplayColorGamut;
		bHDRStale |= bHDREnabled != ViewInfo->bSceneHDREnabled;
	}

	if (IsInGameThread() && !IsInSlateThread() && ViewInfo && (bHDRStale || ViewInfo->Height != Height || ViewInfo->Width != Width || ViewInfo->bFullscreen != bFullscreen || !IsValidRef(ViewInfo->ViewportRHI)))
	{
		// The viewport size we have doesn't match the requested size of the viewport.
		// Resize it now.

		// Prevent the texture update logic to use the RHI while the viewport is resized. 
		// This could happen if a streaming IO request completes and throws a callback.
		// @todo : this does not in fact stop texture tasks from using the RHI while the viewport is resized
		//		because they can be running in other threads, or even in retraction on this thread inside the D3D Wait
		//		this should be removed and whatever streaming thread safety is needed during a viewport resize should be done correctly
		SuspendTextureStreamingRenderTasks();

		// cannot resize the viewport while potentially using it.
		FlushRenderingCommands();

		// Windows are allowed to be zero sized ( sometimes they are animating to/from zero for example)
		// but viewports cannot be zero sized.  Use 8x8 as a reasonably sized viewport in this case.
		uint32 NewWidth = FMath::Max<uint32>(8, Width);
		uint32 NewHeight = FMath::Max<uint32>(8, Height);

		// Sanity check dimensions
		if (NewWidth > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport width size to %d.  Clamping size to max allowed size of %d instead."), NewWidth, MAX_VIEWPORT_SIZE);
			NewWidth = MAX_VIEWPORT_SIZE;
		}

		if (NewHeight > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport height size to %d.  Clamping size to max allowed size of %d instead."), NewHeight, MAX_VIEWPORT_SIZE);
			NewHeight = MAX_VIEWPORT_SIZE;
		}

		ViewInfo->Width = NewWidth;
		ViewInfo->Height = NewHeight;
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
		ViewInfo->ProjectionMatrix = CreateProjectionMatrix(NewWidth, NewHeight);
		ViewInfo->bFullscreen = bFullscreen;

		ViewInfo->PixelFormat = bHDREnabled ? GRHIHDRDisplayOutputFormat : ViewInfo->SDRPixelFormat;
		ViewInfo->HDRDisplayColorGamut = HDRColorGamut;
		ViewInfo->HDRDisplayOutputFormat = HDROutputDevice;
		ViewInfo->bSceneHDREnabled = bHDREnabled;

		PreResizeBackBufferDelegate.Broadcast(&ViewInfo->ViewportRHI);
		if (IsValidRef(ViewInfo->ViewportRHI))
		{
			ensureMsgf(ViewInfo->ViewportRHI->GetRefCount() == 1, TEXT("Viewport backbuffer was not properly released"));
			RHIResizeViewport(ViewInfo->ViewportRHI, NewWidth, NewHeight, bFullscreen, ViewInfo->PixelFormat);
		}
		else
		{
			ViewInfo->ViewportRHI = RHICreateViewport(ViewInfo->OSWindow, NewWidth, NewHeight, bFullscreen, ViewInfo->PixelFormat);
		}

		PostResizeBackBufferDelegate.Broadcast(&ViewInfo->ViewportRHI);

		// Reset texture streaming texture updates.
		ResumeTextureStreamingRenderTasks();

		// when the window's state for HDR changed, we need to invalidate the window to make sure the viewport will end up in the appropriate FSlateBatchData, see FSlateElementBatcher::AddViewportElement
		if (bHDRStale)
		{
			Window->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
}

void FSlateRHIRenderer::OnVirtualDesktopSizeChanged(const FDisplayMetrics& NewDisplayMetric)
{
	// Defer the update to as we need to call FlushRenderingCommands() before sending the event to the RHI. 
	// FlushRenderingCommands -> FRenderCommandFence::IsFenceComplete -> CheckRenderingThreadHealth -> FPlatformApplicationMisc::PumpMessages
	// The Display change event is not been consumed yet, and we do BroadcastDisplayMetricsChanged -> OnVirtualDesktopSizeChanged again
	bUpdateHDRDisplayInformation = true;
}

void FSlateRHIRenderer::UpdateFullscreenState(const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY)
{
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef(&Window.Get());

	if (!ViewInfo)
	{
		CreateViewport(Window);
	}

	ViewInfo = WindowToViewportInfo.FindRef(&Window.Get());

	if (ViewInfo)
	{
		const bool bFullscreen = IsViewportFullscreen(*Window);

		uint32 ResX = OverrideResX ? OverrideResX : GSystemResolution.ResX;
		uint32 ResY = OverrideResY ? OverrideResY : GSystemResolution.ResY;

		bool bIsRenderingStereo = GEngine && GEngine->XRSystem.IsValid() && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
		if ((GIsEditor && Window->IsViewportSizeDrivenByWindow()) || (Window->GetWindowMode() == EWindowMode::WindowedFullscreen) || bIsRenderingStereo)
		{
			ResX = ViewInfo->DesiredWidth;
			ResY = ViewInfo->DesiredHeight;
		}

		ConditionalResizeViewport(ViewInfo, ResX, ResY, bFullscreen, &Window.Get());
	}
}

void FSlateRHIRenderer::SetSystemResolution(uint32 Width, uint32 Height)
{
	FSystemResolution::RequestResolutionChange(Width, Height, FPlatformProperties::HasFixedResolution() ? EWindowMode::Fullscreen : GSystemResolution.WindowMode);
	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void FSlateRHIRenderer::RestoreSystemResolution(const TSharedRef<SWindow> InWindow)
{
	if (!GIsEditor && InWindow->GetWindowMode() == EWindowMode::Fullscreen)
	{
		// Force the window system to resize the active viewport, even though nothing might have appeared to change.
		// On windows, DXGI might change the window resolution behind our backs when we alt-tab out. This will make
		// sure that we are actually in the resolution we think we are.
		GSystemResolution.ForceRefresh();
	}
}

/** Called when a window is destroyed to give the renderer a chance to free resources */
void FSlateRHIRenderer::OnWindowDestroyed(const TSharedRef<SWindow>& InWindow)
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo** ViewportInfoPtr = WindowToViewportInfo.Find(&InWindow.Get());
	if (ViewportInfoPtr)
	{
		OnSlateWindowDestroyedDelegate.Broadcast(&(*ViewportInfoPtr)->ViewportRHI);

		// Need to flush rendering commands as the viewport may be in use by the render thread
		// and the rendering resources must be released on the render thread before the viewport can be deleted
		FlushRenderingCommands();

		BeginReleaseResource(*ViewportInfoPtr);

		// Flush rendering commands again so that the resource deletion request is processed.
		FlushRenderingCommands();

		delete *ViewportInfoPtr;
	}

	WindowToViewportInfo.Remove(&InWindow.Get());
}

/** Called when a window is Finished being Reshaped - Currently need to check if its HDR status has changed */
void FSlateRHIRenderer::OnWindowFinishReshaped(const TSharedPtr<SWindow>& InWindow)
{
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef(InWindow.Get());
	RHICheckViewportHDRStatus(ViewInfo->ViewportRHI);
}

// Limited platform support for HDR UI composition
bool SupportsUICompositionRendering(const EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform));
}

// Pixel shader to generate LUT for HDR UI composition
class FCompositeLUTGenerationPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeLUTGenerationPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsUICompositionRendering(Parameters.Platform);
	}

	FCompositeLUTGenerationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(Initializer.ParameterMap, TEXT("OutputGamut"));
		OutputMaxLuminance.Bind(Initializer.ParameterMap, TEXT("OutputMaxLuminance"));
		ACESMinMaxData.Bind(Initializer.ParameterMap, TEXT("ACESMinMaxData"));
		ACESMidData.Bind(Initializer.ParameterMap, TEXT("ACESMidData"));
		ACESCoefsLow_0.Bind(Initializer.ParameterMap, TEXT("ACESCoefsLow_0"));
		ACESCoefsHigh_0.Bind(Initializer.ParameterMap, TEXT("ACESCoefsHigh_0"));
		ACESCoefsLow_4.Bind(Initializer.ParameterMap, TEXT("ACESCoefsLow_4"));
		ACESCoefsHigh_4.Bind(Initializer.ParameterMap, TEXT("ACESCoefsHigh_4"));
		ACESSceneColorMultiplier.Bind(Initializer.ParameterMap, TEXT("ACESSceneColorMultiplier"));
	}
	FCompositeLUTGenerationPS() {}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, EDisplayOutputFormat DisplayOutputFormat, EDisplayColorGamut DisplayColorGamut, float DisplayMaxLuminance)
	{
		static const auto CVarOutputGamma = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));

		int32 OutputDeviceValue = (int32)DisplayOutputFormat;
		int32 OutputGamutValue = (int32)DisplayColorGamut;
		float Gamma = CVarOutputGamma->GetValueOnRenderThread();

        // In case gamma is unspecified, fall back to 2.2 which is the most common case
		if ((PLATFORM_APPLE || DisplayOutputFormat == EDisplayOutputFormat::SDR_ExplicitGammaMapping) && Gamma == 0.0f)
		{
			Gamma = 2.2f;
		}

		if (Gamma > 0.0f)
		{
			// Enforce user-controlled ramp over sRGB or Rec709
			OutputDeviceValue = FMath::Max(OutputDeviceValue, (int32)EDisplayOutputFormat::SDR_ExplicitGammaMapping);
		}

		SetShaderValue(BatchedParameters, OutputDevice, OutputDeviceValue);
		SetShaderValue(BatchedParameters, OutputGamut, OutputGamutValue);
		SetShaderValue(BatchedParameters, OutputMaxLuminance, DisplayMaxLuminance);

		FACESTonemapParams TmpInternalACESParams;
		GetACESTonemapParameters(TmpInternalACESParams);

		SetShaderValue(BatchedParameters, ACESMinMaxData, TmpInternalACESParams.ACESMinMaxData);
		SetShaderValue(BatchedParameters, ACESMidData, TmpInternalACESParams.ACESMidData);
		SetShaderValue(BatchedParameters, ACESCoefsLow_0, TmpInternalACESParams.ACESCoefsLow_0);
		SetShaderValue(BatchedParameters, ACESCoefsHigh_0, TmpInternalACESParams.ACESCoefsHigh_0);
		SetShaderValue(BatchedParameters, ACESCoefsLow_4, TmpInternalACESParams.ACESCoefsLow_4);
		SetShaderValue(BatchedParameters, ACESCoefsHigh_4, TmpInternalACESParams.ACESCoefsHigh_4);
		SetShaderValue(BatchedParameters, ACESSceneColorMultiplier, TmpInternalACESParams.ACESSceneColorMultiplier);

	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/CompositeUIPixelShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

private:
	LAYOUT_FIELD(FShaderParameter, OutputDevice);
	LAYOUT_FIELD(FShaderParameter, OutputGamut);
	LAYOUT_FIELD(FShaderParameter, OutputMaxLuminance);
	LAYOUT_FIELD(FShaderParameter, ACESMinMaxData) // xy = min ACES/luminance, zw = max ACES/luminance
	LAYOUT_FIELD(FShaderParameter, ACESMidData) // x = mid ACES, y = mid luminance, z = mid slope
	LAYOUT_FIELD(FShaderParameter, ACESCoefsLow_0) // coeflow 0-3
	LAYOUT_FIELD(FShaderParameter, ACESCoefsHigh_0) // coefhigh 0-3
	LAYOUT_FIELD(FShaderParameter, ACESCoefsLow_4)
	LAYOUT_FIELD(FShaderParameter, ACESCoefsHigh_4)
	LAYOUT_FIELD(FShaderParameter, ACESSceneColorMultiplier)
};

IMPLEMENT_SHADER_TYPE(, FCompositeLUTGenerationPS, TEXT("/Engine/Private/CompositeUIPixelShader.usf"), TEXT("GenerateLUTPS"), SF_Pixel);

// Pixel shader to composite UI over HDR buffer
class FCompositeShaderBase : public FGlobalShader
{
	DECLARE_TYPE_LAYOUT(FCompositeShaderBase, NonVirtual);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsUICompositionRendering(Parameters.Platform);
	}

	FCompositeShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		UITexture.Bind(Initializer.ParameterMap, TEXT("UITexture"));
		UIWriteMaskTexture.Bind(Initializer.ParameterMap, TEXT("UIWriteMaskTexture"));
		UISampler.Bind(Initializer.ParameterMap, TEXT("UISampler"));
		ColorSpaceLUT.Bind(Initializer.ParameterMap, TEXT("ColorSpaceLUT"));
		ColorSpaceLUTSampler.Bind(Initializer.ParameterMap, TEXT("ColorSpaceLUTSampler"));
		UILevel.Bind(Initializer.ParameterMap, TEXT("UILevel"));
		UILuminance.Bind(Initializer.ParameterMap, TEXT("UILuminance"));
		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
		ColorVisionDeficiencyType.Bind(Initializer.ParameterMap, TEXT("ColorVisionDeficiencyType"));
		ColorVisionDeficiencySeverity.Bind(Initializer.ParameterMap, TEXT("ColorVisionDeficiencySeverity"));
		bCorrectDeficiency.Bind(Initializer.ParameterMap, TEXT("bCorrectDeficiency"));
		bSimulateCorrectionWithDeficiency.Bind(Initializer.ParameterMap, TEXT("bSimulateCorrectionWithDeficiency"));
	}
	FCompositeShaderBase() = default;

	void SetParametersBase(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* UITextureRHI, FRHITexture* UITextureWriteMaskRHI, FRHITexture* ColorSpaceLUTRHI)
	{
		static const auto CVarOutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));

		SetTextureParameter(BatchedParameters, UITexture, UISampler, TStaticSamplerState<SF_Point>::GetRHI(), UITextureRHI);
		SetTextureParameter(BatchedParameters, ColorSpaceLUT, ColorSpaceLUTSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), ColorSpaceLUTRHI);
		SetShaderValue(BatchedParameters, UILevel, CVarUILevel.GetValueOnRenderThread());
		SetShaderValue(BatchedParameters, OutputDevice, CVarOutputDevice->GetValueOnRenderThread());
		SetShaderValue(BatchedParameters, UILuminance, CVarHDRUILuminance.GetValueOnRenderThread());

		if (UITextureWriteMaskRHI != nullptr)
		{
			SetTextureParameter(BatchedParameters, UIWriteMaskTexture, UITextureWriteMaskRHI);
		}
	}

	void SetColorDeficiencyParamsBase(FRHIBatchedShaderParameters& BatchedParameters, bool bCorrect, EColorVisionDeficiency DeficiencyType, int32 Severity, bool bShowCorrectionWithDeficiency)
	{
		SetShaderValue(BatchedParameters, ColorVisionDeficiencyType, (float)DeficiencyType);
		SetShaderValue(BatchedParameters, ColorVisionDeficiencySeverity, (float)Severity);
		SetShaderValue(BatchedParameters, bCorrectDeficiency, bCorrect ? 1.0f : 0.0f);
		SetShaderValue(BatchedParameters, bSimulateCorrectionWithDeficiency, bShowCorrectionWithDeficiency ? 1.0f : 0.0f);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/CompositeUIPixelShader.usf");
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, UITexture);
	LAYOUT_FIELD(FShaderResourceParameter, UIWriteMaskTexture);
	LAYOUT_FIELD(FShaderResourceParameter, UISampler);
	LAYOUT_FIELD(FShaderResourceParameter, ColorSpaceLUT);
	LAYOUT_FIELD(FShaderResourceParameter, ColorSpaceLUTSampler);
	LAYOUT_FIELD(FShaderParameter, UILevel);
	LAYOUT_FIELD(FShaderParameter, UILuminance);
	LAYOUT_FIELD(FShaderParameter, OutputDevice);
	LAYOUT_FIELD(FShaderParameter, ColorVisionDeficiencyType);
	LAYOUT_FIELD(FShaderParameter, ColorVisionDeficiencySeverity);
	LAYOUT_FIELD(FShaderParameter, bCorrectDeficiency);
	LAYOUT_FIELD(FShaderParameter, bSimulateCorrectionWithDeficiency);
};
IMPLEMENT_TYPE_LAYOUT(FCompositeShaderBase);

class FCompositePSBase : public FCompositeShaderBase
{
	DECLARE_TYPE_LAYOUT(FCompositePSBase, NonVirtual);
public:
	FCompositePSBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FCompositeShaderBase(Initializer)
	{
		SceneTexture.Bind(Initializer.ParameterMap, TEXT("SceneTexture"));
		SceneSampler.Bind(Initializer.ParameterMap, TEXT("SceneSampler"));
	}
	FCompositePSBase() = default;

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		FRHITexture* UITextureRHI,
		FRHITexture* UITextureWriteMaskRHI,
		FRHITexture* SceneTextureRHI,
		FRHITexture* ColorSpaceLUTRHI,
		bool bCorrect,
		EColorVisionDeficiency DeficiencyType,
		int32 Severity,
		bool bShowCorrectionWithDeficiency
	)
	{
		SetParametersBase(BatchedParameters, UITextureRHI, UITextureWriteMaskRHI, ColorSpaceLUTRHI);
		SetTextureParameter(BatchedParameters, SceneTexture, SceneSampler, TStaticSamplerState<SF_Point>::GetRHI(), SceneTextureRHI);
		SetColorDeficiencyParamsBase(BatchedParameters, bCorrect, DeficiencyType, Severity, bShowCorrectionWithDeficiency);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, SceneTexture);
	LAYOUT_FIELD(FShaderResourceParameter, SceneSampler);
};
IMPLEMENT_TYPE_LAYOUT(FCompositePSBase);

template<uint32 EncodingType, bool bApplyColorDeficiency>
class FCompositePS : public FCompositePSBase
{
	DECLARE_SHADER_TYPE(FCompositePS, Global);
public:
	FCompositePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FCompositePSBase(Initializer) {}
	FCompositePS() = default;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SCRGB_ENCODING"), EncodingType);
		OutEnvironment.SetDefine(TEXT("APPLY_COLOR_DEFICIENCY"), uint32(bApplyColorDeficiency ? 1 : 0));
	}
};

typedef FCompositePS<0, false> FCompositePS_PQ;
typedef FCompositePS<0, true> FCompositePS_PQ_CDV;
typedef FCompositePS<1, false> FCompositePS_SCRGB;
typedef FCompositePS<1, true> FCompositePS_SCRGB_CDV;

IMPLEMENT_SHADER_TYPE(template<>, FCompositePS_PQ, FCompositePSBase::GetSourceFilename(), FCompositePSBase::GetFunctionName(), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FCompositePS_PQ_CDV,  FCompositePSBase::GetSourceFilename(), FCompositePSBase::GetFunctionName(), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FCompositePS_SCRGB, FCompositePSBase::GetSourceFilename(), FCompositePSBase::GetFunctionName(), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FCompositePS_SCRGB_CDV,  FCompositePSBase::GetSourceFilename(), FCompositePSBase::GetFunctionName(), SF_Pixel);

class FCompositeCSBase : public FCompositeShaderBase
{
	DECLARE_TYPE_LAYOUT(FCompositeCSBase, NonVirtual);
public:

	static const uint32 NUM_THREADS_PER_GROUP = 16;

	static bool IsShaderSupported(const EShaderPlatform ShaderPlatform)
	{
		return RHISupports4ComponentUAVReadWrite(ShaderPlatform) && RHISupportsSwapchainUAVs(ShaderPlatform);
	}

	FCompositeCSBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FCompositeShaderBase(Initializer)
	{
		RWSceneTexture.Bind(Initializer.ParameterMap, TEXT("RWSceneTexture"));
		SceneTextureDimensions.Bind(Initializer.ParameterMap, TEXT("SceneTextureDimensions"));
	}
	FCompositeCSBase() = default;

	static const TCHAR* GetFunctionName()
	{
		return TEXT("CompositeUICS");
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		FRHITexture* UITextureRHI,
		FRHITexture* UITextureWriteMaskRHI,
		FRHIUnorderedAccessView* InRWSceneTexture,
		FRHITexture* ColorSpaceLUTRHI,
		const FVector4f& InSceneTextureDimensions,
		bool bCorrect,
		EColorVisionDeficiency DeficiencyType,
		int32 Severity,
		bool bShowCorrectionWithDeficiency
	)
	{
		SetParametersBase(BatchedParameters, UITextureRHI, UITextureWriteMaskRHI, ColorSpaceLUTRHI);
		SetUAVParameter(BatchedParameters, RWSceneTexture, InRWSceneTexture);
		SetShaderValue(BatchedParameters, SceneTextureDimensions, InSceneTextureDimensions);
		SetColorDeficiencyParamsBase(BatchedParameters, bCorrect, DeficiencyType, Severity, bShowCorrectionWithDeficiency);
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, RWSceneTexture);
	LAYOUT_FIELD(FShaderParameter, SceneTextureDimensions);
};
IMPLEMENT_TYPE_LAYOUT(FCompositeCSBase);

template<uint32 EncodingType, bool bApplyColorDeficiency>
class FCompositeCS : public FCompositeCSBase
{
	DECLARE_SHADER_TYPE(FCompositeCS, Global);
public:
	FCompositeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FCompositeCSBase(Initializer) {}
	FCompositeCS() = default;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!SupportsUICompositionRendering(Parameters.Platform))
		{
			return false;
		}

		return IsShaderSupported(Parameters.Platform);
	}


	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_COMPUTE_FOR_COMPOSITION"), 1);
		OutEnvironment.SetDefine(TEXT("SCRGB_ENCODING"), EncodingType);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NUM_THREADS_PER_GROUP);
		OutEnvironment.SetDefine(TEXT("APPLY_COLOR_DEFICIENCY"), uint32(bApplyColorDeficiency ? 1 : 0));
	}
};

typedef FCompositeCS<0, false> FCompositeCS_PQ;
typedef FCompositeCS<0, true>  FCompositeCS_PQ_CDV;
typedef FCompositeCS<1, false> FCompositeCS_SCRGB;
typedef FCompositeCS<1, true>  FCompositeCS_SCRGB_CDV;

IMPLEMENT_SHADER_TYPE(template<>, FCompositeCS_PQ,        FCompositeCSBase::GetSourceFilename(), FCompositeCSBase::GetFunctionName(), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FCompositeCS_PQ_CDV,    FCompositeCSBase::GetSourceFilename(), FCompositeCSBase::GetFunctionName(), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FCompositeCS_SCRGB,     FCompositeCSBase::GetSourceFilename(), FCompositeCSBase::GetFunctionName(), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FCompositeCS_SCRGB_CDV, FCompositeCSBase::GetSourceFilename(), FCompositeCSBase::GetFunctionName(), SF_Compute);


int32 SlateWireFrame = 0;
static FAutoConsoleVariableRef CVarSlateWireframe(TEXT("Slate.ShowWireFrame"), SlateWireFrame, TEXT(""), ECVF_Default);

void RenderSlateBatch(FTexture2DRHIRef SlateRenderTarget, bool bClear, bool bIsHDR, FViewportInfo& ViewportInfo, const FMatrix& ViewMatrix, FSlateBatchData& BatchData, FRHICommandListImmediate& RHICmdList,
				      const uint32 ViewportWidth, const uint32 ViewportHeight, const struct FSlateDrawWindowCommandParams& DrawCommandParams, TSharedPtr<FSlateRHIRenderingPolicy> RenderingPolicy, 
					  FTexture2DRHIRef PostProcessBuffer)
{
	FRHIRenderPassInfo RPInfo(SlateRenderTarget, ERenderTargetActions::Load_Store);

	if (bClear)
	{
		RPInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
	}

	if (ViewportInfo.bRequiresStencilTest)
	{
		check(IsValidRef(ViewportInfo.DepthStencil));

		ERenderTargetActions StencilAction = IsMemorylessTexture(ViewportInfo.DepthStencil) ? ERenderTargetActions::DontLoad_DontStore : ERenderTargetActions::DontLoad_Store;
		RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::DontLoad_DontStore, StencilAction);
		RPInfo.DepthStencilRenderTarget.DepthStencilTarget = ViewportInfo.DepthStencil;
		RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthNop_StencilWrite;
	}

#if WITH_SLATE_VISUALIZERS
	if (CVarShowSlateBatching.GetValueOnRenderThread() != 0 || CVarShowSlateOverdraw.GetValueOnRenderThread() != 0)
	{
		RPInfo.ColorRenderTargets[0].Action = ERenderTargetActions::Clear_Store;
		if (ViewportInfo.bRequiresStencilTest)
		{
			// Reset the backbuffer as our color render target and also set a depth stencil buffer
			ERenderTargetActions StencilAction = IsMemorylessTexture(ViewportInfo.DepthStencil) ? ERenderTargetActions::Clear_DontStore : ERenderTargetActions::Clear_Store;
			RPInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(ERenderTargetActions::Load_Store, StencilAction);
			RPInfo.DepthStencilRenderTarget.DepthStencilTarget = ViewportInfo.DepthStencil;
			RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
		}
	}
#endif
	{
		bool bHasBatches = BatchData.GetRenderBatches().Num() > 0;
		if (bHasBatches || bClear)
		{
			TransitionRenderPassTargets(RHICmdList, RPInfo);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateBatches"));
			SCOPE_CYCLE_COUNTER(STAT_SlateRTDrawBatches);

			if (bHasBatches)
			{
				FSlateBackBuffer SlateBackBuffer(SlateRenderTarget, FIntPoint(ViewportWidth, ViewportHeight));

				FSlateRenderingParams RenderParams(ViewMatrix * ViewportInfo.ProjectionMatrix, DrawCommandParams.Time);
				RenderParams.bWireFrame = !!SlateWireFrame;
				RenderParams.bIsHDR = bIsHDR;
				RenderParams.HDRDisplayColorGamut = ViewportInfo.HDRDisplayColorGamut;
				RenderParams.ViewRect = DrawCommandParams.ViewRect;
				RenderParams.UsedSlatePostBuffers = DrawCommandParams.UsedSlatePostBuffers;
				if (ViewportInfo.bSceneHDREnabled && !bIsHDR)
				{
					RenderParams.UITarget = ViewportInfo.UITargetRT;
				}
				RenderingPolicy->SetUseGammaCorrection(!bIsHDR);

				FTexture2DRHIRef EmptyTarget;

				RenderingPolicy->DrawElements
				(
					RHICmdList,
					SlateBackBuffer,
					SlateRenderTarget,
					PostProcessBuffer,
					ViewportInfo.bRequiresStencilTest ? ViewportInfo.DepthStencil : EmptyTarget,
					BatchData.GetFirstRenderBatchIndex(),
					BatchData.GetRenderBatches(),
					RenderParams
				);
			}
		}
	}

	// @todo Could really use a refactor.
	// Kind of gross but we don't want to restart renderpasses for no reason.
	// If the color deficiency shaders are active within DrawElements there will not be a renderpass here.
	// In the general case there will be a RenderPass active at this point.
	if (RHICmdList.IsInsideRenderPass())
	{
		RHICmdList.EndRenderPass();
	}
}

inline bool CompositeUIWithHdrRenderTarget(const FViewportInfo* ViewInfo)
{
	// Optional off-screen UI composition during HDR rendering
	static const auto CVarCompositeMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.UI.CompositeMode"));

	const bool bSupportsUIComposition = GRHISupportsHDROutput && GSupportsVolumeTextureRendering && SupportsUICompositionRendering(GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel));
	const bool bCompositeUI = bSupportsUIComposition
		&& CVarCompositeMode && CVarCompositeMode->GetValueOnAnyThread() != 0
		&& ViewInfo->bSceneHDREnabled;

	return bCompositeUI;
}

/** Draws windows from a FSlateDrawBuffer on the render thread */
void FSlateRHIRenderer::DrawWindow_RenderThread(FRHICommandListImmediate& RHICmdList, FViewportInfo& ViewportInfo, FSlateWindowElementList& WindowElementList, const struct FSlateDrawWindowCommandParams& DrawCommandParams)
{
	LLM_SCOPE(ELLMTag::SceneRender);
	
	bool bRenderOffscreen = false;	// Render to an offscreen texture which can then be finally color converted at the end.
	
#if WITH_EDITOR
	if (RHIGetColorSpace(ViewportInfo.ViewportRHI) != EColorSpaceAndEOTF::ERec709_sRGB)
	{
		bRenderOffscreen = true;
	}
#endif

	static uint32 LastTimestamp = FPlatformTime::Cycles();
	{
		const FRHIGPUMask PresentingGPUMask = FRHIGPUMask::FromIndex(RHIGetViewportNextPresentGPUIndex(ViewportInfo.ViewportRHI));
		SCOPED_GPU_MASK(RHICmdList, PresentingGPUMask);
		SCOPED_DRAW_EVENTF(RHICmdList, SlateUI, TEXT("SlateUI Title = %s"), DrawCommandParams.WindowTitle.IsEmpty() ? TEXT("<none>") : *DrawCommandParams.WindowTitle);
		SCOPED_GPU_STAT(RHICmdList, SlateUI);
		SCOPED_NAMED_EVENT_TEXT("Slate::DrawWindow_RenderThread", FColor::Magenta);

		// Should only be called by the rendering thread
		check(IsInRenderingThread());

		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();
		GetRendererModule().InitializeSystemTextures(RHICmdList);

		const bool bCompositeUI = CompositeUIWithHdrRenderTarget(&ViewportInfo);

		const int32 CompositionLUTSize = 32;

		// Only need to update LUT on settings change
		const int32 HDROutputDevice = (int32)ViewportInfo.HDRDisplayOutputFormat;
		const int32 HDROutputGamut = (int32)ViewportInfo.HDRDisplayColorGamut;

		bool bLUTStale = ViewportInfo.ColorSpaceLUTOutputDevice != HDROutputDevice || ViewportInfo.ColorSpaceLUTOutputGamut != HDROutputGamut;

		ViewportInfo.ColorSpaceLUTOutputDevice = HDROutputDevice;
		ViewportInfo.ColorSpaceLUTOutputGamut = HDROutputGamut;

		bool bRenderedStereo = false;
		if (CVarDrawToVRRenderTarget->GetInt() == 0 && GEngine && IsValidRef(ViewportInfo.GetRenderTargetTexture()) && GEngine->StereoRenderingDevice.IsValid())
		{
			const FVector2D WindowSize = WindowElementList.GetWindowSize();
			GEngine->StereoRenderingDevice->RenderTexture_RenderThread(RHICmdList, RHIGetViewportBackBuffer(ViewportInfo.ViewportRHI), ViewportInfo.GetRenderTargetTexture(), WindowSize);
			bRenderedStereo = true;
		}

		{
			SCOPED_GPU_STAT(RHICmdList, SlateUI);
			SCOPE_CYCLE_COUNTER(STAT_SlateRenderingRTTime);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Slate);

			// should have been created by the game thread
			check(IsValidRef(ViewportInfo.ViewportRHI));

			FTexture2DRHIRef ViewportRT = bRenderedStereo ? nullptr : ViewportInfo.GetRenderTargetTexture();
			FTexture2DRHIRef BackBuffer = (ViewportRT) ? ViewportRT : RHIGetViewportBackBuffer(ViewportInfo.ViewportRHI);
			FTexture2DRHIRef PostProcessBuffer = BackBuffer;	// If compositing UI then this will be different to the back buffer

			const uint32 ViewportWidth = (ViewportRT) ? ViewportRT->GetSizeX() : ViewportInfo.Width;
			const uint32 ViewportHeight = (ViewportRT) ? ViewportRT->GetSizeY() : ViewportInfo.Height;

			// Check to see that targets are up-to-date
			if (bCompositeUI && (!ViewportInfo.UITargetRT || ViewportInfo.UITargetRT->GetRHI()->GetSizeX() != ViewportWidth || ViewportInfo.UITargetRT->GetRHI()->GetSizeY() != ViewportHeight))
			{
				// Composition buffers
				{
					ETextureCreateFlags BaseFlags = RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform) ? TexCreate_NoFastClearFinalize | TexCreate_DisableDCC : TexCreate_None;
					FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(ViewportWidth, ViewportHeight),
						GetSlateRecommendedColorFormat(),
						FClearValueBinding::Transparent,
						BaseFlags,
						TexCreate_ShaderResource | TexCreate_RenderTargetable,
						false,
						1,
						true,
						true));

					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ViewportInfo.UITargetRT, TEXT("UITargetRT"));
				}

				// LUT
				{
					ViewportInfo.ColorSpaceLUT.SafeRelease();

					const FRHITextureCreateDesc Desc =
						FRHITextureCreateDesc::Create3D(TEXT("ColorSpaceLUT"))
						.SetExtent(CompositionLUTSize)
						.SetDepth(CompositionLUTSize)
						.SetFormat(PF_A2B10G10R10)
						.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
						.SetInitialState(ERHIAccess::SRVMask);

					ViewportInfo.ColorSpaceLUT = RHICreateTexture(Desc);
				}

				bLUTStale = true;
			}

			FTexture2DRHIRef FinalBuffer = BackBuffer;

			bool bClear = DrawCommandParams.bClear;
			if (bCompositeUI)
			{
				bClear = true; // Force a clear of the UI buffer to black

#if WITH_EDITOR
				// in editor mode, we actually don't render to the true backbuffer, but to BufferedRT. The Scene image is rendered in the backbuffer with Slate
				// We add it with FSlateElementBatcher::AddViewportElement and render it with RenderSlateBatch
				if (WindowElementList.GetBatchDataHDR().GetRenderBatches().Num() > 0)
				{
					FRHIRenderPassInfo RPInfo(FinalBuffer, ERenderTargetActions::Clear_Store);
					TransitionRenderPassTargets(RHICmdList, RPInfo);
					RHICmdList.BeginRenderPass(RPInfo, TEXT("Clear back buffer"));
					RHICmdList.EndRenderPass();
				}
#endif
				// UI backbuffer is temp target
				BackBuffer = ViewportInfo.UITargetRT->GetRHI();
			}

#if WITH_EDITOR
			TRefCountPtr<IPooledRenderTarget> HDRRenderRT;

			if (bRenderOffscreen )
			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(ViewportWidth, ViewportHeight),
					PF_FloatRGBA,
					FClearValueBinding::Transparent,
					TexCreate_None,
					TexCreate_ShaderResource | TexCreate_RenderTargetable,
					false,
					1,
					true,
					true));

				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, HDRRenderRT, TEXT("HDRTargetRT"));

				BackBuffer = HDRRenderRT->GetRHI();
			}
#endif

			if (SlateWireFrame)
			{
				bClear = true;
			}

			RHICmdList.BeginDrawingViewport(ViewportInfo.ViewportRHI, FTextureRHIRef());
			RHICmdList.SetViewport(0.f, 0.f, 0.f, (float)ViewportWidth, (float)ViewportHeight, 0.0f);

			bool bApplyColorDeficiencyCorrectionToRestore = RenderingPolicy->GetApplyColorDeficiencyCorrection();

			if (bCompositeUI)
			{
				RenderingPolicy->SetApplyColorDeficiencyCorrection(false);
				FSlateBatchData& BatchDataHDR = WindowElementList.GetBatchDataHDR();
				bool bHasBatches = BatchDataHDR.GetRenderBatches().Num() > 0;
				if (bHasBatches)
				{
					RenderingPolicy->BuildRenderingBuffers(RHICmdList, BatchDataHDR);
					FTexture2DRHIRef UITargetHDRRTRHI(FinalBuffer);
					RenderSlateBatch(UITargetHDRRTRHI, /*bClear*/ false, /*bIsHDR*/ true, ViewportInfo, ViewMatrix, BatchDataHDR, RHICmdList, ViewportWidth, ViewportHeight, DrawCommandParams, RenderingPolicy, PostProcessBuffer);
			    }

				PostProcessBuffer = FinalBuffer;
            }

			FSlateBatchData& BatchData = WindowElementList.GetBatchData();

			// Update the vertex and index buffer	
		    RenderingPolicy->BuildRenderingBuffers(RHICmdList, BatchData);

			// This must happen after rendering buffers are created
			ViewportInfo.ConditionallyUpdateDepthBuffer(BatchData.IsStencilClippingRequired(), ViewportInfo.DesiredWidth, ViewportInfo.DesiredHeight);

		    bool bHdrTarget = ViewportInfo.bSceneHDREnabled && !bCompositeUI;
		    RenderSlateBatch(BackBuffer, bClear, bHdrTarget, ViewportInfo, ViewMatrix, BatchData, RHICmdList, ViewportWidth, ViewportHeight, DrawCommandParams, RenderingPolicy, PostProcessBuffer);

			if (bCompositeUI)
			{
				RenderingPolicy->SetApplyColorDeficiencyCorrection(bApplyColorDeficiencyCorrectionToRestore);

				SCOPED_DRAW_EVENT(RHICmdList, SlateUI_Composition);

				static const FName RendererModuleName("Renderer");
				IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

				const auto FeatureLevel = GMaxRHIFeatureLevel;
				auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

				// Generate composition LUT
				if (bLUTStale)
				{
					// #todo-renderpasses will this touch every pixel? use NoAction?
					FRHIRenderPassInfo RPInfo(ViewportInfo.ColorSpaceLUT, ERenderTargetActions::Load_Store);
					RHICmdList.Transition(FRHITransitionInfo(ViewportInfo.ColorSpaceLUT, ERHIAccess::Unknown, ERHIAccess::RTV));
					RHICmdList.BeginRenderPass(RPInfo, TEXT("GenerateLUT"));
					{
						FGraphicsPipelineStateInitializer GraphicsPSOInit;
						RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
						GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
						GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
						GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

						TShaderMapRef<FWriteToSliceVS> VertexShader(ShaderMap);
						TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(ShaderMap);
						TShaderMapRef<FCompositeLUTGenerationPS> PixelShader(ShaderMap);
						const FVolumeBounds VolumeBounds(CompositionLUTSize);

						GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
						GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

						SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(VolumeBounds.MaxX - VolumeBounds.MinX));

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
						if (GeometryShader.IsValid())
						{
							SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
						}
#endif
						const float DisplayMaxLuminance = HDRGetDisplayMaximumLuminance();

						SetShaderParametersLegacyPS(RHICmdList, PixelShader, ViewportInfo.HDRDisplayOutputFormat, ViewportInfo.HDRDisplayColorGamut, DisplayMaxLuminance);

						RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
					}
					RHICmdList.EndRenderPass();
					RHICmdList.Transition(FRHITransitionInfo(ViewportInfo.ColorSpaceLUT, ERHIAccess::RTV, ERHIAccess::SRVMask));
				}

				// Composition pass

				RHICmdList.Transition(FRHITransitionInfo(ViewportInfo.UITargetRT->GetRHI(), ERHIAccess::Unknown, ERHIAccess::SRVMask));

				if (RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform))
				{
					IPooledRenderTarget* RenderTargets[] = { ViewportInfo.UITargetRT.GetReference() };
					FRenderTargetWriteMask::Decode(RHICmdList, ShaderMap, RenderTargets, ViewportInfo.UITargetRTMask, TexCreate_None, TEXT("UIRTWriteMask"));
				}
				FRHITexture* UITargetRTMaskTexture = ViewportInfo.UITargetRTMask.IsValid() ? ViewportInfo.UITargetRTMask->GetRHI() : nullptr;

				bool bUseScRGB = (HDROutputDevice == (int32)EDisplayOutputFormat::HDR_ACES_1000nit_ScRGB || HDROutputDevice == (int32)EDisplayOutputFormat::HDR_ACES_2000nit_ScRGB);
				bool bApplyColorDeficiencyFilter = GSlateColorDeficiencyType != EColorVisionDeficiency::NormalVision && GSlateColorDeficiencySeverity > 0;

				FRHIUnorderedAccessView* BackBufferUAV = (FCompositeCSBase::IsShaderSupported(GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel)) && ViewportRT == nullptr) ? RHIGetViewportBackBufferUAV(ViewportInfo.ViewportRHI) : nullptr;
				if (BackBufferUAV == nullptr)
				{
					TRefCountPtr<IPooledRenderTarget> FinalBufferCopy;
					{
						FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(ViewportWidth, ViewportHeight),
							FinalBuffer->GetFormat(),
							FClearValueBinding::Transparent,
							TexCreate_None,
							TexCreate_ShaderResource,
							false,
							1,
							true,
							true));
						GRenderTargetPool.FindFreeElement(RHICmdList, Desc, FinalBufferCopy, TEXT("FinalBufferCopy"));
					}
					TransitionAndCopyTexture(RHICmdList, FinalBuffer, FinalBufferCopy->GetRHI(), {});

					RHICmdList.Transition({
						FRHITransitionInfo(FinalBuffer, ERHIAccess::Unknown, ERHIAccess::RTV)
					});
					FRHIRenderPassInfo RPInfo(FinalBuffer, ERenderTargetActions::Load_Store);
					RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateComposite"));
					{
						FGraphicsPipelineStateInitializer GraphicsPSOInit;
						RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
						GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
						GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
						GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

						TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

						TShaderRef<FCompositePSBase> PixelShader;
						if (bUseScRGB)
						{
							if (bApplyColorDeficiencyFilter)
							{
								PixelShader = TShaderMapRef<FCompositePS_SCRGB_CDV>(ShaderMap);
							}
							else
							{
								PixelShader = TShaderMapRef<FCompositePS_SCRGB>(ShaderMap);
							}
						}
						else
						{
							if (bApplyColorDeficiencyFilter)
							{
								PixelShader = TShaderMapRef<FCompositePS_PQ_CDV>(ShaderMap);
							}
							else
							{
								PixelShader = TShaderMapRef<FCompositePS_PQ>(ShaderMap);
							}
						}

						GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
						GraphicsPSOInit.PrimitiveType = PT_TriangleList;

						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

						SetShaderParametersLegacyPS(RHICmdList, PixelShader, 
							ViewportInfo.UITargetRT->GetRHI(), UITargetRTMaskTexture, FinalBufferCopy->GetRHI(), ViewportInfo.ColorSpaceLUT,
							GSlateColorDeficiencyCorrection, GSlateColorDeficiencyType, GSlateColorDeficiencySeverity, GSlateShowColorDeficiencyCorrectionWithDeficiency);

						RendererModule.DrawRectangle(
							RHICmdList,
							0.f, 0.f,
							(float)ViewportWidth, (float)ViewportHeight,
							0.f, 0.f,
							(float)ViewportWidth, (float)ViewportHeight,
							FIntPoint(ViewportWidth, ViewportHeight),
							FIntPoint(ViewportWidth, ViewportHeight),
							VertexShader,
							EDRF_UseTriangleOptimization);
					}
					RHICmdList.EndRenderPass();
				}
				else
				{
					TShaderRef<FCompositeCSBase> ComputeShader;
					if (bUseScRGB)
					{
						if (bApplyColorDeficiencyFilter)
						{
							ComputeShader = TShaderMapRef<FCompositeCS_SCRGB_CDV>(ShaderMap);
						}
						else
						{
							ComputeShader = TShaderMapRef<FCompositeCS_SCRGB>(ShaderMap);
						}
					}
					else
					{
						if (bApplyColorDeficiencyFilter)
						{
							ComputeShader = TShaderMapRef<FCompositeCS_PQ_CDV>(ShaderMap);
						}
						else
						{
							ComputeShader = TShaderMapRef<FCompositeCS_PQ>(ShaderMap);
						}
					}

					RHICmdList.Transition({
						FRHITransitionInfo(FinalBuffer, ERHIAccess::Unknown, ERHIAccess::UAVCompute)
					});

					FVector4f SceneTextureDimensions((float)ViewportWidth, (float)ViewportHeight, 1.0f/(float)ViewportWidth, 1.0f/(float)ViewportHeight);
					SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());

					SetShaderParametersLegacyCS(RHICmdList, ComputeShader,
						ViewportInfo.UITargetRT->GetRHI(), UITargetRTMaskTexture, BackBufferUAV, ViewportInfo.ColorSpaceLUT, SceneTextureDimensions,
						GSlateColorDeficiencyCorrection, GSlateColorDeficiencyType, GSlateColorDeficiencySeverity, GSlateShowColorDeficiencyCorrectionWithDeficiency);

					RHICmdList.DispatchComputeShader(FMath::DivideAndRoundUp(ViewportWidth, FCompositeCSBase::NUM_THREADS_PER_GROUP), FMath::DivideAndRoundUp(ViewportHeight, FCompositeCSBase::NUM_THREADS_PER_GROUP), 1);

				}

				// Put the backbuffer back to the correct one.
				BackBuffer = FinalBuffer;
			} //bCompositeUI


#if WITH_EDITOR
			if (bRenderOffscreen)
			{
				const auto FeatureLevel = GMaxRHIFeatureLevel;
				auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

				FRHIRenderPassInfo RPInfo(FinalBuffer, ERenderTargetActions::Load_Store);
				RHICmdList.Transition(FRHITransitionInfo(FinalBuffer, ERHIAccess::Unknown, ERHIAccess::RTV));
				RHICmdList.BeginRenderPass(RPInfo, TEXT("SlateComposite"));

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState			= TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.RasterizerState		= TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState	= TStaticDepthStencilState<false, CF_Always>::GetRHI();

				// ST2084 (PQ) encoding
				TShaderMapRef<FHDREditorConvertPS> PixelShader(ShaderMap);
				TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParametersLegacyPS(RHICmdList, PixelShader, HDRRenderRT->GetRHI());

				static const FName RendererModuleName("Renderer");
				IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

				RendererModule.DrawRectangle(
					RHICmdList,
					0.f, 0.f,
					(float)ViewportWidth, (float)ViewportHeight,
					0.f, 0.f,
					(float)ViewportWidth, (float)ViewportHeight,
					FIntPoint(ViewportWidth, ViewportHeight),
					FIntPoint(ViewportWidth, ViewportHeight),
					VertexShader,
					EDRF_UseTriangleOptimization);
				
				RHICmdList.EndRenderPass();
				BackBuffer = FinalBuffer;
			}
#endif
			if (!bRenderedStereo && GEngine && IsValidRef(ViewportInfo.GetRenderTargetTexture()) && GEngine->StereoRenderingDevice.IsValid())
			{
				const FVector2D WindowSize = WindowElementList.GetWindowSize();
				GEngine->StereoRenderingDevice->RenderTexture_RenderThread(RHICmdList, RHIGetViewportBackBuffer(ViewportInfo.ViewportRHI), ViewportInfo.GetRenderTargetTexture(), WindowSize);
			}
			RHICmdList.Transition(FRHITransitionInfo(BackBuffer, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));

			// Fire delegate to inform bound functions the back buffer is ready to be captured.
			OnBackBufferReadyToPresentDelegate.Broadcast(*DrawCommandParams.Window, BackBuffer);

			if (bRenderedStereo)
			{
				// XR requires the backbuffer to be transitioned back to RTV
				RHICmdList.Transition(FRHITransitionInfo(BackBuffer, ERHIAccess::SRVGraphics, ERHIAccess::RTV));
			}
		}
	}

	if (bTakingAScreenShot && ScreenshotViewportInfo != nullptr && ScreenshotViewportInfo == &ViewportInfo)
	{
		// take screenshot before swapbuffer
		FTexture2DRHIRef BackBuffer = RHIGetViewportBackBuffer(ViewportInfo.ViewportRHI);

		// Sanity check to make sure the user specified a valid screenshot rect.
		FIntRect ClampedScreenshotRect;
		ClampedScreenshotRect.Min = ScreenshotRect.Min;
		ClampedScreenshotRect.Max = ScreenshotRect.Max.ComponentMin(BackBuffer->GetSizeXY());
		ClampedScreenshotRect.Max = ScreenshotRect.Min.ComponentMax(ClampedScreenshotRect.Max);

		if (ClampedScreenshotRect != ScreenshotRect)
		{
			UE_LOG(LogSlate, Warning, TEXT("Slate: Screenshot rect max coordinate had to be clamped from [%d, %d] to [%d, %d]"), ScreenshotRect.Max.X, ScreenshotRect.Max.Y, ClampedScreenshotRect.Max.X, ClampedScreenshotRect.Max.Y);
		}

		if (!ClampedScreenshotRect.IsEmpty())
		{
			if (OutHDRScreenshotData != nullptr)
			{
				RHICmdList.ReadSurfaceData(BackBuffer, ClampedScreenshotRect, *OutHDRScreenshotData, FReadSurfaceDataFlags(RCM_MinMax));
			}
			else
			{
				RHICmdList.ReadSurfaceData(BackBuffer, ClampedScreenshotRect, *OutScreenshotData, FReadSurfaceDataFlags());
			}
		}
		else
		{
			UE_LOG(LogSlate, Warning, TEXT("Slate: Screenshot rect was empty! Skipping readback of back buffer."));
		}
		bTakingAScreenShot = false;
		OutScreenshotData = nullptr;
		OutHDRScreenshotData = nullptr;
		ScreenshotViewportInfo = nullptr;
	}

	// check if we need to cleanup slate render targets alloc.
	RenderingPolicy->TickPostProcessResources();

	// Calculate renderthread time (excluding idle time).	
	uint32 StartTime = FPlatformTime::Cycles();

	RHICmdList.EnqueueLambda([CurrentFrameCounter = GFrameCounterRenderThread](FRHICommandListImmediate& InRHICmdList)
	{
		UEngine::SetPresentLatencyMarkerStart(CurrentFrameCounter);
	});

	RHICmdList.EndDrawingViewport(ViewportInfo.ViewportRHI, true, DrawCommandParams.bLockToVsync);

	RHICmdList.EnqueueLambda([CurrentFrameCounter = GFrameCounterRenderThread](FRHICommandListImmediate& InRHICmdList)
	{
		UEngine::SetPresentLatencyMarkerEnd(CurrentFrameCounter);
	});

	uint32 EndTime = FPlatformTime::Cycles();

	GSwapBufferTime = EndTime - StartTime;
	SET_CYCLE_COUNTER(STAT_PresentTime, GSwapBufferTime);

	uint32 ThreadTime = EndTime - LastTimestamp;
	LastTimestamp = EndTime;

	uint32 RenderThreadIdle = 0;

	FThreadIdleStats& RenderThread = FThreadIdleStats::Get();
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForAllOtherSleep] = RenderThread.Waits;
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += GSwapBufferTime;

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_RenderThreadSleepTime, GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForAllOtherSleep]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUQuery   , GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery     ]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUPresent , GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent   ]);

	const uint32 RenderThreadNonCriticalWaits = RenderThread.Waits - RenderThread.WaitsCriticalPath;
	const uint32 RenderThreadWaitingForGPUQuery = GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery];

	// Set the RenderThreadIdle CSV stats
	CSV_CUSTOM_STAT(RenderThreadIdle, Total          , FPlatformTime::ToMilliseconds(RenderThread.Waits            ), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RenderThreadIdle, CriticalPath   , FPlatformTime::ToMilliseconds(RenderThread.WaitsCriticalPath), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RenderThreadIdle, SwapBuffer     , FPlatformTime::ToMilliseconds(GSwapBufferTime               ), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RenderThreadIdle, NonCriticalPath, FPlatformTime::ToMilliseconds(RenderThreadNonCriticalWaits  ), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RenderThreadIdle, GPUQuery       , FPlatformTime::ToMilliseconds(RenderThreadWaitingForGPUQuery), ECsvCustomStatOp::Set);

	for (int32 Index = 0; Index < ERenderThreadIdleTypes::Num; Index++)
	{
		RenderThreadIdle += GRenderThreadIdle[Index];
		GRenderThreadIdle[Index] = 0;
	}

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime, RenderThreadIdle);
	GRenderThreadTime = (ThreadTime > RenderThreadIdle) ? (ThreadTime - RenderThreadIdle) : ThreadTime;
	GRenderThreadWaitTime = RenderThreadIdle;

	// Compute GRenderThreadTimeCriticalPath
	uint32 RenderThreadNonCriticalPathIdle = RenderThreadIdle - RenderThread.WaitsCriticalPath;
	GRenderThreadTimeCriticalPath = (ThreadTime > RenderThreadNonCriticalPathIdle) ? (ThreadTime - RenderThreadNonCriticalPathIdle) : ThreadTime;
	SET_CYCLE_COUNTER(STAT_RenderThreadCriticalPath, GRenderThreadTimeCriticalPath);


	if (CVarRenderThreadTimeIncludesDependentWaits.GetValueOnRenderThread())
	{
		// Optionally force the renderthread stat to include dependent waits
		GRenderThreadTime = GRenderThreadTimeCriticalPath;
	}

	// Reset the idle stats
	RenderThread.Reset();
	
	if (IsRunningRHIInSeparateThread())
	{
		RHICmdList.EnqueueLambda([](FRHICommandListImmediate&)
		{
			// Restart the RHI thread timer, so we don't count time spent in Present twice when this command list finishes.
			uint32 ThisCycles = FPlatformTime::Cycles();
			GWorkingRHIThreadTime += (ThisCycles - GWorkingRHIThreadStartCycles);
			GWorkingRHIThreadStartCycles = ThisCycles;

			FThreadIdleStats& RHIThreadStats = FThreadIdleStats::Get();

			uint32 NewVal = GWorkingRHIThreadTime;
			if (NewVal > RHIThreadStats.Waits)
			{
				NewVal -= RHIThreadStats.Waits;
			}

			FPlatformAtomics::AtomicStore((int32*)&GRHIThreadTime, (int32)NewVal);
			GWorkingRHIThreadTime = 0;
			RHIThreadStats.Reset();
		});
	}
}

void FSlateRHIRenderer::DrawWindows(FSlateDrawBuffer& WindowDrawBuffer)
{
	DrawWindows_Private(WindowDrawBuffer);
}


void FSlateRHIRenderer::PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData, SWindow* InScreenshotWindow)
{
	check(OutColorData);

	bTakingAScreenShot = true;
	ScreenshotRect = Rect;
	OutScreenshotData = OutColorData;
	OutHDRScreenshotData = nullptr;
	ScreenshotViewportInfo = *WindowToViewportInfo.Find(InScreenshotWindow);
}

void FSlateRHIRenderer::PrepareToTakeHDRScreenshot(const FIntRect& Rect, TArray<FLinearColor>* OutColorData, SWindow* InScreenshotWindow)
{
	check(OutColorData);

	bTakingAScreenShot = true;
	ScreenshotRect = Rect;
	OutScreenshotData = nullptr;
	OutHDRScreenshotData = OutColorData;
	ScreenshotViewportInfo = *WindowToViewportInfo.Find(InScreenshotWindow);
}

/**
* Creates necessary resources to render a window and sends draw commands to the rendering thread
*
* @param WindowDrawBuffer	The buffer containing elements to draw
*/
void FSlateRHIRenderer::DrawWindows_Private(FSlateDrawBuffer& WindowDrawBuffer)
{
	checkSlow(IsThreadSafeForSlateRendering());

	if (bUpdateHDRDisplayInformation && IsHDRAllowed() && IsInGameThread())
	{
		FlushRenderingCommands();
		RHIHandleDisplayChange();
		bUpdateHDRDisplayInformation = false;
	}

	FSlateRHIRenderingPolicy* Policy = RenderingPolicy.Get();
	ENQUEUE_RENDER_COMMAND(SlateBeginDrawingWindowsCommand)(
		[Policy](FRHICommandListImmediate& RHICmdList)
	{
		Policy->BeginDrawingWindows();
	}
	);

	// Update texture atlases if needed and safe
	if (DoesThreadOwnSlateRendering())
	{
		ResourceManager->UpdateTextureAtlases();
	}

	const TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetFontCache();

	// Iterate through each element list and set up an RHI window for it if needed
	const TArray<TSharedRef<FSlateWindowElementList>>& WindowElementLists = WindowDrawBuffer.GetWindowElementLists();
	for (int32 ListIndex = 0; ListIndex < WindowElementLists.Num(); ++ListIndex)
	{
		FSlateWindowElementList& ElementList = *WindowElementLists[ListIndex];

		SWindow* Window = ElementList.GetRenderWindow();

		if (Window)
		{
			const FVector2f WindowSize = UE::Slate::CastToVector2f(Window->GetViewportSize());
			if (WindowSize.X > 0 && WindowSize.Y > 0)
			{
				// The viewport need to be created at this point  
				FViewportInfo* ViewInfo = nullptr;
				{
					FViewportInfo** FoundViewInfo = WindowToViewportInfo.Find(Window);
					if (ensure(FoundViewInfo))
					{
						ViewInfo = *FoundViewInfo;
					}
					else
					{
						UE_LOG(LogSlate, Error, TEXT("The ViewportInfo could not be found for Window."));
						continue;
					}
				}

				bool bCompositeHDRViewports = ElementBatcher->CompositeHDRViewports();
				ElementBatcher->SetCompositeHDRViewports(CompositeUIWithHdrRenderTarget(ViewInfo));
				// Add all elements for this window to the element batcher
				ElementBatcher->AddElements(ElementList);
				ElementBatcher->SetCompositeHDRViewports(bCompositeHDRViewports);

				// Update the font cache with new text after elements are batched
				FontCache->UpdateCache();

				bool bLockToVsync = ElementBatcher->RequiresVsync();

				bool bForceVsyncFromCVar = false;
				if (GIsEditor)
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"));
					bForceVsyncFromCVar = (CVar->GetInt() != 0);
				}
				else
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
					bForceVsyncFromCVar = (CVar->GetInt() != 0);
				}

				bLockToVsync |= bForceVsyncFromCVar;

				// Cache if the element batcher post buffer usage, will get reset on batch reset
				ESlatePostRT UsedSlatePostBufferBits = ElementBatcher->GetUsedSlatePostBuffers();
				ESlatePostRT ResourceUpdatingPostBufferBits = ElementBatcher->GetResourceUpdatingPostBuffers();
				ESlatePostRT SkipDefaultUpdatePostBufferBits = ElementBatcher->GetSkipDefaultUpdatePostBuffers();

				// All elements for this window have been batched and rendering data updated
				ElementBatcher->ResetBatches();

				// Cache off the HDR status
				ViewInfo->bHDREnabled = RHIGetColorSpace(ViewInfo->ViewportRHI) != EColorSpaceAndEOTF::ERec709_sRGB;
				Window->SetIsHDR(ViewInfo->bHDREnabled);

				if (Window->IsViewportSizeDrivenByWindow())
				{
					// Resize the viewport if needed
					ConditionalResizeViewport(ViewInfo, ViewInfo->DesiredWidth, ViewInfo->DesiredHeight, IsViewportFullscreen(*Window), Window);
				}

				// Update slate post buffers before slate draw if needed
				if (CVarCopyBackbufferToSlatePostRenderTargets.GetValueOnAnyThread() && IsInGameThread() && GIsClient && !IsRunningCommandlet() && !GUsingNullRHI && UAssetManager::IsInitialized())
				{
					uint8 SlatePostBufferBitIndex = 0;
					for (ESlatePostRT SlatePostBufferBit : TEnumRange<ESlatePostRT>())
					{
						// We only attempt to load if the buffer is enabled, so just try to load / get the buffer
						UTextureRenderTarget2D* SlatePostBuffer = USlateRHIRendererSettings::GetMutable()->LoadGetPostBufferRT(SlatePostBufferBit);
						if (!SlatePostBuffer)
						{
							SlatePostBufferBitIndex++;
							continue;
						}

						bool bIsViewportPresentForPIE = GIsEditor ? Window->GetViewport().IsValid() : true;
						bool bPostBufferBitUsed = (UsedSlatePostBufferBits & SlatePostBufferBit) != ESlatePostRT::None;
						bool bSkipDefaultPostBufferUpdate = (SkipDefaultUpdatePostBufferBits & SlatePostBufferBit) != ESlatePostRT::None;

						if (bIsViewportPresentForPIE && bPostBufferBitUsed)
						{
							// Viewport only has a RT texture in editor / PIE, this texture will be the size of the viewport
							// typically this texture 'BufferedRT', is smaller than the entire backbuffer used for the editor
							// We initialize to nullptr so this ptr can be copied & resolved here instead of in the render command
							FSlateRenderTargetRHI* ViewportTexture = nullptr;
							if (GIsEditor)
							{
								ViewportTexture = static_cast<FSlateRenderTargetRHI*>(Window->GetViewport()->GetViewportRenderTargetTexture());
							}

							FIntPoint SizeSlatePostRT = GIsEditor
								? FIntPoint(ViewportTexture->GetWidth(), ViewportTexture->GetHeight())
								: FIntPoint(ViewInfo->DesiredWidth, ViewInfo->DesiredHeight);

							bool bHDREnabled = IsHDREnabled();
							bool bIsPixelFormatCorrect = bHDREnabled
								? SlatePostBuffer->GetFormat() == EPixelFormat::PF_FloatRGBA
								: SlatePostBuffer->GetFormat() == EPixelFormat::PF_A2B10G10R10;
							if (SlatePostBuffer->SizeX != SizeSlatePostRT.X || SlatePostBuffer->SizeY != SizeSlatePostRT.Y || !bIsPixelFormatCorrect)
							{
								SlatePostBuffer->InitCustomFormat(SizeSlatePostRT.X, SizeSlatePostRT.Y, bHDREnabled ? EPixelFormat::PF_FloatRGBA : EPixelFormat::PF_A2B10G10R10, true);
							}

							const FVector2D ElementWindowSize = ElementList.GetWindowSize();

							if (!bSkipDefaultPostBufferUpdate)
							{
								if (USlateRHIPostBufferProcessor* PostProcessor = USlateFXSubsystem::GetPostProcessor(SlatePostBufferBit))
								{
									// Allow the post processor to enque render commands, we delgate this task so it can be done in a thread safe manner.
									PostProcessor->PostProcess(ViewInfo, ViewportTexture, ElementWindowSize, FSlateRHIRenderingPolicyInterface(RenderingPolicy.Get()), SlatePostBuffer);
								}
								else
								{
									ENQUEUE_RENDER_COMMAND(FUpdateSlatePostBuffers)([ViewInfo, ElementWindowSize, ViewportTexture, SlatePostBuffer](FRHICommandListImmediate& RHICmdList)
									{
										bool bRenderedStereo = false;
										if (CVarDrawToVRRenderTarget->GetInt() == 0 && GEngine && IsValidRef(ViewInfo->GetRenderTargetTexture()) && GEngine->StereoRenderingDevice.IsValid())
										{
											GEngine->StereoRenderingDevice->RenderTexture_RenderThread(RHICmdList, RHIGetViewportBackBuffer(ViewInfo->ViewportRHI), ViewInfo->GetRenderTargetTexture(), ElementWindowSize);
											bRenderedStereo = true;
										}

										FTexture2DRHIRef ViewportRT = bRenderedStereo ? nullptr : ViewInfo->GetRenderTargetTexture();
										FTexture2DRHIRef BackBuffer = (ViewportRT) ? ViewportRT : RHIGetViewportBackBuffer(ViewInfo->ViewportRHI);

										if (BackBuffer)
										{
											FRHICopyTextureInfo CopyInfo;

											// Copy just the viewport RT if in PIE, else do entire backbuffer
											if (GIsEditor)
											{
												CopyInfo.Size = FIntVector(ViewportTexture->GetWidth(), ViewportTexture->GetHeight(), 1);
												TransitionAndCopyTexture(RHICmdList, ViewportTexture->GetRHIRef(), SlatePostBuffer->TextureReference.TextureReferenceRHI, CopyInfo);
											}
											else
											{
												FIntPoint BackbufferExtent = BackBuffer->GetDesc().Extent;
												CopyInfo.Size = FIntVector(BackbufferExtent.X, BackbufferExtent.Y, 1);
												TransitionAndCopyTexture(RHICmdList, BackBuffer, SlatePostBuffer->TextureReference.TextureReferenceRHI, CopyInfo);
											}
										}
									});
								}

								SlatePostRTFences[SlatePostBufferBitIndex].BeginFence();
								LastFramesPostBufferUsed[SlatePostBufferBitIndex] = GFrameCounter;
							}

							bShrinkPostBufferRequested &= ~SlatePostBufferBit;
						}
						else if (SlatePostBuffer 
							&& SlatePostBuffer->GetResource()
							&& LastFramesPostBufferUsed[SlatePostBufferBitIndex] + 1 < GFrameCounter
							&& SlatePostRTFences[SlatePostBufferBitIndex].IsFenceComplete() 
							&& (SlatePostBuffer->SizeX != 1 || SlatePostBuffer->SizeY != 1))
						{
							if ((bShrinkPostBufferRequested & SlatePostBufferBit) == ESlatePostRT::None)
							{
								// Delay shrink attempts for a frame, since if we resize while there is an active copy in flight, we will crash
								bShrinkPostBufferRequested |= SlatePostBufferBit;
							}
							else
							{
								// Resize unused SlatePostRTs to 1x1.
								SlatePostBuffer->InitCustomFormat(1, 1, IsHDREnabled() ? EPixelFormat::PF_FloatRGBA : EPixelFormat::PF_A2B10G10R10, true);
								bShrinkPostBufferRequested &= ~SlatePostBufferBit;
							}
						}

						SlatePostBufferBitIndex++;
					}
				}

				// Tell the rendering thread to draw the windows
				{
					auto GetViewRect = [Window]()
					{
#if WITH_EDITOR
						if (GIsEditor)
						{
							if (TSharedPtr<ISlateViewport> Viewport = Window->GetViewport())
							{
								if (TSharedPtr<SWidget> ViewportWidget = Viewport->GetWidget().Pin())
								{
									// The actual backbuffer has a padding that extends beyond the draw area, account for this in our offsets
									int32 OffsetX = FMath::RoundToInt32(ViewportWidget->GetTickSpaceGeometry().GetAbsolutePosition().X - Window->GetPositionInScreen().X);
									int32 OffsetY = FMath::RoundToInt32(ViewportWidget->GetTickSpaceGeometry().GetAbsolutePosition().Y - Window->GetPositionInScreen().Y);

									FIntPoint ViewportSize = Viewport->GetSize();
									return FIntRect(FIntPoint(OffsetX, OffsetY), FIntPoint(ViewportSize.X + OffsetX, ViewportSize.Y + OffsetY));
								}
							}
						}
#endif // WITH_EDITOR

						return FIntRect();
					};

					FSlateDrawWindowCommandParams Params;

					Params.Renderer = this;
					Params.WindowElementList = &ElementList;
					Params.Window = Window;
					Params.ViewRect = GetViewRect();
					Params.UsedSlatePostBuffers = UsedSlatePostBufferBits;
#if WANTS_DRAW_MESH_EVENTS
					Params.WindowTitle = Window->GetTitle().ToString();
#endif
					Params.bLockToVsync = bLockToVsync;
#if ALPHA_BLENDED_WINDOWS
					Params.bClear = Window->GetTransparencySupport() == EWindowTransparency::PerPixel;
#else
					Params.bClear = false;
#endif	
					Params.Time = FGameTime::CreateDilated(
						FPlatformTime::Seconds() - GStartTime, (float)FApp::GetDeltaTime(),
						FApp::GetCurrentTime() - GStartTime, (float)FApp::GetDeltaTime());

					// Skip the actual draw if we're in a headless execution environment
					bool bLocalTakingAScreenShot = bTakingAScreenShot;
					if (GIsClient && !IsRunningCommandlet() && !GUsingNullRHI)
					{
						ENQUEUE_RENDER_COMMAND(SlateDrawWindowsCommand)(
							[Params, ViewInfo](FRHICommandListImmediate& RHICmdList)
							{
								Params.Renderer->DrawWindow_RenderThread(RHICmdList, *ViewInfo, *Params.WindowElementList, Params);
							}
						);

						// After we draw, if a resource is going to update a post buffer we need to add another render fence for that buffer
						if (CVarCopyBackbufferToSlatePostRenderTargets.GetValueOnAnyThread())
						{
							uint8 SlatePostBufferBitIndex = 0;
							for (ESlatePostRT SlatePostBufferBit : TEnumRange<ESlatePostRT>())
							{
								bool bResourceUpdatedPostBuffer = (ResourceUpdatingPostBufferBits & SlatePostBufferBit) != ESlatePostRT::None;
								if (bResourceUpdatedPostBuffer)
								{
									SlatePostRTFences[SlatePostBufferBitIndex].BeginFence();
									LastFramesPostBufferUsed[SlatePostBufferBitIndex] = GFrameCounter;
									bShrinkPostBufferRequested &= ~SlatePostBufferBit;
								}

								SlatePostBufferBitIndex++;
							}
						}
					}

					SlateWindowRendered.Broadcast(*Window, &ViewInfo->ViewportRHI);

					if (bLocalTakingAScreenShot)
					{
						// immediately flush the RHI command list
						FlushRenderingCommands();
					}
				}
			}
		}
		else
		{
			ensureMsgf(false, TEXT("Window isnt valid but being drawn!"));
		}
	}

	FSlateDrawBuffer* DrawBuffer = &WindowDrawBuffer;
	ENQUEUE_RENDER_COMMAND(SlateEndDrawingWindowsCommand)(
		[DrawBuffer, Policy](FRHICommandListImmediate& RHICmdList)
	{
		FSlateEndDrawingWindowsCommand::EndDrawingWindows(RHICmdList, DrawBuffer, *Policy);
	}
	);

	if (DeferredUpdateContexts.Num() > 0)
	{
		// Intentionally copy the contexts to avoid contention with the game thread
		ENQUEUE_RENDER_COMMAND(DrawWidgetRendererImmediate)(
			[Contexts = DeferredUpdateContexts](FRHICommandListImmediate& RHICmdList) mutable
			{
				for (const FRenderThreadUpdateContext& Context : Contexts)
				{
					Context.Renderer->DrawWindowToTarget_RenderThread(RHICmdList, Context);
					Context.Renderer->ReleaseDrawBuffer(*Context.WindowDrawBuffer);
				}
			}
		);

		DeferredUpdateContexts.Empty();
	}

	if (FastPathRenderingDataCleanupList)
	{
		FastPathRenderingDataCleanupList->Cleanup();
		FastPathRenderingDataCleanupList = nullptr;
	}

	// flush the cache if needed
	FontCache->ConditionalFlushCache();
	ResourceManager->ConditionalFlushAtlases();
}


FIntPoint FSlateRHIRenderer::GenerateDynamicImageResource(const FName InTextureName)
{
	check(IsInGameThread());

	uint32 Width = 0;
	uint32 Height = 0;
	TArray<uint8> RawData;

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName(InTextureName);
	if (!TextureResource.IsValid())
	{
		// Load the image from disk
		bool bSucceeded = ResourceManager->LoadTexture(InTextureName, InTextureName.ToString(), Width, Height, RawData);
		if (bSucceeded)
		{
			TextureResource = ResourceManager->MakeDynamicTextureResource(InTextureName, Width, Height, RawData);
		}
	}

	return TextureResource.IsValid() ? TextureResource->Proxy->ActualSize : FIntPoint(0, 0);
}

bool FSlateRHIRenderer::GenerateDynamicImageResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes)
{
	check(IsInGameThread());

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName(ResourceName);
	if (!TextureResource.IsValid())
	{
		TextureResource = ResourceManager->MakeDynamicTextureResource(ResourceName, Width, Height, Bytes);
	}
	return TextureResource.IsValid();
}

bool FSlateRHIRenderer::GenerateDynamicImageResource(FName ResourceName, FSlateTextureDataRef TextureData)
{
	check(IsInGameThread());

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName(ResourceName);
	if (!TextureResource.IsValid())
	{
		TextureResource = ResourceManager->MakeDynamicTextureResource(ResourceName, TextureData);
	}
	return TextureResource.IsValid();
}

FSlateResourceHandle FSlateRHIRenderer::GetResourceHandle(const FSlateBrush& Brush, FVector2f LocalSize, float DrawScale)
{
	return ResourceManager->GetResourceHandle(Brush, LocalSize, DrawScale);
}

bool FSlateRHIRenderer::CanRenderResource(UObject& InResourceObject) const
{
	return Cast<UTexture>(&InResourceObject) || Cast<ISlateTextureAtlasInterface>(&InResourceObject) || Cast<UMaterialInterface>(&InResourceObject);
}

void FSlateRHIRenderer::RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove )
{
	if (BrushToRemove.IsValid())
	{
		DynamicBrushesToRemove[FreeBufferIndex].Add(BrushToRemove);
	}
}

/**
* Gives the renderer a chance to wait for any render commands to be completed before returning/
*/
void FSlateRHIRenderer::FlushCommands() const
{
	if (IsInGameThread() || IsInSlateThread())
	{
		FlushRenderingCommands();
	}
}

/**
* Gives the renderer a chance to synchronize with another thread in the event that the renderer runs
* in a multi-threaded environment.  This function does not return until the sync is complete
*/
void FSlateRHIRenderer::Sync() const
{
	// Sync game and render thread. Either total sync or allowing one frame lag.
	static FFrameEndSync FrameEndSync;
	static auto CVarAllowOneFrameThreadLag = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OneFrameThreadLag"));
	FrameEndSync.Sync(CVarAllowOneFrameThreadLag->GetValueOnAnyThread() != 0);
}

/**
 * Inline issues a BeginFrame to the RHI.
 * This is to handle cases like Modal dialogs in the UI. The game loop stops while
 * the dialog is open but continues to issue draws. The RHI thinks there are all part of one super long
 * frame until the Modal window is closed.
 */
void FSlateRHIRenderer::BeginFrame() const
{
	ENQUEUE_RENDER_COMMAND(SlateRHIBeginFrame)(
	   [](FRHICommandListImmediate& RHICmdList)
	   {
		   RHICmdList.BeginFrame();

		   // Suspend stat gathering when running modal dialog 'fake' frame loops
		   GPU_STATS_SUSPENDFRAME();
	   }
	);
}

void FSlateRHIRenderer::EndFrame() const
{
	ENQUEUE_RENDER_COMMAND(SlateRHIEndFrame)(
	   [](FRHICommandListImmediate& RHICmdList)
	   {
		   RHICmdList.EndFrame();
	   }
	);
}

void FSlateRHIRenderer::ReloadTextureResources()
{
	ResourceManager->ReloadTextures();
}

void FSlateRHIRenderer::LoadUsedTextures()
{
	if (ResourceManager.IsValid())
	{
		ResourceManager->LoadUsedTextures();
	}
}

void FSlateRHIRenderer::LoadStyleResources(const ISlateStyle& Style)
{
	if (ResourceManager.IsValid())
	{
		ResourceManager->LoadStyleResources(Style);
	}
}

void FSlateRHIRenderer::ReleaseDynamicResource(const FSlateBrush& InBrush)
{
	ensure(IsInGameThread());
	ResourceManager->ReleaseDynamicResource(InBrush);
}

void* FSlateRHIRenderer::GetViewportResource(const SWindow& Window)
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo** InfoPtr = WindowToViewportInfo.Find(&Window);

	if (InfoPtr)
	{
		FViewportInfo* ViewportInfo = *InfoPtr;

		// Create the viewport if it doesnt exist
		if (!IsValidRef(ViewportInfo->ViewportRHI))
		{
			// Sanity check dimensions
			checkf(ViewportInfo->Width <= MAX_VIEWPORT_SIZE && ViewportInfo->Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), ViewportInfo->Width, ViewportInfo->Height);

			const bool bFullscreen = IsViewportFullscreen(Window);

			ViewportInfo->ViewportRHI = RHICreateViewport(ViewportInfo->OSWindow, ViewportInfo->Width, ViewportInfo->Height, bFullscreen, ViewportInfo->PixelFormat);
		}

		return &ViewportInfo->ViewportRHI;
	}
	else
	{
		return NULL;
	}
}

void FSlateRHIRenderer::SetColorVisionDeficiencyType(EColorVisionDeficiency Type, int32 Severity, bool bCorrectDeficiency, bool bShowCorrectionWithDeficiency)
{
	GSlateColorDeficiencyType = Type;
	GSlateColorDeficiencySeverity = FMath::Clamp(Severity, 0, 10);
	GSlateColorDeficiencyCorrection = bCorrectDeficiency;
	GSlateShowColorDeficiencyCorrectionWithDeficiency = bShowCorrectionWithDeficiency;
}

FSlateUpdatableTexture* FSlateRHIRenderer::CreateUpdatableTexture(uint32 Width, uint32 Height)
{
	const bool bCreateEmptyTexture = true;
	FSlateTexture2DRHIRef* NewTexture = new FSlateTexture2DRHIRef(Width, Height, GetSlateRecommendedColorFormat(), nullptr, TexCreate_Dynamic, bCreateEmptyTexture);
	if (IsInRenderingThread())
	{
		NewTexture->InitResource(FRHICommandListImmediate::Get());
	}
	else
	{
		BeginInitResource(NewTexture);
	}
	return NewTexture;
}

FSlateUpdatableTexture* FSlateRHIRenderer::CreateSharedHandleTexture(void* SharedHandle)
{
	return nullptr;
}

void FSlateRHIRenderer::ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture)
{
	if (IsInRenderingThread())
	{
		Texture->GetRenderResource()->ReleaseResource();
		delete Texture;
	}
	else
	{
		Texture->Cleanup();
	}
}

ISlateAtlasProvider* FSlateRHIRenderer::GetTextureAtlasProvider()
{
	if (ResourceManager.IsValid())
	{
		return ResourceManager->GetTextureAtlasProvider();
	}

	return nullptr;
}



int32 FSlateRHIRenderer::RegisterCurrentScene(FSceneInterface* Scene)
{
	check(IsInGameThread());
	if (Scene && Scene->GetWorld())
	{
		// We only want one scene view per world, (todo per player for split screen)
		CurrentSceneIndex = ActiveScenes.IndexOfByPredicate([&Scene](const FSceneInterface* TestScene) { return TestScene->GetWorld() == Scene->GetWorld(); });
		if (CurrentSceneIndex == INDEX_NONE)
		{
			CurrentSceneIndex = ActiveScenes.Add(Scene);

			// We need to keep the ActiveScenes array synchronized with the Policy's ActiveScenes array on
			// the render thread.
			FSlateRHIRenderingPolicy* InRenderPolicy = RenderingPolicy.Get();
			int32 LocalCurrentSceneIndex = CurrentSceneIndex;
			ENQUEUE_RENDER_COMMAND(RegisterCurrentSceneOnPolicy)(
				[InRenderPolicy, Scene, LocalCurrentSceneIndex](FRHICommandListImmediate& RHICmdList)
			{
				if (LocalCurrentSceneIndex != -1)
				{
					InRenderPolicy->AddSceneAt(Scene, LocalCurrentSceneIndex);
				}
			}
			);
		}
	}
	else
	{
		CurrentSceneIndex = -1;
	}

	return CurrentSceneIndex;
}

int32 FSlateRHIRenderer::GetCurrentSceneIndex() const
{
	return CurrentSceneIndex;
}

void FSlateRHIRenderer::ClearScenes()
{
	if (!IsInSlateThread())
	{
		CurrentSceneIndex = -1;
		ActiveScenes.Empty();

		// We need to keep the ActiveScenes array synchronized with the Policy's ActiveScenes array on
		// the render thread.
		FSlateRenderingPolicy* InRenderPolicy = RenderingPolicy.Get();
		ENQUEUE_RENDER_COMMAND(ClearScenesOnPolicy)(
			[InRenderPolicy](FRHICommandListImmediate& RHICmdList)
		{
			InRenderPolicy->ClearScenes();
		}
		);
	}
}

EPixelFormat FSlateRHIRenderer::GetSlateRecommendedColorFormat()
{
	return bIsStandaloneStereoOnlyDevice ? PF_R8G8B8A8 : PF_B8G8R8A8;
}

FRHICOMMAND_MACRO(FClearCachedRenderingDataCommand)
{
public:
	FClearCachedRenderingDataCommand(FSlateCachedFastPathRenderingData* InCachedRenderingData)
		: CachedRenderingData(InCachedRenderingData)
	{
		
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		delete CachedRenderingData;
	}

private:
	FSlateCachedFastPathRenderingData* CachedRenderingData;
};

FRHICOMMAND_MACRO(FClearCachedElementDataCommand)
{
public:
	FClearCachedElementDataCommand(FSlateCachedElementData* InCachedElementData)
		: CachedElementData(InCachedElementData)
	{

	}

	void Execute(FRHICommandListBase& CmdList)
	{
		delete CachedElementData;
	}

private:
	FSlateCachedElementData* CachedElementData;
};

void FSlateRHIRenderer::DestroyCachedFastPathRenderingData(FSlateCachedFastPathRenderingData* CachedRenderingData)
{
	check(CachedRenderingData);

	if (!FastPathRenderingDataCleanupList)
	{
		// This will be deleted later on the rendering thread
		FastPathRenderingDataCleanupList = new FFastPathRenderingDataCleanupList;
	}

	FastPathRenderingDataCleanupList->FastPathRenderingDataToRemove.Add(CachedRenderingData);
}

void FSlateRHIRenderer::DestroyCachedFastPathElementData(FSlateCachedElementData* CachedElementData)
{
	check(CachedElementData);

	// Cached data should be destroyed in a thread safe way.  If there is an rhi thread it could be reading from the data to copy it into a vertex buffer
	// so delete it on the rhi thread if necessary, otherwise delete it on the render thread
	ENQUEUE_RENDER_COMMAND(ClearCachedElementData)(
		[CachedElementData](FRHICommandListImmediate& RHICmdList)
	{
		if (!RHICmdList.Bypass())
		{
			new (RHICmdList.AllocCommand<FClearCachedElementDataCommand>()) FClearCachedElementDataCommand(CachedElementData);
		}
		else
		{
			FClearCachedElementDataCommand Cmd(CachedElementData);
			Cmd.Execute(RHICmdList);
		}
	});
}

#if WITH_EDITORONLY_DATA
namespace SlateRendererUtil
{
	bool bSlateShadersInitialized = false;
	FDelegateHandle GlobalShaderCompilationDelegateHandle;

	bool AreShadersInitialized()
	{
		if (!bSlateShadersInitialized)
		{
			bSlateShadersInitialized = IsGlobalShaderMapComplete(TEXT("SlateElement"));
			// if shaders are initialized, cache the value until global shaders gets recompiled.
			if (bSlateShadersInitialized)
			{
				GlobalShaderCompilationDelegateHandle = GetOnGlobalShaderCompilation().AddLambda([]()
				{
					bSlateShadersInitialized = false;
					GetOnGlobalShaderCompilation().Remove(GlobalShaderCompilationDelegateHandle);
				});
			}
		}
		return bSlateShadersInitialized;
	}
}
#endif

bool FSlateRHIRenderer::AreShadersInitialized() const
{
#if WITH_EDITORONLY_DATA
	return SlateRendererUtil::AreShadersInitialized();
#else
	return true;
#endif
}

void FSlateRHIRenderer::InvalidateAllViewports()
{
	for (TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It)
	{
		It.Value()->ViewportRHI = nullptr;
	}
}

FCriticalSection* FSlateRHIRenderer::GetResourceCriticalSection()
{
	return ResourceManager->GetResourceCriticalSection();
}

void FSlateRHIRenderer::ReleaseAccessedResources(bool bImmediatelyFlush)
{
	// We keep track of the Scene objects from SceneViewports on the SlateRenderer. Make sure that this gets refreshed every frame.
	ClearScenes();

	if (bImmediatelyFlush)
	{
		// Increment resource version to allow buffers to shrink or cached structures to clean up.
		ResourceVersion++;

		// Release resources generated specifically by the rendering policy if we are flushing.
		// This should NOT be done unless flushing
		RenderingPolicy->FlushGeneratedResources();

		//FlushCommands();
	}
}

void FSlateRHIRenderer::RequestResize(const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight)
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef(Window.Get());

	if (ViewInfo)
	{
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
	}
}

void FSlateRHIRenderer::SetWindowRenderTarget(const SWindow& Window, IViewportRenderTargetProvider* Provider)
{
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef(&Window);
	if (ViewInfo)
	{
		ViewInfo->RTProvider = Provider;
	}
}

FSlateRHIRenderingPolicyInterface FSlateRHIRenderer::GetRenderingPolicyInterface()
{
	return FSlateRHIRenderingPolicyInterface(RenderingPolicy.Get());
}

void FSlateRHIRenderer::AddWidgetRendererUpdate(const struct FRenderThreadUpdateContext& Context, bool bDeferredRenderTargetUpdate)
{
	if (bDeferredRenderTargetUpdate)
	{
		DeferredUpdateContexts.Add(Context);
	}
	else
	{
		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		FRenderThreadUpdateContext InContext = Context;
		ENQUEUE_RENDER_COMMAND(DrawWidgetRendererImmediate)(
			[InContext](FRHICommandListImmediate& RHICmdList)
			{
				InContext.Renderer->DrawWindowToTarget_RenderThread(RHICmdList, InContext);
			});
	}
}

FSlateEndDrawingWindowsCommand::FSlateEndDrawingWindowsCommand(FSlateRHIRenderingPolicy& InPolicy, FSlateDrawBuffer* InDrawBuffer)
	: Policy(InPolicy)
	, DrawBuffer(InDrawBuffer)
{}

void FSlateEndDrawingWindowsCommand::Execute(FRHICommandListBase& CmdList)
{
	Policy.EndDrawingWindows();
}

void FSlateEndDrawingWindowsCommand::EndDrawingWindows(FRHICommandListImmediate& RHICmdList, FSlateDrawBuffer* DrawBuffer, FSlateRHIRenderingPolicy& Policy)
{
	if (!RHICmdList.Bypass())
	{
		ALLOC_COMMAND_CL(RHICmdList, FSlateEndDrawingWindowsCommand)(Policy, DrawBuffer);
	}
	else
	{
		FSlateEndDrawingWindowsCommand Cmd(Policy, DrawBuffer);
		Cmd.Execute(RHICmdList);
	}
}

FSlateReleaseDrawBufferCommand::FSlateReleaseDrawBufferCommand(FSlateDrawBuffer* InDrawBuffer)
	: DrawBuffer(InDrawBuffer)
{}

void FSlateReleaseDrawBufferCommand::Execute(FRHICommandListBase& CmdList)
{
	DrawBuffer->Unlock();
}

void FSlateReleaseDrawBufferCommand::ReleaseDrawBuffer(FRHICommandListImmediate& RHICmdList, FSlateDrawBuffer* DrawBuffer)
{
	if (!RHICmdList.Bypass())
	{
		ALLOC_COMMAND_CL(RHICmdList, FSlateReleaseDrawBufferCommand)(DrawBuffer);
	}
	else
	{
		FSlateReleaseDrawBufferCommand Cmd(DrawBuffer);
		Cmd.Execute(RHICmdList);
	}
}


struct FClearCachedRenderingDataCommand2 final : public FRHICommand < FClearCachedRenderingDataCommand2 >
{
public:
	FClearCachedRenderingDataCommand2(FFastPathRenderingDataCleanupList* InCleanupList)
		: CleanupList(InCleanupList)
	{

	}

	void Execute(FRHICommandListBase& CmdList)
	{
		delete CleanupList;
	}

private:
	FFastPathRenderingDataCleanupList* CleanupList;
};


FFastPathRenderingDataCleanupList::~FFastPathRenderingDataCleanupList()
{
	for (FSlateCachedFastPathRenderingData* Data : FastPathRenderingDataToRemove)
	{
		delete Data;
	}
}

void FFastPathRenderingDataCleanupList::Cleanup()
{
	// Cached data should be destroyed in a thread safe way.  If there is an rhi thread it could be reading from the data to copy it into a vertex buffer
	// so delete it on the rhi thread if necessary, otherwise delete it on the render thread
	ENQUEUE_RENDER_COMMAND(ClearCachedRenderingData)(
		[CleanupList = this](FRHICommandListImmediate& RHICmdList)
	{
		if (!RHICmdList.Bypass())
		{
			new (RHICmdList.AllocCommand<FClearCachedRenderingDataCommand2>()) FClearCachedRenderingDataCommand2(CleanupList);
		}
		else
		{
			FClearCachedRenderingDataCommand2 Cmd(CleanupList);
			Cmd.Execute(RHICmdList);
		}
	});
}
