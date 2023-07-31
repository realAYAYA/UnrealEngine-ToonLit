// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateNaniteDisplacedMeshCommandlet.h"

#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "NaniteDisplacedMesh.h"
#include "NaniteDisplacedMeshLog.h"
#include "NaniteDisplacedMeshEditorModule.h"
#include "NaniteDisplacedMeshFactory.h"
#include "PackageSourceControlHelper.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Level.h"
#include "HAL/FileManager.h"
#include "UObject/GCObjectScopeGuard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenerateNaniteDisplacedMeshCommandlet)

bool UGenerateNaniteDisplacedMeshCommandlet::IsRunning()
{
	static UClass* ThisCommandlet = StaticClass();
	static UClass* RunningCommandlet = GetRunningCommandletClass();
	return RunningCommandlet != nullptr && RunningCommandlet->IsChildOf(ThisCommandlet);
}

int32 UGenerateNaniteDisplacedMeshCommandlet::Main(const FString& CmdLineParams)
{
	const uint64 StartTime = FPlatformTime::Cycles64();
	bool bSuccess = true;

	// Process the arguments
	TArray<FString> Tokens, Switches;
	TMap<FString, FString> Params;
	ParseCommandLine(*CmdLineParams, Tokens, Switches, Params);
	const FString CollectionFilter = Params.FindRef(TEXT("GNDMCollectionFilter"));
	const FString SubmitWithDescription = Params.FindRef(TEXT("GNDMSubmitWithDescription"));
	const bool bDeleteUnused = Switches.Contains(TEXT("GNDMDeleteUnused"));

	FPackageSourceControlHelper SourceControlHelper;
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Source control enabled: %s"), SourceControlHelper.UseSourceControl() ? TEXT("true") : TEXT("false"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bIncludeOnlyOnDiskAssets = true;
	if (!CollectionFilter.IsEmpty())
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Use collection filter: %s"), *CollectionFilter);
		const ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		CollectionManager.GetObjectsInCollection(FName(*CollectionFilter), ECollectionShareType::CST_All, Filter.SoftObjectPaths, ECollectionRecursionFlags::SelfAndChildren);
	}

	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

	if (IsRunningCommandlet())
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Searching all assets (this may take a while)..."));
		// This is automatically called in the regular editor but not when running a commandlet (unless cooking)
		// Must also search synchronously because AssetRegistry.IsLoadingAssets() won't account for this search
		AssetRegistry.SearchAllAssets(true);
	}

	TArray<FAssetData> LevelAssets;
	AssetRegistry.GetAssets(Filter, LevelAssets);

	// Add dependencies of the levels to list of item that also need to be processed even if they are not in the original request list.
	TArray<FAssetData> LevelAssetsAddedByDependencies;
	{
		TSet<FName> StopSearchAt;
		StopSearchAt.Reserve(LevelAssets.Num());
		for (const FAssetData& LevelAsset : LevelAssets)
		{
			StopSearchAt.Add(LevelAsset.PackageName);
		}
		
		TSet<FName> DependenciesToProcess;
		TArray<FName> CurrentDependencies;
		UE::AssetRegistry::FDependencyQuery QueryFlags;
		QueryFlags.Required = UE::AssetRegistry::EDependencyProperty::Game;
		for (const FAssetData& LevelAsset : LevelAssets)
		{
			// Get the dependencies of the level recursively
			AssetRegistry.GetDependencies(LevelAsset.PackageName, CurrentDependencies, UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
			while (!CurrentDependencies.IsEmpty())
			{
				const bool bAllowShrinking = false;
				FName DependendPackageName = CurrentDependencies.Pop(bAllowShrinking);
				if (!StopSearchAt.Contains(DependendPackageName))
				{
					StopSearchAt.Add(DependendPackageName);
					DependenciesToProcess.Add(DependendPackageName);
					AssetRegistry.GetDependencies(DependendPackageName, CurrentDependencies, UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
				}
			}
		}

		FARFilter DependenciesFilter;
		DependenciesFilter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
		DependenciesFilter.bIncludeOnlyOnDiskAssets = true;
		DependenciesFilter.PackageNames = DependenciesToProcess.Array();
		AssetRegistry.GetAssets(DependenciesFilter, LevelAssetsAddedByDependencies);
	}

	FNaniteDisplacedMeshEditorModule& Module = FNaniteDisplacedMeshEditorModule::GetModule();
	Module.OnLinkDisplacedMeshOverride.BindUObject(this, &UGenerateNaniteDisplacedMeshCommandlet::OnLinkDisplacedMesh);

	const int32 LevelCount = LevelAssets.Num() + LevelAssetsAddedByDependencies.Num();
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Processing %d level(s)..."), LevelCount);

	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Processing %d level(s) from the original query..."), LevelAssets.Num());
	uint32 LevelIndex = 0;
	for (const FAssetData& LevelAsset : LevelAssets)
	{
		++LevelIndex;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Process Level: %s (%d/%d)"), *LevelAsset.GetSoftObjectPath().ToString(), LevelIndex + 1, LevelCount);
		LoadLevel(LevelAsset);
	}

	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Processing the %d dependencies of the level(s) ..."), LevelAssetsAddedByDependencies.Num());

	for (const FAssetData& LevelAsset : LevelAssetsAddedByDependencies)
	{
		++LevelIndex;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Process Level: %s (%d/%d)"), *LevelAsset.GetSoftObjectPath().ToString(), LevelIndex + 1, LevelCount);
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("This level was added because it's a dependency of one of the processed level(s)"));
		LoadLevel(LevelAsset);
	}

	Module.OnLinkDisplacedMeshOverride.Unbind();
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("======================= All levels processed ======================"));
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Linked %d unique mesh(es)"), LinkedPackageNames.Num());

	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));

	if (LinkedPackageFolders.Num() > 0)
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Find existing meshes (with prefix %s) in %d folder(s):\n\t%s"), LinkedDisplacedMeshAssetNamePrefix, LinkedPackageFolders.Num(), *FString::Join(LinkedPackageFolders, TEXT("\n\t")));

		const TSet<FString> ExistingPackageNames  = GetPackagesInFolders(LinkedPackageFolders, LinkedDisplacedMeshAssetNamePrefix);
		const TSet<FString> AddedPackageNames = LinkedPackageNames.Difference(ExistingPackageNames);
		const TSet<FString> UnusedPackageNames = ExistingPackageNames.Difference(LinkedPackageNames);

		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));

		if (ExistingPackageNames.Num() > 0)
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Found %d existing mesh(es):\n\t%s"), ExistingPackageNames.Num(), *FString::Join(ExistingPackageNames, TEXT("\n\t")));
		}
		else
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("No existing meshes found"));
		}

		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));

		if (AddedPackageNames.Num() > 0)
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Found %d added mesh(es):\n\t%s"), AddedPackageNames.Num(), *FString::Join(AddedPackageNames, TEXT("\n\t")));
		}
		else
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("No added meshes found"));
		}

		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));

		if (UnusedPackageNames.Num() > 0)
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Found %d unused mesh(es):\n\t%s"), UnusedPackageNames.Num(), *FString::Join(UnusedPackageNames, TEXT("\n\t")));
		}
		else
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("No unused meshes found"));
		}

		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));

		TArray<FString> DeletedFilenames;	
		if (bDeleteUnused)
		{
			const TArray<FString> PackagesToDelete = UnusedPackageNames.Array();
			DeletedFilenames.Append(SourceControlHelpers::PackageFilenames(PackagesToDelete));

			if (PackagesToDelete.Num() > 0)
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Deleting %d unused mesh(es)..."), PackagesToDelete.Num());
				if (!SourceControlHelper.Delete(PackagesToDelete))
				{
					UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Failed to delete unused meshes!"));
					bSuccess = false;
				}
			}
			else
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("No unused meshes to delete"));
			}

			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));
		}

		if (bSuccess && !SubmitWithDescription.IsEmpty())
		{
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Use submit description: %s"), *SubmitWithDescription);

			TArray<FString> SubmitFilenames = SourceControlHelpers::PackageFilenames(AddedPackageNames.Array());
			SubmitFilenames.Append(DeletedFilenames);

			if (SubmitFilenames.Num() > 0)
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Submitting %d changed mesh(es)..."), SubmitFilenames.Num());
				if (!SourceControlHelpers::CheckInFiles(SubmitFilenames, SubmitWithDescription))
				{
					UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Failed to submit changed meshes!"));
					bSuccess = false;
				}
			}
			else
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("No changed meshes to submit"));
			}

			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));
		}
	}
	else
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("No meshes linked in processed levels"));
	}

	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Peak virtual memory usage: %u MiB"), MemoryStats.PeakUsedVirtual / (1024 * 1024));
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Peak physical memory usage: %u MiB"), MemoryStats.PeakUsedPhysical / (1024 * 1024));

	const uint64 EndTime = FPlatformTime::Cycles64();
	const int64 DurationInSeconds = FMath::CeilToInt(FPlatformTime::ToSeconds64(EndTime - StartTime));
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Commandlet duration: %d second(s)"), DurationInSeconds);

	return bSuccess ? 0 : 1;
}

UNaniteDisplacedMesh* UGenerateNaniteDisplacedMeshCommandlet::OnLinkDisplacedMesh(const FNaniteDisplacedMeshParams& Parameters, const FString& Folder)
{
	FNaniteDisplacedMeshEditorModule& Module = FNaniteDisplacedMeshEditorModule::GetModule();
	Module.OnLinkDisplacedMeshOverride.Unbind();

	// This will force the saving of a new asset as persistent and mark it for add in source control (if enabled)
	UNaniteDisplacedMesh* NaniteDisplacedMesh = LinkDisplacedMeshAsset(nullptr, Parameters, Folder, ELinkDisplacedMeshAssetSetting::LinkAgainstPersistentAsset);
	if (NaniteDisplacedMesh != nullptr)
	{
		const FString PackageName = NaniteDisplacedMesh->GetPackage()->GetPathName();
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Linked mesh: %s"), *PackageName);

		LinkedPackageNames.Add(PackageName);
		LinkedPackageFolders.Add(Folder);
	}

	Module.OnLinkDisplacedMeshOverride.BindUObject(this, &UGenerateNaniteDisplacedMeshCommandlet::OnLinkDisplacedMesh);
	return NaniteDisplacedMesh;
}

void UGenerateNaniteDisplacedMeshCommandlet::LoadLevel(const FAssetData& AssetData)
{
	if (AssetData.GetClass() != UWorld::StaticClass())
	{
		return;
	}

	UWorld* World = Cast<UWorld>(AssetData.GetAsset());
	if (World == nullptr)
	{
		return;
	}

	World->AddToRoot();

	// Load the external actors (we should look with the open world team to see if there is a better way to do this)
	if (const ULevel* PersistantLevel = World->PersistentLevel)
	{
		if (PersistantLevel->bUseExternalActors || PersistantLevel->bIsPartitioned)
		{
			const FString ExternalActorsPath = ULevel::GetExternalActorsPath(AssetData.PackageName.ToString());
			const FString ExternalActorsFilePath = FPackageName::LongPackageNameToFilename(ExternalActorsPath);

			if (IFileManager::Get().DirectoryExists(*ExternalActorsFilePath))
			{
				IFileManager::Get().IterateDirectoryRecursively(*ExternalActorsFilePath, [](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
					{
						if (!bIsDirectory)
						{
							const FString Filename(FilenameOrDirectory);
							if (Filename.EndsWith(FPackageName::GetAssetPackageExtension()))
							{
								const FString PackageName = FPackageName::FilenameToLongPackageName(*Filename);
								LoadPackage(nullptr, *Filename, LOAD_None, nullptr, nullptr);
							}
						}

						return true;
					});
			}
		}
	}

	World->RemoveFromRoot();

	CollectGarbage(RF_NoFlags);
}

TSet<FString> UGenerateNaniteDisplacedMeshCommandlet::GetPackagesInFolders(const TSet<FString>& Folders, const FString& NamePrefix)
{
	TSet<FString> PackageNames;
	const IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

	for (const FString& Folder : Folders)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPath(*Folder, Assets, true, true);

		const FString PackageNamePrefix = Folder / NamePrefix;
		for (const FAssetData& Asset : Assets)
		{
			const FString PackageName = Asset.PackageName.ToString();
			if (PackageName.StartsWith(PackageNamePrefix)) PackageNames.Add(PackageName);
		}
	}

	return MoveTemp(PackageNames);
}

namespace UE::GenerateNaniteDisplacedMesh::Private
{
	void RunCommandlet(const TArray<FString>& Args)
	{
		TArray<FString> CmdLineArgs;
		if (Args.IsValidIndex(0)) CmdLineArgs.Add(FString::Printf(TEXT("-GNDMCollectionFilter=\"%s\""), *Args[0]));
		if (Args.IsValidIndex(1) && Args[1].Equals(TEXT("true"), ESearchCase::IgnoreCase)) CmdLineArgs.Add(TEXT("-GNDMDeleteUnused"));
		if (Args.IsValidIndex(2)) CmdLineArgs.Add(FString::Printf(TEXT("-GNDMSubmitWithDescription=\"%s\""), *Args[2]));
		const FString CmdLineParams = FString::Join(CmdLineArgs, TEXT(" "));

		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Run commandlet GenerateNaniteDisplacedMesh: %s"), *CmdLineParams);

		UGenerateNaniteDisplacedMeshCommandlet* Commandlet = NewObject<UGenerateNaniteDisplacedMeshCommandlet>();
		FGCObjectScopeGuard ScopeGuard(Commandlet);
		Commandlet->Main(CmdLineParams);
	}

	static FAutoConsoleCommand ConsoleCommand = FAutoConsoleCommand(
		TEXT("GenerateNaniteDisplacedMesh"),
		TEXT("Generate nanite displacement mesh assets"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunCommandlet)
		);
}

