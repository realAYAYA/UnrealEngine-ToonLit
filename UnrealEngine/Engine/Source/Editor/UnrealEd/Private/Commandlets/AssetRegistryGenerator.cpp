// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/AssetRegistryGenerator.h"

#include "Algo/Sort.h"
#include "Algo/StableSort.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/CookTagList.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "Cooker/CookPackageData.h"
#include "Cooker/CookWorkerClient.h"
#include "Commandlets/ChunkDependencyInfo.h"
#include "Commandlets/IChunkDataGenerator.h"
#include "Engine/AssetManager.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameDelegates.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "ICollectionManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Misc/FileHelper.h"
#include "Misc/PathViews.h"
#include "PakFileUtilities.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonWriter.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Stats/StatsMisc.h"
#include "String/ParseTokens.h"
#include "Templates/AreTypesEqual.h"
#include "Templates/UniquePtr.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "HAL/ThreadHeartBeat.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAssetRegistryGenerator, Log, All);

#define LOCTEXT_NAMESPACE "AssetRegistryGenerator"

//////////////////////////////////////////////////////////////////////////
// Static functions
FName GetPackageNameFromDependencyPackageName(const FName RawPackageFName)
{
	FName PackageFName = RawPackageFName;
	if ((FPackageName::IsValidLongPackageName(RawPackageFName.ToString()) == false) &&
		(FPackageName::IsScriptPackage(RawPackageFName.ToString()) == false))
	{
		FText OutReason;
		if (!FPackageName::IsValidLongPackageName(RawPackageFName.ToString(), true, &OutReason))
		{
			const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
				FText::FromString(RawPackageFName.ToString()), OutReason);

			UE_LOG(LogAssetRegistryGenerator, Warning, TEXT("%s"), *(FailMessage.ToString()));
			return NAME_None;
		}


		FString LongPackageName;
		if (FPackageName::SearchForPackageOnDisk(RawPackageFName.ToString(), &LongPackageName) == false)
		{
			return NAME_None;
		}
		PackageFName = FName(*LongPackageName);
	}

	// don't include script packages in dependencies as they are always in memory
	if (FPackageName::IsScriptPackage(PackageFName.ToString()))
	{
		// no one likes script packages
		return NAME_None;
	}
	return PackageFName;
}

class FDefaultPakFileRules
{
public:
	void InitializeFromConfig(const ITargetPlatform* TargetPlatform)
	{
		if (bInitialized)
		{
			return;
		}

		FConfigFile ConfigFile;
		if (!FConfigCacheIni::LoadLocalIniFile(ConfigFile, TEXT("PakFileRules"), true /* bIsBaseIniName */))
		{
			return;
		}
		// Schema is defined in Engine\Config\BasePakFileRules.ini, see also GetPakFileRules in CopyBuildToStaging.Automation.cs

		FString IniPlatformName = TargetPlatform->IniPlatformName();
		for (TPair<FString, FConfigSection>& Pair : ConfigFile)
		{
			const FString& SectionName = Pair.Key;
			bool bMatchesAllPlatforms = true;
			bool bMatchesPlatform = false;
			FString ApplyToPlatformsValue;
			if (ConfigFile.GetString(*SectionName, TEXT("Platforms"), ApplyToPlatformsValue))
			{
				UE::String::ParseTokens(ApplyToPlatformsValue, ',',
					[&bMatchesAllPlatforms, &bMatchesPlatform, &IniPlatformName](FStringView Token)
					{
						bMatchesAllPlatforms = false;
						if (IniPlatformName == Token)
						{
							bMatchesPlatform = true;
						}
					}, UE::String::EParseTokensOptions::Trim | UE::String::EParseTokensOptions::SkipEmpty);
			}
			if (!bMatchesPlatform && !bMatchesAllPlatforms)
			{
				continue;
			}

			FString OverridePaksValue;
			if (ConfigFile.GetString(*SectionName, TEXT("OverridePaks"), OverridePaksValue))
			{
				UE::String::ParseTokens(OverridePaksValue, ',', [this](FStringView Token)
					{
						ReferencedPaks.Add(FString(Token));
					}, UE::String::EParseTokensOptions::Trim | UE::String::EParseTokensOptions::SkipEmpty);
			}
		}
		bInitialized = true;
	}

	bool IsChunkReferenced(int32 PakchunkIndex)
	{
		TStringBuilder<64> ChunkFileName;
		ChunkFileName.Appendf(TEXT("pakchunk%d"), PakchunkIndex);
		FStringView ChunkFileNameView(ChunkFileName);
		return ReferencedPaks.ContainsByHash(GetTypeHash(ChunkFileNameView), ChunkFileNameView);
	}

private:
	bool bInitialized = false;
	TSet<FString> ReferencedPaks;

};

//////////////////////////////////////////////////////////////////////////
// FAssetRegistryGenerator

void FAssetRegistryGenerator::UpdateAssetManagerDatabase()
{
	InitializeUseAssetManager();
	if (bUseAssetManager)
	{
		UAssetManager::Get().UpdateManagementDatabase();
	}
}

void FAssetRegistryGenerator::InitializeUseAssetManager()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	FAssignStreamingChunkDelegate& AssignStreamingChunkDelegate = FGameDelegates::Get().GetAssignStreamingChunkDelegate();
	FGetPackageDependenciesForManifestGeneratorDelegate& GetPackageDependenciesForManifestGeneratorDelegate = FGameDelegates::Get().GetGetPackageDependenciesForManifestGeneratorDelegate();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
	bUseAssetManager = UAssetManager::IsValid() && !AssignStreamingChunkDelegate.IsBound() && !GetPackageDependenciesForManifestGeneratorDelegate.IsBound();
}

bool FAssetRegistryGenerator::bUseAssetManager = true;

FAssetRegistryGenerator::FAssetRegistryGenerator(const ITargetPlatform* InPlatform)
	: AssetRegistry(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	, TargetPlatform(InPlatform)
	, bGenerateChunks(false)
	, HighestChunkId(0)
	, DependencyInfo(*GetMutableDefault<UChunkDependencyInfo>())
{
	bool bOnlyHardReferences = false;
	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	if (PackagingSettings)
	{
		bOnlyHardReferences = PackagingSettings->bChunkHardReferencesOnly;
	}	

	DependencyQuery = bOnlyHardReferences ? UE::AssetRegistry::EDependencyQuery::Hard : UE::AssetRegistry::EDependencyQuery::NoRequirements;

	InitializeUseAssetManager();
	InitializeChunkIdPakchunkIndexMapping();
}

FAssetRegistryGenerator::~FAssetRegistryGenerator()
{
}

bool FAssetRegistryGenerator::CleanTempPackagingDirectory(const FString& Platform) const
{
	FString TmpPackagingDir = GetTempPackagingDirectoryForPlatform(Platform);
	if (IFileManager::Get().DirectoryExists(*TmpPackagingDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*TmpPackagingDir, false, true))
		{
			UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to delete directory: %s"), *TmpPackagingDir);
			return false;
		}
	}

	FString ChunkListDir = FPaths::Combine(*FPaths::ProjectLogDir(), TEXT("ChunkLists"));
	if (IFileManager::Get().DirectoryExists(*ChunkListDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*ChunkListDir, false, true))
		{
			UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to delete directory: %s"), *ChunkListDir);
			return false;
		}
	}
	return true;
}

bool FAssetRegistryGenerator::ShouldPlatformGenerateStreamingInstallManifest(const ITargetPlatform* Platform) const
{
	if (Platform)
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *Platform->IniPlatformName());
		FString ConfigString;
		if (PlatformIniFile.GetString(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bGenerateChunks"), ConfigString))
		{
			return FCString::ToBool(*ConfigString);
		}
	}

	return false;
}

int64 FAssetRegistryGenerator::GetMaxChunkSizePerPlatform(const ITargetPlatform* Platform) const
{
	if (Platform)
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *Platform->IniPlatformName());
		FString ConfigString;
		if (PlatformIniFile.GetString(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MaxChunkSize"), ConfigString))
		{
			return FCString::Atoi64(*ConfigString);
		}
	}

	return -1;
}

TArray<int32> FAssetRegistryGenerator::GetExistingPackageChunkAssignments(FName PackageFName)
{
	TArray<int32> ExistingChunkIDs;
	for (uint32 ChunkIndex = 0, MaxChunk = ChunkManifests.Num(); ChunkIndex < MaxChunk; ++ChunkIndex)
	{
		if (ChunkManifests[ChunkIndex] && ChunkManifests[ChunkIndex]->Contains(PackageFName))
		{
			ExistingChunkIDs.AddUnique(ChunkIndex);
		}
	}

	if (StartupPackages.Contains(PackageFName))
	{
		ExistingChunkIDs.AddUnique(0);
	}

	return ExistingChunkIDs;
}

TArray<int32> FAssetRegistryGenerator::GetExplicitChunkIDs(const FName& PackageFName)
{
	TArray<int32> PackageInputChunkIds;
	const TArray<int32>* FoundIDs = ExplicitChunkIDs.Find(PackageFName);
	if (FoundIDs)
	{
		PackageInputChunkIds = *FoundIDs;
	}
	return PackageInputChunkIds;
}

class FPackageFileSizeVisitor : public IPlatformFile::FDirectoryStatVisitor
{
	TMap<FString, int64>& PackageFileSizes;
public:
	FPackageFileSizeVisitor(TMap<FString, int64>& InFileSizes)
		: PackageFileSizes(InFileSizes)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData)
	{
		const TCHAR* Extensions[] = { TEXT(".uexp"), TEXT(".uasset"), TEXT(".ubulk"), TEXT(".ufont"), TEXT(".umap"), TEXT(".uptnl") };

		if (StatData.bIsDirectory)
			return true;

		const TCHAR* Extension = FCString::Strrchr(FilenameOrDirectory, '.');
		if (!Extension)
			return true;

		int32 ExtIndex = 0;
		for (; ExtIndex < UE_ARRAY_COUNT(Extensions); ++ExtIndex)
		{
			if (0 == FCString::Stricmp(Extension, Extensions[ExtIndex]))
				break;
		}

		if (ExtIndex >= UE_ARRAY_COUNT(Extensions))
			return true;

		int32 LengthWithoutExtension = Extension - FilenameOrDirectory;
		FString FilenameWithoutExtension(LengthWithoutExtension, FilenameOrDirectory);

		if (int64* CurrentPackageSize = PackageFileSizes.Find(FilenameWithoutExtension))
		{
			int64& TotalPackageSize = *CurrentPackageSize;
			TotalPackageSize += StatData.FileSize;
		}
		else
		{
			PackageFileSizes.Add(FilenameWithoutExtension, StatData.FileSize);
		}

		return true;
	}
};

static void ParseChunkLayerAssignment(TArray<FString> ChunkLayerAssignmentArray, TMap<int32, int32>& OutChunkLayerAssignment)
{
	OutChunkLayerAssignment.Empty();

	const TCHAR* PropertyChunkId = TEXT("ChunkId=");
	const TCHAR* PropertyLayerId = TEXT("Layer=");
	for (FString& Entry : ChunkLayerAssignmentArray)
	{
		// Remove parentheses
		Entry.TrimStartAndEndInline();
		Entry.ReplaceInline(TEXT("("), TEXT(""));
		Entry.ReplaceInline(TEXT(")"), TEXT(""));

		int32 ChunkId = -1;
		int32 LayerId = -1;
		FParse::Value(*Entry, PropertyChunkId, ChunkId);
		FParse::Value(*Entry, PropertyLayerId, LayerId);

		if (ChunkId >= 0 && LayerId >= 0 && !OutChunkLayerAssignment.Contains(ChunkId))
		{
			OutChunkLayerAssignment.Add(ChunkId, LayerId);
		}
	}
}

static void AssignLayerChunkDelegate(const FAssignLayerChunkMap* ChunkManifest, const FString& Platform, const int32 ChunkIndex, int32& OutChunkLayer)
{
	OutChunkLayer = 0;

	FConfigFile PlatformIniFile;
	FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *Platform);
	TArray<FString> ChunkLayerAssignmentArray;
	PlatformIniFile.GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("ChunkLayerAssignment"), ChunkLayerAssignmentArray);

	TMap<int32, int32> ChunkLayerAssignment;
	ParseChunkLayerAssignment(ChunkLayerAssignmentArray, ChunkLayerAssignment);

	int32* LayerId = ChunkLayerAssignment.Find(ChunkIndex);
	if (LayerId)
	{
		OutChunkLayer = *LayerId;
	}
}

void IChunkDataGenerator::GenerateChunkDataFiles(const int32 InChunkId, const TSet<FName>& InPackagesInChunk,
	const ITargetPlatform* TargetPlatform, FSandboxPlatformFile* InSandboxFile, TArray<FString>& OutChunkFilenames)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GenerateChunkDataFiles(InChunkId, InPackagesInChunk, TargetPlatform->PlatformName(), InSandboxFile, OutChunkFilenames);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FAssetRegistryGenerator::GenerateStreamingInstallManifest(int64 InOverrideChunkSize, const TCHAR* InManifestSubDir,
	FSandboxPlatformFile& InSandboxFile)
{
	const FString Platform = TargetPlatform->PlatformName();
	FString TmpPackagingDir = GetTempPackagingDirectoryForPlatform(Platform);
	if (InManifestSubDir)
	{
		TmpPackagingDir /= InManifestSubDir;
	}
	int64 MaxChunkSize = InOverrideChunkSize > 0 ? InOverrideChunkSize : GetMaxChunkSizePerPlatform(TargetPlatform);

	if (!IFileManager::Get().MakeDirectory(*TmpPackagingDir, true /* Tree */))
	{
		UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to create directory: %s"), *TmpPackagingDir);
		return false;
	}
	
	FString PakChunkListFilename = TmpPackagingDir / TEXT("pakchunklist.txt");
	FString PakChunkLayerInfoFilename = TmpPackagingDir / TEXT("pakchunklayers.txt");
	// List of pak file lists
	TUniquePtr<FArchive> PakChunkListFile(IFileManager::Get().CreateFileWriter(*PakChunkListFilename));
	// List of disc layer for each chunk
	TUniquePtr<FArchive> ChunkLayerFile(IFileManager::Get().CreateFileWriter(*PakChunkLayerInfoFilename));
	if (!PakChunkListFile || !ChunkLayerFile)
	{
		UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to open output pakchunklist file %s"),
			!PakChunkListFile ? *PakChunkListFilename : *PakChunkLayerInfoFilename);
		return false;
	}

	TArray<FString> CompressedChunkWildcards;
	TOptional<FConfigFile> PlatformIniFile;
	auto ConditionalLoadPlatformIniFile = [&PlatformIniFile, this]()
	{
		if (!PlatformIniFile)
		{
			PlatformIniFile.Emplace();
			FConfigCacheIni::LoadLocalIniFile(*PlatformIniFile, TEXT("Game"), true, *TargetPlatform->IniPlatformName());
		}
	};
	if (!TargetPlatform->IsServerOnly())
	{
		// Load the list of wildcards to specify which pakfiles should be compressed, if the targetplatform supports it.
		// This is only used in client platforms.
		ConditionalLoadPlatformIniFile();
		PlatformIniFile->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("CompressedChunkWildcard"), CompressedChunkWildcards);
	}

	// Update manifests for any encryption groups that contain non-asset files
	if (bUseAssetManager && !TargetPlatform->HasSecurePackageFormat())
	{
		FContentEncryptionConfig ContentEncryptionConfig;
		UAssetManager::Get().GetContentEncryptionConfig(ContentEncryptionConfig);
		const FContentEncryptionConfig::TGroupMap& EncryptionGroups = ContentEncryptionConfig.GetPackageGroupMap();
		
		for (const FContentEncryptionConfig::TGroupMap::ElementType& GroupElement : EncryptionGroups)
		{
			const FName GroupName = GroupElement.Key;
			const FContentEncryptionConfig::FGroup& EncryptionGroup = GroupElement.Value;

			if (EncryptionGroup.NonAssetFiles.Num() > 0)
			{
				UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Updating non-asset files in manifest for group '%s'"), *GroupName.ToString());
				
				int32 ChunkID = UAssetManager::Get().GetContentEncryptionGroupChunkID(GroupName);
				int32 PakchunkIndex = GetPakchunkIndex(ChunkID);
				if (PakchunkIndex >= FinalChunkManifests.Num())
				{
					// Extend the array until it is large enough to hold the requested index, filling it in with nulls on all the newly added indices.
					// Note that this will temporarily break our contract that FinalChunkManifests does not contain null pointers; we fix up the contract
					// by replacing any remaining null pointers in the loop over FinalChunkManifests at the bottom of this function.
					FinalChunkManifests.AddDefaulted(PakchunkIndex - FinalChunkManifests.Num() + 1);
				}

				FChunkPackageSet* Manifest = FinalChunkManifests[PakchunkIndex].Get();
				if (Manifest == nullptr)
				{
					Manifest = new FChunkPackageSet();
					FinalChunkManifests[PakchunkIndex].Reset(Manifest);
				}

				for (const FString& NonAssetFile : EncryptionGroup.NonAssetFiles)
				{
					// Paths added as relative to the root. The staging code will need to map this onto the target path of all staged assets
					Manifest->Add(*NonAssetFile, FPaths::RootDir() / NonAssetFile);
				}
			}
		}
	}

	TMap<FString, int64> PackageFileSizes;
	if (MaxChunkSize > 0)
	{
		// ZENTODO: Pull file size information from the AssetRegistryState, since the packages do not exist on disk in zen
		FString SandboxPath = InSandboxFile.GetSandboxDirectory();
		SandboxPath.ReplaceInline(TEXT("[Platform]"), *Platform);
		FPackageFileSizeVisitor PackageSearch(PackageFileSizes);
		IFileManager::Get().IterateDirectoryStatRecursively(*SandboxPath, PackageSearch);
	}
	
	bool bEnableGameOpenOrderSort = false;
	bool bUseSecondaryOpenOrder = false;
	TArray<FString> OrderFileSpecStrings;
	{
		ConditionalLoadPlatformIniFile();
		PlatformIniFile->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bEnableAssetRegistryGameOpenOrderSort"), bEnableGameOpenOrderSort);
		PlatformIniFile->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bPakUsesSecondaryOrder"), bUseSecondaryOpenOrder);
		PlatformIniFile->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("PakOrderFileSpecs"), OrderFileSpecStrings);
	}

	// if a game open order can be found then use that to sort the filenames
	FPakOrderMap OrderMap;
	bool bHaveGameOpenOrder = false;
	if (bEnableGameOpenOrderSort)
	{
		TArray<FPakOrderFileSpec> OrderFileSpecs;
		const FPakOrderFileSpec* SecondaryOrderSpec = nullptr;

		if (OrderFileSpecStrings.Num() == 0)
		{
			OrderFileSpecs.Add(FPakOrderFileSpec(TEXT("GameOpenOrder*.log")));
			if (bUseSecondaryOpenOrder)
			{
				OrderFileSpecs.Add(FPakOrderFileSpec(TEXT("CookerOpenOrder*.log")));
				SecondaryOrderSpec = &OrderFileSpecs.Last();
			}
		}
		else
		{
			UScriptStruct* Struct = FPakOrderFileSpec::StaticStruct();
			for (const FString& String : OrderFileSpecStrings)
			{
				FPakOrderFileSpec Spec;
				Struct->ImportText(*String, &Spec, nullptr, PPF_Delimited, nullptr, Struct->GetName());
				OrderFileSpecs.Add(Spec);
			}

		}

		TArray<FString> DirsToSearch;
		DirsToSearch.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Build"), TEXT("FileOpenOrder")));
		FString PlatformsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Platforms"), Platform, TEXT("Build"), TEXT("FileOpenOrder"));
		if (IFileManager::Get().DirectoryExists(*PlatformsDir))
		{
			DirsToSearch.Add(PlatformsDir);
		}
		else
		{
			DirsToSearch.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Build"), Platform, TEXT("FileOpenOrder")));
		}

		TArray<FPakOrderMap> OrderMaps; // Indexes match with OrderFileSpecs
		OrderMaps.Reserve(OrderFileSpecs.Num());
		uint64 StartIndex = 0;
		for (const FPakOrderFileSpec& Spec : OrderFileSpecs)
		{
			// For each order map reserve it a contiguous integer range based on what indices were used by previous maps
			// After building each map we'll then merge in priority order
			UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Order file spec %s starting at index %llu"), *Spec.Pattern, StartIndex);

			FPakOrderMap& LocalOrderMap = OrderMaps.AddDefaulted_GetRef();

			TArray<FString> FoundFiles;
			for (const FString& Directory : DirsToSearch)
			{
				TArray<FString> LocalFoundFiles;
				IFileManager::Get().FindFiles(LocalFoundFiles, *FPaths::Combine(Directory, Spec.Pattern), true, false);

				for (const FString& Filename : LocalFoundFiles)
				{
					FoundFiles.Add(FPaths::Combine(Directory, Filename));
				}
			}
			
			Algo::SortBy(FoundFiles, [](const FString& Filename) {
				FString Number;
				int32 Order = 0;
				if (Filename.Split(TEXT("_"), nullptr, &Number, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					Order = FCString::Atoi(*Number);
				}
				return Order;
			});

			if (FoundFiles.Num() > 0)
			{
				bHaveGameOpenOrder = bHaveGameOpenOrder || (&Spec != SecondaryOrderSpec);
			}

			for (int32 i=0; i < FoundFiles.Num(); ++i)
			{
				const FString& Found = FoundFiles[i];
				UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Found order file %s"), *Found);
				if (LocalOrderMap.Num() == 0) 
				{
					LocalOrderMap.ProcessOrderFile(*Found, &Spec == SecondaryOrderSpec, false, StartIndex);
				}
				else
				{
					LocalOrderMap.ProcessOrderFile(*Found, &Spec == SecondaryOrderSpec, true);
				}
			}

			if (LocalOrderMap.Num() > 0)
			{
				check(LocalOrderMap.GetMaxIndex() >= StartIndex);
				StartIndex = LocalOrderMap.GetMaxIndex() + 1;
			}
		}

		TArray<int32> SpecIndicesByPriority;
		for (int32 i=0; i < OrderFileSpecs.Num(); ++i)
		{
			SpecIndicesByPriority.Add(i);
		}
		Algo::StableSortBy(SpecIndicesByPriority, [&OrderFileSpecs](int32 i) { return  OrderFileSpecs[i].Priority; }, TGreater<int32>());

		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Merging order maps in priority order"));
		for (int32 SpecIndex : SpecIndicesByPriority)
		{
			const FPakOrderFileSpec& Spec = OrderFileSpecs[SpecIndex];
			UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Merging order file spec %d (%s) at priority (%d) "), SpecIndex, *Spec.Pattern, Spec.Priority);
			FPakOrderMap& LocalOrderMap = OrderMaps[SpecIndex];
			OrderMap.MergeOrderMap(MoveTemp(LocalOrderMap));
		}
	}

	// generate per-chunk pak list files
	FDefaultPakFileRules DefaultPakFileRules;
	bool bSucceeded = true;
	for (int32 PakchunkIndex = 0; PakchunkIndex < FinalChunkManifests.Num() && bSucceeded; ++PakchunkIndex)
	{
		const FChunkPackageSet* Manifest = FinalChunkManifests[PakchunkIndex].Get();

		// Serialize chunk layers whether chunk is empty or not
		int32 TargetLayer = 0;
		FGameDelegates::Get().GetAssignLayerChunkDelegate().ExecuteIfBound(Manifest, Platform, PakchunkIndex, TargetLayer);

		FString LayerString = FString::Printf(TEXT("%d\r\n"), TargetLayer);
		ChunkLayerFile->Serialize(TCHAR_TO_ANSI(*LayerString), LayerString.Len());

		// Is this index a null placeholder that we added in the loop over EncryptedNonUFSFileGroups and then never filled in?  If so, 
		// fill it in with an empty FChunkPackageSet
		if (!Manifest)
		{
			FinalChunkManifests[PakchunkIndex].Reset(new FChunkPackageSet());
			Manifest = FinalChunkManifests[PakchunkIndex].Get();
		}
		
		// Split the chunk into subchunks as necessary and create and register a PakListFile for each subchunk
		int32 FilenameIndex = 0;
		TArray<FString> ChunkFilenames;
		Manifest->GenerateValueArray(ChunkFilenames);

		// Do not create any files if the chunk is empty and is not referenced by rules applied during staging
		if (ChunkFilenames.IsEmpty())
		{
			DefaultPakFileRules.InitializeFromConfig(TargetPlatform);
			if (!DefaultPakFileRules.IsChunkReferenced(PakchunkIndex))
			{
				continue;
			}
		}

		bool bFinishedAllFiles = false;
		for (int32 SubChunkIndex = 0; !bFinishedAllFiles; ++SubChunkIndex)
		{
			const FString PakChunkFilename = (SubChunkIndex > 0)
				? FString::Printf(TEXT("pakchunk%d_s%d.txt"), PakchunkIndex, SubChunkIndex)
				: FString::Printf(TEXT("pakchunk%d.txt"), PakchunkIndex);

			const FString PakListFilename = FString::Printf(TEXT("%s/%s"), *TmpPackagingDir, *PakChunkFilename);
			TUniquePtr<FArchive> PakListFile(IFileManager::Get().CreateFileWriter(*PakListFilename));

			if (!PakListFile)
			{
				UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to open output paklist file %s"), *PakListFilename);
				bSucceeded = false;
				break;
			}

			FString PakChunkOptions;
			for (const FString& CompressedChunkWildcard : CompressedChunkWildcards)
			{
				if (PakChunkFilename.MatchesWildcard(CompressedChunkWildcard))
				{
					PakChunkOptions += " compressed";
					break;
				}
			}

			if (bUseAssetManager)
			{
				// For encryption chunks, PakchunkIndex equals ChunkID
				FGuid Guid = UAssetManager::Get().GetChunkEncryptionKeyGuid(PakchunkIndex);
				if (Guid.IsValid())
				{
					PakChunkOptions += TEXT(" encryptionkeyguid=") + Guid.ToString();

					// If this chunk has a seperate unique asset registry, add it to first subchunk's manifest here
					if (SubChunkIndex == 0)
					{
						// For chunks with unique asset registry name, pakchunkIndex should equal chunkid
						FName RegistryName = UAssetManager::Get().GetUniqueAssetRegistryName(PakchunkIndex);
						if (RegistryName != NAME_None)
						{
							FString AssetRegistryFilename = FString::Printf(TEXT("%s%sAssetRegistry%s.bin"),
								*InSandboxFile.GetSandboxDirectory(), *InSandboxFile.GetGameSandboxDirectoryName(),
								*RegistryName.ToString());
							ChunkFilenames.Add(AssetRegistryFilename);
						}
					}
				}
			}

			// Allow the extra data generation steps to run and add their output to the manifest
			if (ChunkDataGenerators.Num() > 0 && SubChunkIndex == 0)
			{
				TSet<FName> PackagesInChunk;
				PackagesInChunk.Reserve(Manifest->Num());
				for (const auto& ChunkManifestPair : *Manifest)
				{
					PackagesInChunk.Add(ChunkManifestPair.Key);
				}

				for (const TSharedRef<IChunkDataGenerator>& ChunkDataGenerator : ChunkDataGenerators)
				{
					ChunkDataGenerator->GenerateChunkDataFiles(PakchunkIndex, PackagesInChunk, TargetPlatform, &InSandboxFile, ChunkFilenames);
				}
			}

			if (bUseAssetManager && SubChunkIndex == 0)
			{
				if (bHaveGameOpenOrder)
				{		
					FString CookedDirectory = FPaths::ConvertRelativePathToFull( FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]")) );
					FString RelativePath = TEXT("../../../");

					struct FFilePaths
					{
						FFilePaths(const FString& InFilename, FString&& InRelativeFilename, uint64 InFileOpenOrder) : Filename(InFilename), RelativeFilename(MoveTemp(InRelativeFilename)), FileOpenOrder(InFileOpenOrder)
						{ }
						FString Filename;
						FString RelativeFilename;
						uint64 FileOpenOrder;
					};

					TArray<FFilePaths> SortedFiles;
					SortedFiles.Empty(ChunkFilenames.Num());
					for (const FString& ChunkFilename : ChunkFilenames)
					{
						FString RelativeFilename = ChunkFilename.Replace(*CookedDirectory, *RelativePath);
						FPaths::RemoveDuplicateSlashes(RelativeFilename);
						FPaths::NormalizeFilename(RelativeFilename);
						if (FPaths::GetExtension(RelativeFilename).IsEmpty())
						{
							RelativeFilename = FPaths::SetExtension(RelativeFilename, TEXT("uasset")); // only use the uassets to decide which pak file these chunks should live in
						}
						RelativeFilename.ToLowerInline();
						uint64 FileOpenOrder = OrderMap.GetFileOrder(RelativeFilename, true /* bAllowUexpUBulkFallback */);
						/*if (FileOpenOrder != MAX_uint64)
						{
							UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Found file open order for %s, %ll"), *RelativeFilename, FileOpenOrder);
						}
						else
						{
							UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Didn't find openorder for %s"), *RelativeFilename, FileOpenOrder);
						}*/
						SortedFiles.Add(FFilePaths(ChunkFilename, MoveTemp(RelativeFilename), FileOpenOrder));
					}

					SortedFiles.Sort([&OrderMap, &CookedDirectory, &RelativePath](const FFilePaths& A, const FFilePaths& B)
					{
						uint64 AOrder = A.FileOpenOrder;
						uint64 BOrder = B.FileOpenOrder;

						if (AOrder == MAX_uint64 && BOrder == MAX_uint64)
						{
							return A.RelativeFilename.Compare(B.RelativeFilename, ESearchCase::IgnoreCase) < 0;
						}
						else 
						{
							return AOrder < BOrder;
						}
					});

					ChunkFilenames.Empty(SortedFiles.Num());
					for (int I = 0; I < SortedFiles.Num(); ++I)
					{
						ChunkFilenames.Add(MoveTemp(SortedFiles[I].Filename));
					}
				}
				else
				{
					// Sort so the order is consistent. If load order is important then it should be specified as a load order file to UnrealPak
					ChunkFilenames.Sort();
				}
			}

			int64 CurrentPakSize = 0;
			int64 NextFileSize = 0;
			FString NextFilename;
			bFinishedAllFiles = true;
			for (; FilenameIndex < ChunkFilenames.Num(); ++FilenameIndex)
			{
				FString Filename = ChunkFilenames[FilenameIndex];
				FString PakListLine = FPaths::ConvertRelativePathToFull(Filename.Replace(TEXT("[Platform]"), *Platform));
				if (MaxChunkSize > 0)
				{
					const int64* ExistingPackageFileSize = PackageFileSizes.Find(PakListLine);
					const int64 PackageFileSize = ExistingPackageFileSize ? *ExistingPackageFileSize : 0;
					CurrentPakSize += PackageFileSize;
					if (MaxChunkSize < CurrentPakSize)
					{
						// early out if we are over memory limit
						bFinishedAllFiles = false;
						NextFileSize = PackageFileSize;
						NextFilename = MoveTemp(PakListLine);
						break;
					}
				}
				
				PakListLine.ReplaceInline(TEXT("/"), TEXT("\\"));
				PakListLine += TEXT("\r\n");
				PakListFile->Serialize(TCHAR_TO_ANSI(*PakListLine), PakListLine.Len());
			}

			const bool bAddedFilesToPakList = PakListFile->Tell() > 0;
			PakListFile->Close();

			if (!bFinishedAllFiles && !bAddedFilesToPakList)
			{
				const TCHAR* UnitsText = TEXT("MB");
				int32 Unit = 1000*1000;
				if (MaxChunkSize < Unit * 10)
				{
					Unit = 1;
					UnitsText = TEXT("bytes");
				}
				UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to add file %s to paklist '%s'. The maximum size for a Pakfile is %d %s, but the file to add is %d %s."),
					*NextFilename, *PakListFilename,
					MaxChunkSize/Unit, UnitsText, // Round the limit down and round the value up, so that the display always shows that the value is greater than the limit
					(NextFileSize+Unit-1)/Unit, UnitsText);
				bSucceeded = false;
				break;
			}

			// add this pakfilelist to our master list of pakfilelists
			FString PakChunkListLine = FString::Printf(TEXT("%s%s\r\n"), *PakChunkFilename, *PakChunkOptions);
			PakChunkListFile->Serialize(TCHAR_TO_ANSI(*PakChunkListLine), PakChunkListLine.Len());

			// Add layer information for this subchunk (we serialize it for the main chunk outside of this loop, hence the check).
			if (SubChunkIndex > 0)
			{
				ChunkLayerFile->Serialize(TCHAR_TO_ANSI(*LayerString), LayerString.Len());
			}
		}
	}

	ChunkLayerFile->Close();
	PakChunkListFile->Close();
	
	if (bSucceeded)
	{
		FString ChunkManifestDirectory = FPaths::ProjectDir() / TEXT("Metadata") / TEXT("ChunkManifest");
		ChunkManifestDirectory =
			InSandboxFile.ConvertToAbsolutePathForExternalAppForWrite(*ChunkManifestDirectory)
				.Replace(TEXT("[Platform]"), *Platform);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.CopyDirectoryTree(*ChunkManifestDirectory, *TmpPackagingDir, true))
		{
			UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to copy chunk manifest from '%s' to '%s'"), *TmpPackagingDir, *ChunkManifestDirectory)
			return false;
		}
	}

	return bSucceeded;
}

void FAssetRegistryGenerator::CalculateChunkIdsAndAssignToManifest(const FName& PackageFName, const FString& PackagePathName,
	const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile& InSandboxFile)
{
	TArray<int32> TargetChunks;
	TArray<int32> ExistingChunkIDs;

	if (!bGenerateChunks)
	{
		TargetChunks.AddUnique(0);
		ExistingChunkIDs.AddUnique(0);
	}
	else
	{
		TArray<int32> PackageChunkIDs = GetExplicitChunkIDs(PackageFName);
		ExistingChunkIDs = GetExistingPackageChunkAssignments(PackageFName);
		if (bUseAssetManager)
		{
			// No distinction between source of existing chunks for new flow
			PackageChunkIDs.Append(ExistingChunkIDs);

			UAssetManager::Get().GetPackageChunkIds(PackageFName, TargetPlatform, PackageChunkIDs, TargetChunks);
		}
		else
		{
			// Try to call game-specific delegate to determine the target chunk ID
			// FString Name = Package->GetPathName();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FAssignStreamingChunkDelegate& AssignStreamingChunkDelegate = FGameDelegates::Get().GetAssignStreamingChunkDelegate();
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			if (AssignStreamingChunkDelegate.IsBound())
			{
				AssignStreamingChunkDelegate.ExecuteIfBound(PackagePathName, LastLoadedMapName, PackageChunkIDs, ExistingChunkIDs, TargetChunks);
			}
			else
			{
				//Take explicit assignments and existing assignments
				TargetChunks.Append(PackageChunkIDs);
				TargetChunks.Append(ExistingChunkIDs);
			}
		}
	}

	// Add the package to the manifest for every chunk the AssetManager found it should belong to
	for (const int32 PackageChunk : TargetChunks)
	{
		AddPackageToManifest(SandboxFilename, PackageFName, PackageChunk);
	}
	// Remove the package from the manifest for every chunk the AssetManager rejected from the existing chunks
	for (const int32 PackageChunk : ExistingChunkIDs)
	{
		if (!TargetChunks.Contains(PackageChunk))
		{
			RemovePackageFromManifest(PackageFName, PackageChunk);
		}
	}
}

void FAssetRegistryGenerator::CleanManifestDirectories()
{
	CleanTempPackagingDirectory(TargetPlatform->PlatformName());
}

void FAssetRegistryGenerator::SetPreviousAssetRegistry(TUniquePtr<FAssetRegistryState>&& InPreviousState)
{
	PreviousPackagesToUpdate.Empty();
	if (InPreviousState)
	{
		const TMap<FName, const FAssetPackageData*>& PreviousPackageDataMap = InPreviousState->GetAssetPackageDataMap();
		PreviousPackagesToUpdate.Reserve(PreviousPackageDataMap.Num());
		for (const TPair<FName, const FAssetPackageData*>& Pair : PreviousPackageDataMap)
		{
			FName PackageName = Pair.Key;
			if (!State.GetAssetPackageData(PackageName))
			{
				continue;
			}
			TArrayView<FAssetData const * const> PreviousAssetDatas = InPreviousState->GetAssetsByPackageName(PackageName);
			TPair<TArray<FAssetData>, FAssetPackageData>& UpdateData = PreviousPackagesToUpdate.FindOrAdd(PackageName);
			if (PreviousAssetDatas.Num() > 0)
			{
				TArray<FAssetData>& UpdateAssetDatas = UpdateData.Get<0>();
				UpdateAssetDatas.Reserve(PreviousAssetDatas.Num());
				for (const FAssetData* AssetData : PreviousAssetDatas)
				{
					UpdateAssetDatas.Add(FAssetData(*AssetData));
				}
			}
			const FAssetPackageData& PreviousPackageData = *Pair.Value;
			UpdateData.Get<1>() = PreviousPackageData;
		}
	}
}

void FAssetRegistryGenerator::InjectEncryptionData(FAssetRegistryState& TargetState)
{
	if (bUseAssetManager)
	{
		UAssetManager& AssetManager = UAssetManager::Get();

		TMap<int32, FGuid> GuidCache;
		FContentEncryptionConfig EncryptionConfig;
		AssetManager.GetContentEncryptionConfig(EncryptionConfig);

		for (FContentEncryptionConfig::TGroupMap::ElementType EncryptedAssetSetElement : EncryptionConfig.GetPackageGroupMap())
		{
			FName SetName = EncryptedAssetSetElement.Key;
			TSet<FName>& EncryptedRootAssets = EncryptedAssetSetElement.Value.PackageNames;

			for (FName EncryptedRootPackageName : EncryptedRootAssets)
			{
				for (const FAssetData* PackageAsset : TargetState.GetAssetsByPackageName(EncryptedRootPackageName))
				{
					FAssetData* AssetData = const_cast<FAssetData*>(PackageAsset);

					if (AssetData)
					{
						FString GuidString;
						const FAssetData::FChunkArrayView ChunkIDs = AssetData->GetChunkIDs();
						if (ChunkIDs.Num() > 1)
						{
							UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Encrypted root asset '%s' exists in two chunks. Only secondary assets should be shared between chunks."), *AssetData->GetObjectPathString());
						}
						else if (ChunkIDs.Num() == 1)
						{
							int32 ChunkID = ChunkIDs[0];
							FGuid Guid;

							if (GuidCache.Contains(ChunkID))
							{
								Guid = GuidCache[ChunkID];
							}
							else
							{
								Guid = GuidCache.Add(ChunkID, AssetManager.GetChunkEncryptionKeyGuid(ChunkID));
							}

							if (Guid.IsValid())
							{
								FAssetDataTagMap TagsAndValues = AssetData->TagsAndValues.CopyMap();
								TagsAndValues.Add(UAssetManager::GetEncryptionKeyAssetTagName(), Guid.ToString());
								FAssetData NewAssetData = FAssetData(AssetData->PackageName, AssetData->PackagePath, AssetData->AssetName, AssetData->AssetClassPath, TagsAndValues, ChunkIDs, AssetData->PackageFlags);
								NewAssetData.TaggedAssetBundles = AssetData->TaggedAssetBundles;
								TargetState.UpdateAssetData(AssetData, MoveTemp(NewAssetData));
							}
						}
					}
				}
			}
		}
	}
}

bool FAssetRegistryGenerator::SaveManifests(FSandboxPlatformFile& InSandboxFile, int64 InOverrideChunkSize,
	const TCHAR* InManifestSubDir)
{
	if (!bGenerateChunks)
	{
		return true;
	}

	if (!GenerateStreamingInstallManifest(InOverrideChunkSize, InManifestSubDir, InSandboxFile))
	{
		return false;
	}

	// Generate map for the platform abstraction
	TMultiMap<FString, int32> PakchunkMap;	// asset -> ChunkIDs map
	TSet<int32> PakchunkIndicesInUse;
	const FString PlatformName = TargetPlatform->PlatformName();

	// Collect all unique chunk indices and map all files to their chunks
	for (int32 PakchunkIndex = 0; PakchunkIndex < FinalChunkManifests.Num(); ++PakchunkIndex)
	{
		check(FinalChunkManifests[PakchunkIndex]);
		if (FinalChunkManifests[PakchunkIndex]->Num())
		{
			PakchunkIndicesInUse.Add(PakchunkIndex);
			for (const TPair<FName,FString>& Pair: *FinalChunkManifests[PakchunkIndex])
			{
				FString PlatFilename = Pair.Value.Replace(TEXT("[Platform]"), *PlatformName);
				PakchunkMap.Add(PlatFilename, PakchunkIndex);
			}
		}
	}

	// Sort our chunk IDs and file paths
	PakchunkMap.KeySort(TLess<FString>());
	PakchunkIndicesInUse.Sort(TLess<int32>());

	// Platform abstraction will generate any required platform-specific files for the chunks
	if (!TargetPlatform->GenerateStreamingInstallManifest(PakchunkMap, PakchunkIndicesInUse))
	{
		return false;
	}

	if (!bUseAssetManager)
	{
		// In new flow, this is written later
		GenerateAssetChunkInformationCSV(FPaths::Combine(*FPaths::ProjectLogDir(), TEXT("ChunkLists")), true);
	}

	return true;
}

bool FAssetRegistryGenerator::ContainsMap(const FName& PackageName) const
{
	return PackagesContainingMaps.Contains(PackageName);
}

FAssetPackageData* FAssetRegistryGenerator::GetAssetPackageData(const FName& PackageName)
{
	return State.CreateOrGetAssetPackageData(PackageName);
}

void FAssetRegistryGenerator::UpdateKeptPackages()
{
	for (TPair<FName, TPair<TArray<FAssetData>, FAssetPackageData>>& Pair : PreviousPackagesToUpdate)
	{
		for (const FAssetData& PreviousAssetData : Pair.Value.Get<0>())
		{
			State.UpdateAssetData(PreviousAssetData);
		}
		FAssetPackageData& PackageData = *State.CreateOrGetAssetPackageData(Pair.Key);
		FAssetPackageData& PreviousPackageData = Pair.Value.Get<1>();
		PackageData.CookedHash = PreviousPackageData.CookedHash;
		PackageData.DiskSize = PreviousPackageData.DiskSize;
	}
}

static void AppendCookTagsToTagMap(TArray<TPair<FName, FString>>&& InTags, FAssetDataTagMap& OutTags)
{
	for (TPair<FName, FString>& Tag : InTags)
	{
		// Don't add empty tags
		if (!Tag.Key.IsNone() && !Tag.Value.IsEmpty())
		{
			// Prepend Cook_
			// Do the accumulation in a stack buffer to avoid a bit of work, but we still
			// eat the FName generation.
			FName TagName(WriteToString<256>(TEXTVIEW("Cook_"), Tag.Key));
			OutTags.Add(TagName, MoveTemp(Tag.Value));
		}
	}
}

static void AddCookTagsToState(TMap<FSoftObjectPath, TArray<TPair<FName, FString>>>&& InCookTags, FAssetRegistryState& InState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AddCookTagToState)

	uint32 ObjectCount = 0;
	uint32 TagCount = 0;

	for (TPair<FSoftObjectPath, TArray<TPair<FName, FString>>>& ObjectToTags : InCookTags)
	{
		ObjectCount++;
		const FAssetData* AssetData = InState.GetAssetByObjectPath(FSoftObjectPath(WriteToString<256>(ObjectToTags.Key)));

		// Migrate to FAssetDataTagMap
		FAssetDataTagMap NewTags;
		AppendCookTagsToTagMap(MoveTemp(ObjectToTags.Value), NewTags);
		TagCount += NewTags.Num();
		InState.AddTagsToAssetData(ObjectToTags.Key, MoveTemp(NewTags));
	} // end for each object

	UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("Added %d cook tags to %d objects"), TagCount, ObjectCount);
	InCookTags.Reset();
}

void FAssetRegistryGenerator::UpdateCollectionAssetData()
{
	// Read out the per-platform settings use to build the list of collections to tag
	bool bTagAllCollections = false;
	TArray<FString> CollectionsToIncludeOrExclude;
	{
		const FString PlatformIniName = TargetPlatform->IniPlatformName();

		FConfigFile PlatformEngineIni;
		FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, (!PlatformIniName.IsEmpty() ? *PlatformIniName : ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())));

		// The list of collections will either be a inclusive or a exclusive depending on the value of bTagAllCollections
		PlatformEngineIni.GetBool(TEXT("AssetRegistry"), TEXT("bTagAllCollections"), bTagAllCollections);
		PlatformEngineIni.GetArray(TEXT("AssetRegistry"), bTagAllCollections ? TEXT("CollectionsToExcludeAsTags") : TEXT("CollectionsToIncludeAsTags"), CollectionsToIncludeOrExclude);
	}

	// Build the list of collections we should tag for each asset
	TMap<FSoftObjectPath, TArray<FName>> AssetPathsToCollectionTags;
	{
		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

		TArray<FCollectionNameType> CollectionNamesToTag;
		CollectionManager.GetCollections(CollectionNamesToTag);
		if (bTagAllCollections)
		{
			CollectionNamesToTag.RemoveAll([&CollectionsToIncludeOrExclude](const FCollectionNameType& CollectionNameAndType)
			{
				return CollectionsToIncludeOrExclude.Contains(CollectionNameAndType.Name.ToString());
			});
		}
		else
		{
			CollectionNamesToTag.RemoveAll([&CollectionsToIncludeOrExclude](const FCollectionNameType& CollectionNameAndType)
			{
				return !CollectionsToIncludeOrExclude.Contains(CollectionNameAndType.Name.ToString());
			});
		}
		
		TArray<FSoftObjectPath> TmpAssetPaths;
		for (const FCollectionNameType& CollectionNameToTag : CollectionNamesToTag)
		{
			const FName CollectionTagName = *FString::Printf(TEXT("%s%s"), FAssetData::GetCollectionTagPrefix(), *CollectionNameToTag.Name.ToString());

			TmpAssetPaths.Reset();
			CollectionManager.GetAssetsInCollection(CollectionNameToTag.Name, CollectionNameToTag.Type, TmpAssetPaths);

			for (const FSoftObjectPath& AssetPath : TmpAssetPaths)
			{
				TArray<FName>& CollectionTagsForAsset = AssetPathsToCollectionTags.FindOrAdd(AssetPath);
				CollectionTagsForAsset.AddUnique(CollectionTagName);
			}
		}
	}

	// Apply the collection tags to the asset registry state
	// Collection tags are queried only by the existence of the key, the value is never used. But Tag Values are not allowed
	// to be empty. Set the value for each tag to an arbitrary field, something short to avoid wasting memory. We use 1 (aka "true") for now.
	FStringView CollectionValue(TEXTVIEW("1"));
	for (const TPair<FSoftObjectPath, TArray<FName>>& AssetPathToCollectionTagsPair : AssetPathsToCollectionTags)
	{
		const FSoftObjectPath& AssetPath = AssetPathToCollectionTagsPair.Key;
		const TArray<FName>& CollectionTagsForAsset = AssetPathToCollectionTagsPair.Value;

		const FAssetData* AssetData = State.GetAssetByObjectPath(AssetPath);
		if (AssetData)
		{
			FAssetDataTagMap TagsAndValues = AssetData->TagsAndValues.CopyMap();
			for (const FName& CollectionTagName : CollectionTagsForAsset)
			{
				TagsAndValues.Add(CollectionTagName, FString(CollectionValue));
			}
			FAssetData NewAssetData(*AssetData);
			NewAssetData.TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(TagsAndValues));
			State.UpdateAssetData(const_cast<FAssetData*>(AssetData), MoveTemp(NewAssetData));
		}
	}
}

void FAssetRegistryGenerator::Initialize(const TArray<FName> &InStartupPackages, bool bInitializeFromExisting)
{
	StartupPackages.Append(InStartupPackages);

	FAssetRegistrySerializationOptions SaveOptions;

	// If the asset registry is still doing it's background scan, we need to wait for it to finish and tick it so that the results are flushed out
	while (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.Tick(-1.0f);
		FThreadHeartBeat::Get().HeartBeat();
		FPlatformProcess::SleepNoStats(0.0001f);
	}

	ensureMsgf(!AssetRegistry.IsLoadingAssets(), TEXT("Cannot initialize asset registry generator while asset registry is still scanning source assets "));

	AssetRegistry.InitializeSerializationOptions(SaveOptions, TargetPlatform->IniPlatformName());

	if (bInitializeFromExisting)
	{
		AssetRegistry.InitializeTemporaryAssetRegistryState(State, SaveOptions);
	}

	FGameDelegates::Get().GetAssignLayerChunkDelegate() = FAssignLayerChunkDelegate::CreateStatic(AssignLayerChunkDelegate);
}

void FAssetRegistryGenerator::ComputePackageDifferences(const FComputeDifferenceOptions& Options, const FAssetRegistryState& PreviousState, FAssetRegistryDifference& OutDifference)
{
	TArray<FName> ModifiedScriptPackages;
	const TMap<FName, const FAssetPackageData*>& PreviousAssetPackageDataMap = PreviousState.GetAssetPackageDataMap();

	for (const TPair<FName, const FAssetPackageData*>& PackagePair : State.GetAssetPackageDataMap())
	{
		FName PackageName = PackagePair.Key;
		const FAssetPackageData* CurrentPackageData = PackagePair.Value;

		const FAssetPackageData* PreviousPackageData = PreviousAssetPackageDataMap.FindRef(PackageName);

		if (!PreviousPackageData)
		{
			OutDifference.NewPackages.Add(PackageName);
		}
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		else if (CurrentPackageData->PackageGuid == PreviousPackageData->PackageGuid)
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			if (PreviousPackageData->DiskSize < 0)
			{
				OutDifference.IdenticalUncookedPackages.Add(PackageName);
			}
			else
			{
				OutDifference.IdenticalCookedPackages.Add(PackageName);
			}
		}
		else
		{
			if (FPackageName::IsScriptPackage(PackageName.ToString()))
			{
				ModifiedScriptPackages.Add(PackageName);
			}
			else
			{
				OutDifference.ModifiedPackages.Add(PackageName);
			}
		}
	}

	for (const TPair<FName, const FAssetPackageData*>& PackagePair : PreviousAssetPackageDataMap)
	{
		FName PackageName = PackagePair.Key;
		const FAssetPackageData* CurrentPackageData = State.GetAssetPackageData(PackageName);
		if (!CurrentPackageData)
		{
			OutDifference.RemovedPackages.Add(PackageName);
		}
	}

	if (Options.bRecurseModifications)
	{
		// Recurse modified packages to their dependencies. This is needed because we only compare package guids
		TArray<FName> ModifiedPackagesToRecurse = OutDifference.ModifiedPackages.Array();

		if (Options.bRecurseScriptModifications)
		{
			ModifiedPackagesToRecurse.Append(ModifiedScriptPackages);
		}

		for (int32 RecurseIndex = 0; RecurseIndex < ModifiedPackagesToRecurse.Num(); RecurseIndex++)
		{
			FName ModifiedPackage = ModifiedPackagesToRecurse[RecurseIndex];
			TArray<FAssetIdentifier> Referencers;
			State.GetReferencers(ModifiedPackage, Referencers, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

			for (const FAssetIdentifier& Referencer : Referencers)
			{
				FName ReferencerPackage = Referencer.PackageName;
				if (!OutDifference.ModifiedPackages.Contains(ReferencerPackage) && (OutDifference.IdenticalCookedPackages.Contains(ReferencerPackage) || OutDifference.IdenticalUncookedPackages.Contains(ReferencerPackage)))
				{
					// Remove from identical list
					OutDifference.IdenticalCookedPackages.Remove(ReferencerPackage);
					OutDifference.IdenticalUncookedPackages.Remove(ReferencerPackage);

					OutDifference.ModifiedPackages.Add(ReferencerPackage);
					ModifiedPackagesToRecurse.Add(ReferencerPackage);
				}
			}
		}
	}
}

void FAssetRegistryGenerator::ComputePackageRemovals(const FAssetRegistryState& PreviousState, TArray<FName>& RemovedPackages)
{
	for (const TPair<FName, const FAssetPackageData*>& PackagePair : PreviousState.GetAssetPackageDataMap())
	{
		FName PackageName = PackagePair.Key;
		const FAssetPackageData* CurrentPackageData = State.GetAssetPackageData(PackageName);
		if (!CurrentPackageData)
		{
			RemovedPackages.Add(PackageName);
		}
	}
}

void FAssetRegistryGenerator::FinalizeChunkIDs(const TSet<FName>& InCookedPackages,
	const TSet<FName>& InDevelopmentOnlyPackages, FSandboxPlatformFile& InSandboxFile,
	bool bGenerateStreamingInstallManifest)
{
	// bGenerateNoChunks overrides bGenerateStreamingInstallManifest overrides ShouldPlatformGenerateStreamingInstallManifest
	// bGenerateChunks means we allow chunks other than 0 based on package ChunkIds, AND we generate a manifest for each chunk
	const UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
	if (PackagingSettings->bGenerateNoChunks)
	{
		bGenerateChunks = false;
	}
	else if (bGenerateStreamingInstallManifest)
	{
		bGenerateChunks = true;
	}
	else
	{
		bGenerateChunks = ShouldPlatformGenerateStreamingInstallManifest(TargetPlatform);
	}

	CookedPackages = InCookedPackages;
	DevelopmentOnlyPackages = InDevelopmentOnlyPackages;

	TSet<FName> AllPackages;
	AllPackages.Append(CookedPackages);
	AllPackages.Append(DevelopmentOnlyPackages);

	// Prune our asset registry to cooked + dev only list
	FAssetRegistrySerializationOptions DevelopmentSaveOptions;
	AssetRegistry.InitializeSerializationOptions(DevelopmentSaveOptions, TargetPlatform->IniPlatformName(), UE::AssetRegistry::ESerializationTarget::ForDevelopment);
	State.PruneAssetData(AllPackages, TSet<FName>(), DevelopmentSaveOptions);

	// Mark development only packages as explicitly -1 size to indicate it was not cooked
	for (FName DevelopmentOnlyPackage : DevelopmentOnlyPackages)
	{
		FAssetPackageData* PackageData = State.CreateOrGetAssetPackageData(DevelopmentOnlyPackage);
		PackageData->DiskSize = -1;
	}

	// Copy ExplicitChunkIDs and other data from the AssetRegistry into the maps we use during finalization
	State.EnumerateAllAssets([&](const FAssetData& AssetData)
	{
		for (int32 ChunkID : AssetData.GetChunkIDs())
		{
			if (ChunkID < 0)
			{
				UE_LOG(LogAssetRegistryGenerator, Warning, TEXT("Out of range ChunkID: %d"), ChunkID);
				ChunkID = 0;
			}
			
			ExplicitChunkIDs.FindOrAdd(AssetData.PackageName).AddUnique(ChunkID);
		}

		// Clear the Asset's chunk id list. We will fill it with the final IDs to use later on.
		// Chunk Ids are safe to modify in place so do a const cast
		const_cast<FAssetData&>(AssetData).ClearChunkIDs();

		// Update whether the owner package contains a map
		if ((AssetData.PackageFlags & PKG_ContainsMap) != 0)
		{
			PackagesContainingMaps.Add(AssetData.PackageName);
		}
	});

	// add all the packages to the unassigned package list
	for (FName CookedPackage : CookedPackages)
	{
		const FString SandboxPath = InSandboxFile.ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(CookedPackage.ToString()));

		AllCookedPackageSet.Add(CookedPackage, SandboxPath);
		UnassignedPackageSet.Add(CookedPackage, SandboxPath);
	}

	TArray<FName> UnassignedPackageList;

	// Old path has map specific code, new code doesn't care about map or load order
	if (!bUseAssetManager)
	{
		// Assign startup packages, these will generally end up in chunk 0
		FString StartupPackageMapName(TEXT("None"));
		for (FName CookedPackage : StartupPackages)
		{
			const FString SandboxPath = InSandboxFile.ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(CookedPackage.ToString()));
			const FString PackagePathName = CookedPackage.ToString();
			AllCookedPackageSet.Add(CookedPackage, SandboxPath);
			CalculateChunkIdsAndAssignToManifest(CookedPackage, PackagePathName, SandboxPath, StartupPackageMapName, InSandboxFile);
		}

		// Capture list at start as elements will be removed during iteration
		UnassignedPackageSet.GenerateKeyArray(UnassignedPackageList);

		// assign chunks for all the map packages
		for (FName MapFName : UnassignedPackageList)
		{
			if (ContainsMap(MapFName) == false)
			{
				continue;
			}

			// get all the dependencies for this map
			TArray<FName> MapDependencies;
			ensure(GatherAllPackageDependencies(MapFName, MapDependencies));

			for (const auto& RawPackageFName : MapDependencies)
			{
				const FName PackageFName = GetPackageNameFromDependencyPackageName(RawPackageFName);

				if (PackageFName == NAME_None)
				{
					continue;
				}

				const FString PackagePathName = PackageFName.ToString();
				const FString MapName = MapFName.ToString();
				const FString* SandboxFilenamePtr = AllCookedPackageSet.Find(PackageFName);
				if (!SandboxFilenamePtr)
				{
					const FString SandboxPath = InSandboxFile.ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(PackagePathName));

					AllCookedPackageSet.Add(PackageFName, SandboxPath);

					SandboxFilenamePtr = AllCookedPackageSet.Find(PackageFName);
					check(SandboxFilenamePtr);
				}
				const FString& SandboxFilename = *SandboxFilenamePtr;

				CalculateChunkIdsAndAssignToManifest(PackageFName, PackagePathName, SandboxFilename, MapName, InSandboxFile);
			}
		}
	}

	// Capture list at start as elements will be removed during iteration
	UnassignedPackageSet.GenerateKeyArray(UnassignedPackageList);

	// process the remaining unassigned packages
	for (FName PackageFName : UnassignedPackageList)
	{
		const FString& SandboxFilename = AllCookedPackageSet.FindChecked(PackageFName);
		const FString PackagePathName = PackageFName.ToString();

		CalculateChunkIdsAndAssignToManifest(PackageFName, PackagePathName, SandboxFilename, FString(), InSandboxFile);
	}
	// anything that remains in the UnAssignedPackageSet is put in chunk0 by FixupPackageDependenciesForChunks

	FixupPackageDependenciesForChunks(InSandboxFile);
}

void FAssetRegistryGenerator::RegisterChunkDataGenerator(TSharedRef<IChunkDataGenerator> InChunkDataGenerator)
{
	ChunkDataGenerators.Add(MoveTemp(InChunkDataGenerator));
}

void FAssetRegistryGenerator::PreSave(const TSet<FName>& InCookedPackages)
{
	if (bUseAssetManager)
	{
		UAssetManager::Get().PreSaveAssetRegistry(TargetPlatform, InCookedPackages);
	}
}

void FAssetRegistryGenerator::PostSave()
{
	if (bUseAssetManager)
	{
		UAssetManager::Get().PostSaveAssetRegistry();
	}
}

void FAssetRegistryGenerator::AddAssetToFileOrderRecursive(const FName& InPackageName, TArray<FName>& OutFileOrder, TSet<FName>& OutEncounteredNames, const TSet<FName>& InPackageNameSet, const TSet<FName>& InTopLevelAssets)
{
	if (!OutEncounteredNames.Contains(InPackageName))
	{
		OutEncounteredNames.Add(InPackageName);

		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(InPackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

		for (FName DependencyName : Dependencies)
		{
			if (InPackageNameSet.Contains(DependencyName))
			{
				if (!InTopLevelAssets.Contains(DependencyName))
				{
					AddAssetToFileOrderRecursive(DependencyName, OutFileOrder, OutEncounteredNames, InPackageNameSet, InTopLevelAssets);
				}
			}
		}

		OutFileOrder.Add(InPackageName);
	}
}

bool FAssetRegistryGenerator::SaveAssetRegistry(const FString& SandboxPath, bool bSerializeDevelopmentAssetRegistry, bool bForceNoFilter)
{
	UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Saving asset registry v%d."), FAssetRegistryVersion::Type::LatestVersion);
	FAutoScopedDurationTimer Timer;

	
	// Write development first, this will always write
	FAssetRegistrySerializationOptions DevelopmentSaveOptions;
	AssetRegistry.InitializeSerializationOptions(DevelopmentSaveOptions, TargetPlatform->IniPlatformName(), UE::AssetRegistry::ESerializationTarget::ForDevelopment);

	// Write runtime registry, this can be excluded per game/platform
	FAssetRegistrySerializationOptions SaveOptions;
	AssetRegistry.InitializeSerializationOptions(SaveOptions, TargetPlatform->IniPlatformName());

	if (bForceNoFilter)
	{
		DevelopmentSaveOptions.DisableFilters();
		SaveOptions.DisableFilters();
	}

	// Filter tags from both development and saved registries, keeping only the tags applicable to the platform
	State.FilterTags(SaveOptions);

	// Then possibly apply previous AssetData and AssetPackageData for packages kept from a previous cook
	UpdateKeptPackages();
	UpdateCollectionAssetData();

	if (DevelopmentSaveOptions.bSerializeAssetRegistry)
	{
		FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
		const TCHAR* DevelopmentAssetRegistryFilename = GetDevelopmentAssetRegistryFilename();
		PlatformSandboxPath.ReplaceInline(TEXT("AssetRegistry.bin"), *FString::Printf(TEXT("Metadata/%s"), DevelopmentAssetRegistryFilename));

		if (bSerializeDevelopmentAssetRegistry)
		{
			// Make a copy of the state so we can add tags to only the development registry.
			FAssetRegistryState DevelopmentState;
			DevelopmentState.InitializeFromExisting(State, DevelopmentSaveOptions);

			AddCookTagsToState(MoveTemp(CookTagsToAdd), DevelopmentState);

			// Create development registry data, used for DLC cooks, iterative cooks, and editor viewing
			FArrayWriter SerializedAssetRegistry;

			DevelopmentState.Save(SerializedAssetRegistry, DevelopmentSaveOptions);

			// Save the generated registry
			FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatformSandboxPath);

			UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Generated development asset registry %s num assets %d, size is %5.2fkb"), *PlatformSandboxPath, State.GetNumAssets(), (float)SerializedAssetRegistry.Num() / 1024.f);
		}

		if (bGenerateChunks && bUseAssetManager)
		{
			FString ChunkListsPath = PlatformSandboxPath.Replace(*FString::Printf(TEXT("/%s"), DevelopmentAssetRegistryFilename), TEXT(""));

			// Write out CSV file with chunking information
			GenerateAssetChunkInformationCSV(ChunkListsPath, false);
		}
	}

	if (SaveOptions.bSerializeAssetRegistry)
	{
		TMap<int32, FString> ChunkBucketNames;
		TMap<int32, TSet<int32>> ChunkBuckets;
		const int32 GenericChunkBucket = -1;
		ChunkBucketNames.Add(GenericChunkBucket, FString());

		// When chunk manifests have been generated (e.g. cook by the book) serialize 
		// an asset registry for each chunk.
		if (FinalChunkManifests.Num() > 0)
		{
			// Pass over all chunks and build a mapping of chunk index to asset registry name. All chunks that don't have a unique registry are assigned to the "generic bucket"
			// which will be written to the master asset registry in chunk 0
			for (int32 PakchunkIndex = 0; PakchunkIndex < FinalChunkManifests.Num(); ++PakchunkIndex)
			{
				FChunkPackageSet* Manifest = FinalChunkManifests[PakchunkIndex].Get();
				if (Manifest == nullptr)
				{
					continue;
				}

				bool bAddToGenericBucket = true;

				if (bUseAssetManager)
				{
					// For chunks with unique asset registry name, pakchunkIndex should equal chunkid
					FName RegistryName = UAssetManager::Get().GetUniqueAssetRegistryName(PakchunkIndex);
					if (RegistryName != NAME_None)
					{
						ChunkBuckets.FindOrAdd(PakchunkIndex).Add(PakchunkIndex);
						ChunkBucketNames.FindOrAdd(PakchunkIndex) = RegistryName.ToString();
						bAddToGenericBucket = false;
					}
				}

				if (bAddToGenericBucket)
				{
					ChunkBuckets.FindOrAdd(GenericChunkBucket).Add(PakchunkIndex);
				}
			}

			FString SandboxPathWithoutExtension = FPaths::ChangeExtension(SandboxPath, TEXT(""));
			FString SandboxPathExtension = FPaths::GetExtension(SandboxPath);

			for (TMap<int32, TSet<int32>>::ElementType& ChunkBucketElement : ChunkBuckets)
			{
				// Prune out the development only packages, and any assets that belong in a different chunk asset registry
				FAssetRegistryState NewState;
				NewState.InitializeFromExistingAndPrune(State, CookedPackages, TSet<FName>(), ChunkBucketElement.Value, SaveOptions);

				if (!TargetPlatform->HasSecurePackageFormat())
				{
					InjectEncryptionData(NewState);
				}

				// Create runtime registry data
				FArrayWriter SerializedAssetRegistry;
				SerializedAssetRegistry.SetFilterEditorOnly(true);

				NewState.Save(SerializedAssetRegistry, SaveOptions);

				// Save the generated registry
				FString PlatformSandboxPath = SandboxPathWithoutExtension.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
				PlatformSandboxPath += ChunkBucketNames[ChunkBucketElement.Key] + TEXT(".") + SandboxPathExtension;

				FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatformSandboxPath);

				FString FilenameForLog;
				if (ChunkBucketElement.Key != GenericChunkBucket)
				{
					check(ChunkBucketElement.Key < FinalChunkManifests.Num());
					check(FinalChunkManifests[ChunkBucketElement.Key]);
					FilenameForLog = FString::Printf(TEXT("[chunkbucket %i] "), ChunkBucketElement.Key);
				}
				UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Generated asset registry %snum assets %d, size is %5.2fkb"), *FilenameForLog, NewState.GetNumAssets(), (float)SerializedAssetRegistry.Num() / 1024.f);
			}
		}
		// If no chunk manifests have been generated (e.g. cook on the fly)
		else
		{
			// Prune out the development only packages
			State.PruneAssetData(CookedPackages, TSet<FName>(), SaveOptions);

			// Create runtime registry data
			FArrayWriter SerializedAssetRegistry;
			SerializedAssetRegistry.SetFilterEditorOnly(true);

			State.Save(SerializedAssetRegistry, SaveOptions);

			// Save the generated registry
			FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
			FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatformSandboxPath);

			int32 NumAssets = State.GetNumAssets();
			UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Generated asset registry num assets %d, size is %5.2fkb"), NumAssets, (float)SerializedAssetRegistry.Num() / 1024.f);
		}
	}
	
	UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Done saving asset registry. Took %.1f seconds."), Timer.GetTime());

	return true;
}

class FPackageCookerOpenOrderVisitor : public IPlatformFile::FDirectoryVisitor
{
	const FSandboxPlatformFile& SandboxFile;
	const FString& PlatformSandboxPath;
	const TSet<FStringView>& ValidExtensions;
	TMultiMap<FString, FString>& PackageExtensions;
public:
	FPackageCookerOpenOrderVisitor(
		const FSandboxPlatformFile& InSandboxFile,
		const FString& InPlatformSandboxPath,
		const TSet<FStringView>& InValidExtensions,
		TMultiMap<FString, FString>& OutPackageExtensions) :
		SandboxFile(InSandboxFile),
		PlatformSandboxPath(InPlatformSandboxPath),
		ValidExtensions(InValidExtensions),
		PackageExtensions(OutPackageExtensions)
	{}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory)
			return true;

		FString Filename = FilenameOrDirectory;
		FStringView UnusedFilePath, FileBaseName, FileExtension;
		FPathViews::Split(Filename, UnusedFilePath, FileBaseName, FileExtension);
		if (ValidExtensions.Contains(FileExtension))
		{
			// if the file base name ends with an optional extension, ignore it. (i.e. .o.uasset/.o.uexp etc)
			if (FileBaseName.EndsWith(FPackagePath::GetOptionalSegmentExtensionModifier()))
			{
				return true;
			}

			FString PackageName;
			Filename.ReplaceInline(*PlatformSandboxPath, *SandboxFile.GetSandboxDirectory());
			FString AssetSourcePath = SandboxFile.ConvertFromSandboxPath(*Filename);
			FString StandardAssetSourcePath = FPaths::CreateStandardFilename(AssetSourcePath);
			if (StandardAssetSourcePath.EndsWith(TEXT(".m.ubulk")))
			{
				// '.' is an 'invalid' character in a filename; FilenameToLongPackageName will fail.
				FString BaseAssetSourcePath(StandardAssetSourcePath);
				BaseAssetSourcePath.RemoveFromEnd(TEXT(".m.ubulk"));
				PackageName = FPackageName::FilenameToLongPackageName(BaseAssetSourcePath);
			}
			else
			{
				PackageName = FPackageName::FilenameToLongPackageName(StandardAssetSourcePath);
			}

			PackageExtensions.AddUnique(PackageName, StandardAssetSourcePath);
		}

		return true;
	}
};

bool FAssetRegistryGenerator::WriteCookerOpenOrder(FSandboxPlatformFile& InSandboxFile)
{
	TSet<FName> PackageNameSet;
	TSet<FName> MapList;
	State.EnumerateAllAssets([this, &PackageNameSet, &MapList](const FAssetData& AssetData)
	{
		PackageNameSet.Add(AssetData.PackageName);

		// REPLACE WITH PRIORITY

		if (ContainsMap(AssetData.PackageName))
		{
			MapList.Add(AssetData.PackageName);
		}
	});

	FString CookerFileOrderString;
	{
		TArray<FName> TopLevelMapPackageNames;
		TArray<FName> TopLevelPackageNames;

		for (FName PackageName : PackageNameSet)
		{
			TArray<FName> Referencers;
			AssetRegistry.GetReferencers(PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

			bool bIsTopLevel = true;
			bool bIsMap = MapList.Contains(PackageName);

			if (!bIsMap && Referencers.Num() > 0)
			{
				for (auto ReferencerName : Referencers)
				{
					if (PackageNameSet.Contains(ReferencerName))
					{
						bIsTopLevel = false;
						break;
					}
				}
			}

			if (bIsTopLevel)
			{
				if (bIsMap)
				{
					TopLevelMapPackageNames.Add(PackageName);
				}
				else
				{
					TopLevelPackageNames.Add(PackageName);
				}
			}
		}

		TArray<FName> FileOrder;
		TSet<FName> EncounteredNames;
		for (FName PackageName : TopLevelPackageNames)
		{
			AddAssetToFileOrderRecursive(PackageName, FileOrder, EncounteredNames, PackageNameSet, MapList);
		}

		for (FName PackageName : TopLevelMapPackageNames)
		{
			AddAssetToFileOrderRecursive(PackageName, FileOrder, EncounteredNames, PackageNameSet, MapList);
		}
		
		// Iterate sandbox folder and generate a map from package name to cooked files
		const TArray<FStringView> ValidExtensions =
		{
			TEXT("uasset"),
			TEXT("uexp"),
			TEXT("ubulk"),
			TEXT("uptnl"),
			TEXT("umap"),
			TEXT("ufont")
		};
		const TSet<FStringView> ValidExtensionSet(ValidExtensions);

		const FString SandboxPath = InSandboxFile.GetSandboxDirectory();
		const FString Platform = TargetPlatform->PlatformName();
		FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform);

		// ZENTODO: Change this to work with Zen
		TMultiMap<FString, FString> CookedPackageFilesMap;
		FPackageCookerOpenOrderVisitor PackageSearch(InSandboxFile, PlatformSandboxPath, ValidExtensionSet, CookedPackageFilesMap);
		IFileManager::Get().IterateDirectoryRecursively(*PlatformSandboxPath, PackageSearch);

		int32 CurrentIndex = 0;
		for (FName PackageName : FileOrder)
		{
			TArray<FString> CookedFiles;
			CookedPackageFilesMap.MultiFind(PackageName.ToString(), CookedFiles);
			CookedFiles.Sort([&ValidExtensions](const FString& A, const FString& B)
			{
				return ValidExtensions.IndexOfByKey(FPaths::GetExtension(A, true)) < ValidExtensions.IndexOfByKey(FPaths::GetExtension(B, true));
			});

			for (const FString& CookedFile : CookedFiles)
			{
				TStringBuilder<256> Line;
				Line.Appendf(TEXT("\"%s\" %i\n"), *CookedFile, CurrentIndex++);
				CookerFileOrderString.Append(Line);
			}
		}
	}

	if (CookerFileOrderString.Len())
	{
		TStringBuilder<256> OpenOrderFilename;
		if (FDataDrivenPlatformInfoRegistry::GetPlatformInfo(TargetPlatform->PlatformName()).bIsConfidential)
		{
			OpenOrderFilename.Appendf(TEXT("%sPlatforms/%s/Build/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *TargetPlatform->PlatformName());
		}
		else
		{
			OpenOrderFilename.Appendf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *TargetPlatform->PlatformName());
		}
		FFileHelper::SaveStringToFile(CookerFileOrderString, *OpenOrderFilename);
	}

	return true;
}

bool FAssetRegistryGenerator::GetPackageDependencyChain(FName SourcePackage, FName TargetPackage, TSet<FName>& VisitedPackages, TArray<FName>& OutDependencyChain)
{	
	//avoid crashing from circular dependencies.
	if (VisitedPackages.Contains(SourcePackage))
	{		
		return false;
	}
	VisitedPackages.Add(SourcePackage);

	if (SourcePackage == TargetPackage)
	{		
		OutDependencyChain.Add(SourcePackage);
		return true;
	}

	TArray<FName> SourceDependencies;
	if (GetPackageDependencies(SourcePackage, SourceDependencies, DependencyQuery) == false)
	{		
		return false;
	}

	int32 DependencyCounter = 0;
	while (DependencyCounter < SourceDependencies.Num())
	{		
		const FName& ChildPackageName = SourceDependencies[DependencyCounter];
		if (GetPackageDependencyChain(ChildPackageName, TargetPackage, VisitedPackages, OutDependencyChain))
		{
			OutDependencyChain.Add(SourcePackage);
			return true;
		}
		++DependencyCounter;
	}
	
	return false;
}

bool FAssetRegistryGenerator::GetPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames, UE::AssetRegistry::EDependencyQuery InDependencyQuery)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FGetPackageDependenciesForManifestGeneratorDelegate& GetPackageDependenciesForManifestGeneratorDelegate = FGameDelegates::Get().GetGetPackageDependenciesForManifestGeneratorDelegate();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (GetPackageDependenciesForManifestGeneratorDelegate.IsBound())
	{
		uint8 DependencyType = 0;
		DependencyType |= (uint8)(!!(InDependencyQuery & UE::AssetRegistry::EDependencyQuery::Soft) ? EAssetRegistryDependencyType::None : EAssetRegistryDependencyType::Hard);
		DependencyType |= (uint8)(!!(InDependencyQuery & UE::AssetRegistry::EDependencyQuery::Hard) ? EAssetRegistryDependencyType::None : EAssetRegistryDependencyType::Soft);
		return GetPackageDependenciesForManifestGeneratorDelegate.Execute(PackageName, DependentPackageNames, DependencyType);
	}
	else
	{
		return AssetRegistry.GetDependencies(PackageName, DependentPackageNames, UE::AssetRegistry::EDependencyCategory::Package, InDependencyQuery);
	}
}

bool FAssetRegistryGenerator::GatherAllPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames)
{	
	if (GetPackageDependencies(PackageName, DependentPackageNames, DependencyQuery) == false)
	{
		return false;
	}

	TSet<FName> VisitedPackages;
	VisitedPackages.Append(DependentPackageNames);

	int32 DependencyCounter = 0;
	while (DependencyCounter < DependentPackageNames.Num())
	{
		const FName& ChildPackageName = DependentPackageNames[DependencyCounter];
		++DependencyCounter;
		TArray<FName> ChildDependentPackageNames;
		if (GetPackageDependencies(ChildPackageName, ChildDependentPackageNames, DependencyQuery) == false)
		{
			return false;
		}

		for (const auto& ChildDependentPackageName : ChildDependentPackageNames)
		{
			if (!VisitedPackages.Contains(ChildDependentPackageName))
			{
				DependentPackageNames.Add(ChildDependentPackageName);
				VisitedPackages.Add(ChildDependentPackageName);
			}
		}
	}

	return true;
}

bool FAssetRegistryGenerator::GenerateAssetChunkInformationCSV(const FString& OutputPath, bool bWriteIndividualFiles)
{
	FString TmpString, TmpStringChunks;
	ANSICHAR HeaderText[] = "ChunkID, Package Name, Class Type, Hard or Soft Chunk, File Size, Other Chunks\n";

	TArray<const FAssetData*> AssetDataList;
	State.EnumerateAllAssets([&AssetDataList](const FAssetData& AssetData)
	{
		AssetDataList.Add(&AssetData);
	});

	// Sort list so it's consistent over time
	AssetDataList.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath());
	});

	// Create file for all chunks
	TUniquePtr<FArchive> AllChunksFile(IFileManager::Get().CreateFileWriter(*FPaths::Combine(*OutputPath, TEXT("AllChunksInfo.csv"))));
	if (!AllChunksFile.IsValid())
	{
		return false;
	}

	AllChunksFile->Serialize(HeaderText, sizeof(HeaderText)-1);

	// Create file for each chunk if needed
	TArray<TUniquePtr<FArchive>> ChunkFiles;
	if (bWriteIndividualFiles)
	{
		for (int32 PakchunkIndex = 0; PakchunkIndex < FinalChunkManifests.Num(); ++PakchunkIndex)
		{
			FArchive* ChunkFile = IFileManager::Get().CreateFileWriter(*FPaths::Combine(*OutputPath, *FString::Printf(TEXT("Chunks%dInfo.csv"), PakchunkIndex)));
			if (ChunkFile == nullptr)
			{
				return false;
			}
			ChunkFile->Serialize(HeaderText, sizeof(HeaderText)-1);
			ChunkFiles.Add(TUniquePtr<FArchive>(ChunkFile));
		}
	}

	for (const FAssetData* AssetDataPtr : AssetDataList)
	{
		const FAssetData& AssetData = *AssetDataPtr;
		const FAssetPackageData* PackageData = State.GetAssetPackageData(AssetData.PackageName);

		// Add only assets that have actually been cooked and belong to any chunk and that have a file size
		const FAssetData::FChunkArrayView ChunkIDs = AssetData.GetChunkIDs();
		if (PackageData != nullptr && ChunkIDs.Num() > 0 && PackageData->DiskSize > 0)
		{
			for (int32 PakchunkIndex : ChunkIDs)
			{
				const int64 FileSize = PackageData->DiskSize;
				FString SoftChain;
				bool bHardChunk = false;
				if (PakchunkIndex < ChunkManifests.Num())
				{
					bHardChunk = ChunkManifests[PakchunkIndex] && ChunkManifests[PakchunkIndex]->Contains(AssetData.PackageName);

					if (!bHardChunk)
					{
						SoftChain = GetShortestReferenceChain(AssetData.PackageName, PakchunkIndex);
					}
				}
				if (SoftChain.IsEmpty())
				{
					SoftChain = TEXT("Soft: Possibly Unassigned Asset");
				}

				// Build "other chunks" string or None if not part of
				TmpStringChunks.Empty(64);
				for (int32 OtherChunk : ChunkIDs)
				{
					if (OtherChunk != PakchunkIndex)
					{
						TmpString = FString::Printf(TEXT("%d "), OtherChunk);
					}
				}

				// Build csv line
				TmpString = FString::Printf(TEXT("%d,%s,%s,%s,%lld,%s\n"),
					PakchunkIndex,
					*AssetData.PackageName.ToString(),
					*AssetData.AssetClassPath.ToString(),
					bHardChunk ? TEXT("Hard") : *SoftChain,
					FileSize,
					ChunkIDs.Num() == 1 ? TEXT("None") : *TmpStringChunks
				);

				// Write line to all chunks file and individual chunks files if requested
				{
					auto Src = StringCast<ANSICHAR>(*TmpString, TmpString.Len());
					AllChunksFile->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
					
					if (bWriteIndividualFiles)
					{
						ChunkFiles[PakchunkIndex]->Serialize((ANSICHAR*)Src.Get(), Src.Length() * sizeof(ANSICHAR));
					}
				}
			}
		}
	}

	return true;
}

void FAssetRegistryGenerator::AddPackageToManifest(const FString& PackageSandboxPath, FName PackageName, int32 ChunkId)
{
	HighestChunkId = ChunkId > HighestChunkId ? ChunkId : HighestChunkId;
	int32 PakchunkIndex = GetPakchunkIndex(ChunkId);

	if (PakchunkIndex >= ChunkManifests.Num())
	{
		ChunkManifests.AddDefaulted(PakchunkIndex - ChunkManifests.Num() + 1);
	}
	if (!ChunkManifests[PakchunkIndex])
	{
		ChunkManifests[PakchunkIndex].Reset(new FChunkPackageSet());
	}
	ChunkManifests[PakchunkIndex]->Add(PackageName, PackageSandboxPath);
	// Now that the package has been assigned to a chunk, remove it from the unassigned set.
	UnassignedPackageSet.Remove(PackageName);
}


void FAssetRegistryGenerator::RemovePackageFromManifest(FName PackageName, int32 ChunkId)
{
	int32 PakchunkIndex = GetPakchunkIndex(ChunkId);

	if (ChunkManifests[PakchunkIndex])
	{
		ChunkManifests[PakchunkIndex]->Remove(PackageName);
	}
}

void FAssetRegistryGenerator::SubtractParentChunkPackagesFromChildChunks(const FChunkDependencyTreeNode& Node,
	const TSet<FName>& CumulativeParentPackages, TArray<TArray<FName>>& OutPackagesMovedBetweenChunks)
{
	if (FinalChunkManifests.Num() <= Node.ChunkID || !FinalChunkManifests[Node.ChunkID])
	{
		return;
	}
	FChunkPackageSet& NodeManifest = *FinalChunkManifests[Node.ChunkID];
	for (FName PackageName : CumulativeParentPackages)
	{
		// Remove any assets belonging to our parents.
		if (NodeManifest.Remove(PackageName) > 0)
		{
			OutPackagesMovedBetweenChunks[Node.ChunkID].Add(PackageName);
			UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("Removed %s from chunk %i because it is duplicated in another chunk."),
				*PackageName.ToString(), Node.ChunkID);
		}
	}

	if (!Node.ChildNodes.Num())
	{
		return;
	}

	// Add the current Chunk's assets
	TSet<FName> CumulativePackages;
	if (!NodeManifest.IsEmpty())
	{
		CumulativePackages.Reserve(CumulativeParentPackages.Num() + NodeManifest.Num());
		CumulativePackages.Append(CumulativeParentPackages);
		for (const TPair<FName, FString>& Pair : NodeManifest)
		{
			CumulativePackages.Add(Pair.Key);
		}
	}

	const TSet<FName>& NewRecursiveParentPackages = CumulativePackages.Num() ? CumulativePackages : CumulativeParentPackages;
	for (const FChunkDependencyTreeNode& ChildNode : Node.ChildNodes)
	{
		SubtractParentChunkPackagesFromChildChunks(ChildNode, NewRecursiveParentPackages, OutPackagesMovedBetweenChunks);
	}
}

bool FAssetRegistryGenerator::CheckChunkAssetsAreNotInChild(const FChunkDependencyTreeNode& Node)
{
	for (const FChunkDependencyTreeNode& ChildNode : Node.ChildNodes)
	{
		if (!CheckChunkAssetsAreNotInChild(ChildNode))
		{
			return false;
		}
	}

	if (!(FinalChunkManifests.Num() > Node.ChunkID && FinalChunkManifests[Node.ChunkID]))
	{
		return true;
	}

	FChunkPackageSet& ParentManifest = *FinalChunkManifests[Node.ChunkID];
	for (const FChunkDependencyTreeNode& ChildNode : Node.ChildNodes)
	{
		if (FinalChunkManifests.Num() > ChildNode.ChunkID && FinalChunkManifests[ChildNode.ChunkID])
		{
			FChunkPackageSet& ChildManifest = *FinalChunkManifests[ChildNode.ChunkID];
			for (const TPair<FName,FString>& ParentPair : ParentManifest)
			{
				if (ChildManifest.Find(ParentPair.Key))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FAssetRegistryGenerator::AddPackageAndDependenciesToChunk(FChunkPackageSet& ThisPackageSet, FName InPkgName,
	const FString& InSandboxFile, int32 PakchunkIndex, FSandboxPlatformFile& SandboxPlatformFile)
{
	FChunkPackageSet* InitialPackageSetForThisChunk =
		ChunkManifests.IsValidIndex(PakchunkIndex) ? ChunkManifests[PakchunkIndex].Get() : nullptr;

	//Add this asset
	ThisPackageSet.Add(InPkgName, InSandboxFile);

	// Only gather dependencies the slow way if we're chunking and not using asset manager
	if (!bGenerateChunks || bUseAssetManager)
	{
		return;
	}

	//now add any dependencies
	TArray<FName> DependentPackageNames;
	if (GatherAllPackageDependencies(InPkgName, DependentPackageNames))
	{
		for (const auto& PkgName : DependentPackageNames)
		{
			bool bSkip = false;
			if (PakchunkIndex != 0 && FinalChunkManifests[0])
			{
				// Do not add if this asset was assigned to the 0 chunk. These assets always exist on disk
				bSkip = FinalChunkManifests[0]->Contains(PkgName);
			}
			if (!bSkip)
			{
				const FName FilteredPackageName = GetPackageNameFromDependencyPackageName(PkgName);
				if (FilteredPackageName == NAME_None)
				{
					continue;
				}
				FString DependentSandboxFile = SandboxPlatformFile.ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(*FilteredPackageName.ToString()));
				if (!ThisPackageSet.Contains(FilteredPackageName))
				{
					if ((InitialPackageSetForThisChunk != nullptr) && InitialPackageSetForThisChunk->Contains(PkgName))
					{
						// Don't print anything out; it was pre-assigned to this chunk but we haven't gotten to it yet in the calling loop; we'll go ahead and grab it now
					}
					else
					{
						if (UE_LOG_ACTIVE(LogAssetRegistryGenerator, Verbose))
						{
							// It was not assigned to this chunk and we're forcing it to be dragged in, let the user known
							UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("Adding %s to chunk %i because %s depends on it."), *FilteredPackageName.ToString(), PakchunkIndex, *InPkgName.ToString());

							TSet<FName> VisitedPackages;
							TArray<FName> DependencyChain;
							GetPackageDependencyChain(InPkgName, PkgName, VisitedPackages, DependencyChain);
							for (const auto& ChainName : DependencyChain)
							{
								UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("\tchain: %s"), *ChainName.ToString());
							}
						}
					}
				}
				ThisPackageSet.Add(FilteredPackageName, DependentSandboxFile);
				UnassignedPackageSet.Remove(PkgName);
			}
		}
	}
}

void FAssetRegistryGenerator::FixupPackageDependenciesForChunks(FSandboxPlatformFile& InSandboxFile)
{
	UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Starting FixupPackageDependenciesForChunks..."));
	SCOPE_LOG_TIME_IN_SECONDS(TEXT("... FixupPackageDependenciesForChunks complete."), nullptr);

	// Clear any existing manifests from the final array
	FinalChunkManifests.Empty(ChunkManifests.Num());

	for (int32 PakchunkIndex = 0, MaxPakchunk = ChunkManifests.Num(); PakchunkIndex < MaxPakchunk; ++PakchunkIndex)
	{
		FChunkPackageSet& FinalManifest = *FinalChunkManifests.Emplace_GetRef(new FChunkPackageSet());
		if (!ChunkManifests[PakchunkIndex])
		{
			continue;
		}
		
		for (const TPair<FName,FString>& Pair : *ChunkManifests[PakchunkIndex])
		{
			AddPackageAndDependenciesToChunk(FinalManifest, Pair.Key, Pair.Value, PakchunkIndex, InSandboxFile);
		}
	}

	FConfigFile PlatformIniFile;
	FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Engine"), true, *TargetPlatform->IniPlatformName());
	bool bSkipResolveChunkDependencyGraph = false;
	PlatformIniFile.GetBool(TEXT("Script/UnrealEd.ChunkDependencyInfo"), TEXT("bSkipResolveChunkDependencyGraph"), bSkipResolveChunkDependencyGraph);

	const FChunkDependencyTreeNode* ChunkDepGraph = DependencyInfo.GetOrBuildChunkDependencyGraph(!bSkipResolveChunkDependencyGraph ? HighestChunkId : 0);

	//Once complete, Add any remaining assets (that are not assigned to a chunk) to the first chunk.
	if (FinalChunkManifests.Num() == 0)
	{
		FinalChunkManifests.Emplace(new FChunkPackageSet());
	}
	FChunkPackageSet& Chunk0Manifest = *FinalChunkManifests[0];
	
	// Copy the remaining assets
	FChunkPackageSet RemainingAssets = UnassignedPackageSet; // Loop removes elements from UnassignedPackageSet
	for (const TPair<FName,FString>& Pair : RemainingAssets)
	{
		AddPackageAndDependenciesToChunk(Chunk0Manifest, Pair.Key, Pair.Value, 0, InSandboxFile);
	}

	if (!CheckChunkAssetsAreNotInChild(*ChunkDepGraph))
	{
		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Initial scan of chunks found duplicate assets in graph children"));
	}
		
	TArray<TArray<FName>> PackagesRemovedFromChunks;
	PackagesRemovedFromChunks.AddDefaulted(ChunkManifests.Num());

	// Recursively remove child chunk's redundant copies of parent chunk's packages.
	// This has to be done after all remaining assets were added to chunk 0 above, since all chunks
	// are children of chunk 0.
	SubtractParentChunkPackagesFromChildChunks(*ChunkDepGraph, TSet<FName>(), PackagesRemovedFromChunks);

	if (!bUseAssetManager)
	{
		for (int32 PakchunkIndex = 0; PakchunkIndex < ChunkManifests.Num(); ++PakchunkIndex)
		{
			FName CollectionName(*FString::Printf(TEXT("PackagesRemovedFromChunk%i"), PakchunkIndex));
			if (CreateOrEmptyCollection(CollectionName))
			{
				WriteCollection(CollectionName, PackagesRemovedFromChunks[PakchunkIndex]);
			}
		}
	}

	for (int32 PakchunkIndex = 0, MaxPakchunk = ChunkManifests.Num(); PakchunkIndex < MaxPakchunk; ++PakchunkIndex)
	{
		const int32 ChunkManifestNum = ChunkManifests[PakchunkIndex] ? ChunkManifests[PakchunkIndex]->Num() : 0;
		check(PakchunkIndex < FinalChunkManifests.Num() && FinalChunkManifests[PakchunkIndex]);
		const int32 FinalChunkManifestNum = FinalChunkManifests[PakchunkIndex]->Num();
		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Chunk: %i, Started with %i packages, Final after dependency resolve: %i"),
			PakchunkIndex, ChunkManifestNum, FinalChunkManifestNum);
	}
	
	// Fix up the data in the FAssetRegistryState to reflect this chunk layout
	for (int32 PakchunkIndex = 0 ; PakchunkIndex < FinalChunkManifests.Num(); ++PakchunkIndex)
	{
		check(FinalChunkManifests[PakchunkIndex]);
		for (const TPair<FName, FString>& Asset : *FinalChunkManifests[PakchunkIndex])
		{
			for (const FAssetData* AssetData : State.GetAssetsByPackageName(Asset.Key))
			{
				// Chunk Ids are safe to modify in place
				const_cast<FAssetData*>(AssetData)->AddChunkID(PakchunkIndex);
			}
		}
	}
}

void FAssetRegistryGenerator::FindShortestReferenceChain(TArray<FReferencePair> PackageNames, int32 PakchunkIndex, uint32& OutParentIndex, FString& OutChainPath)
{
	TArray<FReferencePair> ReferencesToCheck;
	uint32 Index = 0;
	for (const auto& Pkg : PackageNames)
	{
		if (ChunkManifests[PakchunkIndex] && ChunkManifests[PakchunkIndex]->Contains(Pkg.PackageName))
		{
			OutChainPath += TEXT("Soft: ");
			OutChainPath += Pkg.PackageName.ToString();
			OutParentIndex = Pkg.ParentNodeIndex;
			return;
		}
		TArray<FName> AssetReferences;
		AssetRegistry.GetReferencers(Pkg.PackageName, AssetReferences);
		for (const auto& Ref : AssetReferences)
		{
			if (!InspectedNames.Contains(Ref))
			{
				ReferencesToCheck.Add(FReferencePair(Ref, Index));
				InspectedNames.Add(Ref);
			}
		}

		++Index;
	}

	if (ReferencesToCheck.Num() > 0)
	{
		uint32 ParentIndex = INDEX_NONE;
		FindShortestReferenceChain(ReferencesToCheck, PakchunkIndex, ParentIndex, OutChainPath);

		if (ParentIndex < (uint32)PackageNames.Num())
		{
			OutChainPath += TEXT("->");
			OutChainPath += PackageNames[ParentIndex].PackageName.ToString();
			OutParentIndex = PackageNames[ParentIndex].ParentNodeIndex;
		}
	}
	else if (PackageNames.Num() > 0)
	{
		//best guess
		OutChainPath += TEXT("Soft From Unassigned Package? Best Guess: ");
		OutChainPath += PackageNames[0].PackageName.ToString();
		OutParentIndex = PackageNames[0].ParentNodeIndex;
	}
}

FString FAssetRegistryGenerator::GetShortestReferenceChain(FName PackageName, int32 PakchunkIndex)
{
	FString StringChain;
	TArray<FReferencePair> ReferencesToCheck;
	uint32 ParentIndex;
	ReferencesToCheck.Add(FReferencePair(PackageName, 0));
	InspectedNames.Empty();
	InspectedNames.Add(PackageName);
	FindShortestReferenceChain(ReferencesToCheck, PakchunkIndex, ParentIndex, StringChain);

	return StringChain;
}


bool FAssetRegistryGenerator::CreateOrEmptyCollection(FName CollectionName)
{
	ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

	if (CollectionManager.CollectionExists(CollectionName, ECollectionShareType::CST_Local))
	{
		return CollectionManager.EmptyCollection(CollectionName, ECollectionShareType::CST_Local);
	}
	else if (CollectionManager.CreateCollection(CollectionName, ECollectionShareType::CST_Local, ECollectionStorageMode::Static))
	{
		return true;
	}

	return false;
}

void FAssetRegistryGenerator::WriteCollection(FName CollectionName, const TArray<FName>& PackageNames)
{
	if (CreateOrEmptyCollection(CollectionName))
	{
		TArray<FSoftObjectPath> AssetPaths;

		// Convert package names to asset names
		for (const FName& Name : PackageNames)
		{
			FString PackageName = Name.ToString();
			int32 LastPathDelimiter;
			if (PackageName.FindLastChar(TEXT('/'), /*out*/ LastPathDelimiter))
			{
				const FString AssetName = PackageName.Mid(LastPathDelimiter + 1);
				PackageName = PackageName + FString(TEXT(".")) + AssetName;
			}
			AssetPaths.Add(FSoftObjectPath(PackageName));
		}

		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		CollectionManager.AddToCollection(CollectionName, ECollectionShareType::CST_Local, AssetPaths);

		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Updated collection %s"), *CollectionName.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistryGenerator, Warning, TEXT("Failed to update collection %s"), *CollectionName.ToString());
	}
}

int32 FAssetRegistryGenerator::GetPakchunkIndex(int32 ChunkId) const
{
	const int32* PakChunkId = ChunkIdPakchunkIndexMapping.Find(ChunkId);
	if (PakChunkId)
	{
		check(*PakChunkId >= 0);
		return *PakChunkId;
	}

	return ChunkId;
}

void FAssetRegistryGenerator::GetChunkAssignments(TArray<TSet<FName>>& OutAssignments) const
{
	if (ChunkManifests.Num())
	{
		// chunk 0 is special as it also contains startup packages
		TSet<FName> PackagesInChunk0;

		for(const FName& Package : StartupPackages)
		{
			PackagesInChunk0.Add(Package);
		}

		if (ChunkManifests[0])
		{
			for (const TPair<FName,FString>& Pair: *ChunkManifests[0])
			{
				PackagesInChunk0.Add(Pair.Key);
			}
		}
		OutAssignments.Add(PackagesInChunk0);

		for (uint32 ChunkIndex = 1, MaxChunk = ChunkManifests.Num(); ChunkIndex < MaxChunk; ++ChunkIndex)
		{
			TSet<FName> PackagesInChunk;
			if (ChunkManifests[ChunkIndex])
			{
				for (const TPair<FName,FString>& Pair : *ChunkManifests[ChunkIndex])
				{
					PackagesInChunk.Add(Pair.Key);
				}
			}

			OutAssignments.Add(PackagesInChunk);
		}
	}
}

FAssetRegistryGenerator::FCreateOrFindArray FAssetRegistryGenerator::CreateOrFindAssetDatas(const UPackage& Package)
{
	FCreateOrFindArray OutputAssets;

	ForEachObjectWithOuter(&Package, [&OutputAssets, this](UObject* const Object)
	{
		if (Object->IsAsset())
		{
			OutputAssets.Add(CreateOrFindAssetData(*Object));
		}
	}, /*bIncludeNestedObjects*/ false);

	return OutputAssets;
}

const FAssetData* FAssetRegistryGenerator::CreateOrFindAssetData(UObject& Object)
{
	const FAssetData* const AssetData = State.GetAssetByObjectPath(FSoftObjectPath(&Object));
	if (!AssetData)
	{
		FAssetData* const NewAssetData = new FAssetData(&Object, true /* bAllowBlueprintClass */);
		State.AddAssetData(NewAssetData);
		return NewAssetData;
	}
	return AssetData;
}

void FAssetRegistryGenerator::UpdateAssetRegistryData(const UPackage& Package,
	FSavePackageResultStruct& SavePackageResult,
	FCookTagList&& InArchiveCookTagList
	)
{
	const FName PackageName = Package.GetFName();
	PreviousPackagesToUpdate.Remove(PackageName);

	uint32 NewPackageFlags = 0;
	FAssetPackageData* AssetPackageData = GetAssetPackageData(PackageName);
	bool bSaveSucceeded = SavePackageResult.IsSuccessful();

	// Copy latest data for all Assets in the package into the cooked registry. This should be done even
	// if not successful so that editor-only packages are recorded as well
	TArray<FAssetData> AssetDatas;
	constexpr bool bIncludeOnlyDiskAssets = true; // Enumerating memory assets is unnecessary; we waited on AR to update
	AssetRegistry.GetAssetsByPackageName(PackageName, AssetDatas, bIncludeOnlyDiskAssets,
		false /* SkipARFilteredAssets */);
	for (FAssetData& AssetData : AssetDatas)
	{
		State.UpdateAssetData(MoveTemp(AssetData), true /* bCreateIfNotExists */);
	}
	// Create a record for assets that were created during PostLoad or cooking and are not in the global assetregistry
	CreateOrFindAssetDatas(Package);

	if (bSaveSucceeded)
	{
		// Migrate cook tags over
		CookTagsToAdd.Append(MoveTemp(InArchiveCookTagList.ObjectToTags));
		InArchiveCookTagList.Reset();

		// Set the PackageFlags to the recorded value from SavePackage
		NewPackageFlags = SavePackageResult.SerializedPackageFlags;

		AssetPackageData->DiskSize = SavePackageResult.TotalFileSize;

		// The CookedHash is assigned during CookByTheBookFinished		
	}
	else
	{
		// Set the package flags to zero to indicate that the package failed to save
		NewPackageFlags = 0;

		// Set DiskSize (previous value was disksize in the WorkspaceDomain) to -1 to indicate the cooked file does not exist
		AssetPackageData->DiskSize = -1;
	}

	const bool bUpdated = UpdateAssetPackageFlags(PackageName, NewPackageFlags);
	UE_CLOG(!bUpdated && bSaveSucceeded, LogAssetRegistryGenerator, Warning,
		TEXT("Trying to update asset package flags in package '%s' that does not exist"), *PackageName.ToString());
}

void FAssetRegistryGenerator::UpdateAssetRegistryData(UE::Cook::FPackageData& PackageData, UE::Cook::FAssetRegistryPackageMessage&& Message)
{
	const FName PackageName = PackageData.GetPackageName();
	PreviousPackagesToUpdate.Remove(PackageName);

	for (FAssetData& AssetData : Message.AssetDatas)
	{
		AssetData.PackageFlags = Message.PackageFlags;
		State.UpdateAssetData(MoveTemp(AssetData), true /* bCreateIfNotExists */);
	}
	FAssetPackageData* AssetPackageData = GetAssetPackageData(PackageName);
	AssetPackageData->DiskSize = Message.DiskSize;

	CookTagsToAdd.Append(MoveTemp(Message.CookTags));
}

namespace UE::Cook
{

FAssetRegistryReporterRemote::FAssetRegistryReporterRemote(FCookWorkerClient& InClient, const ITargetPlatform* InTargetPlatform)
	: Client(InClient)
	, TargetPlatform(InTargetPlatform)
{
}

void FAssetRegistryReporterRemote::UpdateAssetRegistryData(FPackageData& PackageData, const UPackage& Package,
	FSavePackageResultStruct& SavePackageResult, FCookTagList&& InArchiveCookTagList)
{
	uint32 NewPackageFlags = 0;
	int64 DiskSize = -1;
	bool bSaveSucceeded = SavePackageResult.IsSuccessful();
	if (bSaveSucceeded)
	{
		NewPackageFlags = SavePackageResult.SerializedPackageFlags;
		DiskSize = SavePackageResult.TotalFileSize;
	}
	else
	{
		// Set the package flags to zero to indicate that the package failed to save
		NewPackageFlags = 0;
		// Set DiskSize (previous value was disksize in the WorkspaceDomain) to -1 to indicate the cooked file does not exist
		DiskSize = -1;
	}

	FAssetRegistryPackageMessage Message;
	Message.PackageFlags = NewPackageFlags;
	Message.DiskSize = DiskSize;

	// Add to the message all the AssetDatas in the package from the global AssetRegistry
	constexpr bool bIncludeOnlyDiskAssets = true; // Enumerating memory assets is unnecessary; we waited on AR to update
	IAssetRegistry::Get()->GetAssetsByPackageName(PackageData.GetPackageName(), Message.AssetDatas,
		bIncludeOnlyDiskAssets, false /* SkipARFilteredAssets */);

	// Also add AssetDatas for any assets that were created during PostLoad or cooking and are not in the global assetregistry
	TSet<FName> ExistingAssets;
	for (const FAssetData& AssetData : Message.AssetDatas)
	{
		ExistingAssets.Add(AssetData.AssetName);
	}
	ForEachObjectWithOuter(&Package, [&Message, &ExistingAssets](UObject* const Object)
		{
			if (Object->IsAsset())
			{
				FName AssetName = Object->GetFName();
				if (!ExistingAssets.Contains(AssetName))
				{
					Message.AssetDatas.Emplace(Object, true /* bAllowBlueprintClass */);
				}
			}
		}, /*bIncludeNestedObjects*/ false);

	// Add the cooktags that were recorded during serialization
	Message.CookTags = MoveTemp(InArchiveCookTagList.ObjectToTags);

	// Send the message to the director
	PackageData.GetOrAddPackageRemoteResult().AddMessage(PackageData, TargetPlatform, Message);
}

void FAssetRegistryPackageMessage::Write(FCbWriter& Writer, const FPackageData& PackageData, const ITargetPlatform* TargetPlatform) const
{
	Writer.BeginArray("A");
	for (const FAssetData& AssetData : AssetDatas)
	{
		check(AssetData.PackageName == PackageData.GetPackageName() && !AssetData.AssetName.IsNone()); // We replicate only regular Assets of the form PackageName.AssetName
		AssetData.NetworkWrite(Writer, false /* bWritePackageName */);

	}
	Writer.EndArray();
	// We cast TMap<FSoftObjectPath,                     ValueType>
	//      to TMap<FSoftObjectPathSerializationWrapper, ValueType>
	// to workaround FSoftObjectPath's implicit constructor. See comment in CompactBinaryTCP.h
	static_assert(ARE_TYPES_EQUAL(decltype(CookTags)::KeyType, FSoftObjectPath), "Expected KeyType of CookTags to be FSoftObjectPath");
	Writer << "T" << reinterpret_cast<const TMap<FSoftObjectPathSerializationWrapper, decltype(CookTags)::ValueType>&>(CookTags);
	Writer << "F" << PackageFlags;
	Writer << "S" << DiskSize;
}

bool FAssetRegistryPackageMessage::TryRead(FCbObject&& Object, FPackageData& PackageData, const ITargetPlatform* TargetPlatform)
{
	FCbFieldView AssetDatasField = Object["A"];
	FCbArrayView AssetDatasArray = AssetDatasField.AsArrayView();
	if (AssetDatasField.HasError())
	{
		return false;
	}
	AssetDatas.Reset(AssetDatasArray.Num());
	for (FCbFieldView ElementField : AssetDatasArray)
	{
		FAssetData& AssetData = AssetDatas.Emplace_GetRef();
		if (!AssetData.TryNetworkRead(ElementField, false /* bReadPackageName */, PackageData.GetPackageName()))
		{
			return false;
		}
	}

	static_assert(ARE_TYPES_EQUAL(decltype(CookTags)::KeyType, FSoftObjectPath), "Expected KeyType of CookTags to be FSoftObjectPath");
	if (!LoadFromCompactBinary(Object["T"], reinterpret_cast<TMap<FSoftObjectPathSerializationWrapper, decltype(CookTags)::ValueType>&>(CookTags)))
	{
		return false;
	}
	if (!LoadFromCompactBinary(Object["F"], PackageFlags))
	{
		return false;
	}
	if (!LoadFromCompactBinary(Object["S"], DiskSize))
	{
		return false;
	}
	return true;
}

FGuid FAssetRegistryPackageMessage::MessageType(TEXT("0588DCCEBF1742399EC1E011FC97E4DC"));

}

bool FAssetRegistryGenerator::UpdateAssetPackageFlags(const FName& PackageName, const uint32 PackageFlags)
{
	return State.UpdateAssetDataPackageFlags(PackageName, PackageFlags);
}

void FAssetRegistryGenerator::InitializeChunkIdPakchunkIndexMapping()
{
	FConfigFile PlatformIniFile;
	FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *TargetPlatform->IniPlatformName());
	TArray<FString> ChunkMapping;
	PlatformIniFile.GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("ChunkIdPakchunkIndexMapping"), ChunkMapping);

	FPlatformMisc::ParseChunkIdPakchunkIndexMapping(ChunkMapping, ChunkIdPakchunkIndexMapping);

	// Validate ChunkIdPakchunkIndexMapping
	TArray<int32> AllChunkIDs;
	ChunkIdPakchunkIndexMapping.GetKeys(AllChunkIDs);
	for (int32 ChunkID : AllChunkIDs)
	{
		if(UAssetManager::Get().GetChunkEncryptionKeyGuid(ChunkID).IsValid()
			|| UAssetManager::Get().GetUniqueAssetRegistryName(ChunkID) != NAME_None)
		{
			UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Chunks with encryption key guid or unique assetregistry name (Chunk %d) can not be mapped with ChunkIdPakchunkIndexMapping.  Mapping is removed."), ChunkID);
			ChunkIdPakchunkIndexMapping.Remove(ChunkID);
		}
	}
}
#undef LOCTEXT_NAMESPACE
