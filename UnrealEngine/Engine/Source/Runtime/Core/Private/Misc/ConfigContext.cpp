// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/ConfigContext.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigHierarchy.h"
#include "Misc/CoreMisc.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "Misc/RemoteConfigIni.h"
#include "Misc/Paths.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"
#include "HAL/LowLevelMemStats.h"

namespace
{
	FName VersionName("Version");
	FName PreserveName("Preserve");
	FString LegacyIniVersionString = TEXT("IniVersion");
	FString LegacyEngineString = TEXT("Engine.Engine");
	FString CurrentIniVersionString = TEXT("CurrentIniVersion");
	const TCHAR* SectionsToSaveString = TEXT("SectionsToSave");
	const TCHAR* SaveAllSectionsKey = TEXT("bCanSaveAllSections");
}

#if ALLOW_OTHER_PLATFORM_CONFIG
TMap<FString, TUniquePtr<FConfigPluginDirs>> FConfigContext::ConfigToPluginDirs;
FCriticalSection FConfigContext::ConfigToPluginDirsLock;
#endif

FConfigContext::FConfigContext(FConfigCacheIni* InConfigSystem, bool InIsHierarchicalConfig, const FString& InPlatform, FConfigFile* DestConfigFile)
	: ConfigSystem(InConfigSystem)
	, Platform(InPlatform)
	, bIsHierarchicalConfig(InIsHierarchicalConfig)
{
	if (DestConfigFile != nullptr)
	{
		ConfigFile = DestConfigFile;
		bDoNotResetConfigFile = true;
	}

	if (Platform.IsEmpty())
	{
		// read from, for instance Windows
		Platform = FPlatformProperties::IniPlatformName();
		// but save Generated ini files to, say, WindowsEditor
		SavePlatform = FPlatformProperties::PlatformName();
	}
	else if (Platform == FPlatformProperties::IniPlatformName())
	{
		// but save Generated ini files to, say, WindowsEditor
		SavePlatform = FPlatformProperties::PlatformName();
	}
	else
	{
		SavePlatform = Platform;
	}


	// now set to defaults anything not already set
	EngineConfigDir = FPaths::EngineConfigDir();
	ProjectConfigDir = FPaths::SourceConfigDir();

	// set settings that apply when using GConfig
	if (ConfigSystem != nullptr && ConfigSystem == GConfig)
	{
		bWriteDestIni = true;
		bUseHierarchyCache = true;
		bAllowGeneratedIniWhenCooked = true;
		bAllowRemoteConfig = true;
	}
}

void FConfigContext::CachePaths()
{
	// these are needed for single ini files
	if (bIsHierarchicalConfig)
	{
		// for the hierarchy replacements, we need to have a directory called Config - or we will have to do extra processing for these non-standard cases
		check(EngineConfigDir.EndsWith(TEXT("Config/")));
		check(ProjectConfigDir.EndsWith(TEXT("Config/")));

		EngineRootDir = FPaths::GetPath(FPaths::GetPath(EngineConfigDir));
		ProjectRootDir = FPaths::GetPath(FPaths::GetPath(ProjectConfigDir));

		if (FPaths::IsUnderDirectory(ProjectRootDir, EngineRootDir))
		{
			FString RelativeDir = ProjectRootDir;
			FPaths::MakePathRelativeTo(RelativeDir, *(EngineRootDir + TEXT("/")));
			ProjectNotForLicenseesDir = FPaths::Combine(EngineRootDir, TEXT("Restricted/NotForLicensees"), RelativeDir);
			ProjectNoRedistDir = FPaths::Combine(EngineRootDir, TEXT("Restricted/NoRedist"), RelativeDir);
		}
		else
		{
			ProjectNotForLicenseesDir = FPaths::Combine(ProjectRootDir, TEXT("Restricted/NotForLicensees"));
			ProjectNoRedistDir = FPaths::Combine(ProjectRootDir, TEXT("Restricted/NoRedist"));
		}
	}
}

FConfigContext& FConfigContext::ResetBaseIni(const TCHAR* InBaseIniName)
{
	// for now, there's nothing that needs to be updated, other than the name here
	BaseIniName = InBaseIniName;

	if (!bDoNotResetConfigFile)
	{
		ConfigFile = nullptr;
	}

	return *this;
}

const FConfigContext::FPerPlatformDirs& FConfigContext::GetPerPlatformDirs(const FString& PlatformName)
{
	FConfigContext::FPerPlatformDirs* Dirs = FConfigContext::PerPlatformDirs.Find(PlatformName);
	if (Dirs == nullptr)
	{
		// default to <skip> so we don't look in non-existant platform extension directories
		FString PluginExtDir = TEXT("<skip>");
		if (bIsForPlugin)
		{
			// look if there's a plugin extension for this platform, it will have the platform name in the path
			for (const FString& ChildDir : ChildPluginBaseDirs)
			{
				if (ChildDir.Contains(*FString::Printf(TEXT("/%s/"), *PlatformName)))
				{
					PluginExtDir = ChildDir;
					break;
				}
			}
		}
		
		Dirs = &PerPlatformDirs.Emplace(PlatformName, FConfigContext::FPerPlatformDirs
			{
				// PlatformExtensionEngineDir
				FPaths::ConvertPath(EngineRootDir, FPaths::EPathConversion::Engine_PlatformExtension, *PlatformName),
				// PlatformExtensionProjectDir
				FPaths::ConvertPath(ProjectRootDir, FPaths::EPathConversion::Project_PlatformExtension, *PlatformName, *ProjectRootDir),
				// PluginExtensionDir
				PluginExtDir,
			});
	}
	return *Dirs;
}

bool FConfigContext::Load(const TCHAR* InBaseIniName, FString& OutFinalFilename)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FConfigContext::Load);
	// for single file loads, just return early of the file doesn't exist
	const bool bBaseIniNameIsFullInIFilePath = FString(InBaseIniName).EndsWith(TEXT(".ini"));
	if (!bIsHierarchicalConfig && bBaseIniNameIsFullInIFilePath && !DoesConfigFileExistWrapper(InBaseIniName, IniCacheSet))
	{
		return false;
	}

	if (bCacheOnNextLoad || BaseIniName != InBaseIniName)
	{
		ResetBaseIni(InBaseIniName);
		CachePaths();
		bCacheOnNextLoad = false;
	}


	bool bPerformLoad;
	if (!PrepareForLoad(bPerformLoad))
	{
		return false;
	}

	// if we are reloading a known ini file (where OutFinalIniFilename already has a value), then we need to leave the OutFinalFilename alone until we can remove LoadGlobalIniFile completely
	if (OutFinalFilename.Len() > 0 && OutFinalFilename == BaseIniName)
	{
		// do nothing
	}
	else
	{
		check(!bWriteDestIni || !DestIniFilename.IsEmpty());

		OutFinalFilename = DestIniFilename;
	}

	// now load if we need (PrepareForLoad may find an existing file and just use it)
	bool bSuccess = bPerformLoad ? PerformLoad() : true;

#if ALLOW_OTHER_PLATFORM_CONFIG
	if (bSuccess && bIsForPlugin && Platform == FPlatformProperties::IniPlatformName())
	{
		// We have successfuly loaded a plugin ini file for the main platform. Cache the plugin info in case we want to load this ForPlatform.	
		FScopeLock Lock(&FConfigContext::ConfigToPluginDirsLock);
		FConfigContext::ConfigToPluginDirs.Add(InBaseIniName, TUniquePtr<FConfigPluginDirs>(new FConfigPluginDirs(PluginRootDir, ChildPluginBaseDirs)));
	}
#endif

	return bSuccess;
}

bool FConfigContext::Load(const TCHAR* InBaseIniName)
{
	FString Discard;
	return Load(InBaseIniName, Discard);
}


bool FConfigContext::PrepareForLoad(bool& bPerformLoad)
{
	checkf(ConfigSystem != nullptr || ConfigFile != nullptr, TEXT("Loading config expects to either have a ConfigFile already passed in, or have a ConfigSystem passed in"));

	if (bForceReload)
	{
		// re-use an existing ConfigFile's Engine/Project directories if we have a config system to look in,
		// or no config system and the platform matches current platform (which will look in GConfig)
		if (ConfigSystem != nullptr || (Platform == FPlatformProperties::IniPlatformName() && GConfig != nullptr))
		{
			bool bNeedRecache = false;
			FConfigCacheIni* SearchSystem = ConfigSystem == nullptr ? GConfig : ConfigSystem;
			FConfigFile* BaseConfigFile = SearchSystem->FindConfigFileWithBaseName(*BaseIniName);
			if (BaseConfigFile != nullptr)
			{
				if (!BaseConfigFile->SourceEngineConfigDir.IsEmpty() && BaseConfigFile->SourceEngineConfigDir != EngineConfigDir)
				{
					EngineConfigDir = BaseConfigFile->SourceEngineConfigDir;
					bNeedRecache = true;
				}
				if (!BaseConfigFile->SourceProjectConfigDir.IsEmpty() && BaseConfigFile->SourceProjectConfigDir != ProjectConfigDir)
				{
					ProjectConfigDir = BaseConfigFile->SourceProjectConfigDir;
					bNeedRecache = true;
				}
				if (bNeedRecache)
				{
					CachePaths();
				}
			}
		}

	}

	// setup for writing out later on
	if (bWriteDestIni || bAllowGeneratedIniWhenCooked || FPlatformProperties::RequiresCookedData())
	{
		// delay filling out GeneratedConfigDir because some early configs can be read in that set -savedir, and 
		// FPaths::GeneratedConfigDir() will permanently cache the value
		if (GeneratedConfigDir.IsEmpty())
		{
			GeneratedConfigDir = FPaths::GeneratedConfigDir();
		}

		// calculate where this file will be saved/generated to (or at least the key to look up in the ConfigSystem)
		DestIniFilename = FConfigCacheIni::GetDestIniFilename(*BaseIniName, *SavePlatform, *GeneratedConfigDir);

		if (bAllowRemoteConfig)
		{
			// Start the loading process for the remote config file when appropriate
			if (FRemoteConfig::Get()->ShouldReadRemoteFile(*DestIniFilename))
			{
				FRemoteConfig::Get()->Read(*DestIniFilename, *BaseIniName);
			}

			FRemoteConfigAsyncIOInfo* RemoteInfo = FRemoteConfig::Get()->FindConfig(*DestIniFilename);
			if (RemoteInfo && (!RemoteInfo->bWasProcessed || !FRemoteConfig::Get()->IsFinished(*DestIniFilename)))
			{
				// Defer processing this remote config file to until it has finish its IO operation
				bPerformLoad = false;
				return false;
			}
		}
	}

	// we can re-use an existing file if:
	//   we are not loading into an existing ConfigFile
	//   we don't want to reload
	//   we found an existing file in the ConfigSystem
	//   the existing file has entries (because Known config files are always going to be found, but they will be empty)
	bool bLookForExistingFile = ConfigFile == nullptr && !bForceReload && ConfigSystem != nullptr;
	if (bLookForExistingFile)
	{
		// look up a file that already exists and matches the name
		FConfigFile* FoundConfigFile = ConfigSystem->KnownFiles.GetMutableFile(*BaseIniName);
		if (FoundConfigFile == nullptr)
		{
			FoundConfigFile = ConfigSystem->FindConfigFile(*DestIniFilename);
			//// @todo: this is test to see if we can simplify this to FindConfigFileWithBaseName always (if it never fires, we can)
			//check(FoundConfigFile == nullptr || FoundConfigFile == ConfigSystem->FindConfigFileWithBaseName(*BaseIniName))
		}

		if (FoundConfigFile != nullptr && FoundConfigFile->Num() > 0)
		{
			ConfigFile = FoundConfigFile;
			bPerformLoad = false;
			return true;
		}
	}

	// setup ConfigFile to read into if one isn't already set
	if (ConfigFile == nullptr)
	{
		// first look for a KnownFile
		ConfigFile = ConfigSystem->KnownFiles.GetMutableFile(*BaseIniName);
		if (ConfigFile == nullptr)
		{
			check(!DestIniFilename.IsEmpty());

			ConfigFile = &ConfigSystem->Add(DestIniFilename, FConfigFile());
		}
	}

	bPerformLoad = true;
	return true;
}

/**
 * This will completely load a single .ini file into the passed in FConfigFile.
 *
 * @param FilenameToLoad - this is the path to the file to
 * @param ConfigFile - This is the FConfigFile which will have the contents of the .ini loaded into
 *
 **/
static void LoadAnIniFile(const FString& FilenameToLoad, FConfigFile& ConfigFile)
{
	if (!IsUsingLocalIniFile(*FilenameToLoad, nullptr) || DoesConfigFileExistWrapper(*FilenameToLoad))
	{
		ProcessIniContents(*FilenameToLoad, *FilenameToLoad, &ConfigFile, false, false);
	}
	else
	{
		//UE_LOG(LogConfig, Warning, TEXT( "LoadAnIniFile was unable to find FilenameToLoad: %s "), *FilenameToLoad);
	}
}

bool FConfigContext::PerformLoad()
{
	LLM_SCOPE(ELLMTag::ConfigSystem);
	static const FName ConfigContextClassName = TEXT("ConfigContext");

	// if bIsBaseIniName is false, that means the .ini is a ready-to-go .ini file, and just needs to be loaded into the FConfigFile
	if (!bIsHierarchicalConfig)
	{
		// if the ini name passed in already is a full path, just use it
		if (BaseIniName.EndsWith(TEXT(".ini")))
		{
			DestIniFilename = BaseIniName;
			BaseIniName = FPaths::GetBaseFilename(BaseIniName);
		}
		else
		{
			// generate path to the .ini file (not a Default ini, IniName is the complete name of the file, without path)
			DestIniFilename = FString::Printf(TEXT("%s/%s.ini"), *ProjectConfigDir, *BaseIniName);
		}

		const FName BaseName = FName(*BaseIniName);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(BaseName, ELLMTagSet::Assets);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(ConfigContextClassName, ELLMTagSet::AssetClasses);
		UE_TRACE_METADATA_SCOPE_ASSET_FNAME(BaseName, ConfigContextClassName, BaseName);

		// load the .ini file straight up
		LoadAnIniFile(*DestIniFilename, *ConfigFile);

		ConfigFile->Name = BaseName;
		ConfigFile->PlatformName.Reset();
		ConfigFile->bHasPlatformName = false;
	}
	else
	{
		const FName BaseName = FName(*BaseIniName);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(BaseName, ELLMTagSet::Assets);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH_FNAME(ConfigContextClassName, ELLMTagSet::AssetClasses);
		UE_TRACE_METADATA_SCOPE_ASSET_FNAME(BaseName, ConfigContextClassName, BaseName);
#if DISABLE_GENERATED_INI_WHEN_COOKED
		if (BaseIniName != TEXT("GameUserSettings"))
		{
			// If we asked to disable ini when cooked, disable all ini files except! GameUserSettings, which stores user preferences
			bAllowGeneratedIniWhenCooked = false;
			if (FPlatformProperties::RequiresCookedData())
			{
				ConfigFile->NoSave = true;
			}
		}
		else
		{
			bAllowGeneratedIniWhenCooked = true;
		}
#endif

		// generate the whole standard ini hierarchy
		AddStaticLayersToHierarchy();

		// clear previous source config file and reset
		delete ConfigFile->SourceConfigFile;
		ConfigFile->SourceConfigFile = new FConfigFile();

		// now generate and make sure it's up to date (using IniName as a Base for an ini filename)
		// @todo This bNeedsWrite afaict is always true even if it loaded a completely valid generated/final .ini, and the write below will
		// just write out the exact same thing it read in!
		bool bNeedsWrite = GenerateDestIniFile();

		ConfigFile->Name = BaseName;
		ConfigFile->PlatformName = Platform;
		ConfigFile->bHasPlatformName = true;

		// check if the config file wants to save all sections
		bool bLocalSaveAllSections = false;
		// Do not report the read of SectionsToSave. Some ConfigFiles are reallocated without it, and reporting
		// logs that the section disappeared. But this log is spurious since if the only reason it was read was
		// for the internal save before the FConfigFile is made publicly available.
		const FConfigSection* SectionsToSaveSection = ConfigFile->FindSection(SectionsToSaveString);
		if (SectionsToSaveSection)
		{
			const FConfigValue* Value = SectionsToSaveSection->Find(SaveAllSectionsKey);
			if (Value)
			{
				const FString& ValueStr = UE::ConfigCacheIni::Private::FAccessor::GetValueForWriting(*Value);
				bLocalSaveAllSections = FCString::ToBool(*ValueStr);
			}
		}

		// we can always save all sections of a User config file, Editor* (not Editor.ini tho, that is already handled in the normal method)
		bool bIsUserFile = BaseIniName.Contains(TEXT("User"));
		bool bIsEditorSettingsFile = BaseIniName.Contains(TEXT("Editor")) && BaseIniName != TEXT("Editor");

		ConfigFile->bCanSaveAllSections = bLocalSaveAllSections || bIsUserFile || bIsEditorSettingsFile;

		// don't write anything to disk in cooked builds - we will always use re-generated INI files anyway.
		// Note: Unfortunately bAllowGeneratedIniWhenCooked is often true even in shipping builds with cooked data
		// due to default parameters. We don't dare change this now.
		//
		// Check GIsInitialLoad since no INI changes that should be persisted could have occurred this early.
		// INI changes from code, environment variables, CLI parameters, etc should not be persisted. 
		if (!GIsInitialLoad && bWriteDestIni && (!FPlatformProperties::RequiresCookedData() || bAllowGeneratedIniWhenCooked)
			// We shouldn't save config files when in multiprocess mode,
			// otherwise we get file contention in XGE shader builds.
			&& !FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")))
		{
			// Check the config system for any changes made to defaults and propagate through to the saved.
			ConfigFile->ProcessSourceAndCheckAgainstBackup();

			if (bNeedsWrite)
			{
				// if it was dirtied during the above function, save it out now
				ConfigFile->Write(DestIniFilename);
			}
		}
	}

	// GenerateDestIniFile returns true if nothing is loaded, so check if we actually loaded something
	return ConfigFile->Num() > 0;
}


/**
 * Allows overriding the (default) .ini file for a given base (ie Engine, Game, etc)
 */
static void ConditionalOverrideIniFilename(FString& IniFilename, const TCHAR* BaseIniName)
{
#if !UE_BUILD_SHIPPING
	// Figure out what to look for on the commandline for an override. Disabled in shipping builds for security reasons
	const FString CommandLineSwitch = FString::Printf(TEXT("DEF%sINI="), BaseIniName);
	FParse::Value(FCommandLine::Get(), *CommandLineSwitch, IniFilename);
#endif
}

static FString PerformBasicReplacements(const FString& InString, const TCHAR* BaseIniName)
{
	FString OutString = InString.Replace(TEXT("{TYPE}"), BaseIniName, ESearchCase::CaseSensitive);
	OutString = OutString.Replace(TEXT("{USERSETTINGS}"), FPlatformProcess::UserSettingsDir(), ESearchCase::CaseSensitive);
	OutString = OutString.Replace(TEXT("{USER}"), FPlatformProcess::UserDir(), ESearchCase::CaseSensitive);
	OutString = OutString.Replace(TEXT("{CUSTOMCONFIG}"), *FConfigCacheIni::GetCustomConfigString(), ESearchCase::CaseSensitive);

	return OutString;
}


static FString PerformExpansionReplacements(const FConfigLayerExpansion& Expansion, const FString& InString)
{
	// if there's replacement to do, the output is just the output
	if (Expansion.Before1 == nullptr)
	{
		return InString;
	}

	// if nothing to replace, then skip it entirely
	if (!InString.Contains(Expansion.Before1) && (Expansion.Before2 == nullptr || !InString.Contains(Expansion.Before2)))
	{
		return TEXT("");
	}

	// replace the directory bits
	FString OutString = InString.Replace(Expansion.Before1, Expansion.After1, ESearchCase::CaseSensitive);
	if (Expansion.Before2 != nullptr)
	{
		OutString = OutString.Replace(Expansion.Before2, Expansion.After2, ESearchCase::CaseSensitive);
	}
	return OutString;
}

FString FConfigContext::PerformFinalExpansions(const FString& InString, const FString& InPlatform)
{
	FString OutString = InString.Replace(TEXT("{ENGINE}"), *EngineRootDir);
	OutString = OutString.Replace(TEXT("{PROJECT}"), *ProjectRootDir);
	OutString = OutString.Replace(TEXT("{RESTRICTEDPROJECT_NFL}"), *ProjectNotForLicenseesDir);
	OutString = OutString.Replace(TEXT("{RESTRICTEDPROJECT_NR}"), *ProjectNoRedistDir);

	if (FPaths::IsUnderDirectory(ProjectRootDir, ProjectNotForLicenseesDir))
	{
		FString RelativeDir = ProjectRootDir;
		FPaths::MakePathRelativeTo(RelativeDir, *(ProjectNotForLicenseesDir + TEXT("/")));

		OutString = OutString.Replace(TEXT("{OPT_SUBDIR}"), *(RelativeDir + TEXT("/")));
	}
	else if (FPaths::IsUnderDirectory(ProjectRootDir, ProjectNoRedistDir))
	{
		FString RelativeDir = ProjectRootDir;
		FPaths::MakePathRelativeTo(RelativeDir, *(ProjectNoRedistDir + TEXT("/")));

		OutString = OutString.Replace(TEXT("{OPT_SUBDIR}"), *(RelativeDir + TEXT("/")));
	}
	else if (FPaths::IsUnderDirectory(ProjectRootDir, EngineRootDir))
	{
		FString RelativeDir = ProjectRootDir;
		FPaths::MakePathRelativeTo(RelativeDir, *(EngineRootDir + TEXT("/")));
		
		OutString = OutString.Replace(TEXT("{OPT_SUBDIR}"), *(RelativeDir + TEXT("/")));
	}
	else
	{
		OutString = OutString.Replace(TEXT("{OPT_SUBDIR}"), TEXT(""));
	}
	
	if (InPlatform.Len() > 0)
	{
		OutString = OutString.Replace(TEXT("{EXTENGINE}"), *GetPerPlatformDirs(InPlatform).PlatformExtensionEngineDir);
		OutString = OutString.Replace(TEXT("{EXTPROJECT}"), *GetPerPlatformDirs(InPlatform).PlatformExtensionProjectDir);
		OutString = OutString.Replace(TEXT("{PLATFORM}"), *InPlatform);
	}

	if (bIsForPlugin)
	{
		OutString = OutString.Replace(TEXT("{PLUGIN}"), *PluginRootDir);
		OutString = OutString.Replace(TEXT("{EXTPLUGIN}"), *GetPerPlatformDirs(InPlatform).PlatformExtensionPluginDir);
	}

	return OutString;
}



void FConfigContext::AddStaticLayersToHierarchy(TArray<FString>* GatheredLayerFilenames, bool bIsLogging)
{
	// remember where this file was loaded from
	ConfigFile->SourceEngineConfigDir = EngineConfigDir;
	ConfigFile->SourceProjectConfigDir = ProjectConfigDir;

	// string that can have a reference to it, lower down
	const FString DedicatedServerString = IsRunningDedicatedServer() ? TEXT("DedicatedServer") : TEXT("");

	// cache some platform extension information that can be used inside the loops
	const bool bHasCustomConfig = !FConfigCacheIni::GetCustomConfigString().IsEmpty();


	// figure out what layers and expansions we will want
	EConfigExpansionFlags ExpansionMode = EConfigExpansionFlags::ForUncooked;
	FConfigLayer* Layers = GConfigLayers;
	int32 NumLayers = UE_ARRAY_COUNT(GConfigLayers);
	if (FPlatformProperties::RequiresCookedData())
	{
		ExpansionMode = EConfigExpansionFlags::ForCooked;
	}
	if (bIsForPlugin)
	{
		// this has priority over cooked/uncooked
		ExpansionMode = EConfigExpansionFlags::ForPlugin;
		Layers = GPluginLayers;
		NumLayers = UE_ARRAY_COUNT(GPluginLayers);
	}
	// let the context override the layers if needed
	if (OverrideLayers.Num() > 0)
	{
		Layers = OverrideLayers.GetData();
		NumLayers = OverrideLayers.Num();
	}

	// go over all the config layers
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; LayerIndex++)
	{
		const FConfigLayer& Layer = Layers[LayerIndex];

		// skip optional layers
		if (EnumHasAnyFlags(Layer.Flag, EConfigLayerFlags::RequiresCustomConfig) && !bHasCustomConfig)
		{
			continue;
		}

		// start replacing basic variables
		FString LayerPath = PerformBasicReplacements(Layer.Path, *BaseIniName);
		bool bHasPlatformTag = LayerPath.Contains(TEXT("{PLATFORM}"));

		// expand if it it has {ED} or {EF} expansion tags
		if (!EnumHasAnyFlags(Layer.Flag, EConfigLayerFlags::NoExpand))
		{
			// we assume none of the more special tags in expanded ones
			checkfSlow(FCString::Strstr(Layer.Path, TEXT("{USERSETTINGS}")) == nullptr && FCString::Strstr(Layer.Path, TEXT("{USER}")) == nullptr, TEXT("Expanded config %s shouldn't have a {USER*} tags in it"), Layer.Path);

			// loop over all the possible expansions
			for (int32 ExpansionIndex = 0; ExpansionIndex < UE_ARRAY_COUNT(GConfigExpansions); ExpansionIndex++)
			{
				// does this expansion match our current mode?
				if ((GConfigExpansions[ExpansionIndex].Flags & ExpansionMode) == EConfigExpansionFlags::None)
				{
					continue;
				}

				FString ExpandedPath = PerformExpansionReplacements(GConfigExpansions[ExpansionIndex], LayerPath);

				// if we didn't replace anything, skip it
				if (ExpandedPath.Len() == 0)
				{
					continue;
				}

				// allow for override, only on BASE EXPANSION!
				if (EnumHasAnyFlags(Layer.Flag, EConfigLayerFlags::AllowCommandLineOverride) && ExpansionIndex == 0)
				{
					checkfSlow(!bHasPlatformTag, TEXT("EConfigLayerFlags::AllowCommandLineOverride config %s shouldn't have a PLATFORM in it"), Layer.Path);

					ConditionalOverrideIniFilename(ExpandedPath, *BaseIniName);
				}

				const FDataDrivenPlatformInfo& Info = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(Platform);

				// go over parents, and then this platform, unless there's no platform tag, then we simply want to run through the loop one time to add it to the
				int32 NumPlatforms = bHasPlatformTag ? Info.IniParentChain.Num() + 1 : 1;
				int32 CurrentPlatformIndex = NumPlatforms - 1;
				int32 DedicatedServerIndex = -1;

				// make DedicatedServer another platform
				if (bHasPlatformTag && IsRunningDedicatedServer())
				{
					NumPlatforms++;
					DedicatedServerIndex = CurrentPlatformIndex + 1;
				}

				for (int PlatformIndex = 0; PlatformIndex < NumPlatforms; PlatformIndex++)
				{
					const FString CurrentPlatform =
						(PlatformIndex == DedicatedServerIndex) ? DedicatedServerString :
						(PlatformIndex == CurrentPlatformIndex) ? Platform :
						Info.IniParentChain[PlatformIndex];

					FString PlatformPath = PerformFinalExpansions(ExpandedPath, CurrentPlatform);

					// @todo restricted - ideally, we would move DedicatedServer files into a directory, like platforms are, but for short term compat,
					// convert the path back to the original (DedicatedServer/DedicatedServerEngine.ini -> DedicatedServerEngine.ini)
					if (PlatformIndex == DedicatedServerIndex)
					{
						PlatformPath.ReplaceInline(TEXT("Config/DedicatedServer/"), TEXT("Config/"));
					}

					// if we match the StartSkippingAtFilename, we are done adding to the hierarchy, so just return
					if (PlatformPath == StartSkippingAtFilename)
					{
						return;
					}
					
					if (PlatformPath.StartsWith(TEXT("<skip>")))
					{
						continue;
					}

					// add this to the list!
					if (GatheredLayerFilenames != nullptr)
					{
						if (bIsLogging)
						{
							GatheredLayerFilenames->Add(FString::Printf(TEXT("%s[Exp-%d]: %s"), Layer.EditorName, ExpansionIndex, *PlatformPath));
						}
						else
						{
							GatheredLayerFilenames->Add(PlatformPath);
						}
					}
					else
					{
						ConfigFile->SourceIniHierarchy.AddStaticLayer(PlatformPath, LayerIndex, ExpansionIndex, PlatformIndex);
					}
				}
			}
		}
		// if no expansion, just process the special tags (assume no PLATFORM tags)
		else
		{
			checkfSlow(!bHasPlatformTag, TEXT("Non-expanded config %s shouldn't have a PLATFORM in it"), Layer.Path);
			checkfSlow(!EnumHasAnyFlags(Layer.Flag, EConfigLayerFlags::AllowCommandLineOverride), TEXT("Non-expanded config can't have a EConfigLayerFlags::AllowCommandLineOverride"));

			FString FinalPath = PerformFinalExpansions(LayerPath, TEXT(""));

			// if we match the StartSkippingAtFilename, we are done adding to the hierarchy, so just return
			if (FinalPath == StartSkippingAtFilename)
			{
				return;
			}

			// add with no expansion
			if (GatheredLayerFilenames != nullptr)
			{
				if (bIsLogging)
				{
					GatheredLayerFilenames->Add(FString::Printf(TEXT("%s: %s"), Layer.EditorName, *FinalPath));
				}
				else
				{
					GatheredLayerFilenames->Add(FinalPath);
				}
			}
			else
			{
				ConfigFile->SourceIniHierarchy.AddStaticLayer(FinalPath, LayerIndex);
			}
		}
	}
}



/**
 * This will completely load .ini file hierarchy into the passed in FConfigFile. The passed in FConfigFile will then
 * have the data after combining all of those .ini
 *
 * @param FilenameToLoad - this is the path to the file to
 * @param ConfigFile - This is the FConfigFile which will have the contents of the .ini loaded into and Combined()
 *
 **/
static bool LoadIniFileHierarchy(const FConfigFileHierarchy& HierarchyToLoad, FConfigFile& ConfigFile, bool bUseCache, const TSet<FString>* IniCacheSet)
{
	static bool bDumpIniLoadInfo = FParse::Param(FCommandLine::Get(), TEXT("dumpiniloads"));
	
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadIniFileHierarchy);
	// Traverse ini list back to front, merging along the way.
	for (const TPair<int32, FString>& HierarchyIt : HierarchyToLoad)
	{
		bool bDoCombine = (HierarchyIt.Key != 0);
		const FString& IniFileName = HierarchyIt.Value;

		UE_CLOG(bDumpIniLoadInfo, LogConfig, Display, TEXT("Looking for file: %s"), *IniFileName);
		
		// skip non-existant files
		if (IsUsingLocalIniFile(*IniFileName, nullptr) && !DoesConfigFileExistWrapper(*IniFileName, IniCacheSet))
		{
			continue;
		}

		bool bDoEmptyConfig = false;
		//UE_LOG(LogConfig, Log,  TEXT( "Combining configFile: %s" ), *IniList(IniIndex) );
		ProcessIniContents(*IniFileName, *IniFileName, &ConfigFile, bDoEmptyConfig, bDoCombine);

		UE_CLOG(bDumpIniLoadInfo, LogConfig, Display, TEXT("   Found!"));
	}

	// Set this configs files source ini hierarchy to show where it was loaded from.
	ConfigFile.SourceIniHierarchy = HierarchyToLoad;

	return true;
}

/**
 * This will load up two .ini files and then determine if the destination one is outdated by comparing
 * version number in [CurrentIniVersion] section, Version key against the version in the Default*.ini
 * Outdatedness is determined by the following mechanic:
 *
 * Outdatedness also can be affected by commandline params which allow one to delete all .ini, have
 * automated build system etc.
 */
bool FConfigContext::GenerateDestIniFile()
{
	bool bResult = LoadIniFileHierarchy(ConfigFile->SourceIniHierarchy, *ConfigFile->SourceConfigFile, bUseHierarchyCache, IniCacheSet);
	if (bResult == false)
	{
		return false;
	}

	if (!FPlatformProperties::RequiresCookedData() || bAllowGeneratedIniWhenCooked)
	{
		if (bForceReload)
		{
			// if we are reloading from disk (probably to update GConfig's understanding after updating a Default ini file), we need to 
			// flush any pending updates, replace the in memory version with the hierarchy, and then read the flushed file back on  
			// this will make sure the in-memory version has what is on disk, so when Flush happens later, it does not write out an outdated value
			// note: we only want to copy the TMap base class slice of the FConfigFile, none of the specific members of the FConfigFile class itself
			ConfigFile->TMap<FString, FConfigSection>::operator=(*ConfigFile->SourceConfigFile);
		}

		LoadAnIniFile(*DestIniFilename, *ConfigFile);
	}

#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
	// process any commandline overrides
	FConfigFile::OverrideFromCommandline(ConfigFile, BaseIniName);
#endif

	bool bForceRegenerate = false;
	bool bShouldUpdate = FPlatformProperties::RequiresCookedData();

	// New versioning
	int32 SourceConfigVersionNum = -1;
	int32 CurrentIniVersion = -1;
	bool bVersionChanged = false;

	// Lambda for functionality that we can do in more than one place.
	auto RegenerateFileLambda = [](const FConfigFileHierarchy& InSourceIniHierarchy, FConfigFile& InDestConfigFile, const bool bInUseCache)
	{
		// Regenerate the file.
		bool bReturnValue = LoadIniFileHierarchy(InSourceIniHierarchy, InDestConfigFile, bInUseCache, nullptr);
		if (InDestConfigFile.SourceConfigFile)
		{
			delete InDestConfigFile.SourceConfigFile;
			InDestConfigFile.SourceConfigFile = nullptr;
		}
		InDestConfigFile.SourceConfigFile = new FConfigFile(InDestConfigFile);

		// mark it as dirty (caller may want to save)
		InDestConfigFile.Dirty = true;

		return bReturnValue;
	};

	// Don't try to load any generated files from disk in cooked builds. We will always use the re-generated INIs.
	if (!FPlatformProperties::RequiresCookedData() || bAllowGeneratedIniWhenCooked)
	{
		FString VersionString;
		if (ConfigFile->GetString(*CurrentIniVersionString, *VersionName.ToString(), VersionString))
		{
			CurrentIniVersion = FCString::Atoi(*VersionString);
		}

		// now compare to the source config file
		if (ConfigFile->SourceConfigFile->GetString(*CurrentIniVersionString, *VersionName.ToString(), VersionString))
		{
			SourceConfigVersionNum = FCString::Atoi(*VersionString);

			if (SourceConfigVersionNum > CurrentIniVersion)
			{
				UE_LOG(LogInit, Log, TEXT("%s version has been updated. It will be regenerated."), *FPaths::ConvertRelativePathToFull(DestIniFilename));
				bVersionChanged = true;
			}
			else if (SourceConfigVersionNum < CurrentIniVersion)
			{
				UE_LOG(LogInit, Warning, TEXT("%s version is later than the source. Since the versions are out of sync, nothing will be done."), *FPaths::ConvertRelativePathToFull(DestIniFilename));
			}
		}

		// Regenerate the ini file?
		if (FParse::Param(FCommandLine::Get(), TEXT("REGENERATEINIS")) == true)
		{
			bForceRegenerate = true;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("NOAUTOINIUPDATE")))
		{
			// Flag indicating whether the user has requested 'Yes/No To All'.
			static int32 GIniYesNoToAll = -1;
			// Make sure GIniYesNoToAll's 'uninitialized' value is kosher.
			static_assert(EAppReturnType::YesAll != -1, "EAppReturnType::YesAll must not be -1.");
			static_assert(EAppReturnType::NoAll != -1, "EAppReturnType::NoAll must not be -1.");

			// The file exists but is different.
			// Prompt the user if they haven't already responded with a 'Yes/No To All' answer.
			uint32 YesNoToAll;
			if (GIniYesNoToAll != EAppReturnType::YesAll && GIniYesNoToAll != EAppReturnType::NoAll)
			{
				YesNoToAll = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, FText::Format(NSLOCTEXT("Core", "IniFileOutOfDate", "Your ini ({0}) file is outdated. Do you want to automatically update it saving the previous version? Not doing so might cause crashes!"), FText::FromString(DestIniFilename)));
				// Record whether the user responded with a 'Yes/No To All' answer.
				if (YesNoToAll == EAppReturnType::YesAll || YesNoToAll == EAppReturnType::NoAll)
				{
					GIniYesNoToAll = YesNoToAll;
				}
			}
			else
			{
				// The user has already responded with a 'Yes/No To All' answer, so note it 
				// in the output arg so that calling code can operate on its value.
				YesNoToAll = GIniYesNoToAll;
			}
			// Regenerate the file if approved by the user.
			bShouldUpdate = (YesNoToAll == EAppReturnType::Yes) || (YesNoToAll == EAppReturnType::YesAll);
		}
		else
		{
			// If the version changes, we regenerate, so no need to do this.
			if (!bVersionChanged)
			{
				bShouldUpdate = true;
			}
		}
	}

	// Order is important, we want to let force regenerate happen before version change, in case we're trying to wipe everything.
	//	Version tries to save some info.
	if (ConfigFile->Num() == 0 && ConfigFile->SourceConfigFile->Num() == 0)
	{
		// If both are empty, don't save
		return false;
	}
	else if (bForceRegenerate)
	{
		bResult = RegenerateFileLambda(ConfigFile->SourceIniHierarchy, *ConfigFile, bUseHierarchyCache);
	}
	else if (bVersionChanged)
	{
		// Clear out everything but the preserved sections with the properties in that section, then update the version.
		//	The ini syntax is Preserve=Section=<section name, like /Scipt/FortniteGame.FortConsole>.
		//	Go through and save the preserved sections before we regenerate the file. We'll re-add those after.
		FConfigSection PreservedConfigSectionData;
		if (const FConfigSection* SourceConfigSectionIniVersion = ConfigFile->SourceConfigFile->FindSection(CurrentIniVersionString))
		{
			for (FConfigSectionMap::TConstIterator ItSourceConfigSectionIniVersion(*SourceConfigSectionIniVersion); ItSourceConfigSectionIniVersion; ++ItSourceConfigSectionIniVersion)
			{
				if (ItSourceConfigSectionIniVersion.Key() == PreserveName)
				{
					PreservedConfigSectionData.Add(ItSourceConfigSectionIniVersion.Key(), ItSourceConfigSectionIniVersion.Value());
				}
			}
		}

		FConfigFile PreservedConfigFileData;
		for (FConfigSectionMap::TConstIterator ItPreservedConfigSectionData(PreservedConfigSectionData); ItPreservedConfigSectionData; ++ItPreservedConfigSectionData)
		{
			FString SectionString = ItPreservedConfigSectionData.Value().GetSavedValue();
			if (const FConfigSection* FoundSection = ConfigFile->FindSection(SectionString))
			{
				for (FConfigSectionMap::TConstIterator ItFoundSection(*FoundSection); ItFoundSection; ++ItFoundSection)
				{
					if (FConfigSection* CreatedSection = PreservedConfigFileData.FindOrAddSectionInternal(SectionString))
					{
						CreatedSection->Add(ItFoundSection.Key(), ItFoundSection.Value());
					}
				}
			}
		}

		// Remove everything before we regenerate.
		ConfigFile->Empty();

		// Regnerate.
		bResult = RegenerateFileLambda(ConfigFile->SourceIniHierarchy, *ConfigFile, bUseHierarchyCache);

		// Add back the CurrentIniVersion section.
		if (FConfigSection* DestConfigSectionIniVersion = ConfigFile->FindOrAddSectionInternal(CurrentIniVersionString))
		{
			// Update the version. If it's already there then good but if not, we add it.
			DestConfigSectionIniVersion->FindOrAdd(VersionName, FConfigValue(FString::FromInt(SourceConfigVersionNum)));
		}

		// Add back any preserved sections.
		for (TMap<FString, FConfigSection>::TConstIterator ItPreservedConfigFileData(PreservedConfigFileData); ItPreservedConfigFileData; ++ItPreservedConfigFileData)
		{
			if (FConfigSection* DestConfigSectionPreserved = ConfigFile->FindOrAddSectionInternal(ItPreservedConfigFileData.Key()))
			{
				FConfigSection PreservedConfigFileSection = ItPreservedConfigFileData.Value();
				for (FConfigSectionMap::TConstIterator ItPreservedConfigFileSection(PreservedConfigFileSection); ItPreservedConfigFileSection; ++ItPreservedConfigFileSection)
				{
					DestConfigSectionPreserved->Add(ItPreservedConfigFileSection.Key(), ItPreservedConfigFileSection.Value());
				}
			}
		}
	}
	else if (bShouldUpdate)
	{
		// Merge the .ini files by copying over properties that exist in the default .ini but are
		// missing from the generated .ini
		// NOTE: Most of the time there won't be any properties to add here, since LoadAnIniFile will
		//		 combine properties in the Default .ini with those in the Project .ini
		ConfigFile->AddMissingProperties(*ConfigFile->SourceConfigFile);

		// mark it as dirty (caller may want to save)
		ConfigFile->Dirty = true;
	}

	if (!IsUsingLocalIniFile(*DestIniFilename, nullptr))
	{
		// Save off a copy of the local file prior to overwriting it with the contents of a remote file
		MakeLocalCopy(*DestIniFilename);
	}

	return bResult;
}






/*-----------------------------------------------------------------------------
	FConfigFileHierarchy
-----------------------------------------------------------------------------*/

constexpr int32 MaxPlatformIndex = 99;
constexpr int32 GetStaticKey(int32 LayerIndex, int32 ReplacementIndex, int32 PlatformIndex)
{
	return LayerIndex * 10000 + ReplacementIndex * 100 + PlatformIndex;
}

FConfigFileHierarchy::FConfigFileHierarchy()
	: KeyGen(GetStaticKey(UE_ARRAY_COUNT(GConfigLayers) - 1, UE_ARRAY_COUNT(GConfigExpansions) - 1, MaxPlatformIndex))
{
}


int32 FConfigFileHierarchy::GenerateDynamicKey()
{
	return ++KeyGen;
}

int32 FConfigFileHierarchy::AddStaticLayer(const FString& Filename, int32 LayerIndex, int32 ExpansionIndex /*= 0*/, int32 PlatformIndex /*= 0*/)
{
	int32 Key = GetStaticKey(LayerIndex, ExpansionIndex, PlatformIndex);
	Emplace(Key, Filename);
	return Key;
}

int32 FConfigFileHierarchy::AddDynamicLayer(const FString& Filename)
{
	int32 Key = GenerateDynamicKey();
	Emplace(Key, Filename);
	return Key;
}

void FConfigContext::EnsureRequiredGlobalPathsHaveBeenInitialized()
{
	PerformBasicReplacements(TEXT(""), TEXT("")); // requests user directories and FConfigCacheIni::GetCustomConfigString
}


void FConfigContext::VisualizeHierarchy(FOutputDevice& Ar, const TCHAR* IniName, const TCHAR* OverridePlatform, const TCHAR* OverrideProjectOrProgramDataDir, const TCHAR* OverridePluginDir, const TArray<FString>* ChildPluginBaseDirs)
{
	FConfigFile Test;
	FConfigContext Context(nullptr, true, OverridePlatform ? FString(OverridePlatform) : FString(), &Test);
	if (OverridePluginDir != nullptr)
	{
		Context.bIsForPlugin = true;
		Context.PluginRootDir = OverridePluginDir;
		if (ChildPluginBaseDirs != nullptr)
		{
			Context.ChildPluginBaseDirs = *ChildPluginBaseDirs;
		}
	}
	
	if (OverrideProjectOrProgramDataDir != nullptr)
	{
		Context.ProjectConfigDir = FPaths::Combine(OverrideProjectOrProgramDataDir, "Config/");
	}

	
	Context.VisualizeHierarchy(Ar, IniName);
}

void FConfigContext::VisualizeHierarchy(FOutputDevice& Ar, const TCHAR* IniName)
{
	Ar.Logf(TEXT("======================================================="));

	ResetBaseIni(IniName);
	CachePaths();
	bool _;
	PrepareForLoad(_);

	Ar.Logf(TEXT("Config hierarchy:"));
	if (ProjectRootDir.Contains(TEXT("/Programs/")))
	{
		Ar.Logf(TEXT("  Program Data Dir: %s"), *ProjectRootDir);
	}
	else
	{
		Ar.Logf(TEXT("  Project Dir: %s"), *ProjectRootDir);
	}
	Ar.Logf(TEXT("  Platform: %s"), *Platform);
	if (bIsForPlugin)
	{
		Ar.Logf(TEXT("  Plugin Root Dir: %s"), *PluginRootDir);
		for (FString& Child : ChildPluginBaseDirs)
		{
			Ar.Logf(TEXT("  Plugin Children Dir: %s"), *Child);
		}
	}
	
	TArray<FString> FileList;
	AddStaticLayersToHierarchy(&FileList, true);
	
	Ar.Logf(TEXT("  Files:"));
	for (const FString& File : FileList)
	{
		Ar.Logf(TEXT("    %s"), *File);
	}

	Ar.Logf(TEXT("======================================================="));
}
