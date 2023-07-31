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
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

enum EMetalViewportAccessFlag
{
	EMetalViewportAccessRHI,
	EMetalViewportAccessRenderer,
	EMetalViewportAccessGame,
	EMetalViewportAccessDisplayLink
};

class FMetalCommandQueue;

typedef void (^FMetalViewportPresentHandler)(uint32 CGDirectDisplayID, double OutputSeconds, double OutputDuration);

class FMetalViewport : public FRHIViewport
{
public:
	FMetalViewport(void* WindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen,EPixelFormat Format);
	~FMetalViewport();

	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen,EPixelFormat Format);
	
	TRefCountPtr<FMetalSurface> GetBackBuffer(EMetalViewportAccessFlag Accessor) const;
	id<CAMetalDrawable> GetDrawable(EMetalViewportAccessFlag Accessor);
	FMetalTexture GetDrawableTexture(EMetalViewportAccessFlag Accessor);
	ns::AutoReleased<FMetalTexture> GetCurrentTexture(EMetalViewportAccessFlag Accessor);
	void ReleaseDrawable(void);

	// supports pulling the raw MTLTexture
	virtual void* GetNativeBackBufferTexture() const override { return GetBackBuffer(EMetalViewportAccessRenderer).GetReference(); }
	virtual void* GetNativeBackBufferRT() const override { return (const_cast<FMetalViewport *>(this))->GetDrawableTexture(EMetalViewportAccessRenderer); }
	
#if PLATFORM_MAC
	NSWindow* GetWindow() const;
	
	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override
	{
		CustomPresent = InCustomPresent;
	}

	virtual FRHICustomPresent* GetCustomPresent() const override { return CustomPresent; }
#endif
	
	void Present(FMetalCommandQueue& CommandQueue, bool bLockToVsync);
	void Swap();
	
private:
	uint32 GetViewportIndex(EMetalViewportAccessFlag Accessor) const;

private:
	id<CAMetalDrawable> Drawable;
	TRefCountPtr<FMetalSurface> BackBuffer[2];
	mutable FCriticalSection Mutex;
	
	ns::AutoReleased<FMetalTexture> DrawableTextures[2];
	
	uint32 DisplayID;
	FMetalViewportPresentHandler Block;
	volatile int32 FrameAvailable;
	TRefCountPtr<FMetalSurface> LastCompleteFrame;
	bool bIsFullScreen;

#if PLATFORM_MAC
	FMetalView* View;
	FRHICustomPresent* CustomPresent;
#endif
};

template<>
struct TMetalResourceTraits<FRHIViewport>
{
	typedef FMetalViewport TConcreteType;
};
