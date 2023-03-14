// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationDirtyAreasController.h"
#include "NavigationData.h"
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

void FNavigationDirtyAreasController::Tick(const float DeltaSeconds, const TArray<ANavigationData*>& NavDataSet, bool bForceRebuilding)
{
	DirtyAreasUpdateTime += DeltaSeconds;
	const bool bCanRebuildNow = bForceRebuilding || (DirtyAreasUpdateFreq != 0.f && DirtyAreasUpdateTime >= (1.0f / DirtyAreasUpdateFreq));

	if (DirtyAreas.Num() > 0 && bCanRebuildNow)
	{
		for (ANavigationData* NavData : NavDataSet)
		{
			if (NavData)
			{
				NavData->RebuildDirtyAreas(DirtyAreas);
			}
		}

		DirtyAreasUpdateTime = 0.f;
		DirtyAreas.Reset();
	}
}

void FNavigationDirtyAreasController::AddArea(const FBox& NewArea, const int32 Flags, const TFunction<UObject*()>& ObjectProviderFunc /*= nullptr*/,
	const FNavigationDirtyElement* DirtyElement /*= nullptr*/, const FName& DebugReason /*= NAME_None*/)
{
#if !UE_BUILD_SHIPPING
	// always keep track of reported areas even when filtered out by invalid area as long as flags are valid
	bDirtyAreasReportedWhileAccumulationLocked = bDirtyAreasReportedWhileAccumulationLocked || (Flags > 0 && !bCanAccumulateDirtyAreas);
#endif // !UE_BUILD_SHIPPING

	if (!NewArea.IsValid)
	{
		UE_LOG(LogNavigationDirtyArea, Warning, TEXT("Skipping dirty area creation because of invalid bounds (object: %s, from: %s)"),
			*GetFullNameSafe(ObjectProviderFunc ? ObjectProviderFunc() : nullptr), *DebugReason.ToString());
		return;
	}

	const FVector2D BoundsSize(NewArea.GetSize());
	if (BoundsSize.IsNearlyZero())
	{
		UE_LOG(LogNavigationDirtyArea, Warning, TEXT("Skipping dirty area creation because of empty bounds (object: %s, from: %s)"),
			*GetFullNameSafe(ObjectProviderFunc ? ObjectProviderFunc() : nullptr), *DebugReason.ToString());
		return;
	}

	if (bUseWorldPartitionedDynamicMode)
	{
		// Both conditions must be true to ignore dirtiness.
		//  If it's only a visibility change and it's not in the base navmesh, it's the case created by loading a data layer (dirtiness must be applied)
		//  If there is no visibility change, the change is not from loading/unloading a cell (dirtiness must be applied)
		
		// ObjectProviderFunc() is not always providing a valid object.
		if (const bool bIsFromVisibilityChange = (DirtyElement && DirtyElement->bIsFromVisibilityChange) || (ObjectProviderFunc && FNavigationSystem::IsLevelVisibilityChanging(ObjectProviderFunc())))
		{
			// If the area is from the addition or removal of objects caused by level loading/unloading and it's already in the base navmesh ignore the dirtiness.
			if (const bool bIsIncludedInBaseNavmesh = (DirtyElement && DirtyElement->bIsInBaseNavmesh) || (ObjectProviderFunc && FNavigationSystem::IsInBaseNavmesh(ObjectProviderFunc())))
			{
				UE_LOG(LogNavigationDirtyArea, VeryVerbose, TEXT("Ignoring dirtyness (visibility changed and in base navmesh). (object: %s from: %s)"),
					*GetFullNameSafe(ObjectProviderFunc ? ObjectProviderFunc() : nullptr), *DebugReason.ToString());
				return;
			}
		}		
	}

#if !UE_BUILD_SHIPPING
	auto DumpExtraInfo = [ObjectProviderFunc, DebugReason, BoundsSize, NewArea]() {
		const UObject* ObjectProvider = nullptr;
		if (ObjectProviderFunc)
		{
			ObjectProvider = ObjectProviderFunc();
		}

		const UActorComponent* ObjectAsComponent = Cast<UActorComponent>(ObjectProvider);
		const AActor* ComponentOwner = ObjectAsComponent ? ObjectAsComponent->GetOwner() : nullptr;
		if (ComponentOwner)
		{
			UE_VLOG_BOX(ComponentOwner, LogNavigationDirtyArea, Log, NewArea, FColor::Red, TEXT(""));
		}
		return FString::Printf(TEXT("Adding dirty area object: %s (from: %s) | Potential component's owner: %s | Bounds size: %s"),
			*GetFullNameSafe(ObjectProvider),
			*DebugReason.ToString(),
			*GetFullNameSafe(ComponentOwner),
			*BoundsSize.ToString());
	};

	UE_LOG(LogNavigationDirtyArea, VeryVerbose, TEXT("%s"), *DumpExtraInfo());

	if (ShouldReportOversizedDirtyArea() && BoundsSize.GetMax() > DirtyAreaWarningSizeThreshold)
	{
		UE_LOG(LogNavigationDirtyArea, Warning, TEXT("Adding an oversized dirty area (object:%s size:%s threshold:%.2f)"),
			*GetFullNameSafe(ObjectProviderFunc ? ObjectProviderFunc() : nullptr),
			*BoundsSize.ToString(),
			DirtyAreaWarningSizeThreshold);
	}
#endif // !UE_BUILD_SHIPPING

	if (Flags > 0 && bCanAccumulateDirtyAreas)
	{
		DirtyAreas.Add(FNavigationDirtyArea(NewArea, Flags, ObjectProviderFunc ? ObjectProviderFunc() : nullptr));
	}
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
	DirtyAreas.Reset();
#if !UE_BUILD_SHIPPING
	bDirtyAreasReportedWhileAccumulationLocked = false;
#endif // !UE_BUILD_SHIPPING
}
