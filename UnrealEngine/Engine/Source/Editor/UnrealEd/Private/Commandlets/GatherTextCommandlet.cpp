// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextCommandlet.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/GCObjectScopeGuard.h"
#include "SourceControlHelpers.h"
#include "GeneralProjectSettings.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextCommandlet, Log, All);

/**
 *	UGatherTextCommandlet
 */
UGatherTextCommandlet::UGatherTextCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UGatherTextCommandlet::UsageText
	(
	TEXT("GatherTextCommandlet usage...\r\n")
	TEXT("    <GameName> GatherTextCommandlet -Config=<path to config ini file> [-Preview -EnableSCC -DisableSCCSubmit -GatherType=<All | Source | Asset | Metadata>]\r\n")
	TEXT("    \r\n")
	TEXT("    <path to config ini file> Full path to the .ini config file that defines what gather steps the commandlet will run.\r\n")
	TEXT("    Preview\t Runs the commandlet and its child commandlets in preview. Some commandlets will not be executed in preview mode. Use this to dump all generated warnings without writing any files. Using this switch implies -DisableSCCSubmit\r\n")
	TEXT("    EnableSCC\t Enables revision control and allows the commandlet to check out files for editing.\r\n")
	TEXT("    DisableSCCSubmit\t Prevents the commandlet from submitting checked out files in revision control that have been edited.\r\n")
	TEXT("    GatherType\t Only performs a gather on the specified type of file (currently only works in preview mode). Source only runs commandlets that gather source files. Asset only runs commandlets that gather asset files. All runs commandlets that gather both source and asset files. Leaving this param out implies a gather type of All.")
	TEXT("Metadata only runs commandlets that gather metadata files. All runs commandlets that gather both source and asset files. Leaving this param out implies a gather type of All.\r\n")
	);


int32 UGatherTextCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);
	if (Switches.Contains(TEXT("help")) || Switches.Contains(TEXT("Help")))
	{
		UE_LOG(LogGatherTextCommandlet, Display, TEXT("%s"), *UsageText);
		return 0;
	}

	// Build up the complete list of config files to process
	TArray<FString> GatherTextConfigPaths;
	if (const FString* ConfigParamPtr = ParamVals.Find(UGatherTextCommandletBase::ConfigParam))
	{
		ConfigParamPtr->ParseIntoArray(GatherTextConfigPaths, TEXT(";"));
	}

	if (const FString* ConfigListFileParamPtr = ParamVals.Find(TEXT("ConfigList")))
	{
		if (FPaths::FileExists(*ConfigListFileParamPtr))
		{
			TArray<FString> ConfigFiles;
			FFileHelper::LoadFileToStringArray(ConfigFiles, **ConfigListFileParamPtr);
			if (ConfigFiles.Num() > 0)
			{
				GatherTextConfigPaths.Append(MoveTemp(ConfigFiles));
			}
			else
			{
				UE_LOG(LogGatherTextCommandlet, Warning, TEXT("There are no config file paths in specified config ,list '%s'. Please check to see the is correctly populated."), **ConfigListFileParamPtr);
			}
		}
		else
		{
			UE_LOG(LogGatherTextCommandlet, Warning, TEXT("Specified config list file '%s' does not exist. No additional config files from -ConfigList can be added."), **ConfigListFileParamPtr);
		}
	}

	// @TODOLocalization: Handle the case where -config and -ConfigList both specify the same files.
	// Currently that would just mean that the config files will be launched with the relevant commandlets multiple times. The results should be correct, but it's wasted work.
	
	// Turn all relative paths into absolute paths 
	const FString& ProjectBasePath = UGatherTextCommandletBase::GetProjectBasePath();
	for (FString& GatherTextConfigPath : GatherTextConfigPaths)
	{
		if (FPaths::IsRelative(GatherTextConfigPath))
		{
			GatherTextConfigPath = FPaths::Combine(*ProjectBasePath, *GatherTextConfigPath);
		}
	}

	if (GatherTextConfigPaths.Num() == 0)
	{
		UE_LOG(LogGatherTextCommandlet, Error, TEXT("-config or -ConfigList not specified. If -ConfigList was specified, please check that the file path is valid.\n%s"), *UsageText);
		return -1;
	}

	const bool bRunningInPreview = Switches.Contains(UGatherTextCommandletBase::PreviewSwitch);
	if (bRunningInPreview)
	{
		UE_LOG(LogGatherTextCommandlet, Display, TEXT("Running commandlet in preview mode. Some child commandlets and steps will be skipped."));
		// -GatherType is only valid in preview mode right now 
		if (const FString* GatherTypeParamPtr = ParamVals.Find(UGatherTextCommandletBase::GatherTypeParam))
		{
			UE_LOG(LogGatherTextCommandlet, Display, TEXT("Running commandlet with gather type %s. Only %s files will be gathered."), **GatherTypeParamPtr, **GatherTypeParamPtr);
		}
		else
		{
			// if the -GatherType param is not specified, we default to gathering both source, asset and metadata
			UE_LOG(LogGatherTextCommandlet, Display, TEXT("-GatherType param not specified. Commandlet will gather source, assset and metadata."));
		}
	}

	// If we are only doing a preview run, we will not be writing to any files that will need to be submitted to source control 
	const bool bEnableSourceControl = Switches.Contains(UGatherTextCommandletBase::EnableSourceControlSwitch) && !bRunningInPreview;
	const bool bDisableSubmit = Switches.Contains(UGatherTextCommandletBase::DisableSubmitSwitch) || bRunningInPreview;

	TSharedPtr<FLocalizationSCC> CommandletSourceControlInfo;
	if (bEnableSourceControl)
	{
		CommandletSourceControlInfo = MakeShareable(new FLocalizationSCC());

		FText SCCErrorStr;
		if (!CommandletSourceControlInfo->IsReady(SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("Revision Control error: %s"), *SCCErrorStr.ToString());
			return -1;
		}
	}

	double AllCommandletExecutionStartTime = FPlatformTime::Seconds();
	for (const FString& GatherTextConfigPath : GatherTextConfigPaths)
	{
		const int32 Result = ProcessGatherConfig(GatherTextConfigPath, CommandletSourceControlInfo, Tokens, Switches, ParamVals);
		if (Result != 0)
		{
			return Result;
		}
	}

	if (CommandletSourceControlInfo.IsValid() && !bDisableSubmit)
	{
		FText SCCErrorStr;
		if (CommandletSourceControlInfo->CheckinFiles(GetChangelistDescription(GatherTextConfigPaths), SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandlet, Log, TEXT("Submitted Localization files."));
		}
		else
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
			if (!CommandletSourceControlInfo->CleanUp(SCCErrorStr))
			{
				UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
			}
			return -1;
		}
	}
	UE_LOG(LogGatherTextCommandlet, Display, TEXT("Completed all steps in %.2f seconds"), FPlatformTime::Seconds() - AllCommandletExecutionStartTime);

	// Note: Other things use the below log as a tracker for GatherText completing successfully - DO NOT remove or edit this line without also updating those places
	UE_LOG(LogGatherTextCommandlet, Display, TEXT("GatherText completed with exit code 0"));
	return 0;
}

int32 UGatherTextCommandlet::ProcessGatherConfig(const FString& GatherTextConfigPath, const TSharedPtr<FLocalizationSCC>& CommandletSourceControlInfo, const TArray<FString>& Tokens, const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals)
{
	GConfig->LoadFile(*GatherTextConfigPath);

	if (!GConfig->FindConfigFile(*GatherTextConfigPath))
	{
		UE_LOG(LogGatherTextCommandlet, Error, TEXT("Loading Config File \"%s\" failed."), *GatherTextConfigPath);
		return -1; 
	}

	UE_LOG(LogGatherTextCommandlet, Display, TEXT("Beginning GatherText Commandlet for '%s'"), *GatherTextConfigPath);

	// Read in the platform split mode to use
	ELocTextPlatformSplitMode PlatformSplitMode = ELocTextPlatformSplitMode::None;
	{
		FString PlatformSplitModeString;
		if (GetStringFromConfig(TEXT("CommonSettings"), TEXT("PlatformSplitMode"), PlatformSplitModeString, GatherTextConfigPath))
		{
			UEnum* PlatformSplitModeEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Localization.ELocTextPlatformSplitMode"));
			const int64 PlatformSplitModeInt = PlatformSplitModeEnum->GetValueByName(*PlatformSplitModeString);
			if (PlatformSplitModeInt != INDEX_NONE)
			{
				PlatformSplitMode = (ELocTextPlatformSplitMode)PlatformSplitModeInt;
			}
		}
	}

	FString LocalizationTargetName;
	{
		FString ManifestName;
		GetStringFromConfig(TEXT("CommonSettings"), TEXT("ManifestName"), ManifestName, GatherTextConfigPath);
		LocalizationTargetName = FPaths::GetBaseFilename(ManifestName);
	}

	FString CopyrightNotice;
	if (!GetStringFromConfig(TEXT("CommonSettings"), TEXT("CopyrightNotice"), CopyrightNotice, GatherTextConfigPath))
	{
		CopyrightNotice = GetDefault<UGeneralProjectSettings>()->CopyrightNotice;
	}

	// Basic helper that can be used only to gather a new manifest for writing
	TSharedRef<FLocTextHelper> CommandletGatherManifestHelper = MakeShared<FLocTextHelper>(LocalizationTargetName, MakeShared<FLocFileSCCNotifies>(CommandletSourceControlInfo), PlatformSplitMode);
	CommandletGatherManifestHelper->SetCopyrightNotice(CopyrightNotice);
	CommandletGatherManifestHelper->LoadManifest(ELocTextHelperLoadFlags::Create);

	const FString GatherTextStepPrefix = TEXT("GatherTextStep");

	// Read the list of steps from the config file (they all have the format GatherTextStep{N})
	TArray<FString> StepNames;
	GConfig->GetSectionNames(GatherTextConfigPath, StepNames);
	StepNames.RemoveAllSwap([&GatherTextStepPrefix](const FString& InStepName)
	{
		return !InStepName.StartsWith(GatherTextStepPrefix);
	});

	// Make sure the steps are sorted in ascending order (by numerical suffix)
	StepNames.Sort([&GatherTextStepPrefix](const FString& InStepNameOne, const FString& InStepNameTwo)
	{
		const FString NumericalSuffixOneStr = InStepNameOne.RightChop(GatherTextStepPrefix.Len());
		const int32 NumericalSuffixOne = FCString::Atoi(*NumericalSuffixOneStr);

		const FString NumericalSuffixTwoStr = InStepNameTwo.RightChop(GatherTextStepPrefix.Len());
		const int32 NumericalSuffixTwo = FCString::Atoi(*NumericalSuffixTwoStr);

		return NumericalSuffixOne < NumericalSuffixTwo;
	});
	// Generate the switches and params to be passed on to child commandlets
	FString GeneratedParamsAndSwitches;
	// Add all the command params with the exception of config
	for (auto ParamIter = ParamVals.CreateConstIterator(); ParamIter; ++ParamIter)
	{
		const FString& Key = ParamIter.Key();
		const FString& Val = ParamIter.Value();
		if (Key != UGatherTextCommandletBase::ConfigParam)
		{
			GeneratedParamsAndSwitches += FString::Printf(TEXT(" -%s=%s"), *Key, *Val);
		}
	}

	// Add all the command switches
	for (auto SwitchIter = Switches.CreateConstIterator(); SwitchIter; ++SwitchIter)
	{
		const FString& Switch = *SwitchIter;
		GeneratedParamsAndSwitches += FString::Printf(TEXT(" -%s"), *Switch);
	}

	const bool bRunningInPreview = Switches.Contains(TEXT("Preview"));
	// Execute each step defined in the config file.
	for (const FString& StepName : StepNames)
	{
		FString CommandletClassName = GConfig->GetStr( *StepName, TEXT("CommandletClass"), GatherTextConfigPath ) + TEXT("Commandlet");

		UClass* CommandletClass = FindFirstObject<UClass>(*CommandletClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("UGatherTextCommandlet::ProcessGatherConfig"));
		if (!CommandletClass)
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("The commandlet name %s in section %s is invalid."), *CommandletClassName, *StepName);
			continue;
		}

		UGatherTextCommandletBase* Commandlet = NewObject<UGatherTextCommandletBase>(GetTransientPackage(), CommandletClass);
		check(Commandlet);
		FGCObjectScopeGuard CommandletGCGuard(Commandlet);
		Commandlet->Initialize( CommandletGatherManifestHelper, CommandletSourceControlInfo );
		// As of now, all params and switches (with the exception of config) is passed on to child commandlets
		// Instead of parsing in child commandlets, we'll just pass the params and switches along to determine if we need to run the child commandlet 
		// If we are running in preview mode, then most commandlets should be skipped as ShouldRunInPreview() defaults to false in the base class.
		if (bRunningInPreview && !Commandlet->ShouldRunInPreview(Switches, ParamVals))
		{
			UE_LOG(LogGatherTextCommandlet, Display, TEXT("Should not run %s: %s in preview. Skipping."), *StepName, *CommandletClassName);
			continue;
		}

		// Execute the commandlet.
		double CommandletExecutionStartTime = FPlatformTime::Seconds();

		UE_LOG(LogGatherTextCommandlet, Display, TEXT("Executing %s: %s"), *StepName, *CommandletClassName);
		
		FString GeneratedCmdLine = FString::Printf(TEXT("-Config=\"%s\" -Section=%s"), *GatherTextConfigPath , *StepName);
		GeneratedCmdLine += GeneratedParamsAndSwitches;

		if( 0 != Commandlet->Main( GeneratedCmdLine ) )
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s-%s reported an error."), *StepName, *CommandletClassName);
			if( CommandletSourceControlInfo.IsValid() )
			{
				FText SCCErrorStr;
				if( !CommandletSourceControlInfo->CleanUp( SCCErrorStr ) )
				{
					UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
				}
			}
			return -1;
		}

		UE_LOG(LogGatherTextCommandlet, Display, TEXT("Completed %s: %s in %.2f seconds"), *StepName, *CommandletClassName, FPlatformTime::Seconds() - CommandletExecutionStartTime);
	}

	// Clean-up any stale per-platform data
	{
		FString DestinationPath;
		if (GetPathFromConfig(TEXT("CommonSettings"), TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath))
		{
			IFileManager& FileManager = IFileManager::Get();

			auto RemoveDirectory = [&FileManager](const TCHAR* InDirectory)
			{
				FileManager.IterateDirectoryRecursively(InDirectory, [&FileManager](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
				{
					if (!bIsDirectory)
					{
						if (!USourceControlHelpers::IsAvailable() || !USourceControlHelpers::MarkFileForDelete(FilenameOrDirectory))
						{
							FileManager.Delete(FilenameOrDirectory, false, true);
						}
					}
					return true;
				});
				FileManager.DeleteDirectory(InDirectory, false, true);
			};

			const FString PlatformLocalizationPath = DestinationPath / FPaths::GetPlatformLocalizationFolderName();
			if (CommandletGatherManifestHelper->ShouldSplitPlatformData())
			{
				// Remove any stale platform sub-folders
				FileManager.IterateDirectory(*PlatformLocalizationPath, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
				{
					if (bIsDirectory)
					{
						const FString SplitPlatformName = FPaths::GetCleanFilename(FilenameOrDirectory);
						if (!CommandletGatherManifestHelper->GetPlatformsToSplit().Contains(SplitPlatformName))
						{
							RemoveDirectory(FilenameOrDirectory);
						}
					}
					return true;
				});
			}
			else
			{
				// Remove the entire Platforms folder
				RemoveDirectory(*PlatformLocalizationPath);
			}
		}
		else
		{
			UE_LOG(LogGatherTextCommandlet, Warning, TEXT("No destination path specified in the 'CommonSettings' section. Cannot check for stale per-platform data!"));
		}
	}

	return 0;
}

FText UGatherTextCommandlet::GetChangelistDescription(const TArray<FString>& GatherTextConfigPaths)
{
	FString ProjectName = FApp::GetProjectName();
	if (ProjectName.IsEmpty())
	{
		ProjectName = TEXT("Engine");
	}

	FString ChangeDescriptionString = FString::Printf(TEXT("[Localization Update] %s\n\n"), *ProjectName);

	ChangeDescriptionString += TEXT("Targets:\n");
	for (const FString& GatherTextConfigPath : GatherTextConfigPaths)
	{
		const FString TargetName = FPaths::GetBaseFilename(GatherTextConfigPath, true);
		ChangeDescriptionString += FString::Printf(TEXT("  %s\n"), *TargetName);
	}

	return FText::FromString(MoveTemp(ChangeDescriptionString));
}
