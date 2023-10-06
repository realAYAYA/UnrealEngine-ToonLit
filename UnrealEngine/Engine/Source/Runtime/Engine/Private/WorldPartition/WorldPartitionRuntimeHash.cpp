// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/HierarchicalLogArchive.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescView.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "Misc/ArchiveMD5.h"
#if WITH_EDITOR
#include "WorldPartition/Cook/WorldPartitionCookPackage.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeHash)

#define LOCTEXT_NAMESPACE "WorldPartition"

void URuntimeHashExternalStreamingObjectBase::ForEachStreamingCells(TFunctionRef<void(UWorldPartitionRuntimeCell&)> Func)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(this, Objects);

	for (UObject* Object : Objects)
	{
		if (UWorldPartitionRuntimeCell* Cell = Cast<UWorldPartitionRuntimeCell>(Object))
		{
			Func(*Cell);
		}
	}
}

void URuntimeHashExternalStreamingObjectBase::OnStreamingObjectLoaded(UWorld* InjectedWorld)
{
	bool bIsACookedObject = !CellToLevelStreamingPackage.IsEmpty();
	if (bIsACookedObject)
	{
		// Cooked streaming object's Cells do not have LevelStreaming.
		ForEachStreamingCells([this](UWorldPartitionRuntimeCell& Cell)
		{
			UWorldPartitionRuntimeLevelStreamingCell* RuntimeCell = CastChecked<UWorldPartitionRuntimeLevelStreamingCell>(&Cell);

			FName LevelStreamingPackage = CellToLevelStreamingPackage.FindChecked(RuntimeCell->GetFName());
			RuntimeCell->CreateAndSetLevelStreaming(*LevelStreamingPackage.ToString());
		});
	}
}

#if WITH_EDITOR
void URuntimeHashExternalStreamingObjectBase::PopulateGeneratorPackageForCook()
{
	ForEachStreamingCells([this](UWorldPartitionRuntimeCell& Cell)
	{
		UWorldPartitionRuntimeLevelStreamingCell* RuntimeCell = CastChecked<UWorldPartitionRuntimeLevelStreamingCell>(&Cell);
		UWorldPartitionLevelStreamingDynamic* LevelStreamingDynamic = RuntimeCell->GetLevelStreaming();
		CellToLevelStreamingPackage.Add(RuntimeCell->GetFName(), LevelStreamingDynamic->PackageNameToLoad);

		// Level streaming are outered to the world and would not be saved within the ExternalStreamingObject.
		// Do not save them, instead they will be created once the external streaming object is loaded at runtime. 
		LevelStreamingDynamic->SetFlags(RF_Transient);
	});
}
#endif

UWorldPartitionRuntimeHash::UWorldPartitionRuntimeHash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

UWorldPartitionRuntimeCell* UWorldPartitionRuntimeHash::CreateRuntimeCell(UClass* CellClass, UClass* CellDataClass, const FString& CellName, const FString& CellInstanceSuffix, UObject* InOuter)
{
	//@todo_ow: to reduce file paths on windows, we compute a MD5 hash from the unique cell name and use that hash as the cell filename. We
	//			should create the runtime cell using the name but make sure the package that gets associated with it when cooking gets the
	//			hash indtead.
	auto GetCellObjectName = [](FString CellName) -> FString
	{
		FArchiveMD5 ArMD5;
		ArMD5 << CellName;
		FGuid CellNameGuid = ArMD5.GetGuidFromHash();
		check(CellNameGuid.IsValid());

		return CellNameGuid.ToString(EGuidFormats::Base36Encoded);
	};

	// Cooking should have an empty CellInstanceSuffix
	check(!IsRunningCookCommandlet() || CellInstanceSuffix.IsEmpty());
	const FString CellObjectName = GetCellObjectName(CellName) + CellInstanceSuffix;
	// Use given outer if provided, else use hash as outer
	UObject* Outer = InOuter ? InOuter : this;
	UWorldPartitionRuntimeCell* RuntimeCell = NewObject<UWorldPartitionRuntimeCell>(Outer, CellClass, *CellObjectName);
	RuntimeCell->RuntimeCellData = NewObject<UWorldPartitionRuntimeCellData>(RuntimeCell, CellDataClass);
	return RuntimeCell;
}

URuntimeHashExternalStreamingObjectBase* UWorldPartitionRuntimeHash::CreateExternalStreamingObject(TSubclassOf<URuntimeHashExternalStreamingObjectBase> InClass, UObject* InOuter, FName InName, UWorld* InOwningWorld, UWorld* InOuterWorld)
{
	URuntimeHashExternalStreamingObjectBase* StreamingObject = NewObject<URuntimeHashExternalStreamingObjectBase>(InOuter, InClass, InName, RF_Public);
	StreamingObject->OwningWorld = InOwningWorld;
	StreamingObject->OuterWorld = InOuterWorld;
	return StreamingObject;
}

#if WITH_EDITOR
void UWorldPartitionRuntimeHash::OnBeginPlay()
{
	// Mark always loaded actors so that the Level will force reference to these actors for PIE.
	// These actor will then be duplicated for PIE during the PIE world duplication process
	ForceExternalActorLevelReference(/*bForceExternalActorLevelReferenceForPIE*/true);
}

void UWorldPartitionRuntimeHash::OnEndPlay()
{
	// Unmark always loaded actors
	ForceExternalActorLevelReference(/*bForceExternalActorLevelReferenceForPIE*/false);

	// Release references (will unload actors that were not already loaded in the Editor)
	AlwaysLoadedActorsForPIE.Empty();

	ModifiedActorDescListForPIE.Empty();
}

bool UWorldPartitionRuntimeHash::GenerateStreaming(class UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate)
{
	return PackagesToGenerateForCook.IsEmpty();
}

void UWorldPartitionRuntimeHash::FlushStreaming()
{
	PackagesToGenerateForCook.Empty();
}

// In PIE, Always loaded cell is not generated. Instead, always loaded actors will be added to AlwaysLoadedActorsForPIE.
// This will trigger loading/registration of these actors in the PersistentLevel (if not already loaded).
// Then, duplication of world for PIE will duplicate only these actors. 
// When stopping PIE, WorldPartition will release these FWorldPartitionReferences which 
// will unload actors that were not already loaded in the non PIE world.
bool UWorldPartitionRuntimeHash::ConditionalRegisterAlwaysLoadedActorsForPIE(const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance, bool bIsMainWorldPartition, bool bIsMainContainer, bool bIsCellAlwaysLoaded)
{
	if (bIsMainWorldPartition && bIsMainContainer && bIsCellAlwaysLoaded && !IsRunningCookCommandlet())
	{
		ActorSetInstance->ForEachActor([this, ActorSetInstance](const FGuid& ActorGuid)
		{
			IStreamingGenerationContext::FActorInstance ActorInstance(ActorGuid, ActorSetInstance);
			const FWorldPartitionActorDescView& ActorDescView = ActorInstance.GetActorDescView();

			// This will load the actor if it isn't already loaded
			FWorldPartitionReference Reference(GetOuterUWorldPartition(), ActorDescView.GetGuid());

			if (AActor* AlwaysLoadedActor = FindObject<AActor>(nullptr, *ActorDescView.GetActorSoftPath().ToString()))
			{
				AlwaysLoadedActorsForPIE.Emplace(Reference, AlwaysLoadedActor);
			}
		});

		return true;
	}

	return false;
}

bool UWorldPartitionRuntimeHash::PopulateCellActorInstances(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, bool bIsMainWorldPartition, bool bIsCellAlwaysLoaded, TArray<IStreamingGenerationContext::FActorInstance>& OutCellActorInstances)
{
	for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : ActorSetInstances)
	{
		// Instanced world partition, ContainerID is the main container, but it's not the main world partition,
		// so the always loaded actors don't be part of the process of ForceExternalActorLevelReference/AlwaysLoadedActorsForPIE.
		// In PIE, always loaded actors of an instanced world partition will go in the always loaded cell.
		if (!ConditionalRegisterAlwaysLoadedActorsForPIE(ActorSetInstance, bIsMainWorldPartition, ActorSetInstance->ContainerID.IsMainContainer(), bIsCellAlwaysLoaded))
		{
			ActorSetInstance->ForEachActor([this, ActorSetInstance, &OutCellActorInstances](const FGuid& ActorGuid)
			{
				// Actors that return true to ShouldLevelKeepRefIfExternal will always be part of the partitioned persistent level of a
				// world partition. In PIE, for an instanced world partition, we don't want this actor to be both in the persistent level
				// and also part of the always loaded cell level.
				//
				// @todo_ow: We need to implement PIE always loaded actors of instanced world partitions to be part
				//			 of the persistent level and get rid of the always loaded cell (to have the same behavior
				//			 as non-instanced world partition and as cooked world partition).
				IStreamingGenerationContext::FActorInstance ActorInstance(ActorGuid, ActorSetInstance);
				if (!CastChecked<AActor>(ActorInstance.GetActorDescView().GetActorNativeClass()->GetDefaultObject())->ShouldLevelKeepRefIfExternal())
				{
					OutCellActorInstances.Emplace(ActorInstance);
				}
			});
		}
	}

	return OutCellActorInstances.Num() > 0;
}

void UWorldPartitionRuntimeHash::PopulateRuntimeCell(UWorldPartitionRuntimeCell* RuntimeCell, const TArray<IStreamingGenerationContext::FActorInstance>& ActorInstances, TArray<FString>* OutPackagesToGenerate)
{
	for (const IStreamingGenerationContext::FActorInstance& ActorInstance : ActorInstances)
	{
		if (ActorInstance.GetContainerID().IsMainContainer())
		{
			const FWorldPartitionActorDescView& ActorDescView = ActorInstance.GetActorDescView();
			if (AActor* Actor = FindObject<AActor>(nullptr, *ActorDescView.GetActorSoftPath().ToString()))
			{
				if (ModifiedActorDescListForPIE.GetActorDesc(ActorDescView.GetGuid()))
				{
					// Create an actor container to make sure duplicated actors will share an outer to properly remap inter-actors references
					RuntimeCell->UnsavedActorsContainer = NewObject<UActorContainer>(RuntimeCell);
					break;
				}
			}
		}
	}

	FBox CellContentBounds(ForceInit);
	for (const IStreamingGenerationContext::FActorInstance& ActorInstance : ActorInstances)
	{
		const FWorldPartitionActorDescView& ActorDescView = ActorInstance.GetActorDescView();
		RuntimeCell->AddActorToCell(ActorDescView, ActorInstance.GetContainerID(), ActorInstance.GetTransform(), ActorInstance.GetActorDescContainer());
		const FBox RuntimeBounds = ActorDescView.GetRuntimeBounds();
		if (RuntimeBounds.IsValid)
		{
			CellContentBounds += RuntimeBounds.TransformBy(ActorInstance.GetTransform());
		}
					
		if (ActorInstance.GetContainerID().IsMainContainer() && RuntimeCell->UnsavedActorsContainer)
		{
			if (AActor* Actor = FindObject<AActor>(nullptr, *ActorDescView.GetActorSoftPath().ToString()))
			{
				RuntimeCell->UnsavedActorsContainer->Actors.Add(Actor->GetFName(), Actor);

				// Handle child actors
				Actor->ForEachComponent<UChildActorComponent>(true, [RuntimeCell](UChildActorComponent* ChildActorComponent)
				{
					if (AActor* ChildActor = ChildActorComponent->GetChildActor())
					{
						RuntimeCell->UnsavedActorsContainer->Actors.Add(ChildActor->GetFName(), ChildActor);
					}
				});
			}
		}
	}

	RuntimeCell->RuntimeCellData->ContentBounds = CellContentBounds;
	RuntimeCell->Fixup();

	// Always loaded cell actors are transfered to World's Persistent Level (see UWorldPartitionRuntimeSpatialHash::PopulateGeneratorPackageForCook)
	if (OutPackagesToGenerate && RuntimeCell->GetActorCount() && !RuntimeCell->IsAlwaysLoaded())
	{
		const FString PackageRelativePath = RuntimeCell->GetPackageNameToCreate();
		check(!PackageRelativePath.IsEmpty());

		OutPackagesToGenerate->Add(PackageRelativePath);

		// Map relative package to StreamingCell for PopulateGeneratedPackageForCook/PopulateGeneratorPackageForCook/GetCellForPackage
		PackagesToGenerateForCook.Add(PackageRelativePath, RuntimeCell);
	}
}

bool UWorldPartitionRuntimeHash::PopulateGeneratedPackageForCook(const FWorldPartitionCookPackage& InPackagesToCook, TArray<UPackage*>& OutModifiedPackages)
{
	OutModifiedPackages.Reset();
	if (UWorldPartitionRuntimeCell** MatchingCell = PackagesToGenerateForCook.Find(InPackagesToCook.RelativePath))
	{
		UWorldPartitionRuntimeCell* Cell = *MatchingCell;
		if (ensure(Cell))
		{
			return Cell->PopulateGeneratedPackageForCook(InPackagesToCook.GetPackage(), OutModifiedPackages);
		}
	}
	return false;
}

UWorldPartitionRuntimeCell* UWorldPartitionRuntimeHash::GetCellForPackage(const FWorldPartitionCookPackage& PackageToCook) const
{
	UWorldPartitionRuntimeCell** MatchingCell = const_cast<UWorldPartitionRuntimeCell**>(PackagesToGenerateForCook.Find(PackageToCook.RelativePath));
	return MatchingCell ? *MatchingCell : nullptr;
}

TArray<UWorldPartitionRuntimeCell*> UWorldPartitionRuntimeHash::GetAlwaysLoadedCells() const
{
	TArray<UWorldPartitionRuntimeCell*> Result;
	ForEachStreamingCells([&Result](const UWorldPartitionRuntimeCell* Cell)
	{
		if (Cell->IsAlwaysLoaded())
		{
			Result.Add(const_cast<UWorldPartitionRuntimeCell*>(Cell));
		}
		return true;
	});
	return Result;
}

bool UWorldPartitionRuntimeHash::PrepareGeneratorPackageForCook(TArray<UPackage*>& OutModifiedPackages)
{
	check(IsRunningCookCommandlet());

	for (UWorldPartitionRuntimeCell* Cell : GetAlwaysLoadedCells())
	{
		check(Cell->IsAlwaysLoaded());
		if (!Cell->PopulateGeneratorPackageForCook(OutModifiedPackages))
		{
			return false;
		}
	}

	//@todo_ow: here we can safely remove always loaded cells as they are not part of the OutPackagesToGenerate
	return true;
}

bool UWorldPartitionRuntimeHash::PopulateGeneratorPackageForCook(const TArray<FWorldPartitionCookPackage*>& InPackagesToCook, TArray<UPackage*>& OutModifiedPackages)
{
	check(IsRunningCookCommandlet());

	for (const FWorldPartitionCookPackage* CookPackage : InPackagesToCook)
	{
		UWorldPartitionRuntimeCell** MatchingCell = PackagesToGenerateForCook.Find(CookPackage->RelativePath);
		UWorldPartitionRuntimeCell* Cell = MatchingCell ? *MatchingCell : nullptr;
		if (!Cell || !Cell->PrepareCellForCook(CookPackage->GetPackage()))
		{
			return false;
		}
	}
	return true;
}

void UWorldPartitionRuntimeHash::DumpStateLog(FHierarchicalLogArchive& Ar) const
{
	if (!GIsAutomationTesting)
	{
		Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
		Ar.Printf(TEXT("%s - Persistent Level"), *GetWorld()->GetName());
		Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
		{
			FHierarchicalLogArchive::FIndentScope CellIndentScope = Ar.PrintfIndent(TEXT("Content of %s Persistent Level"), *GetWorld()->GetName());

			TArray<const AActor*> Actors;
			for (const AActor* Actor : GetWorld()->PersistentLevel->Actors)
			{
				if (Actor)
				{
					Actors.Add(Actor);
				}
			}

			Actors.Sort([this](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });

			Ar.Printf(TEXT("Always loaded Actor Count: %d "), Actors.Num());
			for (const AActor* Actor : Actors)
			{
				Ar.Printf(TEXT("Actor Path: %s"), *Actor->GetPathName());
				Ar.Printf(TEXT("Actor Package: %s"), *Actor->GetPackage()->GetName());
			}
		}
		Ar.Printf(TEXT(""));
	}
}

void UWorldPartitionRuntimeHash::ForceExternalActorLevelReference(bool bForceExternalActorLevelReferenceForPIE)
{
	// Do this only on non game worlds prior to PIE so that always loaded actors get duplicated with the world
	if (!GetWorld()->IsGameWorld())
	{
		for (const FAlwaysLoadedActorForPIE& AlwaysLoadedActor : AlwaysLoadedActorsForPIE)
		{
			if (AActor* Actor = AlwaysLoadedActor.Actor.Get())
			{
				Actor->SetForceExternalActorLevelReferenceForPIE(bForceExternalActorLevelReferenceForPIE);
			}
		}
	}
}
#endif

int32 UWorldPartitionRuntimeHash::GetAllStreamingCells(TSet<const UWorldPartitionRuntimeCell*>& Cells, bool bAllDataLayers, bool bDataLayersOnly, const TSet<FName>& InDataLayers) const
{
	ForEachStreamingCells([&Cells, bAllDataLayers, bDataLayersOnly, InDataLayers](const UWorldPartitionRuntimeCell* Cell)
	{
		if (!bDataLayersOnly && !Cell->HasDataLayers())
		{
			Cells.Add(Cell);
		}
		else if (Cell->HasDataLayers() && (bAllDataLayers || Cell->HasAnyDataLayer(InDataLayers)))
		{
			Cells.Add(Cell);
		}
		return true;
	});

	return Cells.Num();
}

bool UWorldPartitionRuntimeHash::GetStreamingCells(const FWorldPartitionStreamingQuerySource& QuerySource, TSet<const UWorldPartitionRuntimeCell*>& OutCells) const
{
	ForEachStreamingCellsQuery(QuerySource, [QuerySource, &OutCells](const UWorldPartitionRuntimeCell* Cell)
	{
		OutCells.Add(Cell);
		return true;
	});

	return !!OutCells.Num();
}

bool UWorldPartitionRuntimeHash::GetStreamingCells(const TArray<FWorldPartitionStreamingSource>& Sources, FStreamingSourceCells& OutActivateCells, FStreamingSourceCells& OutLoadCells) const
{
	ForEachStreamingCellsSources(Sources, [&OutActivateCells, &OutLoadCells](const UWorldPartitionRuntimeCell* Cell, EStreamingSourceTargetState TargetState)
	{
		switch (TargetState)
		{
		case EStreamingSourceTargetState::Loaded:
			OutLoadCells.GetCells().Add(Cell);
			break;
		case EStreamingSourceTargetState::Activated:
			OutActivateCells.GetCells().Add(Cell);
			break;
		}
		return true;
	});

	return !!(OutActivateCells.Num() + OutLoadCells.Num());
}

bool UWorldPartitionRuntimeHash::IsCellRelevantFor(bool bClientOnlyVisible) const
{
	if (bClientOnlyVisible)
	{
		const UWorld* World = GetWorld();
		if (World->IsGameWorld())
		{
			// Dedicated server & listen server without server streaming won't consider client-only visible cells
			const ENetMode NetMode = World->GetNetMode();
			if ((NetMode == NM_DedicatedServer) || ((NetMode == NM_ListenServer) && !GetOuterUWorldPartition()->IsServerStreamingEnabled()))
			{
				return false;
			}
		}
	}
	return true;
}

EWorldPartitionStreamingPerformance UWorldPartitionRuntimeHash::GetStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& CellsToActivate) const
{
	EWorldPartitionStreamingPerformance StreamingPerformance = EWorldPartitionStreamingPerformance::Good;

	if (!CellsToActivate.IsEmpty() && GetWorld()->bMatchStarted)
	{
		UWorld* World = GetWorld();

		for (const UWorldPartitionRuntimeCell* Cell : CellsToActivate)
		{
			if (Cell->GetBlockOnSlowLoading() && !Cell->IsAlwaysLoaded() && Cell->GetStreamingStatus() != LEVEL_Visible)
			{
				EWorldPartitionStreamingPerformance CellPerformance = GetStreamingPerformanceForCell(Cell);
				// Cell Performance is worst than previous cell performance
				if (CellPerformance > StreamingPerformance)
				{
					StreamingPerformance = CellPerformance;
					// Early out performance is critical
					if (StreamingPerformance == EWorldPartitionStreamingPerformance::Critical)
					{
						return StreamingPerformance;
					}
				}
			}
		}
	}

	return StreamingPerformance;
}

bool UWorldPartitionRuntimeHash::InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject)
{
	check(InExternalStreamingObject);
	bool bAlreadyInSet = false;
	InjectedExternalStreamingObjects.Add(InExternalStreamingObject, &bAlreadyInSet);
	if (bAlreadyInSet)
	{
		UE_LOG(LogWorldPartition, Warning, TEXT("External streaming object %s already injected."), *InExternalStreamingObject->GetName());
		return false;
	}
	return true;
}

bool UWorldPartitionRuntimeHash::RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject)
{
	check(InExternalStreamingObject);
	if (!InjectedExternalStreamingObjects.Remove(InExternalStreamingObject))
	{
		UE_LOG(LogWorldPartition, Warning, TEXT("External streaming object %s was not injected."), *InExternalStreamingObject->GetName());
		return false;
	}
	return true;
}

void UWorldPartitionRuntimeHash::FStreamingSourceCells::AddCell(const UWorldPartitionRuntimeCell* Cell, const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape)
{
	if (Cell->ShouldResetStreamingSourceInfo())
	{
		Cell->ResetStreamingSourceInfo();
	}

	Cell->AppendStreamingSourceInfo(Source, SourceShape);
	Cells.Add(Cell);
}

void FWorldPartitionQueryCache::AddCellInfo(const UWorldPartitionRuntimeCell* Cell, const FSphericalSector& SourceShape)
{
	const double SquareDistance = FVector::DistSquared2D(SourceShape.GetCenter(), Cell->GetContentBounds().GetCenter());
	if (double* ExistingSquareDistance = CellToSourceMinSqrDistances.Find(Cell))
	{
		*ExistingSquareDistance = FMath::Min(*ExistingSquareDistance, SquareDistance);
	}
	else
	{
		CellToSourceMinSqrDistances.Add(Cell, SquareDistance);
	}
}

double FWorldPartitionQueryCache::GetCellMinSquareDist(const UWorldPartitionRuntimeCell* Cell) const
{
	const double* Dist = CellToSourceMinSqrDistances.Find(Cell);
	return Dist ? *Dist : MAX_dbl;
}

#undef LOCTEXT_NAMESPACE
