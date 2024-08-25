// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalViewport.h: Metal viewport RHI definitions.
=============================================================================*/

#pragma once

#if PLATFORM_MAC
#include "Mac/CocoaTextView.h"
@interface FMetalView : FCocoaTextView
@end
#endif
#include "HAL/PlatformFramePacer.h"
THIRD_PARTY_INCLUDES_START
#include "MetalInclude.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_VISIONOS
#import <CompositorServices/CompositorServices.h>
#endif

enum EMetalViewportAccessFlag
{
	EMetalViewportAccessRHI,
	EMetalViewportAccessRenderer,
	EMetalViewportAccessGame,
	EMetalViewportAccessDisplayLink
};

class FMetalCommandQueue;

#if PLATFORM_VISIONOS
namespace MetalRHIVisionOS
{
    struct BeginRenderingImmersiveParams;
    struct PresentImmersiveParams;
}
#endif

typedef void (^FMetalViewportPresentHandler)(uint32 CGDirectDisplayID, double OutputSeconds, double OutputDuration);

class FMetalViewport : public FRHIViewport
{
public:
	FMetalViewport(void* WindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen,EPixelFormat Format);
	~FMetalViewport();

	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen,EPixelFormat Format);
	
	TRefCountPtr<FMetalSurface> GetBackBuffer(EMetalViewportAccessFlag Accessor) const;
	CA::MetalDrawable* GetDrawable(EMetalViewportAccessFlag Accessor);
	MTL::Texture* GetDrawableTexture(EMetalViewportAccessFlag Accessor);
	MTL::Texture* GetCurrentTexture(EMetalViewportAccessFlag Accessor);
	void ReleaseDrawable(void);

	// supports pulling the raw MTLTexture
	virtual void* GetNativeBackBufferTexture() const override { return GetBackBuffer(EMetalViewportAccessRenderer).GetReference(); }
	virtual void* GetNativeBackBufferRT() const override { return (const_cast<FMetalViewport *>(this))->GetDrawableTexture(EMetalViewportAccessRenderer); }
	
#if PLATFORM_MAC
	NSWindow* GetWindow() const;
#endif
    
#if PLATFORM_MAC || PLATFORM_VISIONOS
	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override
	{
		CustomPresent = InCustomPresent;
	}

	virtual FRHICustomPresent* GetCustomPresent() const override { return CustomPresent; }
#endif
	
	void Present(FMetalCommandQueue& CommandQueue, bool bLockToVsync);
	void Swap();
	
#if PLATFORM_VISIONOS
	void GetDrawableImmersiveTextures(EMetalViewportAccessFlag Accessor, cp_drawable_t SwiftDrawable, MTL::Texture*& OutColorTexture, MTL::Texture*& OutDepthTexture );
    void PresentImmersive(const MetalRHIVisionOS::PresentImmersiveParams& Params);
#endif
	
private:
	uint32 GetViewportIndex(EMetalViewportAccessFlag Accessor) const;

private:
#if PLATFORM_VISIONOS
	CP_OBJECT_cp_layer_renderer* SwiftLayer = nullptr;
#endif
	
	CA::MetalDrawable* Drawable;
	TRefCountPtr<FMetalSurface> BackBuffer[2];
	mutable FCriticalSection Mutex;
	
    MTL::Texture* DrawableTextures[2];
	
	uint32 DisplayID;
	FMetalViewportPresentHandler Block;
	volatile int32 FrameAvailable;
	TRefCountPtr<FMetalSurface> LastCompleteFrame;
	bool bIsFullScreen;

#if PLATFORM_MAC
	FMetalView* View;
#endif
    
#if PLATFORM_MAC || PLATFORM_VISIONOS
    FCustomPresentRHIRef CustomPresent;
#endif
};

template<>
struct TMetalResourceTraits<FRHIViewport>
{
	typedef FMetalViewport TConcreteType;
};
