// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#if USE_ANDROID_LAUNCH
#include "Misc/App.h"
#include "Misc/OutputDeviceError.h"
#include "LaunchEngineLoop.h"
#include <string.h>
#include <pthread.h>
#include "Android/AndroidJNI.h"
#include "Android/AndroidEventManager.h"
#include "Android/AndroidInputInterface.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <cpu-features.h>
#include <android_native_app_glue.h>
#include <cstdio>
#include <sys/resource.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include "Android/AndroidWindow.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformStackWalk.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IHeadMountedDisplayModule.h"
#include "ISessionServicesModule.h"
#include "ISessionService.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/PlatformInput.h"
#include "HAL/ThreadHeartBeat.h"
#include "Modules/ModuleManager.h"
#include "IMessagingModule.h"
#include "Android/AndroidStats.h"
#include "MoviePlayer.h"
#include "PreLoadScreenManager.h"
#include "Misc/EmbeddedCommunication.h"
#include "Async/Async.h"
#include <jni.h>
#include <android/sensor.h>

// Function pointer for retrieving joystick events
// Function has been part of the OS since Honeycomb, but only appeared in the
// NDK in r19. Querying via dlsym allows its use without tying to the newest
// NDK.
typedef float(*GetAxesType)(const AInputEvent*, int32_t axis, size_t pointer_index);
static GetAxesType GetAxes = NULL;

// Define missing events for earlier NDKs
#if PLATFORM_ANDROID_NDK_VERSION < 140200
#define AMOTION_EVENT_AXIS_RELATIVE_X 27
#define AMOTION_EVENT_AXIS_RELATIVE_Y 28
#endif

#ifndef ANDROID_ALLOWCUSTOMTOUCHEVENT
#define ANDROID_ALLOWCUSTOMTOUCHEVENT	0
#endif

// List of default axes to query for each controller
// Ideal solution is to call out to Java and enumerate the list of axes.
static const int32_t AxisList[] =
{
	AMOTION_EVENT_AXIS_X,
	AMOTION_EVENT_AXIS_Y,
	AMOTION_EVENT_AXIS_Z,
	AMOTION_EVENT_AXIS_RX,
	AMOTION_EVENT_AXIS_RY,
	AMOTION_EVENT_AXIS_RZ,

	//These are DPAD analogs
	AMOTION_EVENT_AXIS_HAT_X,
	AMOTION_EVENT_AXIS_HAT_Y,
};

// map of all supported keycodes
static TSet<uint32> MappedKeyCodes;

// map of always allowed keycodes
static TSet<uint32> AlwaysAllowedKeyCodes;

// List of always allowed keycodes
static const uint32 AlwaysAllowedKeyCodesList[] =
{
	AKEYCODE_MENU,
	AKEYCODE_BACK,
	AKEYCODE_VOLUME_UP,
	AKEYCODE_VOLUME_DOWN
};

// List of desired gamepad keycodes
static const uint32 ValidGamepadKeyCodesList[] =
{
	AKEYCODE_BUTTON_A,
	AKEYCODE_DPAD_CENTER,
	AKEYCODE_BUTTON_B,
	AKEYCODE_BUTTON_C,
	AKEYCODE_BUTTON_X,
	AKEYCODE_BUTTON_Y,
	AKEYCODE_BUTTON_Z,
	AKEYCODE_BUTTON_L1,
	AKEYCODE_BUTTON_R1,
	AKEYCODE_BUTTON_START,
	AKEYCODE_MENU,
	AKEYCODE_BUTTON_SELECT,
	AKEYCODE_BACK,
	AKEYCODE_BUTTON_THUMBL,
	AKEYCODE_BUTTON_THUMBR,
	AKEYCODE_BUTTON_L2,
	AKEYCODE_BUTTON_R2,
	AKEYCODE_DPAD_UP,
	AKEYCODE_DPAD_DOWN,
	AKEYCODE_DPAD_LEFT,
	AKEYCODE_DPAD_RIGHT,
	3002  // touchpad
};

// map of gamepad keycodes that should be passed forward
static TSet<uint32> ValidGamepadKeyCodes;

// -nostdlib means no crtbegin_so.o, so we have to provide our own __dso_handle and atexit()
// this is not needed now we are using stdlib (later NDK has more functionality we should keep)
#if 0
extern "C"
{
	int atexit(void (*func)(void)) { return 0; }

	extern void *__dso_handle __attribute__((__visibility__ ("hidden")));
	void *__dso_handle;
}
#endif

int32 GAndroidEnableNativeResizeEvent = 1;
static FAutoConsoleVariableRef CVarEnableResizeNativeEvent(
	TEXT("Android.EnableNativeResizeEvent"),
	GAndroidEnableNativeResizeEvent,
	TEXT("Whether native resize event is enabled on Android.\n")
	TEXT(" 0: disabled\n")
	TEXT(" 1: enabled (default)"),
	ECVF_ReadOnly);

int32 GAndroidEnableMouse = 0;
static FAutoConsoleVariableRef CVarEnableMouse(
	TEXT("Android.EnableMouse"),
	GAndroidEnableMouse,
	TEXT("Whether mouse support is enabled on Android.\n")
	TEXT(" 0: disabled (default)\n")
	TEXT(" 1: enabled"),
	ECVF_ReadOnly);

int32 GAndroidEnableHardwareKeyboard = 0;
static FAutoConsoleVariableRef CVarEnableHWKeyboard(
	TEXT("Android.EnableHardwareKeyboard"),
	GAndroidEnableHardwareKeyboard,
	TEXT("Whether hardware keyboard support is enabled on Android.\n")
	TEXT(" 0: disabled (default)\n")
	TEXT(" 1: enabled"),
	ECVF_ReadOnly);

extern void AndroidThunkCpp_InitHMDs();
extern void AndroidThunkCpp_ShowConsoleWindow();
extern bool AndroidThunkCpp_VirtualInputIgnoreClick(int, int);
extern bool AndroidThunkCpp_IsVirtuaKeyboardShown();
extern bool AndroidThunkCpp_IsWebViewShown();
extern void AndroidThunkCpp_RestartApplication(const FString& IntentString);

// Base path for file accesses
extern FString GFilePathBase;

/** The global EngineLoop instance */
FEngineLoop	GEngineLoop;

static bool bDidCompleteEngineInit = false;

bool GShowConsoleWindowNextTick = false;

static void AndroidProcessEvents(struct android_app* state);

//Event thread stuff
static void* AndroidEventThreadWorker(void* param);

// How often to process (read & dispatch) events, in seconds.
static const float EventRefreshRate = 1.0f / 20.0f;

// Name of the UE commandline append setprop
static constexpr char UECommandLineSetprop[] = "debug.ue.commandline";

//Android event callback functions
static int32_t HandleInputCB(struct android_app* app, AInputEvent* event); //Touch and key input events
static void OnAppCommandCB(struct android_app* app, int32_t cmd); //Lifetime events

static bool TryIgnoreClick(AInputEvent* event, size_t actionPointer);

bool GAllowJavaBackButtonEvent = false;
bool GHasInterruptionRequest = false;
bool GIsInterrupted = false;

// Set 'SustainedPerformanceMode' via cvar sink.
static TAutoConsoleVariable<int32> CVarEnableSustainedPerformanceMode(
	TEXT("Android.EnableSustainedPerformanceMode"),
	0,
	TEXT("Enable sustained performance mode, if supported. (API >= 24 req. not supported by all devices.)\n")
	TEXT("  0: Disabled (default)\n")
	TEXT("  1: Enabled"),
	ECVF_Default);

extern void AndroidThunkCpp_SetSustainedPerformanceMode(bool);
static void SetSustainedPerformanceMode()
{
	static bool bSustainedPerformanceMode = false;
	bool bIncomingSustainedPerformanceMode = CVarEnableSustainedPerformanceMode.GetValueOnAnyThread() != 0;
	if(bSustainedPerformanceMode != bIncomingSustainedPerformanceMode)
	{
		bSustainedPerformanceMode = bIncomingSustainedPerformanceMode;
		UE_LOG(LogAndroid, Log, TEXT("Setting sustained performance mode: %d"), (int32)bSustainedPerformanceMode);
		AndroidThunkCpp_SetSustainedPerformanceMode(bSustainedPerformanceMode);
	}
}
FAutoConsoleVariableSink CVarEnableSustainedPerformanceModeSink(FConsoleCommandDelegate::CreateStatic(&SetSustainedPerformanceMode));

// Event for coordinating pausing of the main and event handling threads to prevent background spinning
static FEvent* EventHandlerEvent = NULL;

// Wait for Java onCreate to complete before resume main init
static volatile bool GResumeMainInit = false;
volatile bool GEventHandlerInitialized = false;

// The event thread locks this whenever the window unavailable during early init, pause and resume.
FCriticalSection GAndroidWindowLock;
bool bReadyToProcessEvents = false;
void FPlatformMisc::UnlockAndroidWindow()
{
	check(IsInGameThread());
	check(FTaskGraphInterface::IsRunning());
	UE_LOG(LogAndroid, Log, TEXT("Unlocking android HW window during preinit."));
	bReadyToProcessEvents = true;
	GAndroidWindowLock.Unlock();
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeResumeMainInit(JNIEnv* jenv, jobject thiz)
{
	GResumeMainInit = true;

	// now wait for event handler to be set up before returning
	while (!GEventHandlerInitialized)
	{
		FPlatformProcess::Sleep(0.01f);
		FPlatformMisc::MemoryBarrier();
	}
}

static volatile bool GHMDsInitialized = false;
static TArray<IHeadMountedDisplayModule*> GHMDImplementations;
void InitHMDs()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("nohmd")) || FParse::Param(FCommandLine::Get(), TEXT("emulatestereo")))
	{
		return;
	}

	// Get a list of plugins that implement this feature
	GHMDImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IHeadMountedDisplayModule>(IHeadMountedDisplayModule::GetModularFeatureName());

	AndroidThunkCpp_InitHMDs();

	while (!GHMDsInitialized)
	{
		FPlatformProcess::Sleep(0.01f);
		FPlatformMisc::MemoryBarrier();
	}
}

extern AAssetManager * AndroidThunkCpp_GetAssetManager();

static void InitCommandLine()
{
	static const uint32 CMD_LINE_MAX = 16384u;

	// initialize the command line to an empty string
	FCommandLine::Set(TEXT(""));

	AAssetManager* AssetMgr = AndroidThunkCpp_GetAssetManager();
	AAsset* asset = AAssetManager_open(AssetMgr, TCHAR_TO_UTF8(TEXT("UECommandLine.txt")), AASSET_MODE_BUFFER);
	if (nullptr != asset)
	{
		const void* FileContents = AAsset_getBuffer(asset);
		int32 FileLength = AAsset_getLength(asset);

		char CommandLine[CMD_LINE_MAX];
		FileLength = (FileLength < CMD_LINE_MAX - 1) ? FileLength : CMD_LINE_MAX - 1;
		memcpy(CommandLine, FileContents, FileLength);
		CommandLine[FileLength] = '\0';

		AAsset_close(asset);

		// chop off trailing spaces
		while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
		{
			CommandLine[strlen(CommandLine) - 1] = 0;
		}

		FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APK Commandline: %s"), FCommandLine::Get());
	}

	// read in the command line text file from the sdcard if it exists
	FString CommandLineFilePath = GFilePathBase + FString("/UnrealGame/") + (!FApp::IsProjectNameEmpty() ? FApp::GetProjectName() : FPlatformProcess::ExecutableName()) + FString("/UECommandLine.txt");
	FILE* CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	if(CommandLineFile == NULL)
	{
		// if that failed, try the lowercase version
		CommandLineFilePath = CommandLineFilePath.Replace(TEXT("UECommandLine.txt"), TEXT("uecommandline.txt"));
		CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
	}

	if(CommandLineFile)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Using override commandline file: %s"), *CommandLineFilePath);

		char CommandLine[CMD_LINE_MAX];
		fgets(CommandLine, UE_ARRAY_COUNT(CommandLine) - 1, CommandLineFile);

		fclose(CommandLineFile);

		// chop off trailing spaces
		while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
		{
			CommandLine[strlen(CommandLine) - 1] = 0;
		}

		// initialize the command line to an empty string
		FCommandLine::Set(TEXT(""));

		FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Override Commandline: %s"), FCommandLine::Get());
	}

#if !UE_BUILD_SHIPPING
	if (FString* ConfigRulesCmdLineAppend = FAndroidMisc::GetConfigRulesVariable(TEXT("cmdline")))
	{
		FCommandLine::Append(**ConfigRulesCmdLineAppend);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ConfigRules appended: %s"), **ConfigRulesCmdLineAppend);
	}

	char CommandLineSetpropAppend[CMD_LINE_MAX];
	if (__system_property_get(UECommandLineSetprop, CommandLineSetpropAppend) > 0)
	{
		FCommandLine::Append(UTF8_TO_TCHAR(" "));
		FCommandLine::Append(UTF8_TO_TCHAR(CommandLineSetpropAppend));
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("UE setprop appended: %s"), UTF8_TO_TCHAR(CommandLineSetpropAppend));
	}
#endif
}

extern void AndroidThunkCpp_DismissSplashScreen();

//Called in main thread for native window resizing
static void OnNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
{
	static int8_t cmd = APP_CMD_WINDOW_RESIZED;
	struct android_app* app = (struct android_app *)activity->instance;
	write(app->msgwrite, &cmd, sizeof(cmd));
}

static void ApplyAndroidCompatConfigRules()
{
	TArray<FString> AndroidCompatCVars;
	if (GConfig->GetArray(TEXT("AndroidCompatCVars"), TEXT("CVars"), AndroidCompatCVars, GEngineIni))
	{
		TSet<FString> AllowedCompatCVars(AndroidCompatCVars);
		for (const TTuple<FString, FString>& Pair : FAndroidMisc::GetConfigRulesTMap())
		{
			const FString& Key = Pair.Key;
			const FString& Value = Pair.Value;
			static const TCHAR AndroidCompat[] = TEXT("AndroidCompat.");
			if (Key.StartsWith(AndroidCompat))
			{
				FString CVarName = Key.Mid(UE_ARRAY_COUNT(AndroidCompat)-1);
				if (AllowedCompatCVars.Contains(CVarName))
				{
					auto* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
					if (CVar)
					{
						// set with current priority means that DPs etc can still override anything set here.
						// e.g. -dpcvars= is expected to work.
						CVar->SetWithCurrentPriority(*Value); 
						UE_LOG(LogAndroid, Log, TEXT("Compat Setting %s = %s"), *CVarName, *Value);
					}
				}
			}
		}
	}
}

//Main function called from the android entry point
int32 AndroidMain(struct android_app* state)
{
	BootTimingPoint("AndroidMain");

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Entered AndroidMain()\n"));

	// Force the first call to GetJavaEnv() to happen on the game thread, allowing subsequent calls to occur on any thread
	FAndroidApplication::GetJavaEnv();

	// Set window format to 8888
	ANativeActivity_setWindowFormat(state->activity, WINDOW_FORMAT_RGBA_8888);

	// adjust the file descriptor limits to allow as many open files as possible
	rlimit cur_fd_limit;
	{
		int result = getrlimit(RLIMIT_NOFILE, & cur_fd_limit);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("(%d) Current fd limits: soft = %lld, hard = %lld"), result, cur_fd_limit.rlim_cur, cur_fd_limit.rlim_max);
	}
	{
		rlimit new_limit = cur_fd_limit;
		new_limit.rlim_cur = cur_fd_limit.rlim_max;
		new_limit.rlim_max = cur_fd_limit.rlim_max;
		int result = setrlimit(RLIMIT_NOFILE, &new_limit);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("(%d) Setting fd limits: soft = %lld, hard = %lld"), result, new_limit.rlim_cur, new_limit.rlim_max);
	}
	{
		int result = getrlimit(RLIMIT_NOFILE, & cur_fd_limit);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("(%d) Current fd limits: soft = %lld, hard = %lld"), result, cur_fd_limit.rlim_cur, cur_fd_limit.rlim_max);
	}

	// setup joystick support
	// r19 is the first NDK to include AMotionEvent_getAxisValue in the headers
	// However, it has existed in the so since Honeycomb, query for the symbol
	// to determine whether to try controller support
	{
		void* Lib = dlopen("libandroid.so",0);
		if (Lib != NULL)
		{
			GetAxes = (GetAxesType)dlsym(Lib, "AMotionEvent_getAxisValue");
		}

		if (GetAxes != NULL)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Controller interface supported\n"));
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Controller interface UNsupported\n"));
		}
	}

	// setup key filtering
	static const uint32 MAX_KEY_MAPPINGS(256);
	uint32 KeyCodes[MAX_KEY_MAPPINGS];
	uint32 NumKeyCodes = FPlatformInput::GetKeyMap(KeyCodes, nullptr, MAX_KEY_MAPPINGS);

	for (int i = 0; i < NumKeyCodes; ++i)
	{
		MappedKeyCodes.Add(KeyCodes[i]);
	}

	const int AlwaysAllowedKeyCodesCount = sizeof(AlwaysAllowedKeyCodesList) / sizeof(uint32);
	for (int i = 0; i < AlwaysAllowedKeyCodesCount; ++i)
	{
		AlwaysAllowedKeyCodes.Add(AlwaysAllowedKeyCodesList[i]);
	}

	const int ValidGamepadKeyCodeCount = sizeof(ValidGamepadKeyCodesList)/sizeof(uint32);
	for (int i = 0; i < ValidGamepadKeyCodeCount; ++i)
	{
		ValidGamepadKeyCodes.Add(ValidGamepadKeyCodesList[i]);
	}

	FAndroidPlatformStackWalk::InitStackWalking();

	// wait for java activity onCreate to finish
	{
		SCOPED_BOOT_TIMING("Wait for GResumeMainInit");
		while (!GResumeMainInit)
		{
			FPlatformProcess::Sleep(0.01f);
			FPlatformMisc::MemoryBarrier();
		}
	}

	// read the command line file
	InitCommandLine();
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Final commandline: %s\n"), FCommandLine::Get());

#if !(UE_BUILD_SHIPPING)
	// If "-waitforattach" or "-WaitForDebugger" was specified, halt startup and wait for a debugger to attach before continuing
	if (FParse::Param(FCommandLine::Get(), TEXT("waitforattach")) || FParse::Param(FCommandLine::Get(), TEXT("WaitForDebugger")))
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Waiting for debugger to attach...\n"));
		while (!FPlatformMisc::IsDebuggerPresent());
		UE_DEBUG_BREAK();
	}
#endif

	EventHandlerEvent = FPlatformProcess::GetSynchEventFromPool(false);
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Created sync event\n"));
	FAppEventManager::GetInstance()->SetEventHandlerEvent(EventHandlerEvent);

	// ready for onCreate to complete
	GEventHandlerInitialized = true;

	// Initialize file system access (i.e. mount OBBs, etc.).
	// We need to do this really early for Android so that files in the
	// OBBs and APK are found.
	// Have to use a special initialize if using the PersistentStorageManager
	IPlatformFile::GetPlatformPhysical().Initialize(nullptr, FCommandLine::Get());

	{
		SCOPED_BOOT_TIMING("Wait for GAndroidWindowLock.Lock()");
		// wait for a valid window
		// Lock GAndroidWindowLock to ensure no window destroy shenanigans occur between early phase of preinit and UnlockAndroidWindow
		// Note: this is unlocked after Android's PlatformCreateDynamicRHI when the RHI is then able to process window changes.
		// We don't wait for all of preinit to complete as PreLoadScreens will need to process events during preinit.

		UE_LOG(LogAndroid, Log, TEXT("PreInit android HW window lock."));
		GAndroidWindowLock.Lock();
	}

	FDelegateHandle ConfigReadyHandle = FCoreDelegates::ConfigReadyForUse.AddStatic(&ApplyAndroidCompatConfigRules);

	// initialize the engine
	int32 PreInitResult = GEngineLoop.PreInit(0, NULL, FCommandLine::Get());

	FCoreDelegates::ConfigReadyForUse.Remove(ConfigReadyHandle);

	if (PreInitResult != 0)
	{
		checkf(false, TEXT("Engine Preinit Failed"));
		return PreInitResult;
	}

	// register callback for native window resize
	if (GAndroidEnableNativeResizeEvent)
	{
		state->activity->callbacks->onNativeWindowResized = OnNativeWindowResized;
	}

	// initialize HMDs
	{
		SCOPED_BOOT_TIMING("InitHMDs");
		InitHMDs();
	}

	UE_LOG(LogAndroid, Display, TEXT("Passed PreInit()"));

	GLog->SetCurrentThreadAsPrimaryThread();

	FAppEventManager::GetInstance()->SetEmptyQueueHandlerEvent(FPlatformProcess::GetSynchEventFromPool(false));

	GEngineLoop.Init();

	bDidCompleteEngineInit = true;

	UE_LOG(LogAndroid, Log, TEXT("Passed GEngineLoop.Init()"));

	AndroidThunkCpp_DismissSplashScreen();

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("Messaging")))
	{
		// initialize messaging subsystem
		FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");
		TSharedPtr<ISessionService> SessionService = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionService();
		SessionService->Start();

		// Initialize functional testing
		FModuleManager::Get().LoadModule("FunctionalTesting");
	}
#endif

	FAndroidStats::Init(FParse::Param(FCommandLine::Get(), TEXT("hwcpipe")));

	BootTimingPoint("Tick loop starting");
	DumpBootTiming();
	// tick until done
	while (!IsEngineExitRequested())
	{
		FAndroidStats::UpdateAndroidStats();

		FAppEventManager::GetInstance()->Tick();
		if(!FAppEventManager::GetInstance()->IsGamePaused())
		{
			GEngineLoop.Tick();
		}
		else
		{
			// use less CPU when paused
			FPlatformProcess::Sleep(0.10f);
		}

#if !UE_BUILD_SHIPPING
		// show console window on next game tick
		if (GShowConsoleWindowNextTick)
		{
			GShowConsoleWindowNextTick = false;
			AndroidThunkCpp_ShowConsoleWindow();
		}
#endif
	}
	
	FAppEventManager::GetInstance()->TriggerEmptyQueue();

	UE_LOG(LogAndroid, Log, TEXT("Exiting"));

	// exit out!
	GEngineLoop.Exit();

	UE_LOG(LogAndroid, Log, TEXT("Exiting is over"));

	FPlatformMisc::RequestExit(1);
	return 0;
}

struct AChoreographer;
struct FChoreographer
{
	typedef void(*AChoreographer_frameCallback)(long frameTimeNanos, void* data);
	typedef AChoreographer* (*func_AChoreographer_getInstance)();
	typedef void(*func_AChoreographer_postFrameCallback)(
		AChoreographer* choreographer, AChoreographer_frameCallback callback,
		void* data);
	typedef void(*func_AChoreographer_postFrameCallbackDelayed)(
		AChoreographer* choreographer, AChoreographer_frameCallback callback,
		void* data, long delayMillis);

	func_AChoreographer_getInstance AChoreographer_getInstance_ = nullptr;
	func_AChoreographer_postFrameCallback AChoreographer_postFrameCallback_ = nullptr;
	func_AChoreographer_postFrameCallbackDelayed AChoreographer_postFrameCallbackDelayed_ = nullptr;

	FCriticalSection ChoreographerSetupLock;

	TFunction<int64(int64)> Callback;

	void SetupChoreographer()
	{
		FScopeLock Lock(&ChoreographerSetupLock);
		//check(!AChoreographer_getInstance_);
		if (!AChoreographer_getInstance_)
		{
			void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
			if (lib != nullptr)
			{
				// Retrieve function pointers from shared object.
				AChoreographer_getInstance_ =
					reinterpret_cast<func_AChoreographer_getInstance>(
						dlsym(lib, "AChoreographer_getInstance"));
				AChoreographer_postFrameCallback_ =
					reinterpret_cast<func_AChoreographer_postFrameCallback>(
						dlsym(lib, "AChoreographer_postFrameCallback"));
				AChoreographer_postFrameCallbackDelayed_ =
					reinterpret_cast<func_AChoreographer_postFrameCallbackDelayed>(
						dlsym(lib, "AChoreographer_postFrameCallbackDelayed"));
			}

			if (!AChoreographer_getInstance_ || !AChoreographer_postFrameCallback_ || !AChoreographer_postFrameCallbackDelayed_)
			{
				UE_LOG(LogAndroid, Warning, TEXT("Failed to set up Choreographer"));
				AChoreographer_getInstance_ = nullptr;
				AChoreographer_postFrameCallback_ = nullptr;
				AChoreographer_postFrameCallbackDelayed_ = nullptr;
			}
			else
			{
				SetCallback(0);
				UE_LOG(LogAndroid, Display, TEXT("Choreographer set up."));
			}
		}
	}
	void SetupCallback(TFunction<int64(int64)> InCallback)
	{
		check(IsAvailable());
		FScopeLock Lock(&ChoreographerSetupLock);
		Callback = InCallback;
	}

	void SetCallback(int64 Delay);
	void DoCallback(long frameTimeNanos)
	{
		//static long LastFrameTimeNanos = 0;
		//UE_LOG(LogAndroid, Warning, TEXT("Choreographer %lld   delta %lld"), frameTimeNanos, frameTimeNanos - LastFrameTimeNanos);
		//LastFrameTimeNanos = frameTimeNanos;
		int64 NextDelay = -1;
		{
			FScopeLock Lock(&ChoreographerSetupLock);
			if (Callback)
			{
				NextDelay = Callback(frameTimeNanos);
			}
		}
		SetCallback((NextDelay >= 0) ? NextDelay : 0);
	}
	bool IsAvailable()
	{
		return !!AChoreographer_getInstance_;
	}
};
FChoreographer TheChoreographer;

bool ChoreographerIsAvailable()
{
	return TheChoreographer.IsAvailable();
}

void StartChoreographer(TFunction<int64(int64)> Callback)
{
	check(ChoreographerIsAvailable());
	TheChoreographer.SetupCallback(Callback);
}


static void choreographer_callback(long frameTimeNanos, void* data)
{
	TheChoreographer.DoCallback(frameTimeNanos);
}

void FChoreographer::SetCallback(int64 Delay)
{
	check(IsAvailable());
	check(Delay >= 0);
	AChoreographer* choreographer = AChoreographer_getInstance_();
	UE_CLOG(!choreographer, LogAndroid, Fatal, TEXT("Choreographer was null (wrong thread?)."));
	AChoreographer_postFrameCallbackDelayed_(choreographer, choreographer_callback, nullptr, Delay / 1000000);
}

static uint32 EventThreadID = 0;

bool IsInAndroidEventThread()
{
	check(EventThreadID != 0);
	return EventThreadID == FPlatformTLS::GetCurrentThreadId();
}

static void* AndroidEventThreadWorker( void* param )
{
	pthread_setname_np(pthread_self(), "EventWorker");
	EventThreadID = FPlatformTLS::GetCurrentThreadId();
	FAndroidMisc::RegisterThreadName("EventWorker", EventThreadID);

	struct android_app* state = (struct android_app*)param;

	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Entering event processing thread engine entry point"));

	ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
	ALooper_addFd(looper, state->msgread, LOOPER_ID_MAIN, ALOOPER_EVENT_INPUT, NULL,
		&state->cmdPollSource);
	state->looper = looper;

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Prepared looper for event thread"));

	//Assign the callbacks
	state->onAppCmd = OnAppCommandCB;
	state->onInputEvent = HandleInputCB;

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Passed callback initialization"));
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Passed sensor initialization"));

	TheChoreographer.SetupChoreographer();

	// window is initially invalid/locked.
	UE_LOG(LogAndroid, Log, TEXT("event thread, Initial HW window lock."));
	GAndroidWindowLock.Lock();

	//continue to process events until the engine is shutting down
	while (!IsEngineExitRequested())
	{
//		FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidEventThreadWorker"));

		AndroidProcessEvents(state);

		sleep(EventRefreshRate);		// this is really 0 since it takes int seconds.
	}

	GAndroidWindowLock.Unlock();

	UE_LOG(LogAndroid, Log, TEXT("Exiting"));

	return NULL;
}

//Called from the separate event processing thread
static void AndroidProcessEvents(struct android_app* state)
{
	int ident;
	int fdesc;
	int events;
	struct android_poll_source* source;

	while((ident = ALooper_pollAll(-1, &fdesc, &events, (void**)&source)) >= 0)
	{
		// process this event
		if (source)
		{
			source->process(state, source);
		}
	}
}

pthread_t G_AndroidEventThread;

struct android_app* GNativeAndroidApp = NULL;

void android_main(struct android_app* state)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	GGameThreadId = FPlatformTLS::GetCurrentThreadId();

	BootTimingPoint("android_main");
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Entering native app glue main function"));
	
	GNativeAndroidApp = state;
	check(GNativeAndroidApp);

	pthread_attr_t otherAttr; 
	pthread_attr_init(&otherAttr);
	pthread_attr_setdetachstate(&otherAttr, PTHREAD_CREATE_DETACHED);
	pthread_create(&G_AndroidEventThread, &otherAttr, AndroidEventThreadWorker, state);

	FPlatformMisc::LowLevelOutputDebugString(TEXT("Created event thread"));

	// Make sure glue isn't stripped. (not needed in ndk-15)
#if PLATFORM_ANDROID_NDK_VERSION < 150000
	app_dummy();
#endif

	//@todo android: replace with native activity, main loop off of UI thread, etc.
	AndroidMain(state);
}

extern bool GAndroidGPUInfoReady;

static bool TryIgnoreClick(AInputEvent* event, size_t actionPointer)
{
	int pointerId = AMotionEvent_getPointerId(event, actionPointer);
	int32 x = AMotionEvent_getX(event, actionPointer);
	int32 y = AMotionEvent_getY(event, actionPointer);

	//ignore key down events click was within bounds
	if (AndroidThunkCpp_VirtualInputIgnoreClick(x, y))
	{
		return true;
	}
	return false;
}

//Called from the event process thread
static int32_t HandleInputCB(struct android_app* app, AInputEvent* event)
{
//	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("INPUT - type: %x, action: %x, source: %x, keycode: %x, buttons: %x"), AInputEvent_getType(event), 
//		AMotionEvent_getAction(event), AInputEvent_getSource(event), AKeyEvent_getKeyCode(event), AMotionEvent_getButtonState(event));
	check(IsInAndroidEventThread());

	int32 EventType = AInputEvent_getType(event);
	int32 EventSource = AInputEvent_getSource(event);

	if ((EventSource & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE)
	{
		static int32 previousButtonState = 0;

		const int32 device = AInputEvent_getDeviceId(event);
		const int32 action = AMotionEvent_getAction(event);
		const int32 actionType = action & AMOTION_EVENT_ACTION_MASK;
		int32 buttonState = AMotionEvent_getButtonState(event);

		if (!GAndroidEnableMouse)
		{
			if (actionType == AMOTION_EVENT_ACTION_DOWN || actionType == AMOTION_EVENT_ACTION_UP)
			{
				const bool bDown = (actionType == AMOTION_EVENT_ACTION_DOWN);
				if (!bDown)
				{
					buttonState = previousButtonState;
				}
				if (buttonState & AMOTION_EVENT_BUTTON_PRIMARY)
				{
					const int32 ReplacementKeyEvent = FAndroidInputInterface::GetAlternateKeyEventForMouse(device, 0);
					if (ReplacementKeyEvent != 0)
					{
						FAndroidInputInterface::JoystickButtonEvent(device, ReplacementKeyEvent, bDown);
					}
				}
				if (buttonState & AMOTION_EVENT_BUTTON_SECONDARY)
				{
					const int32 ReplacementKeyEvent = FAndroidInputInterface::GetAlternateKeyEventForMouse(device, 1);
					if (ReplacementKeyEvent != 0)
					{
						FAndroidInputInterface::JoystickButtonEvent(device, ReplacementKeyEvent, bDown);
					}
				}
				if (buttonState & AMOTION_EVENT_BUTTON_TERTIARY)
				{
					const int32 ReplacementKeyEvent = FAndroidInputInterface::GetAlternateKeyEventForMouse(device, 2);
					if (ReplacementKeyEvent != 0)
					{
						FAndroidInputInterface::JoystickButtonEvent(device, ReplacementKeyEvent, bDown);
					}
				}
				previousButtonState = buttonState;
			}
			return 1;
		}

//		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("-- EVENT: %d, device: %d, action: %x, actionType: %x, buttonState: %x"), EventType, device, action, actionType, buttonState);

		if (actionType == AMOTION_EVENT_ACTION_DOWN || actionType == AMOTION_EVENT_ACTION_UP)
		{
			const bool bDown = (actionType == AMOTION_EVENT_ACTION_DOWN);
			if (!bDown)
			{
				buttonState = previousButtonState;
			}
			if (buttonState & AMOTION_EVENT_BUTTON_PRIMARY)
			{
//				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mouse button 0: %d"), bDown ? 1 : 0);
				FAndroidInputInterface::MouseButtonEvent(device, 0, bDown);
			}
			if (buttonState & AMOTION_EVENT_BUTTON_SECONDARY)
			{
//				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mouse button 1: %d"), bDown ? 1 : 0);
				FAndroidInputInterface::MouseButtonEvent(device, 1, bDown);
			}
			if (buttonState & AMOTION_EVENT_BUTTON_TERTIARY)
			{
//				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mouse button 2: %d"), bDown ? 1 : 0);
				FAndroidInputInterface::MouseButtonEvent(device, 2, bDown);
			}
			previousButtonState = buttonState;
			return 1;
		}

		if (actionType == AMOTION_EVENT_ACTION_SCROLL)
		{
			if (GetAxes)
			{
				float WheelDelta = GetAxes(event, AMOTION_EVENT_AXIS_VSCROLL, 0);

				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mouse scroll: %f"), WheelDelta);
				FAndroidInputInterface::MouseWheelEvent(device, WheelDelta);
			}
			return 1;
		}

		if (GetAxes && (actionType == AMOTION_EVENT_ACTION_MOVE || actionType == AMOTION_EVENT_ACTION_HOVER_MOVE))
		{
			float XAbsolute = GetAxes(event, AMOTION_EVENT_AXIS_X, 0);
			float YAbsolute = GetAxes(event, AMOTION_EVENT_AXIS_Y, 0);
			float XRelative = GetAxes(event, AMOTION_EVENT_AXIS_RELATIVE_X, 0);
			float YRelative = GetAxes(event, AMOTION_EVENT_AXIS_RELATIVE_Y, 0);

			if (XRelative != 0.0f || YRelative != 0.0f)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mouse absolute: (%f, %f), relative (%f, %f)"), XAbsolute, YAbsolute, XRelative, YRelative);
				FAndroidInputInterface::MouseMoveEvent(device, XAbsolute, YAbsolute, XRelative, YRelative);
			}
		}

		return 1;
	}

	if (EventType == AINPUT_EVENT_TYPE_MOTION)
	{
		int action = AMotionEvent_getAction(event);
		int actionType = action & AMOTION_EVENT_ACTION_MASK;
		size_t actionPointer = (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
		bool isActionTargeted = (actionType == AMOTION_EVENT_ACTION_POINTER_DOWN || actionType == AMOTION_EVENT_ACTION_POINTER_UP);
		int32 device = AInputEvent_getDeviceId(event);

		// trap Joystick events first, with fallthrough if there is no joystick support
		if (((EventSource & AINPUT_SOURCE_CLASS_JOYSTICK) == AINPUT_SOURCE_CLASS_JOYSTICK) &&
			(GetAxes != NULL) &&
			(actionType == AMOTION_EVENT_ACTION_MOVE))
		{
			const int axisCount = sizeof(AxisList)/sizeof(int32_t);

			// poll all the axes and forward to update controller state
			for (int axis = 0; axis < axisCount; axis++)
			{
				float val = GetAxes( event, AxisList[axis], 0);
				FAndroidInputInterface::JoystickAxisEvent(device, AxisList[axis], val);
			}

			// handle L/R trigger and Brake/Gas special (all in 0..1 range)
			// LTRIGGER will either be LTRIGGER or BRAKE, whichever is larger
			// RTRIGGER will either be RTRIGGER or GAS, whichever is larger
			float ltrigger = GetAxes(event, AMOTION_EVENT_AXIS_LTRIGGER, 0);
			float rtrigger = GetAxes(event, AMOTION_EVENT_AXIS_RTRIGGER, 0);
			float brake = GetAxes(event, AMOTION_EVENT_AXIS_BRAKE, 0);
			float gas = GetAxes(event, AMOTION_EVENT_AXIS_GAS, 0);
			FAndroidInputInterface::JoystickAxisEvent(device, AMOTION_EVENT_AXIS_LTRIGGER, ltrigger > brake ? ltrigger : brake);
			FAndroidInputInterface::JoystickAxisEvent(device, AMOTION_EVENT_AXIS_RTRIGGER, rtrigger > gas ? rtrigger : gas);

			return 1;
		}
		else
		{
			TArray<TouchInput> TouchesArray;
			
			TouchType type = TouchEnded;

			switch (actionType)
			{
			case AMOTION_EVENT_ACTION_DOWN:
			case AMOTION_EVENT_ACTION_POINTER_DOWN:
				type = TouchBegan;
				break;
			case AMOTION_EVENT_ACTION_MOVE:
				type = TouchMoved;
				break;
			case AMOTION_EVENT_ACTION_UP:
			case AMOTION_EVENT_ACTION_POINTER_UP:
			case AMOTION_EVENT_ACTION_CANCEL:
			case AMOTION_EVENT_ACTION_OUTSIDE:
				type = TouchEnded;
				break;
			case AMOTION_EVENT_ACTION_SCROLL:
			case AMOTION_EVENT_ACTION_HOVER_ENTER:
			case AMOTION_EVENT_ACTION_HOVER_MOVE:
			case AMOTION_EVENT_ACTION_HOVER_EXIT:
				return 0;
			default:
				UE_LOG(LogAndroid, Verbose, TEXT("Unknown AMOTION_EVENT %d ignored"), actionType);
				return 0;
			}

			size_t pointerCount = AMotionEvent_getPointerCount(event);

			if (pointerCount == 0)
			{
				return 1;
			}

			ANativeWindow* Window = (ANativeWindow*)FAndroidWindow::GetHardwareWindow_EventThread();
			if (!Window)
			{
				return 0;
			}

			int32_t Width = 0 ;
			int32_t Height = 0 ;

			if(Window)
			{
				// we are on the event thread. true here indicates we will retrieve dimensions from the current window.
				FAndroidWindow::CalculateSurfaceSize(Width, Height, true);
			}

			// make sure OpenGL context created before accepting touch events.. FAndroidWindow::GetScreenRect() may try to create it early from wrong thread if this is the first call
			if (!GAndroidGPUInfoReady)
			{
				return 1;
			}
			FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect(true);

			if (AndroidThunkCpp_IsVirtuaKeyboardShown() && (type == TouchBegan || type == TouchMoved))
			{
				//ignore key down events when the native input was clicked or when the keyboard animation is playing
				if (TryIgnoreClick(event, actionPointer))
				{
					return 0;
				}
			}
			else if (AndroidThunkCpp_IsWebViewShown() && (type == TouchBegan || type == TouchMoved || type == TouchEnded))
			{
				//ignore key down events when the the a web view is visible
				if (TryIgnoreClick(event, actionPointer) && ((EventSource & 0x80) != 0x80))
				{
					UE_LOG(LogAndroid, Verbose, TEXT("Received touch event %d - Ignored"), type);
					return 0;
				}
				UE_LOG(LogAndroid, Verbose, TEXT("Received touch event %d"), type);
			}
			if(isActionTargeted)
			{
				if(actionPointer < 0 || pointerCount < (int)actionPointer)
				{
					return 1;
				}

				int pointerId = AMotionEvent_getPointerId(event, actionPointer);
				float x = FMath::Min<float>(AMotionEvent_getX(event, actionPointer) / Width, 1.f);
				x *= (ScreenRect.Right - 1);
				float y = FMath::Min<float>(AMotionEvent_getY(event, actionPointer) / Height, 1.f);
				y *= (ScreenRect.Bottom - 1);

				UE_LOG(LogAndroid, Verbose, TEXT("Received targeted motion event from pointer %u (id %d) action %d: (%.2f, %.2f)"), actionPointer, pointerId, action, x, y);

				TouchInput TouchMessage;
				TouchMessage.DeviceId = device;
				TouchMessage.Handle = pointerId;
				TouchMessage.Type = type;
				TouchMessage.Position = FVector2D(x, y);
				TouchMessage.LastPosition = FVector2D(x, y);		//@todo android: AMotionEvent_getHistoricalRawX
				TouchesArray.Add(TouchMessage);
			}
			else
			{
				for (size_t i = 0; i < pointerCount; ++i)
				{
					int pointerId = AMotionEvent_getPointerId(event, i);

					float x = FMath::Min<float>(AMotionEvent_getX(event, i) / Width, 1.f);
					x *= (ScreenRect.Right - 1);
					float y = FMath::Min<float>(AMotionEvent_getY(event, i) / Height, 1.f);
					y *= (ScreenRect.Bottom - 1);

					UE_LOG(LogAndroid, Verbose, TEXT("Received motion event from index %u (id %d) action %d: (%.2f, %.2f)"), i, pointerId, action, x, y);

					TouchInput TouchMessage;
					TouchMessage.DeviceId= device;
					TouchMessage.Handle = pointerId;
					TouchMessage.Type = type;
					TouchMessage.Position = FVector2D(x, y);
					TouchMessage.LastPosition = FVector2D(x, y);		//@todo android: AMotionEvent_getHistoricalRawX
					TouchesArray.Add(TouchMessage);
				}
			}

			FAndroidInputInterface::QueueTouchInput(TouchesArray);

#if !UE_BUILD_SHIPPING
			if ((pointerCount >= 4) && (type == TouchBegan))
			{
				bool bShowConsole = true;
				GConfig->GetBool(TEXT("/Script/Engine.InputSettings"), TEXT("bShowConsoleOnFourFingerTap"), bShowConsole, GInputIni);

				if (bShowConsole)
				{
					GShowConsoleWindowNextTick = true;
				}
			}
#endif
		}

		return 0;
	}

	if (EventType == AINPUT_EVENT_TYPE_KEY)
	{
		int keyCode = AKeyEvent_getKeyCode(event);
		int keyFlags = AKeyEvent_getFlags(event);
		bool bSoftKey = (keyFlags & AKEY_EVENT_FLAG_SOFT_KEYBOARD) != 0;

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Received keycode: %d, softkey: %d"), keyCode, bSoftKey ? 1 : 0);

		//Only pass on the device id if really a gamepad, joystick or dpad (allows menu and back to be treated as gamepad events)
		int32 device = -1;
		if ((((EventSource & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK) && (GetAxes != NULL)) ||
			((EventSource & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD) ||
			((EventSource & AINPUT_SOURCE_DPAD) == AINPUT_SOURCE_DPAD))
		{
			device = AInputEvent_getDeviceId(event);
		}

		//Trap codes handled as possible gamepad events
		if (device >= 0 && ValidGamepadKeyCodes.Contains(keyCode))
		{
			
			bool down = AKeyEvent_getAction(event) != AKEY_EVENT_ACTION_UP;
			FAndroidInputInterface::JoystickButtonEvent(device, keyCode, down);
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Received gamepad button: %d"), keyCode);
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Received key event: %d"), keyCode);

			// only handle mapped key codes
			if (!MappedKeyCodes.Contains(keyCode))
			{
				return 0;
			}

			if (bSoftKey || GAndroidEnableHardwareKeyboard || AlwaysAllowedKeyCodes.Contains(keyCode))
			{
				FDeferredAndroidMessage Message;

				Message.messageType = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_UP ? MessageType_KeyUp : MessageType_KeyDown;
				Message.KeyEventData.unichar = keyCode;
				Message.KeyEventData.keyId = keyCode;
				Message.KeyEventData.modifier = AKeyEvent_getMetaState(event);
				Message.KeyEventData.isRepeat = AKeyEvent_getAction(event) == AKEY_EVENT_ACTION_MULTIPLE;
				FAndroidInputInterface::DeferMessage(Message);
			}

			// allow event to be generated for volume up and down, but conditionally allow system to handle it, too
			if (keyCode == AKEYCODE_VOLUME_UP || keyCode == AKEYCODE_VOLUME_DOWN)
			{
				if (FPlatformMisc::GetVolumeButtonsHandledBySystem())
				{
					return 0;
				}
			}

			// optionally forward back button
			if (keyCode == AKEYCODE_BACK && GAllowJavaBackButtonEvent)
			{
				return 0;
			}
		}

		return 1;
	}

	return 0;
}

static bool bShouldRestartFromInterrupt = false;
static bool bIgnorePauseOnDownloaderStart = false;
static bool IsStartupMoviePlaying()
{
	return GEngine && GEngine->IsInitialized() && GetMoviePlayer() && GetMoviePlayer()->IsStartupMoviePlaying();
}

static bool IsPreLoadScreenPlaying()
{
    return IsStartupMoviePlaying() 
        || (FPreLoadScreenManager::Get() && (FPreLoadScreenManager::Get()->HasValidActivePreLoadScreen()));
}

FAppEventData::FAppEventData(ANativeWindow* WindowIn)
{
	check(WindowIn);
	WindowWidth = ANativeWindow_getWidth(WindowIn);
	WindowHeight = ANativeWindow_getHeight(WindowIn);
	check(WindowWidth >= 0 && WindowHeight >= 0);
}

static bool bAppIsActive_EventThread = false;


// called when the app has focus + window + resume.
static void ActivateApp_EventThread()
{
	if (bAppIsActive_EventThread)
	{
		// Seems this can occur.
		return;
	}

	// Unlock window when we're ready.
	UE_LOG(LogAndroid, Log, TEXT("event thread, activate app, unlocking HW window"));
	GAndroidWindowLock.Unlock();
	// wake the GT up.
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_APP_ACTIVATED);

	bAppIsActive_EventThread = true;
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_RUN_CALLBACK, FAppEventData([]()
	{
		UE_LOG(LogAndroid, Log, TEXT("performing app foregrounding callback."));
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
		FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
		FAppEventManager::GetInstance()->ResumeAudio();
	}));

	if (EventHandlerEvent)
	{
		// Must flush the queue before enabling rendering.
		EventHandlerEvent->Trigger();
	}

	FThreadHeartBeat::Get().ResumeHeartBeat(true);

	FPreLoadScreenManager::EnableRendering(true);

	extern void AndroidThunkCpp_ShowHiddenAlertDialog();
	AndroidThunkCpp_ShowHiddenAlertDialog();	
}

extern void BlockRendering();
// called whenever the app loses focus, loses window or pause.
static void SuspendApp_EventThread()
{
	if (!bAppIsActive_EventThread)
	{
		return;
	}
	bAppIsActive_EventThread = false;
	// Lock the window, this prevents event thread from removing the window whilst the RHI initializes.

	UE_LOG(LogAndroid, Log, TEXT("event thread, suspending app, acquiring HW window lock."));
	GAndroidWindowLock.Lock();

	if (bReadyToProcessEvents == false)
	{
		// App has stopped before we can process events.
		// AndroidLaunch will lock GAndroidWindowLock, and set bReadyToProcessEvents when we are able to block the RHI and queue up other events.
		// we ignore events until this point as acquiring GAndroidWindowLock means requires the window to be properly initialized.
		UE_LOG(LogAndroid, Log, TEXT("event thread, app not yet ready."));
		return;
	};

	TSharedPtr<FEvent, ESPMode::ThreadSafe> EMDoneTrigger = MakeShareable(FPlatformProcess::GetSynchEventFromPool(), [](FEvent* EventToDelete)
	{
		FPlatformProcess::ReturnSynchEventToPool(EventToDelete);
	});

	// perform the delegates before the window handle is cleared.
	// This ensures any tasks that require a window handle will have it before we block the RT on the invalid window.
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_RUN_CALLBACK, FAppEventData([EMDoneTrigger]()
	{
		UE_LOG(LogAndroid, Log, TEXT("performing app backgrounding callback. %p"), EMDoneTrigger.Get());

		FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
		FAppEventManager::GetInstance()->PauseAudio();
		FAppEventManager::ReleaseMicrophone(false);
		EMDoneTrigger->Trigger();
	}));

	FEmbeddedCommunication::WakeGameThread();

	FPreLoadScreenManager::EnableRendering(false);

	FThreadHeartBeat::Get().SuspendHeartBeat(true);

	// wait for a period of time before blocking rendering
	UE_LOG(LogAndroid, Log, TEXT("AndroidEGL::  SuspendApp_EventThread, waiting for event manager to process. tid: %d"), FPlatformTLS::GetCurrentThreadId());
	bool bSuccess = EMDoneTrigger->Wait(4000);
	UE_CLOG(!bSuccess, LogAndroid, Log, TEXT("backgrounding callback, not responded in timely manner."));

	BlockRendering();

	// Suspend the GT.
	FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_APP_SUSPENDED);
}

//Called from the event process thread
static void OnAppCommandCB(struct android_app* app, int32_t cmd)
{
	check(IsInAndroidEventThread());
	static bool bDidGainFocus = false;
	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("OnAppCommandCB cmd: %u, tid = %d"), cmd, gettid());

	static bool bHasFocus = false;
	static bool bHasWindow = false;
	static bool bIsResumed = false;

	// Set event thread's view of the window dimensions:
	{
		ANativeWindow* DimensionWindow = app->pendingWindow ? app->pendingWindow : app->window;
		if (DimensionWindow)
		{
			FAndroidWindow::SetWindowDimensions_EventThread(DimensionWindow);
		}
	}

	switch (cmd)
	{
	case APP_CMD_SAVE_STATE:
		/**
		* Command from main thread: the app should generate a new saved state
		* for itself, to restore from later if needed.  If you have saved state,
		* allocate it with malloc and place it in android_app.savedState with
		* the size in android_app.savedStateSize.  The will be freed for you
		* later.
		*/
		// the OS asked us to save the state of the app
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_SAVE_STATE"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_SAVE_STATE);
		break;
	case APP_CMD_INIT_WINDOW:
		/**
		 * Command from main thread: a new ANativeWindow is ready for use.  Upon
		 * receiving this command, android_app->window will contain the new window
		 * surface.
		 */
		// get the window ready for showing
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case APP_CMD_INIT_WINDOW"));
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_INIT_WINDOW"));
		FAppEventManager::GetInstance()->HandleWindowCreated_EventThread(app->pendingWindow);
		bHasWindow = true;
		if (bHasWindow && bHasFocus && bIsResumed)
		{
			ActivateApp_EventThread();
		}
		break;
	case APP_CMD_TERM_WINDOW:
		/**
		 * Command from main thread: the existing ANativeWindow needs to be
		 * terminated.  Upon receiving this command, android_app->window still
		 * contains the existing window; after calling android_app_exec_cmd
		 * it will be set to NULL.
		 */
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case APP_CMD_TERM_WINDOW, tid = %d"), gettid());
		// clean up the window because it is being hidden/closed
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_TERM_WINDOW"));

		SuspendApp_EventThread();

		FAppEventManager::GetInstance()->HandleWindowClosed_EventThread();
		bHasWindow = false;
		break;
	case APP_CMD_LOST_FOCUS:
		/**
		 * Command from main thread: the app's activity window has lost
		 * input focus.
		 */
		// if the app lost focus, avoid unnecessary processing (like monitoring the accelerometer)
		bHasFocus = false;

		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_LOST_FOCUS"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_LOST_FOCUS);

		break;
	case APP_CMD_GAINED_FOCUS:
		/**
		 * Command from main thread: the app's activity window has gained
		 * input focus.
		 */

		// remember gaining focus so we know any later pauses are not part of first startup
		bDidGainFocus = true;
		bHasFocus = true;

		// bring back a certain functionality, like monitoring the accelerometer
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_GAINED_FOCUS"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_GAINED_FOCUS);

		if (bHasWindow && bHasFocus && bIsResumed)
		{
			ActivateApp_EventThread();
		}
		break;
	case APP_CMD_INPUT_CHANGED:
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_INPUT_CHANGED"));
		break;
	case APP_CMD_WINDOW_RESIZED:
		/**
		 * Command from main thread: the current ANativeWindow has been resized.
		 * Please redraw with its new size.
		 */
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_WINDOW_RESIZED"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_RESIZED, FAppEventData(app->window));
		break;
	case APP_CMD_WINDOW_REDRAW_NEEDED:
		/**
		 * Command from main thread: the system needs that the current ANativeWindow
		 * be redrawn.  You should redraw the window before handing this to
		 * android_app_exec_cmd() in order to avoid transient drawing glitches.
		 */
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_WINDOW_REDRAW_NEEDED"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_REDRAW_NEEDED );
		break;
	case APP_CMD_CONTENT_RECT_CHANGED:
		/**
		 * Command from main thread: the content area of the window has changed,
		 * such as from the soft input window being shown or hidden.  You can
		 * find the new content rect in android_app::contentRect.
		 */
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_CONTENT_RECT_CHANGED"));
		break;
	/* receive this event from Java instead to work around NDK bug with AConfiguration_getOrientation in Oreo
	case APP_CMD_CONFIG_CHANGED:
		{
			// Command from main thread: the current device configuration has changed.
			UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_CONFIG_CHANGED"));
			
			bool bPortrait = (AConfiguration_getOrientation(app->config) == ACONFIGURATION_ORIENTATION_PORT);
			if (FAndroidWindow::OnWindowOrientationChanged(bPortrait))
			{
				FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CHANGED);
			}
		}
		break;
	*/
	case APP_CMD_LOW_MEMORY:
		/**
		 * Command from main thread: the system is running low on memory.
		 * Try to reduce your memory use.
		 */
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_LOW_MEMORY"));
		break;
	case APP_CMD_START:
		/**
		 * Command from main thread: the app's activity has been started.
		 */
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_START"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_START);
	
		break;
	case APP_CMD_RESUME:
		/**
		 * Command from main thread: the app's activity has been resumed.
		 */
		bIsResumed = true;
		if (bHasWindow && bHasFocus && bIsResumed)
		{
			ActivateApp_EventThread();
		}
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case APP_CMD_RESUME"));
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_RESUME"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_RESUME);

		/*
		* On the initial loading the restart method must be called immediately
		* in order to restart the app if the startup movie was playing
		*/
		if (bShouldRestartFromInterrupt)
		{
			AndroidThunkCpp_RestartApplication(TEXT(""));
		}
		break;
	case APP_CMD_PAUSE:
	{
		/**
		 * Command from main thread: the app's activity has been paused.
		 */
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Case APP_CMD_PAUSE"));
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_PAUSE"));

		// Ignore pause command for Oculus if the window hasn't been initialized to prevent halting initial load
		// if the headset is not active
		if (!bHasWindow && FAndroidMisc::GetDeviceMake() == FString("Oculus"))
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Oculus: Ignoring APP_CMD_PAUSE command before APP_CMD_INIT_WINDOW"));
			UE_LOG(LogAndroid, Log, TEXT("Oculus: Ignoring APP_CMD_PAUSE command before APP_CMD_INIT_WINDOW"));
			break;
		}

		bIsResumed = false;
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_PAUSE);

		bool bAllowReboot = true;
#if FAST_BOOT_HACKS
		if (FEmbeddedDelegates::GetNamedObject(TEXT("LoggedInObject")) == nullptr)
		{
			bAllowReboot = false;
		}
#endif

		// Restart on resuming if did not complete engine initialization
		if (!bDidCompleteEngineInit && bDidGainFocus  && !bIgnorePauseOnDownloaderStart && bAllowReboot)
		{
// 			// only do this if early startup enabled
// 			FString *EarlyRestart = FAndroidMisc::GetConfigRulesVariable(TEXT("earlyrestart"));
// 			if (EarlyRestart != NULL && EarlyRestart->Equals("true", ESearchCase::IgnoreCase))
// 			{
// 				bShouldRestartFromInterrupt = true;
// 			}
		}
		bIgnorePauseOnDownloaderStart = false;

		/*
		 * On the initial loading the pause method must be called immediately
		 * in order to stop the startup movie's sound
		*/
		if (IsPreLoadScreenPlaying() && bAllowReboot)
		{
			UE_LOG(LogAndroid, Log, TEXT("MoviePlayer force completion"));
			GetMoviePlayer()->ForceCompletion();
		}

		SuspendApp_EventThread();

		break;
	}
	case APP_CMD_STOP:
		/**
		 * Command from main thread: the app's activity has been stopped.
		 */
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_STOP"));
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_STOP);
		break;
	case APP_CMD_DESTROY:
		/**
		* Command from main thread: the app's activity is being destroyed,
		* and waiting for the app thread to clean up and exit before proceeding.
		*/
		UE_LOG(LogAndroid, Log, TEXT("Case APP_CMD_DESTROY"));

		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_RUN_CALLBACK, FAppEventData([]()
		{
			FGraphEventRef WillTerminateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
			{
				FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
			}, TStatId(), NULL, ENamedThreads::GameThread);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(WillTerminateTask);
			FAndroidMisc::NonReentrantRequestExit();
		}));


		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_ON_DESTROY);

		// Exit here, avoids having to unlock the window and letting the RHI's deal with invalid window.
		extern void AndroidThunkCpp_ForceQuit();
		AndroidThunkCpp_ForceQuit();

		break;
	}

	if (EventHandlerEvent)
	{
		EventHandlerEvent->Trigger();
	}

	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("#### END OF OnAppCommandCB cmd: %u, tid = %d"), cmd, gettid());
}

//Native-defined functions

JNI_METHOD jint Java_com_epicgames_unreal_GameActivity_nativeGetCPUFamily(JNIEnv* jenv, jobject thiz)
{
	return (jint)android_getCpuFamily();
}

JNI_METHOD jboolean Java_com_epicgames_unreal_GameActivity_nativeSupportsNEON(JNIEnv* jenv, jobject thiz)
{
	AndroidCpuFamily Family = android_getCpuFamily();

	if (Family == ANDROID_CPU_FAMILY_ARM64)
	{
		return JNI_TRUE;
	}
	if (Family == ANDROID_CPU_FAMILY_ARM)
	{
		return ((android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0) ? JNI_TRUE : JNI_FALSE;
	}
	return JNI_FALSE;
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeOnConfigurationChanged(boolean bPortrait);
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOnConfigurationChanged(JNIEnv* jenv, jobject thiz, jboolean bPortrait)
{
	bool bChangedToPortrait = bPortrait == JNI_TRUE;

	// enqueue a window changed event if orientation changed, 
	// note that the HW window handle does not necessarily change.
	if (FAndroidWindow::OnWindowOrientationChanged(bChangedToPortrait))
	{	
		// Enqueue an event to trigger gamethread to update the orientation:
		FAppEventManager::GetInstance()->EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CHANGED);

		if (EventHandlerEvent)
		{
			EventHandlerEvent->Trigger();
		}
	}
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeConsoleCommand(String commandString);"
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeConsoleCommand(JNIEnv* jenv, jobject thiz, jstring commandString)
{
	FString Command = FJavaHelper::FStringFromParam(jenv, commandString);
	if (GEngine != NULL)
	{
		// Run on game thread to avoid race condition with DeferredCommands
		AsyncTask(ENamedThreads::GameThread, [Command]()
		{
			GEngine->DeferredCommands.Add(Command);
		});
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Ignoring console command (too early): %s"), *Command);
	}
}

// This is called from the Java UI thread for initializing VR HMDs
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeInitHMDs(JNIEnv* jenv, jobject thiz)
{
	for (auto HMDModuleIt = GHMDImplementations.CreateIterator(); HMDModuleIt; ++HMDModuleIt)
	{
		(*HMDModuleIt)->PreInit();
	}

	GHMDsInitialized = true;
}

JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeSetAndroidVersionInformation(JNIEnv* jenv, jobject thiz, jstring androidVersion, jint targetSDKversion, jstring phoneMake, jstring phoneModel, jstring phoneBuildNumber, jstring osLanguage)
{
	auto UEAndroidVersion = FJavaHelper::FStringFromParam(jenv, androidVersion);
	auto UEPhoneMake = FJavaHelper::FStringFromParam(jenv, phoneMake);
	auto UEPhoneModel = FJavaHelper::FStringFromParam(jenv, phoneModel);
	auto UEPhoneBuildNumber = FJavaHelper::FStringFromParam(jenv, phoneBuildNumber);
	auto UEOSLanguage = FJavaHelper::FStringFromParam(jenv, osLanguage);

	FAndroidMisc::SetVersionInfo(UEAndroidVersion, targetSDKversion, UEPhoneMake, UEPhoneModel, UEPhoneBuildNumber, UEOSLanguage);
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeOnInitialDownloadStarted();
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOnInitialDownloadStarted(JNIEnv* jenv, jobject thiz)
{
	bIgnorePauseOnDownloaderStart = true;
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeOnInitialDownloadCompleted();
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeOnInitialDownloadCompleted(JNIEnv* jenv, jobject thiz)
{
	bIgnorePauseOnDownloaderStart = false;
}

// MERGE-TODO: Anticheat concerns with custom input
bool GAllowCustomInput = true;
JNI_METHOD void Java_com_epicgames_unreal_NativeCalls_HandleCustomTouchEvent(JNIEnv* jenv, jobject thiz, jint deviceId, jint pointerId, jint action, jint soucre, jfloat x, jfloat y)
{
#if ANDROID_ALLOWCUSTOMTOUCHEVENT
	// make sure fake input is allowed, so hacky Java can't run bots
	if (!GAllowCustomInput)
	{
		return;
	}

	TArray<TouchInput> TouchesArray;

	TouchInput TouchMessage;
	TouchMessage.DeviceId = deviceId;
	TouchMessage.Handle = pointerId;
	switch (action) {
	case 0: //  MotionEvent.ACTION_DOWN
		TouchMessage.Type = TouchBegan;
		break;
	case 2: // MotionEvent.ACTION_MOVE
		TouchMessage.Type = TouchMoved;
		break;
	default: // MotionEvent.ACTION_UP
		TouchMessage.Type = TouchEnded;
		break;
	}
	TouchMessage.Position = FVector2D(x, y);
	TouchMessage.LastPosition = FVector2D(x, y);

	TouchesArray.Add(TouchMessage);

	UE_LOG(LogAndroid, Verbose, TEXT("Handle custom touch event %d (%d) x=%f y=%f"), TouchMessage.Type, action, x, y);
	FAndroidInputInterface::QueueTouchInput(TouchesArray);
#endif
}

JNI_METHOD void Java_com_epicgames_unreal_NativeCalls_AllowJavaBackButtonEvent(JNIEnv* jenv, jobject thiz, jboolean allow)
{
	GAllowJavaBackButtonEvent = (allow == JNI_TRUE);
}

bool WaitForAndroidLoseFocusEvent(double TimeoutSeconds)
{
	return FAppEventManager::GetInstance()->WaitForEventInQueue(EAppEventState::APP_EVENT_STATE_WINDOW_LOST_FOCUS, TimeoutSeconds);
}

#endif
