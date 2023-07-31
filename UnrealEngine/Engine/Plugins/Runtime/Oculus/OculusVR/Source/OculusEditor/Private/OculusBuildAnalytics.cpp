// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusBuildAnalytics.h"
#include "GameProjectGenerationModule.h"
#include "OculusHMDModule.h"
#include "Runtime/Core/Public/HAL/FileManager.h"

FOculusBuildAnalytics* FOculusBuildAnalytics::instance = 0;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FOculusBuildAnalytics* FOculusBuildAnalytics::GetInstance()
{
	if (IOculusHMDModule::IsAvailable())
	{
		if (instance == NULL)
		{
			instance = new FOculusBuildAnalytics();
		}
	}

	return instance;
}

bool FOculusBuildAnalytics::IsOculusHMDAvailable()
{
	return IOculusHMDModule::IsAvailable() && FOculusHMDModule::Get().PreInit();
}

void FOculusBuildAnalytics::Shutdown()
{
}

FOculusBuildAnalytics::FOculusBuildAnalytics()
{
	bool TelemetryEnabled = false;
	if (!GConfig->GetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bEnableOculusBuildTelemetry"), TelemetryEnabled, GEditorIni))
	{
		GConfig->SetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bEnableOculusBuildTelemetry"), TelemetryEnabled, GEditorIni);
		GConfig->Flush(0);
	}

	if (TelemetryEnabled)
	{
		RegisterLauncherCallback();
	}
}

void FOculusBuildAnalytics::OnTelemetryToggled(bool Enabled)
{
	if (Enabled)
	{
		RegisterLauncherCallback();
	}
	else
	{
		if (LauncherCallbackHandle.IsValid())
		{
			ILauncherServicesModule& ProjectLauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>("LauncherServices");
			ProjectLauncherServicesModule.OnCreateLauncherDelegate.Remove(LauncherCallbackHandle);
		}
	}
}

void FOculusBuildAnalytics::RegisterLauncherCallback()
{
	ILauncherServicesModule& ProjectLauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>("LauncherServices");
	LauncherCallbackHandle = ProjectLauncherServicesModule.OnCreateLauncherDelegate.AddRaw(this, &FOculusBuildAnalytics::OnLauncherCreated);
}

void FOculusBuildAnalytics::OnLauncherCreated(ILauncherRef Launcher)
{
	// Add callback for when launcher worker is started
	Launcher->FLauncherWorkerStartedDelegate.AddRaw(this, &FOculusBuildAnalytics::OnLauncherWorkerStarted);
}

void FOculusBuildAnalytics::OnLauncherWorkerStarted(ILauncherWorkerPtr LauncherWorker, ILauncherProfileRef Profile)
{
	TArray<FString> Platforms = Profile.Get().GetCookedPlatforms();
	if (Platforms.Num() == 1)
	{
		if (Platforms[0].Equals("Android_ASTC") || Platforms[0].Contains("Windows"))
		{
			CurrentBuildStage = UNDEFINED_STAGE;
			AndroidPackageTime = 0;
			UATLaunched = false;
			BuildCompleted = false;
			CurrentBuildPlatform = Platforms[0];
			TotalBuildTime = 0;
			BuildStepCount = 0;

			FOculusHMDModule::GetPluginWrapper().SetDeveloperMode(true);

			OutputDirectory = Profile.Get().GetPackageDirectory();

			// Assign callbacks for stages
			LauncherWorker.Get()->OnStageCompleted().AddRaw(this, &FOculusBuildAnalytics::OnStageCompleted);
			LauncherWorker.Get()->OnOutputReceived().AddRaw(this, &FOculusBuildAnalytics::OnBuildOutputRecieved);
			LauncherWorker.Get()->OnStageStarted().AddRaw(this, &FOculusBuildAnalytics::OnStageStarted);
			LauncherWorker.Get()->OnCompleted().AddRaw(this, &FOculusBuildAnalytics::OnCompleted);

			// Get information on what oculus platform we are building for and also the OS platform
			FString OculusPlatform;
			if (CurrentBuildPlatform.Equals("Android_ASTC"))
			{
				UEnum* OculusMobileDevices = StaticEnum<EOculusMobileDevice::Type>();
				UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
				TArray<TEnumAsByte<EOculusMobileDevice::Type>> TargetOculusDevices = Settings->PackageForOculusMobile;
				TArray<FString> Devices;

				if (TargetOculusDevices.Contains(EOculusMobileDevice::Quest))
				{
					Devices.Add("quest");
				}
				if (TargetOculusDevices.Contains(EOculusMobileDevice::Quest2))
				{
					Devices.Add("quest2");
				}
				OculusPlatform = FString::Join(Devices, TEXT("_"));
			}
			else if (CurrentBuildPlatform.Contains("Windows"))
			{
				CurrentBuildPlatform = "Windows";
				OculusPlatform = "rift";
			}

			// Count user asset files
			UserAssetCount = 0;
			TArray<FString> FileNames;
			IFileManager::Get().FindFilesRecursive(FileNames, *FPaths::ProjectContentDir(), TEXT("*.*"), true, false, false);
			UserAssetCount = FileNames.Num();

			// Count user script files
			FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
			SourceFileCount = 0;
			SourceFileDirectorySize = 0;
			GameProjectModule.Get().GetProjectSourceDirectoryInfo(SourceFileCount, SourceFileDirectorySize);

			// Generate build GUID
			FGuid guid = FGuid::NewGuid();
			FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("build_guid", TCHAR_TO_ANSI(*guid.ToString()));

			// Send build start event with corresponding metadata
			FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("asset_count", TCHAR_TO_ANSI(*FString::FromInt(UserAssetCount)));
			FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("script_count", TCHAR_TO_ANSI(*FString::FromInt(SourceFileCount)));

			FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("target_platform", TCHAR_TO_ANSI(*CurrentBuildPlatform));
			FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("target_oculus_platform", TCHAR_TO_ANSI(*OculusPlatform));

			TArray<ILauncherTaskPtr> TaskList;
			LauncherWorker->GetTasks(TaskList);
			BuildStepCount = TaskList.Num();
		}
	}
}

void FOculusBuildAnalytics::OnCompleted(bool Succeeded, double TotalTime, int32 ErrorCode)
{
	if (!BuildCompleted && Succeeded)
	{
		SendBuildCompleteEvent(TotalTime);
	}
}

void FOculusBuildAnalytics::OnStageCompleted(const FString& StageName, double Time)
{
	if (CurrentBuildStage != UNDEFINED_STAGE)
	{
		FString TaskName;
		switch (CurrentBuildStage)
		{
		case COOK_IN_EDITOR_STAGE:	TaskName = "build_step_editor_cook";	break;
		case LAUNCH_UAT_STAGE:		TaskName = "build_step_launch_uat";		break;
		case COMPILE_STAGE:			TaskName = "build_step_compile";		break;
		case COOK_STAGE:			TaskName = "build_step_cook";			break;
		case DEPLOY_STAGE:			TaskName = "build_step_deploy";			break;
		case PACKAGE_STAGE:			TaskName = "build_step_package";		break;
		case RUN_STAGE:				return;
		default:					TaskName = "build_step_undefined";		break;
		}

		if (AndroidPackageTime > 0)
		{
			Time -= AndroidPackageTime;
		}

		TotalBuildTime += Time;
		FOculusHMDModule::GetPluginWrapper().SendEvent2(TCHAR_TO_ANSI(*TaskName), TCHAR_TO_ANSI(*FString::SanitizeFloat(Time)), "ovrbuild");
	}
}

void FOculusBuildAnalytics::OnStageStarted(const FString& StageName)
{
	if (StageName.Equals("Cooking in the editor"))
	{
		CurrentBuildStage = COOK_IN_EDITOR_STAGE;
	}
	else if (StageName.Equals("Build Task"))
	{
		CurrentBuildStage = COMPILE_STAGE;
	}
	else if (StageName.Equals("Launch Task"))
	{
		CurrentBuildStage = LAUNCH_UAT_STAGE;
	}
	else if (StageName.Equals("Cook Task"))
	{
		CurrentBuildStage = COOK_STAGE;
	}
	else if (StageName.Equals("Package Task"))
	{
		CurrentBuildStage = PACKAGE_STAGE;
	}
	else if (StageName.Equals("Deploy Task"))
	{
		CurrentBuildStage = DEPLOY_STAGE;
	}
	else if (StageName.Equals("Run Task"))
	{
		CurrentBuildStage = RUN_STAGE;
		SendBuildCompleteEvent(TotalBuildTime);
		BuildCompleted = true;
	}
	else
	{
		CurrentBuildStage = UNDEFINED_STAGE;
	}
}

void FOculusBuildAnalytics::OnBuildOutputRecieved(const FString& Message)
{
	if (CurrentBuildPlatform.Equals("Android_ASTC") && (CurrentBuildStage == DEPLOY_STAGE || CurrentBuildStage == PACKAGE_STAGE))
	{
		if (Message.Contains("BUILD SUCCESSFUL"))
		{
			FString Text, Time;
			Message.Split("in", &Text, &Time);

			if (!Time.IsEmpty())
			{
				FString SMinutes, SSeconds;
				if (Time.Contains("m"))
				{
					Time.Split("m", &SMinutes, &SSeconds);
				}
				else
				{
					SSeconds = Time;
				}

				int Minutes = FCString::Atoi(*SMinutes);
				int Seconds = FCString::Atoi(*SSeconds);

				AndroidPackageTime = Minutes * 60 + Seconds;

				FOculusHMDModule::GetPluginWrapper().SendEvent2("build_step_gradle_build", TCHAR_TO_ANSI(*FString::SanitizeFloat(AndroidPackageTime)), "ovrbuild");
			}
		}
	}
}

void FOculusBuildAnalytics::SendBuildCompleteEvent(float TotalTime)
{
	if (CurrentBuildPlatform.Equals("Android_ASTC"))
	{
		int64 APKTotalSize = 0;
		TArray<FString> FoundAPKs;
		OutputDirectory = FPaths::ProjectDir() + "Binaries/Android";
		OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
		IFileManager::Get().FindFiles(FoundAPKs, *FPaths::Combine(OutputDirectory, TEXT("*.apk")), true, false);

		FDateTime LatestTime = FDateTime(0);
		FString LatestAPK;
		for (int i = 0; i < FoundAPKs.Num(); i++)
		{
			FDateTime APKCreationTime = IFileManager::Get().GetTimeStamp(*FPaths::Combine(OutputDirectory, FoundAPKs[i]));
			if (APKCreationTime > LatestTime)
			{
				LatestTime = APKCreationTime;
				LatestAPK = FoundAPKs[i];
			}
		}

		TArray<FString> FoundOBBs;
		LatestTime = FDateTime(0);
		FString LatestOBB;
		IFileManager::Get().FindFiles(FoundOBBs, *FPaths::Combine(OutputDirectory, TEXT("*.obb")), true, false);
		for (int i = 0; i < FoundOBBs.Num(); i++)
		{
			FDateTime OBBCreationTime = IFileManager::Get().GetTimeStamp(*FPaths::Combine(OutputDirectory, FoundOBBs[i]));
			if (OBBCreationTime > LatestTime)
			{
				LatestTime = OBBCreationTime;
				LatestOBB = FoundOBBs[i];
			}
		}

		if (!LatestAPK.IsEmpty())
		{
			APKTotalSize += IFileManager::Get().FileSize(*FPaths::Combine(OutputDirectory, LatestAPK));
		}
		if (!LatestOBB.IsEmpty())
		{
			APKTotalSize += IFileManager::Get().FileSize(*FPaths::Combine(OutputDirectory, LatestOBB));
		}

		if (APKTotalSize > 0)
		{
			FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("build_output_size", TCHAR_TO_ANSI(*FString::FromInt(APKTotalSize)));
		}
	}

	FOculusHMDModule::GetPluginWrapper().AddCustomMetadata("build_step_count", TCHAR_TO_ANSI(*FString::FromInt(BuildStepCount)));
	FOculusHMDModule::GetPluginWrapper().SendEvent2("build_complete", TCHAR_TO_ANSI(*FString::SanitizeFloat(TotalTime)), "ovrbuild");
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
