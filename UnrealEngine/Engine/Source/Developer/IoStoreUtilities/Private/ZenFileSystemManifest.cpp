// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenFileSystemManifest.h"

#include "Algo/Sort.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/PathViews.h"
#include "Settings/ProjectPackagingSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogZenFileSystemManifest, Display, All);

const FZenFileSystemManifest::FManifestEntry FZenFileSystemManifest::InvalidEntry = FManifestEntry();

FZenFileSystemManifest::FZenFileSystemManifest(const ITargetPlatform& InTargetPlatform, FString InCookDirectory)
	: TargetPlatform(InTargetPlatform)
	, CookDirectory(MoveTemp(InCookDirectory))
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	ServerRoot = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*FPaths::RootDir());
	FPaths::NormalizeDirectoryName(ServerRoot);
}

void FZenFileSystemManifest::GetExtensionDirs(TArray<FString>& OutExtensionDirs, const TCHAR* BaseDir, const TCHAR* SubDir, const TArray<FString>& PlatformDirectoryNames)
{
	auto AddIfDirectoryExists = [&OutExtensionDirs](FString&& Dir)
	{
		if (FPaths::DirectoryExists(Dir))
		{
			OutExtensionDirs.Emplace(MoveTemp(Dir));
		}
	};
	
	AddIfDirectoryExists(FPaths::Combine(BaseDir, SubDir));

	FString PlatformExtensionBaseDir = FPaths::Combine(BaseDir, TEXT("Platforms"));
	for (const FString& PlatformDirectoryName : PlatformDirectoryNames)
	{
		AddIfDirectoryExists(FPaths::Combine(PlatformExtensionBaseDir, PlatformDirectoryName, SubDir));
	}

	FString RestrictedBaseDir = FPaths::Combine(BaseDir, TEXT("Restricted"));
	IFileManager::Get().IterateDirectory(*RestrictedBaseDir, [&OutExtensionDirs, SubDir, &PlatformDirectoryNames](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
		{
			if (bIsDirectory)
			{
				GetExtensionDirs(OutExtensionDirs, FilenameOrDirectory, SubDir, PlatformDirectoryNames);
			}
			return true;
		});
}

int32 FZenFileSystemManifest::Generate()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GenerateStorageServerFileSystemManifest);

	class FFileFilter
	{
	public:
		FFileFilter& ExcludeDirectory(const TCHAR* Name)
		{
			DirectoryExclusionFilter.Emplace(Name);
			return *this;
		}

		FFileFilter& ExcludeExtension(const TCHAR* Extension)
		{
			ExtensionExclusionFilter.Emplace(Extension);
			return *this;
		}

		FFileFilter& IncludeExtension(const TCHAR* Extension)
		{
			ExtensionInclusionFilter.Emplace(Extension);
			return *this;
		}

		bool FilterDirectory(FStringView Name) const
		{
			for (const FString& ExcludedDirectory : DirectoryExclusionFilter)
			{
				if (Name == ExcludedDirectory)
				{
					return false;
				}
			}
			return true;
		}

		bool FilterFile(FStringView Extension)
		{
			if (!ExtensionExclusionFilter.IsEmpty())
			{
				for (const FString& ExcludedExtension : ExtensionExclusionFilter)
				{
					if (Extension == ExcludedExtension)
					{
						return false;
					}
				}
			}
			if (!ExtensionInclusionFilter.IsEmpty())
			{
				for (const FString& IncludedExtension : ExtensionInclusionFilter)
				{
					if (Extension == IncludedExtension)
					{
						return true;
					}
				}
				return false;
			}
			return true;
		}

	private:
		TArray<FString> DirectoryExclusionFilter;
		TArray<FString> ExtensionExclusionFilter;
		TArray<FString> ExtensionInclusionFilter;
	};

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString RootDir = FPaths::RootDir();
	FString EngineDir = FPaths::EngineDir();
	FPaths::NormalizeDirectoryName(EngineDir);
	FString ProjectDir = FPaths::ProjectDir();
	FPaths::NormalizeDirectoryName(ProjectDir);

	FFileFilter BaseFilter = FFileFilter()
		.ExcludeDirectory(TEXT("Binaries"))
		.ExcludeDirectory(TEXT("Intermediate"))
		.ExcludeDirectory(TEXT("Saved"))
		.ExcludeDirectory(TEXT("Source"));
	auto AddFilesFromDirectory =
		[this, &PlatformFile, &RootDir, &BaseFilter]
		(const FString& ClientDirectory, const FString& LocalDirectory, bool bIncludeSubdirs, FFileFilter* AdditionalFilter = nullptr)
		{
			//TRACE_BOOKMARK(TEXT("AddFilesFromDirectory: %s"), *ClientDirectory);
			FString ServerRelativeDirectory = LocalDirectory;
			FPaths::MakePathRelativeTo(ServerRelativeDirectory, *RootDir);
			ServerRelativeDirectory = TEXT("/") + ServerRelativeDirectory;

			TArray<FString> DirectoriesToVisit;
			auto VisitorFunc =
				[this, &DirectoriesToVisit, &RootDir, &ClientDirectory, &LocalDirectory, &ServerRelativeDirectory, bIncludeSubdirs, &BaseFilter, AdditionalFilter]
				(const TCHAR* InnerFileNameOrDirectory, bool bIsDirectory)
				{
					if (bIsDirectory)
					{
						if (!bIncludeSubdirs)
						{
							return true;
						}
						FStringView DirectoryName = FPathViews::GetPathLeaf(InnerFileNameOrDirectory);
						if (!BaseFilter.FilterDirectory(DirectoryName))
						{
							return true;
						}
						if (AdditionalFilter && !AdditionalFilter->FilterDirectory(DirectoryName))
						{
							return true;
						}
						DirectoriesToVisit.Add(InnerFileNameOrDirectory);
						return true;
					}
					FStringView Extension = FPathViews::GetExtension(InnerFileNameOrDirectory);
					if (!BaseFilter.FilterFile(Extension))
					{
						return true;
					}
					if (AdditionalFilter && !AdditionalFilter->FilterFile(Extension))
					{
						return true;
					}
					//TRACE_CPUPROFILER_EVENT_SCOPE(AddManifestEntry);
					FStringView RelativePath = InnerFileNameOrDirectory;
					RelativePath.RightChopInline(LocalDirectory.Len() + 1);
					FString ClientPath = FPaths::Combine(ClientDirectory, RelativePath.GetData());
					const FIoChunkId FileChunkId = CreateExternalFileChunkId(ClientPath);
					AddManifestEntry(
						FileChunkId,
						FPaths::Combine(ServerRelativeDirectory, RelativePath.GetData()),
						MoveTemp(ClientPath));
					return true;
				};

			DirectoriesToVisit.Push(LocalDirectory);
			while (!DirectoriesToVisit.IsEmpty())
			{
				PlatformFile.IterateDirectory(*DirectoriesToVisit.Pop(EAllowShrinking::No), VisitorFunc);
			}
		};

	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();

	TArray<FString> PlatformDirectoryNames;
	const FDataDrivenPlatformInfo& PlatformInfo = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(TargetPlatform.IniPlatformName());
	PlatformDirectoryNames.Reserve(PlatformInfo.IniParentChain.Num() + PlatformInfo.AdditionalRestrictedFolders.Num() + 1);
	PlatformDirectoryNames.Add(TargetPlatform.IniPlatformName());
	for (const FString& PlatformName : PlatformInfo.AdditionalRestrictedFolders)
	{
		PlatformDirectoryNames.AddUnique(PlatformName);
	}
	for (const FString& PlatformName : PlatformInfo.IniParentChain)
	{
		PlatformDirectoryNames.AddUnique(PlatformName);
	}
	auto AddFilesFromExtensionDirectories = [&AddFilesFromDirectory, &EngineDir, &ProjectDir, &PlatformDirectoryNames](const TCHAR* ExtensionSubDir, FFileFilter* AdditionalFilter = nullptr)
		{
			TArray<FString> ExtensionDirs;
			GetExtensionDirs(ExtensionDirs, *EngineDir, ExtensionSubDir, PlatformDirectoryNames);
			for (const FString& Dir : ExtensionDirs)
			{
				//AddFilesFromDirectory(Dir.Replace(*EngineDir, TEXT("/{engine}")), Dir, true, AdditionalFilter);
				AddFilesFromDirectory(Dir.Replace(*EngineDir, TEXT("/{engine}")), Dir, true, AdditionalFilter);
			}
			ExtensionDirs.Reset();
			GetExtensionDirs(ExtensionDirs, *ProjectDir, ExtensionSubDir, PlatformDirectoryNames);
			for (const FString& Dir : ExtensionDirs)
			{
				AddFilesFromDirectory(Dir.Replace(*ProjectDir, TEXT("/{project}")), Dir, true, AdditionalFilter);
			}
		};

	const int32 PreviousEntryCount = NumEntries();

	FFileFilter CookedFilter = FFileFilter()
		.ExcludeDirectory(TEXT("Metadata"))
		.ExcludeExtension(TEXT("json"))
		.ExcludeExtension(TEXT("uasset"))
		.ExcludeExtension(TEXT("ubulk"))
		.ExcludeExtension(TEXT("uexp"))
		.ExcludeExtension(TEXT("umap"))
		.ExcludeExtension(TEXT("uregs"));
	AddFilesFromDirectory(TEXT("/{engine}"), FPaths::Combine(CookDirectory, TEXT("Engine")), true, &CookedFilter);
	AddFilesFromDirectory(TEXT("/{project}"), FPaths::Combine(CookDirectory, FApp::GetProjectName()), true, &CookedFilter);
	
	AddFilesFromDirectory(TEXT("/{project}"), ProjectDir, false);
	
	FFileFilter ConfigFilter = FFileFilter()
		.IncludeExtension(TEXT("ini"));
	AddFilesFromExtensionDirectories(TEXT("Config"), &ConfigFilter);

	auto AddFromPluginPath = [this](const FString& EngineDir, const FString& ClientDirectory, const FString& SourcePath, const TCHAR* Path) {
		//TRACE_CPUPROFILER_EVENT_SCOPE(AddManifestEntry);

		FString ServerRelativePath = Path;
		FPaths::MakePathRelativeTo(ServerRelativePath, *EngineDir);
		ServerRelativePath = TEXT("/") + ServerRelativePath;

		FString ClientPath = Path;
		FPaths::MakePathRelativeTo(ClientPath, *(SourcePath / TEXT("")));
		ClientPath = ClientDirectory / ClientPath;

		const FIoChunkId FileChunkId = CreateExternalFileChunkId(ClientPath);
		AddManifestEntry(
			FileChunkId,
			ServerRelativePath,
			MoveTemp(ClientPath));
	};

	auto AddFromPluginDir = [this, &PlatformFile, &BaseFilter, &AddFromPluginPath](const FString& EngineDir, const FString& ClientDirectory, const FString& SourcePath, const FString& DirectoryPath, bool bIncludeSubdirs, FFileFilter* AdditionalFilter = nullptr) {
		TArray<FString> DirectoriesToVisit;
		auto VisitorFunc =
			[this, &AddFromPluginPath, &EngineDir, &ClientDirectory, &SourcePath, &DirectoriesToVisit, &BaseFilter, bIncludeSubdirs, AdditionalFilter]//, &RootDir, &ClientDirectory, &LocalDirectory, &ServerRelativeDirectory, &BaseFilter, AdditionalFilter]
			(const TCHAR* InnerFileNameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				if (!bIncludeSubdirs)
				{
					return true;
				}
				FStringView DirectoryName = FPathViews::GetPathLeaf(InnerFileNameOrDirectory);
				if (!BaseFilter.FilterDirectory(DirectoryName))
				{
					return true;
				}
				if (AdditionalFilter && !AdditionalFilter->FilterDirectory(DirectoryName))
				{
					return true;
				}
				DirectoriesToVisit.Add(InnerFileNameOrDirectory);
				return true;
			}
			FStringView Extension = FPathViews::GetExtension(InnerFileNameOrDirectory);
			if (!BaseFilter.FilterFile(Extension))
			{
				return true;
			}
			if (AdditionalFilter && !AdditionalFilter->FilterFile(Extension))
			{
				return true;
			}
			AddFromPluginPath(EngineDir, ClientDirectory, SourcePath, InnerFileNameOrDirectory);
			return true;
		};

		DirectoriesToVisit.Push(DirectoryPath);
		while (!DirectoriesToVisit.IsEmpty())
		{
			PlatformFile.IterateDirectory(*DirectoriesToVisit.Pop(EAllowShrinking::No), VisitorFunc);
		}
	};

	FFileFilter LocalizationFilter = FFileFilter()
		.IncludeExtension(TEXT("locmeta"))
		.IncludeExtension(TEXT("locres"));

	const bool FilterDisabledPlugins = true;
	FString PluginTargetPlatformString = PlatformInfo.UBTPlatformString;
	IPluginManager& PluginManager = IPluginManager::Get();
	TArray<TSharedRef<IPlugin>> DiscoveredPlugins = PluginManager.GetDiscoveredPlugins();
	for (TSharedRef<IPlugin>& Plugin : DiscoveredPlugins)
	{
		FString ProjectName = Plugin->GetName();
		if (FilterDisabledPlugins && !Plugin->IsEnabled())
		{
			UE_LOG(LogZenFileSystemManifest, Verbose, TEXT("Plugin '%s' disabled, skipping"), *ProjectName);
			continue;
		}
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
		if (!Descriptor.SupportsTargetPlatform(PluginTargetPlatformString))
		{
			UE_LOG(LogZenFileSystemManifest, Verbose, TEXT("Plugin '%s' not supported on platform '%s', skipping"), *ProjectName, *TargetPlatform.PlatformName());
			for (const auto& SupportedTargetPlatform : Descriptor.SupportedTargetPlatforms)
			{
				UE_LOG(LogZenFileSystemManifest, Verbose, TEXT("       '%s' supports platform '%s'"), *ProjectName, *SupportedTargetPlatform);
			}
			continue;
		}
		FString BaseDir = Plugin->GetBaseDir();
		FString ProjectFile = Plugin->GetDescriptorFileName();
		FString ContentDir = Plugin->GetContentDir();
		FString LocalizationDir = ContentDir / TEXT("Localization");
		FString ConfigDir = BaseDir / TEXT("Config");
		UE_LOG(LogZenFileSystemManifest, Verbose, TEXT("Plugin '%s': BaseDir: '%s'"), *ProjectName, *BaseDir);

		FString ClientDirectory;
		FString SourcePath;
		switch (Plugin->GetLoadedFrom())
		{
		case EPluginLoadedFrom::Engine:
			ClientDirectory = TEXT("/{engine}");
			SourcePath = EngineDir;
			break;
		case EPluginLoadedFrom::Project:
			ClientDirectory = TEXT("/{project}");
			SourcePath = ProjectDir;
			break;
		}
		AddFromPluginPath(EngineDir, ClientDirectory, SourcePath, *ProjectFile);
		AddFromPluginDir(EngineDir, ClientDirectory, SourcePath, LocalizationDir, true, &LocalizationFilter);
		AddFromPluginDir(EngineDir, ClientDirectory, SourcePath, ConfigDir, true, &ConfigFilter);
	}

	FString InternationalizationPresetAsString = UEnum::GetValueAsString(PackagingSettings->InternationalizationPreset);
	const TCHAR* InternationalizationPresetPath = FCString::Strrchr(*InternationalizationPresetAsString, ':');
	if (InternationalizationPresetPath)
	{
		++InternationalizationPresetPath;
	}
	else
	{
		UE_LOG(LogZenFileSystemManifest, Warning, TEXT("Failed reading internationalization preset setting, defaulting to English"));
		InternationalizationPresetPath = TEXT("English");
	}
	const TCHAR* ICUDataVersion = TEXT("icudt64l"); // TODO: Could this go into datadriven platform info? But it's basically always this.
	AddFilesFromDirectory(*FPaths::Combine(TEXT("/{engine}"), TEXT("Content"), TEXT("Internationalization"), ICUDataVersion), FPaths::Combine(EngineDir, TEXT("Content"), TEXT("Internationalization"), InternationalizationPresetPath, ICUDataVersion), true);
	
	AddFilesFromExtensionDirectories(TEXT("Content/Localization"), &LocalizationFilter);

	FFileFilter ContentFilter = FFileFilter()
		.ExcludeExtension(TEXT("uasset"))
		.ExcludeExtension(TEXT("ubulk"))
		.ExcludeExtension(TEXT("uexp"))
		.ExcludeExtension(TEXT("umap"));
	AddFilesFromDirectory(TEXT("/{engine}/Content/Slate"), FPaths::Combine(EngineDir, TEXT("Content"), TEXT("Slate")), true, &ContentFilter);
	AddFilesFromDirectory(TEXT("/{project}/Content/Slate"), FPaths::Combine(ProjectDir, TEXT("Content"), TEXT("Slate")), true, &ContentFilter);
	AddFilesFromDirectory(TEXT("/{engine}/Content/Movies"), FPaths::Combine(EngineDir, TEXT("Content"), TEXT("Movies")), true, &ContentFilter);
	AddFilesFromDirectory(TEXT("/{project}/Content/Movies"), FPaths::Combine(ProjectDir, TEXT("Content"), TEXT("Movies")), true, &ContentFilter);
	
	FFileFilter OoodleDictionaryFilter = FFileFilter()
		.IncludeExtension(TEXT("udic"));
	AddFilesFromDirectory(TEXT("/{project}/Content/Oodle"), FPaths::Combine(ProjectDir, TEXT("Content"), TEXT("Oodle")), false, &OoodleDictionaryFilter);

	FFileFilter ShaderCacheFilter = FFileFilter()
		.IncludeExtension(TEXT("ushadercache"))
		.IncludeExtension(TEXT("upipelinecache"));
	AddFilesFromDirectory(TEXT("/{project}/Content"), FPaths::Combine(ProjectDir, TEXT("Content")), false, &ShaderCacheFilter);
	AddFilesFromDirectory(FPaths::Combine(TEXT("/{project}"), TEXT("Content"), TEXT("PipelineCaches"), TargetPlatform.IniPlatformName()), FPaths::Combine(ProjectDir, TEXT("Content"), TEXT("PipelineCaches"), TargetPlatform.IniPlatformName()), false, &ShaderCacheFilter);

	auto AddAdditionalFilesFromConfig = [&AddFilesFromDirectory , &ContentFilter, &ProjectDir, &EngineDir](const FString& RelativeDirToStage)
	{
		FString AbsoluteDirToStage = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Content"), RelativeDirToStage));
		FPaths::NormalizeDirectoryName(AbsoluteDirToStage);
		FString AbsoluteEngineDir = FPaths::ConvertRelativePathToFull(EngineDir);
		FString AbsoluteProjectDir = FPaths::ConvertRelativePathToFull(ProjectDir);
		FStringView RelativeToKnownRootView;
		if (FPathViews::TryMakeChildPathRelativeTo(AbsoluteDirToStage, AbsoluteProjectDir, RelativeToKnownRootView))
		{
			AddFilesFromDirectory(FPaths::Combine(TEXT("/{project}"), RelativeToKnownRootView.GetData()), FPaths::Combine(ProjectDir, RelativeToKnownRootView.GetData()), true, &ContentFilter);
		}
		else if (FPathViews::TryMakeChildPathRelativeTo(AbsoluteDirToStage, AbsoluteEngineDir, RelativeToKnownRootView))
		{
			AddFilesFromDirectory(FPaths::Combine(TEXT("/{engine}"), RelativeToKnownRootView.GetData()), FPaths::Combine(EngineDir, RelativeToKnownRootView.GetData()), true, &ContentFilter);
		}
		else
		{
			UE_LOG(LogZenFileSystemManifest, Warning, TEXT("Ignoring additional folder to stage that is not relative to the engine or project directory: %s"), *RelativeDirToStage);
		}
	};
	for (const FDirectoryPath& AdditionalFolderToStage : PackagingSettings->DirectoriesToAlwaysStageAsUFS)
	{
		AddAdditionalFilesFromConfig(AdditionalFolderToStage.Path);
	}
	for (const FDirectoryPath& AdditionalFolderToStage : PackagingSettings->DirectoriesToAlwaysStageAsUFSServer)
	{
		AddAdditionalFilesFromConfig(AdditionalFolderToStage.Path);
	}

	const int32 CurrentEntryCount = NumEntries();
	
	return CurrentEntryCount - PreviousEntryCount;
}

const FZenFileSystemManifest::FManifestEntry& FZenFileSystemManifest::CreateManifestEntry(const FString& Filename)
{
	FString CookedEngineDirectory = FPaths::Combine(CookDirectory, TEXT("Engine"));

	auto AddEntry = [this, &Filename](const FString& ClientDirectory, const FString& LocalDirectory) -> const FManifestEntry&
	{
		FStringView RelativePath = Filename;
		RelativePath.RightChopInline(LocalDirectory.Len() + 1);

		FString ServerRelativeDirectory = LocalDirectory;
		FPaths::MakePathRelativeTo(ServerRelativeDirectory, *FPaths::RootDir());
		ServerRelativeDirectory = TEXT("/") + ServerRelativeDirectory;

		FString ServerPath = FPaths::Combine(ServerRelativeDirectory, RelativePath.GetData());
		FString ClientPath = FPaths::Combine(ClientDirectory, RelativePath.GetData());
		const FIoChunkId FileChunkId = CreateExternalFileChunkId(ClientPath);

		return AddManifestEntry(FileChunkId, MoveTemp(ServerPath), MoveTemp(ClientPath));
	};

	if (Filename.StartsWith(CookedEngineDirectory))
	{
		return AddEntry(TEXT("/{engine}"), CookedEngineDirectory);
	}

	FString CookedProjectDirectory = FPaths::Combine(CookDirectory, FApp::GetProjectName());
	if (Filename.StartsWith(CookedProjectDirectory))
	{
		return AddEntry(TEXT("/{project}"), CookedProjectDirectory);
	}

	return InvalidEntry;
}

const FZenFileSystemManifest::FManifestEntry& FZenFileSystemManifest::AddManifestEntry(const FIoChunkId& FileChunkId, FString ServerPath, FString ClientPath)
{
	check(ServerPath.Len() > 0 && ClientPath.Len() > 0);

	ServerPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	ClientPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	// The server path is always relative to project root
	if (ServerPath[0] == '/')
	{
		ServerPath.RightChopInline(1);
	}

	int32& EntryIndex = ServerPathToEntry.FindOrAdd(ServerPath, INDEX_NONE);

	if (EntryIndex != INDEX_NONE)
	{
		return Entries[EntryIndex];
	}

	EntryIndex = Entries.Num();

	FManifestEntry Entry;
	Entry.ServerPath = MoveTemp(ServerPath);
	Entry.ClientPath = MoveTemp(ClientPath);
	Entry.FileChunkId = FileChunkId;

	Entries.Add(MoveTemp(Entry));

	return Entries[EntryIndex];
}

bool FZenFileSystemManifest::Save(const TCHAR* Filename)
{
	check(Filename);

	TArray<FString> CsvLines;
	CsvLines.Add(FString::Printf(TEXT(";ServerRoot=%s, Platform=%s, CookDirectory=%s"), *ServerRoot, *TargetPlatform.PlatformName(), *CookDirectory));
	CsvLines.Add(TEXT("FileId, ServerPath, ClientPath"));

	TStringBuilder<2048> Sb;
	TArray<const FManifestEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (const FManifestEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
	}
	Algo::Sort(SortedEntries, [](const FManifestEntry* A, const FManifestEntry* B) { return A->ClientPath < B->ClientPath; });

	for (const FManifestEntry* Entry : SortedEntries)
	{
		Sb.Reset();
		Sb << Entry->FileChunkId << TEXT(", ") << Entry->ServerPath << TEXT(", ") << *Entry->ClientPath;
		CsvLines.Add(Sb.ToString());
	}

	return FFileHelper::SaveStringArrayToFile(CsvLines, Filename);
}
