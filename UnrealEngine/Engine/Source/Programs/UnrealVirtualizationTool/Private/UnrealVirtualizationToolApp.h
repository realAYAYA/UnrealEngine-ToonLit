// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "ISourceControlChangelist.h" // TODO
#include "ProjectFiles.h"
#include "Templates/UniquePtr.h"

class ISourceControlProvider;

namespace UE::Virtualization
{

class FCommand;

/** The mode of the application to run */
enum class EMode :uint32
{
	/** Error condition */
	Unknown = 0,

	/// Virtualization

	/** Virtualize and submit a given changelist */
	Changelist,
	/** Virtualize a list of packages provided by text file */
	PackageList,

	/// Rehydration

	/** Rehydrate one or more packages */
	Rehydrate,
};

// Note that the enum doesn't give us huge value right now, it has been provided
// in case that we expand with additional functionality in the future.

/** A bitfield of the various operations that the virtualization process supports */
enum class EProcessOptions : uint32
{
	/** No options */
	None = 0,
	/** Virtualize the packages in the provided changelist */
	Virtualize = 1 << 0,
	/** Submit the changelist */
	Submit = 1 << 1
};
ENUM_CLASS_FLAGS(EProcessOptions);

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

	bool TrySubmitChangelist(const TArray<FString>& DescriptionTags);

	bool TryLoadModules();
	bool TryInitEnginePlugins();
	bool TryConnectToSourceControl();

	EInitResult TryParseCmdLine();
	EInitResult TryParseChangelistCmdLine(const TCHAR* CmdLine);
	EInitResult	TryParsePackageListCmdLine(const TCHAR* CmdLine);

	bool TryParseChangelist(TArray<FString>& OutPackages);
	bool TryParsePackageList(TArray<FString>& OutPackages);

	bool TrySortFilesByProject(const TArray<FString>& Packages);

	bool TryFindProject(const FString& PackagePath, FString& ProjectFilePath, FString& PluginFilePath) const;

	FProject& FindOrAddProject(FString&& ProjectFilePath);

private:

	/** Override used to suppress log messages */
	TUniquePtr<FFeedbackContext> OutputDeviceOverride;

	/** The source control provider that the tool uses */
	TUniquePtr<ISourceControlProvider> SCCProvider;

	/** Pointer to the changelist that should be submitted */
	FSourceControlChangelistPtr ChangelistToSubmit;

	/** Data structure holding the files in the changelist sorted by project and then by plugin*/
	TArray<FProject> Projects;

	/** Name of the client spec (workspace) passed in on the command line*/
	FString ClientSpecName;

	/** The mode for the application to run */
	EMode Mode = EMode::Unknown;

	/** Bitfield control the various options that should be run */
	EProcessOptions ProcessOptions = EProcessOptions::Virtualize;

	/** The number of the changelist being submitted, used with EMode::Changelist  */
	FString ChangelistNumber;

	/** The path to the list of packages to virtualized, used with EMode::PackageList */
	FString PackageListPath;

	TUniquePtr<FCommand> CurrentCommand;
};

} //namespace UE::Virtualization
