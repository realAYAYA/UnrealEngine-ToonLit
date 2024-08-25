// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealVirtualizationToolApp.h"

#include "Commands/CommandBase.h"
#include "Commands/RehydrateCommand.h"
#include "Commands/VirtualizeCommand.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"
#include "HAL/FeedbackContextAnsi.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
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
#include "ProcessUtilities.h"
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

/** 
 * Utility to convert EMode to a string. NOTE that legacy modes will return the
 * name of the newer mode that replaces it and not the legacy mode.
 */
const TCHAR* LexToString(UE::Virtualization::EMode Mode)
{
	using namespace UE::Virtualization;

	switch (Mode)
	{
		case EMode::Unknown:
			return TEXT("Unknown");
		case EMode::Changelist:		// Legacy Mode
		case EMode::PackageList:	// Legacy Mode
		case EMode::Virtualize:
			return TEXT("Virtualize");
		case EMode::Rehydrate:
			return TEXT("Rehydrate");	
		default:
			checkNoEntry();
			return TEXT("");
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

/** Utility to create a file path for the child process input/output file */
void CreateChildProcessFilePath(FStringView Id, FStringView Extension, FStringBuilderBase& OutPath)
{
	FPathViews::ToAbsolutePath(FPaths::EngineSavedDir(), OutPath);
	OutPath << TEXT("UnrealVirtualizationTool/") << Id << TEXT(".") << Extension;
}

/** Utility to create a file path for the child process input/output file from a FGuid */
void CreateChildProcessFilePath(const FGuid& Id, FStringView Extension, FStringBuilderBase& OutPath)
{
	CreateChildProcessFilePath(WriteToString<40>(Id), Extension, OutPath);
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
#if !NO_LOGGING
		if (Verbosity == ELogVerbosity::Display && Category != LogVirtualizationTool.GetCategoryName())
#endif
		{
			Verbosity = ELogVerbosity::Log;
		}

		FFeedbackContextAnsi::Serialize(V, Verbosity, Category, Time);
	}

	virtual void SerializeRecord(const UE::FLogRecord& Record) override
	{
#if !NO_LOGGING
		if (Record.GetVerbosity() == ELogVerbosity::Display && Record.GetCategory() != LogVirtualizationTool.GetCategoryName())
		{
			UE::FLogRecord LocalRecord = Record;
			LocalRecord.SetVerbosity(ELogVerbosity::Log);
			return FFeedbackContextAnsi::SerializeRecord(LocalRecord);
		}
#endif

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

	if (!IsChildProcess())
	{
		TArray<FString> Packages = CurrentCommand->GetPackages();

		UE_LOG(LogVirtualizationTool, Display, TEXT("\tFound '%d' package file(s)"), Packages.Num());

		if (!TrySortFilesByProject(Packages))
		{
			return EInitResult::Error;
		}
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("Initialization complete!"));

	return EInitResult::Success;
}

bool FUnrealVirtualizationToolApp::Run()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Run);

	TArray<TUniquePtr<FCommandOutput>> OutputArray;
	if (!ProcessProjects(OutputArray))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Command '%s' failed!"), *CurrentCommand->GetName());
		return false;
	}

	if (!IsChildProcess())
	{
		if (!CurrentCommand->ProcessOutput(OutputArray))
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Command '%s' failed!"), *CurrentCommand->GetName());
			return false;
		}
	}
	else
	{
		if (!TryWriteChildProcessOutputFile(ChildProcessId, OutputArray))
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Command '%s' failed!"), *CurrentCommand->GetName());
			return false;
		}
	}


	UE_LOG(LogVirtualizationTool, Display, TEXT("Command '%s' succeeded!"), *CurrentCommand->GetName());
	return true;
}

bool FUnrealVirtualizationToolApp::ProcessProjects(TArray<TUniquePtr<FCommandOutput>>& OutputArray)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessProjects);

	UE_LOG(LogVirtualizationTool, Display, TEXT("Running the '%s' command..."), *CurrentCommand->GetName());

	OutputArray.Reserve(Projects.Num());

	for (const FProject& Project : Projects)
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Processing project: %s"), *Project.GetProjectFilePath());

		const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

		if (IsChildProcess() || ProjectPath == Project.GetProjectFilePath())
		{
			if (IsChildProcess() && ProjectPath != Project.GetProjectFilePath())
			{
				// If we are a child process then the correct project path should have been provided and so
				// this check is mostly a paranoid check to make sure things are working as we expect.
				UE_LOG(LogVirtualizationTool, Error, TEXT("The child process was create with project path '%s' but expected '%s'"), *ProjectPath, *Project.GetProjectFilePath());
				return false;
			}

			check(ProjectPath == Project.GetProjectFilePath());

			TUniquePtr<FCommandOutput> Output;
			if (!CurrentCommand->ProcessProject(Project, Output))
			{
				return false;
			}

			if (Output != nullptr)
			{
				OutputArray.Emplace(MoveTemp(Output));
			}
		}
		else
		{
			if (!LaunchChildProcess(*CurrentCommand, Project, GlobalCmdlineOptions, OutputArray))
			{
				return false;
			}
		}
	}

	return true;
}

void FUnrealVirtualizationToolApp::PrintCmdLineHelp() const
{
	UE_LOG(LogVirtualizationTool, Display, TEXT("Usage:"));

	UE_LOG(LogVirtualizationTool, Display, TEXT("Commands:"));
	// TODO: If the commands were registered in some way we could automate this
	FVirtualizeCommand::PrintCmdLineHelp(); 
	FRehydrateCommand::PrintCmdLineHelp();

	UE_LOG(LogVirtualizationTool, Display, TEXT("Legacy Commands:"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=Changelist -ClientSpecName=<name> [optional] -Changelist=<number> -nosubmit [optional]"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=PackageList -Path=<string>"));
	
	UE_LOG(LogVirtualizationTool, Display, TEXT(""));
	UE_LOG(LogVirtualizationTool, Display, TEXT("Global Options:"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\t-MinimalLogging (demote log messages with 'display' verbosity to 'log' verbosity except those using the LogVirtualizationTool category)"));
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

	auto LoadPlugin = [](const FString& PlugInName) -> bool
		{
			IPluginManager& PluginMgr = IPluginManager::Get();

			PluginMgr.MountNewlyCreatedPlugin(PlugInName);

			TSharedPtr<IPlugin> Plugin = PluginMgr.FindPlugin(PlugInName);
			if (Plugin == nullptr || !Plugin->IsEnabled())
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("The plugin '%s' is disabled."), *PlugInName);
				return false;
			}

			return true;
		};

	if (!LoadPlugin(TEXT("PerforceSourceControl")))
	{
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

	EInitResult GlobalOptionResult = TryParseGlobalOptions(CmdLine);
	if(GlobalOptionResult != EInitResult::Success)
	{
		return GlobalOptionResult;
	}

	// Check to see if we are a child process with an input file
	FString ChildProcessInput;
	if (FParse::Value(CmdLine, TEXT("-ChildProcess="), ChildProcessInput))
	{
		if (TryReadChildProcessInputFile(ChildProcessInput))
		{
			return EInitResult::Success;
		}
		else
		{
			return EInitResult::Error;
		}
	}

	// Now parse the mode specific command line options

	FString ModeAsString;
	if (!FParse::Value(CmdLine, TEXT("-Mode="), ModeAsString))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find cmdline switch 'Mode', this is a required parameter!"));
		PrintCmdLineHelp();
		return EInitResult::Error;
	}

	return CreateCommandFromString(ModeAsString, CmdLine);
}

EInitResult FUnrealVirtualizationToolApp::TryParseGlobalOptions(const TCHAR* CmdLine)
{
	GlobalCmdlineOptions.Reset();

	if (FParse::Param(CmdLine, TEXT("MinimalLogging")))
	{
		OutputDeviceOverride = MakeUnique<FOverrideOutputDevice>();

		AddGlobalOption(TEXT("-MinimalLogging"));	
	}
	else
	{
		OutputDeviceOverride.Reset();
	}

	return EInitResult::Success;
}

EInitResult FUnrealVirtualizationToolApp::CreateCommandFromString(const FString& CommandName, const TCHAR* Cmdline)
{
	check(Mode == EMode::Unknown && !CurrentCommand.IsValid());

	LexFromString(Mode, CommandName);

	switch (Mode)
	{
		case EMode::Changelist:
			CurrentCommand = CreateCommand<FVirtualizeLegacyChangeListCommand>(LexToString(Mode), Cmdline);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
			break;
		case EMode::PackageList:
			CurrentCommand = CreateCommand<FVirtualizeLegacyPackageListCommand>(LexToString(Mode), Cmdline);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
			break;
		case EMode::Virtualize:
			CurrentCommand = CreateCommand<FVirtualizeCommand>(CommandName, Cmdline);
			return CurrentCommand.IsValid() ? EInitResult::Success : EInitResult::Error;
			break;
		case EMode::Rehydrate:
			CurrentCommand = CreateCommand<FRehydrateCommand>(CommandName, Cmdline);
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

	UE_LOG(LogVirtualizationTool, Display, TEXT("\tSorting files by project..."));

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

bool FUnrealVirtualizationToolApp::IsChildProcess() const
{
	return !ChildProcessId.IsEmpty();
}

void FUnrealVirtualizationToolApp::AddGlobalOption(FStringView Options)
{
	if (!GlobalCmdlineOptions.IsEmpty())
	{
		GlobalCmdlineOptions.Append(TEXT(" "));
	}

	GlobalCmdlineOptions.Append(Options);
}

bool FUnrealVirtualizationToolApp::TryReadChildProcessOutputFile(const FGuid& ChildProcessId, const FCommand& Command, TArray<TUniquePtr<FCommandOutput>>& OutputArray)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryReadChildProcessOutputFile);

	TStringBuilder<512> FilePath;
	CreateChildProcessFilePath(ChildProcessId, TEXT("output"), FilePath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, FilePath.ToString()))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Could not open child process output file '%s'"), FilePath.ToString());
		return false;
	}

	TSharedPtr<FJsonObject> JsonRootObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonRootObject) || !JsonRootObject.IsValid())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to parse child process output file '%s'"), FilePath.ToString());
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputJsonArray = nullptr;
	if (JsonRootObject->TryGetArrayField(TEXT("OutputArray"), OutputJsonArray))
	{
		check(OutputJsonArray != nullptr);

		for (TSharedPtr<FJsonValue> OutputValue : *OutputJsonArray)
		{
			TUniquePtr<FCommandOutput> Output = Command.CreateOutputObject();

			const TSharedPtr<FJsonObject>* OutputObject = nullptr;

			if (!OutputValue->TryGetObject(OutputObject))
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("Invalid syntax found in child process output file '%s'"), FilePath.ToString());
				return false;
			}

			check(OutputObject != nullptr);
			if (!Output->FromJson(*OutputObject))
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to read FCommandOutput from the child process output file '%s'"), FilePath.ToString());
				return false;
			}

			OutputArray.Emplace(MoveTemp(Output));
		}
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Invalid syntax found in child process output file '%s'"), FilePath.ToString());
		return false;
	}

	return true;
}

bool FUnrealVirtualizationToolApp::TryWriteChildProcessOutputFile(const FString& ChildProcessId, const TArray<TUniquePtr<FCommandOutput>>& OutputArray)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryWriteChildProcessOutputFile);

	FString JsonTcharText;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonTcharText);

	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("OutputArray"));
	for (const TUniquePtr<FCommandOutput>& Output : OutputArray)
	{
		if (Output)
		{
			Writer->WriteObjectStart();
			Output->ToJson(Writer, true);
			Writer->WriteObjectEnd();
		}
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();

	if (!Writer->Close())
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Failed to create child process output file json document"));
		return false;
	}

	TStringBuilder<512> FilePath;
	CreateChildProcessFilePath(ChildProcessId, TEXT("output"), FilePath);

	if (!FFileHelper::SaveStringToFile(JsonTcharText, FilePath.ToString()))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Failed to create child process output file '%s'"), FilePath.ToString());
		return false;
	}

	return true;
}

bool FUnrealVirtualizationToolApp::TryReadChildProcessInputFile(const FString& InputPath)
{
	UE_LOG(LogVirtualizationTool, Display, TEXT("Parsing child process input file..."));

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *InputPath))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Could not open child process input file '%s'"), *InputPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonRootObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonRootObject) || !JsonRootObject.IsValid())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to parse child process input file '%s'"), *InputPath);
		return false;
	}

	if (TSharedPtr<FJsonValue> JsonValue = JsonRootObject->TryGetField(TEXT("CommandName")))
	{
		const FString CommandName = JsonValue->AsString();

		if (CreateCommandFromString(CommandName, TEXT("")) != EInitResult::Success)
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to create command '%s'"), *CommandName);
			return false;
		}
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find 'CommandName' in child process input file '%s'"), *InputPath);
		return false;
	}

	if (TSharedPtr<FJsonObject> JsonObject = JsonRootObject->GetObjectField(TEXT("ProjectData")))
	{
		FProject Project;
		if (Project.FromJson(JsonObject))
		{
			Projects.Emplace(MoveTemp(Project));
		}
		else
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to serialize project data from child process input file '%s'"), *InputPath);
			return false;
		}
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find 'ProjectData' in child process input file '%s'"), *InputPath);
		return false;
	}

	if (TSharedPtr<FJsonObject> JsonObject = JsonRootObject->GetObjectField(TEXT("CommandData")))
	{
		CurrentCommand->FromJson(JsonObject);
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find 'ProjectData' in child process input file '%s'"), *InputPath);
		return false;
	}

	ChildProcessId = FPathViews::GetBaseFilename(InputPath);

	return true;
}

bool FUnrealVirtualizationToolApp::TryWriteChildProcessInputFile(const FGuid& ChildProcessId, const FCommand& Command, const FProject& Project, FStringBuilderBase& OutPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryWriteChildProcessInputFile);

	FString JsonTcharText;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonTcharText);
	Writer->WriteObjectStart();

	Writer->WriteValue(TEXT("CommandName"), Command.GetName());

	Writer->WriteObjectStart(TEXT("ProjectData"));
	Project.ToJson(Writer, true);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("CommandData"));
	Command.ToJson(Writer);
	Writer->WriteObjectEnd();

	Writer->WriteObjectEnd();

	if (!Writer->Close())
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Failed to create child process input file json document"));
		return false;
	}

	CreateChildProcessFilePath(ChildProcessId, TEXT("input"), OutPath);

	if (!FFileHelper::SaveStringToFile(JsonTcharText, OutPath.ToString()))
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("Failed to save child process input file '%s'"), OutPath.ToString());
		return false;
	}

	return true;
}

bool FUnrealVirtualizationToolApp::LaunchChildProcess(const FCommand& Command, const FProject& Project, FStringView GlobalOptions, TArray<TUniquePtr<FCommandOutput>>& OutputArray)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LaunchChildProcess);

	UE_LOG(LogVirtualizationTool, Display, TEXT("Launching and waiting on a new instance of the tool..."));

	const FGuid ChildProcessId = FGuid::NewGuid();

	TStringBuilder<512> InputFilePath;
	if (!TryWriteChildProcessInputFile(ChildProcessId, Command, Project, InputFilePath))
	{
		// No need to log an error here, ::TryWriteChildProcessInputFile will take care of that
		return false;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RunChildProcess);

		const FString CurrentExePath = FPlatformProcess::ExecutablePath();
		FString Args = FString::Printf(TEXT("%s -ChildProcess=%s"), *Project.GetProjectFilePath(), InputFilePath.ToString());

		if (!GlobalOptions.IsEmpty())
		{
			Args.Append(TEXT(" "));
			Args.Append(GlobalOptions);
		}

		const bool bLaunchDetached = false;
		const bool bLaunchHidden = true;
		const bool bLaunchReallyHidden = true;
		uint32 ProcessId = INDEX_NONE;
		const int32 Priority = 0;
		const TCHAR* WorkingDirectory = nullptr;

		FProcessPipes Pipes;
		FProcHandle Handle = FPlatformProcess::CreateProc(*CurrentExePath, *Args, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden,
			/*OutProcessID*/ nullptr, Priority, WorkingDirectory, Pipes.GetStdIn(), Pipes.GetStdOut());

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(WaitOnChildProcess);
			while (FPlatformProcess::IsProcRunning(Handle))
			{
				Pipes.ProcessStdOut();
				FPlatformProcess::Sleep(0.033f);
			}
		}

		int32 ReturnCode = INDEX_NONE;
		if (!FPlatformProcess::GetProcReturnCode(Handle, &ReturnCode))
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("Failed to retrieve the return value of the child process"));
			return false;
		}

		if (ReturnCode != 0)
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("Child process failed with error code: %d"), ReturnCode);
			return false;
		}
	}

	if (!TryReadChildProcessOutputFile(ChildProcessId, Command, OutputArray))
	{
		// No need to log an error here, ::TryReadChildProcessOutputFile will take care of that
		return false;
	}

	return true;
}

} // namespace UE::Virtualization
