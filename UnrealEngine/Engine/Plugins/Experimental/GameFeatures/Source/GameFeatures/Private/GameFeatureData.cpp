// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureData.h"
#include "Engine/AssetManager.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "UObject/UObjectHash.h"
#include "UObject/CoreRedirects.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureData)

#define LOCTEXT_NAMESPACE "GameFeatures"

//@TODO: GameFeaturePluginEnginePush: Editing actions/etc... for auto-activated plugins is a poor user experience;
// the changes won't take effect until the editor is restarted or deactivated/reactivated - should probably bounce
// them for you in pre/post edit change (assuming all actions properly handle unloading...)

#if WITH_EDITORONLY_DATA
void UGameFeatureData::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();
	
	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->AddAdditionalAssetBundleData(AssetBundleData);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
EDataValidationResult UGameFeatureData::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			EDataValidationResult ChildResult = Action->IsDataValid(ValidationErrors);
			Result = CombineDataValidationResults(Result, ChildResult);
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			ValidationErrors.Add(FText::Format(LOCTEXT("ActionEntryIsNull", "Null entry at index {0} in Actions"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

void UGameFeatureData::InitializeBasePluginIniFile(const FString& PluginInstalledFilename) const
{
	const FString PluginName = FPaths::GetBaseFilename(PluginInstalledFilename);
	const FString PluginConfigDir = FPaths::GetPath(PluginInstalledFilename) / TEXT("Config/");
	const FString EngineConfigDir = FPaths::EngineConfigDir();

	const bool bIsBaseIniName = false;
	const bool bForceReloadFromDisk = false;
	const bool bWriteDestIni = false;

	// This will be the generated path including platform
	FString PluginConfigFilename = GConfig->GetConfigFilename(*PluginName);

	// Try the deprecated path first that doesn't include the Default prefix
	FConfigFile& PluginConfig = GConfig->Add(PluginConfigFilename, FConfigFile());
	if (!FConfigCacheIni::LoadExternalIniFile(PluginConfig, *PluginName, *EngineConfigDir, *PluginConfigDir, bIsBaseIniName, nullptr, bForceReloadFromDisk, bWriteDestIni))
	{
		// Now try the same rules as PluginManager using Default and the config hierarchy
		FConfigContext Context = FConfigContext::ReadIntoPluginFile(PluginConfig, *FPaths::GetPath(PluginInstalledFilename));

		if (!Context.Load(*PluginName))
		{
			// Nothing to add, remove from map
			GConfig->Remove(PluginConfigFilename);
		}
		else
		{
			FCoreRedirects::ReadRedirectsFromIni(PluginConfigFilename);
			ReloadConfigs(PluginConfig);
		}
	}
	else
	{
		// This is the deprecated loading path that doesn't handle cases like + in arrays
		UE_LOG(LogGameFeatures, Log, TEXT("[GameFeatureData %s]: Loaded deprecated config %s, rename to start with Default for normal parsing"), *GetPathNameSafe(this), *PluginConfigFilename);

		FCoreRedirects::ReadRedirectsFromIni(PluginConfigFilename);
		ReloadConfigs(PluginConfig);
	}
}

void UGameFeatureData::InitializeHierarchicalPluginIniFiles(const FString& PluginInstalledFilename) const
{
	const FString PluginName = FPaths::GetBaseFilename(PluginInstalledFilename);
	const FString PluginConfigDir = FPaths::GetPath(PluginInstalledFilename) / TEXT("Config/");
	const FString EngineConfigDir = FPaths::EngineConfigDir();

	const bool bIsBaseIniName = false;
	const bool bForceReloadFromDisk = false;
	const bool bWriteDestIni = false;

	// @todo: Likely we need to track the diffs this config caused and/or store versions/layers in order to unwind settings during unloading/deactivation
	TArray<FString> IniNamesToLoad = { TEXT("Input"), TEXT("Game"), TEXT("Engine") };
	for (const FString& IniName : IniNamesToLoad)
	{
		const FString PluginIniName = PluginName + IniName;
		// @note: Loading the INI in this manner in order to have a record of relevant sections that were changed so that affected objects can be reloaded. By virtue of how
		// this is parsed (standalone instead of being treated as a combined diff), the actual data within the sections will likely be incorrect. As an example, users adding
		// to an array with the "+" syntax will have the "+" incorrectly embedded inside the data in the temp FConfigFile. It's properly handled in the Combine() below where the
		// actual INI changes are computed.
		FConfigFile TempConfig;
		if (FConfigCacheIni::LoadExternalIniFile(TempConfig, *PluginIniName, *EngineConfigDir, *PluginConfigDir, bIsBaseIniName, nullptr, bForceReloadFromDisk, bWriteDestIni) && (TempConfig.Num() > 0))
		{
			// Need to get the in-memory config filename, the on disk one is likely not up to date
			FString IniFile = GConfig->GetConfigFilename(*IniName);

			if (FConfigFile* ExistingConfig = GConfig->FindConfigFile(IniFile))
			{
				// @todo: Might want to consider modifying the engine level's API here to allow for a combination that yields affected
				// sections and/or optionally just does the reload itself. This route is less efficient than it needs to be, resulting in parsing twice, 
				// once above and once in the Combine() call. Using Combine() here specifically so that special INI syntax (+, ., etc.) is parsed correctly.
				const FString PluginIniPath = FString::Printf(TEXT("%s%s.ini"), *PluginConfigDir, *PluginIniName);
				if (ExistingConfig->Combine(PluginIniPath))
				{
					ReloadConfigs(TempConfig);
				}
				else
				{
					UE_LOG(LogGameFeatures, Error, TEXT("[GameFeatureData %s]: Failed to combine INI %s with base INI %s. Aborting import/application of INI settings."), *GetPathNameSafe(this), *PluginIniName, *IniFile);
				}
			}
		}
	}
}

void UGameFeatureData::ReloadConfigs(FConfigFile& PluginConfig) const
{
	// Reload configs so objects get the changes
	for (const auto& ConfigEntry : PluginConfig)
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
				UE_LOG(LogGameFeatures, Warning, TEXT("[GameFeatureData %s]: Couldn't find PerObjectConfig class %s for %s while processing %s, config changes won't be reloaded."), *GetPathNameSafe(this), *ClassName, *ObjectName, *PluginConfig.Name.ToString());
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

#undef LOCTEXT_NAMESPACE

