// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "ProjectFiles.h"
#include "Templates/UniquePtr.h"

namespace UE::Virtualization
{

class FCommand;

/** The mode of the application to run */
enum class EMode :uint32
{
	/** Error condition */
	Unknown = 0,

	// Legacy (should be phased out)

	/** Virtualize and submit a given changelist */
	Changelist,
	/** Virtualize a list of packages provided by text file */
	PackageList,

	/// New Style Commands

	/** Virtualize one or more packages */
	Virtualize,

	/** Rehydrate one or more packages */
	Rehydrate
};

/** The result of FUnrealVirtualizationToolApp initialization */
enum class EInitResult
{
	/** Initialization succeeded and the application should run. */
	Success = 0,
	/** No error was encountered but the application should early out and not run. */
	EarlyOut,
	/** Initialization failed with an error, we should not run and should display an error to the user */
	Error
};

class FUnrealVirtualizationToolApp
{
public:
	FUnrealVirtualizationToolApp();
	~FUnrealVirtualizationToolApp();

	EInitResult Initialize();
	bool Run();

private:

	void PrintCmdLineHelp() const;

	bool TryLoadModules();
	bool TryInitEnginePlugins();
	
	EInitResult TryParseCmdLine();

	bool TrySortFilesByProject(const TArray<FString>& Packages);

	bool TryFindProject(const FString& PackagePath, FString& ProjectFilePath, FString& PluginFilePath) const;

	FProject& FindOrAddProject(FString&& ProjectFilePath);

private:

	/** Override used to suppress log messages */
	TUniquePtr<FFeedbackContext> OutputDeviceOverride;

	/** Data structure holding the files in the changelist sorted by project and then by plugin*/
	TArray<FProject> Projects;

	/** The mode for the application to run */
	EMode Mode = EMode::Unknown;

	/** The command being processed */
	TUniquePtr<FCommand> CurrentCommand;
};

} //namespace UE::Virtualization
