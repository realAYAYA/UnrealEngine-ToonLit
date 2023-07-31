// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"

struct FZoneGraphShortPathRequest;
struct FMassZoneGraphCachedLaneFragment;
struct FMassZoneGraphShortPathFragment;
struct FMassMoveTargetFragment;
struct FMassZoneGraphPathRequestFragment;
struct FMassZoneGraphLaneLocationFragment;
class UMassSignalSubsystem;
class UZoneGraphSubsystem;
class UZoneGraphAnnotationSubsystem;

namespace UE::MassNavigation
{
	MASSZONEGRAPHNAVIGATION_API bool ActivateActionMove(const UWorld& World,
											 const UObject* Requester,
											 const FMassEntityHandle Entity,
											 const UZoneGraphSubsystem& ZoneGraphSubsystem,
											 const FMassZoneGraphLaneLocationFragment& LaneLocation,
											 const FZoneGraphShortPathRequest& PathRequest,
											 const float AgentRadius,
											 const float DesiredSpeed,
											 FMassMoveTargetFragment& MoveTarget,
											 FMassZoneGraphShortPathFragment& ShortPath,
											 FMassZoneGraphCachedLaneFragment& CachedLane);

	MASSZONEGRAPHNAVIGATION_API bool ActivateActionStand(const UWorld& World,
											  const UObject* Requester,
											  const FMassEntityHandle Entity,
											  const UZoneGraphSubsystem& ZoneGraphSubsystem,
											  const FMassZoneGraphLaneLocationFragment& LaneLocation,
											  const float DesiredSpeed,
											  FMassMoveTargetFragment& MoveTarget,
											  FMassZoneGraphShortPathFragment& ShortPath,
											  FMassZoneGraphCachedLaneFragment& CachedLane);

	MASSZONEGRAPHNAVIGATION_API bool ActivateActionAnimate(const UWorld& World,
												const UObject* Requester,
												const FMassEntityHandle Entity,
												FMassMoveTargetFragment& MoveTarget);
};
