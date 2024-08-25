// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/HierarchicalLogArchive.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerHelper.h"
#include "Misc/ArchiveMD5.h"
#if WITH_EDITOR
#include "WorldPartition/Cook/WorldPartitionCookPackage.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeHash)

#define LOCTEXT_NAMESPACE "WorldPartition"

ENGINE_API float GBlockOnSlowStreamingRatio = 0.25f;
static FAutoConsoleVariableRef CVarBlockOnSlowStreamingRatio(
	TEXT("wp.Runtime.BlockOnSlowStreamingRatio"),
	GBlockOnSlowStreamingRatio,
	TEXT("Ratio of DistanceToCell / LoadingRange to use to determine if World Partition streaming needs to block"));

ENGINE_API float GBlockOnSlowStreamingWarningFactor = 2.f;
static FAutoConsoleVariableRef CVarBlockOnSlowStreamingWarningFactor(
	TEXT("wp.Runtime.BlockOnSlowStreamingWarningFactor"),
	GBlockOnSlowStreamingWarningFactor,
	TEXT("Factor of wp.Runtime.BlockOnSlowStreamingRatio we want to start notifying the user"));

#if DO_CHECK
void URuntimeHashExternalStreamingObjectBase::BeginDestroy()
{
	checkf(!TargetInjectedWorldPartition.Get(), TEXT("Destroying external streaming object that is still injected."));
	Super::BeginDestroy();
}
#endif

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

TSet<TObjectPtr<UDataLayerInstance>>& URuntimeHashExternalStreamingObjectBase::GetDataLayerInstances()
{
	return DataLayerInstances;
}

const UObject* URuntimeHashExternalStreamingObjectBase::GetLevelMountPointContextObject() const
{
	return GetRootExternalDataLayerAsset();
}

UWorld* URuntimeHashExternalStreamingObjectBase::GetOwningWorld() const
{
	// Once OnStreamingObjectLoaded is called and OwningWorld is set, use this cached value
	return OwningWorld.IsSet() ? OwningWorld.GetValue().Get() : GetOuterWorld()->GetWorldPartition()->GetWorld();
}

void URuntimeHashExternalStreamingObjectBase::OnStreamingObjectLoaded(UWorld* InjectedWorld)
{
#if !WITH_EDITOR
	if (!CellToStreamingData.IsEmpty())
	{
		// Cooked streaming object's Cells do not have LevelStreaming.
		ForEachStreamingCells([this](UWorldPartitionRuntimeCell& Cell)
		{
			UWorldPartitionRuntimeLevelStreamingCell* RuntimeCell = CastChecked<UWorldPartitionRuntimeLevelStreamingCell>(&Cell);

			const FWorldPartitionRuntimeCellStreamingData& CellStreamingData = CellToStreamingData.FindChecked(RuntimeCell->GetFName());
			RuntimeCell->CreateAndSetLevelStreaming(CellStreamingData.PackageName, CellStreamingData.WorldAsset);
		});
	}
#endif

	check(GetOuterWorld());
	check(GetOuterWorld()->GetWorldPartition());
	check(GetOuterWorld()->GetWorldPartition()->GetWorld());
	OwningWorld = GetOuterWorld()->GetWorldPartition()->GetWorld();
}

#if WITH_EDITOR
UWorldPartitionRuntimeCell* URuntimeHashExternalStreamingObjectBase::GetCellForCookPackage(const FString& InCookPackageName) const
{
	if (const TObjectPtr<UWorldPartitionRuntimeCell>* MatchingCell = PackagesToGenerateForCook.Find(InCookPackageName))
	{
		if (ensure(*MatchingCell))
		{
			return const_cast<UWorldPartitionRuntimeCell*>(ToRawPtr(*MatchingCell));
		}
	}
	return nullptr;
}

FString URuntimeHashExternalStreamingObjectBase::GetPackageNameToCreate() const
{
	// GetPackageNameToCreate should not be called for ExternalStreamingObjects
	if (ensure(ExternalDataLayerAsset))
	{
		return TEXT("/") + FExternalDataLayerHelper::GetExternalStreamingObjectPackageName(ExternalDataLayerAsset);
	}
	return FString();
}

bool URuntimeHashExternalStreamingObjectBase::OnPopulateGeneratorPackageForCook(UPackage* InPackage)
{
	ForEachStreamingCells([this](UWorldPartitionRuntimeCell& Cell)
	{
		UWorldPartitionRuntimeLevelStreamingCell* RuntimeCell = CastChecked<UWorldPartitionRuntimeLevelStreamingCell>(&Cell);
		UWorldPartitionLevelStreamingDynamic* LevelStreamingDynamic = RuntimeCell->GetLevelStreaming();
		FWorldPartitionRuntimeCellStreamingData& CellStreamingData = CellToStreamingData.Add(RuntimeCell->GetFName());
		CellStreamingData.PackageName = LevelStreamingDynamic->GetWorldAsset().GetLongPackageName();
		// SoftObjectPath will be automatically remapped when ExternalStreamingObject will be instanced/loaded at runtime
		CellStreamingData.WorldAsset = LevelStreamingDynamic->GetWorldAsset().ToSoftObjectPath();

		// Level streaming are outered to the world and would not be saved within the ExternalStreamingObject.
		// Do not save them, instead they will be created once the external streaming object is loaded at runtime. 
		LevelStreamingDynamic->SetFlags(RF_Transient);
	});
	return true;
}

bool URuntimeHashExternalStreamingObjectBase::OnPopulateGeneratedPackageForCook(UPackage* InPackage, TArray<UPackage*>& OutModifiedPackages)
{
	return Rename(nullptr, InPackage, REN_DontCreateRedirectors);
}

void URuntimeHashExternalStreamingObjectBase::DumpStateLog(FHierarchicalLogArchive& Ar)
{
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("%s%s"), *GetWorld()->GetName(), ExternalDataLayerAsset ? *FString::Printf(TEXT(" - External Data Layer - %s"), *ExternalDataLayerAsset->GetName()) : TEXT(""));
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
}

TMap<TPair<const UClass*, const UClass*>, UWorldPartitionRuntimeHash::FRuntimeHashConvertFunc> UWorldPartitionRuntimeHash::WorldPartitionRuntimeHashConverters;
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

EWorldPartitionStreamingPerformance UWorldPartitionRuntimeHash::GetStreamingPerformanceForCell(const UWorldPartitionRuntimeCell* Cell) const
{
	check(Cell->GetBlockOnSlowLoading());

	if (Cell->RuntimeCellData->bCachedWasRequestedByBlockingSource)
	{
		if (Cell->RuntimeCellData->CachedMinBlockOnSlowStreamingRatio < GBlockOnSlowStreamingRatio)
		{
			return EWorldPartitionStreamingPerformance::Critical;
		}
		else if (Cell->RuntimeCellData->CachedMinBlockOnSlowStreamingRatio < (GBlockOnSlowStreamingRatio * GBlockOnSlowStreamingWarningFactor))
		{
			return EWorldPartitionStreamingPerformance::Slow;
		}
	}

	return EWorldPartitionStreamingPerformance::Good;
}

URuntimeHashExternalStreamingObjectBase* UWorldPartitionRuntimeHash::CreateExternalStreamingObject(TSubclassOf<URuntimeHashExternalStreamingObjectBase> InClass, UObject* InOuter, FName InName, UWorld* InOuterWorld)
{
	if (FindObject<URuntimeHashExternalStreamingObjectBase>(InOuter, *InName.ToString()))
	{
		UE_LOG(LogWorldPartition, Warning, TEXT("UWorldPartitionRuntimeHash::CreateExternalStreamingObject can't create an already existing URuntimeHashExternalStreamingObjectBase object named %s"), *InName.ToString());
		return nullptr;
	}

	URuntimeHashExternalStreamingObjectBase* StreamingObject = NewObject<URuntimeHashExternalStreamingObjectBase>(InOuter, InClass, InName, RF_Public);
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
	{
		FWorldPartitionLoadingContext::FDeferred LoadingContext;
		AlwaysLoadedActorsForPIE.Empty();
	}
}

bool UWorldPartitionRuntimeHash::GenerateStreaming(class UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate)
{
	return PackagesToGenerateForCook.IsEmpty();
}

void UWorldPartitionRuntimeHash::FlushStreamingContent()
{
	PackagesToGenerateForCook.Empty();
}

bool UWorldPartitionRuntimeHash::PopulateCellActorInstances(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, bool bIsMainWorldPartition, bool bIsCellAlwaysLoaded, TArray<IStreamingGenerationContext::FActorInstance>& OutCellActorInstances)
{
	// In PIE, Always loaded cell is not generated. Instead, always loaded actors will be added to AlwaysLoadedActorsForPIE.
	// This will trigger loading/registration of these actors in the PersistentLevel (if not already loaded).
	// Then, duplication of world for PIE will duplicate only these actors. 
	// When stopping PIE, WorldPartition will release these FWorldPartitionReferences which 
	// will unload actors that were not already loaded in the non PIE world.
	TArray<TPair<FWorldPartitionReference, const IWorldPartitionActorDescInstanceView*>> AlwaysLoadedReferences;
	{
		FWorldPartitionLoadingContext::FDeferred LoadingContext;

		const bool bForceLoadAlwaysLoadedReferences = bIsMainWorldPartition && bIsCellAlwaysLoaded && !IsRunningCookCommandlet();

		for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : ActorSetInstances)
		{
			ActorSetInstance->ForEachActor([this, ActorSetInstance, &AlwaysLoadedReferences, &OutCellActorInstances, bForceLoadAlwaysLoadedReferences](const FGuid& ActorGuid)
			{
				IStreamingGenerationContext::FActorInstance ActorInstance(ActorGuid, ActorSetInstance);
				const IWorldPartitionActorDescInstanceView& ActorDescView = ActorInstance.GetActorDescView();

				// Instanced world partition, ContainerID is the main container, but it's not the main world partition,
				// so the always loaded actors don't be part of the process of ForceExternalActorLevelReference/AlwaysLoadedActorsForPIE.
				// In PIE, always loaded actors of an instanced world partition will go in the always loaded cell.
				if (bForceLoadAlwaysLoadedReferences && ActorSetInstance->ContainerID.IsMainContainer())
				{
					// This will load the actor if it isn't already loaded, when the deferred context ends.
					AlwaysLoadedReferences.Emplace(FWorldPartitionReference(GetOuterUWorldPartition(), ActorDescView.GetGuid()), &ActorDescView);
				}
				else
				{
					// Actors that return true to ShouldLevelKeepRefIfExternal will always be part of the partitioned persistent level of a
					// world partition. In PIE, for an instanced world partition, we don't want this actor to be both in the persistent level
					// and also part of the always loaded cell level.
					//
					// @todo_ow: We need to implement PIE always loaded actors of instanced world partitions to be part
					//			 of the persistent level and get rid of the always loaded cell (to have the same behavior
					//			 as non-instanced world partition and as cooked world partition).
					if (!CastChecked<AActor>(ActorDescView.GetActorNativeClass()->GetDefaultObject())->ShouldLevelKeepRefIfExternal())
					{
						OutCellActorInstances.Emplace(ActorInstance);
					}
				}
			});
		}
	}

	// Here we need to use the actor descriptor view, as the always loaded reference object might not have a valid actor descriptor for newly added actors, etc.
	for (auto& [Reference, ActorDescView] : AlwaysLoadedReferences)
	{
		if (AActor* AlwaysLoadedActor = FindObject<AActor>(nullptr, *ActorDescView->GetActorSoftPath().ToString()))
		{
			AlwaysLoadedActorsForPIE.Emplace(Reference, AlwaysLoadedActor);
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
			const FStreamingGenerationActorDescView& ActorDescView = ActorInstance.GetActorDescView();
			if (AActor* Actor = FindObject<AActor>(nullptr, *ActorDescView.GetActorSoftPath().ToString()))
			{
				if (ActorDescView.IsUnsaved())
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
		const FStreamingGenerationActorDescView& ActorDescView = ActorInstance.GetActorDescView();
		RuntimeCell->AddActorToCell(ActorDescView);
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

UWorldPartitionRuntimeCell* UWorldPartitionRuntimeHash::GetCellForCookPackage(const FString& InCookPackageName) const
{
	if (const TObjectPtr<UWorldPartitionRuntimeCell>* MatchingCell = PackagesToGenerateForCook.Find(InCookPackageName))
	{
		if (ensure(*MatchingCell))
		{
			return const_cast<UWorldPartitionRuntimeCell*>(ToRawPtr(*MatchingCell));
		}
	}
	return nullptr;
}

URuntimeHashExternalStreamingObjectBase* UWorldPartitionRuntimeHash::StoreStreamingContentToExternalStreamingObject(FName InStreamingObjectName)
{
	URuntimeHashExternalStreamingObjectBase* NewExternalStreamingObject = CreateExternalStreamingObject(GetExternalStreamingObjectClass(), GetOuterUWorldPartition(), InStreamingObjectName, GetTypedOuter<UWorld>());
	StoreStreamingContentToExternalStreamingObject(NewExternalStreamingObject);
	return NewExternalStreamingObject;
}

void UWorldPartitionRuntimeHash::StoreStreamingContentToExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* OutExternalStreamingObject)
{
	OutExternalStreamingObject->PackagesToGenerateForCook = MoveTemp(PackagesToGenerateForCook);
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

void UWorldPartitionRuntimeHash::DumpStateLog(FHierarchicalLogArchive& Ar) const
{
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("%s - Persistent Level"), *GetWorld()->GetName());
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	{
		FHierarchicalLogArchive::FIndentScope CellIndentScope = Ar.PrintfIndent(TEXT("Content of %s Persistent Level"), *GetWorld()->GetName());

		TArray<TPair<FString, FString>> Actors;

		if (!IsRunningCookCommandlet())
		{
			for (const FAlwaysLoadedActorForPIE& AlwaysLoadedActor : AlwaysLoadedActorsForPIE)
			{
				if (AlwaysLoadedActor.Actor.IsValid())
				{
					Actors.Add(
					{
						FString::Printf(TEXT("Actor Path: %s"), *AlwaysLoadedActor.Actor->GetPathName()), 
						FString::Printf(TEXT("Actor Package: %s"), *AlwaysLoadedActor.Actor->GetPackage()->GetName())
					});
				}
			}
		}
		else
		{
			for (UWorldPartitionRuntimeCell* Cell : GetAlwaysLoadedCells())
			{
				const UWorldPartitionRuntimeLevelStreamingCell* RuntimeCell = CastChecked<UWorldPartitionRuntimeLevelStreamingCell>(Cell);

				for (const FWorldPartitionRuntimeCellObjectMapping& Package : RuntimeCell->GetPackages())
				{
					Actors.Add(
					{
						FString::Printf(TEXT("Actor Path: %s"), *Package.Path.ToString()), 
						FString::Printf(TEXT("Actor Package: %s"), *Package.Package.ToString())
					});
				}
			}
		}

		Actors.Sort();

		Ar.Printf(TEXT("Always loaded Actor Count: %d "), Actors.Num());
		for (const TPair<FString, FString>& Actor : Actors)
		{
			Ar.Print(*Actor.Key);
			Ar.Print(*Actor.Value);
		}
	}
	Ar.Printf(TEXT(""));
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

bool UWorldPartitionRuntimeHash::IsExternalStreamingObjectInjected(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject) const
{
	return InjectedExternalStreamingObjects.Contains(InExternalStreamingObject);
}

bool UWorldPartitionRuntimeHash::InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* InExternalStreamingObject)
{
	if (!ensure(InExternalStreamingObject))
	{
		return false;
	}
	check(IsValid(InExternalStreamingObject));
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
	if (!ensure(InExternalStreamingObject))
	{
		return false;
	}
	check(IsValid(InExternalStreamingObject));
	if (!InjectedExternalStreamingObjects.Remove(InExternalStreamingObject))
	{
		UE_LOG(LogWorldPartition, Warning, TEXT("External streaming object %s was not injected."), *InExternalStreamingObject->GetName());
		return false;
	}
	return true;
}

#if WITH_EDITOR
void UWorldPartitionRuntimeHash::RegisterWorldPartitionRuntimeHashConverter(const UClass* InSrcClass, const UClass* InDstClass, FRuntimeHashConvertFunc&& InConverter)
{
	check(InSrcClass->IsChildOf<UWorldPartitionRuntimeHash>());
	check(InDstClass->IsChildOf<UWorldPartitionRuntimeHash>());
	WorldPartitionRuntimeHashConverters.Add({ InSrcClass, InDstClass }, MoveTemp(InConverter));
}

UWorldPartitionRuntimeHash* UWorldPartitionRuntimeHash::ConvertWorldPartitionHash(const UWorldPartitionRuntimeHash* InSrcHash, const UClass* InDstClass)
{
	check(InSrcHash);
	check(InDstClass);
	check(InDstClass->IsChildOf<UWorldPartitionRuntimeHash>());
	check(!InDstClass->HasAnyClassFlags(CLASS_Abstract));

	// Look for a registered converter
	const UClass* CurrentSrcClass = InSrcHash->GetClass();
	while(CurrentSrcClass != UWorldPartitionRuntimeHash::StaticClass())
	{
		if (FRuntimeHashConvertFunc* Converter = WorldPartitionRuntimeHashConverters.Find({ CurrentSrcClass, InDstClass }))
		{
			if (UWorldPartitionRuntimeHash* NewHash = (*Converter)(InSrcHash))
			{
				UE_LOG(LogWorldPartition, Log, TEXT("Converted '%s' runtime hash class from '%s' to '%s'."), *InSrcHash->GetPackage()->GetName(), *InSrcHash->GetClass()->GetName(), *InDstClass->GetName());
				return NewHash;
			}
			else
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Failed to convert '%s' runtime hash class from '%s' to '%s'."), *InSrcHash->GetPackage()->GetName(), *InSrcHash->GetClass()->GetName(), *InDstClass->GetName());
			}
		}
		CurrentSrcClass = CurrentSrcClass->GetSuperClass();
	}

	// No converter found, create a new hash of the target type with default values
	UE_LOG(LogWorldPartition, Log, TEXT("No converter found to convert '%s' runtime hash class from '%s' to '%s', creating new with default values."), *InSrcHash->GetPackage()->GetName(), *InSrcHash->GetClass()->GetName(), *InDstClass->GetName());
	UWorldPartitionRuntimeHash* NewHash = NewObject<UWorldPartitionRuntimeHash>(InSrcHash->GetOuter(), InDstClass, NAME_None, RF_Transactional);
	NewHash->SetDefaultValues();
	return NewHash;
}
#endif

void UWorldPartitionRuntimeHash::FStreamingSourceCells::AddCell(const UWorldPartitionRuntimeCell* Cell, const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape)
{
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
