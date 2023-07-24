// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealVirtualizationToolApp.h"

#include "Commands/CommandBase.h"
#include "Commands/RehydrateCommand.h"
#include "Commands/VirtualizeCommand.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FeedbackContextAnsi.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/StructuredLog.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Modules/ModuleManager.h"
#include "UnrealVirtualizationTool.h"
#include "Virtualization/VirtualizationSystem.h"

namespace
{

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
	else if (InString == TEXT("Virtualize"))
	{
		OutValue = UE::Virtualization::EMode::Virtualize;
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

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		Serialize(V, Verbosity, Category, -1.0);
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override
	{
		if (Verbosity == ELogVerbosity::Display && Category != LogVirtualizationTool.GetCategoryName())
		{
			Verbosity = ELogVerbosity::Log;
		}

		FFeedbackContextAnsi::Serialize(V, Verbosity, Category, Time);
	}

	virtual void SerializeRecord(const UE::FLogRecord& Record) override
	{
		if (Record.GetVerbosity() == ELogVerbosity::Display && Record.GetCategory() != LogVirtualizationTool.GetCategoryName())
		{
			UE::FLogRecord LocalRecord = Record;
			LocalRecord.SetVerbosity(ELogVerbosity::Log);
			return FFeedbackContextAnsi::SerializeRecord(LocalRecord);
		}

		FFeedbackContextAnsi::SerializeRecord(Record);
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

	TArray<FString> Packages = CurrentCommand->GetPackages();

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

	UE_LOG(LogVirtualizationTool, Display, TEXT("Running the '%s' command..."), *CurrentCommand->GetName());

	if (CurrentCommand->Run(Projects))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Command '%s' succeeded!"), *CurrentCommand->GetName());
		return true;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Command '%s' failed!"), *CurrentCommand->GetName());
		return false;
	}
}

void FUnrealVirtualizationToolApp::PrintCmdLineHelp() const
{
	UE_LOG(LogVirtualizationTool, Display, TEXT("Usage:"));

	// TODO: If the commands were registered in some way we could automate this
	FVirtualizeCommand::PrintCmdLineHelp(); 
	FRehydrateCommand::PrintCmdLineHelp();

	UE_LOG(LogVirtualizationTool, Display, TEXT(""));
	UE_LOG(LogVirtualizationTool, Display, TEXT("Legacy Commands:"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=Changelist -ClientSpecName=<name> [optional] -Changelist=<number> -nosubmit [optional]"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=PackageList -Path=<string>"));
	
	UE_LOG(LogVirtualizationTool, Display, TEXT(""));
	UE_LOG(LogVirtualizationTool, Display, TEXT("Global Options:"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\t-verbose (all log messages with display verbosity will be displayed, not just LogVirtualizationTool)"));
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
			CurrentCommand = CreateCommand<FVirtualizeLegacyChangeListCommand>(ModeAsString, CmdLine);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
			break;
		case EMode::PackageList:
			CurrentCommand = CreateCommand<FVirtualizeLegacyPackageListCommand>(ModeAsString, CmdLine);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
			break;
		case EMode::Virtualize:
			CurrentCommand = CreateCommand<FVirtualizeCommand>(ModeAsString, CmdLine);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
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
