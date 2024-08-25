// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSRuntimeSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/EngineBuildSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"

#if WITH_EDITOR
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "DesktopPlatformModule.h"
#define LOCTEXT_NAMESPACE "IOSRuntimeSettings"
#endif

UIOSRuntimeSettings::UIOSRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CacheSizeKB(65536)
	, MaxSampleRate(48000)
	, HighSampleRate(32000)
	, MedSampleRate(24000)
	, LowSampleRate(12000)
	, MinSampleRate(8000)
	, CompressionQualityModifier(1)
{
	bEnableGameCenterSupport = true;
	bEnableCloudKitSupport = false;
	bUserSwitching = false;
	bSupportsPortraitOrientation = true;
	bSupportsITunesFileSharing = false;
	bSupportsFilesApp = false;
	BundleDisplayName = TEXT("UnrealGame");
	BundleName = TEXT("MyUnrealGame");
	BundleIdentifier = TEXT("com.YourCompany.GameNameNoSpaces");
	VersionInfo = TEXT("1.0.0");
    FrameRateLock = EPowerUsageFrameRateLock::PUFRL_30;
	bEnableDynamicMaxFPS = false;
	bSupportsIPad = true;
	bSupportsIPhone = true;
	bEnableSplitView = false;
	bEnableSimulatorSupport = false;
	MinimumiOSVersion = EIOSVersion::IOS_Minimum;
    bBuildAsFramework = true;
	bGeneratedSYMFile = false;
	bGeneratedSYMBundle = false;
	bGenerateXCArchive = false;
	bSupportSecondaryMac = false;
	bUseRSync = true;
	bCustomLaunchscreenStoryboard = false;
	AdditionalPlistData = TEXT("");
	AdditionalLinkerFlags = TEXT("");
	AdditionalShippingLinkerFlags = TEXT("");
    bGameSupportsMultipleActiveControllers = false;
	bAllowRemoteRotation = true;
	bDisableMotionData = false;
    bEnableRemoteNotificationsSupport = false;
    bEnableBackgroundFetch = false;
	bSupportsMetal = true;
	bSupportsMetalMRT = false;
    bSupportHighRefreshRates = false;
	bDisableHTTPS = false;
    bSupportsBackgroundAudio = false;
}

void UIOSRuntimeSettings::PostReloadConfig(class FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

#if PLATFORM_IOS

	FPlatformApplicationMisc::SetGamepadsAllowed(bAllowControllers);

#endif //PLATFORM_IOS
}

#if WITH_EDITOR
void UIOSRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that at least one orientation is supported
	if (!bSupportsPortraitOrientation && !bSupportsUpsideDownOrientation && !bSupportsLandscapeLeftOrientation && !bSupportsLandscapeRightOrientation)
	{
		bSupportsPortraitOrientation = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsPortraitOrientation)), GetDefaultConfigFilename());
	}

	// Ensure that at least one API is supported
	if (!bSupportsMetal && !bSupportsMetalMRT)
	{
		bSupportsMetal = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsMetal)), GetDefaultConfigFilename());
	}

	// If iOS Simulator setting changed, need to rerun GPF to force xcconfig files to updated the supported platforms
	if (PropertyChangedEvent.GetPropertyName() == TEXT("bEnableSimulatorSupport") &&
		PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		const FText GPFQueryLoc = LOCTEXT("RunGPFQuery", "Xcode Project will refresh. Continue?");
		if (FMessageDialog::Open(EAppMsgType::OkCancel, GPFQueryLoc) == EAppReturnType::Cancel)
		{
			// User canceled, so reset the value
			bEnableSimulatorSupport = !bEnableSimulatorSupport;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bEnableSimulatorSupport)), GetDefaultConfigFilename());
			return;
		}

		FScopedSlowTask SlowTask(0, LOCTEXT("UpdatingCodeProject", "Updating code project..."));
		SlowTask.MakeDialog();

		// Try to generate project files
		FStringOutputDevice OutputLog;
		OutputLog.SetAutoEmitLineTerminator(true);
		GLog->AddOutputDevice(&OutputLog);
		bool bSuccess = FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::ProjectDir() + FApp::GetProjectName(), GWarn);
		GLog->RemoveOutputDevice(&OutputLog);

		if (!bSuccess)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("LogOutput"), FText::FromString(OutputLog));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CouldNotGenerateProjectFiles", "Project files could not be generated. Please run GPF manually or revert the setting. Log:\n\n{LogOutput}"), Args));
		}
	}
}


void UIOSRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// We can have a look for potential keys
	if (!RemoteServerName.IsEmpty() && !RSyncUsername.IsEmpty())
	{
		SSHPrivateKeyLocation = TEXT("");

		FString RealRemoteServerName = RemoteServerName;
		if(RemoteServerName.Contains(TEXT(":")))
		{
			FString RemoteServerPort;
			RemoteServerName.Split(TEXT(":"),&RealRemoteServerName,&RemoteServerPort);
		}
		const FString DefaultKeyFilename = TEXT("RemoteToolChainPrivate.key");
		const FString RelativeFilePathLocation = FPaths::Combine(TEXT("SSHKeys"), *RealRemoteServerName, *RSyncUsername, *DefaultKeyFilename);

		FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));

		TArray<FString> PossibleKeyLocations;
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::ProjectDir(), TEXT("Restricted"), TEXT("NotForLicensees"), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::ProjectDir(), TEXT("Restricted"), TEXT("NoRedist"), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::ProjectDir(), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Restricted"), TEXT("NotForLicensees"), TEXT("Build"), TEXT("NotForLicensees"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Restricted"), TEXT("NoRedist"), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*Path, TEXT("Unreal Engine"), TEXT("UnrealBuildTool"), *RelativeFilePathLocation));

		// Find a potential path that we will use if the user hasn't overridden.
		// For information purposes only
		for (const FString& NextLocation : PossibleKeyLocations)
		{
			if (IFileManager::Get().FileSize(*NextLocation) > 0)
			{
				SSHPrivateKeyLocation = NextLocation;
				break;
			}
		}
	}

	if ((MinimumiOSVersion < EIOSVersion::IOS_15) && (MinimumiOSVersion != EIOSVersion::IOS_Minimum))
	{
		MinimumiOSVersion = EIOSVersion::IOS_Minimum;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, MinimumiOSVersion)), GetDefaultConfigFilename());
	}
	if (!bSupportsMetal && !bSupportsMetalMRT)
	{
		bSupportsMetal = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsMetal)), GetDefaultConfigFilename());
	}
}

#undef LOCTEXT_NAMESPACE

#endif
