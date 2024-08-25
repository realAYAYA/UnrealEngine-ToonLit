// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class IPlugin;

namespace UE::Cook
{

struct FCookSandboxConvertCookedPathToPackageNameContext;

/**
 * A wrapper around FSandboxPlatformFile that provides a similar interface but also handles cook-specific
 * functionality like REMAPPED_PLUGINS
 */
class FCookSandbox
{
public:
	FCookSandbox(FStringView OutputDirectory, TArray<TSharedRef<IPlugin>>& InPluginsToRemap);

	// ISandboxPlatformFile
	const FString& GetSandboxDirectory() const;
	const FString& GetGameSandboxDirectoryName() const;
	FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename) const;
	FString ConvertFromSandboxPath(const TCHAR* Filename) const;

	// Cooker API functions that handle the Platform mapping on top of ISandboxPlatformFile
	FString GetSandboxDirectory(const FString& PlatformName) const;
	FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename, const FString& PlatformName) const;
	FString ConvertFromSandboxPathInPlatformRoot(const TCHAR* Filename, FStringView PlatformSandboxRootDir) const;

	// Cooker API functions on top of ISandboxPlatformFile
	FSandboxPlatformFile& GetSandboxPlatformFile();
	FString ConvertToFullSandboxPath(const FString& FileName, bool bForWrite) const;
	FString ConvertToFullPlatformSandboxPath(const FString& FileName, bool bForWrite,
		const FString& PlatformName) const;
	FString ConvertToFullSandboxPathInPlatformRoot(const FString& FileName, bool bForWrite,
		FStringView PlatformSandboxRootdir) const;

	bool TryConvertUncookedFilenameToCookedRemappedPluginFilename(FStringView FileName, FString& OutCookedFileName,
		FStringView PlatformSandboxRootdir = FStringView()) const;

	void FillContext(FCookSandboxConvertCookedPathToPackageNameContext& Context) const;
	FString& ConvertCookedPathToUncookedPath(FStringView CookedPath,
		FCookSandboxConvertCookedPathToPackageNameContext& Context) const;
	FName ConvertCookedPathToPackageName(FStringView CookedPath,
		FCookSandboxConvertCookedPathToPackageNameContext& Context) const;
	FString ConvertPackageNameToCookedPath(FStringView PackageName,
		FCookSandboxConvertCookedPathToPackageNameContext& Context) const;


private:
	struct FPluginData
	{
		TSharedPtr<IPlugin> Plugin;
		FString NormalizedRootDir;
	};

	TUniquePtr<FSandboxPlatformFile> SandboxFile;
	TArray<FPluginData> PluginsToRemap;
};

struct FCookSandboxConvertCookedPathToPackageNameContext
{
	FStringView SandboxRootDir;
	FStringView UncookedRelativeRootDir;
	FStringView SandboxProjectDir;
	FStringView UncookedRelativeProjectDir;
	FString ScratchSandboxProjectDir;
	FString ScratchUncookedRelativeProjectDir;
	FString ScratchFileName;
	FString ScratchPackageName;
};


} // namespace UE::Cook