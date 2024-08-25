// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectFiles.h"

#include "Containers/StringView.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "UnrealVirtualizationTool.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"

namespace
{

/** Utility to find the two string values we need for a mount point based on the project file path */
void ConvertToMountPoint(const FString& ProjectFilePath, FString& OutRootPath, FString& OutContentPath)
{
	FStringView BaseFilename = FPathViews::GetBaseFilename(ProjectFilePath);

	OutRootPath = *WriteToString<260>(TEXT("/"), BaseFilename, TEXT("/"));
	OutContentPath = FPaths::GetPath(ProjectFilePath) / TEXT("Content");
}

/**
 * Utility taken from UGameFeatureData::ReloadConfigs that allows us to apply changes to the ini files (after
 * loading them from game feature plugins for example) and have the changes applied to UObjects.
 * For our use case we need this so that optin/optout settings for UVirtualizationFilterSettings are applied.
 *
 * This is required because we perform filtering at payload submission time. If we change filtering to be
 * applied when a package is saved (i.e. when the package trailer is created) then we can remove this.
 * If we opt to keep the current strategy then this code should be moved to a location where it can be shared
 * by both this tool and the game feature plugin system rather than maintaining two copies.
 */
void ReloadConfigs(FConfigFile& PluginConfig)
{
	// Reload configs so objects get the changes
	for (const auto& ConfigEntry : AsConst(PluginConfig))
	{
		// Skip out if someone put a config section in the INI without any actual data
		if (ConfigEntry.Value.Num() == 0)
		{
			continue;
		}

		const FString& SectionName = ConfigEntry.Key;

		// @todo: This entire overarching process is very similar in its goals as that of UOnlineHotfixManager::HotfixIniFile.
		// Could consider a combined refactor of the hotfix manager, the base config cache system, etc. to expose an easier way to support this pattern

		// INI files might be handling per-object config items, so need to handle them specifically
		const int32 PerObjConfigDelimIdx = SectionName.Find(" ");
		if (PerObjConfigDelimIdx != INDEX_NONE)
		{
			const FString ObjectName = SectionName.Left(PerObjConfigDelimIdx);
			const FString ClassName = SectionName.Mid(PerObjConfigDelimIdx + 1);

			// Try to find the class specified by the per-object config
			UClass* ObjClass = UClass::TryFindTypeSlow<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst | EFindFirstObjectOptions::EnsureIfAmbiguous);
			if (ObjClass)
			{
				// Now try to actually find the object it's referencing specifically and update it
				// @note: Choosing not to warn on not finding it for now, as Fortnite has transient uses instantiated at run-time (might not be constructed yet)
				UObject* PerObjConfigObj = StaticFindFirstObject(ObjClass, *ObjectName, EFindFirstObjectOptions::ExactClass, ELogVerbosity::Warning, TEXT("UGameFeatureData::ReloadConfigs"));
				if (PerObjConfigObj)
				{
					// Intentionally using LoadConfig instead of ReloadConfig, since we do not want to call modify/preeditchange/posteditchange on the objects changed when GIsEditor
					PerObjConfigObj->LoadConfig(nullptr, nullptr, UE::LCPF_ReloadingConfigData | UE::LCPF_ReadParentSections, nullptr);
				}
			}
			else
			{
				PLATFORM_BREAK();
				//	UE_LOG(LogGameFeatures, Warning, TEXT("[GameFeatureData %s]: Couldn't find PerObjectConfig class %s for %s while processing %s, config changes won't be reloaded."), *GetPathNameSafe(this), *ClassName, *ObjectName, *PluginConfig.Name.ToString());
			}
		}
		// Standard INI section case
		else
		{
			// Find the affected class and push updates to all instances of it, including children
			// @note:	Intentionally not using the propagation flags inherent in ReloadConfig to handle this, as it utilizes a naive complete object iterator
			//			and tanks performance pretty badly
			UClass* ObjClass = FindFirstObject<UClass>(*SectionName, EFindFirstObjectOptions::ExactClass | EFindFirstObjectOptions::EnsureIfAmbiguous | EFindFirstObjectOptions::NativeFirst);
			if (ObjClass)
			{
				TArray<UObject*> FoundObjects;
				GetObjectsOfClass(ObjClass, FoundObjects, true, RF_NoFlags);
				for (UObject* CurFoundObj : FoundObjects)
				{
					if (IsValid(CurFoundObj))
					{
						// Intentionally using LoadConfig instead of ReloadConfig, since we do not want to call modify/preeditchange/posteditchange on the objects changed when GIsEditor
						CurFoundObj->LoadConfig(nullptr, nullptr, UE::LCPF_ReloadingConfigData | UE::LCPF_ReadParentSections, nullptr);
					}
				}
			}
		}
	}
}

} // namespace

namespace UE::Virtualization
{


FProject::FProject(FString&& InProjectFilePath)
	: ProjectFilePath(MoveTemp(InProjectFilePath))
{


}
void FProject::AddFile(const FString& PackagePath)
{
	PackagePaths.Add(PackagePath);
}

void FProject::AddPluginFile(const FString& PackagePath, FString&& PluginFilePath)
{
	FPlugin* Plugin = Plugins.FindByPredicate([&PluginFilePath](const FPlugin& Plugin)->bool
		{
			return Plugin.PluginFilePath == PluginFilePath;
		});

	if (Plugin == nullptr)
	{
		Plugin = &Plugins.AddDefaulted_GetRef();
		Plugin->PluginFilePath = MoveTemp(PluginFilePath);
	}

	check(Plugin != nullptr);
	Plugin->PackagePaths.Add(PackagePath);
};

const FString& FProject::GetProjectFilePath() const
{
	return ProjectFilePath;
}

FStringView FProject::GetProjectName() const
{
	return FPathViews::GetBaseFilename(ProjectFilePath);
}

FStringView FProject::GetProjectRoot() const
{
	return FPathViews::GetPath(ProjectFilePath);
}

TArray<FString> FProject::GetAllPackages() const
{
	TArray<FString> Packages = PackagePaths;
	for (const FPlugin& Plugin : Plugins)
	{
		Packages.Append(Plugin.PackagePaths);
	}

	return Packages;
}

void FProject::RegisterMountPoints() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FProject::RegisterMountPoints);

	FString ProjectRootPath;
	FString ProjectContentPath;

	ConvertToMountPoint(ProjectFilePath, ProjectRootPath, ProjectContentPath);
	FPackageName::RegisterMountPoint(ProjectRootPath, ProjectContentPath);

	for (const FPlugin& Plugin : Plugins)
	{
		FString PluginRootPath;
		FString PluginContentPath;

		ConvertToMountPoint(Plugin.PluginFilePath, PluginRootPath, PluginContentPath);
		FPackageName::RegisterMountPoint(PluginRootPath, PluginContentPath);
	}
}

void FProject::UnRegisterMountPoints() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FProject::UnRegisterMountPoints);

	for (const FPlugin& Plugin : Plugins)
	{
		FString PluginRootPath;
		FString PluginContentPath;

		ConvertToMountPoint(Plugin.PluginFilePath, PluginRootPath, PluginContentPath);
		FPackageName::UnRegisterMountPoint(PluginRootPath, PluginContentPath);
	}

	FString ProjectRootPath;
	FString ProjectContentPath;

	ConvertToMountPoint(ProjectFilePath, ProjectRootPath, ProjectContentPath);
	FPackageName::UnRegisterMountPoint(ProjectRootPath, ProjectContentPath);
}

bool FProject::TryLoadConfig(FConfigFile& OutConfig) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FProject::TryLoadConfig);

	const FString ProjectPath = FPaths::GetPath(ProjectFilePath);
	const FString EngineConfigPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Config/"));
	const FString ProjectConfigPath = FPaths::Combine(ProjectPath, TEXT("Config/"));

	OutConfig.Reset();

	if (!FConfigCacheIni::LoadExternalIniFile(OutConfig, TEXT("Engine"), *EngineConfigPath, *ProjectConfigPath, true))
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to load config files for the project '%s"), *ProjectFilePath);
		return false;
	}

	//  Note that the following is taken from UGameFeatureData::InitializeHierarchicalPluginIniFiles as with
	// ReloadConfigs if we decide to keep filtering at submission time, rather than save time then we should
	// probably move this code to a shared location rather than the copy/paste.
	for (const FPlugin& Plugin : Plugins)
	{
		const FString PluginName = FPaths::GetBaseFilename(Plugin.PluginFilePath);
		const FString PluginIniName = PluginName + TEXT("Engine");

		const FString PluginPath = FPaths::GetPath(Plugin.PluginFilePath);
		const FString PluginConfigPath = FPaths::Combine(PluginPath, TEXT("Config/"));

		FConfigFile PluginConfig;
		if (FConfigCacheIni::LoadExternalIniFile(PluginConfig, *PluginIniName, *EngineConfigPath, *PluginConfigPath, false) && (PluginConfig.Num() > 0))
		{
			const FString IniFile = GConfig->GetConfigFilename(TEXT("Engine"));

			if (FConfigFile* ExistingConfig = GConfig->FindConfigFile(IniFile))
			{
				const FString PluginIniPath = FString::Printf(TEXT("%s%s.ini"), *PluginConfigPath, *PluginIniName);
				if (ExistingConfig->Combine(PluginIniPath))
				{
					ReloadConfigs(PluginConfig);
				}
			}
		}
	}

	return true;
}

} // namespace UE::Virtualization
