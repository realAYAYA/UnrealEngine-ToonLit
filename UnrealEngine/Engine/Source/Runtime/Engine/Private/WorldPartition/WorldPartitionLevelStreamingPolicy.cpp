// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * WorldPartitionLevelStreamingPolicy implementation
 */

#include "WorldPartition/WorldPartitionLevelStreamingPolicy.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h"
#include "WorldPartition/WorldPartitionLevelStreamingDynamic.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/LevelStreamingGCHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionLevelStreamingPolicy)

#if WITH_EDITOR
#include "Misc/PackageName.h"
#endif

int32 UWorldPartitionLevelStreamingPolicy::GetCellLoadingCount() const
{
	int32 CellLoadingCount = 0;

	ForEachActiveRuntimeCell([&CellLoadingCount](const UWorldPartitionRuntimeCell* Cell)
	{
		if (Cell->IsLoading())
		{
			++CellLoadingCount;
		}
	});
	return CellLoadingCount;
}

void UWorldPartitionLevelStreamingPolicy::ForEachActiveRuntimeCell(TFunctionRef<void(const UWorldPartitionRuntimeCell*)> Func) const
{
	UWorld* World = WorldPartition->GetWorld();
	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		if (UWorldPartitionLevelStreamingDynamic* WorldPartitionLevelStreaming = Cast<UWorldPartitionLevelStreamingDynamic>(LevelStreaming))
		{
			if (const UWorldPartitionRuntimeCell* Cell = WorldPartitionLevelStreaming->GetWorldPartitionRuntimeCell())
			{
				Func(Cell);
			}
		}
	}
}

bool UWorldPartitionLevelStreamingPolicy::IsStreamingCompleted(const FWorldPartitionStreamingSource* InStreamingSource) const
{
	const UWorld* World = GetWorld();
	check(World);
	check(World->IsGameWorld());

	if (!Super::IsStreamingCompleted(InStreamingSource))
	{
		return false;
	}

	if (!InStreamingSource)
	{
		// Also verify that there's no remaining activity (mainly for unloading) 
		// waiting to be processed on level streaming of world partition runtime cells
		for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			ULevel* Level = StreamingLevel ? StreamingLevel->GetLoadedLevel() : nullptr;
			if (Level && Level->IsWorldPartitionRuntimeCell() && StreamingLevel->IsStreamingStatePending())
			{
				return false;
			}
		}
	}
	return true;
}

#if WITH_EDITOR

FString UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(const FName& InCellName, const UWorld* InWorld)
{
	if (InWorld->IsGameWorld())
	{
		// Set as memory package to avoid wasting time in FPackageName::DoesPackageExist
		return FString::Printf(TEXT("/Memory/%s"), *InCellName.ToString());
	}
	else
	{
		return FString::Printf(TEXT("/%s"), *InCellName.ToString());
	}
}

TSubclassOf<UWorldPartitionRuntimeCell> UWorldPartitionLevelStreamingPolicy::GetRuntimeCellClass() const
{
	return UWorldPartitionRuntimeLevelStreamingCell::StaticClass();
}

void UWorldPartitionLevelStreamingPolicy::PrepareActorToCellRemapping()
{
	// Build Actor-to-Cell remapping
	WorldPartition->RuntimeHash->ForEachStreamingCells([this](const UWorldPartitionRuntimeCell* Cell)
	{
		const UWorldPartitionRuntimeLevelStreamingCell* StreamingCell = Cast<const UWorldPartitionRuntimeLevelStreamingCell>(Cell);
		check(StreamingCell);
		for (const FWorldPartitionRuntimeCellObjectMapping& CellObjectMap : StreamingCell->GetPackages())
		{
			FString RemappedActorPath;
			FString CellActorPath = CellObjectMap.Path.ToString();

			// Add actor container id to actor path so that we can distinguish between actors of different Level Instances
			const bool ActorPathNeedsRemapping = FWorldPartitionLevelHelper::RemapActorPath(CellObjectMap.ContainerID, CellActorPath, RemappedActorPath);

			if (ActorPathNeedsRemapping || Cell->NeedsActorToCellRemapping())
			{
				const FString& ActorPath = ActorPathNeedsRemapping ? RemappedActorPath : CellActorPath;

				ActorToCellRemapping.Add(FName(*ActorPath), StreamingCell->GetFName());

				const int32 LastDotPos = ActorPath.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				check(LastDotPos != INDEX_NONE);

				SubObjectsToCellRemapping.Add(FName(*ActorPath.Mid(LastDotPos + 1)), StreamingCell->GetFName());
			}
		}
		return true;
	});
}

void UWorldPartitionLevelStreamingPolicy::RemapSoftObjectPath(FSoftObjectPath& ObjectPath)
{
	// Make sure to work on non-PIE path (can happen for modified actors in PIE)
	int32 PIEInstanceID = INDEX_NONE;
	FString SrcPath = UWorld::RemovePIEPrefix(ObjectPath.ToString(), &PIEInstanceID);
	const FSoftObjectPath SrcObjectPath(SrcPath);

	FName* CellName = ActorToCellRemapping.Find(FName(*SrcPath));
	if (!CellName)
	{
		const FString& SubPathString = ObjectPath.GetSubPathString();
		constexpr const TCHAR PersistenLevelName[] = TEXT("PersistentLevel.");
		constexpr const int32 DotPos = UE_ARRAY_COUNT(PersistenLevelName);
		if (SubPathString.StartsWith(PersistenLevelName))
		{
			const int32 SubObjectPos = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, DotPos);
			if (SubObjectPos != INDEX_NONE)
			{
				const FString ActorSubPathString = SubPathString.Left(SubObjectPos);
				FSoftObjectPath ActorObjectPath(SrcObjectPath);
				ActorObjectPath.SetSubPathString(ActorSubPathString);
				CellName = ActorToCellRemapping.Find(FName(*ActorObjectPath.ToString()));
			}
		}
	}

	if (CellName)
	{
		if (!SrcObjectPath.GetSubPathString().IsEmpty())
		{
			UWorld* OuterWorld = WorldPartition->GetTypedOuter<UWorld>();
			const FString PackagePath = UWorldPartitionLevelStreamingPolicy::GetCellPackagePath(*CellName, OuterWorld);
			FString PrefixPath;
			if (IsRunningCookCommandlet())
			{
				//@todo_ow: Temporary workaround. This information should be provided by the COTFS
				const UPackage* Package = OuterWorld->GetPackage();
				PrefixPath = FString::Printf(TEXT("%s/%s/_Generated_"), *FPackageName::GetLongPackagePath(Package->GetPathName()), *FPackageName::GetShortName(Package->GetName()));
			}

			// Use the WorldPartition world name here instead of using the world name from the path to support converting level instance paths to main world paths.
			ObjectPath = FSoftObjectPath(FTopLevelAssetPath(FString::Printf(TEXT("%s%s.%s"), *PrefixPath, *PackagePath, *OuterWorld->GetName())), SrcObjectPath.GetSubPathString());
			// Put back PIE prefix
			if (OuterWorld->IsPlayInEditor() && (PIEInstanceID != INDEX_NONE))
			{
				ObjectPath.FixupForPIE(PIEInstanceID);
			}
		}
	}
}
#endif

UObject* UWorldPartitionLevelStreamingPolicy::GetSubObject(const TCHAR* SubObjectPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionLevelStreamingPolicy::GetSubObject);

	// Support for subobjects such as Actor.Component
	FString SubObjectName;
	FString SubObjectContext;	
	if (!FString(SubObjectPath).Split(TEXT("."), &SubObjectContext, &SubObjectName))
	{
		SubObjectContext = SubObjectPath;
	}

	const FString SrcPath = UWorld::RemovePIEPrefix(*SubObjectContext);
	if (FName* CellName = SubObjectsToCellRemapping.Find(FName(*SrcPath)))
	{
		if (UWorldPartitionRuntimeLevelStreamingCell* Cell = (UWorldPartitionRuntimeLevelStreamingCell*)StaticFindObject(UWorldPartitionRuntimeLevelStreamingCell::StaticClass(), GetOuterUWorldPartition()->RuntimeHash, *(CellName->ToString())))
		{
			if (UWorldPartitionLevelStreamingDynamic* LevelStreaming = Cell->GetLevelStreaming())
			{
				if (LevelStreaming->GetLoadedLevel())
				{
					return StaticFindObject(UObject::StaticClass(), LevelStreaming->GetLoadedLevel(), SubObjectPath);
				}
			}
		}
	}

	return nullptr;
}

void UWorldPartitionLevelStreamingPolicy::DrawRuntimeCellsDetails(UCanvas* Canvas, FVector2D& Offset)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionLevelStreamingPolicy::DrawRuntimeCellsDetails);

	UWorld* World = WorldPartition->GetWorld();
	struct FCellsPerStreamingStatus
	{
		TArray<const UWorldPartitionRuntimeCell*> Cells;
	};
	FCellsPerStreamingStatus CellsPerStreamingStatus[(int32)LEVEL_StreamingStatusCount];
	ForEachActiveRuntimeCell([&CellsPerStreamingStatus](const UWorldPartitionRuntimeCell* Cell)
	{
		if (Cell->IsDebugShown())
		{
			CellsPerStreamingStatus[(int32)Cell->GetStreamingStatus()].Cells.Add(Cell);
		}
	});

	FVector2D Pos = Offset;
	const float BaseY = Offset.Y;

	float CurrentColumnWidth = 0.f;
	float MaxPosY = Pos.Y;

	auto DrawCellDetails = [&](const FString& Text, const UFont* Font, const FColor& Color)
	{
		FWorldPartitionDebugHelper::DrawText(Canvas, Text, Font, Color, Pos, &CurrentColumnWidth);
		MaxPosY = FMath::Max(MaxPosY, Pos.Y);
		if ((Pos.Y + 30) > Canvas->ClipY)
		{
			Pos.Y = BaseY;
			Pos.X += CurrentColumnWidth + 5;
			CurrentColumnWidth = 0.f;
		}
	};

	for (int32 i = 0; i < (int32)LEVEL_StreamingStatusCount; ++i)
	{
		const EStreamingStatus StreamingStatus = (EStreamingStatus)i;
		const TArray<const UWorldPartitionRuntimeCell*>& Cells = CellsPerStreamingStatus[i].Cells;
		if (Cells.Num() > 0)
		{
			const FString StatusDisplayName = *FString::Printf(TEXT("%s (%d)"), ULevelStreaming::GetLevelStreamingStatusDisplayName(StreamingStatus), Cells.Num());
			DrawCellDetails(StatusDisplayName, GEngine->GetSmallFont(), FColor::Yellow);

			const FColor Color = ULevelStreaming::GetLevelStreamingStatusColor(StreamingStatus);
			for (const UWorldPartitionRuntimeCell* Cell : Cells)
			{
				if ((StreamingStatus == EStreamingStatus::LEVEL_Loaded) ||
					(StreamingStatus == EStreamingStatus::LEVEL_MakingVisible) ||
					(StreamingStatus == EStreamingStatus::LEVEL_Visible) ||
					(StreamingStatus == EStreamingStatus::LEVEL_MakingInvisible))
				{
					float LoadTime = Cell->GetLevel() ? Cell->GetLevel()->GetPackage()->GetLoadTime() : 0.f;
					DrawCellDetails(FString::Printf(TEXT("%s (%s)"), *Cell->GetDebugName(), *FPlatformTime::PrettyTime(LoadTime)), GEngine->GetTinyFont(), Color);
				}
				else
				{
					DrawCellDetails(Cell->GetDebugName(), GEngine->GetTinyFont(), Color);
				}
			}
		}
	}

	Offset.Y = MaxPosY;
}

/**
 * Debug Draw Streaming Status Legend
 */
void UWorldPartitionLevelStreamingPolicy::DrawStreamingStatusLegend(UCanvas* Canvas, FVector2D& Offset)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionLevelStreamingPolicy::DrawStreamingStatusLegend);

	check(Canvas);

	// Cumulate counter stats
	int32 StatusCount[(int32)LEVEL_StreamingStatusCount] = { 0 };
	ForEachActiveRuntimeCell([&StatusCount](const UWorldPartitionRuntimeCell* Cell)
	{
		StatusCount[(int32)Cell->GetStreamingStatus()]++;
	});

	// @todo_ow: This is not exactly the good value, as we could have pending unload level from Level Instances, etc.
	//           We could modify GetNumLevelsPendingPurge to return the number of pending purge levels from the grid, 
	//           bu that will do for now.
	StatusCount[LEVEL_UnloadedButStillAround] = FLevelStreamingGCHelper::GetNumLevelsPendingPurge();

	// Draw legend
	FVector2D Pos = Offset;
	float MaxTextWidth = 0.f;
	FWorldPartitionDebugHelper::DrawText(Canvas, TEXT("Streaming Status Legend"), GEngine->GetSmallFont(), FColor::Yellow, Pos, &MaxTextWidth);
	
	for (int32 i = 0; i < (int32)LEVEL_StreamingStatusCount; ++i)
	{
		EStreamingStatus Status = (EStreamingStatus)i;
		const FColor& StatusColor = ULevelStreaming::GetLevelStreamingStatusColor(Status);
		FString DebugString = *FString::Printf(TEXT("%d) %s"), i, ULevelStreaming::GetLevelStreamingStatusDisplayName(Status));
		if (Status != LEVEL_Unloaded)
		{
			DebugString += *FString::Printf(TEXT(" (%d)"), StatusCount[(int32)Status]);
		}
		FWorldPartitionDebugHelper::DrawLegendItem(Canvas, *DebugString, GEngine->GetSmallFont(), StatusColor, FColor::White, Pos, &MaxTextWidth);
	}

	Offset.X += MaxTextWidth + 10;
}
