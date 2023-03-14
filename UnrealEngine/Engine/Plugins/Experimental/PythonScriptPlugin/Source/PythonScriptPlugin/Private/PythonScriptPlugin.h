// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IncludePython.h"
#include "IPythonScriptPlugin.h"
#include "PyUtil.h"
#include "PyPtr.h"
#include "Containers/Ticker.h"
#include "Misc/CoreMisc.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Commands/InputChord.h"
#include "Kismet2/EnumEditorUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "PythonScriptPluginSettings.h"

class FPythonScriptPlugin;
class FPythonScriptRemoteExecution;
class FPackageReloadedEvent;
class UContentBrowserFileDataSource;
class UToolMenu;

struct FAssetData;

enum class EPackageReloadPhase : uint8;

#if WITH_PYTHON

/**
 * Executor for "Python" commands
 */
class FPythonCommandExecutor : public IConsoleCommandExecutor
{
public:
	FPythonCommandExecutor(IPythonScriptPlugin* InPythonScriptPlugin);

	static FName StaticName();
	virtual FName GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDescription() const override;
	virtual FText GetHintText() const override;
	virtual void GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out) override;
	virtual void GetExecHistory(TArray<FString>& Out) override;
	virtual bool Exec(const TCHAR* Input) override;
	virtual bool AllowHotKeyClose() const override;
	virtual bool AllowMultiLine() const override;
	virtual FInputChord GetHotKey() const override;
	virtual FInputChord GetIterateExecutorHotKey() const override;

private:
	IPythonScriptPlugin* PythonScriptPlugin;
};

/**
 * Executor for "Python (REPL)" commands
 */
class FPythonREPLCommandExecutor : public IConsoleCommandExecutor
{
public:
	FPythonREPLCommandExecutor(IPythonScriptPlugin* InPythonScriptPlugin);

	static FName StaticName();
	virtual FName GetName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDescription() const override;
	virtual FText GetHintText() const override;
	virtual void GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out) override;
	virtual void GetExecHistory(TArray<FString>& Out) override;
	virtual bool Exec(const TCHAR* Input) override;
	virtual bool AllowHotKeyClose() const override;
	virtual bool AllowMultiLine() const override;
	virtual FInputChord GetHotKey() const override;
	virtual FInputChord GetIterateExecutorHotKey() const override;

private:
	IPythonScriptPlugin* PythonScriptPlugin;
};

/**
 *
 */
struct IPythonCommandMenu
{
	virtual ~IPythonCommandMenu() {}

	virtual void OnStartupMenu() = 0;
	virtual void OnShutdownMenu() = 0;

	virtual void OnRunFile(const FString& InFile, bool bAdd) = 0;
};
#endif	// WITH_PYTHON

class FPythonScriptPlugin 
	: public IPythonScriptPlugin
	, public FSelfRegisteringExec
	, public FEnumEditorUtils::INotifyOnEnumChanged
{
public:
	FPythonScriptPlugin();

	/** Get this module */
	static FPythonScriptPlugin* Get()
	{
		return static_cast<FPythonScriptPlugin*>(IPythonScriptPlugin::Get());
	}

	//~ IPythonScriptPlugin interface
	virtual bool IsPythonAvailable() const override;
	virtual bool ExecPythonCommand(const TCHAR* InPythonCommand) override;
	virtual bool ExecPythonCommandEx(FPythonCommandEx& InOutPythonCommand) override;
	virtual FSimpleMulticastDelegate& OnPythonInitialized() override;
	virtual FSimpleMulticastDelegate& OnPythonShutdown() override;

	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//~ FSelfRegisteringExec interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	//~ FEnumEditorUtils::INotifyOnEnumChanged interface
	virtual void PreChange(const UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info) override;
	virtual void PostChange(const UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info) override;

#if WITH_PYTHON

#if WITH_EDITOR	
	void OnPostEngineInit();
#endif

	/** Sync the remote execution environment to the current settings, starting or stopping it as required */
	void SyncRemoteExecutionToSettings();

	/** 
	 * Import the given module into the "unreal" package.
	 * This function will take the given name and attempt to import either "unreal_{name}" or "_unreal_{name}" into the "unreal" package as "unreal.{name}".
	 */
	void ImportUnrealModule(const TCHAR* InModuleName);

	/** Evaluate/Execute a Python string, and return the result */
	PyObject* EvalString(const TCHAR* InStr, const TCHAR* InContext, const int InMode);
	PyObject* EvalString(const TCHAR* InStr, const TCHAR* InContext, const int InMode, PyObject* InGlobalDict, PyObject* InLocalDict);

	/** Run literal Python script */
	bool RunString(FPythonCommandEx& InOutPythonCommand);

	/** Run a Python file */
	bool RunFile(const TCHAR* InFile, const TCHAR* InArgs, FPythonCommandEx& InOutPythonCommand);

	PyObject* GetDefaultGlobalDict() { return PyDefaultGlobalDict.Get(); }
	PyObject* GetDefaultLocalDict()  { return PyDefaultLocalDict.Get();  }
	PyObject* GetConsoleGlobalDict() { return PyConsoleGlobalDict.Get(); }
	PyObject* GetConsoleLocalDict()  { return PyConsoleLocalDict.Get();  }
#endif	// WITH_PYTHON

private:
#if WITH_PYTHON
	void InitializePython();

	void ShutdownPython();

	void RequestStubCodeGeneration();

	void GenerateStubCode();

	void Tick(const float InDeltaTime);

	void OnModuleDirtied(FName InModuleName);

	void OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason);

	void OnContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath);

	void OnContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath);

	static void RegisterModulePaths(const FString& InFilesystemPath);

	static void UnregisterModulePaths(const FString& InFilesystemPath);

	static bool IsDeveloperModeEnabled();

	static ETypeHintingMode GetTypeHintingMode();

	void OnAssetRenamed(const FAssetData& Data, const FString& OldName);

	void OnAssetRemoved(const FAssetData& Data);

	void OnAssetReload(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);

	void OnAssetUpdated(const UObject* InObj);

#if WITH_EDITOR
	void OnPrepareToCleanseEditorObject(UObject* InObject);

	void PopulatePythonFileContextMenu(UToolMenu* InMenu);

	TStrongObjectPtr<UContentBrowserFileDataSource> PythonFileDataSource;
#endif	// WITH_EDITOR

	TUniquePtr<FPythonScriptRemoteExecution> RemoteExecution;
	FPythonCommandExecutor CmdExec;
	FPythonREPLCommandExecutor CmdREPLExec;
	IPythonCommandMenu* CmdMenu;
	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle ModuleDelayedHandle;

	PyUtil::FPyApiBuffer PyProgramName;
	PyUtil::FPyApiBuffer PyHomePath;
	FPyObjectPtr PyDefaultGlobalDict;
	FPyObjectPtr PyDefaultLocalDict;
	FPyObjectPtr PyConsoleGlobalDict;
	FPyObjectPtr PyConsoleLocalDict;
	FPyObjectPtr PyUnrealModule;
	PyThreadState* PyMainThreadState = nullptr;
	bool bInitialized;
	bool bHasTicked;
#endif	// WITH_PYTHON

	FSimpleMulticastDelegate OnPythonInitializedDelegate;
	FSimpleMulticastDelegate OnPythonShutdownDelegate;
};
