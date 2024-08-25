// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateNaniteDisplacedMeshCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CollectionManagerModule.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ICollectionManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "NaniteDisplacedMeshEditorModule.h"
#include "NaniteDisplacedMeshLog.h"
#include "PackageSourceControlHelper.h"
#include "String/ParseTokens.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/ObjectResource.h"
#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenerateNaniteDisplacedMeshCommandlet)

namespace UE::NaniteDisplacedMesh::Private::GenerateNaniteDisplacedMesh
{
	bool ShouldLoadLevel(const FAssetData& LevelAssetData, const TSet<FTopLevelAssetPath>& ImportToLookFor)
	{
		if (ImportToLookFor.IsEmpty())
		{
			return true;
		}

		if (ULevel::GetIsLevelUsingExternalActorsFromAsset(LevelAssetData))
		{
			return true;
		}

		/**
		 * Load_Verify tell the linker to not load the package but it will create the linker if the file exist and is valid.
		 * LOAD_NoVerify tell the linker to not check the import of the package (in the editor this will avoid loading the hard dependencies of the package)
		 * 
		 * This will create the package and allow us to check the import map without fully loading the level
		 */
		if (UPackage* Package = LoadPackage(nullptr, *LevelAssetData.PackageName.ToString(), LOAD_Verify | LOAD_NoVerify))
		{
			FLinkerLoad* LinkerLoad = Package->GetLinker();

			// Clear the load flags so that an load further down line won't be affected by the existing flags
			LinkerLoad->LoadFlags = LOAD_None;

			for (const FObjectImport& Dependency : LinkerLoad->ImportMap)
			{
				if (Dependency.OuterIndex.IsImport() && !Dependency.ObjectName.IsNone())
				{
					const FObjectImport& PackageDependency = LinkerLoad->ImportMap[Dependency.OuterIndex.ToImport()];
					if (PackageDependency.OuterIndex.IsNull())
					{
						FTopLevelAssetPath Import(PackageDependency.ObjectName, Dependency.ObjectName);

						// Low level optimization here. Only fully load the level if it import an class that might generate an displaced mesh.
						if (ImportToLookFor.Contains(Import))
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}
}


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
	const FString ActorClassPathFilterString = Params.FindRef(TEXT("GNDMActorClassPathFilter"));
	const bool bDeleteUnused = Switches.Contains(TEXT("GNDMDeleteUnused"));

	FPackageSourceControlHelper SourceControlHelper;
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Revision control enabled: %s"), SourceControlHelper.UseSourceControl() ? TEXT("true") : TEXT("false"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bIncludeOnlyOnDiskAssets = true;
	if (!CollectionFilter.IsEmpty())
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Use collection filter: %s"), *CollectionFilter);
		const ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		CollectionManager.GetObjectsInCollection(FName(*CollectionFilter), ECollectionShareType::CST_All, Filter.SoftObjectPaths, ECollectionRecursionFlags::SelfAndChildren);
	}

	TArray<FTopLevelAssetPath> ClassesPathForActor;
	if (!ActorClassPathFilterString.IsEmpty())
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Use actor class path filter: %s"), *ActorClassPathFilterString);
		FStringView Argument = ActorClassPathFilterString;
		TArray<FStringView> ActorClassesString;
		UE::String::ParseTokens(Argument, TEXT(","), ActorClassesString, UE::String::EParseTokensOptions::SkipEmpty | UE::String::EParseTokensOptions::Trim);

		ClassesPathForActor.Reserve(ActorClassesString.Num());
		for (const FStringView& ClassString : ActorClassesString)
		{
			ClassesPathForActor.Emplace(ClassString);
		}
	}

	// Loading the memory settings from the cook
	int32 ValueInMB;
	if (GConfig->GetInt(TEXT("CookSettings"), TEXT("MemoryMinFreeVirtual"), ValueInMB, GEditorIni))
	{
		ValueInMB = FMath::Max(ValueInMB, 0);
		MemoryMinFreeVirtual = ValueInMB * 1024ULL * 1024ULL;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Loaded MemoryMinFreeVirtual from CookSettings (%d MiB)"), ValueInMB);
	}

	if (GConfig->GetInt(TEXT("CookSettings"), TEXT("MemoryMaxUsedVirtual"), ValueInMB, GEditorIni))
	{
		ValueInMB = FMath::Max(ValueInMB, 0);
		MemoryMaxUsedVirtual = ValueInMB * 1024ULL * 1024ULL;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Loaded MemoryMaxUsedVirtual from CookSettings (%d MiB)"), ValueInMB);
	}

	if (GConfig->GetInt(TEXT("CookSettings"), TEXT("MemoryMinFreePhysical"), ValueInMB, GEditorIni))
	{
		ValueInMB = FMath::Max(ValueInMB, 0);
		MemoryMinFreePhysical = ValueInMB * 1024ULL * 1024ULL;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Loaded MemoryMinFreePhysical from CookSettings (%d MiB)"), ValueInMB);
	}

	if (GConfig->GetInt(TEXT("CookSettings"), TEXT("MemoryMaxUsedPhysical"), ValueInMB, GEditorIni))
	{
		ValueInMB = FMath::Max(ValueInMB, 0);
		MemoryMaxUsedPhysical = ValueInMB * 1024ULL * 1024ULL;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Loaded MemoryMaxUsedPhysical from CookSettings (%d MiB)"), ValueInMB);
	}

	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

	if (UE::AssetRegistry::ShouldSearchAllAssetsAtStart())
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Searching the levels that need to be processed and their dependencies (this may take a while)..."));
	}
	else
	{
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Searching all assets (this may take a while)..."));
		// This is automatically called in the regular editor but not always when running a commandlet
		// Must also search synchronously because AssetRegistry.IsLoadingAssets() won't account for this search
		AssetRegistry.SearchAllAssets(true);
	}

	// Make sure the level are loaded in the asset registry
	for (const FSoftObjectPath& SoftObjectPaths : Filter.SoftObjectPaths)
	{
		AssetRegistry.WaitForPackage(SoftObjectPaths.GetLongPackageName());
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
				FName DependendPackageName = CurrentDependencies.Pop(EAllowShrinking::No);
				if (!StopSearchAt.Contains(DependendPackageName))
				{
					StopSearchAt.Add(DependendPackageName);
					DependenciesToProcess.Add(DependendPackageName);
					AssetRegistry.WaitForPackage(DependendPackageName.ToString());
					AssetRegistry.GetDependencies(DependendPackageName, CurrentDependencies, UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
				}
			}
		}


		/**
		 * Temporary solution to handle the fact that the dependencies created by the content bundles are not present in the asset registry.
		 * This assume that the level instance is created in via an actor that live in a content bundle won't also have a content bundle.
		 */
		{
			AssetRegistry.WaitForCompletion();
			FARFilter WorldFilter;
			WorldFilter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
			WorldFilter.bIncludeOnlyOnDiskAssets = true;
			TArray<FAssetData> AllLevels;
			AssetRegistry.GetAssets(WorldFilter, AllLevels);
			
			TSet<FName> AlreadyScannedReferences;
			TSet<FName> LevelsName;
			LevelsName.Reserve(AllLevels.Num());
			for (const FAssetData& LevelAsset : AllLevels)
			{
				LevelsName.Add(LevelAsset.PackageName);
			}

			FString ExternalActorFolder(TEXT("/__ExternalActors__/"));
			/**
			 * For all levels search their references for 1 level and then search their dependencies for 1 level.
			 * Example: Level referenced by an possible the level instance actor that live in a content bundle.
			 */
			for (const FAssetData& LevelAsset : AllLevels)
			{
				if (StopSearchAt.Contains(LevelAsset.PackageName))
				{
					continue;
				}

				TArray<FName> LevelReferencers;
				AssetRegistry.GetReferencers(LevelAsset.PackageName, LevelReferencers, UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
				for (const FName& LevelReference : LevelReferencers)
				{
					if (StopSearchAt.Contains(LevelReference))
					{
						DependenciesToProcess.Add(LevelAsset.PackageName);
						StopSearchAt.Add(LevelAsset.PackageName);
						break;
					}

					if (AlreadyScannedReferences.Contains(LevelReference))
					{
						continue;
					}



					bool bHasAddedLevelToDependenciesToProcess = false;
					// Limit the references search to the external actor folders
					if (LevelReference.ToString().Contains(ExternalActorFolder))
					{
						TArray<FName> Dependencies;
						AssetRegistry.GetDependencies(LevelReference, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, QueryFlags);
						for (const FName& Dependency : Dependencies)
						{
							// Check if the asset is refered in the original dependencies chain and that it is a level.
							if (StopSearchAt.Contains(Dependency) && LevelsName.Contains(Dependency))
							{
								bHasAddedLevelToDependenciesToProcess = true;
								DependenciesToProcess.Add(LevelAsset.PackageName);
								break;
							}
						}

						if (bHasAddedLevelToDependenciesToProcess)
						{
							StopSearchAt.Add(LevelReference);
							break;
						}
						else
						{
							AlreadyScannedReferences.Add(LevelReference);
						}
					}
				}
			}
		}

		FARFilter DependenciesFilter;
		DependenciesFilter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
		DependenciesFilter.bIncludeOnlyOnDiskAssets = true;
		DependenciesFilter.PackageNames = DependenciesToProcess.Array();
		AssetRegistry.GetAssets(DependenciesFilter, LevelAssetsAddedByDependencies);
	}

	// Prepare the actor filters for the level that use world partition or one file per actor
	{
		FARFilter ActorsARFilter;
		ActorsARFilter.ClassPaths = MoveTemp(ClassesPathForActor);
		ActorsARFilter.bRecursiveClasses = true;
		ActorsARFilter.bIncludeOnlyOnDiskAssets = true;
		AssetRegistry.CompileFilter(ActorsARFilter, PreCompiledActorsFilter);

		// The class filtering doesn't handle the redirector for the blueprints properly so we add them there
		// Not need to wait for the load of everything in the asset registry as the loading of the dependencies chain should include everything we need
		TArray<FAssetData> Redirectors;
		AssetRegistry.GetAssetsByClass(UObjectRedirector::StaticClass()->GetClassPathName(), Redirectors);
		for (const FAssetData& Redirector : Redirectors)
		{
			FSoftObjectPath RedirectedPath = AssetRegistry.GetRedirectedObjectPath(Redirector.GetSoftObjectPath());
			FTopLevelAssetPath RedirectedPathAsTopLevelAsset(RedirectedPath.GetAssetPath());
			if (PreCompiledActorsFilter.ClassPaths.Contains(RedirectedPathAsTopLevelAsset))
			{
				PreCompiledActorsFilter.ClassPaths.Add(FTopLevelAssetPath(Redirector.PackageName, Redirector.AssetName));
			}
		}
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
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Process Level: %s (%d/%d)"), *LevelAsset.GetSoftObjectPath().ToString(), LevelIndex, LevelCount);
		LoadLevel(LevelAsset);
	}

	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Processing the %d dependencies of the level(s) ..."), LevelAssetsAddedByDependencies.Num());

	for (const FAssetData& LevelAsset : LevelAssetsAddedByDependencies)
	{
		++LevelIndex;
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("-------------------------------------------------------------------"));
		UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Process Level: %s (%d/%d)"), *LevelAsset.GetSoftObjectPath().ToString(), LevelIndex, LevelCount);
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


		TArray<FString> AddedPackageNamesAsArray = AddedPackageNames.Array();
		if (AddedPackageNames.Num() > 0)
		{
			// Might be redundant, but not sure if we can rely on the auto check out on save
			SourceControlHelper.AddToSourceControl(AddedPackageNamesAsArray);
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

			TArray<FString> SubmitFilenames = SourceControlHelpers::PackageFilenames(AddedPackageNamesAsArray);
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

	CollectGarbage(RF_NoFlags);

	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Peak virtual memory usage: %u MiB"), MemoryStats.PeakUsedVirtual / (1024 * 1024));
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Peak physical memory usage: %u MiB"), MemoryStats.PeakUsedPhysical / (1024 * 1024));

	const uint64 EndTime = FPlatformTime::Cycles64();
	const int64 DurationInSeconds = FMath::CeilToInt(FPlatformTime::ToSeconds64(EndTime - StartTime));
	UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Commandlet duration: %d second(s)"), DurationInSeconds);

	return bSuccess ? 0 : 1;
}

UNaniteDisplacedMesh* UGenerateNaniteDisplacedMeshCommandlet::OnLinkDisplacedMesh(const FNaniteDisplacedMeshParams& Parameters, const FString& Folder, const ELinkDisplacedMeshAssetSetting& LinkDisplacedMeshAssetSetting)
{
	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		FNaniteDisplacedMeshEditorModule& Module = FNaniteDisplacedMeshEditorModule::GetModule();
		Module.OnLinkDisplacedMeshOverride.Unbind();

		bool bCreatedNewAsset = false;
		UNaniteDisplacedMesh* NaniteDisplacedMesh = LinkDisplacedMeshAsset(nullptr, Parameters, Folder, ELinkDisplacedMeshAssetSetting::LinkAgainstPersistentAsset, &bCreatedNewAsset);
		if (NaniteDisplacedMesh != nullptr)
		{
			const FString PackageName = NaniteDisplacedMesh->GetPackage()->GetPathName();
			UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Linked mesh: %s"), *PackageName);

			LinkedPackageNames.Add(PackageName);
			LinkedPackageFolders.Add(Folder);

			if (bCreatedNewAsset)
			{
				AddedPackageNames.Add(PackageName);
			}
		}

		Module.OnLinkDisplacedMeshOverride.BindUObject(this, &UGenerateNaniteDisplacedMeshCommandlet::OnLinkDisplacedMesh);
		return NaniteDisplacedMesh;
	}
	else
	{
		// Capture the request but process it later to avoid some issues during the save of the assets
		QueuedLinkingRequest.Emplace(Parameters, Folder, LinkDisplacedMeshAssetSetting);

		FNaniteDisplacedMeshEditorModule& Module = FNaniteDisplacedMeshEditorModule::GetModule();
		Module.OnLinkDisplacedMeshOverride.Unbind();

		UNaniteDisplacedMesh* NaniteDisplacedMesh = LinkDisplacedMeshAsset(nullptr, Parameters, Folder, LinkDisplacedMeshAssetSetting);
	
		Module.OnLinkDisplacedMeshOverride.BindUObject(this, &UGenerateNaniteDisplacedMeshCommandlet::OnLinkDisplacedMesh);
		return NaniteDisplacedMesh;
	}
}

void UGenerateNaniteDisplacedMeshCommandlet::LoadLevel(const FAssetData& AssetData)
{
	if (AssetData.GetClass() != UWorld::StaticClass())
	{
		return;
	}

	if (!UE::NaniteDisplacedMesh::Private::GenerateNaniteDisplacedMesh::ShouldLoadLevel(AssetData, PreCompiledActorsFilter.ClassPaths))
	{
		return;
	}

	TSet<FName> LoadTags;
	// Tell the level to don't the external objects (has we will load those that interest us manually)
	LoadTags.Add(ULevel::DontLoadExternalObjectsTag);

	// Finish loading the potentially partially loaded package
	FLinkerInstancingContext InstancingContext(MoveTemp(LoadTags));
	LoadPackage(nullptr, *AssetData.PackageName.ToString(), LOAD_None, nullptr, &InstancingContext);
	
	// Get the world
	UWorld* World = Cast<UWorld>(AssetData.GetAsset(LoadTags));
	if (World == nullptr)
	{
		return;
	}

	World->AddToRoot();

	auto ShouldKickGC = [this]()
		{
			const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
			if (MemoryMinFreeVirtual > 0 && MemoryStats.AvailableVirtual < MemoryMinFreeVirtual)
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Low virtual memory available (%d MiB). kicking GC."), MemoryStats.AvailableVirtual / 1024 / 1024);
				return true;
			}

			if (MemoryMinFreePhysical > 0 && MemoryStats.AvailablePhysical < MemoryMinFreePhysical)
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("Low physical memory available (%d MiB). kicking GC."), MemoryStats.AvailablePhysical / 1024 / 1024);
				return true;
			}

			if (MemoryMaxUsedVirtual > 0 && MemoryStats.UsedVirtual > MemoryMaxUsedVirtual)
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("High virtual memory usage (%d MiB). kicking GC."), MemoryStats.UsedVirtual / 1024 / 1024);
				return true;
			}

			if (MemoryMaxUsedPhysical > 0 && MemoryStats.UsedPhysical > MemoryMaxUsedPhysical)
			{
				UE_LOG(LogNaniteDisplacedMesh, Display, TEXT("High physical memory usage (% dMiB). kicking GC."), MemoryStats.UsedPhysical / 1024 / 1024);
				return true;
			}

			return false;
		};



	// Load the external actors (we should look with the open world team to see if there is a better way to do this)
	if (const ULevel* PersistantLevel = World->PersistentLevel)
	{
		if (PersistantLevel->bUseExternalActors)
		{
			const FString ExternalActorsPath = ULevel::GetExternalActorsPath(AssetData.PackageName.ToString());
			
			IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

			// No need to scan the folder since those are already scanned when we scanned the level dependencies

			FARFilter ActorsPathFilter;
			ActorsPathFilter.PackagePaths.Add(FName(*ExternalActorsPath));
			ActorsPathFilter.bRecursivePaths = true;
			ActorsPathFilter.bIncludeOnlyOnDiskAssets = true;
			FARCompiledFilter CompiledActorsPathFilter;
			AssetRegistry.CompileFilter(ActorsPathFilter, CompiledActorsPathFilter);

			PreCompiledActorsFilter.PackagePaths = MoveTemp(CompiledActorsPathFilter.PackagePaths);
			AssetRegistry.EnumerateAssets(PreCompiledActorsFilter, [this, &ShouldKickGC](const FAssetData& AssetData)
				{
					// This will load the actor and the commandlet will capture the emitted linking events.
					AssetData.GetAsset();

					while (!QueuedLinkingRequest.IsEmpty())
					{
						FOnLinkDisplacedMeshArgs LinkDisplacedMeshArgs = QueuedLinkingRequest.Pop(EAllowShrinking::No);
						OnLinkDisplacedMesh(LinkDisplacedMeshArgs.Parameters, LinkDisplacedMeshArgs.Folder, LinkDisplacedMeshArgs.LinkDisplacedMeshAssetSetting);
					}

					if (ShouldKickGC())
					{
						CollectGarbage(RF_NoFlags);
					}

					return true;
				});
		}
		else
		{
			// If the world don't use the external actors we still need to process the linking requests captured and kick the GC if needed
			while (!QueuedLinkingRequest.IsEmpty())
			{
				FOnLinkDisplacedMeshArgs LinkDisplacedMeshArgs = QueuedLinkingRequest.Pop(EAllowShrinking::No);
				OnLinkDisplacedMesh(LinkDisplacedMeshArgs.Parameters, LinkDisplacedMeshArgs.Folder, LinkDisplacedMeshArgs.LinkDisplacedMeshAssetSetting);
			}
			if (ShouldKickGC())
			{
				CollectGarbage(RF_NoFlags);
			}
		}
	}

	World->RemoveFromRoot();
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

