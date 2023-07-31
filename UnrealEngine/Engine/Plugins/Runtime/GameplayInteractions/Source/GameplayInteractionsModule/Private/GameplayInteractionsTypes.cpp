// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionsTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "NavCorridor.h"
#include "NavigationPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionsTypes)

DEFINE_LOG_CATEGORY(LogGameplayInteractions);

namespace UE::GameplayInteraction::Debug
{
void VLogPath(const UObject* LogOwner, const FNavigationPath& Path)
{
#if ENABLE_VISUAL_LOG
	if (LogOwner == nullptr)
	{
		return;
	}
	
	// Draw path
	const TArray<FNavPathPoint>& PathPoints = Path.GetPathPoints();
	for (int32 PointIndex = 0; PointIndex < PathPoints.Num(); PointIndex++)
	{
		const FNavPathPoint& PathPoint = PathPoints[PointIndex];

		if ((PointIndex + 1) < PathPoints.Num())
		{
			const FNavPathPoint& NextPathPoint = PathPoints[PointIndex + 1];
			UE_VLOG_SEGMENT_THICK(LogOwner, LogGameplayInteractions, Log, PathPoint.Location, NextPathPoint.Location, FColor(64,64,64), 2, TEXT_EMPTY);
		}
	}
#endif // ENABLE_VISUAL_LOG
}

void VLogCorridor(const UObject* LogOwner, const FNavCorridor& Corridor)
{
#if ENABLE_VISUAL_LOG
	if (LogOwner == nullptr)
	{
		return;
	}
	
	const FVector Offset(0,0,10);

	for (int32 PortalIndex = 0; PortalIndex < Corridor.Portals.Num(); PortalIndex++)
	{
		const FNavCorridorPortal& Portal = Corridor.Portals[PortalIndex];

		UE_VLOG_SEGMENT(LogOwner, LogGameplayInteractions, Log, Offset + Portal.Left, Offset + Portal.Right, Portal.bIsPathCorner ? FColor::Red : FColor::Orange, TEXT_EMPTY);

		if ((PortalIndex + 1) < Corridor.Portals.Num())
		{
			const FNavCorridorPortal& NextPortal = Corridor.Portals[PortalIndex+1];

			UE_VLOG_SEGMENT_THICK(LogOwner, LogGameplayInteractions, Log, Offset + Portal.Left, Offset + NextPortal.Left, FColor::Orange, 2, TEXT_EMPTY);
			UE_VLOG_SEGMENT_THICK(LogOwner, LogGameplayInteractions, Log, Offset + Portal.Right, Offset + NextPortal.Right, FColor::Orange, 2, TEXT_EMPTY);
			UE_VLOG_SEGMENT_THICK(LogOwner, LogGameplayInteractions, Log, Offset + Portal.Location, Offset + NextPortal.Location, FColor(255,128,128), 2, TEXT_EMPTY);
		}
	}
#endif // ENABLE_VISUAL_LOG
}

}; // UE::GameplayInteraction::Debug
