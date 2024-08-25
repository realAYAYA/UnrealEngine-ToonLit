// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureData.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "GameFeaturesSubsystem.h"
#include "InstallBundleUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/CoreRedirects.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileFragment.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Interfaces/IPluginManager.h"

#if WITH_EDITOR
#include "Settings/EditorExperimentalSettings.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerHelper.h"
#include "GameFeatureAction_AddWorldPartitionContent.h"
#include "GameFeatureAction_AddWPContent.h"
#include "Misc/DataValidation.h"
#include "Engine/Level.h"
#endif

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
EDataValidationResult UGameFeatureData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			EDataValidationResult ChildResult = Action->IsDataValid(Context);
			Result = CombineDataValidationResults(Result, ChildResult);
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("ActionEntryIsNull", "Null entry at index {0} in Actions"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

static TAutoConsoleVariable<bool> CVarAllowRuntimeDeviceProfiles(
	TEXT("GameFeaturePlugin.AllowRuntimeDeviceProfiles"),
	true,
	TEXT("Allow game feature plugins to generate device profiles from config based on existing parents"),
	ECVF_Default);

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
		FConfigContext Context = FConfigContext::ReadIntoPluginFile(PluginConfig, *FPaths::GetPath(PluginInstalledFilename),
				IPluginManager::Get().FindPluginFromPath(PluginName)->GetExtensionBaseDirs());

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
	UDeviceProfileManager& DeviceProfileManager = UDeviceProfileManager::Get();

	FString PlatformName = FPlatformProperties::IniPlatformName();

#if ALLOW_OTHER_PLATFORM_CONFIG
	const UDeviceProfile* PreviewDeviceProfile = DeviceProfileManager.GetPreviewDeviceProfile();
	if (PreviewDeviceProfile)
	{
		PlatformName = PreviewDeviceProfile->ConfigPlatform.IsEmpty() ? PreviewDeviceProfile->DeviceType : PreviewDeviceProfile->ConfigPlatform;
	}
#endif

	FString PluginInstalledStandardFilename = PluginInstalledFilename;
	if (!FPaths::IsRelative(PluginInstalledStandardFilename))
	{
		FPaths::MakeStandardFilename(PluginInstalledStandardFilename);
	}

	const FString PluginName = FPaths::GetBaseFilename(PluginInstalledStandardFilename);
	const FString PlatformExtensionDir = FPaths::ProjectPlatformExtensionDir(*PlatformName);
	const FString EngineConfigDir = FPaths::EngineConfigDir();
	const FString PluginConfigDir = FPaths::GetPath(PluginInstalledStandardFilename) / TEXT("Config/");
	const FString PluginPlatformConfigDir = FPaths::Combine(PluginConfigDir, PlatformName);
	const FString PluginPlatformExtensionDir = FPaths::GetPath(PluginInstalledStandardFilename).Replace(*FPaths::ProjectDir(), *PlatformExtensionDir) / TEXT("Config");

	// We're going to test a lot of paths, so only do it if some config actually exists
	if (!FPaths::DirectoryExists(PluginConfigDir) && !FPaths::DirectoryExists(PluginPlatformConfigDir) && !FPaths::DirectoryExists(PluginPlatformExtensionDir))
	{
		return;
	}

	const bool bIsBaseIniName = false;
	const bool bForceReloadFromDisk = false;
	const bool bWriteDestIni = false;
	const bool bCreateDeviceProfiles = CVarAllowRuntimeDeviceProfiles->GetBool();

	struct FIniLoadingParams
	{
		FIniLoadingParams(const FString& InName, bool bInUsePlatformDir = false, bool bInCreateDeviceProfiles = false)
			: Name(InName), bUsePlatformDir(bInUsePlatformDir), bCreateDeviceProfiles(bInCreateDeviceProfiles)
		{}

		FString Name;
		bool bUsePlatformDir;
		bool bCreateDeviceProfiles;
	};

	// Engine.ini and DeviceProfiles.ini will also support platform extensions
	TArray<FIniLoadingParams> IniFilesToLoad = { FIniLoadingParams(TEXT("Input")),
		FIniLoadingParams(TEXT("Game")), FIniLoadingParams(TEXT("Game"), true),
		FIniLoadingParams(TEXT("Engine")), FIniLoadingParams(TEXT("Engine"), true),
#if UE_EDITOR
		FIniLoadingParams(TEXT("Editor")),
#endif
		FIniLoadingParams(TEXT("DeviceProfiles"), false, bCreateDeviceProfiles),
		FIniLoadingParams(TEXT("DeviceProfiles"), true, bCreateDeviceProfiles)
	};

	// Create overridden device profiles for each matching rule in config
	auto InsertRuntimeDeviceProfilesIntoConfig = [&](FConfigFile& PluginConfig, const FConfigFile& ExistingConfig)
	{
		TMap<FString, FConfigSection> ConfigSectionsToAdd;

		TArray<FString> ResultingProfiles;

		for (auto& Section : AsConst(PluginConfig))
		{
			FString RuleName, ParentClass;
			if (Section.Key.Split(TEXT(" "), &RuleName, &ParentClass))
			{
				// Early reject anything that's not handled in here
				const bool bIsRuntimeDeviceProfileRule = (ParentClass == "RuntimeDeviceProfileRule");
				const bool bIsDeviceProfileFragment = (ParentClass == UDeviceProfileFragment::StaticClass()->GetName());
				if (!(bIsRuntimeDeviceProfileRule || bIsDeviceProfileFragment))
				{
					continue;
				}

				// Check the existing config because at this point in time it has already been hotfixed from the base empty config
				// Those CVars are also always without a +-. prefix because they're the result of the hotfix applied to the empty config.
				// @todo: we cannot use hotfixes to remove CVars because HF happens way before GFPs have a chance to load.
				// The hotfix process is destructive and doesn't leave us an opportunity to read the hotfix delta when loading GFPs.
				TArray<FConfigValue> HotfixCVars;
				if (const FConfigSection* HotfixSection = ExistingConfig.FindSection(Section.Key))
				{
					HotfixSection->MultiFind("CVars", HotfixCVars);
				}

				// Extract key-value pairs for CVars and FragmentIncludes, keeping the +-. prefix
				TMultiMap<FName, FConfigValue> PluginCVars;
				TMultiMap<FName, FConfigValue> FragmentIncludes;
				for (const auto& Entry : Section.Value)
				{
					const FString& EntryKey = Entry.Key.ToString();
					if (EntryKey.RightChop(1).StartsWith("CVars"))
					{
						PluginCVars.Add(Entry.Key, Entry.Value);
					}
					else if (EntryKey.RightChop(1).StartsWith("FragmentIncludes"))
					{
						FragmentIncludes.Add(Entry.Key, Entry.Value);
					}
				}

				// Check if a CVar should be either included, or removed for a new hotfix value
				auto ShouldKeepCVar = [&HotfixCVars](const FString& Key, FString& Value) -> bool
				{
					for (const FConfigValue& HotfixCVarData : HotfixCVars)
					{
						FString HotfixCVarKey, HotfixCVarValue;
						if (HotfixCVarData.GetValue().Split(TEXT("="), &HotfixCVarKey, &HotfixCVarValue) && HotfixCVarKey == Key && HotfixCVarValue != Value)
						{
							Value = HotfixCVarValue;
							return false;
						}
					}
					return true;
				};

				// Process new runtime device profile
				if (bIsRuntimeDeviceProfileRule)
				{
					UE_LOG(LogGameFeatures, Log, TEXT("Game feature '%s' found runtime device profile rule %s"), *PluginName, *RuleName);

					// Extract metadata
					const FConfigValue* ParentProfileName = Section.Value.Find("ParentProfileName");
					const FConfigValue* ProfileSuffix = Section.Value.Find("ProfileSuffix");

					if (ParentProfileName && ProfileSuffix)
					{
						// We need to load all candidate device profiles here or else we won't be able to create a child for them
						TArray<FString> LoadableProfileNames = DeviceProfileManager.GetLoadableProfileNames(*PlatformName);
						for (const FString& ProfileName : LoadableProfileNames)
						{
							DeviceProfileManager.FindProfile(ProfileName, true, *PlatformName);
						}

						for (const UDeviceProfile* Profile : DeviceProfileManager.Profiles)
						{
							// Check if one of the parents for this profile is the one this rule applies to
							bool bProfileHasCompatibleParent = false;
							const UDeviceProfile* CurrentProfile = Profile;
							do
							{
								bProfileHasCompatibleParent = CurrentProfile->GetName() == ParentProfileName->GetValue();
								CurrentProfile = CurrentProfile->GetParentProfile();
							} while (!bProfileHasCompatibleParent && CurrentProfile != nullptr);

							// Create the config for a runtime profile
							if (bProfileHasCompatibleParent && !Profile->GetName().EndsWith(ProfileSuffix->GetValue()))
							{
								// Ignore duplicates: only the first match in a given config file will be accepted
								const FString FinalProfileName = Profile->GetName() + ProfileSuffix->GetValue();
								if (ResultingProfiles.Contains(FinalProfileName))
								{
									UE_LOG(LogGameFeatures, Log, TEXT("Ignoring profile %s that has already been overriden as %s"), *Profile->GetName(), *FinalProfileName);
									continue;
								}

								FConfigSection RuntimeProfile;
								RuntimeProfile.Add("DeviceType", PlatformName);
								RuntimeProfile.Add("BaseProfileName", FConfigValue(Profile->GetName()));

								UE_LOG(LogGameFeatures, Log, TEXT("Creating override for base profile %s"), *Profile->GetName());

								// Inject the parent's matched fragments into the config, if any
								if (Profile->GetName().Contains("MatchedFragments"))
								{
									FString MatchingRulesSectionName = Profile->GetName() + TEXT(" ") + UDeviceProfile::StaticClass()->GetName();
									FString MatchingRulesArrayName = TEXT("MatchingRules");
									TArray<FString> MatchingRulesArray;

#if ALLOW_OTHER_PLATFORM_CONFIG
									FConfigCacheIni* PlatformConfigSystem = FConfigCacheIni::ForPlatform(*PlatformName);
#else
									FConfigCacheIni* PlatformConfigSystem = GConfig;
#endif

									PlatformConfigSystem->GetArray(*MatchingRulesSectionName, *MatchingRulesArrayName, MatchingRulesArray, GDeviceProfilesIni);
									UE_LOG(LogGameFeatures, Log, TEXT("Found %d fragment matching rules"), MatchingRulesArray.Num());

									for (const FString& Rule : MatchingRulesArray)
									{
										RuntimeProfile.Add("+MatchingRules", FConfigValue(Rule));
									}
								}

								// Add fragment includes
								for (const auto& FragmentInclude : FragmentIncludes)
								{
									RuntimeProfile.Add(FragmentInclude.Key, FConfigValue(FragmentInclude.Value.GetValue()));
								}

								// Add direct CVars
								for (const auto& CVar : PluginCVars)
								{
									FString CVarKey, CVarValue;
									if (CVar.Value.GetValue().Split(TEXT("="), &CVarKey, &CVarValue) && ShouldKeepCVar(CVarKey, CVarValue))
									{
										UE_LOG(LogGameFeatures, Log, TEXT(" Found CVar: %s=%s"), *CVarKey, *CVarValue);
										RuntimeProfile.Add(CVar.Key, FConfigValue(CVarKey + "=" + CVarValue));
									}
								}

								// Add hotfix CVars
								for (const auto& CVar : HotfixCVars)
								{
									FString CVarKey, CVarValue;
									if (CVar.GetValue().Split(TEXT("="), &CVarKey, &CVarValue) && !PluginCVars.Contains(FName(*CVarKey)))
									{
										UE_LOG(LogGameFeatures, Log, TEXT(" Added CVar: %s=%s"), *CVarKey, *CVarValue);
										RuntimeProfile.Add("+CVars", FConfigValue(CVarKey + "=" + CVarValue));
									}
								}

								ConfigSectionsToAdd.Add(FinalProfileName + TEXT(" ") + UDeviceProfile::StaticClass()->GetName(), RuntimeProfile);
								ResultingProfiles.Add(FinalProfileName);
							}
						}
					}
					else
					{
						UE_LOG(LogGameFeatures, Warning, TEXT("Game feature '%s' has invalid runtime device profile with parent %s, suffix %s, %d CVars, %d fragments"),
							*PluginName, ParentProfileName ? *ParentProfileName->GetValue() : TEXT("null"), ProfileSuffix ? *ProfileSuffix->GetValue() : TEXT("null"),
							FragmentIncludes.Num(), PluginCVars.Num());
					}
				}

				// Hotfix device profile fragments
				else if (bIsDeviceProfileFragment)
				{
					UE_LOG(LogGameFeatures, Log, TEXT("Game feature '%s' found device profile fragment %s"), *PluginName, *RuleName);

					if (HotfixCVars.Num() > 0)
					{
						// Update existing CVars
						for (const auto& CVar : PluginCVars)
						{
							FString PluginCVarKey, PluginCVarValue;
							if (CVar.Value.GetValue().Split(TEXT("="), &PluginCVarKey, &PluginCVarValue) && !ShouldKeepCVar(PluginCVarKey, PluginCVarValue))
							{
								UE_LOG(LogGameFeatures, Log, TEXT(" Removed CVar: %s"), *CVar.Value.GetValue());
								PluginConfig.RemoveFromSection(*Section.Key, CVar.Key, CVar.Value.GetValue());
							}
							else
							{
								UE_LOG(LogGameFeatures, Log, TEXT(" Kept CVar: %s=%s"), *PluginCVarKey, *PluginCVarValue);
							}
						}

						// Add new hotfix CVars
						for (const auto& CVar : HotfixCVars)
						{
							FString HotfixCVarKey, HotfixCVarValue;
							if (CVar.GetValue().Split(TEXT("="), &HotfixCVarKey, &HotfixCVarValue) && !PluginCVars.Contains(FName(*HotfixCVarKey)))
							{
								UE_LOG(LogGameFeatures, Log, TEXT(" Added CVar: %s=%s"), *HotfixCVarKey, *HotfixCVarValue);
								PluginConfig.AddToSection(*Section.Key, "+CVars", HotfixCVarKey + "=" + HotfixCVarValue);
							}
						}
					}
				}
			}
		}

		PluginConfig.Append(ConfigSectionsToAdd);
	};

	// Create device profiles for this plugin from config
	auto LoadDeviceProfilesFromConfig = [&](const FConfigFile& Config)
	{
		for (TPair<const FString&, const FConfigSection&> Section : Config)
		{
			FString ProfileName;
			FString ParentClass;
			if (Section.Key.Split(TEXT(" "), &ProfileName, &ParentClass) && ParentClass == UDeviceProfile::StaticClass()->GetName())
			{
				const FConfigValue* DeviceType = Section.Value.Find("DeviceType");
				if (DeviceType)
				{
					UE_LOG(LogGameFeatures, Log, TEXT("Game feature '%s' adding new device profile %s"), *PluginName, *ProfileName);
					DeviceProfileManager.CreateProfile(ProfileName, DeviceType->GetValue(), FString(), *PlatformName);
				}
			}
		}
	};

	// @todo: Likely we need to track the diffs this config caused and/or store versions/layers in order to unwind settings during unloading/deactivation
	for (const FIniLoadingParams& Ini : IniFilesToLoad)
	{
		const FString PluginIniName = Ini.bUsePlatformDir ? (PlatformName + PluginName + Ini.Name) : PluginName + Ini.Name;

		FString ConfigDirectory;
		if (Ini.bUsePlatformDir)
		{
			// We'll look first in the platform extension directory, then in the plugin's platform directory
			if (FPaths::FileExists(FPaths::Combine(PluginPlatformExtensionDir, PluginIniName + ".ini")))
			{
				ConfigDirectory = PluginPlatformExtensionDir;
			}
			else
			{
				ConfigDirectory = PluginPlatformConfigDir;
			}
		}
		else
		{
			ConfigDirectory = PluginConfigDir;
		}
		ConfigDirectory += TEXT("/");

		// @note: Loading the INI in this manner in order to have a record of relevant sections that were changed so that affected objects can be reloaded. By virtue of how
		// this is parsed (standalone instead of being treated as a combined diff), the actual data within the sections will likely be incorrect. As an example, users adding
		// to an array with the "+" syntax will have the "+" incorrectly embedded inside the data in the temp FConfigFile. It's properly handled in the Combine() below where the
		// actual INI changes are computed.
		FConfigFile Config;
		if (FConfigCacheIni::LoadExternalIniFile(Config, *PluginIniName, *EngineConfigDir, *ConfigDirectory, bIsBaseIniName, nullptr, bForceReloadFromDisk, bWriteDestIni) && (Config.Num() > 0))
		{
			UE_LOG(LogGameFeatures, Log, TEXT("Game feature '%s' loaded config file %s"), *PluginName, *PluginIniName);

			// Need to get the in-memory config filename, the on disk one is likely not up to date
			FString IniFile = GConfig->GetConfigFilename(*Ini.Name);

			// Ensure we push new device profile config to the appropriate config branch - GConfig could be Windows while we're previewing a console
			FConfigFile* ExistingConfig = nullptr;
#if ALLOW_OTHER_PLATFORM_CONFIG
			if (Ini.bCreateDeviceProfiles && Ini.bUsePlatformDir && !FPlatformProperties::RequiresCookedData())
			{
				FConfigCacheIni* PlatformConfigSystem = FConfigCacheIni::ForPlatform(*PlatformName);
				ExistingConfig = PlatformConfigSystem->FindConfigFile(GDeviceProfilesIni);
			}
#endif

			if (ExistingConfig == nullptr)
			{
				ExistingConfig = GConfig->FindConfigFile(IniFile);
			}

			if (ExistingConfig)
			{
				if (Ini.bCreateDeviceProfiles)
				{
					InsertRuntimeDeviceProfilesIntoConfig(Config, *ExistingConfig);
				}

				FString ConfigAsString;
				Config.WriteToString(ConfigAsString, PluginIniName);

				// @todo: Might want to consider modifying the engine level's API here to allow for a combination that yields affected
				// sections and/or optionally just does the reload itself. This route is less efficient than it needs to be, resulting in parsing twice, 
				// once above and once in the Combine() call. Using Combine() here specifically so that special INI syntax (+, ., etc.) is parsed correctly.
				const FString PluginIniPath = FString::Printf(TEXT("%s%s.ini"), *ConfigDirectory, *PluginIniName);
				ExistingConfig->CombineFromBuffer(ConfigAsString, PluginIniPath);

#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
				FConfigFile::OverrideFromCommandline(ExistingConfig, Ini.Name);
#endif // ALLOW_INI_OVERRIDE_FROM_COMMANDLINE

				if (Ini.bCreateDeviceProfiles)
				{
					LoadDeviceProfilesFromConfig(Config);
				}
				else
				{
					ReloadConfigs(Config);
				}
			}
		}
	}
}

void UGameFeatureData::ReloadConfigs(FConfigFile& PluginConfig) const
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
				UE_LOG(LogGameFeatures, Warning, TEXT("[GameFeatureData %s]: Couldn't find PerObjectConfig class %s for %s while processing %s, config changes won't be reloaded."), *GetPathNameSafe(this), *ClassName, *ObjectName, *PluginConfig.Name.ToString());
			}
		}
		// Standard INI section case
		else
		{
			// Find the affected class and push updates to all instances of it, including children
			// @note:	Intentionally not using the propagation flags inherent in ReloadConfig to handle this, as it utilizes a naive complete object iterator
			//			and tanks performance pretty badly
			UClass* ObjClass = FindFirstObject<UClass>(*SectionName, EFindFirstObjectOptions::EnsureIfAmbiguous | EFindFirstObjectOptions::NativeFirst);
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

#if WITH_EDITOR

TArray<UClass*> UGameFeatureData::GetDisallowedActions() const
{
	TArray<UClass*> DisallowedClasses;

	if (!GetDefault<UEditorExperimentalSettings>()->bEnableWorldPartitionExternalDataLayers)
	{
		DisallowedClasses.Add(UGameFeatureAction_AddWorldPartitionContent::StaticClass());
	}

	return DisallowedClasses;
}

FName UGameFeatureData::GetContentBundleGuidsAssetRegistryTagPrivate()
{
	static const FName ContentBundlesTag("ContentBundleGuids");
	return ContentBundlesTag;
}

void UGameFeatureData::GetContentBundleGuids(const FAssetData& Asset, TArray<FGuid>& OutContentBundleGuids)
{
	FString ContentBundleGuidsStr;
	if (Asset.GetTagValue(GetContentBundleGuidsAssetRegistryTagPrivate(), ContentBundleGuidsStr))
	{
		TArray<FString> ContentBundleGuidsStrArray;
		ContentBundleGuidsStr.ParseIntoArray(ContentBundleGuidsStrArray, TEXT(","));
		for (const FString& GuidStr : ContentBundleGuidsStrArray)
		{
			FGuid ContentBundleGuid;
			if (FGuid::Parse(GuidStr, ContentBundleGuid))
			{
				OutContentBundleGuids.Add(ContentBundleGuid);
			}
		}
	}
}

void UGameFeatureData::GetDependencyDirectoriesFromAssetData(const FAssetData& AssetData, TArray<FString>& OutDependencyDirectories)
{
	const FString MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString()).ToString();

	TArray<FGuid> ContentBundleGuids;
	GetContentBundleGuids(AssetData, ContentBundleGuids);
	for (const FGuid& ContentBundleGuid : ContentBundleGuids)
	{
		FString ContentBundleExternalActorPath;
		if (ContentBundlePaths::BuildContentBundleExternalActorPath(MountPoint, ContentBundleGuid, ContentBundleExternalActorPath))
		{
			const FString ExternalActorPath = ULevel::GetExternalActorsPath(ContentBundleExternalActorPath);
			OutDependencyDirectories.Add(ExternalActorPath);
		}
	}

	TArray<FExternalDataLayerUID> ExternalDataLayerUIDs;
	FExternalDataLayerHelper::GetExternalDataLayerUIDs(AssetData, ExternalDataLayerUIDs);
	for (const FExternalDataLayerUID& ExternalDataLayerUID : ExternalDataLayerUIDs)
	{
		FString ExternalDataLayerRootPath;
		if (FExternalDataLayerHelper::BuildExternalDataLayerRootPath(MountPoint, ExternalDataLayerUID, ExternalDataLayerRootPath))
		{
			const FString ExternalActorsPath = ULevel::GetExternalActorsPath(ExternalDataLayerRootPath);
			OutDependencyDirectories.Add(ExternalActorsPath);
		}
	}
}

void UGameFeatureData::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UGameFeatureData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	TArray<FGuid> ContentBundleGuids;
	TArray<FExternalDataLayerUID> ExternalDataLayerUIDs;

	for (UGameFeatureAction* Action : Actions)
	{
		if (UGameFeatureAction_AddWPContent* WPAction = Cast<UGameFeatureAction_AddWPContent>(Action))
		{
			if (const UContentBundleDescriptor* ContentBundleDescriptor = WPAction->GetContentBundleDescriptor())
			{
				ContentBundleGuids.Add(ContentBundleDescriptor->GetGuid());
			}
		}

		if (UGameFeatureAction_AddWorldPartitionContent* WPAction = Cast<UGameFeatureAction_AddWorldPartitionContent>(Action))
		{
			if (const UExternalDataLayerAsset* ExternalDataLayerAsset = WPAction->GetExternalDataLayerAsset())
			{
				ExternalDataLayerUIDs.Add(ExternalDataLayerAsset->GetUID());
			}
		}
	}

	if (ContentBundleGuids.Num() > 0)
	{
		FString ContentBundleGuidsStr = FString::JoinBy(ContentBundleGuids, TEXT(","), [&](const FGuid& Guid) { return Guid.ToString(); });
		Context.AddTag(FAssetRegistryTag(GetContentBundleGuidsAssetRegistryTagPrivate(), ContentBundleGuidsStr, FAssetRegistryTag::TT_Hidden));
	}

	FExternalDataLayerHelper::AddAssetRegistryTags(Context, ExternalDataLayerUIDs);
}
#endif

FString UGameFeatureData::GetInstallBundleName(FStringView PluginName, bool bEvenIfDoesntExist /*= false*/)
{
	const FString BundleName = FString::Printf(TEXT("GFP_%.*s"), PluginName.Len(), PluginName.GetData());
	if (bEvenIfDoesntExist)
	{
		return BundleName;
	}

	if (InstallBundleUtil::HasInstallBundleInConfig(BundleName))
	{
		return BundleName;
	}
	else
	{
		return TEXT("");
	}
}

void UGameFeatureData::GetPluginName(FString& PluginName) const
{
	UGameFeatureData::GetPluginName(this, PluginName);
}

void UGameFeatureData::GetPluginName(const UGameFeatureData* GFD, FString& PluginName)
{
	if (GFD)
	{
		const bool bIsTransient = (GFD->GetFlags() & RF_Transient) != 0;
		if (bIsTransient)
		{
			PluginName = GFD->GetName();
		}
		else
		{
			const FString GameFeaturePath = GFD->GetOutermost()->GetName();
			if (ensureMsgf(UAssetManager::GetContentRootPathFromPackageName(GameFeaturePath, PluginName), TEXT("Must be a valid package path with a root. GameFeaturePath: %s"), *GameFeaturePath))
			{
				// Trim the leading and trailing slashes
				PluginName = PluginName.LeftChop(1).RightChop(1);
			}
			else
			{
				// Not a great fallback but better than nothing. Make sure this asset is in the right folder so we can get the plugin name.
				PluginName = GFD->GetName();
			}
		}
	}
}

bool UGameFeatureData::IsGameFeaturePluginRegistered() const
{
	FString PluginURL;
	FString PluginName;
	GetPluginName(PluginName);
	if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, PluginURL))
	{
		return UGameFeaturesSubsystem::Get().IsGameFeaturePluginRegistered(PluginURL);
	}
	return false;
}

bool UGameFeatureData::IsGameFeaturePluginActive() const
{
	FString PluginURL;
	FString PluginName;
	GetPluginName(PluginName);
	if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, PluginURL))
	{
		return UGameFeaturesSubsystem::Get().IsGameFeaturePluginActive(PluginURL);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

