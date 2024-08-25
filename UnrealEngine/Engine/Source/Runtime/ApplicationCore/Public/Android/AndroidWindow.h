// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include <android/native_window.h> 
#if USE_ANDROID_JNI
#include <android/native_window_jni.h>
#endif


/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
 
class FAndroidWindow : public FGenericWindow
{
public:
	APPLICATIONCORE_API ~FAndroidWindow();

	/** Create a new FAndroidWindow.
	 *
	 * @param OwnerWindow		The SlateWindow for which we are crating a backing AndroidWindow
	 * @param InParent			Parent iOS window; usually NULL.
	 */
	static APPLICATIONCORE_API TSharedRef<FAndroidWindow> Make();

	
	virtual void* GetOSWindowHandle() const override { return nullptr; }

	APPLICATIONCORE_API void Initialize( class FAndroidApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FAndroidWindow >& InParent, const bool bShowImmediately );

	/** Returns the rectangle of the screen the window is associated with */
	APPLICATIONCORE_API virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const override;

	APPLICATIONCORE_API virtual void SetOSWindowHandle(void*);

	static APPLICATIONCORE_API FPlatformRect GetScreenRect(bool bUseEventThreadWindow = false);
	static APPLICATIONCORE_API void InvalidateCachedScreenRect();

	// When bUseEventThreadWindow == false this uses dimensions cached when the game thread processes android events.
	// When bUseEventThreadWindow == true this uses dimensions directly from the android event thread, unless called from event thread this requires acquiring GAndroidWindowLock to use.
	static APPLICATIONCORE_API void CalculateSurfaceSize(int32_t& SurfaceWidth, int32_t& SurfaceHeight, bool bUseEventThreadWindow = false);
	static APPLICATIONCORE_API bool OnWindowOrientationChanged(EDeviceScreenOrientation DeviceScreenOrientation);

	static APPLICATIONCORE_API int32 GetDepthBufferPreference();
	
	static APPLICATIONCORE_API void AcquireWindowRef(ANativeWindow* InWindow);
	static APPLICATIONCORE_API void ReleaseWindowRef(ANativeWindow* InWindow);

	// This returns the current hardware window as set from the event thread.
	static APPLICATIONCORE_API void* GetHardwareWindow_EventThread();
	static APPLICATIONCORE_API void SetHardwareWindow_EventThread(void* InWindow);

	/** Waits on the current thread for a hardware window and returns it. 
	 *  May return nullptr if the application is shutting down.
	 */
	static APPLICATIONCORE_API void* WaitForHardwareWindow();

	static APPLICATIONCORE_API bool IsPortraitOrientation();
	static APPLICATIONCORE_API FVector4 GetSafezone(bool bPortrait);

	// called by the Android event thread to initially set the current window dimensions.
	static APPLICATIONCORE_API void SetWindowDimensions_EventThread(ANativeWindow* DimensionWindow);

	// Called by the event manager to update the cached window dimensions to match the event it is processing.
	static APPLICATIONCORE_API void EventManagerUpdateWindowDimensions(int32 Width, int32 Height);

protected:
	/** @return true if the native window is currently in fullscreen mode, false otherwise */
	virtual EWindowMode::Type GetWindowMode() const override { return EWindowMode::Fullscreen; }

private:
	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	APPLICATIONCORE_API FAndroidWindow();

	FAndroidApplication* OwningApplication;

	/** Store the window region size for querying whether a point lies within the window */
	int32 RegionX;
	int32 RegionY;

	static APPLICATIONCORE_API void* NativeWindow;

	// Waits for the event thread to report an initial window size.
	static APPLICATIONCORE_API bool WaitForWindowDimensions();

	static APPLICATIONCORE_API bool bAreCachedNativeDimensionsValid;
	static APPLICATIONCORE_API int32 CachedNativeWindowWidth;
	static APPLICATIONCORE_API int32 CachedNativeWindowHeight;
};
