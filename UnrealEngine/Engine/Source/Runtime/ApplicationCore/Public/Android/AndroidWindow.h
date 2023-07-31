// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include <android/native_window.h> 
#if USE_ANDROID_JNI
#include <android/native_window_jni.h>
#endif


/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
 
class APPLICATIONCORE_API FAndroidWindow : public FGenericWindow
{
public:
	~FAndroidWindow();

	/** Create a new FAndroidWindow.
	 *
	 * @param OwnerWindow		The SlateWindow for which we are crating a backing AndroidWindow
	 * @param InParent			Parent iOS window; usually NULL.
	 */
	static TSharedRef<FAndroidWindow> Make();

	
	virtual void* GetOSWindowHandle() const override { return nullptr; }

	void Initialize( class FAndroidApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FAndroidWindow >& InParent, const bool bShowImmediately );

	/** Returns the rectangle of the screen the window is associated with */
	virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const override;

	virtual void SetOSWindowHandle(void*);

	static FPlatformRect GetScreenRect(bool bUseEventThreadWindow = false);
	static void InvalidateCachedScreenRect();

	// When bUseEventThreadWindow == false this uses dimensions cached when the game thread processes android events.
	// When bUseEventThreadWindow == true this uses dimensions directly from the android event thread, unless called from event thread this requires acquiring GAndroidWindowLock to use.
	static void CalculateSurfaceSize(int32_t& SurfaceWidth, int32_t& SurfaceHeight, bool bUseEventThreadWindow = false);
	static bool OnWindowOrientationChanged(bool bIsPortrait);

	static int32 GetDepthBufferPreference();
	
	static void AcquireWindowRef(ANativeWindow* InWindow);
	static void ReleaseWindowRef(ANativeWindow* InWindow);

	// This returns the current hardware window as set from the event thread.
	static void* GetHardwareWindow_EventThread();
	static void SetHardwareWindow_EventThread(void* InWindow);

	/** Waits on the current thread for a hardware window and returns it. 
	 *  May return nullptr if the application is shutting down.
	 */
	static void* WaitForHardwareWindow();

	static bool IsPortraitOrientation();
	static FVector4 GetSafezone(bool bPortrait);

	// called by the Android event thread to initially set the current window dimensions.
	static void SetWindowDimensions_EventThread(ANativeWindow* DimensionWindow);

	// Called by the event manager to update the cached window dimensions to match the event it is processing.
	static void EventManagerUpdateWindowDimensions(int32 Width, int32 Height);

protected:
	/** @return true if the native window is currently in fullscreen mode, false otherwise */
	virtual EWindowMode::Type GetWindowMode() const override { return EWindowMode::Fullscreen; }

private:
	/** called from GetScreenRect function */
	/** test cached values from the latest computations stored by CacheRect to decide their validity with the provided arguments */
	static bool IsCachedRectValid(bool bUseEventThreadWindow, const float RequestedContentScaleFactor, const int32 RequestedMobileResX, const int32 RequestedMobileResY, ANativeWindow* Window);
	/** caches some values used to compute the size of the window by GetScreenRect function */
	static void CacheRect(bool bUseEventThreadWindow, const int32 Width, const int32 Height, const float RequestedContentScaleFactor, const int32 RequestedMobileResX, const int32 RequestedMobileResY, ANativeWindow* Window);

	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	FAndroidWindow();

	FAndroidApplication* OwningApplication;

	/** Store the window region size for querying whether a point lies within the window */
	int32 RegionX;
	int32 RegionY;

	static void* NativeWindow;

	// Waits for the event thread to report an initial window size.
	static bool WaitForWindowDimensions();

	static bool bAreCachedNativeDimensionsValid;
	static int32 CachedNativeWindowWidth;
	static int32 CachedNativeWindowHeight;
};
