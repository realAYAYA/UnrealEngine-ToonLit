// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Interfaces/IPluginManager.h"
#include "Templates/SharedPointer.h"

#if WITH_PYTHON

class FJsonObject;
class FFeedbackContext;

class IProgressParser;

class FPipInstall
{
public:
	static FPipInstall& Get();

	bool IsEnabled() const;
	bool IsCmdLineDisabled() const;

	void CheckRemoveOrphanedPackages(const FString& SitePackagesPath);
	void CheckInvalidPipEnv() const;

	FString WritePluginsListing(TArray<TSharedRef<IPlugin>>& OutPythonPlugins) const;
	FString WritePluginDependencies(const TArray<TSharedRef<IPlugin>>& PythonPlugins, TArray<FString>& OutRequirements, TArray<FString>& OutExtraUrls) const;

	void SetupPipEnv(FFeedbackContext* Context, bool bForceRebuild = false) const;
	void RemoveParsedDependencyFiles() const;
	FString ParsePluginDependencies(const FString& MergedInRequirementsFile, FFeedbackContext* Context) const;
	bool RunPipInstall(FFeedbackContext* Context) const;

	int NumPackagesToInstall() const;

	FString GetPipInstallPath() const;
	FString GetPipSitePackagesPath() const;

private:
	FPipInstall();

	void WriteSitePackagePthFile() const;
	void SetupPipInstallUtils(FFeedbackContext* Context) const;
	bool CheckPipInstallUtils(FFeedbackContext* Context) const;

	FString SetupPipInstallCmd(const FString& ParsedReqsFile, const TArray<FString>& ExtraUrls) const;

	static int32 RunPythonCmd(const FText& Description, const FString& PythonInterp, const FString& Cmd, FFeedbackContext* Context, TSharedPtr<IProgressParser> CmdParser = nullptr);
	static bool RunLoggedSubprocess(int32* OutExitCode, const FText& Description, const FString& URL, const FString& Params, FFeedbackContext* Context, TSharedPtr<IProgressParser> CmdParser);

	FString ParseVenvVersion() const;

	static FString GetPythonScriptPluginPath();
	static FString GetVenvInterpreter(const FString& InstallPath);
	static bool CheckCompatiblePlatform(const TSharedPtr<FJsonObject>& JsonObject, const FString& PlatformName);

	static int CountInstallLines(const TArray<FString>& RequirementLines);

	bool bRunOnStartup;
	bool bCmdLineDisable;

	FString PipInstallPath;
	FString VenvInterp;

	static const FString PipInstallUtilsVer;
	static const FString PluginsListingFilename;
	static const FString PluginsSitePackageFilename;
	static const FString RequirementsInputFilename;
	static const FString ExtraUrlsFilename;
	static const FString ParsedRequirementsFilename;
};

#endif //WITH_PYTHON
