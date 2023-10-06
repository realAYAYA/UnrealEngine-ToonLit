// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * WorldPartitionLevelHelper implementation
 */

#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "Misc/PackageName.h"
#include "WorldPartition/WorldPartitionPackageHelper.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "Containers/StringFwd.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "WorldPartition/IWorldPartitionObjectResolver.h"

#if WITH_EDITOR

#include "Engine/Level.h"
#include "Misc/Paths.h"
#include "Model.h"
#include "UnrealEngine.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionPackageHelper.h"
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"
#include "GameFramework/WorldSettings.h"
#include "LevelUtils.h"
#include "ActorFolder.h"

#endif

bool FWorldPartitionResolveData::ResolveObject(UWorld* InWorld, const FSoftObjectPath& InObjectPath, UObject*& OutObject) const
{
	OutObject = nullptr;
	if (InWorld)
	{
		if (IsValid() && SourceWorldAssetPath == InObjectPath.GetAssetPath().ToString())
		{
			const FString SubPathString = FWorldPartitionLevelHelper::AddActorContainerIDToSubPathString(ContainerID, InObjectPath.GetSubPathString());
			// We don't read the return value as we always want to return true when using the resolve data.
			InWorld->ResolveSubobject(*SubPathString, OutObject, /*bLoadIfExists*/false);
			return true;
		}
	}

	return false;
}

FString FWorldPartitionLevelHelper::AddActorContainerIDToSubPathString(const FActorContainerID& InContainerID, const FString& InSubPathString)
{
	if (!InContainerID.IsMainContainer())
	{
		constexpr const TCHAR PersistenLevelName[] = TEXT("PersistentLevel.");
		constexpr const int32 DotPos = UE_ARRAY_COUNT(PersistenLevelName);
		if (InSubPathString.StartsWith(PersistenLevelName))
		{
			const int32 SubObjectPos = InSubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, DotPos);
			if (SubObjectPos == INDEX_NONE)
			{
				return InSubPathString + TEXT("_") + InContainerID.ToShortString();
			}
			else
			{
				return InSubPathString.Mid(0, SubObjectPos) + TEXT("_") + InContainerID.ToShortString() + InSubPathString.Mid(SubObjectPos);
			}
		}
	}

	return InSubPathString;
}

#if WITH_EDITOR
FWorldPartitionLevelHelper& FWorldPartitionLevelHelper::Get()
{
	static FWorldPartitionLevelHelper Instance;
	return Instance;
}

FWorldPartitionLevelHelper::FWorldPartitionLevelHelper()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FWorldPartitionLevelHelper::PreGarbageCollect);
}

void FWorldPartitionLevelHelper::PreGarbageCollect()
{
	for (TWeakObjectPtr<UPackage>& PackageToUnload : PreGCPackagesToUnload)
	{
		// Test if WeakObjectPtr is valid since clean up could have happened outside of this helper
		if (PackageToUnload.IsValid())
		{
			FWorldPartitionPackageHelper::UnloadPackage(PackageToUnload.Get());
		}
	}
	PreGCPackagesToUnload.Reset();
}

void FWorldPartitionLevelHelper::AddReference(UPackage* InPackage, FPackageReferencer* InReferencer)
{
	check(InPackage);
	FPackageReference& RefInfo = PackageReferences.FindOrAdd(InPackage->GetFName());
	check(RefInfo.Package == nullptr || RefInfo.Package == InPackage);
	RefInfo.Package = InPackage;
	RefInfo.Referencers.Add(InReferencer);
	PreGCPackagesToUnload.Remove(InPackage);
}

void FWorldPartitionLevelHelper::RemoveReferences(FPackageReferencer* InReferencer)
{
	for (auto It = PackageReferences.CreateIterator(); It; ++It)
	{
		FPackageReference& RefInfo = It->Value;
		RefInfo.Referencers.Remove(InReferencer);
		if (RefInfo.Referencers.Num() == 0)
		{
			// Test if WeakObjectPtr is valid since clean up could have happened outside of this helper
			if (RefInfo.Package.IsValid())
			{
				PreGCPackagesToUnload.Add(RefInfo.Package);
			}
			It.RemoveCurrent();
		}
	}
}

void FWorldPartitionLevelHelper::FPackageReferencer::AddReference(UPackage* InPackage)
{
	FWorldPartitionLevelHelper::Get().AddReference(InPackage, this);
}

void FWorldPartitionLevelHelper::FPackageReferencer::RemoveReferences()
{
	FWorldPartitionLevelHelper::Get().RemoveReferences(this);
}


 /**
  * Defaults World's initialization values for World Partition StreamingLevels
  */
UWorld::InitializationValues FWorldPartitionLevelHelper::GetWorldInitializationValues()
{
	return UWorld::InitializationValues()
		.InitializeScenes(false)
		.AllowAudioPlayback(false)
		.RequiresHitProxies(false)
		.CreatePhysicsScene(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.EnableTraceCollision(false)
		.SetTransactional(false)
		.CreateFXSystem(false);
}

/**
 * Moves external actors into the given level
 */
void FWorldPartitionLevelHelper::MoveExternalActorsToLevel(const TArray<FWorldPartitionRuntimeCellObjectMapping>& InChildPackages, ULevel* InLevel, TArray<UPackage*>& OutModifiedPackages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWorldPartitionLevelHelper::MoveExternalActorsToLevel);

	check(InLevel);
	UPackage* LevelPackage = InLevel->GetPackage();

	// Gather existing actors to validate only the one we expect are added to the level
	TSet<FName> LevelActors;
	for (AActor* Actor : InLevel->Actors)
	{
		if (Actor)
		{
			LevelActors.Add(Actor->GetFName());
		}
	}

	// Move all actors to Cell level
	for (const FWorldPartitionRuntimeCellObjectMapping& PackageObjectMapping : InChildPackages)
	{
		// We assume actor failed to duplicate if LoadedPath equals NAME_None (warning already logged we can skip this mapping)
		if (PackageObjectMapping.LoadedPath == NAME_None && !PackageObjectMapping.ContainerID.IsMainContainer())
		{
			continue;
		}

		AActor* Actor = FindObject<AActor>(nullptr, *PackageObjectMapping.LoadedPath.ToString());
		if (Actor)
		{
			UPackage* ActorExternalPackage = Actor->GetExternalPackage();
			check(ActorExternalPackage);

			const bool bSameOuter = (InLevel == Actor->GetOuter());
			Actor->SetPackageExternal(false, false);

			// Avoid calling Rename on the actor if it's already outered to InLevel as this will cause it's name to be changed. 
			// (UObject::Rename doesn't check if Rename is being called with existing outer and assigns new name)
			if (!bSameOuter)
			{
				Actor->Rename(nullptr, InLevel, REN_ForceNoResetLoaders);
			}
			else if (!InLevel->Actors.Contains(Actor))
			{
				InLevel->AddLoadedActor(Actor);
			}
			check(Actor->GetPackage() == LevelPackage);

			// Process objects found in the source actor package
			TArray<UObject*> Objects;
			const bool bIncludeNestedSubobjects = false;
			// Skip Garbage objects as the initial Rename on an actor with an ChildActorComponent can destroy its child actors.
			// This happens when the component has bNeedsRecreate set to true (when it has a valid ChildActorTemplate).
			GetObjectsWithPackage(ActorExternalPackage, Objects, bIncludeNestedSubobjects, RF_NoFlags, EInternalObjectFlags::Garbage);
			for (UObject* Object : Objects)
			{
				if (Object->GetFName() != NAME_PackageMetaData)
				{
					if (Object->GetOuter()->IsA<ULevel>())
					{
						// Move objects that are outered the level in the destination level
						AActor* NestedActor = Cast<AActor>(Object);
						if (InLevel != Object->GetOuter())
						{
							Object->Rename(nullptr, InLevel, REN_ForceNoResetLoaders);
						}
						else if (NestedActor && !InLevel->Actors.Contains(NestedActor))
						{
							InLevel->AddLoadedActor(NestedActor);
						}
						if (NestedActor)
						{
							LevelActors.Add(NestedActor->GetFName());
						}
					}
					else
					{
						// Move objects in the destination level package
						Object->Rename(nullptr, LevelPackage, REN_ForceNoResetLoaders);
					}
				}
			}

			OutModifiedPackages.Add(ActorExternalPackage);
			LevelActors.Add(Actor->GetFName());
		}
		else
		{
			UE_LOG(LogWorldPartition, Warning, TEXT("Can't find actor %s."), *PackageObjectMapping.Path.ToString());
		}
	}

	for (AActor* Actor : InLevel->Actors)
	{
		if (Actor && Actor->HasAllFlags(RF_WasLoaded))
		{
			checkf(LevelActors.Contains(Actor->GetFName()), TEXT("Actor %s(%s) was unexpectedly loaded when moving actors to streaming cell"), *Actor->GetActorNameOrLabel(), *Actor->GetName());
		}
	}
}

void FWorldPartitionLevelHelper::RemapLevelSoftObjectPaths(ULevel* InLevel, UWorldPartition* InWorldPartition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWorldPartitionLevelHelper::RemapLevelSoftObjectPaths);

	check(InLevel);
	check(InWorldPartition);

	FSoftObjectPathFixupArchive FixupSerializer([InWorldPartition](FSoftObjectPath& Value)
	{
		if(!Value.IsNull())
		{
			InWorldPartition->RemapSoftObjectPath(Value);
		}
	});
	FixupSerializer.Fixup(InLevel);
}

FString FWorldPartitionLevelHelper::GetContainerPackage(const FActorContainerID& InContainerID, const FString& InPackageName, const FString& InDestLevelPackageName)
{
	// Generate a unique name to load a Level Instance embedded actor if there are multiple instances of this Level Instance and possibly across multiple instances of the World Partition world
	// InContainerID will distinguish between instances of the same Level Instance
	// InDestLevelPackageName will distinguish between instances of the same top level World Partition world (Only needed in PIE, In Cook we always cook the source WP and not an instance and Actor packages no longer exist at runtime)
	uint64 DestLevelID = 0;
	TStringBuilder<512> PackageNameBuilder;
	PackageNameBuilder.Appendf(TEXT("/Temp%s_%s"), *InPackageName, *InContainerID.ToShortString());
		
	if (!InDestLevelPackageName.IsEmpty())
	{
		DestLevelID = CityHash64((const char*)*InDestLevelPackageName, InDestLevelPackageName.Len() * sizeof(TCHAR));
		PackageNameBuilder.Appendf(TEXT("_%016llx"), DestLevelID);
	}

	return PackageNameBuilder.ToString();
}

FSoftObjectPath FWorldPartitionLevelHelper::RemapActorPath(const FActorContainerID& InContainerID, const FString& InSourceWorldPath, const FSoftObjectPath& InActorPath)
{
	// If Path is in an instanced package it will now be remapped to its source package
	FSoftObjectPath OutActorPath(FTopLevelAssetPath(InSourceWorldPath), InActorPath.GetSubPathString());
	
	if(!InContainerID.IsMainContainer())
	{
		// This gets called by UWorldPartitionLevelStreamingPolicy::PrepareActorToCellRemapping and FWorldPartitionLevelHelper::LoadActors
		// 
		// At this point we are changing the top level asset and remapping the SubPathString to add a ContainerID suffix so
		// '/Game/SomePath/LevelInstance.LevelInstance:PersistentLevel.ActorA' becomes
		// '/Game/SomeOtherPath/SourceWorldName.SourceWorldName:PersistentLevel.ActorA_{ContainerID}'
		FString RemappedSubPathString = FWorldPartitionLevelHelper::AddActorContainerIDToSubPathString(InContainerID, InActorPath.GetSubPathString());
		OutActorPath.SetSubPathString(RemappedSubPathString);
	}
	
	return OutActorPath;
}

bool FWorldPartitionLevelHelper::RemapLevelCellPathInContentBundle(ULevel* Level, const class FContentBundleEditor* ContentBundleEditor, const UWorldPartitionRuntimeCell* Cell)
{
	FString CellPath = ContentBundleEditor->GetExternalStreamingObjectPackagePath();
	CellPath += TEXT(".");
	CellPath += ContentBundleEditor->GetExternalStreamingObjectName();
	CellPath += TEXT(".");
	CellPath += Cell->GetName();
	
	Level->WorldPartitionRuntimeCell = FSoftObjectPath(CellPath);

	return !Level->WorldPartitionRuntimeCell.GetUniqueID().IsNull();
}

/**
 * Creates an empty Level used in World Partition
 */
ULevel* FWorldPartitionLevelHelper::CreateEmptyLevelForRuntimeCell(const UWorldPartitionRuntimeCell* Cell, const UWorld* InWorld, const FString& InWorldAssetName, UPackage* InPackage)
{
	// Create or use given package
	UPackage* CellPackage = nullptr;
	if (InPackage)
	{
		check(FindObject<UPackage>(nullptr, *InPackage->GetName()));
		CellPackage = InPackage;
	}
	else
	{
		FString PackageName = FPackageName::ObjectPathToPackageName(InWorldAssetName);
		check(!FindObject<UPackage>(nullptr, *PackageName));
		CellPackage = CreatePackage(*PackageName);
		CellPackage->SetPackageFlags(PKG_NewlyCreated);
	}

	if (InWorld->IsPlayInEditor())
	{
		check(!InPackage);
		CellPackage->SetPackageFlags(PKG_PlayInEditor);
		CellPackage->SetPIEInstanceID(InWorld->GetPackage()->GetPIEInstanceID());
	}

	// Create World & Persistent Level
	UWorld::InitializationValues IVS = FWorldPartitionLevelHelper::GetWorldInitializationValues();
	const FName WorldName = FName(FPackageName::ObjectPathToObjectName(InWorldAssetName));
	check(!FindObject<UWorld>(CellPackage, *WorldName.ToString()));
	UWorld* NewWorld = UWorld::CreateWorld(InWorld->WorldType, /*bInformEngineOfWorld*/false, WorldName, CellPackage, /*bAddToRoot*/false, InWorld->GetFeatureLevel(), &IVS, /*bInSkipInitWorld*/true);
	check(NewWorld);
	NewWorld->SetFlags(RF_Public | RF_Standalone);
	check(NewWorld->GetWorldSettings());
	check(UWorld::FindWorldInPackage(CellPackage) == NewWorld);
	check(InPackage || (NewWorld->GetPathName() == InWorldAssetName));
	// We don't need the cell level's world setting to replicate
	FSetActorReplicates SetActorReplicates(NewWorld->GetWorldSettings(), false);
	
	// Setup of streaming cell Runtime Level
	ULevel* NewLevel = NewWorld->PersistentLevel;
	check(NewLevel);
	check(NewLevel->GetFName() == InWorld->PersistentLevel->GetFName());
	check(NewLevel->OwningWorld == NewWorld);
	check(NewLevel->Model);
	check(!NewLevel->bIsVisible);

	NewLevel->WorldPartitionRuntimeCell = Cell;
	
	// Mark the level package as fully loaded
	CellPackage->MarkAsFullyLoaded();

	// Mark the level package as containing a map
	CellPackage->ThisContainsMap();

	// Set the guids on the constructed level to something based on the generator rather than allowing indeterminism by
	// constructing new Guids on every cook
	// @todo_ow: revisit for static lighting support. We need to base the LevelBuildDataId on the relevant information from the
	// actor's package.
	NewLevel->LevelBuildDataId = InWorld->PersistentLevel->LevelBuildDataId;
	check(InWorld->PersistentLevel->Model && NewLevel->Model);
	NewLevel->Model->LightingGuid = InWorld->PersistentLevel->Model->LightingGuid;

	return NewLevel;
}

bool FWorldPartitionLevelHelper::LoadActors(UWorld* InOuterWorld, ULevel* InDestLevel, TArrayView<FWorldPartitionRuntimeCellObjectMapping> InActorPackages, FWorldPartitionLevelHelper::FPackageReferencer& InPackageReferencer, TFunction<void(bool)> InCompletionCallback, bool bInLoadAsync, FLinkerInstancingContext InstancingContext)
{
	UPackage* DestPackage = InDestLevel ? InDestLevel->GetPackage() : nullptr;
	FString ShortLevelPackageName = DestPackage? FPackageName::GetShortName(DestPackage->GetFName()) : FString();

	struct FLoadProgress
	{
		int32 NumPendingLoadRequests = 0;
		int32 NumFailedLoadedRequests = 0;
	};
	TSharedPtr<FLoadProgress> LoadProgress = MakeShared<FLoadProgress>();

	// Actors to load
	TArray<FWorldPartitionRuntimeCellObjectMapping*> ActorPackages;
	ActorPackages.Reserve(InActorPackages.Num());

	TMap<FActorContainerID, FLinkerInstancingContext> LinkerInstancingContexts;
	// Add Main container context
	LinkerInstancingContexts.Add(FActorContainerID::GetMainContainerID(), MoveTemp(InstancingContext));
			
	for (FWorldPartitionRuntimeCellObjectMapping& PackageObjectMapping : InActorPackages)
	{
		FLinkerInstancingContext* Context = LinkerInstancingContexts.Find(PackageObjectMapping.ContainerID);
		if (!Context)
		{
			check(!PackageObjectMapping.ContainerID.IsMainContainer());
		
			const FString DestLevelPackageName = InDestLevel ? InDestLevel->GetPackage()->GetName() : FString();
			const FName ContainerPackageInstanceName(GetContainerPackage(PackageObjectMapping.ContainerID, PackageObjectMapping.ContainerPackage.ToString(), DestLevelPackageName));

			FLinkerInstancingContext& NewContext = LinkerInstancingContexts.Add(PackageObjectMapping.ContainerID);

			// Make sure here we don't remap the SoftObjectPaths through the linker when loading the embedded actor packages. 
			// A remapping will happen in the packaged loaded callback later in this method.
			NewContext.SetSoftObjectPathRemappingEnabled(false); 
			
			NewContext.AddTag(ULevel::DontLoadExternalObjectsTag);
			NewContext.AddPackageMapping(PackageObjectMapping.ContainerPackage, ContainerPackageInstanceName);
			Context = &NewContext;
		}
		
		const FName ContainerPackageInstanceName = Context->RemapPackage(PackageObjectMapping.ContainerPackage);
		if (PackageObjectMapping.ContainerPackage != ContainerPackageInstanceName)
		{
			const FName ActorPackageName = *FPackageName::ObjectPathToPackageName(PackageObjectMapping.Package.ToString());
			const FName ActorPackageInstanceName = PackageObjectMapping.bIsEditorOnly ? NAME_None : FName(*ULevel::GetExternalActorPackageInstanceName(ContainerPackageInstanceName.ToString(), ActorPackageName.ToString()));

			Context->AddPackageMapping(ActorPackageName, ActorPackageInstanceName);
		}

		if (!PackageObjectMapping.bIsEditorOnly)
		{
			ActorPackages.Add(&PackageObjectMapping);
		}
	}

	LoadProgress->NumPendingLoadRequests = ActorPackages.Num();

	for (FWorldPartitionRuntimeCellObjectMapping* PackageObjectMapping : ActorPackages)
	{
		FLoadPackageAsyncDelegate CompletionCallback = FLoadPackageAsyncDelegate::CreateLambda([LoadProgress, PackageObjectMapping, &InPackageReferencer, InOuterWorld, InDestLevel, InCompletionCallback](const FName& LoadedPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
		{
			const FName ActorName = *FPaths::GetExtension(PackageObjectMapping->Path.ToString());
			check(LoadProgress->NumPendingLoadRequests);
			LoadProgress->NumPendingLoadRequests--;

			// In PIE, we make sure to clear RF_Standalone flag on objects in external packages (UMetaData) 
			// This guarantees that external packages of actors that are destroyed during the PIE session will
			// properly get GC'ed and will allow future edits/modifications of OFPA actors.
			if (LoadedPackage && InDestLevel && InDestLevel->GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor))
			{
				ForEachObjectWithPackage(LoadedPackage, [](UObject* Object)
				{
					Object->ClearFlags(RF_Standalone);
					return true;
				}, false);
			}

			AActor* Actor = LoadedPackage ? FindObject<AActor>(LoadedPackage, *ActorName.ToString()) : nullptr;

			if (Actor)
			{
				const UWorld* ContainerWorld = PackageObjectMapping->ContainerID.IsMainContainer() ? InOuterWorld : Actor->GetTypedOuter<UWorld>();
				
				TOptional<FName> SrcActorFolderPath;

				// Make sure Source level actor folder fixup was called
				if (ContainerWorld->PersistentLevel->IsUsingActorFolders())
				{ 
					if (!ContainerWorld->PersistentLevel->LoadedExternalActorFolders.IsEmpty())
					{
						ContainerWorld->PersistentLevel->bFixupActorFoldersAtLoad = false;
						ContainerWorld->PersistentLevel->FixupActorFolders();
					}
				
					// Since actor's level doesn't necessarily uses actor folders, access Folder Guid directly
					const bool bDirectAccess = true;
					const FGuid ActorFolderGuid = Actor->GetFolderGuid(bDirectAccess);
					// Resolve folder guid from source container level and resolve/backup the folder path
					if (UActorFolder* SrcFolder = ContainerWorld->PersistentLevel->GetActorFolder(ActorFolderGuid))
					{
						SrcActorFolderPath = SrcFolder->GetPath();
					}
				}

				if (!PackageObjectMapping->ContainerID.IsMainContainer())
				{					
					// Add Cache handle on world so it gets unloaded properly
					InPackageReferencer.AddReference(ContainerWorld->GetPackage());
										
					// We only care about the source paths here
					FString SourceWorldPath, DummyUnusedPath;
					// Verify that it is indeed an instanced world
					verify(ContainerWorld->GetSoftObjectPathMapping(SourceWorldPath, DummyUnusedPath));
					FString SourceOuterWorldPath;
					InOuterWorld->GetSoftObjectPathMapping(SourceOuterWorldPath, DummyUnusedPath);

					// Rename through UObject to avoid changing Actor's external packaging and folder properties
					Actor->UObject::Rename(*FString::Printf(TEXT("%s_%s"), *Actor->GetName(), *PackageObjectMapping->ContainerID.ToShortString()), InDestLevel, REN_NonTransactional | REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors);

					// Handle child actors
					Actor->ForEachComponent<UChildActorComponent>(true, [InDestLevel, PackageObjectMapping](UChildActorComponent* ChildActorComponent)
					{
						if (AActor* ChildActor = ChildActorComponent->GetChildActor())
						{
							ChildActor->UObject::Rename(*FString::Printf(TEXT("%s_%s"), *ChildActor->GetName(), *PackageObjectMapping->ContainerID.ToShortString()), InDestLevel, REN_NonTransactional | REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors);
						}
					});
					
					FLevelUtils::FApplyLevelTransformParams TransformParams(nullptr, PackageObjectMapping->ContainerTransform);
					TransformParams.Actor = Actor;
					TransformParams.bDoPostEditMove = false;
					FLevelUtils::ApplyLevelTransform(TransformParams);

					// Set the actor's instance guid
					FSetActorInstanceGuid SetActorInstanceGuid(Actor, PackageObjectMapping->ActorInstanceGuid);
						
					// Path to use when searching for this actor in MoveExternalActorsToLevel
					PackageObjectMapping->LoadedPath = *Actor->GetPathName();

					// Fixup any FSoftObjectPath from this Actor (and its SubObjects) in this container to another object in the same container with a ContainerID suffix that can be remapped to
					// a Cell package in the StreamingPolicy.
					// 
					// At  this point we are remapping the SubPathString and adding a ContainerID suffix so
					// '/Game/SomePath/WorldName.WorldName:PersistentLevel.ActorA' becomes
					// '/Game/SomeOtherPath/OuterWorldName.OuterWorldName:PersistentLevel.ActorA_{ContainerID}'
					FSoftObjectPathFixupArchive FixupArchive([&](FSoftObjectPath& Value)
					{
						if (!Value.IsNull() && Value.GetAssetPathString().Equals(SourceWorldPath, ESearchCase::IgnoreCase))
						{
							Value = RemapActorPath(PackageObjectMapping->ContainerID, SourceOuterWorldPath, Value);
						}
					});
					FixupArchive.Fixup(Actor);

					if (IWorldPartitionObjectResolver* ObjectResolver = Cast<IWorldPartitionObjectResolver>(Actor))
					{
						ObjectResolver->SetWorldPartitionResolveData(FWorldPartitionResolveData(PackageObjectMapping->ContainerID, SourceWorldPath));
					}
				}

				if (InDestLevel)
				{
					// Propagate resolved actor folder path
					check(!InDestLevel->IsUsingActorFolders());
					if (SrcActorFolderPath.IsSet())
					{
						Actor->SetFolderPath(*SrcActorFolderPath);
					}

					check(Actor->IsPackageExternal());
					InDestLevel->Actors.Add(Actor);
					checkf(Actor->GetLevel() == InDestLevel, TEXT("Levels mismatch, got : %s, expected: %s\nActor: %s\nActorFullName: %s\nActorPackage: %s"), *InDestLevel->GetFullName(), *Actor->GetLevel()->GetFullName(), *Actor->GetActorNameOrLabel(), *Actor->GetFullName(), *Actor->GetPackage()->GetFullName());

					// Handle child actors
					Actor->ForEachComponent<UChildActorComponent>(true, [InDestLevel](UChildActorComponent* ChildActorComponent)
					{
						if (AActor* ChildActor = ChildActorComponent->GetChildActor())
						{
							InDestLevel->Actors.Add(ChildActor);
							check(ChildActor->GetLevel() == InDestLevel);
						}
					});
				}

				UE_LOG(LogWorldPartition, Verbose, TEXT(" ==> Loaded %s (remaining: %d)"), *Actor->GetFullName(), LoadProgress->NumPendingLoadRequests);
			}
			else
			{
				if (LoadedPackage)
				{
					UE_LOG(LogWorldPartition, Warning, TEXT("Failed to find actor in package %s. Package Content:"), *LoadedPackageName.ToString());
					ForEachObjectWithPackage(LoadedPackage, [](UObject* Object)
					{
						UE_LOG(LogWorldPartition, Warning, TEXT("\t Object %s, Flags 0x%llx"), *Object->GetName(), static_cast<uint64>(Object->GetFlags()));
						return true;
					}, false);
				}
				else
				{
					UE_LOG(LogWorldPartition, Warning, TEXT("Failed to load actor package %s"), *LoadedPackageName.ToString());
				}

				//@todo_ow: cumulate and process when NumPendingActorRequests == 0
				LoadProgress->NumFailedLoadedRequests++;
			}

			if (!LoadProgress->NumPendingLoadRequests)
			{
				InCompletionCallback(!LoadProgress->NumFailedLoadedRequests);
			}
		});

		FName PackageToLoad(*FPackageName::ObjectPathToPackageName(PackageObjectMapping->Package.ToString()));
		const FLinkerInstancingContext& ContainerInstancingContext = LinkerInstancingContexts.FindChecked(PackageObjectMapping->ContainerID);
		FName PackageName = ContainerInstancingContext.RemapPackage(PackageToLoad);

		if (bInLoadAsync)
		{
			FPackagePath PackagePath = FPackagePath::FromPackageNameChecked(PackageToLoad);

			check(DestPackage);
			const EPackageFlags PackageFlags = DestPackage->HasAnyPackageFlags(PKG_PlayInEditor) ? PKG_PlayInEditor : PKG_None;
			::LoadPackageAsync(PackagePath, PackageName, CompletionCallback, PackageFlags, DestPackage->GetPIEInstanceID(), 0, &ContainerInstancingContext);
		}
		else
		{
			UPackage* InstancingPackage = nullptr;
			if (PackageName != PackageToLoad)
			{
				InstancingPackage = CreatePackage(*PackageName.ToString());
			}

			UPackage* Package = LoadPackage(InstancingPackage, *PackageToLoad.ToString(), LOAD_None, nullptr, &ContainerInstancingContext);
			CompletionCallback.Execute(PackageToLoad, Package, Package ? EAsyncLoadingResult::Succeeded : EAsyncLoadingResult::Failed);
		}
	}

	return (LoadProgress->NumPendingLoadRequests == 0);
}

#endif