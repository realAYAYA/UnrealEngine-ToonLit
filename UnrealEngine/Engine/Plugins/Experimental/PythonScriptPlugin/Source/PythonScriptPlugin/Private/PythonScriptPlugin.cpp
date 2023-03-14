// Copyright Epic Games, Inc. All Rights Reserved.

#include "PythonScriptPlugin.h"
#include "PythonScriptPluginSettings.h"
#include "PythonScriptPluginStyle.h"
#include "PythonScriptRemoteExecution.h"
#include "PyGIL.h"
#include "PyCore.h"
#include "PySlate.h"
#include "PyEngine.h"
#include "PyEditor.h"
#include "PyConstant.h"
#include "PyConversion.h"
#include "PyMethodWithClosure.h"
#include "PyReferenceCollector.h"
#include "PyWrapperTypeRegistry.h"
#include "EngineAnalytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/PackageReload.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Char.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Ticker.h"
#include "Features/IModularFeatures.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "String/Find.h"

#if WITH_EDITOR
#include "EditorSupportDelegates.h"
#include "DesktopPlatformModule.h"
#include "Styling/AppStyle.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "AssetViewUtils.h"
#include "IContentBrowserDataModule.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserFileDataCore.h"
#include "ContentBrowserFileDataSource.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#endif	// WITH_EDITOR

#if PLATFORM_WINDOWS
	#include <stdio.h>
	#include <fcntl.h>
	#include <io.h>
#endif	// PLATFORM_WINDOWS
#include <locale.h>

#define LOCTEXT_NAMESPACE "PythonScriptPlugin"

#if WITH_PYTHON

static PyUtil::FPyApiBuffer NullPyArg = PyUtil::TCHARToPyApiBuffer(TEXT(""));
static PyUtil::FPyApiChar* NullPyArgPtrs[] = { NullPyArg.GetData() };

/** Util struct to set the sys.argv data for Python when executing a file with arguments */
struct FPythonScopedArgv
{
	FPythonScopedArgv(const TCHAR* InArgs)
	{
		if (InArgs && *InArgs)
		{
			FString NextToken;
			while (FParse::Token(InArgs, NextToken, false))
			{
				PyCommandLineArgs.Add(PyUtil::TCHARToPyApiBuffer(*NextToken));
			}

			PyCommandLineArgPtrs.Reserve(PyCommandLineArgs.Num());
			for (PyUtil::FPyApiBuffer& PyCommandLineArg : PyCommandLineArgs)
			{
				PyCommandLineArgPtrs.Add(PyCommandLineArg.GetData());
			}
		}
		PySys_SetArgvEx(PyCommandLineArgPtrs.Num(), PyCommandLineArgPtrs.GetData(), 0);
	}

	~FPythonScopedArgv()
	{
		PySys_SetArgvEx(1, NullPyArgPtrs, 0);
	}

	TArray<PyUtil::FPyApiBuffer> PyCommandLineArgs;
	TArray<PyUtil::FPyApiChar*> PyCommandLineArgPtrs;
};

FPythonCommandExecutor::FPythonCommandExecutor(IPythonScriptPlugin* InPythonScriptPlugin)
	: PythonScriptPlugin(InPythonScriptPlugin)
{
}

FName FPythonCommandExecutor::StaticName()
{
	static const FName CmdExecName = TEXT("Python");
	return CmdExecName;
}

FName FPythonCommandExecutor::GetName() const
{
	return StaticName();
}

FText FPythonCommandExecutor::GetDisplayName() const
{
	return LOCTEXT("PythonCommandExecutorDisplayName", "Python");
}

FText FPythonCommandExecutor::GetDescription() const
{
	return LOCTEXT("PythonCommandExecutorDescription", "Execute Python scripts (including files)");
}

FText FPythonCommandExecutor::GetHintText() const
{
	return LOCTEXT("PythonCommandExecutorHintText", "Enter Python script or a filename");
}

void FPythonCommandExecutor::GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out)
{
}

void FPythonCommandExecutor::GetExecHistory(TArray<FString>& Out)
{
	IConsoleManager::Get().GetConsoleHistory(TEXT("Python"), Out);
}

bool FPythonCommandExecutor::Exec(const TCHAR* Input)
{
	IConsoleManager::Get().AddConsoleHistoryEntry(TEXT("Python"), Input);

	UE_LOG(LogPython, Log, TEXT("%s"), Input);

	PythonScriptPlugin->ExecPythonCommand(Input);

	return true;
}

bool FPythonCommandExecutor::AllowHotKeyClose() const
{
	return true;
}

bool FPythonCommandExecutor::AllowMultiLine() const
{
	return true;
}

FInputChord FPythonCommandExecutor::GetHotKey() const
{
#if WITH_EDITOR
	return FGlobalEditorCommonCommands::Get().OpenConsoleCommandBox->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
#else
	return FInputChord();
#endif
}

FInputChord FPythonCommandExecutor::GetIterateExecutorHotKey() const
{
#if WITH_EDITOR
	return FGlobalEditorCommonCommands::Get().SelectNextConsoleExecutor->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
#else
	return FInputChord();
#endif
}

FPythonREPLCommandExecutor::FPythonREPLCommandExecutor(IPythonScriptPlugin* InPythonScriptPlugin)
	: PythonScriptPlugin(InPythonScriptPlugin)
{
}

FName FPythonREPLCommandExecutor::StaticName()
{
	static const FName CmdExecName = TEXT("PythonREPL");
	return CmdExecName;
}

FName FPythonREPLCommandExecutor::GetName() const
{
	return StaticName();
}

FText FPythonREPLCommandExecutor::GetDisplayName() const
{
	return LOCTEXT("PythonREPLCommandExecutorDisplayName", "Python (REPL)");
}

FText FPythonREPLCommandExecutor::GetDescription() const
{
	return LOCTEXT("PythonREPLCommandExecutorDescription", "Execute a single Python statement and show its result");
}

FText FPythonREPLCommandExecutor::GetHintText() const
{
	return LOCTEXT("PythonREPLCommandExecutorHintText", "Enter a Python statement");
}

void FPythonREPLCommandExecutor::GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out)
{
}

void FPythonREPLCommandExecutor::GetExecHistory(TArray<FString>& Out)
{
	IConsoleManager::Get().GetConsoleHistory(TEXT("PythonREPL"), Out);
}

bool FPythonREPLCommandExecutor::Exec(const TCHAR* Input)
{
	IConsoleManager::Get().AddConsoleHistoryEntry(TEXT("PythonREPL"), Input);

	UE_LOG(LogPython, Log, TEXT("%s"), Input);

	FPythonCommandEx PythonCommand;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
	PythonCommand.Command = Input;
	PythonScriptPlugin->ExecPythonCommandEx(PythonCommand);

	return true;
}

bool FPythonREPLCommandExecutor::AllowHotKeyClose() const
{
	return true;
}

bool FPythonREPLCommandExecutor::AllowMultiLine() const
{
	return true;
}

FInputChord FPythonREPLCommandExecutor::GetHotKey() const
{
#if WITH_EDITOR
	return FGlobalEditorCommonCommands::Get().OpenConsoleCommandBox->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
#else
	return FInputChord();
#endif
}

FInputChord FPythonREPLCommandExecutor::GetIterateExecutorHotKey() const
{
#if WITH_EDITOR
	return FGlobalEditorCommonCommands::Get().SelectNextConsoleExecutor->GetActiveChord(EMultipleKeyBindingIndex::Primary).Get();
#else
	return FInputChord();
#endif
}

#if WITH_EDITOR
class FPythonCommandMenuImpl : public IPythonCommandMenu
{
public:
	FPythonCommandMenuImpl()
		: bRecentsFilesDirty(false)
	{
	}

	virtual void OnStartupMenu() override
	{
		LoadConfig();

		RegisterMenus();
	}

	virtual void OnShutdownMenu() override
	{
		UToolMenus::UnregisterOwner(this);

		// Write to file
		if (bRecentsFilesDirty)
		{
			SaveConfig();
			bRecentsFilesDirty = false;
		}
	}

	virtual void OnRunFile(const FString& InFile, bool bAdd) override
	{
		if (bAdd)
		{
			int32 Index = RecentsFiles.Find(InFile);
			if (Index != INDEX_NONE)
			{
				// If already in the list but not at the last position
				if (Index != RecentsFiles.Num() - 1)
				{
					RecentsFiles.RemoveAt(Index);
					RecentsFiles.Add(InFile);
					bRecentsFilesDirty = true;
				}
			}
			else
			{
				if (RecentsFiles.Num() >= MaxNumberOfFiles)
				{
					RecentsFiles.RemoveAt(0);
				}
				RecentsFiles.Add(InFile);
				bRecentsFilesDirty = true;
			}
		}
		else
		{
			if (RecentsFiles.RemoveSingle(InFile) > 0)
			{
				bRecentsFilesDirty = true;
			}
		}
	}

private:
	const TCHAR* STR_ConfigSection = TEXT("Python");
	const TCHAR* STR_ConfigDirectoryKey = TEXT("LastDirectory");
	const FName NAME_ConfigRecentsFilesyKey = TEXT("RecentsFiles");
	static const int32 MaxNumberOfFiles = 10;

	void LoadConfig()
	{
		RecentsFiles.Reset();

		GConfig->GetString(STR_ConfigSection, STR_ConfigDirectoryKey, LastDirectory, GEditorPerProjectIni);

		FConfigSection* Sec = GConfig->GetSectionPrivate(STR_ConfigSection, false, true, GEditorPerProjectIni);
		if (Sec)
		{
			TArray<FConfigValue> List;
			Sec->MultiFind(NAME_ConfigRecentsFilesyKey, List);

			int32 ListNum = FMath::Min(List.Num(), MaxNumberOfFiles);

			RecentsFiles.Reserve(ListNum);
			for (int32 Index = 0; Index < ListNum; ++Index)
			{
				RecentsFiles.Add(List[Index].GetValue());
			}
		}
	}

	void SaveConfig() const
	{
		GConfig->SetString(STR_ConfigSection, STR_ConfigDirectoryKey, *LastDirectory, GEditorPerProjectIni);

		FConfigSection* Sec = GConfig->GetSectionPrivate(STR_ConfigSection, true, false, GEditorPerProjectIni);
		if (Sec)
		{
			Sec->Remove(NAME_ConfigRecentsFilesyKey);
			for (int32 Index = RecentsFiles.Num() - 1; Index >= 0; --Index)
			{
				Sec->Add(NAME_ConfigRecentsFilesyKey, *RecentsFiles[Index]);
			}
		}

		GConfig->Flush(false);
	}

	void MakeRecentPythonScriptMenu(UToolMenu* InMenu)
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		FToolMenuSection& Section = InMenu->AddSection("Files");
		for (int32 Index = RecentsFiles.Num() - 1; Index >= 0; --Index)
		{
			Section.AddMenuEntry(
				NAME_None,
				FText::FromString(RecentsFiles[Index]),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FPythonCommandMenuImpl::Menu_ExecutePythonRecent, Index))
			);
		}

		FToolMenuSection& ClearSection = InMenu->AddSection("Clear");
		Section.AddMenuEntry(
			"ClearRecentPython",
			LOCTEXT("ClearRecentPython", "Clear Recent Python Scripts"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FPythonCommandMenuImpl::Menu_ClearRecentPython))
		);
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("Python", LOCTEXT("Python", "Python"));//, FToolMenuInsert("FileLoadAndSave", EToolMenuInsertType::After));
		Section.AddMenuEntry(
			"OpenPython",
			LOCTEXT("OpenPython", "Execute Python Script..."),
			LOCTEXT("OpenPythonTooltip", "Open a Python Script file and Execute it."),
			FSlateIcon(FPythonScriptPluginEditorStyle::Get().GetStyleSetName(), "Icons.PythonExecute"),
			FUIAction(FExecuteAction::CreateRaw(this, &FPythonCommandMenuImpl::Menu_ExecutePython))
		);
		Section.AddSubMenu(
			"RecentPythonsSubMenu",
			LOCTEXT("RecentPythonsSubMenu", "Recent Python Scripts"),
			LOCTEXT("RecentPythonsSubMenu_ToolTip", "Select a recent Python Script file and Execute it."),
			FNewToolMenuDelegate::CreateRaw(this, &FPythonCommandMenuImpl::MakeRecentPythonScriptMenu), false,
			FSlateIcon(FPythonScriptPluginEditorStyle::Get().GetStyleSetName(), "Icons.PythonRecent")
		);
	}

	void Menu_ExecutePythonRecent(int32 Index)
	{
		if (RecentsFiles.IsValidIndex(Index))
		{
			FString PyCopied = RecentsFiles[Index];
			GEngine->Exec(NULL, *FString::Printf(TEXT("py \"%s\""), *PyCopied));
		}
	}

	void Menu_ClearRecentPython()
	{
		if (RecentsFiles.Num() > 0)
		{
			RecentsFiles.Reset();
			bRecentsFilesDirty = true;
		}
	}

	void Menu_ExecutePython()
	{
		TArray<FString> OpenedFiles;
		FString DefaultDirectory = LastDirectory;

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform)
		{
			bool bOpened = DesktopPlatform->OpenFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				LOCTEXT("ExecutePython", "Execute Python Script").ToString(),
				DefaultDirectory,
				TEXT(""),
				TEXT("Python files|*.py|"),
				EFileDialogFlags::None,
				OpenedFiles
			);

			if (bOpened && OpenedFiles.Num() > 0)
			{
				if (DefaultDirectory != LastDirectory)
				{
					LastDirectory = DefaultDirectory;
					bRecentsFilesDirty = true;
				}

				GEngine->Exec(NULL, *FString::Printf(TEXT("py \"%s\""), *OpenedFiles.Last()));
			}
		}
	}

private:

	TArray<FString> RecentsFiles;
	FString LastDirectory;

	bool bRecentsFilesDirty;
};
#endif // WITH_EDITOR

#endif	// WITH_PYTHON

FPythonScriptPlugin::FPythonScriptPlugin()
#if WITH_PYTHON
	: CmdExec(this)
	, CmdREPLExec(this)
	, CmdMenu(nullptr)
	, bInitialized(false)
	, bHasTicked(false)
#endif	// WITH_PYTHON
{
}

bool FPythonScriptPlugin::IsPythonAvailable() const
{
	static const bool bDisablePython = FParse::Param(FCommandLine::Get(), TEXT("DisablePython"));
	return WITH_PYTHON && !bDisablePython;
}

bool FPythonScriptPlugin::ExecPythonCommand(const TCHAR* InPythonCommand)
{
	FPythonCommandEx PythonCommand;
	PythonCommand.Command = InPythonCommand;
	return ExecPythonCommandEx(PythonCommand);
}

bool FPythonScriptPlugin::ExecPythonCommandEx(FPythonCommandEx& InOutPythonCommand)
{
#if WITH_PYTHON
	if (!IsPythonAvailable())
#endif
	{
		InOutPythonCommand.CommandResult = TEXT("Python is not available!");
		ensureAlwaysMsgf(false, TEXT("%s"), *InOutPythonCommand.CommandResult);
		return false;
	}

#if WITH_PYTHON
	if (InOutPythonCommand.ExecutionMode == EPythonCommandExecutionMode::ExecuteFile)
	{
		// The EPythonCommandExecutionMode::ExecuteFile name is misleading as it is used to run literal code or a .py file. Detect
		// if the user supplied a .py files. The command can have the python pathname with spaces, command line parameter(s) and could be quoted.
		//   C:\My Scripts\Test.py -param1 -param2     -> Ok
		//   "C:\My Scripts\Test.py  " -param1 -param2 -> Ok
		//   "C:\My Scripts\Test.py"-param1 -param2    -> Ok
		//   C:\My Scripts\Test.py "param with spaces" -> OK
		//   C:\My Scripts\Test.py-param1 -param2      -> Error missing a space between .py and -param1
		//   "C:\My Scripts\Test.py                    -> Error missing closing quote.
		//   C:\My Scripts\Test.py  "                  -> Error missing opening quote.
		//   test_wrapper_types.py                     -> Search the 'sys.path' to find the script.
		auto TryExtractPathnameAndCommand = [&InOutPythonCommand](FString& OutExtractedFilename, FString& OutExtractedCommand) -> bool
		{
			const TCHAR* PyFileExtension = TEXT(".py");
			int32 Pos = UE::String::FindFirst(InOutPythonCommand.Command, PyFileExtension);
			if (Pos == INDEX_NONE)
			{
				return false; // no .py file extension found.
			}

			int32 EndPathnamePos = Pos + FCString::Strlen(PyFileExtension);
			OutExtractedFilename = InOutPythonCommand.Command.Left(EndPathnamePos);
			bool bCommandQuoted = false;
			
			// The caller may quote the pathname if it contains space(s). Trim a leading quote, if any. (Any trailing quote is already removed by the Left() command).
			if (OutExtractedFilename.StartsWith(TEXT("\"")))
			{
				OutExtractedFilename.RemoveAt(0);
				bCommandQuoted = true;
			}

			// If the pathname started with a quote, expect a closing quote after the .py.
			if (bCommandQuoted)
			{
				if (EndPathnamePos == InOutPythonCommand.Command.Len())
				{
					return false; // Missing the closing quote.
				}

				// Scan after the .py to find the closing quote.
				for (int32 CurrPos = EndPathnamePos; CurrPos < InOutPythonCommand.Command.Len(); ++CurrPos)
				{
					if (InOutPythonCommand.Command[CurrPos] == TEXT('\"'))
					{
						EndPathnamePos = CurrPos + 1; // +1 to put the end just after the quote like a std::end() iterator.
						break;
					}
					else if (FChar::IsWhitespace(InOutPythonCommand.Command[CurrPos]))
					{
						continue; // It is legal to have blank space after the .py.
					}
					else
					{
						return false; // Invalid character found after .py.
					}
				}
			}
			// Some characters appears after the .py
			else if (EndPathnamePos < InOutPythonCommand.Command.Len() && !FChar::IsWhitespace(InOutPythonCommand.Command[EndPathnamePos]))
			{
				return false; // Some non-blank characters are there and we don't expect a closing quote. This is not a valid command Ex: C:\MyScript.py-t
			}

			// Quote/re-quote the command. This allows Python to set sys.argv/sys.argc property if the pathname contains space. (So parts of the pathname are not interpreted as arguments)
			OutExtractedCommand = FString::Printf(TEXT("\"%s\""), *OutExtractedFilename);

			// Append the arguments (if any).
			if (EndPathnamePos < InOutPythonCommand.Command.Len())
			{
				OutExtractedCommand += InOutPythonCommand.Command.Mid(EndPathnamePos);
			}

			return true; // Pathname, command and argument were successfully parsed.
		};

		FString ExtractedFilename;
		FString ExtractedCommand;

		if (TryExtractPathnameAndCommand(ExtractedFilename, ExtractedCommand))
		{
			return RunFile(*ExtractedFilename, *ExtractedCommand, InOutPythonCommand);
		}
		else
		{
			return RunString(InOutPythonCommand);
		}
	}
	else
	{
		return RunString(InOutPythonCommand);
	}	
#endif	// WITH_PYTHON
}

FSimpleMulticastDelegate& FPythonScriptPlugin::OnPythonInitialized()
{
	return OnPythonInitializedDelegate;
}

FSimpleMulticastDelegate& FPythonScriptPlugin::OnPythonShutdown()
{
	return OnPythonShutdownDelegate;
}

void FPythonScriptPlugin::StartupModule()
{
	if (!IsPythonAvailable())
	{
		return;
	}

#if WITH_PYTHON
	LLM_SCOPE_BYNAME(TEXT("PythonScriptPlugin"));

	InitializePython();
	IModularFeatures::Get().RegisterModularFeature(IConsoleCommandExecutor::ModularFeatureName(), &CmdExec);
	IModularFeatures::Get().RegisterModularFeature(IConsoleCommandExecutor::ModularFeatureName(), &CmdREPLExec);

	check(!RemoteExecution);
	RemoteExecution = MakeUnique<FPythonScriptRemoteExecution>(this);

#if WITH_EDITOR
	FPythonScriptPluginEditorStyle::Get();
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FPythonScriptPlugin::OnPostEngineInit);
#endif // WITH_EDITOR

	FCoreDelegates::OnPreExit.AddRaw(this, &FPythonScriptPlugin::ShutdownPython);
#endif	// WITH_PYTHON
}

#if WITH_PYTHON
#if WITH_EDITOR
void FPythonScriptPlugin::OnPostEngineInit()
{
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		check(CmdMenu == nullptr);
		CmdMenu = new FPythonCommandMenuImpl();
		CmdMenu->OnStartupMenu();

		UToolMenus::Get()->RegisterStringCommandHandler("Python", FToolMenuExecuteString::CreateLambda([this](const FString& InString, const FToolMenuContext& InContext) {
			ExecPythonCommand(*InString);
		}));
	}
}

#endif // WITH_EDITOR
#endif // WITH_PYTHON

void FPythonScriptPlugin::ShutdownModule()
{
	if (!IsPythonAvailable())
	{
		return;
	}

#if WITH_PYTHON
	FCoreDelegates::OnPreExit.RemoveAll(this);

	RemoteExecution.Reset();

#if WITH_EDITOR
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	if (CmdMenu)
	{
		CmdMenu->OnShutdownMenu();
		delete CmdMenu;
		CmdMenu = nullptr;
	}

	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->UnregisterStringCommandHandler("Python");
	}
#endif // WITH_EDITOR

	IModularFeatures::Get().UnregisterModularFeature(IConsoleCommandExecutor::ModularFeatureName(), &CmdExec);
	IModularFeatures::Get().UnregisterModularFeature(IConsoleCommandExecutor::ModularFeatureName(), &CmdREPLExec);
	ShutdownPython();
#endif	// WITH_PYTHON
}

bool FPythonScriptPlugin::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if WITH_PYTHON
	if (FParse::Command(&Cmd, TEXT("PY")))
	{
		ExecPythonCommand(Cmd);
		return true;
	}
#endif	// WITH_PYTHON
	return false;
}

void FPythonScriptPlugin::PreChange(const UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info)
{
}

void FPythonScriptPlugin::PostChange(const UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info)
{
#if WITH_PYTHON
	OnAssetUpdated(Enum);
#endif //WITH_PYTHON
}

#if WITH_PYTHON

void FPythonScriptPlugin::InitializePython()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::InitializePython)

	bInitialized = true;

	const UPythonScriptPluginSettings* PythonPluginSettings = GetDefault<UPythonScriptPluginSettings>();

	// Set-up the correct program name
	{
		FString ProgramName = FPlatformProcess::GetCurrentWorkingDirectory() / FPlatformProcess::ExecutableName(false);
		FPaths::NormalizeFilename(ProgramName);
		PyProgramName = PyUtil::TCHARToPyApiBuffer(*ProgramName);
	}

	// Set-up the correct home path
	{
		// Build the full Python directory (UE_PYTHON_DIR may be relative to the engine directory for portability)
		FString PythonDir = UTF8_TO_TCHAR(UE_PYTHON_DIR);
		PythonDir.ReplaceInline(TEXT("{ENGINE_DIR}"), *FPaths::EngineDir(), ESearchCase::CaseSensitive);
		FPaths::NormalizeDirectoryName(PythonDir);
		FPaths::RemoveDuplicateSlashes(PythonDir);
		PyHomePath = PyUtil::TCHARToPyApiBuffer(*PythonDir);
	}

	// Initialize the Python interpreter
	{
		static_assert(PY_MAJOR_VERSION >= 3, "Unreal Engine Python integration doesn't support versions prior to Python 3.x");
		UE_LOG(LogPython, Log, TEXT("Using Python %d.%d.%d"), PY_MAJOR_VERSION, PY_MINOR_VERSION, PY_MICRO_VERSION);

		// Python 3 changes the console mode from O_TEXT to O_BINARY which affects other uses of the console
		// So change the console mode back to its current setting after Py_Initialize has been called
#if PLATFORM_WINDOWS
		// We call _setmode here to cache the current state
		CA_SUPPRESS(6031)
		fflush(stdin);
		const int StdInMode  = _setmode(_fileno(stdin), _O_TEXT);
		CA_SUPPRESS(6031)
		fflush(stdout);
		const int StdOutMode = _setmode(_fileno(stdout), _O_TEXT);
		CA_SUPPRESS(6031)
		fflush(stderr);
		const int StdErrMode = _setmode(_fileno(stderr), _O_TEXT);
#endif	// PLATFORM_WINDOWS

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 7
		// Python 3.7+ changes the C locale which affects functions using C string APIs
		// So change the C locale back to its current setting after Py_Initialize has been called
		FString CurrentLocale;
		if (const char* CurrentLocalePtr = setlocale(LC_ALL, nullptr))
		{
			CurrentLocale = ANSI_TO_TCHAR(CurrentLocalePtr);
		}
#endif	// PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 7

		// Check if the interpreter is should run in isolation mode.
		int IsolatedInterpreterFlag = PythonPluginSettings->bIsolateInterpreterEnvironment ? 1 : 0;
		Py_IgnoreEnvironmentFlag = IsolatedInterpreterFlag; // If not zero, ignore all PYTHON* environment variables, e.g. PYTHONPATH, PYTHONHOME, that might be set.

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 4
		Py_IsolatedFlag = IsolatedInterpreterFlag; // If not zero, sys.path contains neither the script's directory nor the user's site-packages directory.
		Py_SetStandardStreamEncoding("utf-8", nullptr);
#endif	// PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 4
		Py_SetProgramName(PyProgramName.GetData());
		Py_SetPythonHome(PyHomePath.GetData());
		Py_InitializeEx(0); // 0 so Python doesn't override any signal handling

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION < 7
		// NOTE: Since 3.7, those functions are called by Py_InitializeEx()
		// 
		// Ensure Python supports multiple threads via the GIL, as UE GC runs over multiple threads, 
		// which may invoke FPyReferenceCollector::AddReferencedObjects on a background thread...
		if (!PyEval_ThreadsInitialized())
		{
			PyEval_InitThreads();
		}
#endif // PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION < 7

#if PLATFORM_WINDOWS
		// We call _setmode here to restore the previous state
		if (StdInMode != -1)
		{
			CA_SUPPRESS(6031)
			fflush(stdin);
			CA_SUPPRESS(6031)
			_setmode(_fileno(stdin), StdInMode);
		}
		if (StdOutMode != -1)
		{
			CA_SUPPRESS(6031)
			fflush(stdout);
			CA_SUPPRESS(6031)
			_setmode(_fileno(stdout), StdOutMode);
		}
		if (StdErrMode != -1)
		{
			CA_SUPPRESS(6031)
			fflush(stderr);
			CA_SUPPRESS(6031)
			_setmode(_fileno(stderr), StdErrMode);
		}
#endif	// PLATFORM_WINDOWS

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 7
		// We call setlocale here to restore the previous state
		if (!CurrentLocale.IsEmpty())
		{
			setlocale(LC_ALL, TCHAR_TO_ANSI(*CurrentLocale));
		}
#endif	// PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 7

		PySys_SetArgvEx(1, NullPyArgPtrs, 0);

		// Enable developer warnings if requested
		if (IsDeveloperModeEnabled())
		{
			PyUtil::EnableDeveloperWarnings();
		}

		// Check if the user wants type hinting. (In the stub and/or Docstrings).
		PyGenUtil::SetTypeHintingMode(GetTypeHintingMode());

		// Initialize our custom method type as we'll need it when generating bindings
		InitializePyMethodWithClosure();

		// Initialize our custom constant type as we'll need it when generating bindings
		InitializePyConstant();

		PyObject* PyMainModule = PyImport_AddModule("__main__");
		PyDefaultGlobalDict = FPyObjectPtr::NewReference(PyModule_GetDict(PyMainModule));
		PyDefaultLocalDict = PyDefaultGlobalDict;

		PyConsoleGlobalDict = FPyObjectPtr::StealReference(PyDict_Copy(PyDefaultGlobalDict));
		PyConsoleLocalDict = PyConsoleGlobalDict;

#if WITH_EDITOR
		FEditorSupportDelegates::PrepareToCleanseEditorObject.AddRaw(this, &FPythonScriptPlugin::OnPrepareToCleanseEditorObject);
#endif	// WITH_EDITOR
	}

	// Set-up the known Python script paths
	{
		PyUtil::AddSystemPath(FPaths::ConvertRelativePathToFull(FPlatformProcess::UserDir() / FApp::GetEpicProductIdentifier() / TEXT("Python")));

		TArray<FString> RootPaths;
		FPackageName::QueryRootContentPaths(RootPaths);
		for (const FString& RootPath : RootPaths)
		{
			const FString RootFilesystemPath = FPackageName::LongPackageNameToFilename(RootPath);
			RegisterModulePaths(RootFilesystemPath);
		}

		for (const FDirectoryPath& AdditionalPath : PythonPluginSettings->AdditionalPaths)
		{
			PyUtil::AddSystemPath(FPaths::ConvertRelativePathToFull(AdditionalPath.Path));
		}

		TArray<FString> SystemEnvPaths;
		FPlatformMisc::GetEnvironmentVariable(TEXT("UE_PYTHONPATH")).ParseIntoArray(SystemEnvPaths, FPlatformMisc::GetPathVarDelimiter());
		for (const FString& SystemEnvPath : SystemEnvPaths)
		{
			PyUtil::AddSystemPath(SystemEnvPath);
		}

		FPackageName::OnContentPathMounted().AddRaw(this, &FPythonScriptPlugin::OnContentPathMounted);
		FPackageName::OnContentPathDismounted().AddRaw(this, &FPythonScriptPlugin::OnContentPathDismounted);
		FCoreUObjectDelegates::OnPackageReloaded.AddRaw(this, &FPythonScriptPlugin::OnAssetReload);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FPythonScriptPlugin::OnAssetRenamed);
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FPythonScriptPlugin::OnAssetRemoved);
	}

	// Initialize the Unreal Python module
	{
		// Create the top-level "unreal" module
		PyUnrealModule = FPyObjectPtr::NewReference(PyImport_AddModule("unreal"));
		
		// Import "unreal" into the console by default
		PyDict_SetItemString(PyConsoleGlobalDict, "unreal", PyUnrealModule);

		// Initialize the and import the "core" module
		PyCore::InitializeModule();
		ImportUnrealModule(TEXT("core"));

		// Initialize the and import the "slate" module
		PySlate::InitializeModule();
		ImportUnrealModule(TEXT("slate"));

		// Initialize the and import the "engine" module
		PyEngine::InitializeModule();
		ImportUnrealModule(TEXT("engine"));

#if WITH_EDITOR
		// Initialize the and import the "editor" module
		PyEditor::InitializeModule();
		ImportUnrealModule(TEXT("editor"));
#endif	// WITH_EDITOR

		FPyWrapperTypeRegistry::Get().OnModuleDirtied().AddRaw(this, &FPythonScriptPlugin::OnModuleDirtied);
		FModuleManager::Get().OnModulesChanged().AddRaw(this, &FPythonScriptPlugin::OnModulesChanged);

		// Initialize the wrapped types
		FPyWrapperTypeRegistry::Get().GenerateWrappedTypes();

		// Initialize the tick handler
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FPythonScriptPlugin_Tick);
			Tick(DeltaTime);
			return true;
		}));
	}

	// Release the GIL taken by Py_Initialize now that initialization has finished, to allow other threads access to Python
	// We have to take this again prior to calling Py_Finalize, and all other code will lock on-demand via FPyScopedGIL
	PyMainThreadState = PyEval_SaveThread();

#if WITH_EDITOR
	// Initialize the Content Browser integration
	if (GIsEditor && !IsRunningCommandlet() && GetDefault<UPythonScriptPluginUserSettings>()->bEnableContentBrowserIntegration)
	{
		ContentBrowserFileData::FFileConfigData PythonFileConfig;
		{
			auto PyItemPassesFilter = [](const FName InFilePath, const FString& InFilename, const FContentBrowserDataFilter& InFilter, const bool bIsFile)
			{
				const FContentBrowserDataPackageFilter* PackageFilter = InFilter.ExtraFilters.FindFilter<FContentBrowserDataPackageFilter>();
				if (PackageFilter && PackageFilter->PathPermissionList && PackageFilter->PathPermissionList->HasFiltering())
				{
					return PackageFilter->PathPermissionList->PassesStartsWithFilter(InFilePath, /*bAllowParentPaths*/!bIsFile);
				}

				return true;
			};

			auto GetPyItemAttribute = [](const FName InFilePath, const FString& InFilename, const bool InIncludeMetaData, const FName InAttributeKey, FContentBrowserItemDataAttributeValue& OutAttributeValue)
			{
				// TODO: Need to way to avoid all this ToString() churn (FPackageNameView?)

				if (InAttributeKey == ContentBrowserItemAttributes::ItemIsDeveloperContent)
				{
					const bool bIsDevelopersFolder = AssetViewUtils::IsDevelopersFolder(InFilePath.ToString());
					OutAttributeValue.SetValue(bIsDevelopersFolder);
					return true;
				}

				if (InAttributeKey == ContentBrowserItemAttributes::ItemIsLocalizedContent)
				{
					const bool bIsLocalizedFolder = FPackageName::IsLocalizedPackage(InFilePath.ToString());
					OutAttributeValue.SetValue(bIsLocalizedFolder);
					return true;
				}

				if (InAttributeKey == ContentBrowserItemAttributes::ItemIsEngineContent)
				{
					const bool bIsEngineFolder = AssetViewUtils::IsEngineFolder(InFilePath.ToString(), /*bIncludePlugins*/true);
					OutAttributeValue.SetValue(bIsEngineFolder);
					return true;
				}

				if (InAttributeKey == ContentBrowserItemAttributes::ItemIsProjectContent)
				{
					const bool bIsProjectFolder = AssetViewUtils::IsProjectFolder(InFilePath.ToString(), /*bIncludePlugins*/true);
					OutAttributeValue.SetValue(bIsProjectFolder);
					return true;
				}

				if (InAttributeKey == ContentBrowserItemAttributes::ItemIsPluginContent)
				{
					const bool bIsPluginFolder = AssetViewUtils::IsPluginFolder(InFilePath.ToString());
					OutAttributeValue.SetValue(bIsPluginFolder);
					return true;
				}

				return false;
			};

			auto PyItemPreview = [this](const FName InFilePath, const FString& InFilename)
			{
				ExecPythonCommand(*InFilename);
				return true;
			};

			ContentBrowserFileData::FDirectoryActions PyDirectoryActions;
			PyDirectoryActions.PassesFilter.BindLambda(PyItemPassesFilter, false);
			PyDirectoryActions.GetAttribute.BindLambda(GetPyItemAttribute);
			PythonFileConfig.SetDirectoryActions(PyDirectoryActions);

			ContentBrowserFileData::FFileActions PyFileActions;
			PyFileActions.TypeExtension = TEXT("py");
			PyFileActions.TypeName = FTopLevelAssetPath(TEXT("/Script/Python.Python")); // Fake path to satisfy FFileActions requirements
			PyFileActions.TypeDisplayName = LOCTEXT("PythonTypeName", "Python");
			PyFileActions.TypeShortDescription = LOCTEXT("PythonTypeShortDescription", "Python Script");
			PyFileActions.TypeFullDescription = LOCTEXT("PythonTypeFullDescription", "A file used to script the editor using Python");
			PyFileActions.DefaultNewFileName = TEXT("new_python_script");
			PyFileActions.TypeColor = FColor(255, 156, 0);
			PyFileActions.PassesFilter.BindLambda(PyItemPassesFilter, true);
			PyFileActions.GetAttribute.BindLambda(GetPyItemAttribute);
			PyFileActions.Preview.BindLambda(PyItemPreview);
			PythonFileConfig.RegisterFileActions(PyFileActions);
		}

		PythonFileDataSource.Reset(NewObject<UContentBrowserFileDataSource>(GetTransientPackage(), "PythonData"));
		PythonFileDataSource->Initialize(PythonFileConfig);

		TArray<FString> RootPaths;
		FPackageName::QueryRootContentPaths(RootPaths);
		for (const FString& RootPath : RootPaths)
		{
			const FString RootFilesystemPath = FPackageName::LongPackageNameToFilename(RootPath);
			PythonFileDataSource->AddFileMount(*(RootPath / TEXT("Python")), RootFilesystemPath / TEXT("Python"));
		}

		{
			FToolMenuOwnerScoped OwnerScoped(this);
			if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.ItemContextMenu.PythonData"))
			{
				Menu->AddDynamicSection(TEXT("DynamicSection_PythonScriptPlugin"), FNewToolMenuDelegate::CreateRaw(this, &FPythonScriptPlugin::PopulatePythonFileContextMenu));
			}
		}
	}
#endif	// WITH_EDITOR
}

void FPythonScriptPlugin::ShutdownPython()
{
	if (!bInitialized)
	{
		return;
	}

	// We need to restore the original GIL prior to calling Py_Finalize
	PyEval_RestoreThread(PyMainThreadState);
	PyMainThreadState = nullptr;

#if WITH_EDITOR
	// Remove the Content Browser integration
	UToolMenus::UnregisterOwner(this);
	PythonFileDataSource.Reset();
#endif	// WITH_EDITOR

	// Notify any external listeners
	OnPythonShutdownDelegate.Broadcast();

	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (ModuleDelayedHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ModuleDelayedHandle);
	}

	FPyWrapperTypeRegistry::Get().OnModuleDirtied().RemoveAll(this);
	FModuleManager::Get().OnModulesChanged().RemoveAll(this);

	FPackageName::OnContentPathMounted().RemoveAll(this);
	FPackageName::OnContentPathDismounted().RemoveAll(this);
	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);

	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet();
		if (AssetRegistry)
		{
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
			AssetRegistry->OnAssetRemoved().RemoveAll(this);
		}
	}

#if WITH_EDITOR
	FEditorSupportDelegates::PrepareToCleanseEditorObject.RemoveAll(this);
#endif	// WITH_EDITOR

	FPyReferenceCollector::Get().PurgeUnrealGeneratedTypes();

#if WITH_EDITOR
	PyEditor::ShutdownModule();
#endif	// WITH_EDITOR
	PyEngine::ShutdownModule();
	PySlate::ShutdownModule();
	PyCore::ShutdownModule();

	PyUnrealModule.Reset();
	PyDefaultGlobalDict.Reset();
	PyDefaultLocalDict.Reset();
	PyConsoleGlobalDict.Reset();
	PyConsoleLocalDict.Reset();

	ShutdownPyMethodWithClosure();

	Py_Finalize();

	bInitialized = false;
	bHasTicked = false;
}

void FPythonScriptPlugin::RequestStubCodeGeneration()
{
	// Ignore requests made before the fist Tick
	if (!bHasTicked)
	{
		return;
	}

	// Delay 2 seconds before generating as this may be triggered by loading several modules at once
	static const float Delay = 2.0f;

	// If there is an existing pending notification, remove it so that it can be reset
	if (ModuleDelayedHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ModuleDelayedHandle);
		ModuleDelayedHandle.Reset();
	}

	// Set new tick
	ModuleDelayedHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[this](float DeltaTime)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FPythonScriptPlugin_ModuleDelayed);

			// Once ticked, the delegate will be removed so reset the handle to indicate that it isn't set.
			ModuleDelayedHandle.Reset();

			// Call the event now that the delay has passed.
			GenerateStubCode();

			// Don't reschedule to run again.
			return false;
		}),
		Delay);
}

void FPythonScriptPlugin::GenerateStubCode()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::GenerateStubCode)

	if (IsDeveloperModeEnabled())
	{
		// Generate stub code if developer mode enabled
		FPyWrapperTypeRegistry::Get().GenerateStubCodeForWrappedTypes();
	}
}

void FPythonScriptPlugin::Tick(const float InDeltaTime)
{
	// If this is our first Tick, handle any post-init logic that should happen once the engine is fully initialized
	if (!bHasTicked)
	{
		bHasTicked = true;

		// Run start-up scripts now
		TArray<FString> PySysPaths;
		{
			FPyScopedGIL GIL;
			PySysPaths = PyUtil::GetSystemPaths();
		}
		for (const FString& PySysPath : PySysPaths)
		{
			const FString PotentialFilePath = PySysPath / TEXT("init_unreal.py");
			if (FPaths::FileExists(PotentialFilePath))
			{
				// Execute these files in the "public" scope, as if their contents had been run directly in the console
				// This allows them to be used to set-up an editor environment for the console
				FPythonCommandEx InitUnrealPythonCommand;
				InitUnrealPythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;
				RunFile(*PotentialFilePath, *InitUnrealPythonCommand.Command, InitUnrealPythonCommand);
			}
		}
		for (const FString& StartupScript : GetDefault<UPythonScriptPluginSettings>()->StartupScripts)
		{
			ExecPythonCommand(*StartupScript);
		}

		// Notify any external listeners
		OnPythonInitializedDelegate.Broadcast();

#if WITH_EDITOR
		// Activate the Content Browser integration (now that editor subsystems are available)
		if (PythonFileDataSource)
		{
			UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
			ContentBrowserData->ActivateDataSource("PythonData");
		}

		// Register to generate stub code after a short delay
		RequestStubCodeGeneration();
#endif	// WITH_EDITOR
	}

	RemoteExecution->Tick(InDeltaTime);

	FPyWrapperTypeReinstancer::Get().ProcessPending();
}

void FPythonScriptPlugin::SyncRemoteExecutionToSettings()
{
	RemoteExecution->SyncToSettings();
}

void FPythonScriptPlugin::ImportUnrealModule(const TCHAR* InModuleName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FPythonScriptPlugin::ImportUnrealModule %s"), InModuleName));

	const FString PythonModuleName = FString::Printf(TEXT("unreal_%s"), InModuleName);
	const FString NativeModuleName = FString::Printf(TEXT("_unreal_%s"), InModuleName);

	FPyScopedGIL GIL;

	const TCHAR* ModuleNameToImport = nullptr;
	PyObject* ModuleToReload = nullptr;
	if (PyUtil::IsModuleAvailableForImport(*PythonModuleName, &PyUtil::GetOnDiskUnrealModulesCache()))
	{
		// Python modules that are already loaded should be reloaded if we're requested to import them again
		if (!PyUtil::IsModuleImported(*PythonModuleName, &ModuleToReload))
		{
			ModuleNameToImport = *PythonModuleName;
		}
	}
	else if (PyUtil::IsModuleAvailableForImport(*NativeModuleName, &PyUtil::GetOnDiskUnrealModulesCache()))
	{
		ModuleNameToImport = *NativeModuleName;
	}

	FPyObjectPtr PyModule;
	if (ModuleToReload)
	{
		PyModule = FPyObjectPtr::StealReference(PyImport_ReloadModule(ModuleToReload));
	}
	else if (ModuleNameToImport)
	{
		PyModule = FPyObjectPtr::StealReference(PyImport_ImportModule(TCHAR_TO_UTF8(ModuleNameToImport)));
	}

	if (PyModule)
	{
		check(PyUnrealModule);
		PyObject* PyUnrealModuleDict = PyModule_GetDict(PyUnrealModule);

		// Hoist every public symbol from this module into the top-level "unreal" module
		{
			PyObject* PyModuleDict = PyModule_GetDict(PyModule);

			PyObject* PyObjKey = nullptr;
			PyObject* PyObjValue = nullptr;
			Py_ssize_t ModuleDictIndex = 0;
			while (PyDict_Next(PyModuleDict, &ModuleDictIndex, &PyObjKey, &PyObjValue))
			{
				if (PyObjKey)
				{
					const FString Key = PyUtil::PyObjectToUEString(PyObjKey);
					if (Key.Len() > 0 && Key[0] != TEXT('_'))
					{
						PyDict_SetItem(PyUnrealModuleDict, PyObjKey, PyObjValue);
					}
				}
			}
		}
	}
	else
	{
		PyUtil::LogPythonError(nullptr, /*bInteractive*/true);
	}
}

PyObject* FPythonScriptPlugin::EvalString(const TCHAR* InStr, const TCHAR* InContext, const int InMode)
{
	return EvalString(InStr, InContext, InMode, PyConsoleGlobalDict, PyConsoleLocalDict);
}

PyObject* FPythonScriptPlugin::EvalString(const TCHAR* InStr, const TCHAR* InContext, const int InMode, PyObject* InGlobalDict, PyObject* InLocalDict)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::EvalString)

	PyCompilerFlags *PyCompFlags = nullptr;

	PyArena* PyArena = PyArena_New();
	if (!PyArena)
	{
		return nullptr;
	}

	_mod* PyModule = PyParser_ASTFromString(TCHAR_TO_UTF8(InStr), TCHAR_TO_UTF8(InContext), InMode, PyCompFlags, PyArena);
	if (!PyModule)
	{
		PyArena_Free(PyArena);
		return nullptr;
	}

	typedef TPyPtr<PyCodeObject> PyCodeObjectPtr;
	PyCodeObjectPtr PyCodeObj = PyCodeObjectPtr::StealReference(PyAST_Compile(PyModule, TCHAR_TO_UTF8(InContext), PyCompFlags, PyArena));
	if (!PyCodeObj)
	{
		return nullptr;
	}

	return PyEval_EvalCode((PyUtil::FPyCodeObjectType*)PyCodeObj.Get(), InGlobalDict, InLocalDict);
}

bool FPythonScriptPlugin::RunString(FPythonCommandEx& InOutPythonCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::RunString)

	// Execute Python code within this block
	{
		FPyScopedGIL GIL;
		TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, GIsRunningUnattendedScript || EnumHasAnyFlags(InOutPythonCommand.Flags, EPythonCommandFlags::Unattended));

		int PyExecMode = 0;
		switch (InOutPythonCommand.ExecutionMode)
		{
		case EPythonCommandExecutionMode::ExecuteFile:
			PyExecMode = Py_file_input;
			break;
		case EPythonCommandExecutionMode::ExecuteStatement:
			PyExecMode = Py_single_input;
			break;
		case EPythonCommandExecutionMode::EvaluateStatement:
			PyExecMode = Py_eval_input;
			break;
		default:
			checkf(false, TEXT("Invalid EPythonCommandExecutionMode!"));
			break;
		}

		FDelegateHandle LogCaptureHandle = PyCore::GetPythonLogCapture().AddLambda([&InOutPythonCommand](EPythonLogOutputType InLogType, const TCHAR* InLogString) { InOutPythonCommand.LogOutput.Add(FPythonLogOutputEntry{ InLogType, InLogString }); });
		FPyObjectPtr PyResult = FPyObjectPtr::StealReference(EvalString(*InOutPythonCommand.Command, TEXT("<string>"), PyExecMode));
		PyCore::GetPythonLogCapture().Remove(LogCaptureHandle);
		
		if (PyResult)
		{
			InOutPythonCommand.CommandResult = PyUtil::PyObjectToUEStringRepr(PyResult);
		}
		else if (PyUtil::LogPythonError(&InOutPythonCommand.CommandResult))
		{
			return false;
		}
	}

	FPyWrapperTypeReinstancer::Get().ProcessPending();
	return true;
}

bool FPythonScriptPlugin::RunFile(const TCHAR* InFile, const TCHAR* InArgs, FPythonCommandEx& InOutPythonCommand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::RunFile)

	auto ResolveFilePath = [InFile]() -> FString
	{
		// Favor the CWD
		if (FPaths::FileExists(InFile))
		{
			return FPaths::ConvertRelativePathToFull(InFile);
		}

		// Execute Python code within this block
		{
			FPyScopedGIL GIL;

			// Then test against each system path in order (as Python would)
			const TArray<FString> PySysPaths = PyUtil::GetSystemPaths();
			for (const FString& PySysPath : PySysPaths)
			{
				const FString PotentialFilePath = PySysPath / InFile;
				if (FPaths::FileExists(PotentialFilePath))
				{
					return PotentialFilePath;
				}
			}
		}

		// Didn't find a match... we know this file doesn't exist, but we'll use this path in the error reporting
		return FPaths::ConvertRelativePathToFull(InFile);
	};

	const FString ResolvedFilePath = ResolveFilePath();

	FString FileStr;
	bool bLoaded = FFileHelper::LoadFileToString(FileStr, *ResolvedFilePath);
#if WITH_EDITOR
	if (CmdMenu)
	{
		CmdMenu->OnRunFile(ResolvedFilePath, bLoaded);
	}
#endif // WITH_EDITOR

	if (!bLoaded)
	{
		InOutPythonCommand.CommandResult = FString::Printf(TEXT("Could not load Python file '%s' (resolved from '%s')"), *ResolvedFilePath, InFile);
		UE_LOG(LogPython, Error, TEXT("%s"), *InOutPythonCommand.CommandResult);
		return false;
	}

	// Execute Python code within this block
	double ElapsedSeconds = 0.0;
	{
		FPyScopedGIL GIL;
		TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, GIsRunningUnattendedScript || EnumHasAnyFlags(InOutPythonCommand.Flags, EPythonCommandFlags::Unattended));

		FPyObjectPtr PyFileGlobalDict = PyConsoleGlobalDict;
		FPyObjectPtr PyFileLocalDict = PyConsoleLocalDict;
		if (InOutPythonCommand.FileExecutionScope == EPythonFileExecutionScope::Private)
		{
			PyFileGlobalDict = FPyObjectPtr::StealReference(PyDict_Copy(PyDefaultGlobalDict));
			PyFileLocalDict = PyFileGlobalDict;
		}
		{
			FPyObjectPtr PyResolvedFilePath;
			if (PyConversion::Pythonize(ResolvedFilePath, PyResolvedFilePath.Get(), PyConversion::ESetErrorState::No))
			{
				PyDict_SetItemString(PyFileGlobalDict, "__file__", PyResolvedFilePath);
			}
		}

		FPyObjectPtr PyResult;
		{
			FScopedDurationTimer Timer(ElapsedSeconds);
			FPythonScopedArgv ScopedArgv(InArgs);

			FDelegateHandle LogCaptureHandle = PyCore::GetPythonLogCapture().AddLambda([&InOutPythonCommand](EPythonLogOutputType InLogType, const TCHAR* InLogString) { InOutPythonCommand.LogOutput.Add(FPythonLogOutputEntry{ InLogType, InLogString }); });
			PyResult = FPyObjectPtr::StealReference(EvalString(*FileStr, *ResolvedFilePath, Py_file_input, PyFileGlobalDict, PyFileLocalDict)); // We can't just use PyRun_File here as Python isn't always built against the same version of the CRT as UE, so we get a crash at the CRT layer
			PyCore::GetPythonLogCapture().Remove(LogCaptureHandle);
		}

		PyDict_DelItemString(PyFileGlobalDict, "__file__");

		if (PyResult)
		{
			InOutPythonCommand.CommandResult = PyUtil::PyObjectToUEStringRepr(PyResult);
		}
		else if (PyUtil::LogPythonError(&InOutPythonCommand.CommandResult))
		{
			return false;
		}
	}

	FPyWrapperTypeReinstancer::Get().ProcessPending();

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Duration"), ElapsedSeconds));
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("PythonScriptPlugin"), EventAttributes);
	}

	return true;
}

void FPythonScriptPlugin::OnModuleDirtied(FName InModuleName)
{
	ImportUnrealModule(*InModuleName.ToString());
}

void FPythonScriptPlugin::OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason)
{
	LLM_SCOPE_BYNAME(TEXT("PythonScriptPlugin"));
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnModulesChanged)

	switch (InModuleChangeReason)
	{
	case EModuleChangeReason::ModuleLoaded:
		FPyWrapperTypeRegistry::Get().GenerateWrappedTypesForModule(InModuleName);
#if WITH_EDITOR
		// Register to generate stub code after a short delay
		RequestStubCodeGeneration();
#endif	// WITH_EDITOR
		break;

	case EModuleChangeReason::ModuleUnloaded:
		FPyWrapperTypeRegistry::Get().OrphanWrappedTypesForModule(InModuleName);
#if WITH_EDITOR
		// Register to generate stub code after a short delay
		RequestStubCodeGeneration();
#endif	// WITH_EDITOR
		break;

	default:
		break;
	}
}

void FPythonScriptPlugin::OnContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnContentPathMounted)

	{
		FPyScopedGIL GIL;
		RegisterModulePaths(InFilesystemPath);
	}

#if WITH_EDITOR
	if (PythonFileDataSource)
	{
		PythonFileDataSource->AddFileMount(*(InAssetPath / TEXT("Python")), InFilesystemPath / TEXT("Python"));
	}
#endif	// WITH_EDITOR
}

void FPythonScriptPlugin::OnContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnContentPathDismounted)

	{
		FPyScopedGIL GIL;
		UnregisterModulePaths(InFilesystemPath);
	}

#if WITH_EDITOR
	if (PythonFileDataSource)
	{
		PythonFileDataSource->RemoveFileMount(*(InAssetPath / TEXT("Python")));
	}
#endif	// WITH_EDITOR
}

void FPythonScriptPlugin::RegisterModulePaths(const FString& InFilesystemPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::RegisterModulePaths)

	const FString PythonContentPath = FPaths::ConvertRelativePathToFull(InFilesystemPath / TEXT("Python"));
	if (IFileManager::Get().DirectoryExists(*PythonContentPath))
	{
		PyUtil::AddSystemPath(PythonContentPath);

		const FString PythonContentPlatformSitePackagesPath = PythonContentPath / TEXT("Lib") / FPlatformMisc::GetUBTPlatform() / TEXT("site-packages");
		const FString PythonContentGeneralSitePackagesPath = PythonContentPath / TEXT("Lib") / TEXT("site-packages");
		PyUtil::AddSitePackagesPath(PythonContentPlatformSitePackagesPath);
		PyUtil::AddSitePackagesPath(PythonContentGeneralSitePackagesPath);

		PyUtil::GetOnDiskUnrealModulesCache().AddModules(*PythonContentPath);
	}
}

void FPythonScriptPlugin::UnregisterModulePaths(const FString& InFilesystemPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::UnregisterModulePaths)

	const FString PythonContentPath = FPaths::ConvertRelativePathToFull(InFilesystemPath / TEXT("Python"));
	PyUtil::RemoveSystemPath(PythonContentPath);

	const FString PythonContentPlatformSitePackagesPath = PythonContentPath / TEXT("Lib") / FPlatformMisc::GetUBTPlatform() / TEXT("site-packages");
	const FString PythonContentGeneralSitePackagesPath = PythonContentPath / TEXT("Lib") / TEXT("site-packages");
	PyUtil::RemoveSystemPath(PythonContentPlatformSitePackagesPath);
	PyUtil::RemoveSystemPath(PythonContentGeneralSitePackagesPath);

	PyUtil::GetOnDiskUnrealModulesCache().RemoveModules(*PythonContentPath);
}

bool FPythonScriptPlugin::IsDeveloperModeEnabled()
{
	return GetDefault<UPythonScriptPluginSettings>()->bDeveloperMode || GetDefault<UPythonScriptPluginUserSettings>()->bDeveloperMode;
}

ETypeHintingMode FPythonScriptPlugin::GetTypeHintingMode()
{
	return GetDefault<UPythonScriptPluginUserSettings>()->TypeHintingMode;
}

void FPythonScriptPlugin::OnAssetRenamed(const FAssetData& Data, const FString& OldName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnAssetRenamed)

	FPyWrapperTypeRegistry& PyWrapperTypeRegistry = FPyWrapperTypeRegistry::Get();
	const FName OldPackageName = *FPackageName::ObjectPathToPackageName(OldName);
	
	// If this asset has an associated Python type, then we need to rename it
	if (PyWrapperTypeRegistry.HasWrappedTypeForObjectName(OldPackageName))
	{
		if (const UObject* AssetPtr = PyGenUtil::GetAssetTypeRegistryType(Data.GetAsset()))
		{
			PyWrapperTypeRegistry.UpdateGenerateWrappedTypeForRename(OldPackageName, AssetPtr);
			OnAssetUpdated(AssetPtr);
		}
		else
		{
			PyWrapperTypeRegistry.RemoveGenerateWrappedTypeForDelete(OldPackageName);
		}
	}
}

void FPythonScriptPlugin::OnAssetRemoved(const FAssetData& Data)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnAssetRemoved)

	FPyWrapperTypeRegistry& PyWrapperTypeRegistry = FPyWrapperTypeRegistry::Get();
	
	// If this asset has an associated Python type, then we need to remove it
	if (PyWrapperTypeRegistry.HasWrappedTypeForObjectName(Data.PackageName))
	{
		PyWrapperTypeRegistry.RemoveGenerateWrappedTypeForDelete(Data.PackageName);
	}
}

void FPythonScriptPlugin::OnAssetReload(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnAssetReload)

	if (InPackageReloadPhase == EPackageReloadPhase::PostPackageFixup)
	{
		// Get the primary asset in this package
		// Use the new package as it has the correct name
		const UPackage* NewPackage = InPackageReloadedEvent->GetNewPackage();
		const UObject* NewAsset = StaticFindObject(UObject::StaticClass(), (UPackage*)NewPackage, *FPackageName::GetLongPackageAssetName(NewPackage->GetName()));
		OnAssetUpdated(NewAsset);
	}
}

void FPythonScriptPlugin::OnAssetUpdated(const UObject* InObj)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPythonScriptPlugin::OnAssetUpdated)

	if (const UObject* AssetPtr = PyGenUtil::GetAssetTypeRegistryType(InObj))
	{
		// If this asset has an associated Python type, then we need to re-generate it
		FPyWrapperTypeRegistry& PyWrapperTypeRegistry = FPyWrapperTypeRegistry::Get();
		if (PyWrapperTypeRegistry.HasWrappedTypeForObject(AssetPtr))
		{
			FPyWrapperTypeRegistry::FGeneratedWrappedTypeReferences GeneratedWrappedTypeReferences;
			TSet<FName> DirtyModules;

			PyWrapperTypeRegistry.GenerateWrappedTypeForObject(AssetPtr, GeneratedWrappedTypeReferences, DirtyModules, EPyTypeGenerationFlags::IncludeBlueprintGeneratedTypes | EPyTypeGenerationFlags::OverwriteExisting);

			PyWrapperTypeRegistry.GenerateWrappedTypesForReferences(GeneratedWrappedTypeReferences, DirtyModules);
			PyWrapperTypeRegistry.NotifyModulesDirtied(DirtyModules);
		}
	}
}

#if WITH_EDITOR

void FPythonScriptPlugin::OnPrepareToCleanseEditorObject(UObject* InObject)
{
	FPyReferenceCollector::Get().PurgeUnrealObjectReferences(InObject, true);
}

void FPythonScriptPlugin::PopulatePythonFileContextMenu(UToolMenu* InMenu)
{
	const UContentBrowserDataMenuContext_FileMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_FileMenu>();
	checkf(ContextObject, TEXT("Required context UContentBrowserDataMenuContext_FileMenu was missing!"));

	if (!PythonFileDataSource)
	{
		return;
	}

	// Extract the internal file paths that belong to this data source from the full list of selected paths given in the context
	TArray<TSharedRef<const FContentBrowserFileItemDataPayload>> SelectedPythonFiles;
	for (const FContentBrowserItem& SelectedItem : ContextObject->SelectedItems)
	{
		if (const FContentBrowserItemData* ItemDataPtr = SelectedItem.GetPrimaryInternalItem())
		{
			if (TSharedPtr<const FContentBrowserFileItemDataPayload> FilePayload = ContentBrowserFileData::GetFileItemPayload(PythonFileDataSource.Get(), *ItemDataPtr))
			{
				SelectedPythonFiles.Add(FilePayload.ToSharedRef());
			}
		}
	}

	// Only add the file items if we have a file path selected
	if (SelectedPythonFiles.Num() > 0)
	{
		// Run
		{
			FToolMenuSection& Section = InMenu->AddSection("PythonScript", LOCTEXT("PythonScriptMenuHeading", "Python Script"));
			Section.InsertPosition.Position = EToolMenuInsertType::First;

			const FExecuteAction ExecuteRunAction = FExecuteAction::CreateLambda([this, SelectedPythonFiles]()
			{
				for (const TSharedRef<const FContentBrowserFileItemDataPayload>& SelectedPythonFile : SelectedPythonFiles)
				{
					ExecPythonCommand(*SelectedPythonFile->GetFilename());
				}
			});

			Section.AddMenuEntry(
				"RunPythonScript",
				LOCTEXT("RunPythonScript", "Run..."),
				LOCTEXT("RunPythonScriptToolTip", "Run this script."),
				FSlateIcon(),
				FUIAction(ExecuteRunAction)
			);
		}
	}
}

#endif	// WITH_EDITOR

#endif	// WITH_PYTHON

IMPLEMENT_MODULE(FPythonScriptPlugin, PythonScriptPlugin)

#undef LOCTEXT_NAMESPACE
