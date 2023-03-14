// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.cpp: Metal viewport RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#if PLATFORM_MAC
#include "Mac/CocoaWindow.h"
#include "Mac/CocoaThread.h"
#else
#include "IOS/IOSAppDelegate.h"
#endif
#include "RenderCommandFence.h"
#include "Containers/Set.h"
#include "MetalProfiler.h"
#include "RenderUtils.h"

extern int32 GMetalSupportsIntermediateBackBuffer;
extern int32 GMetalSeparatePresentThread;
extern int32 GMetalNonBlockingPresent;
extern float GMetalPresentFramePacing;

#if PLATFORM_IOS
static int32 GEnablePresentPacing = 0;
static FAutoConsoleVariableRef CVarMetalEnablePresentPacing(
	   TEXT("ios.PresentPacing"),
	   GEnablePresentPacing,
	   TEXT(""),
		ECVF_Default);
#endif

#if PLATFORM_MAC

// Quick way to disable availability warnings is to duplicate the definitions into a new type - gotta love ObjC dynamic-dispatch!
@interface FCAMetalLayer : CALayer
@property BOOL displaySyncEnabled;
@property BOOL allowsNextDrawableTimeout;
@end

@implementation FMetalView

- (id)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame:frameRect];
	if (self)
	{
	}
	return self;
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
	return YES;
}

@end

#endif

static FCriticalSection ViewportsMutex;
static TSet<FMetalViewport*> Viewports;

FMetalViewport::FMetalViewport(void* WindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat Format)
	: Drawable{nil}
	, BackBuffer{nullptr, nullptr}
	, Mutex{}
	, DrawableTextures{}
	, DisplayID{0}
	, Block{nullptr}
	, FrameAvailable{0}
	, LastCompleteFrame{nullptr}
	, bIsFullScreen{bInIsFullscreen}
#if PLATFORM_MAC
	, View{nullptr}
	, CustomPresent{nullptr}
#endif
{
#if PLATFORM_MAC
	MainThreadCall(^{
		FCocoaWindow* Window = (FCocoaWindow*)WindowHandle;
		const NSRect ContentRect = NSMakeRect(0, 0, InSizeX, InSizeY);
		View = [[FMetalView alloc] initWithFrame:ContentRect];
		[View setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		[View setWantsLayer:YES];

		CAMetalLayer* Layer = [CAMetalLayer new];

		CGFloat bgColor[] = { 0.0, 0.0, 0.0, 0.0 };
		Layer.edgeAntialiasingMask = 0;
		Layer.masksToBounds = YES;
		Layer.backgroundColor = CGColorCreate(CGColorSpaceCreateDeviceRGB(), bgColor);
		Layer.presentsWithTransaction = NO;
		Layer.anchorPoint = CGPointMake(0.5, 0.5);
		Layer.frame = ContentRect;
		Layer.magnificationFilter = kCAFilterNearest;
		Layer.minificationFilter = kCAFilterNearest;

		[Layer setDevice:GetMetalDeviceContext().GetDevice()];
		
		[Layer setFramebufferOnly:NO];
		[Layer removeAllAnimations];

		[View setLayer:Layer];

		[Window setContentView:View];
		[[Window standardWindowButton:NSWindowCloseButton] setAction:@selector(performClose:)];
	}, NSDefaultRunLoopMode, true);
#endif
	Resize(InSizeX, InSizeY, bInIsFullscreen, Format);
	
	{
		FScopeLock Lock(&ViewportsMutex);
		Viewports.Add(this);
	}
}

FMetalViewport::~FMetalViewport()
{
	if (Block)
	{
		FScopeLock BlockLock(&Mutex);
		if (GMetalSeparatePresentThread)
		{
			FPlatformRHIFramePacer::RemoveHandler(Block);
		}
		Block_release(Block);
		Block = nil;
	}
	{
		FScopeLock Lock(&ViewportsMutex);
		Viewports.Remove(this);
	}
	
	BackBuffer[0].SafeRelease();	// when the rest of the engine releases it, its framebuffers will be released too (those the engine knows about)
	BackBuffer[1].SafeRelease();
	check(!IsValidRef(BackBuffer[0]));
	check(!IsValidRef(BackBuffer[1]));
}

uint32 FMetalViewport::GetViewportIndex(EMetalViewportAccessFlag Accessor) const
{
	switch(Accessor)
	{
		case EMetalViewportAccessRHI:
			check(IsInRHIThread() || IsInRenderingThread());
			// Deliberate fall-through
		case EMetalViewportAccessDisplayLink: // Displaylink is not an index, merely an alias that avoids the check...
			return (GRHISupportsRHIThread && IsRunningRHIInSeparateThread()) ? EMetalViewportAccessRHI : EMetalViewportAccessRenderer;
		case EMetalViewportAccessRenderer:
			check(IsInRenderingThread());
			return Accessor;
		case EMetalViewportAccessGame:
			check(IsInGameThread());
			return EMetalViewportAccessRenderer;
		default:
			check(false);
			return EMetalViewportAccessRenderer;
	}
}

void FMetalViewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen,EPixelFormat Format)
{
	bIsFullScreen = bInIsFullscreen;
	uint32 Index = GetViewportIndex(EMetalViewportAccessGame);
	
	bool bUseHDR = GRHISupportsHDROutput && Format == GRHIHDRDisplayOutputFormat;
	
	// Format can come in as PF_Unknown in the LDR case or if this RHI doesn't support HDR.
	// So we'll fall back to BGRA8 in those cases.
	if (!bUseHDR)
	{
		Format = PF_B8G8R8A8;
	}
	
	mtlpp::PixelFormat MetalFormat = (mtlpp::PixelFormat)GPixelFormats[Format].PlatformFormat;
	
	if (IsValidRef(BackBuffer[Index]) && Format != BackBuffer[Index]->GetFormat())
	{
		// Really need to flush the RHI thread & GPU here...
		AddRef();
		ENQUEUE_RENDER_COMMAND(FlushPendingRHICommands)(
			[Viewport = this](FRHICommandListImmediate& RHICmdList)
			{
				GRHICommandList.GetImmediateCommandList().BlockUntilGPUIdle();
				Viewport->ReleaseDrawable();
				Viewport->Release();
			});
		
		// Issue a fence command to the rendering thread and wait for it to complete.
		FRenderCommandFence Fence;
		Fence.BeginFence();	
		Fence.Wait();
	}
    
#if PLATFORM_MAC
	MainThreadCall(^
	{
		CAMetalLayer* MetalLayer = (CAMetalLayer*)View.layer;
		
		MetalLayer.drawableSize = CGSizeMake(InSizeX, InSizeY);
		
		if (MetalFormat != (mtlpp::PixelFormat)MetalLayer.pixelFormat)
		{
			MetalLayer.pixelFormat = (MTLPixelFormat)MetalFormat;
		}
		
		if (bUseHDR != MetalLayer.wantsExtendedDynamicRangeContent)
		{
			MetalLayer.wantsExtendedDynamicRangeContent = bUseHDR;
		}
		
	}, NSDefaultRunLoopMode, true);
#else
	// A note on HDR in iOS
	// Setting the pixel format to one of the Apple XR formats is all you need.
	// iOS expects the app to output in sRGB regadless of the display
	// (even though Apple's HDR displays are P3)
	// and its compositor will do the conversion.
	{
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* IOSView = AppDelegate.IOSView;
		CAMetalLayer* MetalLayer = (CAMetalLayer*) IOSView.layer;
		
		if (MetalFormat != (mtlpp::PixelFormat) MetalLayer.pixelFormat)
		{
			MetalLayer.pixelFormat = (MTLPixelFormat) MetalFormat;
		}
		
		[IOSView UpdateRenderWidth:InSizeX andHeight:InSizeY];
	}
#endif

    {
        FScopeLock Lock(&Mutex);

		TRefCountPtr<FMetalSurface> NewBackBuffer;
		TRefCountPtr<FMetalSurface> DoubleBuffer;

		FRHITextureCreateDesc CreateDesc =
			FRHITextureCreateDesc::Create2D(TEXT("BackBuffer"), InSizeX, InSizeY, Format)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable);
		
		if (!GMetalSupportsIntermediateBackBuffer)
		{
			CreateDesc.AddFlags(ETextureCreateFlags::Presentable);
		}
		
		CreateDesc.SetInitialState(RHIGetDefaultResourceState(CreateDesc.Flags, false));

		NewBackBuffer = new FMetalSurface(CreateDesc);
		NewBackBuffer->Viewport = this;

        if (GMetalSupportsIntermediateBackBuffer && GMetalSeparatePresentThread)
        {
            DoubleBuffer = new FMetalSurface(CreateDesc);
            DoubleBuffer->Viewport = this;
        }

        BackBuffer[Index] = NewBackBuffer;
        if (GMetalSeparatePresentThread)
        {
            BackBuffer[EMetalViewportAccessRHI] = DoubleBuffer;
        }
        else
        {
            BackBuffer[EMetalViewportAccessRHI] = BackBuffer[Index];
        }
    }
}

TRefCountPtr<FMetalSurface> FMetalViewport::GetBackBuffer(EMetalViewportAccessFlag Accessor) const
{
	FScopeLock Lock(&Mutex);
	
	uint32 Index = GetViewportIndex(Accessor);
	check(IsValidRef(BackBuffer[Index]));
	return BackBuffer[Index];
}

#if PLATFORM_MAC
@protocol CAMetalLayerSPI <NSObject>
- (BOOL)isDrawableAvailable;
@end
#endif

id<CAMetalDrawable> FMetalViewport::GetDrawable(EMetalViewportAccessFlag Accessor)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalMakeDrawableTime);
    if (!Drawable
#if !PLATFORM_MAC
        || (Drawable.texture.width != BackBuffer[GetViewportIndex(Accessor)]->GetSizeX() || Drawable.texture.height != BackBuffer[GetViewportIndex(Accessor)]->GetSizeY())
#endif
        )
	{
		// Drawable changed, release the previously retained object.
		if (Drawable != nil)
		{
			[Drawable release];
			Drawable = nil;
		}

		@autoreleasepool
		{
			uint32 IdleStart = FPlatformTime::Cycles();

#if PLATFORM_MAC
			CAMetalLayer* CurrentLayer = (CAMetalLayer*)[View layer];
			if (GMetalNonBlockingPresent == 0 || [((id<CAMetalLayerSPI>)CurrentLayer) isDrawableAvailable])
			{
				Drawable = CurrentLayer ? [CurrentLayer nextDrawable] : nil;
			}

#if METAL_DEBUG_OPTIONS
			if (Drawable)
			{
				CGSize Size = Drawable.layer.drawableSize;
				if ((Size.width != BackBuffer[GetViewportIndex(Accessor)]->GetSizeX() || Size.height != BackBuffer[GetViewportIndex(Accessor)]->GetSizeY()))
				{
					UE_LOG(LogMetal, Display, TEXT("Viewport Size Mismatch: Drawable W:%f H:%f, Viewport W:%u H:%u"), Size.width, Size.height, BackBuffer[GetViewportIndex(Accessor)]->GetSizeX(), BackBuffer[GetViewportIndex(Accessor)]->GetSizeY());
				}
			}
#endif // METAL_DEBUG_OPTIONS

#else // PLATFORM_MAC
			CGSize Size;
			IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
			do
			{
				Drawable = [AppDelegate.IOSView MakeDrawable];
				if (Drawable != nil)
				{
					Size.width = Drawable.texture.width;
					Size.height = Drawable.texture.height;
				}
				else
				{
					FPlatformProcess::SleepNoStats(0.001f);
				}
			}
			while (Drawable == nil || Size.width != BackBuffer[GetViewportIndex(Accessor)]->GetSizeX() || Size.height != BackBuffer[GetViewportIndex(Accessor)]->GetSizeY());

#endif // PLATFORM_MAC

			if (IsInRHIThread())
			{
				GWorkingRHIThreadStallTime += FPlatformTime::Cycles() - IdleStart;
			}
			else
			{
				GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += FPlatformTime::Cycles() - IdleStart;
				GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
			}

			// Retain the drawable here or it will be released when the
			// autorelease pool goes out of scope.
			if (Drawable != nil)
			{
				[Drawable retain];
			}
		} // autoreleasepool
	}

	return Drawable;
}

FMetalTexture FMetalViewport::GetDrawableTexture(EMetalViewportAccessFlag Accessor)
{
	id<CAMetalDrawable> CurrentDrawable = GetDrawable(Accessor);
#if METAL_DEBUG_OPTIONS
	@autoreleasepool
	{
#if PLATFORM_MAC
		CAMetalLayer* CurrentLayer = (CAMetalLayer*)[View layer];
#else
		CAMetalLayer* CurrentLayer = (CAMetalLayer*)[[IOSAppDelegate GetDelegate].IOSView layer];
#endif
		
		uint32 Index = GetViewportIndex(Accessor);
		CGSize Size = CurrentLayer.drawableSize;
		if (CurrentDrawable.texture.width != BackBuffer[Index]->GetSizeX() || CurrentDrawable.texture.height != BackBuffer[Index]->GetSizeY())
		{
			UE_LOG(LogMetal, Display, TEXT("Viewport Size Mismatch: Drawable W:%f H:%f, Texture W:%llu H:%llu, Viewport W:%u H:%u"), Size.width, Size.height, CurrentDrawable.texture.width, CurrentDrawable.texture.height, BackBuffer[Index]->GetSizeX(), BackBuffer[Index]->GetSizeY());
		}
	}
#endif
	DrawableTextures[Accessor] = CurrentDrawable.texture;
	return CurrentDrawable.texture;
}

ns::AutoReleased<FMetalTexture> FMetalViewport::GetCurrentTexture(EMetalViewportAccessFlag Accessor)
{
	return DrawableTextures[Accessor];
}

void FMetalViewport::ReleaseDrawable()
{
	if (!GMetalSeparatePresentThread)
	{
		if (Drawable != nil)
		{
			[Drawable release];
			Drawable = nil;
		}

		if (!GMetalSupportsIntermediateBackBuffer && IsValidRef(BackBuffer[GetViewportIndex(EMetalViewportAccessRHI)]))
		{
			BackBuffer[GetViewportIndex(EMetalViewportAccessRHI)]->Texture = nil;
		}
	}
}

#if PLATFORM_MAC
NSWindow* FMetalViewport::GetWindow() const
{
	return [View window];
}
#endif

void FMetalViewport::Present(FMetalCommandQueue& CommandQueue, bool bLockToVsync)
{
	FScopeLock Lock(&Mutex);
	
	bool bIsLiveResize = false;
#if PLATFORM_MAC
	NSNumber* ScreenId = [View.window.screen.deviceDescription objectForKey:@"NSScreenNumber"];
	DisplayID = ScreenId.unsignedIntValue;
	bIsLiveResize = View.inLiveResize;
	{
		FCAMetalLayer* CurrentLayer = (FCAMetalLayer*)[View layer];
		CurrentLayer.displaySyncEnabled = bLockToVsync || (!(IsRunningGame() && bIsFullScreen));
	}
#endif
	
	LastCompleteFrame = GetBackBuffer(EMetalViewportAccessRHI);
	FPlatformAtomics::InterlockedExchange(&FrameAvailable, 1);
	
	if (!Block)
	{
		Block = Block_copy(^(uint32 InDisplayID, double OutputSeconds, double OutputDuration)
		{
#if !PLATFORM_MAC
			uint32 FramePace = FPlatformRHIFramePacer::GetFramePace();
			float MinPresentDuration = FramePace ? (1.0f / (float)FramePace) : 0.0f;
#endif
			bool bIsInLiveResize = false;
#if PLATFORM_MAC
			bIsInLiveResize = View.inLiveResize;
#endif
			if (FrameAvailable > 0 && (InDisplayID == 0 || (DisplayID == InDisplayID && !bIsInLiveResize)))
			{
				FPlatformAtomics::InterlockedDecrement(&FrameAvailable);
				id<CAMetalDrawable> LocalDrawable = [GetDrawable(EMetalViewportAccessDisplayLink) retain];
				{
					FScopeLock BlockLock(&Mutex);
#if PLATFORM_MAC
					bIsInLiveResize = View.inLiveResize;
#endif
					
					if (LocalDrawable && LocalDrawable.texture && (InDisplayID == 0 || !bIsInLiveResize))
					{
						mtlpp::CommandBuffer CurrentCommandBuffer = CommandQueue.CreateCommandBuffer();
						check(CurrentCommandBuffer);
						
#if ENABLE_METAL_GPUPROFILE
						FMetalProfiler* Profiler = FMetalProfiler::GetProfiler();
						FMetalCommandBufferStats* Stats = Profiler->AllocateCommandBuffer(CurrentCommandBuffer, 0);
#endif
						
						if (GMetalSupportsIntermediateBackBuffer)
						{
							TRefCountPtr<FMetalSurface> Texture = LastCompleteFrame;
							check(IsValidRef(Texture));
							
							FMetalTexture Src = Texture->Texture;
							FMetalTexture Dst = LocalDrawable.texture;
							
							NSUInteger Width = FMath::Min(Src.GetWidth(), Dst.GetWidth());
							NSUInteger Height = FMath::Min(Src.GetHeight(), Dst.GetHeight());
							
							mtlpp::BlitCommandEncoder Encoder = CurrentCommandBuffer.BlitCommandEncoder();
							check(Encoder.GetPtr());
#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
							FMetalBlitCommandEncoderDebugging Debugging;
							if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
							{
								FMetalCommandBufferDebugging CmdDebug = FMetalCommandBufferDebugging::Get(CurrentCommandBuffer);
								Debugging = FMetalBlitCommandEncoderDebugging(Encoder, CmdDebug);
							}
#endif
							METAL_GPUPROFILE(Profiler->EncodeBlit(Stats, __FUNCTION__));

							Encoder.Copy(Src, 0, 0, mtlpp::Origin(0, 0, 0), mtlpp::Size(Width, Height, 1), Dst, 0, 0, mtlpp::Origin(0, 0, 0));
							METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, Debugging.Copy(Src, 0, 0, mtlpp::Origin(0, 0, 0), mtlpp::Size(Width, Height, 1), Dst, 0, 0, mtlpp::Origin(0, 0, 0)));

							Encoder.EndEncoding();
							METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, Debugging.EndEncoder());

							mtlpp::CommandBufferHandler H = [Src, Dst](const mtlpp::CommandBuffer &) {
								// void
							};

							CurrentCommandBuffer.AddCompletedHandler(H);

							[Drawable release];
							Drawable = nil;
						}
						
						// This is a bit different than the usual pattern.
						// This command buffer here is committed directly, instead of going through
						// FMetalCommandList::Commit. So long as Present() is called within
						// high level RHI BeginFrame/EndFrame this will not fine.
						// Otherwise the recording of the Present time will be offset by one in the
						// FMetalGPUProfiler frame indices.
      
#if PLATFORM_MAC
						FMetalView* theView = View;
						mtlpp::CommandBufferHandler C = [LocalDrawable, theView](const mtlpp::CommandBuffer & cmd_buf) {
#else
						mtlpp::CommandBufferHandler C = [LocalDrawable](const mtlpp::CommandBuffer & cmd_buf) {
#endif
							FMetalGPUProfiler::RecordPresent(cmd_buf);
							[LocalDrawable release];
#if PLATFORM_MAC
							MainThreadCall(^{
								FCocoaWindow* Window = (FCocoaWindow*)[theView window];
								[Window startRendering];
							}, NSDefaultRunLoopMode, false);
#endif
						};
						
#if PLATFORM_MAC		// Mac needs the older way to present otherwise we end up with bad behaviour of the completion handlers that causes GPU timeouts.
						mtlpp::CommandBufferHandler H = [LocalDrawable](mtlpp::CommandBuffer const&)
						{
							[LocalDrawable present];
						};
								
						CurrentCommandBuffer.AddCompletedHandler(C);
						CurrentCommandBuffer.AddScheduledHandler(H);

#else // PLATFORM_MAC
						CurrentCommandBuffer.AddCompletedHandler(C);

						if (MinPresentDuration && GEnablePresentPacing)
						{
							CurrentCommandBuffer.PresentAfterMinimumDuration(LocalDrawable, 1.0f/(float)FramePace);
						}
						else
						{
							CurrentCommandBuffer.Present(LocalDrawable);
						}
#endif // PLATFORM_MAC

						METAL_GPUPROFILE(Stats->End(CurrentCommandBuffer));
						CommandQueue.CommitCommandBuffer(CurrentCommandBuffer);
					}
				}
			}
		});
		
		if (GMetalSeparatePresentThread)
		{
			FPlatformRHIFramePacer::AddHandler(Block);
		}
	}
	
	if (bIsLiveResize || !GMetalSeparatePresentThread)
	{
		Block(0, 0.0, 0.0);
	}
	
	if (!(GRHISupportsRHIThread && IsRunningRHIInSeparateThread()))
	{
		Swap();
	}
}

void FMetalViewport::Swap()
{
	if (GMetalSeparatePresentThread)
	{
		FScopeLock Lock(&Mutex);
		
		check(IsValidRef(BackBuffer[0]));
		check(IsValidRef(BackBuffer[1]));
		
		TRefCountPtr<FMetalSurface> BB0 = BackBuffer[0];
		TRefCountPtr<FMetalSurface> BB1 = BackBuffer[1];
		
		BackBuffer[0] = BB1;
		BackBuffer[1] = BB0;
	}
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FMetalDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat PreferredPixelFormat)
{
	check( IsInGameThread() );
	@autoreleasepool {
	return new FMetalViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
}

void FMetalDynamicRHI::RHIResizeViewport(FRHIViewport* Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	RHIResizeViewport(Viewport, SizeX, SizeY, bIsFullscreen, PF_Unknown);
}

void FMetalDynamicRHI::RHIResizeViewport(FRHIViewport* ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat Format)
{
	@autoreleasepool {
	check( IsInGameThread() );

	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	Viewport->Resize(SizeX, SizeY, bIsFullscreen, Format);
	}
}

void FMetalDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FMetalRHICommandContext::RHIBeginDrawingViewport(FRHIViewport* ViewportRHI, FRHITexture* RenderTargetRHI)
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIBeginDrawingViewport(FRHIViewport* ViewportRHI, FRHITexture* RenderTargetRHI)
{
	@autoreleasepool {
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	check(Viewport);

	((FMetalDeviceContext*)Context)->BeginDrawingViewport(Viewport);

	// Set the render target and viewport.
	if (RenderTargetRHI)
	{
		FRHIRenderTargetView RTV(RenderTargetRHI, GIsEditor ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);
		SetRenderTargets(1, &RTV, nullptr);
	}
	else
	{
		FRHIRenderTargetView RTV(Viewport->GetBackBuffer(EMetalViewportAccessRHI), GIsEditor ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad);
		SetRenderTargets(1, &RTV, nullptr);
	}
	}
}

void FMetalRHICommandContext::RHIEndDrawingViewport(FRHIViewport* ViewportRHI,bool bPresent,bool bLockToVsync)
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndDrawingViewport(FRHIViewport* ViewportRHI,bool bPresent,bool bLockToVsync)
{
	@autoreleasepool {
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	((FMetalDeviceContext*)Context)->EndDrawingViewport(Viewport, bPresent, bLockToVsync);
	}
}

FTexture2DRHIRef FMetalDynamicRHI::RHIGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	@autoreleasepool {
	FMetalViewport* Viewport = ResourceCast(ViewportRHI);
	return FTexture2DRHIRef(Viewport->GetBackBuffer(EMetalViewportAccessRenderer).GetReference());
	}
}

void FMetalDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	if (GMetalSeparatePresentThread && (GRHISupportsRHIThread && IsRunningRHIInSeparateThread()))
	{
		FScopeLock Lock(&ViewportsMutex);
		for (FMetalViewport* Viewport : Viewports)
		{
			Viewport->Swap();
		}
	}
}
