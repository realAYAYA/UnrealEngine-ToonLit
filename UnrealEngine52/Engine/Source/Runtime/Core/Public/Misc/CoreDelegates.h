// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Async/TaskGraphInterfaces.h"
#endif
#include "Async/TaskGraphFwd.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "Logging/LogCategory.h"
#include "Logging/LogVerbosity.h"
#include "Math/IntVector.h"
#include "Math/MathFwd.h"
#include "Misc/AES.h"
#include "Misc/Build.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class AActor;
class Error;
class FConfigFile;
class FName;
class FSHAHash;
class FText;
class IPakFile;
class ITargetPlatform;
struct FGuid;

enum class EForkProcessRole : uint8;

// delegates for hotfixes
namespace EHotfixDelegates
{
	enum Type
	{
		Test,
	};
}


// this is an example of a hotfix arg and return value structure. Once we have other examples, it can be deleted.
struct FTestHotFixPayload
{
	FString Message;
	bool ValueToReturn;
	bool Result;
};

// Parameters passed to CrashOverrideParamsChanged used to customize crash report client behavior/appearance. If the corresponding bool is not true, this value will not be stored.
struct FCrashOverrideParameters
{
	/** Appended to the end of GameName (which is retreived from FApp::GetGameName). */
	FString GameNameSuffix;
	/** Default this to true for backward compatibility before these bools were added. */
	bool bSetCrashReportClientMessageText = true;
	bool bSetGameNameSuffix = false;
	TOptional<bool> SendUnattendedBugReports;
	TOptional<bool> SendUsageData;

	CORE_API ~FCrashOverrideParameters();
};

class CORE_API FCoreDelegates
{
public:
	// Callback for platform handling when flushing async loads.
	static TMulticastDelegate<void()> OnAsyncLoadingFlush;

	// Callback for a game thread interruption point when a async load flushing. Used to updating UI during long loads.
	static TMulticastDelegate<void()> OnAsyncLoadingFlushUpdate;

	// Callback on the game thread when an async load is started. This goes off before the packages has finished loading
	static TMulticastDelegate<void(const FString&)> OnAsyncLoadPackage;

	static TMulticastDelegate<void(const FString&)> OnSyncLoadPackage;

	// get a hotfix delegate
	static TDelegate<void(void*, int32)>& GetHotfixDelegate(EHotfixDelegates::Type HotFix);

	// Callback when a user logs in/out of the platform.
	static TMulticastDelegate<void(bool, int32, int32)> OnUserLoginChangedEvent;

	// Callback when controllers disconnected / reconnected
	UE_DEPRECATED(5.1, "OnControllerConnectionChange, use IPlatformInputDeviceMapper::GetOnInputDeviceConnectionChange() instead")
	static TMulticastDelegate<void(bool, FPlatformUserId, int32)> OnControllerConnectionChange;

	// Callback when a single controller pairing changes
	UE_DEPRECATED(5.1, "OnControllerPairingChange, use IPlatformInputDeviceMapper::GetOnInputDevicePairingChange() instead")
	static TMulticastDelegate<void(int32 ControllerIndex, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId)> OnControllerPairingChange;

	// Callback when a user changes the safe frame size
	static TMulticastDelegate<void()> OnSafeFrameChangedEvent;

	// Callback for mounting all the pak files in default locations
	static TDelegate<int32(const TArray<FString>&)> OnMountAllPakFiles;

	// Callback to prompt the pak system to mount a pak file
	static TDelegate<IPakFile*(const FString&, int32)> MountPak;

	// Callback to prompt the pak system to unmount a pak file.
	static TDelegate<bool(const FString&)> OnUnmountPak;

	// Callback to optimize memeory for currently mounted paks
	static TDelegate<void()> OnOptimizeMemoryUsageForMountedPaks;

	// After a pakfile is mounted this is called
	static TMulticastDelegate<void(const IPakFile&)> OnPakFileMounted2;

	// After a file is added this is called
	static TMulticastDelegate<void(const FString&)> NewFileAddedDelegate;

	// After an attempt to mount all pak files, but none wre found, this is called
	static TMulticastDelegate<void()> NoPakFilesMountedDelegate;

	// When a file is opened for read from a pak file
	static TMulticastDelegate<void(const TCHAR* PakFile, const TCHAR* FileName)> OnFileOpenedForReadFromPakFile;

	typedef TSharedPtr<class IMovieStreamer, ESPMode::ThreadSafe> FMovieStreamerPtr;

    // Delegate used to register a movie streamer with any movie player modules that bind to this delegate
    // Designed to be called when a platform specific movie streamer plugin starts up so that it doesn't need to implement a register for all movie player plugins
    static TMulticastDelegate<void(FMovieStreamerPtr)> RegisterMovieStreamerDelegate;
    // Delegate used to un-register a movie streamer with any movie player modules that bind to this delegate
    // Designed to be called when a platform specific movie streamer plugin shuts down so that it doesn't need to implement a register for all movie player plugins
    static TMulticastDelegate<void(FMovieStreamerPtr)> UnRegisterMovieStreamerDelegate;

	// Callback when an ensure has occurred
	static TMulticastDelegate<void()> OnHandleSystemEnsure;

	// Callback when an error (crash) has occurred
	static TMulticastDelegate<void()> OnHandleSystemError;

	// Called when an actor label is changed
	static TMulticastDelegate<void(AActor*)> OnActorLabelChanged;

	static TMulticastDelegate<void(const FGuid&, const FAES::FAESKey&)>& GetRegisterEncryptionKeyMulticastDelegate();
	static TDelegate<void(uint8[32])>& GetPakEncryptionKeyDelegate();
	static TDelegate<void(TArray<uint8>&, TArray<uint8>&)>& GetPakSigningKeysDelegate();

	

#if WITH_EDITOR
	// Called before the editor displays a modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
	static FSimpleMulticastDelegate PreModal;

	// Called after the editor dismisses a modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
	static FSimpleMulticastDelegate PostModal;
    
    // Called before the editor displays a Slate (non-platform) modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
    static FSimpleMulticastDelegate PreSlateModal;
    
    // Called after the editor dismisses a Slate (non-platform) modal window, allowing other windows the opportunity to disable themselves to avoid reentrant calls
    static FSimpleMulticastDelegate PostSlateModal;
    
#endif	//WITH_EDITOR

#if ALLOW_OTHER_PLATFORM_CONFIG
	// Called when the CVar (ConsoleManager) needs to retrieve CVars for a deviceprofile for another platform - this dramatically simplifies module dependencies
	typedef TMap<FName, FString> FCVarKeyValueMap;
	static TDelegate<FCVarKeyValueMap(const FString& DeviceProfileName)> GatherDeviceProfileCVars;
#endif

	// Called when an error occurred.
	static FSimpleMulticastDelegate OnShutdownAfterError;

	// Called when appInit is called, very early in startup
	static FSimpleMulticastDelegate OnInit;

	// Called during FEngineLoop::PreInit after GWarn & GError have been first set so that they can be overridden before anything in PreInit uses them
	static FSimpleMulticastDelegate OnOutputDevicesInit;

	// Called at the end of UEngine::Init, right before loading PostEngineInit modules for both normal execution and commandlets
	static FSimpleMulticastDelegate OnPostEngineInit;

	// Called after all modules have been loaded for all phases
	static FSimpleMulticastDelegate OnAllModuleLoadingPhasesComplete;

	// Called at the very end of engine initialization, right before the engine starts ticking. This is not called for commandlets
	static FSimpleMulticastDelegate OnFEngineLoopInitComplete;

	// Called when the application is about to exit.
	static FSimpleMulticastDelegate OnExit;

	// Called when before the application is exiting.
	static FSimpleMulticastDelegate OnPreExit;

	// Called before the engine exits. Separate from OnPreExit as OnEnginePreExit occurs before shutting down any core modules.
	static FSimpleMulticastDelegate OnEnginePreExit;

	/** Delegate for gathering up additional localization paths that are unknown to the engine core (such as plugins) */
	static TMulticastDelegate<void(TArray<FString>&)> GatherAdditionalLocResPathsCallback;

	/** Color picker color has changed, please refresh as needed*/
	static FSimpleMulticastDelegate ColorPickerChanged;

	/** requests to open a message box */
	static TDelegate<EAppReturnType::Type(EAppMsgType::Type, const FText&, const FText&)> ModalErrorMessage;

	/** Called when the user accepts an invitation to the current game */
	static TMulticastDelegate<void(const FString&, const FString&)> OnInviteAccepted;

	// Called at the beginning of a frame
	static FSimpleMulticastDelegate OnBeginFrame;

	// Called at the moment of sampling the input (currently on the gamethread)
	static FSimpleMulticastDelegate OnSamplingInput;

	// Called at the end of a frame
	static FSimpleMulticastDelegate OnEndFrame;

	// Called at the beginning of a frame on the renderthread
	static FSimpleMulticastDelegate OnBeginFrameRT;

	// Called at the end of a frame on the renderthread
	static FSimpleMulticastDelegate OnEndFrameRT;


	/** called before world origin shifting */
	static TMulticastDelegate<void(class UWorld*, FIntVector, FIntVector)> PreWorldOriginOffset;
	/** called after world origin shifting */
	static TMulticastDelegate<void(class UWorld*, FIntVector, FIntVector)> PostWorldOriginOffset;

	/** called when the main loop would otherwise starve. */
	static TDelegate<void()> StarvedGameLoop;

	// IOS-style temperature updates, allowing game to scale down to let temp drop (to avoid thermal throttling on mobile, for instance) */
	// There is a parellel enum in ApplicationLifecycleComponent
	enum class ETemperatureSeverity : uint8
	{
		Unknown,
		Good,
		Bad,
		Serious,
		Critical,

		NumSeverities,
	};
	static TMulticastDelegate<void(ETemperatureSeverity)> OnTemperatureChange;

	/** Called when the OS goes into low power mode */
	static TMulticastDelegate<void(bool)> OnLowPowerMode;

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSCountPreLoadConfigFileRespondersDelegate()")
	static TMulticastDelegate<void(const TCHAR* IniFilename, int32& ResponderCount)> CountPreLoadConfigFileRespondersDelegate;
	static TTSMulticastDelegate<void(const TCHAR* IniFilename, int32& ResponderCount)>& TSCountPreLoadConfigFileRespondersDelegate();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSPreLoadConfigFileDelegate()")
	static TMulticastDelegate<void(const TCHAR* IniFilename, FString& LoadedContents)> PreLoadConfigFileDelegate;
	static TTSMulticastDelegate<void(const TCHAR* IniFilename, FString& LoadedContents)>& TSPreLoadConfigFileDelegate();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSPreSaveConfigFileDelegate()")
	static TMulticastDelegate<void(const TCHAR* IniFilename, const FString& ContentsToSave, int32& SavedCount)> PreSaveConfigFileDelegate;
	static TTSMulticastDelegate<void(const TCHAR* IniFilename, const FString& ContentsToSave, int32& SavedCount)>& TSPreSaveConfigFileDelegate();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSOnFConfigCreated()")
	static TMulticastDelegate<void(const FConfigFile*)> OnFConfigCreated;
	static TTSMulticastDelegate<void(const FConfigFile*)>& TSOnFConfigCreated();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSOnFConfigDeleted()")
	static TMulticastDelegate<void(const FConfigFile*)> OnFConfigDeleted;
	static TTSMulticastDelegate<void(const FConfigFile*)>& TSOnFConfigDeleted();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSOnConfigValueRead()")
	static TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName, const TCHAR* Key)> OnConfigValueRead;
	static TTSMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName, const TCHAR* Key)>& TSOnConfigValueRead();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSOnConfigSectionRead()")
	static TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName)> OnConfigSectionRead;
	static TTSMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName)>& TSOnConfigSectionRead();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSOnConfigSectionNameRead()")
	static TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName)> OnConfigSectionNameRead;
	static TTSMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName)>& TSOnConfigSectionNameRead();

	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSOnConfigSectionsChanged()")
	static TMulticastDelegate<void(const FString& IniFilename, const TSet<FString>& SectionNames)> OnConfigSectionsChanged;
	static TTSMulticastDelegate<void(const FString& IniFilename, const TSet<FString>& SectionNames)>& TSOnConfigSectionsChanged();

	static TMulticastDelegate<void(const TCHAR* SectionName, const TCHAR* IniFilename, uint32 SetBy, bool bAllowCheating)> OnApplyCVarFromIni;

	static TMulticastDelegate<void(uint32 ResX, uint32 ResY)> OnSystemResolutionChanged;

#if WITH_EDITOR
	// called when a target platform changes it's return value of supported formats.  This is so anything caching those results can reset (like cached shaders for cooking)
	static TMulticastDelegate<void(const ITargetPlatform*)> OnTargetPlatformChangedSupportedFormats;

	// Called when a feature level is disabled by the user.
	static TMulticastDelegate<void(int, const FName&)> OnFeatureLevelDisabled;
#endif

	/** IOS-style application lifecycle delegates */

	// This is called when the application is about to be deactivated (e.g., due to a phone call or SMS or the sleep button).
	// The game should be paused if possible, etc...
	static TMulticastDelegate<void()> ApplicationWillDeactivateDelegate;

	// Called when the application has been reactivated (reverse any processing done in the Deactivate delegate)
	static TMulticastDelegate<void()> ApplicationHasReactivatedDelegate;

	// This is called when the application is being backgrounded (e.g., due to switching
	// to another app or closing it via the home button)
	// The game should release shared resources, save state, etc..., since it can be
	// terminated from the background state without any further warning.
	static TMulticastDelegate<void()> ApplicationWillEnterBackgroundDelegate; // for instance, hitting the home button

	// Called when the application is returning to the foreground (reverse any processing done in the EnterBackground delegate)
	static TMulticastDelegate<void()> ApplicationHasEnteredForegroundDelegate;

	// This *may* be called when the application is getting terminated by the OS.
	// There is no guarantee that this will ever be called on a mobile device,
	// save state when ApplicationWillEnterBackgroundDelegate is called instead.
	static TMulticastDelegate<void()> ApplicationWillTerminateDelegate;

	// Some platform have a System UI Overlay that can draw on top of the application.
	// The game might want to be notified so it can pause, etc...
	// Parameter (bool) should be true if the system UI is displayed, otherwise false should be passed
	static TMulticastDelegate<void(bool)> ApplicationSystemUIOverlayStateChangedDelegate;

	// Called when in the background, if the OS is giving CPU time to the device. It is very likely
	// this will never be called due to mobile OS backgrounded CPU restrictions. But if, for instance,
	// VOIP is active on iOS, the will be getting called
	static TMulticastDelegate<void(float DeltaTime)> MobileBackgroundTickDelegate;

	// Called when the OS needs control of the music (parameter is true) or when the OS returns
	// control of the music to the application (parameter is false). This can happen due to a
	// phone call or timer or other OS-level event. This is currently triggered only on iOS
	// devices.
	static TMulticastDelegate<void(bool)> UserMusicInterruptDelegate;
	
	// [iOS only] Called when the mute switch is detected as changed or when the
	// volume changes. Parameter 1 is the mute switch state (true is muted, false is
	// unmuted). Parameter 2 is the volume as an integer from 0 to 100.
	static TMulticastDelegate<void(bool, int)> AudioMuteDelegate;
	
	// [iOS only] Called when the audio device changes
	// For instance, when the headphones are plugged in or removed
	static TMulticastDelegate<void(bool)> AudioRouteChangedDelegate;

	// Generally, events triggering UserMusicInterruptDelegate or AudioMuteDelegate happen only
	// when a change occurs. When a system comes online needing the current audio state but the
	// event has already been broadcast, calling ApplicationRequestAudioState will force the
	// UserMusicInterruptDelegate and AudioMuteDelegate to be called again if the low-level
	// application layer supports it. Currently, this is available only on iOS.
	static TMulticastDelegate<void()> ApplicationRequestAudioState;
	
	// Called when the OS is running low on resources and asks the application to free up any cached resources, drop graphics quality etc.
	static TMulticastDelegate<void()> ApplicationShouldUnloadResourcesDelegate;

	// Called with arguments passed to the application on statup, perhaps meta data passed on by another application which launched this one.
	static TMulticastDelegate<void(const TArray<FString>&)> ApplicationReceivedStartupArgumentsDelegate;

	/** IOS-style push notification delegates */

	// called when the user grants permission to register for remote notifications
	static TMulticastDelegate<void(TArray<uint8>)> ApplicationRegisteredForRemoteNotificationsDelegate;

	// called when the user grants permission to register for notifications
	static TMulticastDelegate<void(int)> ApplicationRegisteredForUserNotificationsDelegate;

	// called when the application fails to register for remote notifications
	static TMulticastDelegate<void(FString)> ApplicationFailedToRegisterForRemoteNotificationsDelegate;

	// called when the application receives a remote notification
	static TMulticastDelegate<void(FString, int)> ApplicationReceivedRemoteNotificationDelegate;

	// called when the application receives a local notification
	static TMulticastDelegate<void(FString, int, int)> ApplicationReceivedLocalNotificationDelegate;

    // called when the application receives notice to perform a background fetch
    static TMulticastDelegate<void()> ApplicationPerformFetchDelegate;

    // called when the application receives notice that a background download has completed
    static TMulticastDelegate<void(FString)> ApplicationBackgroundSessionEventDelegate;

	/** Sent when a device screen orientation changes */
	static TMulticastDelegate<void(int32)> ApplicationReceivedScreenOrientationChangedNotificationDelegate;

	/** Checks to see if the stat is already enabled */
	static TMulticastDelegate<void(const TCHAR*, bool&, bool&)> StatCheckEnabled;

	/** Sent after each stat is enabled */
	static TMulticastDelegate<void(const TCHAR*)> StatEnabled;

	/** Sent after each stat is disabled */
	static TMulticastDelegate<void(const TCHAR*)> StatDisabled;

	/** Sent when all stats need to be disabled */
	static TMulticastDelegate<void(const bool)> StatDisableAll;

	// Called when an application is notified that the application license info has been updated.
	// The new license data should be polled and steps taken based on the results (i.e. halt application if license is no longer valid).
	static TMulticastDelegate<void()> ApplicationLicenseChange;

	/** Sent when the platform changed its laptop mode (for convertible laptops).*/
	static TMulticastDelegate<void(EConvertibleLaptopMode)> PlatformChangedLaptopMode;

	/** Sent when the platform needs the user to fix headset tracking on startup (Most platforms do not need this.) */
	static TMulticastDelegate<void()> VRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate;

	/** Sent when the platform finds that needed headset tracking on startup has completed (Most platforms do not need this.) */
	static TMulticastDelegate<void()> VRHeadsetTrackingInitializedDelegate;

	/** Sent when the platform requests a low-level VR recentering */
	static TMulticastDelegate<void()> VRHeadsetRecenter;

	/** Sent when connection to VR HMD is lost */
	static TMulticastDelegate<void()> VRHeadsetLost;

	/** Sent when connection to VR HMD is restored */
	static TMulticastDelegate<void()> VRHeadsetReconnected;

	/** Sent when connection to VR HMD connection is refused by the player */
	static TMulticastDelegate<void()> VRHeadsetConnectCanceled;

	/** Sent when the VR HMD detects that it has been put on by the player. */
	static TMulticastDelegate<void()> VRHeadsetPutOnHead;

	/** Sent when the VR HMD detects that it has been taken off by the player. */
	static TMulticastDelegate<void()> VRHeadsetRemovedFromHead;

	/** Sent when a 3DOF VR controller is recentered */
	static TMulticastDelegate<void()> VRControllerRecentered;

	/** Sent when application code changes the user activity hint string for analytics, crash reports, etc */
	static TMulticastDelegate<void(const FString&)> UserActivityStringChanged;

	/** Sent when application code changes the currently active game session. The exact semantics of this will vary between games but it is useful for analytics, crash reports, etc  */
	static TMulticastDelegate<void(const FString&)> GameSessionIDChanged;

	/** Sent when application code changes game state. The exact semantics of this will vary between games but it is useful for analytics, crash reports, etc  */
	static TMulticastDelegate<void(const FString&)> GameStateClassChanged;

	/** Sent by application code to set params that customize crash reporting behavior. */
	static TMulticastDelegate<void(const FCrashOverrideParameters&)> CrashOverrideParamsChanged;
	
	/** Sent by engine code when the "vanilla" status of the engine changes */
	static TMulticastDelegate<void(bool)> IsVanillaProductChanged;

	// Callback for platform specific very early init code.
	static TMulticastDelegate<void()>& GetPreMainInitDelegate();
	
	/** Sent when GConfig is finished initializing */
	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSConfigReadyForUse()")
	static TMulticastDelegate<void()> ConfigReadyForUse;
	static TTSMulticastDelegate<void()>& TSConfigReadyForUse();

	/** Callback for notifications regarding changes of the rendering thread. */

	/** Sent just after the rendering thread has been created. */
	static TMulticastDelegate<void()> PostRenderingThreadCreated;
	/* Sent just before the rendering thread is destroyed. */
	static TMulticastDelegate<void()> PreRenderingThreadDestroyed;

	// Callback to allow custom resolution of package names. Arguments are InRequestedName, OutResolvedName.
	// Should return True of resolution occured.
	static TArray<TDelegate<bool(const FString&, FString&)>> PackageNameResolvers;

	// Called to request that systems free whatever memory they are able to. Called early in LoadMap.
	// Caller is responsible for flushing rendering etc. See UEngine::TrimMemory
	static FSimpleMulticastDelegate& GetMemoryTrimDelegate();

	// Called to request that low level allocator free whatever memory they are able to. 
	static FSimpleMulticastDelegate& GetLowLevelAllocatorMemoryTrimDelegate();

	// Called to request that low level allocator must refreshed
	static FSimpleMulticastDelegate& GetRefreshLowLevelAllocatorDelegate();

	// Called when OOM event occurs, after backup memory has been freed, so there's some hope of being effective
	static FSimpleMulticastDelegate& GetOutOfMemoryDelegate();

	// Called from TerminateOnOutOfMemory in D3D11Util.cpp/D3D12Util.cpp
	static TMulticastDelegate<void(const uint64, const uint64)>& GetGPUOutOfMemoryDelegate();

	enum class EOnScreenMessageSeverity : uint8
	{
		Info,
		Warning,
		Error,
	};
	typedef TMultiMap<EOnScreenMessageSeverity, FText> FSeverityMessageMap;

	// Called when displaying on screen messages (like the "Lighting needs to be rebuilt"), to let other systems add any messages as needed
	// Sample Usage:
	// void GetMyOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages)
	// {
	//		OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::Format(LOCTEXT("MyMessage", "My Status: {0}"), SomeStatus));
	// }
	static TMulticastDelegate<void(FSeverityMessageMap&)> OnGetOnScreenMessages;

	static TDelegate<bool()> IsLoadingMovieCurrentlyPlaying;

	// Callback to allow user code to prevent url from being launched from FPlatformProcess::LaunchURL. Used to apply http allow list
	// Return true for to launch the url
	static TDelegate<bool(const TCHAR* URL)> ShouldLaunchUrl;

	// Callback when the application has been activated by protocol (with optional user id, depending on the platform)
	static TMulticastDelegate<void(const FString& Parameter, FPlatformUserId UserId /*= PLATFORMUSERID_NONE*/)> OnActivatedByProtocol;

	/** Sent when GC finish destroy takes more time than expected */
	static TMulticastDelegate<void(const FString&)> OnGCFinishDestroyTimeExtended;

	/** Called when the application's network initializes or shutdowns on platforms where the network stack is not always available */
	static TMulticastDelegate<void(bool bIsNetworkInitialized)> ApplicationNetworkInitializationChanged;

	/**
	 * Called when the connection state as reported by the platform changes
	 *
	 * @param LastConnectionState last state of the connection
	 * @param ConnectionState current state of the connection
	 */
	static TMulticastDelegate<void(ENetworkConnectionStatus LastConnectionState, ENetworkConnectionStatus ConnectionState)> OnNetworkConnectionStatusChanged;

	// Callback to let code read or write specialized binary data that is generated at Stage time, for optimizing data right before 
	// final game data is being written to disk
	// The TMap is a map of an identifier for owner of the data, and a boolean where true means the data is being generated (ie editor), and false 
	// means the data is for use (ie runtime game)
	struct FExtraBinaryConfigData
	{
		// the data that will be saved/loaded quickly
		TMap<FString, TArray<uint8>> Data;

		// Ini config data (not necessarily GConfig)
		class FConfigCacheIni& Config;

		// if true, the callback should fill out Data/Config
		bool bIsGenerating;

		FExtraBinaryConfigData(class FConfigCacheIni& InConfig, bool InIsGenerating)
			: Config(InConfig)
			, bIsGenerating(InIsGenerating)
		{
		}
	};
	
	UE_DEPRECATED(5.2, "This delegate is not thread-safe. Please use FCoreDelegates::TSAccessExtraBinaryConfigData()")
	static TMulticastDelegate<void(FExtraBinaryConfigData&)> AccessExtraBinaryConfigData;
	static TTSMulticastDelegate<void(FExtraBinaryConfigData&)>& TSAccessExtraBinaryConfigData();

	using FAttachShaderReadRequestFunc = TFunctionRef<class FIoRequest(const class FIoChunkId&, FGraphEventRef)>;
	static TDelegate<void(TArrayView<const FSHAHash>, FAttachShaderReadRequestFunc)> PreloadPackageShaderMaps;
	static TDelegate<void(TArrayView<const FSHAHash>)> ReleasePreloadedPackageShaderMaps;
	/** Called when the verbosity of a log category is changed */
	static TMulticastDelegate<void(const FLogCategoryName& CategoryName, ELogVerbosity::Type OldVerbosity, ELogVerbosity::Type NewVerbosity)> OnLogVerbosityChanged;

	UE_DEPRECATED(5.1, "Use FPackageStore::Mount() instead")
	static TDelegate<TSharedPtr<class IPackageStore>()> CreatePackageStore;

	// Called immediately before the parent process will start responding to signals to fork
	static FSimpleMulticastDelegate OnParentBeginFork;
	// Called each time immediately before the parent process forks itself
	static FSimpleMulticastDelegate OnParentPreFork;

	// Called immediately after the process spawned a fork
	static TMulticastDelegate<void(EForkProcessRole ProcessRole)> OnPostFork;
	// Called at the end of the frame where the process spawned a fork
	static FSimpleMulticastDelegate OnChildEndFramePostFork;

private:

	// Callbacks for hotfixes
	static TArray<TDelegate<void(void*, int32)>> HotFixDelegates;

	// This class is only for namespace use
	FCoreDelegates() = default;

public:	// deprecated delegate type aliases
	using FHotFixDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void(void*, int32)>`") = TDelegate<void(void*, int32)>;
	using FOnActorLabelChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(AActor*)>`") = TMulticastDelegate<void(AActor*)>;
	using FOnMountAllPakFiles UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<int32(const TArray<FString>&)>`") = TDelegate<int32(const TArray<FString>&)>;
	using FMountPak UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<IPakFile*(const FString&, int32)>`") = TDelegate<IPakFile*(const FString&, int32)>;
	using FOnUnmountPak UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<bool(const FString&)>`") = TDelegate<bool(const FString&)>;
	using FOnOptimizeMemoryUsageForMountedPaks UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void()>`") = TDelegate<void()>;
	using FOnPakFileMounted2 UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const IPakFile&)>`") = TMulticastDelegate<void(const IPakFile&)>;
	using FNoPakFilesMountedDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FOnModalMessageBox UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<EAppReturnType::Type(EAppMsgType::Type, const FText&, const FText&)>`") = TDelegate<EAppReturnType::Type(EAppMsgType::Type, const FText&, const FText&)>;
	using FOnHandleSystemEnsure UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FOnHandleSystemError UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FRegisterMovieStreamerDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FMovieStreamerPtr)>`") = TMulticastDelegate<void(FMovieStreamerPtr)>;
	using FUnRegisterMovieStreamerDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FMovieStreamerPtr)>`") = TMulticastDelegate<void(FMovieStreamerPtr)>;
	using FOnUserLoginChangedEvent UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool, int32, int32)>`") = TMulticastDelegate<void(bool, int32, int32)>;
	using FOnSafeFrameChangedEvent UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FOnInviteAccepted UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&, const FString&)>`") = TMulticastDelegate<void(const FString&, const FString&)>;
	using FRegisterEncryptionKeyMulticastDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FGuid&, const FAES::FAESKey&)>`") = TMulticastDelegate<void(const FGuid&, const FAES::FAESKey&)>;
	using FPakEncryptionKeyDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void(uint8[32])>`") = TDelegate<void(uint8[32])>;
	using FPakSigningKeysDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void(TArray<uint8>&, TArray<uint8>&)>`") = TDelegate<void(TArray<uint8>&, TArray<uint8>&)>;
	using FOnUserControllerConnectionChange UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool, FPlatformUserId, int32)>`") = TMulticastDelegate<void(bool, FPlatformUserId, int32)>;
	using FOnUserControllerPairingChange UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(int32 ControllerIndex, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId)>`") = TMulticastDelegate<void(int32 ControllerIndex, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId)>;
	using FOnAsyncLoadingFlush UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FOnAsyncLoadingFlushUpdate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FOnAsyncLoadPackage UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FOnSyncLoadPackage UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FNewFileAddedDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FOnFileOpenedForReadFromPakFile UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* PakFile, const TCHAR* FileName)>`") = TMulticastDelegate<void(const TCHAR* PakFile, const TCHAR* FileName)>;
#if ALLOW_OTHER_PLATFORM_CONFIG
	using FGatherDeviceProfileCVars UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<FCVarKeyValueMap(const FString& DeviceProfileName)>`") = TDelegate<FCVarKeyValueMap(const FString& DeviceProfileName)>;
#endif
	using FGatherAdditionalLocResPathsDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(TArray<FString>&)>`") = TMulticastDelegate<void(TArray<FString>&)>;
	using FWorldOriginOffset UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(class UWorld*, FIntVector, FIntVector)>`") = TMulticastDelegate<void(class UWorld*, FIntVector, FIntVector)>;
	using FStarvedGameLoop UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void()>`") = TDelegate<void()>;
	using FOnTemperatureChange UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(ETemperatureSeverity)>`") = TMulticastDelegate<void(ETemperatureSeverity)>;
	using FOnLowPowerMode UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool)>`") = TMulticastDelegate<void(bool)>;
	using FCountPreLoadConfigFileRespondersDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* IniFilename, int32& ResponderCount)>`") = TMulticastDelegate<void(const TCHAR* IniFilename, int32& ResponderCount)>;
	using FPreLoadConfigFileDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* IniFilename, FString& LoadedContents)>`") = TMulticastDelegate<void(const TCHAR* IniFilename, FString& LoadedContents)>;
	using FPreSaveConfigFileDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* IniFilename, const FString& ContentsToSave, int32& SavedCount)>`") = TMulticastDelegate<void(const TCHAR* IniFilename, const FString& ContentsToSave, int32& SavedCount)>;
	using FOnFConfigFileCreated UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FConfigFile*)>`") = TMulticastDelegate<void(const FConfigFile*)>;
	using FOnFConfigFileDeleted UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FConfigFile*)>`") = TMulticastDelegate<void(const FConfigFile*)>;
	using FOnConfigValueRead UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName, const TCHAR* Key)>`") = TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName, const TCHAR* Key)>;
	using FOnConfigSectionRead UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName)>`") = TMulticastDelegate<void(const TCHAR* IniFilename, const TCHAR* SectionName)>;
	using FOnConfigSectionsChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString& IniFilename, const TSet<FString>& SectionNames)>`") = TMulticastDelegate<void(const FString& IniFilename, const TSet<FString>& SectionNames)>;
	using FOnApplyCVarFromIni UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR* SectionName, const TCHAR* IniFilename, uint32 SetBy, bool bAllowCheating)>`") = TMulticastDelegate<void(const TCHAR* SectionName, const TCHAR* IniFilename, uint32 SetBy, bool bAllowCheating)>;
	using FOnSystemResolutionChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(uint32 ResX, uint32 ResY)>`") = TMulticastDelegate<void(uint32 ResX, uint32 ResY)>;
	using FOnTargetPlatformChangedSupportedFormats UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const ITargetPlatform*)>`") = TMulticastDelegate<void(const ITargetPlatform*)>;
	using FOnFeatureLevelDisabled UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(int, const FName&)>`") = TMulticastDelegate<void(int, const FName&)>;
	using FApplicationLifetimeDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FBackgroundTickDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(float DeltaTime)>`") = TMulticastDelegate<void(float DeltaTime)>;
	using FUserMusicInterruptDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool)>`") = TMulticastDelegate<void(bool)>;
	using FAudioMuteDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool, int)>`") = TMulticastDelegate<void(bool, int)>;
	using FAudioRouteChangedDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool)>`") = TMulticastDelegate<void(bool)>;
	using FApplicationRequestAudioState UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FApplicationStartupArgumentsDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TArray<FString>&)>`") = TMulticastDelegate<void(const TArray<FString>&)>;
	using FApplicationRegisteredForRemoteNotificationsDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(TArray<uint8>)>`") = TMulticastDelegate<void(TArray<uint8>)>;
	using FApplicationRegisteredForUserNotificationsDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(int)>`") = TMulticastDelegate<void(int)>;
	using FApplicationFailedToRegisterForRemoteNotificationsDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FString)>`") = TMulticastDelegate<void(FString)>;
	using FApplicationReceivedRemoteNotificationDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FString, int)>`") = TMulticastDelegate<void(FString, int)>;
	using FApplicationReceivedLocalNotificationDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FString, int, int)>`") = TMulticastDelegate<void(FString, int, int)>;
	using FApplicationPerformFetchDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FApplicationBackgroundSessionEventDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FString)>`") = TMulticastDelegate<void(FString)>;
	using FApplicationReceivedOnScreenOrientationChangedNotificationDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(int32)>`") = TMulticastDelegate<void(int32)>;
	using FStatCheckEnabled UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR*, bool&, bool&)>`") = TMulticastDelegate<void(const TCHAR*, bool&, bool&)>;
	using FStatEnabled UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR*)>`") = TMulticastDelegate<void(const TCHAR*)>;
	using FStatDisabled UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const TCHAR*)>`") = TMulticastDelegate<void(const TCHAR*)>;
	using FStatDisableAll UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const bool)>`") = TMulticastDelegate<void(const bool)>;
	using FApplicationLicenseChange UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FPlatformChangedLaptopMode UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(EConvertibleLaptopMode)>`") = TMulticastDelegate<void(EConvertibleLaptopMode)>;
	using FVRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetTrackingInitializedDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetRecenter UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetLost UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetReconnected UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetConnectCanceled UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetPutOnHead UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRHeadsetRemovedFromHead UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FVRControllerRecentered UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FOnUserActivityStringChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FOnGameSessionIDChange UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FOnGameStateClassChange UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FOnCrashOverrideParamsChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FCrashOverrideParameters&)>`") = TMulticastDelegate<void(const FCrashOverrideParameters&)>;
	using FOnIsVanillaProductChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool)>`") = TMulticastDelegate<void(bool)>;
	using FOnPreMainInit UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FConfigReadyForUse UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FRenderingThreadChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void()>`") = TMulticastDelegate<void()>;
	using FResolvePackageNameDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<bool(const FString&, FString&)>`") = TMulticastDelegate<bool(const FString&, FString&)>;
	using FGPUOutOfMemoryDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const uint64, const uint64)>`") = TMulticastDelegate<void(const uint64, const uint64)>;
	using FGetOnScreenMessagesDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FSeverityMessageMap&)>`") = TMulticastDelegate<void(FSeverityMessageMap&)>;
	using FIsLoadingMovieCurrentlyPlaying UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<bool()>`") = TDelegate<bool()>;
	using FShouldLaunchUrl UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<bool(const TCHAR* URL)>`") = TDelegate<bool(const TCHAR* URL)>;
	using FOnActivatedByProtocol UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString& Parameter, FPlatformUserId UserId /*= PLATFORMUSERID_NONE*/)>`") = TMulticastDelegate<void(const FString& Parameter, FPlatformUserId UserId /*= PLATFORMUSERID_NONE*/)>;
	using FOnGCFinishDestroyTimeExtended UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FString&)>`") = TMulticastDelegate<void(const FString&)>;
	using FApplicationNetworkInitializationChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(bool bIsNetworkInitialized)>`") = TMulticastDelegate<void(bool bIsNetworkInitialized)>;
	using FAccesExtraBinaryConfigData UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(FExtraBinaryConfigData&)>`") = TMulticastDelegate<void(FExtraBinaryConfigData&)>;
	using FPreloadPackageShaderMaps UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void(TArrayView<const FSHAHash>, FAttachShaderReadRequestFunc)>`") = TDelegate<void(TArrayView<const FSHAHash>, FAttachShaderReadRequestFunc)>;
	using FReleasePreloadedPackageShaderMaps UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<void(TArrayView<const FSHAHash>)>`") = TDelegate<void(TArrayView<const FSHAHash>)>;
	using FOnLogVerbosityChanged UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(const FLogCategoryName& CategoryName, ELogVerbosity::Type OldVerbosity, ELogVerbosity::Type NewVerbosity)>`") = TMulticastDelegate<void(const FLogCategoryName& CategoryName, ELogVerbosity::Type OldVerbosity, ELogVerbosity::Type NewVerbosity)>;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	using FCreatePackageStore UE_DEPRECATED(5.2, "Use template instantiation instead as `TDelegate<TSharedPtr<class IPackageStore>()>`") = TDelegate<TSharedPtr<class IPackageStore>()>;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	using FProcessForkDelegate UE_DEPRECATED(5.2, "Use template instantiation instead as `TMulticastDelegate<void(EForkProcessRole ProcessRole)>`") = TMulticastDelegate<void(EForkProcessRole ProcessRole)>;
};
