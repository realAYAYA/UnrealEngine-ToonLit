// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSCommandLineHelper.h"
#include "HAL/ExceptionHandling.h"
#include "Modules/Boilerplate/ModuleBoilerplate.h"
#include "Misc/CallbackDevice.h"
#include "IOS/IOSView.h"
#include "IOS/IOSWindow.h"
#include "Async/TaskGraphInterfaces.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "IOS/IOSPlatformMisc.h"
#include "IOS/IOSBackgroundURLSessionHandler.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/CommandLine.h"
#include "IOS/IOSPlatformFramePacer.h"
#include "IOS/IOSApplication.h"
#include "IOS/IOSAsyncTask.h"
#include "Misc/ConfigCacheIni.h"
#include "IOS/IOSPlatformCrashContext.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"
#include "Misc/App.h"
#include "Algo/AllOf.h"
#include "Misc/App.h"
#include "Misc/EmbeddedCommunication.h"
#if USE_MUTE_SWITCH_DETECTION
#include "SharkfoodMuteSwitchDetector.h"
#include "SharkfoodMuteSwitchDetector.m"
#endif

#include <AudioToolbox/AudioToolbox.h>
#include <AVFoundation/AVAudioSession.h>
#import <AVFoundation/AVFoundation.h>
#include "HAL/IConsoleManager.h"

#if WITH_ACCESSIBILITY
#include "IOS/Accessibility/IOSAccessibilityCache.h"
#endif

// this is the size of the game thread stack, it must be a multiple of 4k
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define GAME_THREAD_STACK_SIZE 2 * 1024 * 1024
#else
#define GAME_THREAD_STACK_SIZE 16 * 1024 * 1024
#endif



DEFINE_LOG_CATEGORY(LogIOSAudioSession);

int GAudio_ForceAmbientCategory = 1;
//static FAutoConsoleVariableRef CVar_ForceAmbientCategory(
//															  TEXT("audio.ForceAmbientCategory"),
//															  GAudio_ForceAmbientCategory,
//															  TEXT("Force the iOS AVAudioSessionCategoryAmbient category over AVAudioSessionCategorySoloAmbient")
//															  );

extern bool GShowSplashScreen;

FIOSCoreDelegates::FOnOpenURL FIOSCoreDelegates::OnOpenURL;
FIOSCoreDelegates::FOnWillResignActive FIOSCoreDelegates::OnWillResignActive;
FIOSCoreDelegates::FOnDidBecomeActive FIOSCoreDelegates::OnDidBecomeActive;
TArray<FIOSCoreDelegates::FFilterDelegateAndHandle> FIOSCoreDelegates::PushNotificationFilters;

static uint GEnabledAudioFeatures[(uint8)EAudioFeature::NumFeatures];

/*
	From: https://developer.apple.com/library/ios/documentation/UIKit/Reference/UIApplicationDelegate_Protocol/#//apple_ref/occ/intfm/UIApplicationDelegate/applicationDidEnterBackground:
	"In practice, you should return from applicationDidEnterBackground: as quickly as possible. If the method does not return before time runs out your app is terminated and purged from memory."
*/

static float GOverrideThreadWaitTime = 0.0;
static float GMaxThreadWaitTime = 2.0;	// Setting this to be 2 seconds since this wait has to be done twice (once for sending the enter background event to the game thread, and another for waiting on the suspend msg
										// I could not find a reference for this but in the past I believe the timeout was 5 seconds
FAutoConsoleVariableRef CVarThreadBlockTime(
		TEXT("ios.lifecycleblocktime"),
		GMaxThreadWaitTime,
		TEXT("How long to block main IOS thread to make sure gamethread gets time.\n"),
		ECVF_Default);


static void SignalHandler(int32 Signal, struct __siginfo* Info, void* Context)
{
	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*)FMemory::Malloc(StackTraceSize);
		StackTrace[0] = 0;
		
		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, (ucontext_t*)Context);
		UE_LOG(LogIOS, Error, TEXT("%s"), ANSI_TO_TCHAR(StackTrace));
		FMemory::Free(StackTrace);
		
		GError->HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void InstallSignalHandlers()
{
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	
	Action.sa_sigaction = SignalHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	
	sigaction(SIGQUIT, &Action, NULL);
	sigaction(SIGILL, &Action, NULL);
	sigaction(SIGEMT, &Action, NULL);
	sigaction(SIGFPE, &Action, NULL);
	sigaction(SIGBUS, &Action, NULL);
	sigaction(SIGSEGV, &Action, NULL);
	sigaction(SIGSYS, &Action, NULL);
}

void LexFromString(EAudioFeature& OutFeature, const TCHAR* String)
{
	OutFeature = EAudioFeature::NumFeatures;
	if (FCString::Stricmp(String, TEXT("Playback")) == 0)
	{
		OutFeature = EAudioFeature::Playback;
	}
	else if (FCString::Stricmp(String, TEXT("Record")) == 0)
	{
		OutFeature = EAudioFeature::Record;
	}
	else if (FCString::Stricmp(String, TEXT("DoNotMixWithOthers")) == 0)
	{
		OutFeature = EAudioFeature::DoNotMixWithOthers;
	}
	else if (FCString::Stricmp(String, TEXT("VoiceChat")) == 0)
	{
		OutFeature = EAudioFeature::VoiceChat;
	}
	else if (FCString::Stricmp(String, TEXT("UseReceiver")) == 0)
	{
		OutFeature = EAudioFeature::UseReceiver;
	}
	else if (FCString::Stricmp(String, TEXT("DisableBluetoothSpeaker")) == 0)
	{
		OutFeature = EAudioFeature::DisableBluetoothSpeaker;
	}
	else if (FCString::Stricmp(String, TEXT("BluetoothMicrophone")) == 0)
	{
		OutFeature = EAudioFeature::BluetoothMicrophone;
	}
	else if (FCString::Stricmp(String, TEXT("BackgroundAudio")) == 0)
	{
		OutFeature = EAudioFeature::BackgroundAudio;
	}
}

FString LexToString(EAudioFeature Feature)
{
	switch (Feature)
	{
	case EAudioFeature::Playback: return TEXT("Playback");
	case EAudioFeature::Record: return TEXT("Record");
	case EAudioFeature::DoNotMixWithOthers: return TEXT("DoNotMixWithOthers");
	case EAudioFeature::VoiceChat: return TEXT("VoiceChat");
	case EAudioFeature::UseReceiver: return TEXT("UseReceiver");
	case EAudioFeature::DisableBluetoothSpeaker: return TEXT("DisableBluetoothSpeaker");
	case EAudioFeature::BluetoothMicrophone: return TEXT("BluetoothMicrophone");
	case EAudioFeature::BackgroundAudio: return TEXT("BackgroundAudio");
	default: return FString();
	}
}

FDelegateHandle FIOSCoreDelegates::AddPushNotificationFilter(const FPushNotificationFilter& FilterDel)
{
	FDelegateHandle NewHandle(FDelegateHandle::EGenerateNewHandleType::GenerateNewHandle);
	PushNotificationFilters.Push({FilterDel, NewHandle});
	return NewHandle;
}

void FIOSCoreDelegates::RemovePushNotificationFilter(FDelegateHandle Handle)
{
	PushNotificationFilters.RemoveAll([Handle](const FFilterDelegateAndHandle& Entry) {
		return Entry.Handle == Handle;
	});
}

bool FIOSCoreDelegates::PassesPushNotificationFilters(NSDictionary* Payload)
{
	return Algo::AllOf(PushNotificationFilters, [Payload](const FFilterDelegateAndHandle& Entry) {
		return Entry.Filter.Execute(Payload);
	});
}

@interface IOSAppDelegate()

// move private things from header to here

@end

@implementation IOSAppDelegate

#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
    @synthesize ConsoleAlertController;
	@synthesize ConsoleHistoryValues;
	@synthesize ConsoleHistoryValuesIndex;
#endif

@synthesize AlertResponse;
@synthesize bDeviceInPortraitMode;
@synthesize bEngineInit;
@synthesize OSVersion;

@synthesize Window;
@synthesize IOSView;
@synthesize SlateController;
@synthesize timer;
@synthesize IdleTimerEnableTimer;
@synthesize IdleTimerEnablePeriod;
#if WITH_ACCESSIBILITY
@synthesize AccessibilityCacheTimer;
#endif
@synthesize savedOpenUrlParameters;
@synthesize BackgroundSessionEventCompleteDelegate;


static IOSAppDelegate* CachedDelegate = nil;

+ (IOSAppDelegate*)GetDelegate
{
#if BUILD_EMBEDDED_APP
	if (CachedDelegate == nil)
	{
		UE_LOG(LogIOS, Fatal, TEXT("Currently, a native app embedding Unreal must have the AppDelegate subclass from IOSAppDelegate."));

		// if we are embedded, but CachedDelegate is nil, then that means the delegate was not an IOSAppDelegate subclass,
		// so we need to do a switcheroo - but this is unlikely to work well
#if 0 // ALLOW_NATIVE_APP_DELEGATE
		SCOPED_BOOT_TIMING("Delegate Switcheroo");

		id<UIApplicationDelegate> ExistingDelegate = [UIApplication sharedApplication].delegate;
		
		CachedDelegate = [[IOSAppDelegate alloc] init];
		CachedDelegate.Window = ExistingDelegate.window;
		
		// @todo critical: The IOSAppDelegate needs to save the old delegate, override EVERY UIApplication function, and call the old delegate's functions for each one
		[UIApplication sharedApplication].delegate = CachedDelegate;
		
		// we will have missd the didFinishLaunchingWithOptions call, so we need to call it now, but we don't have the original launchOptions, nothing we can do now!
		[CachedDelegate application:[UIApplication sharedApplication] didFinishLaunchingWithOptions:nil];
#endif
	}
#endif

	return CachedDelegate;
}

-(id)init
{
#if UE_USE_SWIFT_UI_MAIN

	NSArray* Arguments = [[NSProcessInfo processInfo] arguments];
	Arguments = [Arguments subarrayWithRange:NSMakeRange(1, [Arguments count] - 1)];
	FString CmdLine = [Arguments componentsJoinedByString:@" "];

	FIOSCommandLineHelper::InitCommandArgs(*CmdLine);

#endif
	self = [super init];
	CachedDelegate = self;
	memset(GEnabledAudioFeatures, 0, sizeof(GEnabledAudioFeatures));
	return self;
}


-(void)dealloc
{
#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
	[ConsoleAlertController release];
	[ConsoleHistoryValues release];
#endif
	[Window release];
	[IOSView release];
	[SlateController release];
	[timer release];
#if WITH_ACCESSIBILITY
	if (AccessibilityCacheTimer != nil)
	{
		[AccessibilityCacheTimer invalidate];
		[AccessibilityCacheTimer release];
	}
#endif
	[super dealloc];
}

-(void)MainAppThread:(NSDictionary*)launchOptions
{
	// make sure this thread has an auto release pool setup
	NSAutoreleasePool* AutoreleasePool = [[NSAutoreleasePool alloc] init];

	{
		SCOPED_BOOT_TIMING("[IOSAppDelegate MainAppThread setup]");

		self.bHasStarted = true;
		GIsGuarded = false;
		GStartTime = FPlatformTime::Seconds();


		while(!self.bCommandLineReady)
		{
			usleep(100);
		}
	}

    FTaskTagScope Scope(ETaskTag::EGameThread);

	FAppEntry::Init();

	// check for update on app store if cvar is enabled
/*	dispatch_async(dispatch_get_main_queue(), ^{
		[[IOSAppDelegate GetDelegate] DoUpdateCheck];
	});*/
	
	// now that GConfig has been loaded, load the EnabledAudioFeatures from ini
	TArray<FString> EnabledAudioFeatures;
	GConfig->GetArray(TEXT("Audio"), TEXT("EnabledAudioFeatures"), EnabledAudioFeatures, GEngineIni);
	for (const FString& EnabledAudioFeature : EnabledAudioFeatures)
	{
		EAudioFeature Feature = EAudioFeature::NumFeatures;
		LexFromString(Feature, *EnabledAudioFeature);
		if (Feature != EAudioFeature::NumFeatures)
		{
			GEnabledAudioFeatures[static_cast<uint8>(Feature)] = 1;
		}
	}
	[self ToggleAudioSession:true];

#if !BUILD_EMBEDDED_APP
	[self InitIdleTimerSettings];
#endif

	bEngineInit = true;

	// put a render thread job to turn off the splash screen after the first render flip
	if (GShowSplashScreen)
	{
		FGraphEventRef SplashTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
		{
			GShowSplashScreen = false;
		}, TStatId(), NULL, ENamedThreads::ActualRenderingThread);
	}

	for (NSDictionary* openUrlParameter in self.savedOpenUrlParameters)
	{
		UIApplication* application = [openUrlParameter valueForKey : @"application"];
		NSURL* url = [openUrlParameter valueForKey : @"url"];
		NSString * sourceApplication = [openUrlParameter valueForKey : @"sourceApplication"];
		id annotation = [openUrlParameter valueForKey : @"annotation"];
		FIOSCoreDelegates::OnOpenURL.Broadcast(application, url, sourceApplication, annotation);
	}
	self.savedOpenUrlParameters = nil; // clear after saved openurl delegate running

#if BUILD_EMBEDDED_APP
	// tell the embedded app that the while 1 loop is going
	FEmbeddedCallParamsHelper Helper;
	Helper.Command = TEXT("engineisrunning");
	FEmbeddedDelegates::GetEmbeddedToNativeParamsDelegateForSubsystem(TEXT("native")).Broadcast(Helper);
#endif

#if WITH_ACCESSIBILITY
	// Initialize accessibility code if VoiceOver is enabled. This must happen after Slate has been initialized.
	dispatch_async(dispatch_get_main_queue(), ^{
		if (UIAccessibilityIsVoiceOverRunning())
		{
			[[IOSAppDelegate GetDelegate] OnVoiceOverStatusChanged];
		}
	});
#endif

    while( !IsEngineExitRequested() )
	{
        if (self.bIsSuspended)
        {
            FAppEntry::SuspendTick();
            
            self.bHasSuspended = true;
        }
        else
        {
			bool bOtherAudioPlayingNow = [self IsBackgroundAudioPlaying];
			if (bOtherAudioPlayingNow != self.bLastOtherAudioPlaying || self.bForceEmitOtherAudioPlaying)
			{
				FGraphEventRef UserMusicInterruptTask = FFunctionGraphTask::CreateAndDispatchWhenReady([bOtherAudioPlayingNow]()
				   {
					   //NSLog(@"UserMusicInterrupt Change: %s", bOtherAudioPlayingNow ? "playing" : "paused");
					   FCoreDelegates::UserMusicInterruptDelegate.Broadcast(bOtherAudioPlayingNow);
				   }, TStatId(), NULL, ENamedThreads::GameThread);
				
				self.bLastOtherAudioPlaying = bOtherAudioPlayingNow;
				self.bForceEmitOtherAudioPlaying = false;
			}
			
			int OutputVolume = [self GetAudioVolume];
			bool bMuted = false;
			
#if USE_MUTE_SWITCH_DETECTION
			SharkfoodMuteSwitchDetector* MuteDetector = [SharkfoodMuteSwitchDetector shared];
			bMuted = MuteDetector.isMute;
			if (bMuted != self.bLastMutedState || self.bForceEmitMutedState)
			{
				FGraphEventRef AudioMuteTask = FFunctionGraphTask::CreateAndDispatchWhenReady([bMuted, OutputVolume]()
					{
						//NSLog(@"Audio Session %s", bMuted ? "MUTED" : "UNMUTED");
						FCoreDelegates::AudioMuteDelegate.Broadcast(bMuted, OutputVolume);
					}, TStatId(), NULL, ENamedThreads::GameThread);
				
				self.bLastMutedState = bMuted;
				self.bForceEmitMutedState = false;
			}
#endif

			if (OutputVolume != self.LastVolume || self.bForceEmitVolume)
			{
				FGraphEventRef AudioMuteTask = FFunctionGraphTask::CreateAndDispatchWhenReady([bMuted, OutputVolume]()
					{
						//NSLog(@"Audio Volume: %d", OutputVolume);
						FCoreDelegates::AudioMuteDelegate.Broadcast(bMuted, OutputVolume);
					}, TStatId(), NULL, ENamedThreads::GameThread);

				self.LastVolume = OutputVolume;
				self.bForceEmitVolume = false;
			}

            FAppEntry::Tick();
        
            // free any autoreleased objects every once in awhile to keep memory use down (strings, splash screens, etc)
            if (((GFrameCounter) & 31) == 0)
            {
				// If you crash upon release, turn on Zombie Objects (Edit Scheme... | Diagnostics | Zombie Objects)
				// This will list the last object sent the release message, which will help identify the double free
                [AutoreleasePool release];
                AutoreleasePool = [[NSAutoreleasePool alloc] init];
            }
        }

        // drain the async task queue from the game thread
        [FIOSAsyncTask ProcessAsyncTasks];
	}

    dispatch_sync(dispatch_get_main_queue(),^
    {
        [UIApplication sharedApplication].idleTimerDisabled = NO;
    });

	[AutoreleasePool release];
	FAppEntry::Shutdown();
    
    self.bHasStarted = false;
    
    if(bForceExit || FApp::IsUnattended())
    {
        _Exit(0);
        //exit(0);  // As far as I can tell we run into a lot of trouble trying to run static destructors, so this is a no go :(
    }
}

-(void)timerForSplashScreen
{
    if (!GShowSplashScreen)
    {
        if ([self.Window viewWithTag:200] != nil)
        {
            [[self.Window viewWithTag:200] removeFromSuperview];
            [self.viewController release];
        }
        [timer invalidate];
    }
}

-(void)RecordPeakMemory
{
    FPlatformMemoryStats MemoryStats = FIOSPlatformMemory::GetStats();

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	FLowLevelMemTracker::Get().SetTagAmountForTracker(ELLMTracker::Platform, ELLMTag::PlatformVM, MemoryStats.UsedVirtual, false);
#endif
}

-(void)InitIdleTimerSettings
{
	float TimerDuration = 0.0F;
	GConfig->GetFloat(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("IdleTimerEnablePeriod"), TimerDuration, GEngineIni);
    IdleTimerEnablePeriod = TimerDuration;
	self.IdleTimerEnableTimer = nil;
	bool bEnableTimer = YES;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableIdleTimer"), bEnableTimer, GEngineIni);
	[self EnableIdleTimer : bEnableTimer];
}

-(bool)IsIdleTimerEnabled
{
	return ([UIApplication sharedApplication].idleTimerDisabled == NO);
}

-(void)DeferredEnableIdleTimer
{
	[UIApplication sharedApplication].idleTimerDisabled = NO;
	self.IdleTimerEnableTimer = nil;
}

-(void)EnableIdleTimer:(bool)bEnabled
{
	dispatch_async(dispatch_get_main_queue(),^
	{
		if (bEnabled)
		{
			// Nothing needs to be done, if the enable timer is already running.
			if (self.IdleTimerEnableTimer == nil)
			{
				self.IdleTimerEnableTimer = [NSTimer scheduledTimerWithTimeInterval:IdleTimerEnablePeriod target:self selector:@selector(DeferredEnableIdleTimer) userInfo:nil repeats:NO];
			}
		}
		else
		{
			// Ensure pending attempts to enable the idle timer are cancelled.
			if (self.IdleTimerEnableTimer != nil)
			{
				[self.IdleTimerEnableTimer invalidate];
				self.IdleTimerEnableTimer = nil;
			}

			[UIApplication sharedApplication].idleTimerDisabled = NO;
			[UIApplication sharedApplication].idleTimerDisabled = YES;
		}
	});
}

-(void)NoUrlCommandLine
{
	//Since it is non-repeating, the timer should kill itself.
	self.bCommandLineReady = true;
}

- (void)InitializeAudioSession
{
	[[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionInterruptionNotification object:nil queue:nil usingBlock:^(NSNotification *notification)
	{
		// the audio context should resume immediately after interrupt, if suspended
		FAppEntry::ResetAudioContextResumeTime();

		switch ([[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue])
		{
			case AVAudioSessionInterruptionTypeBegan:
				// Notification that our audio was stopped by the OS. No action needed.
				break;

			case AVAudioSessionInterruptionTypeEnded:
				NSNumber * interruptionOption = [[notification userInfo] objectForKey:AVAudioSessionInterruptionOptionKey];
				if (interruptionOption != nil && interruptionOption.unsignedIntegerValue > 0)
				{
                    FAppEntry::RestartAudio();
                }
                [self ToggleAudioSession:true];
				break;
		}
	}];

	[[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionRouteChangeNotification object : nil queue : nil usingBlock : ^ (NSNotification *notification)
	{
		switch ([[[notification userInfo] objectForKey:AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue])
		{
			case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
				// headphones plugged in
				FCoreDelegates::AudioRouteChangedDelegate.Broadcast(true);
				break;

			case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
				// headphones unplugged
				FCoreDelegates::AudioRouteChangedDelegate.Broadcast(false);
				break;
		}
	}];

	self.bAudioSessionInitialized = true;

	self.bUsingBackgroundMusic = [self IsBackgroundAudioPlaying];
	self.bForceEmitOtherAudioPlaying = true;

#if USE_MUTE_SWITCH_DETECTION
	// Initialize the mute switch detector.
	[SharkfoodMuteSwitchDetector shared];
	self.bForceEmitMutedState = true;
#endif
	
	self.bForceEmitVolume = true;
	
	FCoreDelegates::ApplicationRequestAudioState.AddLambda([self]()
		{
			self.bForceEmitOtherAudioPlaying = true;
#if USE_MUTE_SWITCH_DETECTION
			self.bForceEmitMutedState = true;
#endif
			self.bForceEmitVolume = true;
		});
	
	[self ToggleAudioSession:true];
}

- (void)ToggleAudioSession:(bool)bActive
{
	if(!self.bAudioSessionInitialized)
	{
		return;
	}
	
	self.bAudioActive = bActive;
	
	// get the category and settings to use
	NSString* Category = AVAudioSessionCategoryAmbient;
	if([self IsFeatureActive:EAudioFeature::DoNotMixWithOthers])
	{
		Category = AVAudioSessionCategorySoloAmbient;
	}
#if !PLATFORM_TVOS
	bool bSupportsBackgroundAudio = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsBackgroundAudio"), bSupportsBackgroundAudio, GEngineIni);
	if (bSupportsBackgroundAudio)
	{
		Category = AVAudioSessionCategoryPlayback;
	}
#endif
	
	NSString* Mode = AVAudioSessionModeDefault;
	AVAudioSessionCategoryOptions Options = 0;
	if (self.bAudioActive || [self IsBackgroundAudioPlaying] || [self IsFeatureActive:EAudioFeature::BackgroundAudio])
	{
		// if we are running unattended, do not enable record as this would bring up a mic dialog
		const bool bRecordActive = [self IsFeatureActive:EAudioFeature::Record] && !FApp::IsUnattended();
		if ([self IsFeatureActive:EAudioFeature::Playback] && bRecordActive)
		{
			Category = AVAudioSessionCategoryPlayAndRecord;

			if ([self IsFeatureActive:EAudioFeature::VoiceChat])
			{
				Mode = AVAudioSessionModeVoiceChat;
			}

			if (![self IsFeatureActive:EAudioFeature::DoNotMixWithOthers])
			{
				Options |= AVAudioSessionCategoryOptionMixWithOthers;
			}

#if !PLATFORM_TVOS
			if (![self IsFeatureActive:EAudioFeature::UseReceiver])
			{
				Options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
			}

			if ([self IsFeatureActive:EAudioFeature::BluetoothMicrophone])
			{
				Options |= AVAudioSessionCategoryOptionAllowBluetooth;
			}
#endif

			if (![self IsFeatureActive:EAudioFeature::DisableBluetoothSpeaker])
			{
				Options |= AVAudioSessionCategoryOptionAllowBluetoothA2DP;
			}
		}
		else if ([self IsFeatureActive:EAudioFeature::Playback])
		{
			Category = AVAudioSessionCategoryPlayback;
			
			if (![self IsFeatureActive:EAudioFeature::DoNotMixWithOthers])
			{
				Options |= AVAudioSessionCategoryOptionMixWithOthers;
			}
		}
		else if (bRecordActive)
		{
			Category = AVAudioSessionCategoryRecord;

#if !PLATFORM_TVOS
			if ([self IsFeatureActive:EAudioFeature::BluetoothMicrophone])
			{
				Options |= AVAudioSessionCategoryOptionAllowBluetooth;
			}
#endif
		}
	}
	else
	{
		Category = AVAudioSessionCategoryAmbient;
	}

	
	AVAudioSessionCategoryOptions TestOptions = Options;
	if ([[[AVAudioSession sharedInstance] category] compare:AVAudioSessionCategoryAmbient] == NSOrderedSame)
	{
		// This option is implicitly set, so we need to compare the existing options with it
		TestOptions |= AVAudioSessionCategoryOptionMixWithOthers;
	}
	// set the category if anything has changed
	NSError* ActiveError = nil;
	if ([[[AVAudioSession sharedInstance] category] compare:Category] != NSOrderedSame ||
		[[[AVAudioSession sharedInstance] mode] compare:Mode] != NSOrderedSame ||
		[[AVAudioSession sharedInstance] categoryOptions] != TestOptions)
	{
		UE_LOG(LogIOSAudioSession, Log, TEXT("Setting AVAudioSession category to Category:%s Mode:%s Options:%x"), *FString(Category), *FString(Mode), Options);

		[[AVAudioSession sharedInstance] setCategory:Category mode:Mode options:Options error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set AVAudioSession category to Category:%s Mode:%s Options:%x! [Error = %s]"), *FString(Category), *FString(Mode), Options, *FString([ActiveError description]));
		}
	}
	
	// set the AVAudioSession active if necessary
	if (bActive)
	{
		[[AVAudioSession sharedInstance] setActive:bActive error:&ActiveError];
		if (ActiveError)
		{
			UE_LOG(LogIOSAudioSession, Error, TEXT("Failed to set audio session to active = %d [Error = %s]"), bActive, *FString([ActiveError description]));
		}
	}
}

- (bool)IsBackgroundAudioPlaying
{
	AVAudioSession* Session = [AVAudioSession sharedInstance];
	return Session.otherAudioPlaying;
}

-(bool)HasRecordPermission
{
#if PLATFORM_TVOS || PLATFORM_VISIONOS
	// TVOS does not have sound recording capabilities.
	return false;
#elif (defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0)
    return [[AVAudioApplication sharedInstance] recordPermission] == AVAudioApplicationRecordPermissionGranted;
#else
    return [[AVAudioSession sharedInstance] recordPermission] == AVAudioSessionRecordPermissionGranted;
#endif
}

-(void)EnableHighQualityVoiceChat:(bool)bEnable
{
	[self SetFeature:EAudioFeature::VoiceChat Active:bEnable];
}

- (void)EnableVoiceChat:(bool)bEnable
{
    // mobile will prompt for microphone access
    if (FApp::IsUnattended())
	{
		return;
	}
	if (bEnable != self.bVoiceChatEnabled)
	{
		[self SetFeature:EAudioFeature::Playback Active:bEnable];
		[self SetFeature:EAudioFeature::Record Active:bEnable];
		self.bVoiceChatEnabled = bEnable;
	}
}

- (bool)IsVoiceChatEnabled
{
	return self.bVoiceChatEnabled;
}


- (void)SetFeature:(EAudioFeature)Feature Active:(bool)bIsActive
{
	if (bIsActive)
	{
		++GEnabledAudioFeatures[static_cast<uint8>(Feature)];
		if (GEnabledAudioFeatures[static_cast<uint8>(Feature)] == 1)
		{
			// Setting changed from disabled to enabled, apply the change
			[self ToggleAudioSession:self.bAudioActive];
		}
	}
	else
	{
		if (GEnabledAudioFeatures[static_cast<uint8>(Feature)] == 0)
		{
			UE_LOG(LogIOSAudioSession, Warning, TEXT("Attempted to disable audio feature that is already disabled"));
		}
		else
		{
			--GEnabledAudioFeatures[static_cast<uint8>(Feature)];
			if (GEnabledAudioFeatures[static_cast<uint8>(Feature)] == 0)
			{
				// Setting changed from enabled to disabled, apply the change
				[self ToggleAudioSession:self.bAudioActive];
			}
		}
	}
}

- (bool)IsFeatureActive:(EAudioFeature)Feature
{
	return GEnabledAudioFeatures[static_cast<uint8>(Feature)] > 0;
}


- (int)GetAudioVolume
{
	float vol = [[AVAudioSession sharedInstance] outputVolume];
	int roundedVol = (int)((vol * 100.0f) + 0.5f);
	return roundedVol;
}

- (bool)AreHeadphonesPluggedIn
{
	AVAudioSessionRouteDescription *route = [[AVAudioSession sharedInstance] currentRoute];

	bool headphonesFound = false;
	for (AVAudioSessionPortDescription *portDescription in route.outputs)
	{
		//compare to the iOS constant for headphones
		if ([portDescription.portType isEqualToString : AVAudioSessionPortHeadphones])
		{
			headphonesFound = true;
			break;
		}
	}
	return headphonesFound;
}

- (int)GetBatteryLevel
{
#if PLATFORM_TVOS
	// TVOS does not have a battery, return fully charged
	return 100;
#else
    return self.BatteryLevel;
#endif
}

- (bool)IsRunningOnBattery
{
#if PLATFORM_TVOS
	// TVOS does not have a battery, return plugged in
	return false;
#else
    return self.bBatteryState;
#endif
}

- (void)LoadScreenResolutionModifiers
{
#if PLATFORM_VISIONOS
	self.ScreenScale = 1.0f;
	self.NativeScale = 1.0f;
#else
	// cache these UI thread sensitive vars for later use
	self.ScreenScale = (float)[[UIScreen mainScreen] scale];
	self.NativeScale = (float)[[UIScreen mainScreen] nativeScale];
#endif

	// need to cache the MobileContentScaleFactor for framebuffer creation.
	static IConsoleVariable* CVarScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	check(CVarScale);
	self.MobileContentScaleFactor = CVarScale ? CVarScale->GetFloat() : 0;

	// Can also be overridden from the commandline using "mcsf="
	FString CmdLineCSF;
	if (FParse::Value(FCommandLine::Get(), TEXT("mcsf="), CmdLineCSF, false))
	{
		self.MobileContentScaleFactor = FCString::Atof(*CmdLineCSF);
	}

	static IConsoleVariable* CVarResX = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DesiredResX"));
	static IConsoleVariable* CVarResY = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DesiredResY"));
	check(CVarResX);
	check(CVarResY);
		
	self.RequestedResX = CVarResX ? CVarResX->GetInt() : 0;
	self.RequestedResY = CVarResY ? CVarResY->GetInt() : 0;
		
	static bool bOnFirstUse = true;
	if (bOnFirstUse)
	{
		FString CmdLineMDRes;
		if (FParse::Value(FCommandLine::Get(), TEXT("mobileresx="), CmdLineMDRes, false))
		{
				self.RequestedResX = FCString::Atoi(*CmdLineMDRes);
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("mobileresy="), CmdLineMDRes, false))
		{
				self.RequestedResY = FCString::Atoi(*CmdLineMDRes);
		}

		CVarScale->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FIOSWindow::OnScaleFactorChanged));
		CVarResX->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FIOSWindow::OnConsoleResolutionChanged));
		CVarResY->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FIOSWindow::OnConsoleResolutionChanged));
			
		bOnFirstUse = false;
	}
}

- (void)CheckForZoomAccessibility
{
#if !PLATFORM_TVOS
    // warn about zoom conflicting
    UIAccessibilityRegisterGestureConflictWithZoom();
#endif
}

- (float)GetBackgroundingMainThreadBlockTime
{
	return GOverrideThreadWaitTime > 0.0f ? GOverrideThreadWaitTime : GMaxThreadWaitTime;
}

-(void)OverrideBackgroundingMainThreadBlockTime:(float)BlockTime
{
	GOverrideThreadWaitTime = BlockTime;
}



- (NSProcessInfoThermalState)GetThermalState
{
    return self.ThermalState;
}

- (UIViewController*) IOSController
{
	// walk the responder chain until we get to a VC
	UIResponder *Responder = IOSView;
	while (Responder != nil && ![Responder isKindOfClass:[UIViewController class]])
	{
		Responder = [Responder nextResponder];
	}
	return (UIViewController*)Responder;
}


bool GIsSuspended = 0;
- (void)ToggleSuspend:(bool)bSuspend
{
    self.bHasSuspended = !bSuspend;
    self.bIsSuspended = bSuspend;
    GIsSuspended = self.bIsSuspended;

	if (bSuspend)
	{
		FAppEntry::Suspend();
	}
	else
	{
        FIOSPlatformRHIFramePacer::Resume();
		FAppEntry::Resume();
	}
    
	if (IOSView && IOSView->bIsInitialized)
	{
		// Don't deadlock here because a msg box may appear super early blocking the game thread and then the app may go into the background
		double	startTime = FPlatformTime::Seconds();

		// don't wait for FDefaultGameMoviePlayer::WaitForMovieToFinish(), crash with 0x8badf00d if "Wait for Movies to Complete" is checked
		FEmbeddedCommunication::KeepAwake(TEXT("Background"), false);
		while(!self.bHasSuspended && !FAppEntry::IsStartupMoviePlaying() &&  (FPlatformTime::Seconds() - startTime) < [self GetBackgroundingMainThreadBlockTime])
		{
            FIOSPlatformRHIFramePacer::Suspend();
			FPlatformProcess::Sleep(0.05f);
		}
		FEmbeddedCommunication::AllowSleep(TEXT("Background"));
	}
}

- (void)ForceExit
{
	RequestEngineExit(TEXT("IOS ForceExit"));
    bForceExit = true;
}

- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	self.bDeviceInPortraitMode = false;
	bEngineInit = false;

	return YES;
}

static int32 GEnableThermalsReport = 0;
static FAutoConsoleVariableRef CVarGEnableThermalsReport(
	TEXT("ios.EnableThermalsReport"),
	GEnableThermalsReport,
	TEXT("When set to 1, will enable on-screen thermals debug display.")
);

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	// save launch options
	self.launchOptions = launchOptions;

#if PLATFORM_TVOS
	self.bDeviceInPortraitMode = false;
#else
	// use the status bar orientation to properly determine landscape vs portrait
	self.InterfaceOrientation = [self.Window.windowScene interfaceOrientation];
	self.bDeviceInPortraitMode = UIInterfaceOrientationIsPortrait(self.InterfaceOrientation);
	printf("========= This app is in %s mode\n", self.bDeviceInPortraitMode ? "PORTRAIT" : "LANDSCAPE");
#endif

	// check OS version to make sure we have the API
	OSVersion = [[[UIDevice currentDevice] systemVersion] floatValue];
	if (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
	{
//        InstallSignalHandlers();
	}

	self.savedOpenUrlParameters = [[NSMutableArray alloc] init];
	self.PeakMemoryTimer = [NSTimer scheduledTimerWithTimeInterval:0.1f target:self selector:@selector(RecordPeakMemory) userInfo:nil repeats:YES];

#if !BUILD_EMBEDDED_APP
    
#if PLATFORM_VISIONOS
    CGRect MainFrame = CGRectMake(0, 0, 1000, 1000);
#else
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
#endif
    self.Window = [[UIWindow alloc] initWithFrame:MainFrame];

    [self.Window makeKeyAndVisible];

    FAppEntry::PreInit(self, application);

#if !PLATFORM_VISIONOS
    UIStoryboard *storyboard = [UIStoryboard storyboardWithName:@"LaunchScreen" bundle:nil];
    if (storyboard != nil)
    {
        self.viewController = [storyboard instantiateViewControllerWithIdentifier:@"LaunchScreen"];
        self.viewController.view.tag = 200;
        [self.Window addSubview: self.viewController.view];
        GShowSplashScreen = true;
    }
#endif

    timer = [NSTimer scheduledTimerWithTimeInterval: 0.05f target:self selector:@selector(timerForSplashScreen) userInfo:nil repeats:YES];

	[self StartGameThread];

	self.CommandLineParseTimer = [NSTimer scheduledTimerWithTimeInterval:0.01f target:self selector:@selector(NoUrlCommandLine) userInfo:nil repeats:NO];

#endif
	
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
	UNUserNotificationCenter *Center = [UNUserNotificationCenter currentNotificationCenter];
	Center.delegate = self;
	// Register for device orientation changes
	[[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didRotate:) name:UIDeviceOrientationDidChangeNotification object:nil];

#if !UE_BUILD_SHIPPING
	// make a history buffer
	self.ConsoleHistoryValues = [[NSMutableArray alloc] init];

	// load saved history from disk
	NSArray* SavedHistory = [[NSUserDefaults standardUserDefaults] objectForKey:@"ConsoleHistory"];
	if (SavedHistory != nil)
	{
		[self.ConsoleHistoryValues addObjectsFromArray:SavedHistory];
	}
	self.ConsoleHistoryValuesIndex = -1;

    FCoreDelegates::OnGetOnScreenMessages.AddLambda([&EnableThermalsReport = GEnableThermalsReport](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
    {
        if (EnableThermalsReport)
        {
            switch ([[NSProcessInfo processInfo] thermalState])
            {
                case NSProcessInfoThermalStateNominal:	OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(TEXT("Thermals are Nominal"))); break;
                case NSProcessInfoThermalStateFair:		OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(TEXT("Thermals are Fair"))); break;
                case NSProcessInfoThermalStateSerious:	OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(TEXT("Thermals are Serious"))); break;
                case NSProcessInfoThermalStateCritical:	OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Error, FText::FromString(TEXT("Thermals are Critical"))); break;
            }
        }
        
        // Uncomment to view the state of the AVAudioSession category, mode, and options.
        //#define VIEW_AVAUDIOSESSION_INFO
#if defined(VIEW_AVAUDIOSESSION_INFO)
        FString Message = FString::Printf(
                                          TEXT("Session Category: %s, Mode: %s, Options: %x"),
                                          UTF8_TO_TCHAR([[AVAudioSession sharedInstance].category UTF8String]),
                                          UTF8_TO_TCHAR([[AVAudioSession sharedInstance].mode UTF8String]),
                                          [AVAudioSession sharedInstance].categoryOptions);
        OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(Message));
#endif // defined(VIEW_AVAUDIOSESSION_INFO)
    });
    

#endif // UE_BUILD_SHIPPING
#endif // !TVOS

#if !PLATFORM_TVOS
    UIDevice* UiDevice = [UIDevice currentDevice];
    UiDevice.batteryMonitoringEnabled = YES;
    
    // Battery level is from 0.0 to 1.0, get it in terms of 0-100
    self.BatteryLevel = ((int)([UiDevice batteryLevel] * 100));
    UIDeviceBatteryState State = UiDevice.batteryState;
    self.bBatteryState = State == UIDeviceBatteryStateUnplugged || State == UIDeviceBatteryStateUnknown;
    self.ThermalState = [[NSProcessInfo processInfo] thermalState];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(temperatureChanged:) name:NSProcessInfoThermalStateDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(lowPowerModeChanged:) name:NSProcessInfoPowerStateDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(batteryChanged:) name:UIDeviceBatteryLevelDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(batteryStateChanged:) name:UIDeviceBatteryStateDidChangeNotification object:nil];
#endif
    
    self.bAudioSessionInitialized = false;
	
	// InitializeAudioSession is now called from FEngineLoop::AppInit after the config system is initialized
//	[self InitializeAudioSession];

#if WITH_ACCESSIBILITY
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(OnVoiceOverStatusChanged) name:UIAccessibilityVoiceOverStatusDidChangeNotification object:nil];
#endif

	return YES;
}

#if !PLATFORM_TVOS
- (UIInterfaceOrientationMask)application:(UIApplication *)application supportedInterfaceOrientationsForWindow:(UIWindow*)window
{
	bool bSupportsPortrait;
	bool bSupportsPortraitUpsideDown;
	bool bSupportsLandscapeLeft;
	bool bSupportsLandscapeRight;
	
	// This is called during app startup and IOSRuntimeSettings may not have been loaded yet
	bool hasValue = GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsPortraitOrientation"), bSupportsPortrait, GEngineIni);
	
	NSArray<NSString*> *SupportedOrientations = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UISupportedInterfaceOrientations"];
	if (!hasValue && SupportedOrientations != NULL)
	{
		// Loop through the Info.plist UISupportedInterfaceOrientations array values looking for "Portrait", "Left" and "Right"
		NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF == %@", @"UIInterfaceOrientationPortrait"];
		bSupportsPortrait = ([SupportedOrientations filteredArrayUsingPredicate:predicate].count > 0);
		
		NSPredicate *predicateDown = [NSPredicate predicateWithFormat:@"SELF == %@", @"UIInterfaceOrientationPortraitUpsideDown"];
		bSupportsPortraitUpsideDown = ([SupportedOrientations filteredArrayUsingPredicate:predicateDown].count > 0);
		
		NSPredicate *PredicateLeft = [NSPredicate predicateWithFormat:@"SELF == %@", @"UIInterfaceOrientationLandscapeLeft"];
		bSupportsLandscapeLeft = ([SupportedOrientations filteredArrayUsingPredicate:PredicateLeft].count > 0);
		
		NSPredicate *PredicateRight = [NSPredicate predicateWithFormat:@"SELF == %@", @"UIInterfaceOrientationLandscapeRight"];
		bSupportsLandscapeRight = ([SupportedOrientations filteredArrayUsingPredicate:PredicateRight].count > 0);
	}
	else
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsUpsideDownOrientation"), bSupportsPortraitUpsideDown, GEngineIni);
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsLandscapeLeftOrientation"), bSupportsLandscapeLeft, GEngineIni);
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsLandscapeRightOrientation"), bSupportsLandscapeRight, GEngineIni);
	}
	
	UIInterfaceOrientationMask Mask = 0;
	if (bSupportsPortrait)
	{
		Mask |= UIInterfaceOrientationMaskPortrait;
	}
	if (bSupportsPortraitUpsideDown)
	{
		Mask |= UIInterfaceOrientationMaskPortraitUpsideDown;
	}
	if (bSupportsLandscapeLeft)
	{
		Mask |= UIInterfaceOrientationMaskLandscapeLeft;
	}
	if (bSupportsLandscapeRight)
	{
		Mask |= UIInterfaceOrientationMaskLandscapeRight;
	}
	
	// If no orientation constraints are set, default to MaskAll
	return (Mask==0)?UIInterfaceOrientationMaskAll:Mask;
}
#endif

#if WITH_ACCESSIBILITY
-(void)OnVoiceOverStatusChanged
{
	if (UIAccessibilityIsVoiceOverRunning() && self.IOSApplication->GetAccessibleMessageHandler()->ApplicationIsAccessible())
	{
		// This must happen asynchronously because when the app activates from a suspended state,
		// the IOS notification will emit before the game thread wakes up. This does mean that the
		// accessibility element tree will probably not be 100% completed when the application
		// opens for the first time. If this is a problem we can add separate branches for startup
		// vs waking up.
		FFunctionGraphTask::CreateAndDispatchWhenReady([]()
		{
			FIOSApplication* Application = [IOSAppDelegate GetDelegate].IOSApplication;
			Application->GetAccessibleMessageHandler()->SetActive(true);
			AccessibleWidgetId WindowId = Application->GetAccessibleMessageHandler()->GetAccessibleWindowId(Application->FindWindowByAppDelegateView());
			dispatch_async(dispatch_get_main_queue(), ^{
                IOSAppDelegate* Delegate = [IOSAppDelegate GetDelegate];
				[Delegate.IOSView SetAccessibilityWindow:WindowId];
				if (Delegate.AccessibilityCacheTimer == nil)
				{
					// Start caching accessibility data so that it can be returned instantly to IOS. If not cached, the data takes too long
					// to retrieve due to cross-thread waiting and IOS will timeout.
					Delegate.AccessibilityCacheTimer = [NSTimer scheduledTimerWithTimeInterval:0.25f target:[FIOSAccessibilityCache AccessibilityElementCache] selector:@selector(UpdateAllCachedProperties) userInfo:nil repeats:YES];
				}
			});
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
	else if (AccessibilityCacheTimer != nil)
	{
		[AccessibilityCacheTimer invalidate];
		AccessibilityCacheTimer = nil;
		[[IOSAppDelegate GetDelegate].IOSView SetAccessibilityWindow : IAccessibleWidget::InvalidAccessibleWidgetId];
		[[FIOSAccessibilityCache AccessibilityElementCache] Clear];
		FFunctionGraphTask::CreateAndDispatchWhenReady([]()
		{
			[IOSAppDelegate GetDelegate].IOSApplication->GetAccessibleMessageHandler()->SetActive(false);
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
}
#endif

// checking for update on the app store
-(void)DoUpdateCheck
{
	static bool bInit = false;
	static NSString* CurrentVersion = [[NSBundle mainBundle] infoDictionary][@"CFBundleShortVersionString"];
	static NSString* BundleID = [[NSBundle mainBundle] infoDictionary][@"CFBundleIdentifier"];
	bool bEnableUpdateCheck = NO;
	if (!bInit)
	{
		bool bReadData = GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableUpdateCheck"), bEnableUpdateCheck, GEngineIni);
		self.bUpdateAvailable = false;
		bInit = bReadData;
	}
	if (bEnableUpdateCheck && bInit)
	{
		// kick off a check on the app store for an update
		NSLocale* Locale = [NSLocale autoupdatingCurrentLocale];
		NSURL* StoreURL = [NSURL URLWithString: [NSString stringWithFormat: @"http://itunes.apple.com/%@/lookup?bundleId=%@", Locale.countryCode, BundleID]];
		
		// kick off an NSURLSession to read the data
		NSURLSession* Session = [NSURLSession sharedSession];
		NSURLSessionDataTask* SessionTask = [Session dataTaskWithRequest: [NSURLRequest requestWithURL: StoreURL cachePolicy: NSURLRequestReloadIgnoringLocalCacheData timeoutInterval: Session.configuration.timeoutIntervalForRequest] completionHandler:^(NSData* _Nullable data, NSURLResponse* _Nullable response, NSError* _Nullable error) {
			
			if (error == nil && data != nil)
			{
				NSDictionary* StoreDictionary = [NSJSONSerialization JSONObjectWithData: data options: 0 error: nil];
			
				if ([StoreDictionary[@"resultCount"] integerValue] == 1)
				{
					// get the store version
					NSString* StoreVersion = StoreDictionary[@"results"][0][@"version"];
					if ([StoreVersion compare: CurrentVersion options: NSNumericSearch] == NSOrderedDescending)
					{
						self.bUpdateAvailable = true;
					}
					else
					{
						self.bUpdateAvailable = false;
					}
				}
				else
				{
					self.bUpdateAvailable = false;
				}
			}
			else
			{
				self.bUpdateAvailable = false;
			}
		}];
		
		[SessionTask resume];
	}
}

-(bool)IsUpdateAvailable
{
	dispatch_async(dispatch_get_main_queue(), ^{
		[[IOSAppDelegate GetDelegate] DoUpdateCheck];
	});
	return self.bUpdateAvailable;
}

- (void) StartGameThread
{
	// create a new thread (the pointer will be retained forever)
	NSThread* GameThread = [[NSThread alloc] initWithTarget:self selector:@selector(MainAppThread:) object:self.launchOptions];
	[GameThread setStackSize:GAME_THREAD_STACK_SIZE];
	[GameThread start];

	// this can be slow (1/3 of a second!), so don't make the game thread stall loading for it
	// check to see if we are using the network file system, if so, disable the idle timer
	FString HostIP;
//	if (FParse::Value(FCommandLine::Get(), TEXT("-FileHostIP="), HostIP))
	{
		[UIApplication sharedApplication].idleTimerDisabled = YES;
	}
}

+(bool)WaitAndRunOnGameThread:(TUniqueFunction<void()>)Function
{
	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(Function), TStatId(), NULL, ENamedThreads::GameThread);

	const double MaxThreadWaitTime = 2.0;
	const double StartTime = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - StartTime) < MaxThreadWaitTime)
	{
		FPlatformProcess::Sleep(0.05f);
		if (Task->IsComplete())
		{
			return true;
		}
	}
	return false;
}

- (void) didRotate:(NSNotification *)notification
{   
#if !PLATFORM_TVOS &&!PLATFORM_VISIONOS
	// get the interface orientation
	
	NSLog(@"didRotate orientation = %d", (int)[self.Window.windowScene interfaceOrientation]);
	
    UIInterfaceOrientation Orientation = [self.Window.windowScene interfaceOrientation];
	self.InterfaceOrientation = Orientation;
	
    if (bEngineInit)
    {
		FFunctionGraphTask::CreateAndDispatchWhenReady([Orientation]()
		{
			FIOSApplication* Application = [IOSAppDelegate GetDelegate].IOSApplication;
			Application->OrientationChanged(Orientation);
			FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.Broadcast((int32)[IOSAppDelegate ConvertFromUIInterfaceOrientation:Orientation]);

			//we also want to fire off the safe frame event
			FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
#endif
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
#if !NO_LOGGING
	NSLog(@"%s", "IOSAppDelegate openURL\n");
#endif

	NSString* EncdodedURLString = [url absoluteString];
	NSString* URLString = [EncdodedURLString stringByRemovingPercentEncoding];
	FString CommandLineParameters(URLString);

	// Strip the "URL" part of the URL before treating this like args. It comes in looking like so:
	// "MyGame://arg1 arg2 arg3 ..."
	// So, we're going to make it look like:
	// "arg1 arg2 arg3 ..."
	int32 URLTerminator = CommandLineParameters.Find( TEXT("://"), ESearchCase::CaseSensitive);
	if ( URLTerminator > -1 )
	{
		CommandLineParameters.RightChopInline(URLTerminator + 3, false);
	}

	FIOSCommandLineHelper::InitCommandArgs(CommandLineParameters);
	self.bCommandLineReady = true;
	[self.CommandLineParseTimer invalidate];
	self.CommandLineParseTimer = nil;
	
	//    Save openurl infomation before engine initialize.
	//    When engine is done ready, running like previous. ( if OnOpenUrl is bound on game source. )
	if (bEngineInit)
	{
		FIOSCoreDelegates::OnOpenURL.Broadcast(application, url, sourceApplication, annotation);
	}
	else
	{
#if !NO_LOGGING
		NSLog(@"%s", "Before Engine Init receive IOSAppDelegate openURL\n");
#endif
			NSDictionary* openUrlParameter = [NSDictionary dictionaryWithObjectsAndKeys :
		application, @"application",
			url, @"url",
			sourceApplication, @"sourceApplication",
			annotation, @"annotation",
			nil];

		[savedOpenUrlParameters addObject : openUrlParameter];
	}

	return YES;
}

FCriticalSection RenderSuspend;
- (void)applicationWillResignActive:(UIApplication *)application
{
    /*
		Sent when the application is about to move from active to inactive
		state. This can occur for certain types of temporary interruptions (such
		as an incoming phone call or SMS message) or when the user quits the
		application and it begins the transition to the background state.

		Use this method to pause ongoing tasks, disable timers, and throttle
	 	down OpenGL ES frame rates. Games should use this method to pause the
		game.
	 */
    if (bEngineInit)
    {
 		FEmbeddedCommunication::KeepAwake(TEXT("Background"), false);
        FGraphEventRef ResignTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
        {
            FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
            FEmbeddedCommunication::AllowSleep(TEXT("Background"));
        }, TStatId(), NULL, ENamedThreads::GameThread);
		
		// Do not wait forever for this task to complete since the game thread may be stuck on waiting for user input from a modal dialog box
		double	startTime = FPlatformTime::Seconds();
		while((FPlatformTime::Seconds() - startTime) < [self GetBackgroundingMainThreadBlockTime])
		{
			FPlatformProcess::Sleep(0.05f);
			if(ResignTask->IsComplete())
			{
				UE_LOG(LogTemp, Display, TEXT("Task was completed before time."));
				break;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("Done with entering background tasks time."));
    }
    
// fix for freeze on tvOS, moving to applicationDidEnterBackground. Not making the changes for iOS platforms as the bug does not happen and could bring some side effets.
#if !PLATFORM_TVOS
	bool bSupportsBackgroundAudio = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsBackgroundAudio"), bSupportsBackgroundAudio, GEngineIni);
    if (!bSupportsBackgroundAudio)
    {
        [self ToggleSuspend:true];
        [self ToggleAudioSession:false];
    }
#else
    [self ToggleAudioSession:false];
#endif
    
    RenderSuspend.TryLock();
    if (FTaskGraphInterface::IsRunning())
    {
        if (bEngineInit)
        {
            FGraphEventRef ResignTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
            {
                FScopeLock ScopeLock(&RenderSuspend);
            }, TStatId(), NULL, ENamedThreads::GameThread);
        }
        else
        {
            FGraphEventRef ResignTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
            {
                FScopeLock ScopeLock(&RenderSuspend);
            }, TStatId(), NULL, ENamedThreads::ActualRenderingThread);
        }
    }
	
	FIOSCoreDelegates::OnWillResignActive.Broadcast();
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
	/*
	 Use this method to release shared resources, save user data, invalidate
	 timers, and store enough application state information to restore your
	 application to its current state in case it is terminated later.
	 
	 If your application supports background execution, this method is called
	 instead of applicationWillTerminate: when the user quits.
	 */

    // fix for freeze on tvOS, moving to applicationDidEnterBackground. Not making the changes for iOS platforms as the bug does not happen and could bring some side effets.
#if PLATFORM_TVOS
    [self ToggleSuspend:true];
#endif

    self.bAudioActive = false;
    FAppEntry::Suspend(true);

	FEmbeddedCommunication::KeepAwake(TEXT("Background"), false);

	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// the audio context should resume immediately after interrupt, if suspended
		FAppEntry::ResetAudioContextResumeTime();

		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	
		FEmbeddedCommunication::AllowSleep(TEXT("Background"));
		return true;
	}];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
	FEmbeddedCommunication::KeepAwake(TEXT("Background"), false);
	/*
	 Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
	 */
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		// the audio context should resume immediately after interrupt, if suspended
		FAppEntry::ResetAudioContextResumeTime();

		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();

		FEmbeddedCommunication::AllowSleep(TEXT("Background"));
		return true;
	}];
}

extern double GCStartTime;
- (void)applicationDidBecomeActive:(UIApplication *)application
{
	FIOSCoreDelegates::OnDidBecomeActive.Broadcast();

	// make sure a GC will not timeout because it was started before entering background
    GCStartTime = FPlatformTime::Seconds();
	/*
	 Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
	 */
	RenderSuspend.Unlock();
	[self ToggleSuspend : false];
    [self ToggleAudioSession:true];
	FAppEntry::RestartAudio();
    FAppEntry::Resume(true);


    if (bEngineInit)
    {
		FEmbeddedCommunication::KeepAwake(TEXT("Background"), false);

       FGraphEventRef ResignTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]()
       {
		   
            FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
		
			FEmbeddedCommunication::AllowSleep(TEXT("Background"));
        }, TStatId(), NULL, ENamedThreads::GameThread);

		// Do not wait forever for this task to complete since the game thread may be stuck on waiting for user input from a modal dialog box
		double	startTime = FPlatformTime::Seconds();
 		while((FPlatformTime::Seconds() - startTime) < [self GetBackgroundingMainThreadBlockTime])
		{
			FPlatformProcess::Sleep(0.05f);
			if(ResignTask->IsComplete())
			{
				break;
			}
		}
    }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
	/*
	 Called when the application is about to terminate.
	 Save data if appropriate.
	 See also applicationDidEnterBackground:.
	 */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	FCoreDelegates::GetApplicationWillTerminateDelegate().Broadcast();
    
    // note that we are shutting down
    // TODO: fix the reason why we are hanging when asked to shutdown
/*    RequestEngineExit(TEXT("IOS applicationWillTerminate"));
    
    if (!bEngineInit)*/
    {
        // we haven't yet made it to the point where the engine is initialized, so just exit the app
        _Exit(0);
    }
/*    else
    {
        // wait for the game thread to shut down
        while (self.bHasStarted == true)
        {
            usleep(3);
        }
    }*/
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application
{
	/*
	Tells the delegate when the application receives a memory warning from the system
	*/
	FPlatformMisc::HandleLowMemoryWarning();
}

#if !PLATFORM_TVOS && BACKGROUNDFETCH_ENABLED // NOTE: TVOS can do this starting in tvOS 11
- (void)application:(UIApplication *)application performFetchWithCompletionHandler:(void(^)(UIBackgroundFetchResult result))completionHandler
{
    // NOTE: the completionHandler must be called within 30 seconds
    FCoreDelegates::ApplicationPerformFetchDelegate.Broadcast();
    completionHandler(UIBackgroundFetchResultNewData);
}

#endif

#if !PLATFORM_TVOS
- (void)application:(UIApplication *)application handleEventsForBackgroundURLSession:(NSString *)identifier completionHandler:(void(^)(void))completionHandler
{
    //Save off completionHandler so that a future call to FCoreDelegates::ApplicationBackgroundSessionEventsAllSentDelegate can execute it
    self.BackgroundSessionEventCompleteDelegate = completionHandler;
    
    //Create background session with this identifier if needed to handle these events
	FString Id(identifier);
	FBackgroundURLSessionHandler::InitBackgroundSession(Id);

	FCoreDelegates::ApplicationBackgroundSessionEventDelegate.Broadcast(Id);
}
#endif

#if !PLATFORM_TVOS && NOTIFICATIONS_ENABLED

- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
	if (FApp::IsUnattended())
	{
		return;
	}

	TArray<uint8> Token;
	Token.AddUninitialized([deviceToken length]);
	memcpy(Token.GetData(), [deviceToken bytes], [deviceToken length]);

	const char *data = (const char*)([deviceToken bytes]);
	NSMutableString *token = [NSMutableString string];

	for (NSUInteger i = 0; i < [deviceToken length]; i++) {
		[token appendFormat : @"%02.2hhX", data[i]];
	}

	UE_LOG(LogTemp, Display, TEXT("Device Token: %s"), *FString(token));

    FFunctionGraphTask::CreateAndDispatchWhenReady([Token]()
    {
		FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.Broadcast(Token);
    }, TStatId(), NULL, ENamedThreads::GameThread);
}

-(void)application:(UIApplication *)application didFailToRegisterForRemoteNotificationsWithError:(NSError *)error
{
	FString errorDescription([error description]);
	
    FFunctionGraphTask::CreateAndDispatchWhenReady([errorDescription]()
    {
		FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate.Broadcast(errorDescription);
    }, TStatId(), NULL, ENamedThreads::GameThread);
}

#endif

#if !PLATFORM_TVOS && !PLATFORM_VISIONOS

+(EDeviceScreenOrientation) ConvertFromUIInterfaceOrientation:(UIInterfaceOrientation)Orientation
{
	switch(Orientation)
	{
		default:
		case UIInterfaceOrientationUnknown : return EDeviceScreenOrientation::Unknown; break;
		case UIInterfaceOrientationPortrait : return EDeviceScreenOrientation::Portrait; break;
		case UIInterfaceOrientationPortraitUpsideDown : return EDeviceScreenOrientation::PortraitUpsideDown; break;
		case UIInterfaceOrientationLandscapeLeft : return EDeviceScreenOrientation::LandscapeLeft; break;
		case UIInterfaceOrientationLandscapeRight : return EDeviceScreenOrientation::LandscapeRight; break;
	}
}

void HandleReceivedNotification(UNNotification* notification)
{
	bool IsLocal = false;
	
	if ([IOSAppDelegate GetDelegate].bEngineInit)
	{
		NSString* NotificationType = (NSString*)[notification.request.content.userInfo objectForKey: @"NotificationType"];
		if(NotificationType != nullptr)
		{
			FString LocalOrRemote(NotificationType);
			if(LocalOrRemote == FString(TEXT("Local")))
			{
				IsLocal = true;
			}
		}
		
		int AppState;
		if ([UIApplication sharedApplication].applicationState == UIApplicationStateInactive)
		{
			AppState = 1; // EApplicationState::Inactive;
		}
		else if ([UIApplication sharedApplication].applicationState == UIApplicationStateBackground)
		{
			AppState = 2; // EApplicationState::Background;
		}
		else
		{
			AppState = 3; // EApplicationState::Active;
		}
		
		if(IsLocal)
		{
			NSString*	activationEvent = (NSString*)[notification.request.content.userInfo objectForKey: @"ActivationEvent"];
			if(activationEvent != nullptr)
			{
				FString	activationEventFString(activationEvent);
				int32	fireDate = FMath::TruncToInt([notification.date timeIntervalSince1970]);
				
				FFunctionGraphTask::CreateAndDispatchWhenReady([activationEventFString, fireDate, AppState]()
															   {
																   FCoreDelegates::ApplicationReceivedLocalNotificationDelegate.Broadcast(activationEventFString, fireDate, AppState);
															   }, TStatId(), NULL, ENamedThreads::GameThread);
			}
		}
		else
		{
			NSString* JsonString = @"{}";
			NSError* JsonError;
			NSData* JsonData = [NSJSONSerialization dataWithJSONObject : notification.request.content.userInfo
															   options : 0
																 error : &JsonError];
			
			if (JsonData)
			{
				JsonString = [[[NSString alloc] initWithData:JsonData encoding : NSUTF8StringEncoding] autorelease];
			}
			
			FString	jsonFString(JsonString);
			
			FFunctionGraphTask::CreateAndDispatchWhenReady([jsonFString, AppState]()
														   {
															   FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate.Broadcast(jsonFString, AppState);
														   }, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
	   willPresentNotification:(UNNotification *)notification
		 withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
	// Received notification while app is in the foreground
	HandleReceivedNotification(notification);
	
	completionHandler(UNNotificationPresentationOptionNone);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
		 withCompletionHandler:(void (^)())completionHandler
{
	// Received notification while app is in the background or closed
	
	// Save launch local notification so the app can check for it when it is ready
	NSDictionary* userInfo = response.notification.request.content.userInfo;
	if(userInfo != nullptr)
	{
		NSString*	activationEvent = (NSString*)[userInfo objectForKey: @"ActivationEvent"];
		
		if(activationEvent != nullptr)
		{
			FAppEntry::gAppLaunchedWithLocalNotification = true;
			FAppEntry::gLaunchLocalNotificationActivationEvent = FString(activationEvent);
			FAppEntry::gLaunchLocalNotificationFireDate = FMath::TruncToInt([response.notification.date timeIntervalSince1970]);
		}
	}
	
	HandleReceivedNotification(response.notification);
	
	completionHandler();
}

#endif

/**
 * Shows the given Game Center supplied controller on the screen
 *
 * @param Controller The Controller object to animate onto the screen
 */
-(void)ShowController:(UIViewController*)Controller
{
	// slide it onto the screen
	[[IOSAppDelegate GetDelegate].IOSController presentViewController : Controller animated : YES completion : nil];
	
	// stop drawing the 3D world for faster UI speed
	//FViewport::SetGameRenderingEnabled(false);
}

/**
 * Hides the given Game Center supplied controller from the screen, optionally controller
 * animation (sliding off)
 *
 * @param Controller The Controller object to animate off the screen
 * @param bShouldAnimate YES to slide down, NO to hide immediately
 */
-(void)HideController:(UIViewController*)Controller Animated : (BOOL)bShouldAnimate
{
	// slide it off
	[Controller dismissViewControllerAnimated : bShouldAnimate completion : nil];

	// stop drawing the 3D world for faster UI speed
	//FViewport::SetGameRenderingEnabled(true);
}

/**
 * Hides the given Game Center supplied controller from the screen
 *
 * @param Controller The Controller object to animate off the screen
 */
-(void)HideController:(UIViewController*)Controller
{
	// call the other version with default animation of YES
	[self HideController : Controller Animated : YES];
}

-(void)gameCenterViewControllerDidFinish:(GKGameCenterViewController*)GameCenterDisplay
{
	// close the view 
	[self HideController : GameCenterDisplay];
}

/**
 * Show the leaderboard interface (call from iOS main thread)
 */
-(void)ShowLeaderboard:(NSString*)Category
{
	// create the leaderboard display object 
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] init] autorelease];
    [GameCenterDisplay initWithState:GKGameCenterViewControllerStateLeaderboards];
	GameCenterDisplay.gameCenterDelegate = self;

	// show it 
	[self ShowController : GameCenterDisplay];
}

/**
 * Show the achievements interface (call from iOS main thread)
 */
-(void)ShowAchievements
{
#if !PLATFORM_TVOS
	// create the leaderboard display object 
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] init] autorelease];
    [GameCenterDisplay initWithState : GKGameCenterViewControllerStateAchievements];
	GameCenterDisplay.gameCenterDelegate = self;

	// show it 
	[self ShowController : GameCenterDisplay];
#endif // !PLATFORM_TVOS
}

/**
 * Show the dashboard interface (call from iOS main thread)
 */
-(void)ShowDashboard
{
	// create the dashboard display object
	GKGameCenterViewController* GameCenterDisplay = [[[GKGameCenterViewController alloc] initWithState:GKGameCenterViewControllerStateDashboard] autorelease];
	GameCenterDisplay.gameCenterDelegate = self;

	// show it
		[self ShowController : GameCenterDisplay];
}

/**
 * Show the leaderboard interface (call from game thread)
 */
CORE_API bool IOSShowLeaderboardUI(const FString& CategoryName)
{
	// route the function to iOS thread, passing the category string along as the object
	NSString* CategoryToShow = [NSString stringWithFString : CategoryName];
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowLeaderboard : ) withObject:CategoryToShow waitUntilDone : NO];

	return true;
}

/**
* Show the achievements interface (call from game thread)
*/
CORE_API bool IOSShowAchievementsUI()
{

	// route the function to iOS thread
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowAchievements) withObject:nil waitUntilDone : NO];

	return true;
}

/**
 * Show the dashboard interface (call from game thread)
 */
CORE_API bool IOSShowDashboardUI()
{
	// route the function to iOS thread
	[[IOSAppDelegate GetDelegate] performSelectorOnMainThread:@selector(ShowDashboard) withObject:nil waitUntilDone : NO];

	return true;
}

-(void)batteryChanged:(NSNotification*)notification
{
#if !PLATFORM_TVOS
    UIDevice* Device = [UIDevice currentDevice];
    
    // Battery level is from 0.0 to 1.0, get it in terms of 0-100
    self.BatteryLevel = ((int)([Device batteryLevel] * 100));
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		UE_LOG(LogIOS, Display, TEXT("Battery Level Changed: %d"), self.BatteryLevel);
		return true;
	}];
#endif
}

-(void)batteryStateChanged:(NSNotification*)notification
{
#if !PLATFORM_TVOS
    UIDevice* Device = [UIDevice currentDevice];
    UIDeviceBatteryState State = Device.batteryState;
    self.bBatteryState = State == UIDeviceBatteryStateUnplugged || State == UIDeviceBatteryStateUnknown;
	[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		UE_LOG(LogIOS, Display, TEXT("Battery State Changed: %d"), self.bBatteryState);
		return true;
	}];
#endif
}

-(void)temperatureChanged:(NSNotification *)notification
{
#if !PLATFORM_TVOS
    // send game callback with new temperature severity
    FCoreDelegates::ETemperatureSeverity Severity;
    FString Level = TEXT("Unknown");
    self.ThermalState = [[NSProcessInfo processInfo] thermalState];
    switch (self.ThermalState)
    {
        case NSProcessInfoThermalStateNominal:	Severity = FCoreDelegates::ETemperatureSeverity::Good; Level = TEXT("Good"); break;
        case NSProcessInfoThermalStateFair:		Severity = FCoreDelegates::ETemperatureSeverity::Bad; Level = TEXT("Bad"); break;
        case NSProcessInfoThermalStateSerious:	Severity = FCoreDelegates::ETemperatureSeverity::Serious; Level = TEXT("Serious"); break;
        case NSProcessInfoThermalStateCritical:	Severity = FCoreDelegates::ETemperatureSeverity::Critical; Level = TEXT("Critical"); break;
    }
    
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
     {
        UE_LOG(LogIOS, Display, TEXT("Temperature Changed: %s"), *Level);
        FCoreDelegates::OnTemperatureChange.Broadcast(Severity);
        return true;
    }];
#endif
}

-(void)lowPowerModeChanged:(NSNotification *)notification
{
#if !PLATFORM_TVOS
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
     {
        bool bInLowPowerMode = [[NSProcessInfo processInfo] isLowPowerModeEnabled];
        UE_LOG(LogIOS, Display, TEXT("Low Power Mode Changed: %d"), bInLowPowerMode);
        FCoreDelegates::OnLowPowerMode.Broadcast(bInLowPowerMode);
        return true;
    }];
#endif
}

-(UIWindow*)window
{
	return Window;
}

@end
