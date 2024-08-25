// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformMisc.mm: iOS implementations of misc functions
=============================================================================*/

#include "IOS/IOSPlatformMisc.h"
#include "Async/TaskGraphInterfaces.h"
#include "Apple/ApplePlatformCrashContext.h"
#include "HAL/ExceptionHandling.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Regex.h"
#include "IOSChunkInstaller.h"
#include "IOS/IOSMallocZone.h"
#include "IOS/IOSApplication.h"
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSPlatformCrashContext.h"
#include "IOS/IOSPlatformPLCrashReporterIncludes.h"
#include "IOS/IOSView.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Templates/Function.h"

#include "Apple/PreAppleSystemHeaders.h"

#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
#include <AdSupport/ASIdentifierManager.h>
#endif // !PLATFORM_TVOS

#import <DeviceCheck/DeviceCheck.h>
#import <Foundation/Foundation.h>
//#include <libproc.h>
#import <mach-o/dyld.h>
#include <netinet/in.h>
#include <SystemConfiguration/SystemConfiguration.h>
#import <StoreKit/StoreKit.h>
#if !PLATFORM_VISIONOS
#import <UserNotifications/UserNotifications.h>
#endif

#include "Apple/PostAppleSystemHeaders.h"

#if !defined ENABLE_ADVERTISING_IDENTIFIER
	#define ENABLE_ADVERTISING_IDENTIFIER 0
#endif

/** Amount of free memory in MB reported by the system at startup */
CORE_API int32 GStartupFreeMemoryMB;

extern CORE_API bool GIsGPUCrashed;

/** Global pointer to memory warning handler */
void (* GMemoryWarningHandler)(const FGenericMemoryWarningContext& Context) = NULL;

/** global for showing the splash screen */
bool GShowSplashScreen = true;

static int32 GetFreeMemoryMB()
{
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	return MemoryStats.AvailablePhysical / 1024 / 1024;
}

void FIOSPlatformMisc::PlatformInit()
{
	// PlatformInit() starts the UI thread which creates the framebuffer and it requires
	// "r.MobileContentScaleFactor" and "r.Mobile.DesiredResX/Y" to be available before 
	// it's creation, so need to cache those value now.
	[[IOSAppDelegate GetDelegate] LoadScreenResolutionModifiers];
		
	FAppEntry::PlatformInit();

	// Increase the maximum number of simultaneously open files
	struct rlimit Limit;
	Limit.rlim_cur = OPEN_MAX;
	Limit.rlim_max = RLIM_INFINITY;
	int32 Result = setrlimit(RLIMIT_NOFILE, &Limit);
	check(Result == 0);

	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName() );
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName() );

	
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("CPU Page size=%i, Cores=%i"), MemoryConstants.PageSize, FPlatformMisc::NumberOfCores() );

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle() );
	GStartupFreeMemoryMB = GetFreeMemoryMB();
	UE_LOG(LogInit, Log, TEXT("Free Memory at startup: %d MB"), GStartupFreeMemoryMB);

	// create the Documents/<GameName>/Content directory so we can exclude it from iCloud backup
	FString ResultStr = FPaths::ProjectContentDir();
	ResultStr.ReplaceInline(TEXT("../"), TEXT(""));
	ResultStr.ReplaceInline(TEXT(".."), TEXT(""));
	ResultStr.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
#if FILESHARING_ENABLED
	FString DownloadPath = FString([NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
#else
	FString DownloadPath = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
#endif
	ResultStr = DownloadPath + ResultStr;
	NSURL* URL = [NSURL fileURLWithPath : ResultStr.GetNSString()];
	if (![[NSFileManager defaultManager] fileExistsAtPath:[URL path]])
	{
		[[NSFileManager defaultManager] createDirectoryAtURL:URL withIntermediateDirectories : YES attributes : nil error : nil];
	}

	// mark it to not be uploaded
	NSError *error = nil;
	BOOL success = [URL setResourceValue : [NSNumber numberWithBool : YES] forKey : NSURLIsExcludedFromBackupKey error : &error];
	if (!success)
	{
		NSLog(@"Error excluding %@ from backup %@",[URL lastPathComponent], error);
	}

	// create the Documents/Engine/Content directory so we can exclude it from iCloud backup
	ResultStr = FPaths::EngineContentDir();
	ResultStr.ReplaceInline(TEXT("../"), TEXT(""));
	ResultStr.ReplaceInline(TEXT(".."), TEXT(""));
	ResultStr.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
	ResultStr = DownloadPath + ResultStr;
	URL = [NSURL fileURLWithPath : ResultStr.GetNSString()];
	if (![[NSFileManager defaultManager] fileExistsAtPath:[URL path]])
	{
		[[NSFileManager defaultManager] createDirectoryAtURL:URL withIntermediateDirectories : YES attributes : nil error : nil];
	}

	// mark it to not be uploaded
	success = [URL setResourceValue : [NSNumber numberWithBool : YES] forKey : NSURLIsExcludedFromBackupKey error : &error];
	if (!success)
	{
		NSLog(@"Error excluding %@ from backup %@",[URL lastPathComponent], error);
	}
}

// Defines the PlatformFeatures module name for iOS, used by PlatformFeatures.h.
const TCHAR* FIOSPlatformMisc::GetPlatformFeaturesModuleName()
{
	return TEXT("IOSPlatformFeatures");
}

void FIOSPlatformMisc::PlatformHandleSplashScreen(bool ShowSplashScreen)
{
    if (GShowSplashScreen != ShowSplashScreen)
    {
        // put a render thread job to turn off the splash screen after the first render flip
        FGraphEventRef SplashTask = FFunctionGraphTask::CreateAndDispatchWhenReady([ShowSplashScreen]()
        {
            GShowSplashScreen = ShowSplashScreen;
        }, TStatId(), NULL, ENamedThreads::ActualRenderingThread);
    }
}

const TCHAR* FIOSPlatformMisc::GamePersistentDownloadDir()
{
    static FString GamePersistentDownloadDir = TEXT("");
    
    if (GamePersistentDownloadDir.Len() == 0)
    {
        FString BaseProjectDir = ProjectDir();
        
        if (BaseProjectDir.Len() > 0)
        {
            GamePersistentDownloadDir = BaseProjectDir / TEXT("PersistentDownloadDir");
        }
        
        // create the directory so we can exclude it from iCloud backup
        FString Result = GamePersistentDownloadDir;
        Result.ReplaceInline(TEXT("../"), TEXT(""));
        Result.ReplaceInline(TEXT(".."), TEXT(""));
        Result.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
#if FILESHARING_ENABLED
        FString DownloadPath = FString([NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
#else
		FString DownloadPath = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
#endif
        Result = DownloadPath + Result;
        NSURL* URL = [NSURL fileURLWithPath : Result.GetNSString()];
#if !PLATFORM_TVOS		
		// this folder is expected to not exist on TVOS 
        if (![[NSFileManager defaultManager] fileExistsAtPath:[URL path]])
        {
            [[NSFileManager defaultManager] createDirectoryAtURL:URL withIntermediateDirectories : YES attributes : nil error : nil];
        }
        
        // mark it to not be uploaded
        NSError *error = nil;
        BOOL success = [URL setResourceValue : [NSNumber numberWithBool : YES] forKey : NSURLIsExcludedFromBackupKey error : &error];
        if (!success)
        {
            NSLog(@"Error excluding %@ from backup %@",[URL lastPathComponent], error);
        }
#endif		
    }
    return *GamePersistentDownloadDir;
}

EAppReturnType::Type FIOSPlatformMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
	extern EAppReturnType::Type MessageBoxExtImpl( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );
	return MessageBoxExtImpl(MsgType, Text, Caption);
}

int FIOSPlatformMisc::GetAudioVolume()
{
	return [[IOSAppDelegate GetDelegate] GetAudioVolume];
}

int32 FIOSPlatformMisc::GetDeviceVolume()
{
	return [[IOSAppDelegate GetDelegate] GetAudioVolume];
}

bool FIOSPlatformMisc::AreHeadphonesPluggedIn()
{
	return [[IOSAppDelegate GetDelegate] AreHeadphonesPluggedIn];
}

int FIOSPlatformMisc::GetBatteryLevel()
{
	return [[IOSAppDelegate GetDelegate] GetBatteryLevel];
}

float FIOSPlatformMisc::GetBrightness()
{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
	return (float)[[[IOSAppDelegate GetDelegate] window] screen].brightness;
#else
	return 1.0f;
#endif // !PLATFORM_TVOS
}

void FIOSPlatformMisc::SetBrightness(float Brightness)
{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
	dispatch_async(dispatch_get_main_queue(), ^{
		[[[IOSAppDelegate GetDelegate] window] screen].brightness = Brightness;
	});
#endif // !PLATFORM_TVOS
}

bool FIOSPlatformMisc::IsRunningOnBattery()
{
	return [[IOSAppDelegate GetDelegate] IsRunningOnBattery];
}

float FIOSPlatformMisc::GetDeviceTemperatureLevel()
{
#if !PLATFORM_TVOS
    switch ([[IOSAppDelegate GetDelegate] GetThermalState])
    {
        case NSProcessInfoThermalStateNominal:	return (float)FCoreDelegates::ETemperatureSeverity::Good; break;
        case NSProcessInfoThermalStateFair:		return (float)FCoreDelegates::ETemperatureSeverity::Bad; break;
        case NSProcessInfoThermalStateSerious:	return (float)FCoreDelegates::ETemperatureSeverity::Serious; break;
        case NSProcessInfoThermalStateCritical:	return (float)FCoreDelegates::ETemperatureSeverity::Critical; break;
    }
#endif
	return -1.0f;
}

bool FIOSPlatformMisc::IsInLowPowerMode()
{
#if !PLATFORM_TVOS
    bool bInLowPowerMode = [[NSProcessInfo processInfo] isLowPowerModeEnabled];
    return bInLowPowerMode;
#endif
    return false;
}


#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
UIInterfaceOrientationMask GetUIInterfaceOrientationMask(EDeviceScreenOrientation ScreenOrientation)
{
	switch (ScreenOrientation)
	{
	default:
		// Fallthrough...
	case EDeviceScreenOrientation::Unknown:
		return UIInterfaceOrientationMaskAll;
		break;
	case EDeviceScreenOrientation::Portrait:
		return UIInterfaceOrientationMaskPortrait;
		break;
	case EDeviceScreenOrientation::PortraitUpsideDown:
		return UIInterfaceOrientationMaskPortraitUpsideDown;
		break;
	case EDeviceScreenOrientation::LandscapeLeft:
		return UIInterfaceOrientationMaskLandscapeLeft;
		break;
	case EDeviceScreenOrientation::LandscapeRight:
		return UIInterfaceOrientationMaskLandscapeRight;
		break;
	case EDeviceScreenOrientation::FaceUp:
		return UIInterfaceOrientationMaskAll;
		break;
	case EDeviceScreenOrientation::FaceDown:
		return UIInterfaceOrientationMaskAll;
		break;
	case EDeviceScreenOrientation::PortraitSensor:
		return UIInterfaceOrientationMaskPortrait;
		break;
	case EDeviceScreenOrientation::LandscapeSensor:
		return UIInterfaceOrientationMaskLandscape;
		break;
	case EDeviceScreenOrientation::FullSensor:
		return UIInterfaceOrientationMaskAll;
		break;
	}
}
#endif

EDeviceScreenOrientation FIOSPlatformMisc::GetDeviceOrientation()
{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	if (AppDelegate.InterfaceOrientation == UIInterfaceOrientationUnknown)
	{
		AppDelegate.InterfaceOrientation = [[[[[UIApplication sharedApplication] delegate] window] windowScene] interfaceOrientation];
	}

	return [IOSAppDelegate ConvertFromUIInterfaceOrientation:AppDelegate.InterfaceOrientation];
#else
	return EDeviceScreenOrientation::Unknown;
#endif
}

void FIOSPlatformMisc::SetDeviceOrientation(EDeviceScreenOrientation NewDeviceOrientation)
{
	SetAllowedDeviceOrientation(NewDeviceOrientation);
}

void FIOSPlatformMisc::SetAllowedDeviceOrientation(EDeviceScreenOrientation NewAllowedDeviceOrientation)
{
	AllowedDeviceOrientation = NewAllowedDeviceOrientation;

#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
	[IOSAppDelegate GetDelegate].IOSView->SupportedInterfaceOrientations = GetUIInterfaceOrientationMask(NewAllowedDeviceOrientation);
#endif
}

bool FIOSPlatformMisc::HasPlatformFeature(const TCHAR* FeatureName)
{
	if (FCString::Stricmp(FeatureName, TEXT("Metal")) == 0)
	{
		return true;
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

FString GetIOSDeviceIDString()
{
	static FString CachedResult;
	static bool bCached = false;
	if (!bCached)
	{
		// get the device hardware type string length
		size_t DeviceIDLen;
		sysctlbyname("hw.machine", NULL, &DeviceIDLen, NULL, 0);

		// get the device hardware type
		char* DeviceID = (char*)malloc(DeviceIDLen);
		sysctlbyname("hw.machine", DeviceID, &DeviceIDLen, NULL, 0);

		CachedResult = ANSI_TO_TCHAR(DeviceID);
		bCached = true;

		free(DeviceID);
		
		// arm simulator
		// @todo test intel simulator
		if (CachedResult == "arm64")
		{
#if PLATFORM_VISIONOS
			CachedResult = TEXT("VisionPro0,1");
#elif PLATFORM_TVOS
			CachedResult = TEXT("AppleTV0,1");
#else
			NSString* ModelID = [[NSProcessInfo processInfo] environment][@"SIMULATOR_MODEL_IDENTIFIER"];
			CachedResult = FString(ModelID);
#endif
		}
	}

	return CachedResult;
}


const TCHAR* FIOSPlatformMisc::GetDefaultDeviceProfileName()
{
	static FString IOSDeviceProfileName;
	if (IOSDeviceProfileName.Len() == 0)
	{
		IOSDeviceProfileName = TEXT("IOS");

		FString DeviceIDString = GetIOSDeviceIDString();
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Device Type: %s") LINE_TERMINATOR, *DeviceIDString);

		TArray<FString> Mappings;
		if (ensure(GConfig->GetSection(TEXT("IOSDeviceMappings"), Mappings, GDeviceProfilesIni)))
		{
			for (const FString& MappingString : Mappings)
			{
				FString MappingRegex, ProfileName;
				if (MappingString.Split(TEXT("="), &MappingRegex, &ProfileName))
				{
					const FRegexPattern RegexPattern(MappingRegex);
					FRegexMatcher RegexMatcher(RegexPattern, *DeviceIDString);
					if (RegexMatcher.FindNext())
					{
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Matched %s as %s") LINE_TERMINATOR, *MappingRegex, *ProfileName);
						IOSDeviceProfileName = ProfileName;
						break;
					}
				}
				else
				{
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Invalid IOSDeviceMappings: %s") LINE_TERMINATOR, *MappingString);
				}
			}
		}
	}

	return *IOSDeviceProfileName;
}

// Deprecated in 4.26.
FIOSPlatformMisc::EIOSDevice FIOSPlatformMisc::GetIOSDeviceType()
{
	// default to unknown
	static EIOSDevice DeviceType = IOS_Unknown;

	// if we've already figured it out, return it
	if (DeviceType != IOS_Unknown)
	{
		return DeviceType;
	}

	const FString DeviceIDString = GetIOSDeviceIDString();

    FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Device Type: %s") LINE_TERMINATOR, *DeviceIDString);

#if PLATFORM_VISIONOS
	DeviceType = IOS_RealityPro;
#else
	
    // iPods
	if (DeviceIDString.StartsWith(TEXT("iPod")))
	{
		// get major revision number
        int Major = FCString::Atoi(&DeviceIDString[4]);

		if (Major == 5)
		{
			DeviceType = IOS_IPodTouch5;
		}
		else if (Major == 7)
		{
			DeviceType = IOS_IPodTouch6;
		}
		else if (Major >= 9)
		{
			DeviceType = IOS_IPodTouch7;
		}
	}
	// iPads
	else if (DeviceIDString.StartsWith(TEXT("iPad")))
	{
		// get major revision number
		const int Major = FCString::Atoi(&DeviceIDString[4]);
		const int CommaIndex = DeviceIDString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, 4);
		const int Minor = FCString::Atoi(&DeviceIDString[CommaIndex + 1]);

		// iPad2,[1|2|3] is iPad 2 (1 - wifi, 2 - gsm, 3 - cdma)
		if (Major == 2)
		{
			// iPad2,5+ is the new iPadMini, anything higher will use these settings until released
			if (Minor >= 5)
			{
				DeviceType = IOS_IPadMini;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
		// iPad3,[1|2|3] is iPad 3 and iPad3,4+ is iPad (4th generation)
		else if (Major == 3)
		{
			if (Minor <= 3)
			{
				DeviceType = IOS_IPad3;
			}
			// iPad3,4+ is the new iPad, anything higher will use these settings until released
			else if (Minor >= 4)
			{
				DeviceType = IOS_IPad4;
			}
		}
		// iPadAir and iPad Mini 2nd Generation
		else if (Major == 4)
		{
			if (Minor >= 4)
			{
				DeviceType = IOS_IPadMini2;
			}
			else
			{
				DeviceType = IOS_IPadAir;
			}
		}
		// iPad Air 2 and iPadMini 4
		else if (Major == 5)
		{
			if (Minor == 1 || Minor == 2)
			{
				DeviceType = IOS_IPadMini4;
			}
			else
			{
				DeviceType = IOS_IPadAir2;
			}
		}
		else if (Major == 6)
		{
			if (Minor == 3 || Minor == 4)
			{
				DeviceType = IOS_IPadPro_97;
			}
			else if (Minor == 11 || Minor == 12)
			{
				DeviceType = IOS_IPad5;
			}
			else
			{
				DeviceType = IOS_IPadPro_129;
			}
		}
		else if (Major == 7)
		{
			if (Minor == 3 || Minor == 4)
			{
				DeviceType = IOS_IPadPro_105;
			}
			else if (Minor == 5 || Minor == 6)
			{
				DeviceType = IOS_IPad6;
			}
			else if (Minor == 11 || Minor == 12)
			{
				DeviceType = IOS_IPad7;
			}
			else
			{
				DeviceType = IOS_IPadPro2_129;
			}
		}
		else if (Major == 8)
		{
			if (Minor <= 4)
			{
				DeviceType = IOS_IPadPro_11;
			}
			else if (Minor <= 8)
			{
				DeviceType = IOS_IPadPro3_129;
			}
			else if (Minor <= 10)
			{
				DeviceType = IOS_IPadPro2_11;
			}
			else
			{
				DeviceType = IOS_IPadPro4_129;
			}
		}
        else if (Major == 11)
        {
            if (Minor <= 2)
            {
                DeviceType = IOS_IPadMini5;
            }
            else
            {
                DeviceType = IOS_IPadAir3;
            }
        }
		// Default to highest settings currently available for any future device
		else if (Major >= 9)
		{
			DeviceType = IOS_IPadPro4_129;
		}
	}
	// iPhones
	else if (DeviceIDString.StartsWith(TEXT("iPhone")))
	{
        const int Major = FCString::Atoi(&DeviceIDString[6]);
		const int CommaIndex = DeviceIDString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, 6);
		const int Minor = FCString::Atoi(&DeviceIDString[CommaIndex + 1]);

		if (Major == 3)
		{
			DeviceType = IOS_IPhone4;
		}
		else if (Major == 4)
		{
			DeviceType = IOS_IPhone4S;
		}
		else if (Major == 5)
		{
			DeviceType = IOS_IPhone5;
		}
		else if (Major == 6)
		{
			DeviceType = IOS_IPhone5S;
		}
		else if (Major == 7)
		{
			if (Minor == 1)
			{
				DeviceType = IOS_IPhone6Plus;
			}
			else if (Minor == 2)
			{
				DeviceType = IOS_IPhone6;
			}
		}
		else if (Major == 8)
		{
			// note that Apple switched the minor order around between 6 and 6S (gotta keep us on our toes!)
			if (Minor == 1)
			{
				DeviceType = IOS_IPhone6S;
			}
			else if (Minor == 2)
			{
				DeviceType = IOS_IPhone6SPlus;
			}
			else if (Minor == 4)
			{
				DeviceType = IOS_IPhoneSE;
			}
		}
		else if (Major == 9)
		{
            if (Minor == 1 || Minor == 3)
            {
                DeviceType = IOS_IPhone7;
            }
            else if (Minor == 2 || Minor == 4)
            {
                DeviceType = IOS_IPhone7Plus;
            }
		}
        else if (Major == 10)
        {
			if (Minor == 1 || Minor == 4)
			{
				DeviceType = IOS_IPhone8;
			}
			else if (Minor == 2 || Minor == 5)
			{
				DeviceType = IOS_IPhone8Plus;
			}
			else if (Minor == 3 || Minor == 6)
			{
				DeviceType = IOS_IPhoneX;
			}
		}
        else if (Major == 11)
        {
            if (Minor == 2)
            {
                DeviceType = IOS_IPhoneXS;
            }
            else if (Minor == 4 || Minor == 6)
            {
                DeviceType = IOS_IPhoneXSMax;
            }
            else if (Minor == 8)
            {
                DeviceType = IOS_IPhoneXR;
            }
        }
		else if (Major == 12)
		{
			if (Minor < 3)
			{
				DeviceType = IOS_IPhone11;
			}
			else if (Minor < 5)
			{
				DeviceType = IOS_IPhone11Pro;
			}
			else if (Minor < 7)
			{
				DeviceType = IOS_IPhone11ProMax;
			}
			else if (Minor == 8)
			{
				DeviceType = IOS_IPhoneSE2;
			}
		}
		else if (Major >= 13)
		{
			// for going forward into unknown devices (like 8/8+?), we can't use Minor,
			// so treat devices with a scale > 2.5 to be 6SPlus type devices, < 2.5 to be 6S type devices
			if ([UIScreen mainScreen].scale > 2.5f)
			{
				DeviceType = IOS_IPhone11ProMax;
			}
			else
			{
				DeviceType = IOS_IPhone11Pro;
			}
		}
	}
	// tvOS
	else if (DeviceIDString.StartsWith(TEXT("AppleTV")))
	{
		const int Major = FCString::Atoi(&DeviceIDString[7]);
		const int CommaIndex = DeviceIDString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, 6);
		const int Minor = FCString::Atoi(&DeviceIDString[CommaIndex + 1]);

		if (Major == 5)
		{
			DeviceType = IOS_AppleTV;
		}
		else if (Major == 6)
		{
			DeviceType = IOS_AppleTV4K;
		}
		else if (Major >= 6)
		{
			DeviceType = IOS_AppleTV4K;
		}
	}
	// simulator
	else if (DeviceIDString.StartsWith(TEXT("x86")))
	{
		// iphone
		if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
		{
			CGSize result = [[UIScreen mainScreen] bounds].size;
			if(result.height >= 586)
			{
				DeviceType = IOS_IPhone5;
			}
			else
			{
				DeviceType = IOS_IPhone4S;
			}
		}
		else
		{
			if ([[UIScreen mainScreen] scale] > 1.0f)
			{
				DeviceType = IOS_IPad4;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
	}
#endif
	
	// if this is unknown at this point, we have a problem
	if (DeviceType == IOS_Unknown)
	{
		UE_LOG(LogInit, Fatal, TEXT("This IOS device type is not supported by UE4 [%s]\n"), *FString(DeviceIDString));
	}

	return DeviceType;
}

int FIOSPlatformMisc::GetDefaultStackSize()
{
	return 512 * 1024;
}

void FIOSPlatformMisc::SetMemoryWarningHandler(void (* InHandler)(const FGenericMemoryWarningContext& Context))
{
	GMemoryWarningHandler = InHandler;
}

bool FIOSPlatformMisc::HasMemoryWarningHandler()
{
	return GMemoryWarningHandler != nullptr;
}

void FIOSPlatformMisc::HandleLowMemoryWarning()
{
	UE_LOG(LogInit, Log, TEXT("Low Memory Warning Triggered"));
	UE_LOG(LogInit, Log, TEXT("Free Memory at Startup: %d MB"), GStartupFreeMemoryMB);
	UE_LOG(LogInit, Log, TEXT("Free Memory Now       : %d MB"), GetFreeMemoryMB());

	if(GMemoryWarningHandler != NULL)
	{
		FGenericMemoryWarningContext Context;
		GMemoryWarningHandler(Context);
	}
}

bool FIOSPlatformMisc::IsPackagedForDistribution()
{
#if !UE_BUILD_SHIPPING
	static bool PackagingModeCmdLine = FParse::Param(FCommandLine::Get(), TEXT("PACKAGED_FOR_DISTRIBUTION"));
	if (PackagingModeCmdLine)
	{
		return true;
	}
#endif
	NSString* PackagingMode = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"EpicPackagingMode"];
	return PackagingMode != nil && [PackagingMode isEqualToString : @"Distribution"];
}

FString FIOSPlatformMisc::GetDeviceId()
{
#if GET_DEVICE_ID_UNAVAILABLE
	return FString();
#else
	// Check to see if this OS has this function

	if ([[UIDevice currentDevice] respondsToSelector:@selector(identifierForVendor)])
	{
	    NSUUID* Id = [[UIDevice currentDevice] identifierForVendor];
	    if (Id != nil)
	    {
		    NSString* IdfvString = [Id UUIDString];
		    FString IDFV(IdfvString);
		    return IDFV;
	    }
	}
	return FString();
#endif
}

FString FIOSPlatformMisc::GetOSVersion()
{
	return FString([[UIDevice currentDevice] systemVersion]);
}

bool FIOSPlatformMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
    //On iOS 11 use new method to return disk space available for important usages
#if !PLATFORM_TVOS
    bool GetValueSuccess = false;
    
    NSNumber *FreeBytes = nil;
    NSURL *URL = [NSURL fileURLWithPath : NSHomeDirectory()];
    GetValueSuccess = [URL getResourceValue : &FreeBytes forKey : NSURLVolumeAvailableCapacityForImportantUsageKey error : nil];
    if (FreeBytes)
    {
        NumberOfFreeBytes = [FreeBytes longLongValue];
    }
    
    NSNumber *TotalBytes = nil;
    GetValueSuccess = GetValueSuccess &&[URL getResourceValue : &TotalBytes forKey : NSURLVolumeTotalCapacityKey error : nil];
    if (TotalBytes)
    {
        TotalNumberOfBytes = [TotalBytes longLongValue];
    }
    
    if (GetValueSuccess
        && (NumberOfFreeBytes > 0)
        && (TotalNumberOfBytes > 0))
    {
        return true;
    }
#endif

    //fallback to old method if we didn't return above
    {
        NSDictionary<NSFileAttributeKey, id>* FSStat = [[NSFileManager defaultManager] attributesOfFileSystemForPath:NSHomeDirectory() error : nil];
        if (FSStat)
        {
            NumberOfFreeBytes = [[FSStat objectForKey : NSFileSystemFreeSize] longLongValue];
            TotalNumberOfBytes = [[FSStat objectForKey : NSFileSystemSize] longLongValue];

            return true;
        }

        return false;
    }
}


void FIOSPlatformMisc::RequestStoreReview()
{
#if !PLATFORM_TVOS
    [SKStoreReviewController requestReviewInScene:[[[[UIApplication sharedApplication] delegate] window] windowScene]];
#endif
}

bool FIOSPlatformMisc::IsUpdateAvailable()
{
	return [[IOSAppDelegate GetDelegate] IsUpdateAvailable];
}

/**
* Returns a unique string for advertising identification
*
* @return the unique string generated by this platform for this device
*/
FString FIOSPlatformMisc::GetUniqueAdvertisingId()
{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS && ENABLE_ADVERTISING_IDENTIFIER
	// Check to see if this OS has this function
	if ([[ASIdentifierManager sharedManager] respondsToSelector:@selector(advertisingIdentifier)])
	{
		NSString* IdfaString = [[[ASIdentifierManager sharedManager] advertisingIdentifier] UUIDString];
		FString IDFA(IdfaString);
		return IDFA;
	}
#endif
	return FString();
}

class IPlatformChunkInstall* FIOSPlatformMisc::GetPlatformChunkInstall()
{
	static IPlatformChunkInstall* ChunkInstall = nullptr;
	static bool bIniChecked = false;
	if (!ChunkInstall || !bIniChecked)
	{
		FString ProviderName;
		IPlatformChunkInstallModule* PlatformChunkInstallModule = nullptr;
		if (!GEngineIni.IsEmpty())
		{
			FString InstallModule;
			GConfig->GetString(TEXT("StreamingInstall"), TEXT("DefaultProviderName"), InstallModule, GEngineIni);
			FModuleStatus Status;
			if (FModuleManager::Get().QueryModule(*InstallModule, Status))
			{
				PlatformChunkInstallModule = FModuleManager::LoadModulePtr<IPlatformChunkInstallModule>(*InstallModule);
				if (PlatformChunkInstallModule != nullptr)
				{
					// Attempt to grab the platform installer
					ChunkInstall = PlatformChunkInstallModule->GetPlatformChunkInstall();
				}
			}
			else if (ProviderName == TEXT("IOSChunkInstaller"))
			{
				static FIOSChunkInstall Singleton;
				ChunkInstall = &Singleton;
			}
			bIniChecked = true;
		}
		if (!ChunkInstall)
		{
			// Placeholder instance
			ChunkInstall = FGenericPlatformMisc::GetPlatformChunkInstall();
		}
	}

	return ChunkInstall;
}

bool FIOSPlatformMisc::SupportsForceTouchInput()
{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
	return [[[IOSAppDelegate GetDelegate].IOSView traitCollection] forceTouchCapability];
#else
	return false;
#endif
}

#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
static UIFeedbackGenerator* GFeedbackGenerator = nullptr;
#endif // !PLATFORM_TVOS
static EMobileHapticsType GHapticsType;
void FIOSPlatformMisc::PrepareMobileHaptics(EMobileHapticsType Type)
{
	// these functions must run on the main IOS thread
	dispatch_async(dispatch_get_main_queue(), ^
	{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
		if (GFeedbackGenerator != nullptr)
		{
            UE_LOG(LogIOS, Warning, TEXT("Multiple haptics were prepared at once! Implement a stack of haptics types, or a wrapper object that is returned, with state"));
			[GFeedbackGenerator release];
		}

		GHapticsType = Type;
		switch (GHapticsType)
		{
			case EMobileHapticsType::FeedbackSuccess:
			case EMobileHapticsType::FeedbackWarning:
			case EMobileHapticsType::FeedbackError:
				GFeedbackGenerator = [[UINotificationFeedbackGenerator alloc] init];
				break;

			case EMobileHapticsType::SelectionChanged:
				GFeedbackGenerator = [[UISelectionFeedbackGenerator alloc] init];
				break;

			default:
				GHapticsType = EMobileHapticsType::ImpactLight;
				// fall-through, and treat like Impact

			case EMobileHapticsType::ImpactLight:
				GFeedbackGenerator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
				break;

			case EMobileHapticsType::ImpactMedium:
				GFeedbackGenerator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
				break;

			case EMobileHapticsType::ImpactHeavy:
				GFeedbackGenerator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
				break;
		}

		// prepare the generator object so Trigger won't delay
		[GFeedbackGenerator prepare];
#endif // !PLATFORM_TVOS
	});
}

void FIOSPlatformMisc::TriggerMobileHaptics()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
		if (GFeedbackGenerator == nullptr)
		{
			return;
		}

		switch (GHapticsType)
		{
			case EMobileHapticsType::FeedbackSuccess:
				[(UINotificationFeedbackGenerator*)GFeedbackGenerator notificationOccurred:UINotificationFeedbackTypeSuccess];
				break;

			case EMobileHapticsType::FeedbackWarning:
				[(UINotificationFeedbackGenerator*)GFeedbackGenerator notificationOccurred:UINotificationFeedbackTypeWarning];
				break;

			case EMobileHapticsType::FeedbackError:
				[(UINotificationFeedbackGenerator*)GFeedbackGenerator notificationOccurred:UINotificationFeedbackTypeError];
				break;

			case EMobileHapticsType::SelectionChanged:
				[(UISelectionFeedbackGenerator*)GFeedbackGenerator selectionChanged];
				break;

			case EMobileHapticsType::ImpactLight:
			case EMobileHapticsType::ImpactMedium:
			case EMobileHapticsType::ImpactHeavy:
				[(UIImpactFeedbackGenerator*)GFeedbackGenerator impactOccurred];
				break;
		}
#endif // !PLATFORM_TVOS
	});
}

void FIOSPlatformMisc::ReleaseMobileHaptics()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
		if (GFeedbackGenerator == nullptr)
		{
			return;
		}

		[GFeedbackGenerator release];
		GFeedbackGenerator = nullptr;
#endif // !PLATFORM_TVOS
	});
}

void FIOSPlatformMisc::ShareURL(const FString& URL, const FText& Description, int32 LocationHintX, int32 LocationHintY)
{
	NSString* SharedString = [NSString stringWithFString:Description.ToString()];
	NSURL* SharedURL = [NSURL URLWithString:[NSString stringWithFString:URL]];
	CGRect PopoverLocation = CGRectMake(LocationHintX, LocationHintY, 1, 1);

	dispatch_async(dispatch_get_main_queue(),^ {
		NSArray* ObjectsToShare = @[SharedString, SharedURL];
#if !PLATFORM_TVOS
		// create the share sheet view
		UIActivityViewController* ActivityVC = [[UIActivityViewController alloc] initWithActivityItems:ObjectsToShare applicationActivities:nil];
		[ActivityVC autorelease];
	
		// skip over some things that don't make sense
		ActivityVC.excludedActivityTypes = @[UIActivityTypePrint,
											 UIActivityTypeAssignToContact,
											 UIActivityTypeSaveToCameraRoll,
											 UIActivityTypePostToFlickr,
											 UIActivityTypePostToVimeo];
		
		if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
		{
			[[IOSAppDelegate GetDelegate].IOSController presentViewController:ActivityVC animated:YES completion:nil];
		}
		else
		{
			// Present the view controller using the popover style.
			ActivityVC.modalPresentationStyle = UIModalPresentationPopover;
			[[IOSAppDelegate GetDelegate].IOSController presentViewController:ActivityVC
							   animated:YES
							 completion:nil];
			
			// Get the popover presentation controller and configure it.
			UIPopoverPresentationController* PresentationController = [ActivityVC popoverPresentationController];
			PresentationController.sourceView = [IOSAppDelegate GetDelegate].IOSView;
			PresentationController.sourceRect = PopoverLocation;
			
		}
#endif // !PLATFORM_TVOS
	});
}


FString FIOSPlatformMisc::LoadTextFileFromPlatformPackage(const FString& RelativePath)
{
	FString FilePath = FString([[NSBundle mainBundle] bundlePath]) / RelativePath;

	// read in the command line text file (coming from UnrealFrontend) if it exists
	int32 File = open(TCHAR_TO_UTF8(*FilePath), O_RDONLY);
	if (File == -1)
	{
		LowLevelOutputDebugStringf(TEXT("No file found at %s") LINE_TERMINATOR, *FilePath);
		return FString();
	}

	ON_SCOPE_EXIT
	{
		close(File);
	};

	struct stat FileInfo;
	FileInfo.st_size = -1;

	if (fstat(File, &FileInfo))
	{
		LowLevelOutputDebugStringf(TEXT("Failed to determine file size of %s") LINE_TERMINATOR, *FilePath);
		return FString();
	}

	if (FileInfo.st_size > MAX_int32 - 1)
	{
		LowLevelOutputDebugStringf(TEXT("File too big %s") LINE_TERMINATOR, *FilePath);
		return FString();
	}

	LowLevelOutputDebugStringf(TEXT("Found %s file") LINE_TERMINATOR, *RelativePath);

	int32 FileSize = static_cast<int32>(FileInfo.st_size);
	TArray<char> FileContents;
	FileContents.AddUninitialized(FileSize + 1);
	FileContents[FileSize] = 0;

	int32 NumRead = read(File, FileContents.GetData(), FileSize);
	if (NumRead != FileSize)
	{
		LowLevelOutputDebugStringf(TEXT("Failed to read %s") LINE_TERMINATOR, *FilePath);
		return FString();
	}

	// chop off trailing spaces
	int32 Last = FileSize - 1;
	while (FileContents[0] && isspace(FileContents[Last]))
	{
		FileContents[Last] = 0;
		--Last;
	}

	return FString(UTF8_TO_TCHAR(FileContents.GetData()));
}

bool FIOSPlatformMisc::FileExistsInPlatformPackage(const FString& RelativePath)
{
	FString FilePath = FString([[NSBundle mainBundle] bundlePath]) / RelativePath;

	return 0 == access(TCHAR_TO_UTF8(*FilePath), F_OK);
}

void FIOSPlatformMisc::EnableVoiceChat(bool bEnable)
{
	return [[IOSAppDelegate GetDelegate] EnableVoiceChat:bEnable];
}

bool FIOSPlatformMisc::IsVoiceChatEnabled()
{
	return [[IOSAppDelegate GetDelegate] IsVoiceChatEnabled];
}

void FIOSPlatformMisc::RegisterForRemoteNotifications()
{
	if (FApp::IsUnattended())
	{
		return;
	}

    dispatch_async(dispatch_get_main_queue(), ^{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS && NOTIFICATIONS_ENABLED
		UNUserNotificationCenter *Center = [UNUserNotificationCenter currentNotificationCenter];
		[Center requestAuthorizationWithOptions:(UNAuthorizationOptionBadge | UNAuthorizationOptionSound | UNAuthorizationOptionAlert)
							  completionHandler:^(BOOL granted, NSError * _Nullable error) {
								  if (error)
								  {
									  UE_LOG(LogIOS, Log, TEXT("Failed to register for notifications."));
								  }
								  else
								  {
									  int32 types = (int32)granted;
                                      if (granted)
                                      {
                                          UIApplication* application = [UIApplication sharedApplication];
                                          [application registerForRemoteNotifications];
                                          
                                      }
									  FFunctionGraphTask::CreateAndDispatchWhenReady([types]()
																					 {
																						 FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate.Broadcast(types);
																					 }, TStatId(), NULL, ENamedThreads::GameThread);
									  
								  }
							  }];
#endif
    });
}

bool FIOSPlatformMisc::IsRegisteredForRemoteNotifications()
{
	return false;
}

bool FIOSPlatformMisc::IsAllowedRemoteNotifications()
{
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS && NOTIFICATIONS_ENABLED
	checkf(false, TEXT("For min iOS version >= 10 use FIOSLocalNotificationService::CheckAllowedNotifications."));
	return true;
#else
	return true;
#endif
}

void FIOSPlatformMisc::UnregisterForRemoteNotifications()
{

}


// See for more information about the Blobs
// https://opensource.apple.com/source/xnu/xnu-4570.61.1/osfmk/kern/cs_blobs.h.auto.html

typedef struct __Blob {
    uint32_t magic;
    uint32_t length;
    char data[];
} CS_GenericBlob;

typedef struct __BlobIndex {
    uint32_t type;
    uint32_t offset;
} CS_BlobIndex;

typedef struct __MultiBlob {
    uint32_t magic;
    uint32_t length;
    uint32_t count;
    CS_BlobIndex index[];
} CS_MultiBlob;

extern NSString *EntitlementsData(void)
{
    // iterate through the headers to find the executable, since only the executable has the entitlements
    const struct mach_header_64* executableHeader = nullptr;
    char* ImageName = nullptr;
    for (uint32_t i = 0; i < _dyld_image_count() && executableHeader == nullptr; i++)
    {
        const struct mach_header_64 *header = (struct mach_header_64 *)_dyld_get_image_header(i);
        if (header->filetype == MH_EXECUTE)
        {
            ImageName = (char *)_dyld_get_image_name(i);
            executableHeader = header;
        }
    }

    if (executableHeader == nullptr)
    {
        return nil;
    }

    // verify that it's a 64bit app
    if (executableHeader->magic != MH_MAGIC_64)
    {
        UE_LOG(LogIOS, Error, TEXT("Executable is NOT 64bit. Entitlement retrieval not supported."));
        return nil;
    }
    uintptr_t cursor = (uintptr_t)executableHeader + sizeof(struct mach_header_64);
    const struct linkedit_data_command *segmentCommand = NULL;

    for (uint32_t i = 0; i < executableHeader->ncmds; i++, cursor += segmentCommand->cmdsize)
    {
        segmentCommand = (struct linkedit_data_command *)cursor;
    
        switch (segmentCommand->cmd)
        {
            case LC_CODE_SIGNATURE:
                UE_LOG(LogIOS, Log, TEXT("LC_CODE_SIGNATURE found"));
                break;
            default:
                continue;
        }

        const struct linkedit_data_command *dataCommand = (const struct linkedit_data_command *)segmentCommand;

        FILE* file = fopen(ImageName, "rb");
        if (file == NULL)
        {
            UE_LOG(LogIOS, Error, TEXT("Could not open binary file"));
            return nil;
        }
        CS_MultiBlob multiBlob;
        int isSuccess = fseek(file, UInt64(dataCommand->dataoff), SEEK_SET);
        size_t Count = fread(&multiBlob, sizeof(CS_MultiBlob), 1, file);
        uint32 multiBlobCount = ntohl(multiBlob.count);
        CS_BlobIndex multiBlobIndex[multiBlobCount];
        fread(&multiBlobIndex[0], sizeof(CS_BlobIndex) * multiBlobCount, 1, file);
        
        if (__builtin_bswap32(multiBlob.magic) != 0xfade0cc0)
        {
            fclose(file);
            return nil;
        }
        
        uint32 multiBlobSize = sizeof(multiBlob);
        uint32 blobSize = sizeof(CS_GenericBlob);

        for (int j = 0; j < multiBlobCount; j++)
        {
            uint32_t currentOffset = dataCommand->dataoff;
            uint32_t blobOffset = ntohl(multiBlobIndex[j].offset);
            CS_GenericBlob blob;
            isSuccess = fseek(file, currentOffset + blobOffset, SEEK_SET);
            Count = fread(&blob, sizeof(CS_GenericBlob), 1, file);
            

            if (__builtin_bswap32(blob.magic) == 0xfade7171)
            {
                uint32 blobLength = ntohl(blob.length);
                char data[blobLength];
                fread(&data[0], sizeof(char) * blobLength, 1, file);
                NSString *stringFromData = [NSString stringWithFormat: @"%s", data];
                NSLog(@"%@", stringFromData);
                fclose(file);
                return stringFromData;
            }
            else
            {
                continue;
            }
        }
        fclose(file);
    }
    return nil;
}

extern bool IsEntitlementPresentInEmbeddedProvision(const char *EntitlementsToFind)
{
    NSString* mobileprovisionPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"embedded.mobileprovision"];
  
    FILE* file = fopen([mobileprovisionPath cStringUsingEncoding:1],"rb");

    size_t entitlements_len = strlen(EntitlementsToFind);
    
    if (file == NULL)
    {
        NSLog(@"Mobile Provision not found");
        return false;
    }

    size_t readcount;
    char buffer[1025]; //room for null
    while(true)
    {
        readcount = fread(buffer, 1, 1024, file);
        if (readcount <= 0)
        {
            break;
        }
        for(size_t i = 0; i < readcount; i++)
        {
            if(buffer[i] == 0)
            {
                buffer[i] = ' ';  // replace any null terminators that might be in the binary data
            }
        }
        
        buffer[readcount] = 0; // null terminate buffer!
        char* entitlementskeyptr = strstr(buffer, EntitlementsToFind);
        
        if(entitlementskeyptr)
        {
            fseek(file, -readcount + entitlementskeyptr - buffer, SEEK_CUR); // seek to immediately after the entitlements key
            readcount = fread(buffer, 1, 1024, file); // read 1024 bytes immediately after the entitlements key so we definitely get the value untruncated
            // we don't expect any \0 characters between the key and the value, so we don't do the replace thing.
            buffer[readcount] = 0; // null terminate buffer!
            
            // there could be keys following our one with their own true or false values that we could accidentally match if we just look for 'true' and return immediately.
            char* trueptr = strstr(buffer, "true");
            char* falseptr = strstr(buffer, "false");
            
            if(trueptr == NULL && falseptr == NULL)
            {
                UE_LOG(LogIOS, Error, TEXT("Unexpected Behaviour. The entitlement key is found but its value is not set."));
                return false;
            }
            if(trueptr && falseptr == NULL)  // only true
            {
                UE_LOG(LogIOS, Log, TEXT("Entitlements found in embedded mobile provision file."));
                return true;
            }
            if(trueptr == NULL && falseptr) // only false
            {
                UE_LOG(LogIOS, Log, TEXT("Entitlements found but set to false."));
                return false;
            }
            return (trueptr < falseptr); // return true if true comes before false
        }
        
        if(readcount < 1024)
        {
            break; // end of file
        }
        
        // seek back in case the entitlements was truncated.
        fseek(file, -entitlements_len, SEEK_CUR);
    }
    fclose(file);
    return false;
}

bool FIOSPlatformMisc::IsEntitlementEnabled(const char * EntitlementToCheck)
{
    NSString* TrueFlag = @"</key><true/>";
    NSString* EntitlementsToFind = [NSString stringWithFormat: @"%s%@" , EntitlementToCheck, TrueFlag];
    
    NSString* CleanedEntitlementData = [EntitlementsData() stringByReplacingOccurrencesOfString: @"\\s+" withString: @"" options: NSRegularExpressionSearch range: NSMakeRange(0, EntitlementsToFind.length)];

    CleanedEntitlementData = [[CleanedEntitlementData componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"\t"]] componentsJoinedByString:@""];
    CleanedEntitlementData = [[CleanedEntitlementData componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]] componentsJoinedByString:@""];

    if ([CleanedEntitlementData rangeOfString: (@"%s", EntitlementsToFind)].location == NSNotFound)
    {
        UE_LOG(LogIOS, Log, TEXT("Entitlements not found in binary Mach-O header. Looking at the embedded mobile provision file."));
        return IsEntitlementPresentInEmbeddedProvision(EntitlementToCheck);
    }
    else
    {
        return true;
    }
}

void FIOSPlatformMisc::GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames)
{
	// this is only used to cook with the proper TargetPlatform with COTF, it's not the runtime platform (which is just IOS for both)
#if PLATFORM_TVOS
	TargetPlatformNames.Add(TEXT("TVOS"));
#elif PLATFORM_VISIONOS
	TargetPlatformNames.Add(TEXT("VISIONOS"));
#else
	TargetPlatformNames.Add(FIOSPlatformProperties::PlatformName());
#endif
}

ENetworkConnectionType FIOSPlatformMisc::GetNetworkConnectionType()
{
	struct sockaddr_in ZeroAddress;
	FMemory::Memzero(&ZeroAddress, sizeof(ZeroAddress));
	ZeroAddress.sin_len = sizeof(ZeroAddress);
	ZeroAddress.sin_family = AF_INET;
	
	SCNetworkReachabilityRef ReachabilityRef = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (const struct sockaddr*)&ZeroAddress);
	SCNetworkReachabilityFlags ReachabilityFlags;
	bool bFlagsAvailable = SCNetworkReachabilityGetFlags(ReachabilityRef, &ReachabilityFlags);
	CFRelease(ReachabilityRef);
	
	bool bHasActiveWiFiConnection = false;
    bool bHasActiveCellConnection = false;
    bool bInAirplaneMode = false;
	if (bFlagsAvailable)
	{
		bool bReachable =	(ReachabilityFlags & kSCNetworkReachabilityFlagsReachable) != 0 &&
		(ReachabilityFlags & kSCNetworkReachabilityFlagsConnectionRequired) == 0 &&
		// in case kSCNetworkReachabilityFlagsConnectionOnDemand  || kSCNetworkReachabilityFlagsConnectionOnTraffic
		(ReachabilityFlags & kSCNetworkReachabilityFlagsInterventionRequired) == 0;
		
		bHasActiveWiFiConnection = bReachable && (ReachabilityFlags & kSCNetworkReachabilityFlagsIsWWAN) == 0;
        bHasActiveCellConnection = bReachable && (ReachabilityFlags & kSCNetworkReachabilityFlagsIsWWAN) != 0;
        bInAirplaneMode = ReachabilityFlags == 0;
	}
	
    if (bHasActiveWiFiConnection)
    {
        return ENetworkConnectionType::WiFi;
    }
    else if (bHasActiveCellConnection)
    {
        return ENetworkConnectionType::Cell;
    }
    else if (bInAirplaneMode)
    {
        return ENetworkConnectionType::AirplaneMode;
    }
    return ENetworkConnectionType::None;
}

bool FIOSPlatformMisc::HasActiveWiFiConnection()
{
    return GetNetworkConnectionType() == ENetworkConnectionType::WiFi;
}

FString FIOSPlatformMisc::GetCPUVendor()
{
	return TEXT("Apple");
}

FString FIOSPlatformMisc::GetCPUBrand()
{
	return GetIOSDeviceIDString();
}

void FIOSPlatformMisc::GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel)
{
#if PLATFORM_TVOS
	out_OSVersionLabel = TEXT("TVOS");
#elif PLATFORM_VISIONOS
	out_OSVersionLabel = TEXT("VisionOS");
#else
	out_OSVersionLabel = TEXT("IOS");
#endif
	NSOperatingSystemVersion IOSVersion;
	IOSVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
	out_OSSubVersionLabel = FString::Printf(TEXT("%ld.%ld.%ld"), IOSVersion.majorVersion, IOSVersion.minorVersion, IOSVersion.patchVersion);
}

int32 FIOSPlatformMisc::IOSVersionCompare(uint8 Major, uint8 Minor, uint8 Revision)
{
	NSOperatingSystemVersion IOSVersion;
	IOSVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
	uint8 TargetValues[3] = { Major, Minor, Revision };
	NSInteger ComponentValues[3] = { IOSVersion.majorVersion, IOSVersion.minorVersion, IOSVersion.patchVersion };

	for (uint32 i = 0; i < 3; i++)
	{
		if (ComponentValues[i] < TargetValues[i])
		{
			return -1;
		}
		else if (ComponentValues[i] > TargetValues[i])
		{
			return 1;
		}
	}

	return 0;
}

FString FIOSPlatformMisc::GetProjectVersion()
{
	NSDictionary* infoDictionary = [[NSBundle mainBundle] infoDictionary];
	FString localVersionString = FString(infoDictionary[@"CFBundleShortVersionString"]);
	return localVersionString;
}

FString FIOSPlatformMisc::GetBuildNumber()
{
	NSDictionary* infoDictionary = [[NSBundle mainBundle]infoDictionary];
	FString BuildString = FString(infoDictionary[@"CFBundleVersion"]);
	return BuildString;
}

bool FIOSPlatformMisc::RequestDeviceCheckToken(TFunction<void(const TArray<uint8>&)> QuerySucceededFunc, TFunction<void(const FString&, const FString&)> QueryFailedFunc)
{
	DCDevice* DeviceCheckDevice = [DCDevice currentDevice];
	if ([DeviceCheckDevice isSupported])
	{
		[DeviceCheckDevice generateTokenWithCompletionHandler : ^ (NSData * _Nullable token, NSError * _Nullable error)
		{
			bool bSuccess = (error == NULL);
			if (bSuccess)
			{
				TArray<uint8> DeviceToken((uint8*)[token bytes], [token length]);

				QuerySucceededFunc(DeviceToken);
			}
			else
			{
				FString ErrorDescription([error localizedDescription]);

				NSDate* currentDate = [[[NSDate alloc] init] autorelease];
                NSTimeZone* timeZone = [NSTimeZone defaultTimeZone];
                NSDateFormatter* dateFormatter = [[[NSDateFormatter alloc] init] autorelease];
                [dateFormatter setTimeZone:timeZone];
                [dateFormatter setDateFormat:@"yyyy-mm-dd'T'HH:mm:ss.SSS'Z'"];
                FString localDateString([dateFormatter stringFromDate:currentDate]);
                
				QueryFailedFunc(ErrorDescription, localDateString);
			}
		}];

		return true;
	}

	return false;
}

void (*GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;

// good enough default crash reporter
static void DefaultCrashHandler(FIOSCrashContext const& Context)
{
    Context.ReportCrash();
    if (GLog)
    {
        GLog->Panic();
    }
    if (GWarn)
    {
        GWarn->Flush();
    }
    if (GError)
    {
        GError->Flush();
        GError->HandleError();
    }
    return Context.GenerateCrashInfo();
}

// true system specific crash handler that gets called first
static FIOSCrashContext TempCrashContext(ECrashContextType::Crash, TEXT("Temp Context"));
static void PlatformCrashHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	// switch to crash handler malloc to avoid malloc reentrancy
	check(FIOSApplicationInfo::CrashMalloc);
	FIOSApplicationInfo::CrashMalloc->Enable(&TempCrashContext, FPlatformTLS::GetCurrentThreadId());
	
    FIOSCrashContext CrashContext(ECrashContextType::Crash, TEXT("Caught signal"));
    CrashContext.InitFromSignal(Signal, Info, Context);
	
	// switch to the crash malloc to the new context now that we have everything
	FIOSApplicationInfo::CrashMalloc->SetContext(&CrashContext);
	
    if (GCrashHandlerPointer)
    {
        GCrashHandlerPointer(CrashContext);
    }
    else
    {
        // call default one
        DefaultCrashHandler(CrashContext);
    }
}

static void PLCrashReporterHandler(siginfo_t* Info, ucontext_t* Uap, void* Context)
{
    PlatformCrashHandler((int32)Info->si_signo, Info, Uap);
}

// handles graceful termination.
static void GracefulTerminationHandler(int32 Signal, siginfo_t* Info, void* Context)
{
    // make sure we write out as much as possible
    if (GLog)
    {
        GLog->Panic();
    }
    if (GWarn)
    {
        GWarn->Flush();
    }
    if (GError)
    {
        GError->Flush();
    }
    
    if (!IsEngineExitRequested())
    {
		RequestEngineExit(TEXT("iOS GracefulTerminationHandler"));
    }
    else
    {
        _Exit(0);
    }
}

void FIOSPlatformMisc::PlatformPreInit()
{
    FGenericPlatformMisc::PlatformPreInit();
    
    GIOSAppInfo.Init();
    
    // turn off SIGPIPE crashes
    signal(SIGPIPE, SIG_IGN);
}

// Make sure that SetStoredValue and GetStoredValue generate the same key
static NSString* MakeStoredValueKeyName(const FString& SectionName, const FString& KeyName)
{
	return [NSString stringWithFString:(SectionName + "/" + KeyName)];
}

bool FIOSPlatformMisc::SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];

	// convert input to an NSString
	NSString* StoredValue = [NSString stringWithFString:InValue];

	// store it
	[UserSettings setObject:StoredValue forKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	return true;
}

bool FIOSPlatformMisc::GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];

	// get the stored NSString
	NSString* StoredValue = [UserSettings objectForKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	// if it was there, convert back to FString
	if (StoredValue != nil)
	{
		OutValue = StoredValue;
		return true;
	}

	return false;
}

bool FIOSPlatformMisc::DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];
	
	// Remove it
	[UserSettings removeObjectForKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	return true;
}

bool FIOSPlatformMisc::DeleteStoredSection(const FString& InStoreId, const FString& InSectionName)
{
	bool bRemoved = false;
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];
	NSDictionary<NSString*,id>* KeyValues = [UserSettings dictionaryRepresentation];
	NSString* SectionName = [NSString stringWithFString:InSectionName];

	for (id Key in KeyValues)
	{
		if ([Key hasPrefix:SectionName])
		{
			[UserSettings removeObjectForKey:Key];
			bRemoved = true;
		}
	}

	return bRemoved;
}

void FIOSPlatformMisc::SetGracefulTerminationHandler()
{
    struct sigaction Action;
    FMemory::Memzero(&Action, sizeof(struct sigaction));
    Action.sa_sigaction = GracefulTerminationHandler;
    sigemptyset(&Action.sa_mask);
    Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
    sigaction(SIGINT, &Action, NULL);
    sigaction(SIGTERM, &Action, NULL);
    sigaction(SIGHUP, &Action, NULL);
}

void FIOSPlatformMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context))
{
    SCOPED_AUTORELEASE_POOL;
    
    GCrashHandlerPointer = CrashHandler;
    
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
    if (!FIOSApplicationInfo::CrashReporter && !FIOSApplicationInfo::CrashMalloc)
    {
        // configure the crash handler malloc zone to reserve a little memory for itself
        FIOSApplicationInfo::CrashMalloc = new FIOSMallocCrashHandler(4*1024*1024);
        
        PLCrashReporterConfig* Config = [[[PLCrashReporterConfig alloc] initWithSignalHandlerType: PLCrashReporterSignalHandlerTypeBSD symbolicationStrategy: PLCrashReporterSymbolicationStrategyNone crashReportFolder: FIOSApplicationInfo::TemporaryCrashReportFolder().GetNSString() crashReportName: FIOSApplicationInfo::TemporaryCrashReportName().GetNSString()] autorelease];
        FIOSApplicationInfo::CrashReporter = [[PLCrashReporter alloc] initWithConfiguration: Config];
        
        PLCrashReporterCallbacks CrashReportCallback = {
            .version = 0,
            .context = nullptr,
            .handleSignal = PLCrashReporterHandler
        };
        
        [FIOSApplicationInfo::CrashReporter setCrashCallbacks: &CrashReportCallback];
        
        NSError* Error = nil;
        if ([FIOSApplicationInfo::CrashReporter enableCrashReporterAndReturnError: &Error])
        {
            /* no-op */
        }
        else
        {
            UE_LOG(LogIOS, Log, TEXT("Failed to enable PLCrashReporter: %s"), *FString([Error localizedDescription]));
            UE_LOG(LogIOS, Log, TEXT("Falling back to native signal handlers"));
 
            struct sigaction Action;
            FMemory::Memzero(&Action, sizeof(struct sigaction));
            Action.sa_sigaction = PlatformCrashHandler;
            sigemptyset(&Action.sa_mask);
            Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
            sigaction(SIGQUIT, &Action, NULL);
            sigaction(SIGILL, &Action, NULL);
            sigaction(SIGEMT, &Action, NULL);
            sigaction(SIGFPE, &Action, NULL);
            sigaction(SIGBUS, &Action, NULL);
            sigaction(SIGSEGV, &Action, NULL);
            sigaction(SIGSYS, &Action, NULL);
            sigaction(SIGABRT, &Action, NULL);
        }
    }
#endif
}

bool FIOSPlatformMisc::HasSeparateChannelForDebugOutput()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    // We should not just check if we are being debugged because you can use the Xcode log even for
    // apps launched outside the debugger.
    return true;
#endif
}

void FIOSPlatformMisc::RequestExit(bool Force, const TCHAR* CallSite)
{
	if (Force)
	{
		FApplePlatformMisc::RequestExit(Force, CallSite);
	}
	else
	{
		// ForceExit is sort of a misnomer here.  This will exit the engine loop before calling _Exit() from the app delegate
		[[IOSAppDelegate GetDelegate] ForceExit];
	}
}

void FIOSPlatformMisc::RequestExitWithStatus(bool Force, uint8 ReturnCode, const TCHAR* CallSite)
{
	if (Force)
	{
		FApplePlatformMisc::RequestExit(Force, CallSite);
	}
	else
	{
		// Implementation will ignore the return code - this may be important, so warn.
		UE_LOG(LogIOS, Warning, TEXT("FIOSPlatformMisc::RequestExitWithStatus(%i, %d, %s) - return code will be ignored by the generic implementation."),
			Force, ReturnCode, CallSite ? CallSite : TEXT("<NoCallSiteInfo>"));

		// ForceExit is sort of a misnomer here.  This will exit the engine loop before calling _Exit() from the app delegate
		[[IOSAppDelegate GetDelegate] ForceExit];
	}
}

int32 FIOSPlatformMisc::GetMaxRefreshRate()
{
#if PLATFORM_VISIONOS
	return 60;
#else
	return [UIScreen mainScreen].maximumFramesPerSecond;
#endif
}

void FIOSPlatformMisc::GPUAssert()
{
    // make this a fatal error that ends here not in the log
    // changed to 3 from NULL because clang noticed writing to NULL and warned about it
    *(int32 *)13 = 123;
}

void FIOSPlatformMisc::MetalAssert()
{
    // make this a fatal error that ends here not in the log
    // changed to 3 from NULL because clang noticed writing to NULL and warned about it
    *(int32 *)7 = 123;
}

bool FIOSPlatformMisc::CPUHasHwCrcSupport()
{
	// HW CRC instructions support is available on Apple A10+
	static int HwCrcSupported = -1;
	if (HwCrcSupported == -1)
	{
		HwCrcSupported = 0;
		
		const FString DeviceIDString = GetIOSDeviceIDString();
		if (DeviceIDString.StartsWith(TEXT("iPod")))
		{
			const int Major = FCString::Atoi(&DeviceIDString[4]);
			//iPod Touch 6 and lower don't support hw CRC32
			HwCrcSupported = Major > 7;
		}
		else if (DeviceIDString.StartsWith(TEXT("iPad")))
		{
			// get major revision number
			const int Major = FCString::Atoi(&DeviceIDString[4]);

			//iPad 5, iPad Pro and lower
			HwCrcSupported = Major > 6;
		}
		else if (DeviceIDString.StartsWith(TEXT("iPhone")))
		{
			const int Major = FCString::Atoi(&DeviceIDString[6]);
			
			// iPhone 6S, iPhone SE and below
			HwCrcSupported = Major > 9;
		}
		else if (DeviceIDString.StartsWith(TEXT("AppleTV")))
		{
			const int Major = FCString::Atoi(&DeviceIDString[7]);
			
			// Apple TV
			HwCrcSupported = Major > 5;
		}
	}

	return HwCrcSupported == 1;
}

static FCriticalSection EnsureLock;
static bool bReentranceGuard = false;

void ReportEnsure( const TCHAR* ErrorMessage, int NumStackFramesToIgnore )
{
    // Simple re-entrance guard.
    EnsureLock.Lock();
    
    if( bReentranceGuard )
    {
        EnsureLock.Unlock();
        return;
    }
    
    bReentranceGuard = true;
    
#if !PLATFORM_TVOS && !PLATFORM_VISIONOS
    if(FIOSApplicationInfo::CrashReporter != nil)
    {
        siginfo_t Signal;
        Signal.si_signo = SIGTRAP;
        Signal.si_code = TRAP_TRACE;
        Signal.si_addr = __builtin_return_address(0);
        
        FIOSCrashContext EnsureContext(ECrashContextType::Ensure, ErrorMessage);
        EnsureContext.InitFromSignal(SIGTRAP, &Signal, nullptr);
        EnsureContext.GenerateEnsureInfo();
    }
#endif
    
    bReentranceGuard = false;
    EnsureLock.Unlock();
}

FString FIOSCrashContext::CreateCrashFolder() const
{
	// create a crash-specific directory
	char CrashInfoFolder[PATH_MAX] = {};
	FCStringAnsi::Strncpy(CrashInfoFolder, GIOSAppInfo.CrashReportPath, PATH_MAX);
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "/CrashReport-UE-");
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, GIOSAppInfo.AppNameUTF8);
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-pid-");
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(getpid(), 10));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-");
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.A, 16));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.B, 16));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.C, 16));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GIOSAppInfo.RunUUID.D, 16));
	
	return FString(ANSI_TO_TCHAR(CrashInfoFolder));
}


class FIOSExec : public FSelfRegisteringExec
{
public:
	FIOSExec()
		: FSelfRegisteringExec()
	{
		
	}
	
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("IOS")))
		{
			// commands to override and append commandline options for next boot (see FIOSCommandLineHelper)
			if (FParse::Command(&Cmd, TEXT("OverrideCL")))
			{
				return FPlatformMisc::SetStoredValue(TEXT(""), TEXT("IOSCommandLine"), TEXT("ReplacementCL"), Cmd);
			}
			else if (FParse::Command(&Cmd, TEXT("AppendCL")))
			{
				return FPlatformMisc::SetStoredValue(TEXT(""), TEXT("IOSCommandLine"), TEXT("AppendCL"), Cmd);
			}
			else if (FParse::Command(&Cmd, TEXT("ClearAllCL")))
			{
				return FPlatformMisc::DeleteStoredValue(TEXT(""), TEXT("IOSCommandLine"), TEXT("ReplacementCL")) &&
						FPlatformMisc::DeleteStoredValue(TEXT(""), TEXT("IOSCommandLine"), TEXT("AppendCL"));
			}
		}
		
		return false;
	}
} GIOSExec;
