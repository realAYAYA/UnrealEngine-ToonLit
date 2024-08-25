// Copyright Epic Games, Inc. All Rights Reserved.

#include "Launcher/LauncherWorker.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "ISourceCodeAccessor.h"
#include "ISourceCodeAccessModule.h"
#include "ITargetDeviceProxy.h"
#include "ITargetDeviceProxyManager.h"
#include "ITurnkeyIOModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"
#include "Modules/ModuleManager.h"
#include "Launcher/LauncherTaskChainState.h"
#include "Launcher/LauncherTask.h"
#include "Launcher/LauncherUATTask.h"
#include "PlatformInfo.h"
#include "Misc/ConfigCacheIni.h"
#include "Profiles/LauncherProfile.h"
#include "DerivedDataCacheInterface.h"


#define LOCTEXT_NAMESPACE "LauncherWorker"


/* Static class member instantiations
*****************************************************************************/

FThreadSafeCounter FLauncherTask::TaskCounter;


/* FLauncherWorker structors
 *****************************************************************************/

FLauncherWorker::FLauncherWorker(const TSharedRef<ITargetDeviceProxyManager>& InDeviceProxyManager, const ILauncherProfileRef& InProfile)
	: DeviceProxyManager(InDeviceProxyManager)
	, Profile(InProfile)
{
	CreateAndExecuteTasks(InProfile);
}


/* FRunnable overrides
 *****************************************************************************/

bool FLauncherWorker::Init( )
{
	return true;
}


uint32 FLauncherWorker::Run( )
{
	FString Line;

	LaunchStartTime = FPlatformTime::Seconds();

	auto MessageReceived = [this](const FString& InMessage)
	{
		FStringView MessageView = InMessage;
		{
			FStringView PackageDevicePrefix = TEXTVIEW("Running Package@Device:");
			if (MessageView.StartsWith(PackageDevicePrefix))
			{
				FStringView Value = MessageView.RightChop(PackageDevicePrefix.Len());
				int32 SplitIndex;
				if (Value.FindChar('@', SplitIndex))
				{
					FString Package(Value.SubStr(0, SplitIndex));
					FString Device(Value.SubStr(SplitIndex + 1, Value.Len()));
					AddDevicePackagePair(Device, Package);
				}
			}
		}
		OutputMessageReceived.Broadcast(InMessage);
	};

	// wait for tasks to be completed
	while (Status == ELauncherWorkerStatus::Busy)
	{
		FPlatformProcess::Sleep(0.01f);

		FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
		if (NewLine.Len() > 0)
		{
			// process the string to break it up in to lines
			Line += NewLine;
			TArray<FString> StringArray;
			int32 count = Line.ParseIntoArray(StringArray, TEXT("\n"), true);
			if (count > 1)
			{
				for (int32 Index = 0; Index < count-1; ++Index)
				{
					StringArray[Index].TrimEndInline();
					MessageReceived(StringArray[Index]);
				}
				Line = StringArray[count-1];
				if (NewLine.EndsWith(TEXT("\n")))
				{
					Line += TEXT("\n");
				}
			}
		}

		if (TaskChain->IsChainFinished())
		{
			Status = ELauncherWorkerStatus::Completed;

			NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			while (NewLine.Len() > 0)
			{
				// process the string to break it up in to lines
				Line += NewLine;
				TArray<FString> StringArray;
				int32 count = Line.ParseIntoArray(StringArray, TEXT("\n"), true);
				if (count > 1)
				{
					for (int32 Index = 0; Index < count-1; ++Index)
					{
						StringArray[Index].TrimEndInline();
						MessageReceived(StringArray[Index]);
					}
					Line = StringArray[count-1];
					if (NewLine.EndsWith(TEXT("\n")))
					{
						Line += TEXT("\n");
					}
				}

				NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			}

			// fire off the last line
			MessageReceived(Line);

		}
	}

	// wait for tasks to be canceled
	if (Status == ELauncherWorkerStatus::Canceling)
	{
		// kill the uat process tree
		FPlatformProcess::TerminateProc(ProcHandle, true);
		// kill any lingering target processes left after killing uat
		TerminateLaunchedProcess();

		TaskChain->Cancel();

		while (!TaskChain->IsChainFinished())
		{
			FPlatformProcess::Sleep(0.0);
		}		
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (Status == ELauncherWorkerStatus::Canceling)
	{
		LaunchCanceled.Broadcast(FPlatformTime::Seconds() - LaunchStartTime);
		Status = ELauncherWorkerStatus::Canceled;
	}
	else
	{
		LaunchCompleted.Broadcast(TaskChain->Succeeded(), FPlatformTime::Seconds() - LaunchStartTime, TaskChain->ReturnCode());
	}

	//delete the application@device dictionary
	CachedDevicePackagePair.Empty();

	//stop looking for disconnected devices
	DisableDeviceDiscoveryListener();

	return 0;
}


void FLauncherWorker::Stop( )
{
	Cancel();
}


/* ILauncherWorker overrides
 *****************************************************************************/

void FLauncherWorker::Cancel( )
{
	if (Status == ELauncherWorkerStatus::Busy)
	{
		Status = ELauncherWorkerStatus::Canceling;
	}
}


void FLauncherWorker::CancelAndWait( )
{
	if (Status == ELauncherWorkerStatus::Busy)
	{
		Status = ELauncherWorkerStatus::Canceling;
		while (Status != ELauncherWorkerStatus::Canceled)
		{
			FPlatformProcess::Sleep(0);
		}
	}
}


int32 FLauncherWorker::GetTasks( TArray<ILauncherTaskPtr>& OutTasks ) const
{
	OutTasks.Reset();

	if (TaskChain.IsValid())
	{
		TQueue<TSharedPtr<FLauncherTask> > Queue;

		Queue.Enqueue(TaskChain);

		TSharedPtr<FLauncherTask> Task;

		// breadth first traversal
		while (Queue.Dequeue(Task))
		{
			OutTasks.Add(Task);

			const TArray<TSharedPtr<FLauncherTask> >& Continuations = Task->GetContinuations();

			for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
			{
				Queue.Enqueue(Continuations[ContinuationIndex]);
			}
		}
	}

	return OutTasks.Num();
}


void FLauncherWorker::OnTaskStarted(const FString& TaskName)
{
	StageStartTime = FPlatformTime::Seconds();
	StageStarted.Broadcast(TaskName);
	// look for disconnected devices only after displaying "Running on..."
	if(TaskName.Contains(TEXT("Run Task")))
	{
		EnableDeviceDiscoveryListener();
	}
}


void FLauncherWorker::OnTaskCompleted(const FString& TaskName)
{
	StageCompleted.Broadcast(TaskName, FPlatformTime::Seconds() - StageStartTime);
}

static void AddDeviceToLaunchCommand(const FString& DeviceId, TSharedPtr<ITargetDeviceProxy> DeviceProxy, const ILauncherProfileRef& InProfile, FString& DeviceNames, FString& RoleCommands, bool& bVsyncAdded)
{
	// add the platform
	DeviceNames += TEXT("+\"") + DeviceId + TEXT("\"");
	TArray<ILauncherProfileLaunchRolePtr> Roles;
	if (InProfile->GetLaunchRolesFor(DeviceId, Roles) > 0)
	{
		for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); RoleIndex++)
		{
			if (!bVsyncAdded && Roles[RoleIndex]->IsVsyncEnabled())
			{
				RoleCommands += TEXT(" -vsync");
				bVsyncAdded = true;
			}
			RoleCommands += *(TEXT(" ") + Roles[RoleIndex]->GetUATCommandLine());
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("nomcp")))
	{
		// if our editor has nomcp then pass it through the launched game
		RoleCommands += TEXT(" -nomcp");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("opengl")))
	{
		RoleCommands += TEXT(" -opengl");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("d3d11")) || FParse::Param(FCommandLine::Get(), TEXT("dx11")))
	{
		RoleCommands += TEXT(" -d3d11");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12")))
	{
		RoleCommands += TEXT(" -d3d12");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("es31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")))
	{
		RoleCommands += TEXT(" -es31");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("sm5")))
	{
		RoleCommands += TEXT(" -sm5");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("sm6")))
	{
		RoleCommands += TEXT(" -sm6");
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("vulkan")))
	{
		FName Variant = DeviceProxy->GetTargetDeviceVariant(DeviceId);
		FString Platform = DeviceProxy->GetTargetPlatformName(Variant);

		bool bCookedVulkan = false;
		bool bCheckTargetedRHIs = false;
		TArray<FString> TargetedShaderFormats;

		if (Platform.StartsWith(TEXT("Windows")))
		{
			FConfigFile WindowsEngineSettings;
			FConfigCacheIni::LoadLocalIniFile(WindowsEngineSettings, TEXT("Engine"), true, TEXT("Windows"));

			bCheckTargetedRHIs = true;
			WindowsEngineSettings.GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("VulkanTargetedShaderFormats"), TargetedShaderFormats);

			TArray<FString> OldConfigShaderFormats;
			WindowsEngineSettings.GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("TargetedRHIs"), OldConfigShaderFormats);

			for (const FString& OldConfigShaderFormat : OldConfigShaderFormats)
			{
				TargetedShaderFormats.AddUnique(OldConfigShaderFormat);
			}
		}
		else if (Platform.StartsWith(TEXT("Linux")))
		{
			FConfigFile LinuxEngineSettings;
			FConfigCacheIni::LoadLocalIniFile(LinuxEngineSettings, TEXT("Engine"), true, TEXT("Linux"));

			bCheckTargetedRHIs = true;
			LinuxEngineSettings.GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats);
		}
		else if (Platform.StartsWith(TEXT("Android")))
		{
			FConfigFile AndroidEngineSettings;
			FConfigCacheIni::LoadLocalIniFile(AndroidEngineSettings, TEXT("Engine"), true, TEXT("Android"));

			bool bAndroidSupportsVulkan, bAndroidSupportsVulkanSM5;
			AndroidEngineSettings.GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bAndroidSupportsVulkan);
			AndroidEngineSettings.GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkanSM5"), bAndroidSupportsVulkanSM5);
			bCookedVulkan = bAndroidSupportsVulkan || bAndroidSupportsVulkanSM5;
			bCheckTargetedRHIs = false;
		}

		if (bCheckTargetedRHIs)
		{
			for (const FString& ShaderFormat : TargetedShaderFormats)
			{
				if (ShaderFormat.StartsWith(TEXT("SF_VULKAN_")))
				{
					bCookedVulkan = true;
				}
			}
		}

		if (bCookedVulkan)
		{
			RoleCommands += TEXT(" -vulkan");
		}
		else
		{
			UE_LOG(LogLauncherProfile, Warning, TEXT("The editor is running on Vulkan, but Vulkan is not enabled for launch platform '%s'. Launching process with the default RHI."), *Platform);
		}
	}
}

static FString Join(const TSet<FString>& Tokens, const FString& Delimeter)
{
	FString Result;
	for (const FString& Token : Tokens)
	{
		Result+= Delimeter;
		Result+= Token;
	}

	return Result.RightChop(Delimeter.Len());
}

FString FLauncherWorker::CreateUATCommand( const ILauncherProfileRef& InProfile, const TArray<FString>& InPlatforms, TArray<FCommandDesc>& OutCommands, FString& CommandStart )
{
	CommandStart = TEXT("");
	FString UATCommand = TEXT(" -utf8output");
	FGuid SessionId(FGuid::NewGuid());
	FString InitialMap = InProfile->GetDefaultLaunchRole()->GetInitialMap();
	if (InitialMap.IsEmpty() && InProfile->GetCookedMaps().Num() == 1)
	{
		InitialMap = InProfile->GetCookedMaps()[0];
	}

	// staging directory
	FString StageDirectory = TEXT("");
	auto PackageDirectory = InProfile->GetPackageDirectory();
	if (PackageDirectory.Len() > 0)
	{
		StageDirectory += FString::Printf(TEXT(" -stagingdirectory=\"%s\""), *PackageDirectory);
	}

	// determine if there is a server platform
	FString ServerCommand = TEXT("");
	FString ServerPlatforms = TEXT("");
	FString Platforms = TEXT("");
	FString PlatformCommand = TEXT("");
	FString OptionalParams = TEXT("");
	TSet<FString> OptionalTargetPlatforms;
	TSet<FString> OptionalCookFlavors;
	
	bool bUATClosesAfterLaunch = false;
	for (int32 PlatformIndex = 0; PlatformIndex < InPlatforms.Num(); ++PlatformIndex)
	{
		// Platform info for the given platform
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(*InPlatforms[PlatformIndex]));

		if (ensure(PlatformInfo))
		{
			// separate out Server platforms
			FString& PlatformString = (PlatformInfo->PlatformType == EBuildTargetType::Server) ? ServerPlatforms : Platforms;

			PlatformString += TEXT("+");
			PlatformString += PlatformInfo->DataDrivenPlatformInfo->UBTPlatformString;

			// Append any extra UAT flags specified for this platform flavor
			if (!PlatformInfo->UATCommandLine.IsEmpty())
			{
				FString OptionalUATCommandLine = PlatformInfo->UATCommandLine;

				FString OptionalTargetPlatform;
				if (FParse::Value(*OptionalUATCommandLine, TEXT("-targetplatform="), OptionalTargetPlatform))
				{
					OptionalTargetPlatforms.Add(OptionalTargetPlatform);
					OptionalUATCommandLine.ReplaceInline(*(TEXT("-targetplatform=") + OptionalTargetPlatform), TEXT(""));
				}

				FString OptionalCookFlavor;
				if (FParse::Value(*OptionalUATCommandLine, TEXT("-cookflavor="), OptionalCookFlavor))
				{
					OptionalCookFlavors.Add(OptionalCookFlavor);
					OptionalUATCommandLine.ReplaceInline(*(TEXT("-cookflavor=") + OptionalCookFlavor), TEXT(""));
				}

				OptionalParams += TEXT(" ");
				OptionalParams += OptionalUATCommandLine;
			}
			bUATClosesAfterLaunch |= PlatformInfo->DataDrivenPlatformInfo->bUATClosesAfterLaunch;
		}
	}

	// If both Client/Game and Server are desired to be built avoid Server causing clients/game to not be built PlatformInfo wise
	if (ServerPlatforms.Len() > 0 && Platforms.Len() > 0 && OptionalParams.Contains(TEXT("-noclient")))
	{
		OptionalParams = OptionalParams.Replace(TEXT("-noclient"), TEXT(""));
	}

	if (ServerPlatforms.Len() > 0)
	{
		ServerCommand = TEXT(" -server -serverplatform=") + ServerPlatforms.RightChop(1);
		if (Platforms.Len() == 0)
		{
			OptionalParams += TEXT(" -noclient");
		}
	}
	if (Platforms.Len() > 0)
	{
		PlatformCommand = TEXT(" -platform=") + Platforms.RightChop(1);
	}
	
	UATCommand += PlatformCommand;
	UATCommand += ServerCommand;
	UATCommand += OptionalParams;

	if (OptionalTargetPlatforms.Num() > 0)
	{
		UATCommand += (TEXT(" -targetplatform=") + Join(OptionalTargetPlatforms, TEXT("+")));
	}
	
	if (OptionalCookFlavors.Num() > 0)
	{
		UATCommand += (TEXT(" -cookflavor=") + Join(OptionalCookFlavors, TEXT("+")));
	}

	if (InProfile->GetBuildTarget().Len() > 0)
	{
		UATCommand += TEXT(" -target=") + InProfile->GetBuildTarget();
	}

	if (InProfile->IsDeviceASimulator())
	{
		if (Platforms.Contains(TEXT("IOS")))
		{
			UATCommand += TEXT(" -clientarchitecture=iossimulator");
		}
		// TODO: add tvOS and VisionOS simulators below
	}

	// device list
	FString DeviceNames = TEXT("");
	FString DeviceCommand = TEXT("");
	FString RoleCommands = TEXT("");
	ILauncherDeviceGroupPtr DeviceGroup = InProfile->GetDeployedDeviceGroup();

	bool bVsyncAdded = false;

	if (DeviceGroup.IsValid())
	{
		const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();

		if (Devices.Num() > 0)
		{
			// for each deployed device...
			for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
			{
				const FString& DeviceId = Devices[DeviceIndex];
				TSharedPtr<ITargetDeviceProxy> DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);
				if (DeviceProxy.IsValid())
				{
					AddDeviceToLaunchCommand(DeviceId, DeviceProxy, InProfile, DeviceNames, RoleCommands, bVsyncAdded);

					// also add the credentials, if necessary
					FString DeviceUser = DeviceProxy->GetDeviceUser();
					if (DeviceUser.Len() > 0)
					{
						DeviceCommand += FString::Printf(TEXT(" -deviceuser=%s"), *DeviceUser);
					}

					FString DeviceUserPassword = DeviceProxy->GetDeviceUserPassword();
					if (DeviceUserPassword.Len() > 0)
					{
						DeviceCommand += FString::Printf(TEXT(" -devicepass=%s"), *DeviceUserPassword);
					}
				}
			}
		}
		else
		{
			RoleCommands = InProfile->GetDefaultLaunchRole()->GetUATCommandLine();
		}
	}

	if (DeviceNames.Len() > 0)
	{
		DeviceCommand += TEXT(" -device=") + DeviceNames.RightChop(1);
	}

	// game command line
	FString CommandLine = FString::Printf(TEXT(" -cmdline=\"%s -Messaging\""),
		*InitialMap);

	// localization command line
	FString LocalizationCommands;
#if WITH_EDITOR
	const FString PreviewGameLanguage = FTextLocalizationManager::Get().GetConfiguredGameLocalizationPreviewLanguage();
	if (!PreviewGameLanguage.IsEmpty())
	{
		LocalizationCommands += TEXT(" -culture=");
		LocalizationCommands += PreviewGameLanguage;
	}
#endif	// WITH_EDITOR

	// to reduce UECommandLine.txt churn (timestamp causing extra work), for LaunchOn (ie iterative deploy) we use a single session guid
	if (InProfile->GetDeploymentMode() == ELauncherProfileDeploymentModes::CopyToDevice && Profile->IsDeployingIncrementally())
	{
		static FGuid StaticGuid(FGuid::NewGuid());
		SessionId = StaticGuid;
	}

	// additional commands to be sent to the commandline
	FString SessionName = InProfile->GetName().Replace(TEXT("\'"), TEXT("_")).Replace(TEXT("\'"), TEXT("_"));
	FString SessionOwner = FString(FPlatformProcess::UserName(false)).Replace(TEXT("\'"), TEXT("_")).Replace(TEXT("\'"), TEXT("_"));;
	FString AdditionalCommandLine = FString::Printf(TEXT(" -addcmdline=\"-SessionId=%s -SessionOwner='%s' -SessionName='%s'%s%s %s\""),
		*SessionId.ToString(),
		*SessionOwner,
		*SessionName,
		*RoleCommands,
		*LocalizationCommands,
		*InProfile->GetAdditionalCommandLineParameters());

	// map list
	FString MapList;
	const TArray<FString>& CookedMaps = InProfile->GetCookedMaps();
	if (CookedMaps.Num() > 0 && (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBook || InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor))
	{
		if (!InitialMap.IsEmpty())
		{
			MapList += InitialMap;
			MapList += TEXT("+");
		}
		MapList += FString::Join(CookedMaps, TEXT("+"));
	}
	else
	{
		MapList = InitialMap;
	}
	if (!MapList.IsEmpty())
	{
		MapList = FString(TEXT(" -map=")) + MapList;
	}

	// culture list
	FString CultureList;
	{
		const TArray<FString>& CookedCultures = InProfile->GetCookedCultures();
		if (CookedCultures.Num() > 0 && (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBook || InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor))
		{
			CultureList += TEXT(" -CookCultures=");
			CultureList += FString::Join(CookedCultures, TEXT("+"));
		}
	}

	bool bIsBuilding = InProfile->ShouldBuild();

	// build
	if (bIsBuilding)
	{
		UATCommand += TEXT(" -build");

		FCommandDesc Desc;
		FText Command = FText::Format(LOCTEXT("LauncherBuildDesc", "Build game for {0}"), FText::FromString(Platforms.RightChop(1)));
		Desc.Name = "Build Task";
		Desc.Desc = Command.ToString();
		Desc.EndText = TEXT("********** BUILD COMMAND COMPLETED **********");
		OutCommands.Add(Desc);
		CommandStart = TEXT("********** BUILD COMMAND STARTED **********");
		// @todo: server
	}

	// cook
	switch(InProfile->GetCookMode())
	{
	case ELauncherProfileCookModes::ByTheBook:
		{
			UATCommand += TEXT(" -cook");

			UATCommand += MapList;
			UATCommand += CultureList;

			if (InProfile->IsCookingUnversioned())
			{
				UATCommand += TEXT(" -unversionedcookedcontent");
			}

			if (InProfile->IsEncryptingIniFiles())
			{
				UATCommand += TEXT(" -encryptinifiles");
			}

			FString AdditionalOptions = InProfile->GetCookOptions();
			if (!AdditionalOptions.IsEmpty())
			{
				UATCommand += TEXT(" -additionalcookeroptions=\"");

				// Escape any quotes in the argument list
				UATCommand += AdditionalOptions.Replace(TEXT("\""), TEXT("\\\""));

				// If the additional options ends with a slash, make sure we don't escape the quote
				if (UATCommand.EndsWith("\\"))
				{
					UATCommand += TEXT("\\");
				}

				UATCommand += TEXT("\"");
			}

			if (FParse::Param(FCommandLine::Get(), TEXT("fastcook")))
			{
				// if our editor has nomcp then pass it through the launched game
				UATCommand += TEXT(" -fastcook");
			}

			if (InProfile->IsUsingZenStore())
			{
				// TODO: launch the zen server from the client once the external CBTB is done
				// -fileserver tells UAT to take the cotf/fileserver path and stage a thin client that loads data via the network
				// -skipserver prevents UAT from launching a COTF server for this CBTB scenario
				UATCommand += TEXT(" -zenstore -fileserver -skipserver");
			}

			if (FDerivedDataCacheInterface* DDC = TryGetDerivedDataCache())
			{
				const TCHAR* GraphName = DDC->GetGraphName();
				if (FCString::Strcmp(GraphName, DDC->GetDefaultGraphName()))
				{
					UATCommand += FString::Printf(TEXT(" -DDC=%s"), GraphName);
				}
			}

			if (InProfile->IsPackingWithUnrealPak())
			{
				UATCommand += TEXT(" -pak");
				if (InProfile->IsUsingIoStore())
				{
					UATCommand += TEXT(" -iostore");
				}
				if (InProfile->IsCompressed())
				{
					UATCommand += TEXT(" -compressed");
				}
			}

			if (InProfile->MakeBinaryConfig())
			{
				UATCommand += TEXT(" -makebinaryconfig");
			}

			if ( InProfile->IsCreatingReleaseVersion() )
			{
				UATCommand += TEXT(" -createreleaseversion=");
				UATCommand += InProfile->GetCreateReleaseVersionName();
				
			}

			if ( InProfile->IsCreatingDLC() )
			{
				UATCommand += TEXT(" -dlcname=");
				UATCommand += InProfile->GetDLCName();
			}

			if ( InProfile->IsDLCIncludingEngineContent() )
			{
				UATCommand += TEXT(" -DLCIncludeEngineContent");
			}

			if ( InProfile->IsGeneratingPatch() )
			{
				UATCommand += TEXT(" -generatepatch");

				if ( InProfile->ShouldAddPatchLevel() )
				{
					UATCommand += TEXT(" -addpatchlevel");
				}
			}

			if ( InProfile->IsGeneratingPatch() || 
				InProfile->IsCreatingReleaseVersion() || 
				InProfile->IsCreatingDLC() )
			{
				if ( InProfile->GetBasedOnReleaseVersionName().IsEmpty() == false )
				{
					UATCommand += TEXT(" -basedonreleaseversion=");
					UATCommand += InProfile->GetBasedOnReleaseVersionName();

					if ( InProfile->ShouldStageBaseReleasePaks() )
					{
						UATCommand += TEXT(" -stagebasereleasepaks");
					}
				}

				if (InProfile->GetOriginalReleaseVersionName().IsEmpty() == false)
				{
					UATCommand += TEXT(" -originalreleaseversion=");
					UATCommand += InProfile->GetOriginalReleaseVersionName();
				}
			}

			if (InProfile->IsGeneratingChunks())
			{
				UATCommand += TEXT(" -manifests");
			}

			if (InProfile->IsGenerateHttpChunkData())
			{
				auto Cmd = FString::Printf(TEXT(" -createchunkinstall -chunkinstalldirectory=\"%s\" -chunkinstallversion=\"%s\""), *InProfile->GetHttpChunkDataDirectory(), *InProfile->GetHttpChunkDataReleaseName());
				UATCommand += Cmd;
			}
			
			// Creating a packed DLC requires staging
			if (InProfile->GetPackagingMode() == ELauncherProfilePackagingModes::DoNotPackage && InProfile->IsCreatingDLC() && InProfile->IsPackingWithUnrealPak())
			{
				UATCommand += TEXT(" -stage");
			}

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherCookDesc", "Cook content for {0}"), FText::FromString(Platforms.RightChop(1)));
			Desc.Name = "Cook Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** COOK COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** COOK COMMAND STARTED **********");
			}
		}
		break;
	case ELauncherProfileCookModes::OnTheFly:
		{
			UATCommand += TEXT(" -cookonthefly");
			
			if (InProfile->IsUsingZenStore())
			{
				UATCommand += TEXT(" -zenstore");
			}

			if (FDerivedDataCacheInterface* DDC = GetDerivedDataCache())
			{
				UATCommand += FString::Printf(TEXT(" -ddc=%s"), DDC->GetGraphName());
			}

			//if UAT doesn't stick around as long as the process we are going to run, then we can't kill the COTF server when UAT goes down because the program
			//will still need it.  If UAT DOES stick around with the process then we DO want the COTF server to die with UAT so the next time we launch we don't end up
			//with two COTF servers.
			if (bUATClosesAfterLaunch)
			{
				UATCommand += " -nokill";
			}
			UATCommand += MapList;

			FCommandDesc Desc;
			FText Command = LOCTEXT("LauncherCookOnTheFlyDesc", "Starting cook on the fly server");
			Desc.Name = "Cook Server Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** COOK COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** COOK COMMAND STARTED **********");
			}
		}
		break;
	case ELauncherProfileCookModes::OnTheFlyInEditor:
		UATCommand += MapList;
		UATCommand += " -skipcook -cookonthefly -CookInEditor";
		if (InProfile->IsUsingZenStore())
		{
			UATCommand += TEXT(" -zenstore");
		}
		break;
	case ELauncherProfileCookModes::ByTheBookInEditor:
		UATCommand += MapList;
		UATCommand += CultureList;
		UATCommand += TEXT(" -skipcook -CookInEditor"); // don't cook anything the editor is doing it ;)
		if (InProfile->IsUsingZenStore())
		{
			// TODO: launch the zen server from the client once the external CBTB is done
			// -fileserver tells UAT to take the cotf/fileserver path and stage a thin client that loads data via the network
			// -skipserver prevents UAT from launching a COTF server for this CBTB scenario
			UATCommand += TEXT(" -zenstore -fileserver -skipserver");
		}
		if (InProfile->IsPackingWithUnrealPak())
		{
			UATCommand += TEXT(" -pak");
			if (InProfile->IsUsingIoStore())
			{
				UATCommand += TEXT(" -iostore");
			}
			if (InProfile->IsCompressed())
			{
				UATCommand += TEXT(" -compressed");
			}
		}
		break;
	case ELauncherProfileCookModes::DoNotCook:
		UATCommand += TEXT(" -skipcook");
		break;
	}

	if ( InProfile->IsForDistribution() )
	{
		UATCommand += TEXT(" -distribution");
	}

	if (InProfile->IsCookingIncrementally())
	{
		UATCommand += TEXT(" -iterativecooking");
	}

	if ( InProfile->IsIterateSharedCookedBuild() )
	{
		UATCommand += TEXT(" -iteratesharedcookedbuild=usesyncedbuild");
	}

	if (InProfile->GetSkipCookingEditorContent())
	{
		UATCommand += TEXT(" -SkipCookingEditorContent");
	}

	FString StageAdditionalCommandLine;
	if (InProfile->IsUsingIoStore() &&
		InProfile->GetReferenceContainerGlobalFileName().Len())
	{
		StageAdditionalCommandLine += TEXT(" -ReferenceContainerGlobalFileName=\"") + InProfile->GetReferenceContainerGlobalFileName() + TEXT("\"");
		if (InProfile->GetReferenceContainerCryptoKeysFileName().Len())
		{
			StageAdditionalCommandLine += TEXT(" -ReferenceContainerCryptoKeys=\"") + InProfile->GetReferenceContainerCryptoKeysFileName() + TEXT("\"") ;
		}
	}

	// stage/package/deploy
	if (InProfile->GetDeploymentMode() != ELauncherProfileDeploymentModes::DoNotDeploy)
	{
		switch (InProfile->GetDeploymentMode())
		{
		case ELauncherProfileDeploymentModes::CopyRepository:
			{
				UATCommand += TEXT(" -skipstage -deploy");
				UATCommand += CommandLine;
				UATCommand += StageDirectory;
				UATCommand += DeviceCommand;
				UATCommand += AdditionalCommandLine;

				FCommandDesc Desc;
				FText Command = FText::Format(LOCTEXT("LauncherDeployDesc", "Deploying content for {0}"), FText::FromString(Platforms.RightChop(1)));
				Desc.Name = "Deploy Task";
				Desc.Desc = Command.ToString();
				Desc.EndText = TEXT("********** DEPLOY COMMAND COMPLETED **********");
				OutCommands.Add(Desc);
				if (CommandStart.Len() == 0)
				{
					CommandStart = TEXT("********** DEPLOY COMMAND STARTED **********");
				}
			}
			break;

		case ELauncherProfileDeploymentModes::CopyToDevice:
			{
				if (Profile->IsDeployingIncrementally())
				{
					UATCommand += " -iterativedeploy";
				}
			}
		case ELauncherProfileDeploymentModes::FileServer:
			{
				UATCommand += TEXT(" -stage -deploy");
				UATCommand += CommandLine;
				UATCommand += StageDirectory;
				UATCommand += DeviceCommand;
				UATCommand += AdditionalCommandLine;
				UATCommand += StageAdditionalCommandLine;

				FCommandDesc Desc;
				FText Command = FText::Format(LOCTEXT("LauncherDeployDesc", "Deploying content for {0}"), FText::FromString(Platforms.RightChop(1)));
				Desc.Name = "Deploy Task";
				Desc.Desc = Command.ToString();
				Desc.EndText = TEXT("********** DEPLOY COMMAND COMPLETED **********");
				OutCommands.Add(Desc);
				if (CommandStart.Len() == 0)
				{
					CommandStart = TEXT("********** STAGE COMMAND STARTED **********");
				}
			}
			break;
		}

		// run
		if (InProfile->GetLaunchMode() != ELauncherProfileLaunchModes::DoNotLaunch)
		{
			UATCommand += TEXT(" -run ");

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherRunDesc", "Launching on {0}"), FText::FromString(DeviceNames.RightChop(1)));
			Desc.Name = "Run Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** RUN COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** RUN COMMAND STARTED **********");
			}
		}
	}
	else
	{
		if (InProfile->IsIncludingPrerequisites())
		{
			UATCommand += TEXT(" -prereqs");
		}

		if (InProfile->GetPackagingMode() == ELauncherProfilePackagingModes::Locally)
		{
			UATCommand += TEXT(" -stage -package");
			UATCommand += StageDirectory;
			UATCommand += CommandLine;
			UATCommand += AdditionalCommandLine;
			UATCommand += StageAdditionalCommandLine;

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherPackageDesc", "Packaging content for {0}"), FText::FromString(Platforms.RightChop(1)));
			Desc.Name = "Package Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** PACKAGE COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** STAGE COMMAND STARTED **********");
			}
		}

		if (InProfile->IsArchiving())
		{
			UATCommand += FString::Printf(TEXT(" -archive -archivedirectory=\"%s\""), *Profile->GetArchiveDirectory());

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherArchiveDesc", "Archiving content for {0}"), FText::FromString(Platforms.RightChop(1)));
			Desc.Name = "Archive Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** ARCHIVE COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** ARCHIVE COMMAND STARTED **********");
			}
		}
	}

	// wait for completion of UAT
	FCommandDesc Desc;
	FText Command = LOCTEXT("LauncherCompletionDesc", "UAT post launch cleanup");
	Desc.Name = "Post Launch Task";
	Desc.Desc = Command.ToString();
	Desc.EndText = TEXT("********** LAUNCH COMPLETED **********");
	OutCommands.Add(Desc);

	return UATCommand;
}

/* FLauncherWorker implementation
 *****************************************************************************/

void FLauncherWorker::CreateAndExecuteTasks( const ILauncherProfileRef& InProfile )
{
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	// create task chains
	TArray<FString> Platforms;
	if (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBook || InProfile->ShouldBuild())
	{
		Platforms = InProfile->GetCookedPlatforms();
	}

	// determine deployment platforms
	ILauncherDeviceGroupPtr DeviceGroup = InProfile->GetDeployedDeviceGroup();
	FName Variant = NAME_None;

	if (DeviceGroup.IsValid() && Platforms.Num() < 1)
	{
		const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();
		// for each deployed device...
		for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
		{
			const FString& DeviceId = Devices[DeviceIndex];

			TSharedPtr<ITargetDeviceProxy> DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);

			if (DeviceProxy.IsValid())
			{
				// add the platform
				Variant = DeviceProxy->GetTargetDeviceVariant(DeviceId);
				Platforms.AddUnique(DeviceProxy->GetTargetPlatformName(Variant));
			}			
		}
	}

#if !WITH_EDITOR
	// can't cook by the book in the editor if we are not in the editor...
	check( InProfile->GetCookMode() != ELauncherProfileCookModes::ByTheBookInEditor );
	check( InProfile->GetCookMode() != ELauncherProfileCookModes::OnTheFlyInEditor );
#endif

	TSharedPtr<FLauncherTask> NextTask;
	auto AddTask = [this, &NextTask](TSharedPtr<FLauncherTask> NewTask)
	{
		NewTask->OnStarted().AddRaw(this, &FLauncherWorker::OnTaskStarted);
		NewTask->OnCompleted().AddRaw(this, &FLauncherWorker::OnTaskCompleted);

		if (NextTask.IsValid())
		{
			NextTask->AddContinuation(NewTask);
			NextTask = NewTask;
		}
		else
		{
			NextTask = TaskChain = NewTask;
		}
	};

	if (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor)
	{
		// need a command which will wait for the cook to finish
		class FWaitForCookInEditorToFinish : public FLauncherTask
		{
		public:
			FWaitForCookInEditorToFinish() : FLauncherTask( FString(TEXT("Cooking in the editor")), FString(TEXT("Prepairing content to run on device")))
			{
			}
			virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
			{
				while ( !ChainState.Profile->OnIsCookFinished().Execute() )
				{
					if (IsCancelling())
					{
						ChainState.Profile->OnCookCanceled().Execute();
						return false;
					}
					FPlatformProcess::Sleep( 0.1f );
				}
				return true;
			}
		};
		TSharedPtr<FLauncherTask> WaitTask = MakeShareable(new FWaitForCookInEditorToFinish());
		AddTask(WaitTask);
	}
	TArray<FCommandDesc> Commands;
	FString StartString;
	FString UATCommand = CreateUATCommand(InProfile, Platforms, Commands, StartString);
	
	// have Turnkey 
	FString TurnkeyCommand;
	if (Profile->ShouldUpdateDeviceFlash() && DeviceGroup->GetDeviceIDs().Num() >= 0)
	{
		TurnkeyCommand = FString::Printf(TEXT("Turnkey -command=VerifySdk -type=Flash -device=%s -UpdateIfNeeded -utf8output -WaitForUATMutex %s"), *FString::Join(DeviceGroup->GetDeviceIDs(), TEXT("+")), *ITurnkeyIOModule::Get().GetUATParams());
	}

	TSharedPtr<FLauncherTask> LaunchTask = MakeShareable(new FLauncherUATTask(UATCommand, TEXT("Launch Task"), TEXT("Launching UAT..."), ReadPipe, WritePipe, InProfile->GetEditorExe(), ProcHandle, this, StartString, TurnkeyCommand));
	AddTask(LaunchTask);
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		class FLauncherWaitTask : public FLauncherTask
		{
		public:
			FLauncherWaitTask( const FString& InCommandEnd, const FString& InName, const FString& InDesc, FProcHandle& InProcessHandle, ILauncherWorker* InWorker)
				: FLauncherTask(InName, InDesc)
				, CommandText(InCommandEnd)
				, ProcessHandle(InProcessHandle)
				, LauncherWorker(InWorker)
			{
				InWorker->OnOutputReceived().AddRaw(this, &FLauncherWaitTask::HandleOutputReceived);
			}

			virtual void Exit()
			{
				LauncherWorker->OnOutputReceived().RemoveAll(this);
			}

		protected:
			virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
			{
				while (FPlatformProcess::IsProcRunning(ProcessHandle) && !EndTextFound)
				{
					FPlatformProcess::Sleep(0.25);
				}
				if (!EndTextFound && !FPlatformProcess::GetProcReturnCode(ProcessHandle, &Result))
				{
					return false;
				}
				return (Result == 0);
			}

			void HandleOutputReceived(const FString& InMessage)
			{
				EndTextFound |= InMessage.Contains(CommandText);
			}

		private:
			FString CommandText;
			FProcHandle& ProcessHandle;
			ILauncherWorker* LauncherWorker = nullptr;
			bool EndTextFound = false;
		};			

		TSharedPtr<FLauncherTask> WaitTask = MakeShareable(new FLauncherWaitTask(Commands[Index].EndText, Commands[Index].Name, Commands[Index].Desc, ProcHandle, this));
		AddTask(WaitTask);
	}

	// execute the chain
	FLauncherTaskChainState ChainState;
	ChainState.Profile = InProfile;
	ChainState.SessionId = FGuid::NewGuid();
	TaskChain->Execute(ChainState);
}

/** Callback for when a device proxy has been removed from the device proxy manager. */
void FLauncherWorker::HandleDeviceProxyManagerProxyRemoved(const TSharedRef<ITargetDeviceProxy>& RemovedProxy)
{
	FString TargetDeviceId = RemovedProxy->GetTargetDeviceId(NAME_None);
	
	// determine deployment devices
	ILauncherDeviceGroupPtr DeviceGroup = Profile->GetDeployedDeviceGroup();

	if (DeviceGroup.IsValid() && DeviceGroup->GetNumDevices() > 0)
	{
		// remove disconnected device from the list
		DeviceGroup->RemoveDevice(TargetDeviceId);
		if (DeviceGroup->GetNumDevices() == 0)
		{
			// this was the last device running, stop working
			Stop();
		}
	}
}

// start looking for disconnected devices
void FLauncherWorker::EnableDeviceDiscoveryListener()
{
	if (!OnProxyRemovedDelegateHandle.IsValid())
	{
		OnProxyRemovedDelegateHandle = DeviceProxyManager->OnProxyRemoved().AddRaw(this, &FLauncherWorker::HandleDeviceProxyManagerProxyRemoved);
	}
}

// stop looking for disconnected devices
void FLauncherWorker::DisableDeviceDiscoveryListener()
{
	if (OnProxyRemovedDelegateHandle.IsValid())
	{
		DeviceProxyManager->OnProxyRemoved().Remove(OnProxyRemovedDelegateHandle);
		OnProxyRemovedDelegateHandle.Reset();
	}

}

//Cancel the currently running application on all devices
bool FLauncherWorker::TerminateLaunchedProcess()
{
	// determine deployment devices
	ILauncherDeviceGroupPtr DeviceGroup = Profile->GetDeployedDeviceGroup();
	FName Variant = NAME_None;

	// device list
	FString DeviceNamesParam = TEXT("");
	if (DeviceGroup.IsValid())
	{
		const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();
		// cancel the app onr each device
		for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
		{
			const FString& DeviceId = Devices[DeviceIndex];

			TSharedPtr<ITargetDeviceProxy> DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);
			if (DeviceProxy.IsValid())
			{
				FName TargetDeviceVariant = DeviceProxy->GetTargetDeviceVariant(DeviceId);

				FString TargetDeviceId = DeviceId;
				
				// remove the variant prefix (eg. Android_ETC@deviceId)
				int32 InPos = TargetDeviceId.Find("@", ESearchCase::CaseSensitive);
				if (InPos > 0) 
				{ 
					TargetDeviceId.RightInline(TargetDeviceId.Len() -  InPos - 1, EAllowShrinking::No);

				}

				// try to find the corresponding app id
				if (CachedDevicePackagePair.Contains(TargetDeviceId))
				{
					DeviceProxy->TerminateLaunchedProcess(TargetDeviceVariant, CachedDevicePackagePair[TargetDeviceId]);
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
