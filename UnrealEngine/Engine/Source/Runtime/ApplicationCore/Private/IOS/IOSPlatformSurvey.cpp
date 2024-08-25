// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformSurvey.cpp: HardwareSurvey implementation
=============================================================================*/

#include "IOS/IOSPlatformSurvey.h"
#include "IOS/IOSAppDelegate.h"
#include "IOSWindow.h"

bool FIOSPlatformSurvey::GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait)
{
	FCString::Strcpy(OutResults.Platform, FPlatformMisc::GetDefaultDeviceProfileName());
	FCString::Strcpy(OutResults.OSVersion, *FString([[UIDevice currentDevice] systemVersion]));
#if PLATFORM_64BITS
	OutResults.OSBits = 64;
#else
	OutResults.OSBits = 32;
#endif
	FCString::Strcpy(OutResults.OSLanguage, *FString([[NSLocale preferredLanguages] objectAtIndex:0]));
	FCString::Strcpy(OutResults.RenderingAPI, TEXT("Metal"));
	OutResults.CPUCount = FPlatformMisc::NumberOfCores();

#if !PLATFORM_VISIONOS
	// display 0 is max size
	CGRect MainFrame = [[UIScreen mainScreen] bounds];
	double Scale = [[UIScreen mainScreen] scale];
	OutResults.Displays[0].CurrentModeWidth = FMath::TruncToInt(MainFrame.size.width * Scale);
	OutResults.Displays[0].CurrentModeHeight = FMath::TruncToInt(MainFrame.size.height * Scale);

	// display 1 is current size
	FPlatformRect ScreenSize = FIOSWindow::GetScreenRect();
	OutResults.Displays[1].CurrentModeWidth = ScreenSize.Right - ScreenSize.Left;
	OutResults.Displays[1].CurrentModeHeight = ScreenSize.Bottom - ScreenSize.Top;
#endif
	return true;
}
