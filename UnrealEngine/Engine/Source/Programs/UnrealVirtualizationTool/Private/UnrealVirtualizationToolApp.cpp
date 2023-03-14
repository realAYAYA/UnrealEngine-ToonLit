// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealVirtualizationToolApp.h"

#include "Commands/CommandBase.h"
#include "Commands/RehydrateCommand.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FeedbackContextAnsi.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "SourceControlInitSettings.h"
#include "SourceControlOperations.h"
#include "UnrealVirtualizationTool.h"
#include "Virtualization/VirtualizationSystem.h"

namespace
{

/** Utility for testing if a file path resolves to a valid package file or not */
bool IsPackageFile(const FString FilePath)
{
	// ::IsPackageExtension requires a TCHAR so we cannot use FPathViews here
	const FString Extension = FPaths::GetExtension(FilePath);

	// Currently we don't virtualize text based assets so no call to FPackageName::IsTextPackageExtension
	return FPackageName::IsPackageExtension(*Extension);
}

/**
 * Utility to clean up the tags we got from the virtualization system. Convert the FText to FString and
 * discard any duplicate entries.
 */
TArray<FString> BuildFinalTagDescriptions(TArray<FText>& DescriptionTags)
{
	TArray<FString> CleanedDescriptions;
	CleanedDescriptions.Reserve(DescriptionTags.Num());

	for (const FText& Tag : DescriptionTags)
	{
		CleanedDescriptions.AddUnique(Tag.ToString());
	}

	return CleanedDescriptions;
}

/** Utility to get EMode from a string */
void LexFromString(UE::Virtualization::EMode& OutValue, const FStringView& InString)
{
	if (InString == TEXT("Changelist"))
	{
		OutValue = UE::Virtualization::EMode::Changelist;
	}
	else if (InString == TEXT("PackageList"))
	{
		OutValue = UE::Virtualization::EMode::PackageList;
	}
	else if (InString == TEXT("Rehydrate"))
	{
		OutValue = UE::Virtualization::EMode::Rehydrate;
	}
	else
	{
		OutValue = UE::Virtualization::EMode::Unknown;
	}
}

/** Utility for creating a new command */
template<typename CommandType>
TUniquePtr<UE::Virtualization::FCommand> CreateCommand(const FString& ModeName, const TCHAR* CmdLine)
{
	UE_LOG(LogVirtualizationTool, Display, TEXT("Attempting to initialize command '%s'..."), *ModeName);

	TUniquePtr<UE::Virtualization::FCommand> Command = MakeUnique<CommandType>(ModeName);
	if (Command->Initialize(CmdLine))
	{
		return Command;
	}
	else
	{
		return TUniquePtr<UE::Virtualization::FCommand>();
	}
}

/**
 * This class can be used to prevent log messages from other systems being logged with the Display verbosity.
 * In practical terms this means as long as the class is alive, only LogVirtualizationTool messages will
 * be logged to the display meaning the user will have less information to deal with.
 */
class FOverrideOutputDevice final : public FFeedbackContextAnsi
{
public:
	FOverrideOutputDevice()
	{
		OriginalLog = GWarn;
		GWarn = this;
	}

	virtual ~FOverrideOutputDevice()
	{
		GWarn = OriginalLog;
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Display && Category != LogVirtualizationTool.GetCategoryName())
		{
			Verbosity = ELogVerbosity::Log;
		}

		FFeedbackContextAnsi::Serialize(V, Verbosity, Category);
	}

private:
	FFeedbackContext* OriginalLog;
};

} // namespace

namespace UE::Virtualization
{

FUnrealVirtualizationToolApp::FUnrealVirtualizationToolApp()
{

}

FUnrealVirtualizationToolApp::~FUnrealVirtualizationToolApp()
{

}

EInitResult FUnrealVirtualizationToolApp::Initialize()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Initialize);

	UE_LOG(LogVirtualizationTool, Display, TEXT("Initializing..."));

	// Display the log path to the user so that they can more easily find it
	// Note that ::GetAbsoluteLogFilename does not always return an absolute filename
	FString LogFilePath = FGenericPlatformOutputDevices::GetAbsoluteLogFilename();
	LogFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*LogFilePath);

	UE_LOG(LogVirtualizationTool, Display, TEXT("Logging process to '%s'"), *LogFilePath);

	if (!TryLoadModules())
	{
		return EInitResult::Error;
	}

	if (!TryInitEnginePlugins())
	{
		return EInitResult::Error;
	}

	EInitResult CmdLineResult = TryParseCmdLine();
	if (CmdLineResult != EInitResult::Success)
	{
		return CmdLineResult;
	}

	TArray<FString> Packages;
	if (!CurrentCommand.IsValid())
	{
		// Legacy code path for virtualization (which has not yet been ported to a command)	
		switch (Mode)
		{
			case EMode::Changelist:
				if (!TryParseChangelist(Packages))
				{
					return EInitResult::Error;
				}
				break;

			case EMode::PackageList:
				if (!TryParsePackageList(Packages))
				{
					return EInitResult::Error;
				}
				break;

			default:
				UE_LOG(LogVirtualizationTool, Display, TEXT("Unknown mode, cannot find packages!"));
				return EInitResult::Error;
				break;
		}
	}
	else
	{
		Packages = CurrentCommand->GetPackages();
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("\tFound '%d' package file(s)"), Packages.Num());

	if (!TrySortFilesByProject(Packages))
	{
		return EInitResult::Error;
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("Initialization complete!"));

	return EInitResult::Success;
}

bool FUnrealVirtualizationToolApp::Run()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Run);

	if (!CurrentCommand.IsValid())
	{
		// Legacy code path for virtualization (which has not yet been ported to a command)	
		TArray<FString> FinalDescriptionTags;

		if (EnumHasAllFlags(ProcessOptions, EProcessOptions::Virtualize))
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("Running the virtualization process..."));

			TArray<FText> DescriptionTags;

			for (const FProject& Project : Projects)
			{
				TStringBuilder<128> ProjectName;
				ProjectName << Project.GetProjectName();

				UE_LOG(LogVirtualizationTool, Display, TEXT("\tChecking package(s) for the project '%s'..."), ProjectName.ToString());

				FConfigFile EngineConfigWithProject;
				if (!Project.TryLoadConfig(EngineConfigWithProject))
				{
					return false;
				}

				Project.RegisterMountPoints();

				UE::Virtualization::FInitParams InitParams(ProjectName, EngineConfigWithProject);
				UE::Virtualization::Initialize(InitParams, UE::Virtualization::EInitializationFlags::ForceInitialize);

				TArray<FString> Packages = Project.GetAllPackages();

				TArray<FText> Errors;
				UE::Virtualization::IVirtualizationSystem::Get().TryVirtualizePackages(Packages, DescriptionTags, Errors);

				if (!Errors.IsEmpty())
				{
					UE_LOG(LogVirtualizationTool, Error, TEXT("The virtualization process failed with the following errors:"));
					for (const FText& Error : Errors)
					{
						UE_LOG(LogVirtualizationTool, Error, TEXT("\t%s"), *Error.ToString());
					}
					return false;
				}

				UE_LOG(LogVirtualizationTool, Display, TEXT("\tCheck complete"));

				UE::Virtualization::Shutdown();
				Project.UnRegisterMountPoints();
			}

			FinalDescriptionTags = BuildFinalTagDescriptions(DescriptionTags);
		}
		else
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("Skipping the virtualization process"));
		}

		if (Mode == EMode::Changelist)
		{
			if (!TrySubmitChangelist(FinalDescriptionTags))
			{
				return false;
			}
		}

		return true;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Running the '%s' command..."), *CurrentCommand->GetName());

		if (CurrentCommand->Run(Projects))
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("Command '%s' suceeeded!"), *CurrentCommand->GetName());
			return true;
		}
		else
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Command '%s' failed!"), *CurrentCommand->GetName());
			return false;
		}
	}
}

void FUnrealVirtualizationToolApp::PrintCmdLineHelp() const
{
	UE_LOG(LogVirtualizationTool, Display, TEXT("Usage:"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool -ClientSpecName=<name> -Mode=Changelist -Changelist=<number> [-nosubmit] [global options]"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\t[optional]-nosubmit (the changelist will be virtualized but not submitted)"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool -ClientSpecName=<name> -Mode=PackageList -Path=<string> [global options]"));

	// TODO: If the commands were registered in some way we could automate this
	FRehydrateCommand::PrintCmdLineHelp();

	UE_LOG(LogVirtualizationTool, Display, TEXT("Global Options:"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\t-verbose (all log messages with display verbosity will be displayed, not just LogVirtualizationTool)"));
}

bool FUnrealVirtualizationToolApp::TrySubmitChangelist(const TArray<FString>& DescriptionTags)
{
	if (!EnumHasAllFlags(ProcessOptions, EProcessOptions::Submit))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Skipping submit of changelist '%s' due to cmdline options"), *ChangelistNumber);
		return true;
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("Attempting to submit the changelist '%s'"), *ChangelistNumber);

	if (!TryConnectToSourceControl())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Submit failed, cannot find a valid source control provider"));
		return false;
	}

	if (!ChangelistToSubmit.IsValid())
	{
		// This should not be possible, the check and error message is to guard against potential future problems only.
		UE_LOG(LogVirtualizationTool, Error, TEXT("Submit failed, could not find the changelist"));
		return false;
	}

	FSourceControlChangelistRef Changelist = ChangelistToSubmit.ToSharedRef();
	FSourceControlChangelistStatePtr ChangelistState = SCCProvider->GetState(Changelist, EStateCacheUsage::Use);

	if (!ChangelistState.IsValid())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Submit failed, failed to find the state for the changelist"));
		return false;
	}

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();

	// Grab the original changelist description then append our tags afterwards
	TStringBuilder<512> Description;
	Description << ChangelistState->GetDescriptionText().ToString();

	for (const FString& Tag : DescriptionTags)
	{
		Description << TEXT("\n") << Tag;
	}

	CheckInOperation->SetDescription(FText::FromString(Description.ToString()));

	if (SCCProvider->Execute(CheckInOperation, Changelist) == ECommandResult::Succeeded)
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("%s"), *CheckInOperation->GetSuccessMessage().ToString());	
		return true;
	}
	else
	{
		// Even when log suppression is active we still show errors to the users and as the source control
		// operation should have logged the problem as an error the user will see it. This means we don't 
		// have to extract it from CheckInOperation .
		UE_LOG(LogVirtualizationTool, Error, TEXT("Submit failed, please check the log!"));

		return false;
	}
}

bool FUnrealVirtualizationToolApp::TryLoadModules()
{
	if (FModuleManager::Get().LoadModule(TEXT("Virtualization"), ELoadModuleFlags::LogFailures) == nullptr)
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to load the 'Virtualization' module"));
	}

	return true;
}

bool FUnrealVirtualizationToolApp::TryInitEnginePlugins()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryInitEnginePlugins);

	UE_LOG(LogVirtualizationTool, Log, TEXT("Loading Engine Plugins"));

	IPluginManager& PluginMgr = IPluginManager::Get();

	const FString PerforcePluginPath = FPaths::EnginePluginsDir() / TEXT("Developer/PerforceSourceControl/PerforceSourceControl.uplugin");
	FText ErrorMsg;
	if (!PluginMgr.AddToPluginsList(PerforcePluginPath, &ErrorMsg))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find 'PerforceSourceControl' plugin due to: %s"), *ErrorMsg.ToString());
		return false;
	}

	PluginMgr.MountNewlyCreatedPlugin(TEXT("PerforceSourceControl"));

	TSharedPtr<IPlugin> Plugin = PluginMgr.FindPlugin(TEXT("PerforceSourceControl"));
	if (Plugin == nullptr || !Plugin->IsEnabled())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("The 'PerforceSourceControl' plugin is disabled."));
		return false;
	}

	return true;
}

bool FUnrealVirtualizationToolApp::TryConnectToSourceControl()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryConnectToSourceControl);

	if (SCCProvider.IsValid())
	{
		// Already connected so just return
		return true;
	}

	UE_LOG(LogVirtualizationTool, Log, TEXT("Trying to connect to source control..."));

	FSourceControlInitSettings SCCSettings(FSourceControlInitSettings::EBehavior::OverrideAll);
	SCCSettings.AddSetting(TEXT("P4Client"), ClientSpecName);

	SCCProvider = ISourceControlModule::Get().CreateProvider(FName("Perforce"), TEXT("UnrealVirtualizationTool"), SCCSettings);
	if (SCCProvider.IsValid())
	{
		return true;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to create a perforce connection"));
		return false;
	}
}

EInitResult FUnrealVirtualizationToolApp::TryParseCmdLine()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryParseCmdLine);

	UE_LOG(LogVirtualizationTool, Log, TEXT("Parsing the commandline"));

	const TCHAR* CmdLine = FCommandLine::Get();

	if (CmdLine == nullptr || CmdLine[0] == TEXT('\0'))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("No commandline parameters found!"));
		PrintCmdLineHelp();
		return EInitResult::Error;
	}

	if (FParse::Param(CmdLine, TEXT("Help")) || FParse::Param(CmdLine, TEXT("?")))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Commandline help requested"));
		PrintCmdLineHelp();
		return EInitResult::EarlyOut;
	}

	// First parse the command line options that can apply to all modes

	if (!FParse::Value(CmdLine, TEXT("-ClientSpecName="), ClientSpecName))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find cmdline switch 'ClientSpecName', this is a required parameter!"));
		PrintCmdLineHelp();
		return EInitResult::Error;
	}

	if (FParse::Param(CmdLine, TEXT("Verbose")))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Cmdline parameter '-Verbose' found, no longer supressing Display log messages!"));
		OutputDeviceOverride.Reset();
	}
	else
	{
		OutputDeviceOverride = MakeUnique<FOverrideOutputDevice>();
	}

	// Now parse the mode specific command line options

	FString ModeAsString;
	if (!FParse::Value(CmdLine, TEXT("-Mode="), ModeAsString))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find cmdline switch 'Mode', this is a required parameter!"));
		PrintCmdLineHelp();
		return EInitResult::Error;
	}

	LexFromString(Mode, ModeAsString);

	switch (Mode)
	{
		case EMode::Changelist:
			return TryParseChangelistCmdLine(CmdLine);
			break;
		case EMode::PackageList:
			return TryParsePackageListCmdLine(CmdLine);
			break;
		case EMode::Rehydrate:
			CurrentCommand = CreateCommand<FRehydrateCommand>(ModeAsString, CmdLine);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
			break;
		case EMode::Unknown:
		default:
			UE_LOG(LogVirtualizationTool, Error, TEXT("Unexpected value for the cmdline switch 'Mode', this is a required parameter!"));
			PrintCmdLineHelp();
			return EInitResult::Error;

			break;
	}
}

EInitResult	FUnrealVirtualizationToolApp::TryParseChangelistCmdLine(const TCHAR* CmdLine)
{
	if (FParse::Value(CmdLine, TEXT("-Changelist="), ChangelistNumber))
	{
		// Optional switches
		if (FParse::Param(CmdLine, TEXT("NoSubmit")))
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("Cmdline parameter '-NoSubmit' found, the changelist will be virtualized but not submitted!"));
		}
		else
		{
			ProcessOptions |= EProcessOptions::Submit;
		}

		UE_LOG(LogVirtualizationTool, Display, TEXT("Attempting to virtualize changelist '%s'"), *ChangelistNumber);
		
		return EInitResult::Success;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find cmdline switch 'Changelist', this is a required parameter for the 'Changelist' mode!"));
		PrintCmdLineHelp();
		return EInitResult::Error;
	}
}

EInitResult FUnrealVirtualizationToolApp::TryParsePackageListCmdLine(const TCHAR* CmdLine)
{
	if (FParse::Value(CmdLine, TEXT("-Path="), PackageListPath))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Virtualizing packages found in package list: '%s'"), *PackageListPath);
		return EInitResult::Success;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find cmdline switch 'Path', this is a required parameter for the 'PackageList mode!"));
		PrintCmdLineHelp();
		return EInitResult::Error;
	}	
}

bool FUnrealVirtualizationToolApp::TryParseChangelist(TArray<FString>& OutPackages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryParseChangelist);

	if (!TryConnectToSourceControl())
	{
		return false;
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("Attempting to parse changelist '%s' in workspace '%s'"), *ChangelistNumber, *ClientSpecName);

	if (!SCCProvider.IsValid())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("No valid source control connection found!"));
		return false;
	}

	SCCProvider->Init(true);

	if (!SCCProvider->UsesChangelists())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("The source control provider does not support the use of changelists"));
		return false;
	}

	TArray<FSourceControlChangelistRef> Changelists = SCCProvider->GetChangelists(EStateCacheUsage::ForceUpdate);
	if (Changelists.IsEmpty())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find any changelists"));
		return false;
	}

	TArray<FSourceControlChangelistStateRef> ChangelistsStates;
	if (SCCProvider->GetState(Changelists, ChangelistsStates, EStateCacheUsage::Use) != ECommandResult::Succeeded)
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find changelist data"));
		return false;
	}

	for (FSourceControlChangelistStateRef& ChangelistState : ChangelistsStates)
	{
		const FText DisplayText = ChangelistState->GetDisplayText();

		if (ChangelistNumber == DisplayText.ToString())
		{
			TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> Operation = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();

			// TODO: Updating only the CL we want does not currently work and even if it did we still end up with a pointless
			// p4 changes command before updating the files. Given we know the changelist number via FSourceControlChangelistRef
			// we should be able to just request the file states be updated.
			// This is also a lot of code to write for a simple "give me all files in a changelist" operation, if we don't add
			// support directly in the API we should move this to a utility namespace in the source control module.

			FSourceControlChangelistRef Changelist = ChangelistState->GetChangelist();
			Operation->SetChangelistsToUpdate(MakeArrayView(&Changelist, 1));
			Operation->SetUpdateFilesStates(true);
			
			if (SCCProvider->Execute(Operation, EConcurrency::Synchronous) != ECommandResult::Succeeded)
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find the files in changelist '%s'"), *ChangelistNumber);
				return false;
			}

			const TArray<FSourceControlStateRef>& FilesinChangelist = ChangelistState->GetFilesStates();
			for (const FSourceControlStateRef& FileState : FilesinChangelist)
			{
				if (::IsPackageFile(FileState->GetFilename()))
				{
					OutPackages.Add(FileState->GetFilename());
				}
				else
				{
					UE_LOG(LogVirtualizationTool, Log, TEXT("Ignoring non-package file '%s'"), *FileState->GetFilename());
				}
			}

			ChangelistToSubmit = Changelist;

			return true;
		}
	}

	UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find the changelist '%s'"), *ChangelistNumber);
	return false;
}

bool FUnrealVirtualizationToolApp::TryParsePackageList(TArray<FString>& OutPackages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TrySortFilesByProject);

	UE_LOG(LogVirtualizationTool, Display, TEXT("Parsing the package list..."));

	if (!IFileManager::Get().FileExists(*PackageListPath))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("\tThe package list '%s' does not exist"), *PackageListPath);
		return false;
	}

	if (FFileHelper::LoadFileToStringArray(OutPackages, *PackageListPath))
	{
		// We don't have control over how the package list was generated so make sure that the paths
		// are in the format that we want.
		for (FString& PackagePath : OutPackages)
		{
			FPaths::NormalizeFilename(PackagePath);
		}

		return true;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("\tFailed to parse the package list '%s'"), *PackageListPath);
		return false;
	}
}

bool FUnrealVirtualizationToolApp::TrySortFilesByProject(const TArray<FString>& Packages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TrySortFilesByProject);

	UE_LOG(LogVirtualizationTool, Display, TEXT("Sorting files by project..."));

	for (const FString& PackagePath : Packages)
	{
		FString ProjectFilePath;
		FString PluginFilePath;

		if (TryFindProject(PackagePath, ProjectFilePath, PluginFilePath))
		{
			FProject& Project = FindOrAddProject(MoveTemp(ProjectFilePath));
			if (PluginFilePath.IsEmpty())
			{
				Project.AddFile(PackagePath);
			}
			else
			{
				Project.AddPluginFile(PackagePath, MoveTemp(PluginFilePath));
			}
		}
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("\tThe package files are associated with '%d' projects(s)"), Projects.Num());

	return true;
}

bool FUnrealVirtualizationToolApp::TryFindProject(const FString& PackagePath, FString& ProjectFilePath, FString& PluginFilePath) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryFindProject);

	// TODO: This could be heavily optimized by caching known project files

	int32 ContentIndex = PackagePath.Find(TEXT("/content/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	// Early out if there is not a single content directory in the path
	if (ContentIndex == INDEX_NONE)
	{
		UE_LOG(LogVirtualizationTool, Warning, TEXT("'%s' is not under a content directory"), *PackagePath);
		return false;
	}

	while (ContentIndex != INDEX_NONE)
	{
		// Assume that the project directory is the parent of the /content/ directory
		FString ProjectDirectory = PackagePath.Left(ContentIndex);
		FString PluginDirectory;
		
		TArray<FString> ProjectFile;
		TArray<FString> PluginFile;

		IFileManager::Get().FindFiles(ProjectFile, *ProjectDirectory, TEXT(".uproject"));

		if (ProjectFile.IsEmpty())
		{
			// If there was no project file, the package could be in a plugin, so lets check for that
			PluginDirectory = ProjectDirectory;
			IFileManager::Get().FindFiles(PluginFile, *PluginDirectory, TEXT(".uplugin"));

			if (PluginFile.Num() == 1)
			{
				PluginFilePath = PluginDirectory / PluginFile[0];

				// We have a valid plugin file, so we should be able to find a /plugins/ directory which will be just below the project directory
				const int32 PluginIndex = PluginDirectory.Find(TEXT("/plugins/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (PluginIndex != INDEX_NONE)
				{
					// We found the plugin root directory so the one above it should be the project directory
					ProjectDirectory = PluginDirectory.Left(PluginIndex);
					IFileManager::Get().FindFiles(ProjectFile, *ProjectDirectory, TEXT(".uproject"));
				}
			}
			else if (PluginFile.Num() > 1)
			{
				UE_LOG(LogVirtualizationTool, Warning, TEXT("Found multiple .uplugin files for '%s' at '%s'"), *PackagePath, *PluginDirectory);
				return false;
			}
		}

		if (ProjectFile.Num() == 1)
		{
			ProjectFilePath = ProjectDirectory / ProjectFile[0];
			return true;
		}
		else if (!ProjectFile.IsEmpty())
		{
			UE_LOG(LogVirtualizationTool, Warning, TEXT("Found multiple .uproject files for '%s' at '%s'"), *PackagePath, *ProjectDirectory);
			return false;
		}
		
		// Could be more than one content directory in the path so lets keep looking
		ContentIndex = PackagePath.Find(TEXT("/content/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, ContentIndex);
	}
	
	// We found one or more content directories but none of them contained a project file
	UE_LOG(LogVirtualizationTool, Warning, TEXT("Failed to find project file for '%s'"), *PackagePath);
	return false;
}

FProject& FUnrealVirtualizationToolApp::FindOrAddProject(FString&& ProjectFilePath)
{
	FProject* Project = Projects.FindByPredicate([&ProjectFilePath](const FProject& Project)->bool
	{
		return Project.GetProjectFilePath() == ProjectFilePath;
	});

	if (Project != nullptr)
	{
		return *Project;
	}
	else
	{
		int32 Index = Projects.Emplace(MoveTemp(ProjectFilePath));
		return Projects[Index];
	}
}

} // namespace UE::Virtualization
