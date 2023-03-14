// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidWindow.h"
#include "Android/AndroidWindowUtils.h"
#include "Android/AndroidEventManager.h"

#if USE_ANDROID_JNI
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#endif
#include "HAL/OutputDevices.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformStackWalk.h"

int GAndroidPropagateAlpha = 0;

struct FCachedWindowRect
{
	FCachedWindowRect() : WindowWidth(-1), WindowHeight(-1), WindowInit(false), ContentScaleFactor(-1.0f), MobileResX(-1), MobileResY(-1), Window_EventThread(nullptr)
	{
	}

	int32 WindowWidth;
	int32 WindowHeight;
	bool WindowInit;
	float ContentScaleFactor;
	int32 MobileResX;
	int32 MobileResY;
	ANativeWindow* Window_EventThread;
};


// Cached calculated screen resolution
static FCachedWindowRect CachedWindowRect;
static FCachedWindowRect CachedWindowRect_EventThread;

static void ClearCachedWindowRects()
{
	CachedWindowRect.WindowInit = false;
	CachedWindowRect_EventThread.WindowInit = false;
}

static int32 GSurfaceViewWidth = -1;
static int32 GSurfaceViewHeight = -1;

void* FAndroidWindow::NativeWindow = NULL;

FAndroidWindow::~FAndroidWindow()
{
	//       Use NativeWindow_Destroy() instead.
}

TSharedRef<FAndroidWindow> FAndroidWindow::Make()
{
	return MakeShareable( new FAndroidWindow() );
}

FAndroidWindow::FAndroidWindow()
{
}

void FAndroidWindow::Initialize( class FAndroidApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FAndroidWindow >& InParent, const bool bShowImmediately )
{
	OwningApplication = Application;
	Definition = InDefinition;
}

bool FAndroidWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();

	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;

	return true;
}


void FAndroidWindow::SetOSWindowHandle(void* InWindow)
{
	// not expecting a window to be supplied on android.
	check(InWindow == nullptr);
}


//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeSetObbInfo(String PackageName, int Version, int PatchVersion);"
static bool GAndroidIsPortrait = false;
static int GAndroidDepthBufferPreference = 0;
static FVector4 GAndroidPortraitSafezone = FVector4(-1.0f, -1.0f, -1.0f, -1.0f);
static FVector4 GAndroidLandscapeSafezone = FVector4(-1.0f, -1.0f, -1.0f, -1.0f);
#if USE_ANDROID_JNI
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeSetWindowInfo(JNIEnv* jenv, jobject thiz, jboolean bIsPortrait, jint DepthBufferPreference, jint PropagateAlpha)
{
	ClearCachedWindowRects();
	GAndroidIsPortrait = bIsPortrait == JNI_TRUE;
	GAndroidDepthBufferPreference = DepthBufferPreference;
	GAndroidPropagateAlpha = PropagateAlpha;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("App is running in %s\n"), GAndroidIsPortrait ? TEXT("Portrait") : TEXT("Landscape"));
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AndroidPropagateAlpha =  %d\n"), GAndroidPropagateAlpha);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeSetSurfaceViewInfo(JNIEnv* jenv, jobject thiz, jint width, jint height)
{
	GSurfaceViewWidth = width;
	GSurfaceViewHeight = height;
	UE_LOG(LogAndroid, Log, TEXT("nativeSetSurfaceViewInfo width=%d and height=%d"), GSurfaceViewWidth, GSurfaceViewHeight);
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeSetSafezoneInfo(JNIEnv* jenv, jobject thiz, jboolean bIsPortrait, jfloat left, jfloat top, jfloat right, jfloat bottom)
{
	if (bIsPortrait)
	{
		GAndroidPortraitSafezone.X = left;
		GAndroidPortraitSafezone.Y = top;
		GAndroidPortraitSafezone.Z = right;
		GAndroidPortraitSafezone.W = bottom;
	}
	else
	{
		GAndroidLandscapeSafezone.X = left;
		GAndroidLandscapeSafezone.Y = top;
		GAndroidLandscapeSafezone.Z = right;
		GAndroidLandscapeSafezone.W = bottom;
	}
}
#endif

bool FAndroidWindow::bAreCachedNativeDimensionsValid = false;
int32 FAndroidWindow::CachedNativeWindowWidth = -1;
int32 FAndroidWindow::CachedNativeWindowHeight = -1;

bool FAndroidWindow::IsPortraitOrientation()
{
	return GAndroidIsPortrait;
}

FVector4 FAndroidWindow::GetSafezone(bool bPortrait)
{
	return bPortrait ? GAndroidPortraitSafezone : GAndroidLandscapeSafezone;
}

int32 FAndroidWindow::GetDepthBufferPreference()
{
	return GAndroidDepthBufferPreference;
}

void FAndroidWindow::InvalidateCachedScreenRect()
{
	ClearCachedWindowRects();
}

void FAndroidWindow::AcquireWindowRef(ANativeWindow* InWindow)
{
#if USE_ANDROID_JNI
	ANativeWindow_acquire(InWindow);
#endif
}

void FAndroidWindow::ReleaseWindowRef(ANativeWindow* InWindow)
{
#if USE_ANDROID_JNI
	ANativeWindow_release(InWindow);
#endif
}

 void FAndroidWindow::SetHardwareWindow_EventThread(void* InWindow)
{
#if USE_ANDROID_EVENTS
	check(IsInAndroidEventThread());
#endif
	NativeWindow = InWindow; //using raw native window handle for now. Could be changed to use AndroidWindow later if needed
}

void* FAndroidWindow::GetHardwareWindow_EventThread()
{
	return NativeWindow;
}

bool FAndroidWindow::WaitForWindowDimensions()
{
	while (!bAreCachedNativeDimensionsValid)
	{
		if (IsEngineExitRequested()
#if USE_ANDROID_EVENTS
		|| FAppEventManager::GetInstance()->WaitForEventInQueue(EAppEventState::APP_EVENT_STATE_ON_DESTROY, 0.0f)
#endif
			)
		{
			// Application is shutting down
			return false;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return true;
}

// To be called during initialization from the event thread.
// once set the dimensions are 'valid' and further changes are updated via FAppEventManager::Tick 
void FAndroidWindow::SetWindowDimensions_EventThread(ANativeWindow* DimensionWindow)
{
	if(bAreCachedNativeDimensionsValid == false)
	{
#if USE_ANDROID_JNI
		CachedNativeWindowWidth = ANativeWindow_getWidth(DimensionWindow);
		CachedNativeWindowHeight = ANativeWindow_getHeight(DimensionWindow);
#else // Lumin case.
		int32 ResWidth, ResHeight;
		if (FPlatformMisc::GetOverrideResolution(ResWidth, ResHeight))
		{
			CachedNativeWindowWidth = ResWidth;
			CachedNativeWindowHeight = ResHeight;
		}
#endif
		FPlatformMisc::MemoryBarrier();
		bAreCachedNativeDimensionsValid = true;
	}
}

// Update the dimensions from FAppEventManager::Tick based on messages posted from the event thread.
void FAndroidWindow::EventManagerUpdateWindowDimensions(int32 Width, int32 Height)
{
	check(bAreCachedNativeDimensionsValid);
	check(Width >= 0 && Height >= 0);

	bool bChanged = CachedNativeWindowWidth != Width || CachedNativeWindowHeight != Height;

	CachedNativeWindowWidth = Width;
	CachedNativeWindowHeight = Height;

	if (bChanged)
	{
		InvalidateCachedScreenRect();
	}
}

void* FAndroidWindow::WaitForHardwareWindow()
{
	// Sleep if the hardware window isn't currently available.
	// The Window may not exist if the activity is pausing/resuming, in which case we make this thread wait
	// This case will come up frequently as a result of the DON flow in Gvr.
	// Until the app is fully resumed. It would be nicer if this code respected the lifecycle events
	// of an android app instead, but all of those events are handled on a separate thread and it would require
	// significant re-architecturing to do.

	// Before sleeping, we peek into the event manager queue to see if it contains an ON_DESTROY event, 
	// in which case, we exit the loop to allow the application to exit before a window has been created.
	// It is not sufficient to check the IsEngineExitRequested() global function, as the handler reacting to the APP_EVENT_STATE_ON_DESTROY
	// may be running in the same thread as this method and therefore lead to a deadlock.

	void* WindowEventThread = GetHardwareWindow_EventThread();
	while (WindowEventThread == nullptr)
	{
#if USE_ANDROID_EVENTS
		if (IsEngineExitRequested() || FAppEventManager::GetInstance()->WaitForEventInQueue(EAppEventState::APP_EVENT_STATE_ON_DESTROY, 0.0f))
		{
			// Application is shutting down soon, abort the wait and return nullptr
			return nullptr;
		}
#endif
		FPlatformProcess::Sleep(0.001f);
		WindowEventThread = GetHardwareWindow_EventThread();
	}
	return WindowEventThread;
}

#if USE_ANDROID_JNI
extern bool AndroidThunkCpp_IsOculusMobileApplication();
#endif

bool FAndroidWindow::IsCachedRectValid(bool bUseEventThreadWindow, const float RequestedContentScaleFactor, const int32 RequestedMobileResX, const int32 RequestedMobileResY, ANativeWindow* Window)
{
	// window must be valid when bUseEventThreadWindow and null when !bUseEventThreadWindow.
	check((Window != nullptr) == bUseEventThreadWindow);

	const FCachedWindowRect& CachedRect = bUseEventThreadWindow ? CachedWindowRect_EventThread : CachedWindowRect;

	if (!CachedRect.WindowInit)
	{
		return false;
	}

	bool bValidCache = true;

	if (CachedRect.ContentScaleFactor != RequestedContentScaleFactor )
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedContentScaleFactor different %f != %f, not using res cache (%d)"), RequestedContentScaleFactor, CachedWindowRect.ContentScaleFactor, (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	if (CachedRect.MobileResX != RequestedMobileResX)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedMobileResX different %d != %d, not using res cache (%d)"), RequestedMobileResX, CachedWindowRect.MobileResX, (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	if (CachedRect.MobileResY != RequestedMobileResY)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedMobileResY different %d != %d, not using res cache (%d)"), RequestedMobileResY, CachedWindowRect.MobileResY, (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	if (CachedRect.Window_EventThread != Window)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** Window different, not using res cache (%d)"), (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	if (CachedRect.WindowWidth <= 8)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** WindowWidth is %d, not using res cache (%d)"), CachedRect.WindowWidth, (int32)bUseEventThreadWindow);
		bValidCache = false;
	}

	return bValidCache;
}

void FAndroidWindow::CacheRect(bool bUseEventThreadWindow, const int32 Width, const int32 Height, const float RequestedContentScaleFactor, const int32 RequestedMobileResX, const int32 RequestedMobileResY, ANativeWindow* Window)
{
	check(Window != nullptr || !bUseEventThreadWindow);

	FCachedWindowRect& CachedRect = bUseEventThreadWindow ? CachedWindowRect_EventThread : CachedWindowRect;

	CachedRect.WindowWidth = Width;
	CachedRect.WindowHeight = Height;
	CachedRect.WindowInit = true;
	CachedRect.ContentScaleFactor = RequestedContentScaleFactor;
	CachedRect.MobileResX = RequestedMobileResX;
	CachedRect.MobileResY = RequestedMobileResY;
	CachedRect.Window_EventThread = Window;
}

FPlatformRect FAndroidWindow::GetScreenRect(bool bUseEventThreadWindow)
{
	int32 OverrideResX, OverrideResY;
	// allow a subplatform to dictate resolution - we can't easily subclass FAndroidWindow the way its used
	if (FPlatformMisc::GetOverrideResolution(OverrideResX, OverrideResY))
	{
		FPlatformRect Rect;
		Rect.Left = Rect.Top = 0;
		Rect.Right = OverrideResX;
		Rect.Bottom = OverrideResY;

		return Rect;
	}

	// too much of the following code needs JNI things, just assume override
#if !USE_ANDROID_JNI

	UE_LOG(LogAndroid, Fatal, TEXT("FAndroidWindow::CalculateSurfaceSize currently expedcts non-JNI platforms to override resolution"));
	return FPlatformRect();
#else

	static const bool bIsOculusMobileApp = AndroidThunkCpp_IsOculusMobileApplication();

	// CSF is a multiplier to 1280x720
	static IConsoleVariable* CVarScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	// If the app is for Oculus Mobile then always use 0 as ScaleFactor (to match window size).
	float RequestedContentScaleFactor = bIsOculusMobileApp ? 0.0f : CVarScale->GetFloat();

	FString CmdLineCSF;
	if (FParse::Value(FCommandLine::Get(), TEXT("mcsf="), CmdLineCSF, false))
	{
		RequestedContentScaleFactor = FCString::Atof(*CmdLineCSF);
	}

	static IConsoleVariable* CVarResX = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DesiredResX"));
	static IConsoleVariable* CVarResY = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DesiredResY"));
	int32 RequestedResX = bIsOculusMobileApp ? 0 : CVarResX->GetInt();
	int32 RequestedResY = bIsOculusMobileApp ? 0 : CVarResY->GetInt();

	FString CmdLineMDRes;
	if (FParse::Value(FCommandLine::Get(), TEXT("mobileresx="), CmdLineMDRes, false))
	{
		RequestedResX = FCString::Atoi(*CmdLineMDRes);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("mobileresy="), CmdLineMDRes, false))
	{
		RequestedResY = FCString::Atoi(*CmdLineMDRes);
	}

	// since orientation won't change on Android, use cached results if still valid. Different cache is maintained for event_thread flavor.
	ANativeWindow* Window = bUseEventThreadWindow ? (ANativeWindow*)FAndroidWindow::GetHardwareWindow_EventThread() : nullptr;
	bool bComputeRect = !IsCachedRectValid(bUseEventThreadWindow, RequestedContentScaleFactor, RequestedResX, RequestedResY, Window);
	if (bComputeRect)
	{
		// currently hardcoding resolution

		// get the aspect ratio of the physical screen
		int32 ScreenWidth, ScreenHeight;
		CalculateSurfaceSize(ScreenWidth, ScreenHeight, bUseEventThreadWindow);

		static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
		const bool bMobileHDR = (MobileHDRCvar && MobileHDRCvar->GetValueOnAnyThread() == 1);
		UE_LOG(LogAndroid, Log, TEXT("Mobile HDR: %s"), bMobileHDR ? TEXT("YES") : TEXT("no"));

		if (!bIsOculusMobileApp)
		{
			AndroidWindowUtils::ApplyContentScaleFactor(ScreenWidth, ScreenHeight);
		}

		// save for future calls
		CacheRect(bUseEventThreadWindow, ScreenWidth, ScreenHeight, RequestedContentScaleFactor, RequestedResX, RequestedResY, Window);
	}

	const FCachedWindowRect& CachedRect = bUseEventThreadWindow ? CachedWindowRect_EventThread : CachedWindowRect;

	// create rect and return
	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;
	ScreenRect.Right = CachedRect.WindowWidth;
	ScreenRect.Bottom = CachedRect.WindowHeight;

	return ScreenRect;
#endif
}

void FAndroidWindow::CalculateSurfaceSize(int32_t& SurfaceWidth, int32_t& SurfaceHeight, bool bUseEventThreadWindow)
{
	// allow a subplatform to dictate resolution - we can't easily subclass FAndroidWindow the way its used
	if (FPlatformMisc::GetOverrideResolution(SurfaceWidth, SurfaceHeight))
	{
		return;
	}

	// too much of the following code needs JNI things, just assume override
#if !USE_ANDROID_JNI

	UE_LOG(LogAndroid, Fatal, TEXT("FAndroidWindow::CalculateSurfaceSize currently expects non-JNI platforms to override resolution"));

#else

	if (bUseEventThreadWindow)
	{
		check(IsInAndroidEventThread());
		ANativeWindow* WindowEventThread = (ANativeWindow*)GetHardwareWindow_EventThread();
		check(WindowEventThread);

		SurfaceWidth = (GSurfaceViewWidth > 0) ? GSurfaceViewWidth : ANativeWindow_getWidth(WindowEventThread);
		SurfaceHeight = (GSurfaceViewHeight > 0) ? GSurfaceViewHeight : ANativeWindow_getHeight(WindowEventThread);
	}
	else
	{
		FAndroidWindow::WaitForWindowDimensions();

		SurfaceWidth = (GSurfaceViewWidth > 0) ? GSurfaceViewWidth : CachedNativeWindowWidth;
		SurfaceHeight = (GSurfaceViewHeight > 0) ? GSurfaceViewHeight : CachedNativeWindowHeight;
	}

	// some phones gave it the other way (so, if swap if the app is landscape, but width < height)
	if ((GAndroidIsPortrait && SurfaceWidth > SurfaceHeight) ||
		(!GAndroidIsPortrait && SurfaceWidth < SurfaceHeight))
	{
		Swap(SurfaceWidth, SurfaceHeight);
	}

	// ensure the size is divisible by a specified amount
	// do not convert to a surface size that is larger than native resolution
	// Mobile VR doesn't need buffer quantization as Unreal never renders directly to the buffer in VR mode. 
	static const bool bIsMobileVRApp = AndroidThunkCpp_IsOculusMobileApplication();
	const int DividableBy = bIsMobileVRApp ? 1 : 8;
	SurfaceWidth = (SurfaceWidth / DividableBy) * DividableBy;
	SurfaceHeight = (SurfaceHeight / DividableBy) * DividableBy;
#endif
}

bool FAndroidWindow::OnWindowOrientationChanged(bool bIsPortrait)
{
	if (GAndroidIsPortrait != bIsPortrait)
	{
		UE_LOG(LogAndroid, Log, TEXT("Window orientation changed: %s"), bIsPortrait ? TEXT("Portrait") : TEXT("Landscape"));
		GAndroidIsPortrait = bIsPortrait;
		return true;
	}
	return false;
}
