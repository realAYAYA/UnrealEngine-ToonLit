// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosInterfaceWrapperCore.h"
#include "Chaos/ParticleHandleFwd.h"
#include "PhysicsInterfaceUtilsCore.h"
#include "CollisionQueryFilterCallbackCore.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
//todo: move this include into an impl header

class FPhysScene_Chaos;
class UPhysicalMaterial;
struct FBodyInstance;
struct FCollisionFilterData;
struct FCollisionQueryParams;

namespace Chaos
{
	class FPerShapeData;
}

namespace ChaosInterface
{

// Needed by low level SQ calls.
struct FScopedSceneReadLock
{
	FScopedSceneReadLock(FPhysScene_Chaos& SceneIn);
	~FScopedSceneReadLock();

	Chaos::FPBDRigidsSolver* Solver;
};

inline FQueryFilterData MakeQueryFilterData(const FCollisionFilterData& FilterData, EQueryFlags QueryFlags, const FCollisionQueryParams& Params)
{
	return FChaosQueryFilterData(U2CFilterData(FilterData), U2CQueryFlags(QueryFlags));
}

FBodyInstance* GetUserData(const Chaos::FGeometryParticle& Actor);
UPhysicalMaterial* GetUserData(const Chaos::FChaosPhysicsMaterial& Material);

}

void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams = FQueryDebugParams());
void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FPTRaycastHit>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams = FQueryDebugParams());

void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams = FQueryDebugParams());
void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FPTSweepHit>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams = FQueryDebugParams());

void LowLevelOverlap(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams = FQueryDebugParams());
void LowLevelOverlap(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& GeomPose, FPhysicsHitCallback<FPTOverlapHit>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams = FQueryDebugParams());
