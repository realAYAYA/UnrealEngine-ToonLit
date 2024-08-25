// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionCookPackageSplitter.h"

#if WITH_EDITOR

#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "Editor.h"

// Register FWorldPartitionCookPackageSplitter for UWorld class
REGISTER_COOKPACKAGE_SPLITTER(FWorldPartitionCookPackageSplitter, UWorld);

bool FWorldPartitionCookPackageSplitter::ShouldSplit(UObject* SplitData)
{
	UWorld* World = Cast<UWorld>(SplitData);
	return World && World->IsPartitionedWorld();
}

FWorldPartitionCookPackageSplitter::FWorldPartitionCookPackageSplitter()
{
}

FWorldPartitionCookPackageSplitter::~FWorldPartitionCookPackageSplitter()
{
	check(!ReferencedWorld);
}

void FWorldPartitionCookPackageSplitter::Teardown(ETeardown Status)
{
	UE_LOG(LogWorldPartition, Display, TEXT("[Cook] Debug(TearDown): OwnerObject=%s bForceInitializedWorld=%d bInitializedPhysicsSceneForSave=%d"), *GetFullNameSafe(ReferencedWorld),  bForceInitializedWorld ? 1 : 0, bInitializedPhysicsSceneForSave ? 1 : 0);

	FWorldDelegates::OnWorldCleanup.RemoveAll(this);

	// Assume that the world is partitioned as per FWorldPartitionCookPackageSplitter::ShouldSplit
	UWorldPartition* WorldPartition = ReferencedWorld->PersistentLevel->GetWorldPartition();
	check(WorldPartition);

	WorldPartition->EndCook(CookContext);
	WorldPartition->Uninitialize();

	if (bInitializedPhysicsSceneForSave)
	{
		GEditor->CleanupPhysicsSceneThatWasInitializedForSave(ReferencedWorld, bForceInitializedWorld);
		bInitializedPhysicsSceneForSave = false;
		bForceInitializedWorld = false;
	}

	ReferencedWorld = nullptr;
}

void FWorldPartitionCookPackageSplitter::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	checkf(InWorld != ReferencedWorld, TEXT("[Cook] %s is being cleaned up while still referenced by a package splitter."), *GetFullNameSafe(InWorld));
}

void FWorldPartitionCookPackageSplitter::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ReferencedWorld);
}

FString FWorldPartitionCookPackageSplitter::GetReferencerName() const
{
	return TEXT("FWorldPartitionCookPackageSplitter");
}

TArray<ICookPackageSplitter::FGeneratedPackage> FWorldPartitionCookPackageSplitter::GetGenerateList(const UPackage* OwnerPackage, const UObject* OwnerObject)
{
	UE_LOG(LogWorldPartition, Display, TEXT("[Cook] Gathering packages to cook from generators for owner object %s."), *GetFullNameSafe(OwnerObject));

	// Store the World pointer to declare it to GarbageCollection; we do not want to allow the World to be Garbage Collected
	// until we have finished all of our PreSaveGeneratedPackage calls, because we store information on the World 
	// that is necessary for populate 
	ReferencedWorld = const_cast<UWorld*>(CastChecked<const UWorld>(OwnerObject));

	check(!bInitializedPhysicsSceneForSave && !bForceInitializedWorld);
	bInitializedPhysicsSceneForSave = GEditor->InitializePhysicsSceneForSaveIfNecessary(ReferencedWorld, bForceInitializedWorld);

	// Assume that the world is partitioned as per FWorldPartitionCookPackageSplitter::ShouldSplit
	UWorldPartition* WorldPartition = ReferencedWorld->PersistentLevel->GetWorldPartition();
	check(WorldPartition);

	// We expect the WorldPartition has not yet been initialized
	ensure(!WorldPartition->IsInitialized());
	WorldPartition->Initialize(ReferencedWorld, FTransform::Identity);
	WorldPartition->BeginCook(CookContext);

	bool bIsSuccess = CookContext.GatherPackagesToCook();
	UE_CLOG(!bIsSuccess, LogWorldPartition, Warning, TEXT("[Cook] Errors while gathering packages to took from generators for owner object %s."), *GetFullNameSafe(OwnerObject));

	UE_LOG(LogWorldPartition, Display, TEXT("[Cook] Gathered %u packages to generate from %u Generators."), CookContext.NumPackageToGenerate(), CookContext.NumGenerators());

	TArray<ICookPackageSplitter::FGeneratedPackage> PackagesToGenerate;
	BuildPackagesToGenerateList(PackagesToGenerate);

	UE_LOG(LogWorldPartition, Display, TEXT("[Cook] Sending %u packages to be generated."), PackagesToGenerate.Num());
	UE_LOG(LogWorldPartition, Display, TEXT("[Cook] Debug(GetGenerateList) : OwnerObject=%s bForceInitializedWorld=%d bInitializedPhysicsSceneForSave=%d"), *GetFullNameSafe(OwnerObject), bForceInitializedWorld ? 1 : 0, bInitializedPhysicsSceneForSave ? 1 : 0);

	FWorldDelegates::OnWorldCleanup.AddRaw(this, &FWorldPartitionCookPackageSplitter::OnWorldCleanup);

	return PackagesToGenerate;
}

bool FWorldPartitionCookPackageSplitter::PopulateGeneratedPackage(UPackage* OwnerPackage, UObject* OwnerObject,
	const FGeneratedPackageForPopulate& GeneratedPackage, TArray<UObject*>& OutObjectsToMove,
	TArray<UPackage*>& OutModifiedPackages)
{
	UE_LOG(LogWorldPartition, Verbose, TEXT("[Cook][PopulateGeneratedPackage] Processing %s"), *FWorldPartitionCookPackage::MakeGeneratedFullPath(GeneratedPackage.GeneratedRootPath, GeneratedPackage.RelativePath));

	bool bIsSuccess = true;

	IWorldPartitionCookPackageGenerator* CookPackageGenerator = nullptr;
	FWorldPartitionCookPackage* CookPackage = nullptr;
	TArray<UPackage*> ModifiedPackages;
	if (CookContext.GetCookPackageGeneratorAndPackage(GeneratedPackage.GeneratedRootPath, GeneratedPackage.RelativePath, CookPackageGenerator, CookPackage))
	{
		bIsSuccess = CookPackageGenerator->PopulateGeneratedPackageForCook(CookContext, *CookPackage, ModifiedPackages);
	}
	else
	{
		UE_LOG(LogWorldPartition, Error, TEXT("[Cook][PopulateGeneratedPackage] Could not find WorldPartitionCookPackage for %s"), *FWorldPartitionCookPackage::MakeGeneratedFullPath(GeneratedPackage.GeneratedRootPath, GeneratedPackage.RelativePath));
		bIsSuccess = false;
	}

	UE_LOG(LogWorldPartition, Verbose, TEXT("[Cook][PopulateGeneratedPackage] Gathered %u modified packages for %s"), ModifiedPackages.Num() , *FWorldPartitionCookPackage::MakeGeneratedFullPath(GeneratedPackage.GeneratedRootPath, GeneratedPackage.RelativePath));
	OutModifiedPackages = MoveTemp(ModifiedPackages);

	return bIsSuccess;
}

bool FWorldPartitionCookPackageSplitter::PopulateGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject,
	const TArray<ICookPackageSplitter::FGeneratedPackageForPreSave>& GeneratedPackages, TArray<UObject*>& OutObjectsToMove,
	TArray<UPackage*>& OutModifiedPackages)
{
	UE_LOG(LogWorldPartition, Display, TEXT("[Cook][PopulateGeneratorPackage] Processing %u packages"), GeneratedPackages.Num());

	bool bIsSuccess = true;
	if (GeneratedPackages.Num() != CookContext.NumPackageToGenerate())
	{
		UE_LOG(LogWorldPartition, Error, TEXT("[Cook][PopulateGeneratorPackage] Receieved %u generated packages. Was expecting %u"), GeneratedPackages.Num(), CookContext.NumPackageToGenerate());
		bIsSuccess = false;
	}

	TArray<UPackage*> ModifiedPackages;
	for (IWorldPartitionCookPackageGenerator* CookPackageGenerator : CookContext.GetCookPackageGenerators())
	{
		bIsSuccess &= CookPackageGenerator->PrepareGeneratorPackageForCook(CookContext, ModifiedPackages);
		if (const TArray<FWorldPartitionCookPackage*>* CookPackages = CookContext.GetCookPackages(CookPackageGenerator))
		{
			bIsSuccess &= CookPackageGenerator->PopulateGeneratorPackageForCook(CookContext, *CookPackages, ModifiedPackages);
		}
	}

	UE_LOG(LogWorldPartition, Display, TEXT("[Cook][PopulateGeneratorPackage] Gathered %u modified packages"), ModifiedPackages.Num());
	OutModifiedPackages = MoveTemp(ModifiedPackages);

	return bIsSuccess;
}

void FWorldPartitionCookPackageSplitter::OnOwnerReloaded(UPackage* OwnerPackage, UObject* OwnerObject)
{
	// It should not be possible for the owner to reload due to garbage collection while we are active and keeping it referenced
	check(!ReferencedWorld);
}

void FWorldPartitionCookPackageSplitter::BuildPackagesToGenerateList(TArray<ICookPackageSplitter::FGeneratedPackage>& PackagesToGenerate) const
{
	for (const IWorldPartitionCookPackageGenerator* CookPackageGenerator : CookContext.GetCookPackageGenerators())
	{
		if (const TArray<FWorldPartitionCookPackage*>* CookPackages = CookContext.GetCookPackages(CookPackageGenerator))
		{
			PackagesToGenerate.Reserve(CookPackages->Num());

			for (const FWorldPartitionCookPackage* CookPackage : *CookPackages)
			{
				ICookPackageSplitter::FGeneratedPackage& GeneratedPackage = PackagesToGenerate.Emplace_GetRef();
				GeneratedPackage.GeneratedRootPath = CookPackage->Root;
				GeneratedPackage.RelativePath = CookPackage->RelativePath;
				// GenerationHash is left as empty. All dependencies for the package's bytes are specified by PackageDependencies

				CookPackage->Type == FWorldPartitionCookPackage::EType::Level ? GeneratedPackage.SetCreateAsMap(true) : GeneratedPackage.SetCreateAsMap(false);

				// Fill generated package dependencies for iterative cooking
				if (UWorldPartitionRuntimeCell* Cell = CookPackageGenerator->GetCellForPackage(*CookPackage))
				{
					TSet<FName> ActorPackageNames = Cell->GetActorPackageNames();
					GeneratedPackage.PackageDependencies.Reset(ActorPackageNames.Num());
					for (FName ActorPackageName : ActorPackageNames)
					{
						GeneratedPackage.PackageDependencies.Add(FAssetDependency::PackageDependency(ActorPackageName,
							UE::AssetRegistry::EDependencyProperty::Hard | UE::AssetRegistry::EDependencyProperty::Game));
					}
				}
			}
		}
	}
}

#endif
