// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "UObject/Package.h"
#include "Engine/Level.h"
#include "Misc/HierarchicalLogArchive.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeCell)

UWorldPartitionRuntimeCell::UWorldPartitionRuntimeCell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsAlwaysLoaded(false)
	, Priority(0)
#if !UE_BUILD_SHIPPING
	, DebugStreamingPriority(-1.f)
#endif
	, RuntimeCellData(nullptr)
{
	check(HasAnyFlags(RF_ClassDefaultObject) || GetOuter()->Implements<UWorldPartitionRuntimeCellOwner>());
}

#if WITH_EDITOR
void UWorldPartitionRuntimeCell::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (UnsavedActorsContainer)
	{
		// Make sure actor container isn't under PIE World so those template actors will never be considered part of the world.
		UnsavedActorsContainer->Rename(nullptr, GetPackage());

		for (auto& [ActorName, Actor] : UnsavedActorsContainer->Actors)
		{
			if (ensure(Actor))
			{
				// Don't use AActor::Rename here since the actor is not par of the world, it's only a duplication template.	
				Actor->UObject::Rename(nullptr, UnsavedActorsContainer);
			}
		}
	}
}

bool UWorldPartitionRuntimeCell::NeedsActorToCellRemapping() const
{
	// When cooking, always loaded cells content is moved to persistent level (see PopulateGeneratorPackageForCook)
	return !(IsAlwaysLoaded() && IsRunningCookCommandlet());
}

void UWorldPartitionRuntimeCell::SetDataLayers(const TArray<const UDataLayerInstance*>& InDataLayerInstances)
{
	check(DataLayers.IsEmpty());
	DataLayers.Reserve(InDataLayerInstances.Num());
	for (const UDataLayerInstance* DataLayerInstance : InDataLayerInstances)
	{
		check(DataLayerInstance->IsRuntime());
		DataLayers.Add(DataLayerInstance->GetDataLayerFName());
	}
	DataLayers.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	UpdateDebugName();
}

void UWorldPartitionRuntimeCell::SetDebugInfo(int64 InCoordX, int64 InCoordY, int64 InCoordZ, FName InGridName)
{
	DebugInfo.CoordX = InCoordX;
	DebugInfo.CoordY = InCoordY;
	DebugInfo.CoordZ = InCoordZ;
	DebugInfo.GridName = InGridName;
	UpdateDebugName();
}

void UWorldPartitionRuntimeCell::UpdateDebugName()
{
	TStringBuilder<512> Builder;
	Builder += DebugInfo.GridName.ToString();
	Builder += TEXT("_");
	Builder += FString::Printf(TEXT("L%d_X%d_Y%d"), DebugInfo.CoordZ, DebugInfo.CoordX, DebugInfo.CoordY);
	int32 DataLayerCount = DataLayers.Num();

	if (DataLayerCount > 0)
	{
		if (const UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(GetCellOwner()->GetOuterWorld()))
		{
			TArray<const UDataLayerInstance*> DataLayerObjects;
			Builder += TEXT(" DL[");
			for (int i = 0; i < DataLayerCount; ++i)
			{
				const UDataLayerInstance* DataLayer = DataLayerSubsystem->GetDataLayerInstance(DataLayers[i]);
				DataLayerObjects.Add(DataLayer);
				Builder += DataLayer->GetDataLayerShortName();
				Builder += TEXT(",");
			}
			Builder += FString::Printf(TEXT("ID:%X]"), FDataLayersID(DataLayerObjects).GetHash());
		}
	}

	if (ContentBundleID.IsValid())
	{
		Builder += FString::Printf(TEXT(" CB[%s]"), *UContentBundleDescriptor::GetContentBundleCompactString(ContentBundleID));
	}

	DebugInfo.Name = Builder.ToString();
}

void UWorldPartitionRuntimeCell::DumpStateLog(FHierarchicalLogArchive& Ar)
{
	Ar.Printf(TEXT("Actor Count: %d"), GetActorCount());
}
#endif

bool UWorldPartitionRuntimeCell::ShouldResetStreamingSourceInfo() const
{
	return RuntimeCellData->ShouldResetStreamingSourceInfo();
}

void UWorldPartitionRuntimeCell::ResetStreamingSourceInfo() const
{
	RuntimeCellData->ResetStreamingSourceInfo();
}

void UWorldPartitionRuntimeCell::AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const
{
	RuntimeCellData->AppendStreamingSourceInfo(Source, SourceShape);
}

void UWorldPartitionRuntimeCell::MergeStreamingSourceInfo() const
{
	RuntimeCellData->MergeStreamingSourceInfo();
}

int32 UWorldPartitionRuntimeCell::SortCompare(const UWorldPartitionRuntimeCell* Other, bool bCanUseSortingCache) const
{
	const int32 Comparison = RuntimeCellData->SortCompare(Other->RuntimeCellData, bCanUseSortingCache);

	// Cell priority (lower value is higher prio)
	return (Comparison != 0) ? Comparison : (Priority - Other->Priority);
}

bool UWorldPartitionRuntimeCell::IsDebugShown() const
{
	return FWorldPartitionDebugHelper::IsDebugRuntimeHashGridShown(GetGridName()) &&
		   FWorldPartitionDebugHelper::IsDebugStreamingStatusShown(GetStreamingStatus()) &&
	       FWorldPartitionDebugHelper::AreDebugDataLayersShown(DataLayers) &&
		   FWorldPartitionDebugHelper::IsDebugCellNameShown(DebugInfo.Name) &&
		   (FWorldPartitionDebugHelper::CanDrawContentBundles() || !ContentBundleID.IsValid());
}

FLinearColor UWorldPartitionRuntimeCell::GetDebugStreamingPriorityColor() const
{
#if !UE_BUILD_SHIPPING
	if (DebugStreamingPriority >= 0.f && DebugStreamingPriority <= 1.f)
	{
		return FWorldPartitionDebugHelper::GetHeatMapColor(1.f - DebugStreamingPriority);
	}
#endif
	return FLinearColor::Transparent;
}

TArray<const UDataLayerInstance*> UWorldPartitionRuntimeCell::GetDataLayerInstances() const
{
	if (const UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>())
	{
		return DataLayerSubsystem->GetDataLayerInstances(GetDataLayers());
	}

	return TArray<const UDataLayerInstance*>();
}

bool UWorldPartitionRuntimeCell::ContainsDataLayer(const UDataLayerAsset* DataLayerAsset) const
{
	if (const UDataLayerSubsystem* DataLayerSubsystem = GetWorld()->GetSubsystem<UDataLayerSubsystem>())
	{
		if (const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerAsset))
		{
			return ContainsDataLayer(DataLayerInstance);
		}
	}

	return false;
}

bool UWorldPartitionRuntimeCell::ContainsDataLayer(const UDataLayerInstance* DataLayerInstance) const
{
	return GetDataLayers().Contains(DataLayerInstance->GetDataLayerFName());
}

const FBox& UWorldPartitionRuntimeCell::GetContentBounds() const
{
	return RuntimeCellData->GetContentBounds();
}

FBox UWorldPartitionRuntimeCell::GetCellBounds() const
{
	return RuntimeCellData->GetCellBounds();
}

FName UWorldPartitionRuntimeCell::GetLevelPackageName() const
{ 
#if WITH_EDITOR
	return LevelPackageName;
#else
	return NAME_None;
#endif
}
