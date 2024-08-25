// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationDirtyAreasController.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "VisualLogger/VisualLogger.h"
#include "AI/Navigation/NavigationDirtyElement.h"

DEFINE_LOG_CATEGORY(LogNavigationDirtyArea);

//----------------------------------------------------------------------//
// FNavigationDirtyAreasController
//----------------------------------------------------------------------//
FNavigationDirtyAreasController::FNavigationDirtyAreasController()
	: bCanAccumulateDirtyAreas(true)
	, bUseWorldPartitionedDynamicMode(false)
#if !UE_BUILD_SHIPPING
	, bDirtyAreasReportedWhileAccumulationLocked(false)
	, bCanReportOversizedDirtyArea(false)
	, bNavigationBuildLocked(false)
#endif // !UE_BUILD_SHIPPING
{

}

void FNavigationDirtyAreasController::ForceRebuildOnNextTick()
{
	float MinTimeForUpdate = (DirtyAreasUpdateFreq != 0.f ? (1.0f / DirtyAreasUpdateFreq) : 0.f);
	DirtyAreasUpdateTime = FMath::Max(DirtyAreasUpdateTime, MinTimeForUpdate);
}

namespace UE::Navigation::Private
{
	const UNavigationSystemV1* FindNavigationSystem(const TArray<ANavigationData*>& NavDataSet)
	{
		const UNavigationSystemV1* NavSys = nullptr;
		for (const ANavigationData* NavData : NavDataSet)
		{
			if (NavData)
			{
				NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(NavData->GetWorld());
				if (NavSys)
				{
					return NavSys;
				}
			}
		}

		return NavSys;
	}
}

void FNavigationDirtyAreasController::Tick(const float DeltaSeconds, const TArray<ANavigationData*>& NavDataSet, bool bForceRebuilding /*= false*/)
{
	DirtyAreasUpdateTime += DeltaSeconds;
	const bool bCanRebuildNow = bForceRebuilding || (DirtyAreasUpdateFreq != 0.f && DirtyAreasUpdateTime >= (1.0f / DirtyAreasUpdateFreq));

	if (DirtyAreas.Num() > 0 && bCanRebuildNow)
	{
		bool bIsUsingActiveTileGeneration = false;
		TArray<FNavigationDirtyArea> SubAreaArray;
		SubAreaArray.Reserve(DirtyAreas.Num());
		
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMeshGenerator_MakingSubAreas);

			// Find the relevant navigation system
			const UNavigationSystemV1* NavSys = UE::Navigation::Private::FindNavigationSystem(NavDataSet);
			
			const TArray<FBox>* SeedsBoundsArrayPtr = nullptr;
			bIsUsingActiveTileGeneration = NavSys && NavSys->IsActiveTilesGenerationEnabled(); 
			if (bIsUsingActiveTileGeneration)
			{
				SeedsBoundsArrayPtr = &NavSys->GetInvokersSeedBounds();
			}

			for (const FNavigationDirtyArea& DirtyArea : DirtyAreas)
			{
				const FBox& AreaBound = DirtyArea.Bounds;
				if (!ensureMsgf(AreaBound.IsValid, TEXT("%hs Attempting to use DirtyArea.Bounds which are not valid. SourceObject: %s"), __FUNCTION__, *GetFullNameSafe(DirtyArea.OptionalSourceObject.Get())))
				{
					continue;
				}

				if (SeedsBoundsArrayPtr != nullptr && SeedsBoundsArrayPtr->Num() > 0)
				{
					for (const FBox& SeedBounds : *SeedsBoundsArrayPtr)
					{
						// Compute sub area bound
						const FBox OverlapBox = AreaBound.Overlap(SeedBounds);
						if (OverlapBox.IsValid)
						{
							SubAreaArray.Emplace(OverlapBox, DirtyArea.Flags, DirtyArea.OptionalSourceObject.Get());
						}
					}
				}
				else
				{
					SubAreaArray.Emplace(DirtyArea);
				}
			}
		}
		
		for (ANavigationData* NavData : NavDataSet)
		{
			if (NavData)
			{
				NavData->RebuildDirtyAreas(bIsUsingActiveTileGeneration ? SubAreaArray : DirtyAreas);
			}
		}

		DirtyAreasUpdateTime = 0.f;
		DirtyAreas.Reset();
	}
}

void FNavigationDirtyAreasController::AddArea(const FBox& NewArea, const int32 Flags, const TFunction<UObject*()>& ObjectProviderFunc /*= nullptr*/,
	const FNavigationDirtyElement* DirtyElement /*= nullptr*/, const FName& DebugReason /*= NAME_None*/)
{
	AddAreas({NewArea}, Flags, ObjectProviderFunc, DirtyElement, DebugReason);
}

void FNavigationDirtyAreasController::AddAreas(const TConstArrayView<FBox> NewAreas, const int32 Flags, const TFunction<UObject*()>& ObjectProviderFunc, const FNavigationDirtyElement* DirtyElement, const FName& DebugReason)
{
#if !UE_BUILD_SHIPPING
	// always keep track of reported areas even when filtered out by invalid area as long as flags are valid
	bDirtyAreasReportedWhileAccumulationLocked = bDirtyAreasReportedWhileAccumulationLocked || (Flags > 0 && !bCanAccumulateDirtyAreas);

	checkf(NewAreas.Num() > 0, TEXT("All callers of this method are expected to provide at least one area."));
#endif // !UE_BUILD_SHIPPING

	UObject* SourceObject = ObjectProviderFunc ? ObjectProviderFunc() : nullptr;

	if (bUseWorldPartitionedDynamicMode)
	{
		// Both conditions must be true to ignore dirtiness.
		//  If it's only a visibility change and it's not in the base navmesh, it's the case created by loading a data layer (dirtiness must be applied)
		//  If there is no visibility change, the change is not from loading/unloading a cell (dirtiness must be applied)
		
		// ObjectProviderFunc() is not always providing a valid object.
		if (const bool bIsFromVisibilityChange = (DirtyElement && DirtyElement->bIsFromVisibilityChange) || (SourceObject && FNavigationSystem::IsLevelVisibilityChanging(SourceObject)))
		{
			// If the area is from the addition or removal of objects caused by level loading/unloading and it's already in the base navmesh ignore the dirtiness.
			if (const bool bIsIncludedInBaseNavmesh = (DirtyElement && DirtyElement->bIsInBaseNavmesh) || (SourceObject && FNavigationSystem::IsInBaseNavmesh(SourceObject)))
			{
				UE_LOG(LogNavigationDirtyArea, VeryVerbose, TEXT("Ignoring dirtyness (visibility changed and in base navmesh). (object: %s from: %s)"),
					*GetFullNameSafe(SourceObject), *DebugReason.ToString());
				return;
			}
		}
	}

	if (ShouldSkipObjectPredicate.IsBound() && SourceObject)
	{
		if (ShouldSkipObjectPredicate.Execute(*SourceObject))
		{
			return;
		}
	}

	int32 NumInvalidBounds = 0;
	int32 NumEmptyBounds = 0;
	for (const FBox& NewArea : NewAreas)
	{
		if (!NewArea.IsValid)
		{
			NumInvalidBounds++;
			continue;
		}

		const FVector2D BoundsSize(NewArea.GetSize());
		if (BoundsSize.IsNearlyZero())
		{
			NumEmptyBounds++;
			continue;
		}

#if !UE_BUILD_SHIPPING
		auto DumpExtraInfo = [SourceObject, DebugReason, BoundsSize, NewArea]() {
				FString ObjectInfo;
				if (const UObject* ObjectOwner = (SourceObject != nullptr ? SourceObject->GetOuter() : nullptr))
				{
					UE_VLOG_BOX(ObjectOwner, LogNavigationDirtyArea, Log, NewArea, FColor::Red, TEXT(""));
					ObjectInfo = FString::Printf(TEXT(" | Element's owner: %s"), *GetFullNameSafe(ObjectOwner));
				}

				return FString::Printf(TEXT("From: %s | Object: %s %s | Bounds: %s"),
					*DebugReason.ToString(),
					*GetFullNameSafe(SourceObject),
					*ObjectInfo,
					*BoundsSize.ToString());
		};

		if (ShouldReportOversizedDirtyArea() && BoundsSize.GetMax() > DirtyAreaWarningSizeThreshold)
		{
			UE_LOG(LogNavigationDirtyArea, Warning, TEXT("Adding an oversized dirty area: %s | Threshold: %.2f"), *DumpExtraInfo(), DirtyAreaWarningSizeThreshold);
		}
		else
		{
			UE_LOG(LogNavigationDirtyArea, VeryVerbose, TEXT("Adding dirty area object: %s"), *DumpExtraInfo());
		}
#endif // !UE_BUILD_SHIPPING

		if (Flags > 0 && bCanAccumulateDirtyAreas)
		{
			DirtyAreas.Add(FNavigationDirtyArea(NewArea, Flags, SourceObject));
		}
	}
	
	UE_CLOG(NumInvalidBounds > 0 || NumEmptyBounds > 0, LogNavigationDirtyArea, Warning,
		TEXT("Skipped some dirty area creation due to: %d invalid bounds, %d empty bounds (object: %s, from: %s)"),
		NumInvalidBounds, NumEmptyBounds, *GetFullNameSafe(SourceObject), *DebugReason.ToString());
}

void FNavigationDirtyAreasController::OnNavigationBuildLocked()
{
#if !UE_BUILD_SHIPPING
	bNavigationBuildLocked = true;
#endif // !UE_BUILD_SHIPPING
}

void FNavigationDirtyAreasController::OnNavigationBuildUnlocked()
{
#if !UE_BUILD_SHIPPING
	bNavigationBuildLocked = false;
#endif // !UE_BUILD_SHIPPING
}

void FNavigationDirtyAreasController::SetDirtyAreaWarningSizeThreshold(const float Threshold)
{
#if !UE_BUILD_SHIPPING
	DirtyAreaWarningSizeThreshold = Threshold;
#endif // !UE_BUILD_SHIPPING
}

void FNavigationDirtyAreasController::SetUseWorldPartitionedDynamicMode(bool bIsWPDynamic)
{
	bUseWorldPartitionedDynamicMode = bIsWPDynamic;
}

void FNavigationDirtyAreasController::SetCanReportOversizedDirtyArea(bool bCanReport)
{
#if !UE_BUILD_SHIPPING
	bCanReportOversizedDirtyArea = bCanReport;
#endif // !UE_BUILD_SHIPPING
}

#if !UE_BUILD_SHIPPING
bool FNavigationDirtyAreasController::ShouldReportOversizedDirtyArea() const
{ 
	return bNavigationBuildLocked == false && bCanReportOversizedDirtyArea && DirtyAreaWarningSizeThreshold >= 0.0f;
}
#endif // !UE_BUILD_SHIPPING


void FNavigationDirtyAreasController::Reset()
{
	// discard all pending dirty areas, we are going to rebuild navmesh anyway 
	UE_LOG(LogNavigationDirtyArea, VeryVerbose, TEXT("%hs: Reseting All Dirty Areas. DirtyAreas.Num = [%d]"),__FUNCTION__, DirtyAreas.Num());

	DirtyAreas.Reset();
#if !UE_BUILD_SHIPPING
	bDirtyAreasReportedWhileAccumulationLocked = false;
#endif // !UE_BUILD_SHIPPING
}
