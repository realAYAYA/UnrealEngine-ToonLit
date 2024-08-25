// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosInterfaceWrapperCore.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/SpatialAccelerationFwd.h"
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
UPrimitiveComponent* GetPrimitiveComponentFromUserData(const Chaos::FGeometryParticle& Actor);

}

template<typename TContainer, typename THit>
void LowLevelRaycast(const TContainer& Container, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<THit>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const ChaosInterface::FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const ChaosInterface::FQueryDebugParams& DebugParams = {});

template<typename TContainer, typename THit>
void LowLevelSweep(const TContainer& Container, const FPhysicsGeometry& Geom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<THit>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const ChaosInterface::FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const ChaosInterface::FQueryDebugParams& DebugParams = {});

template<typename TContainer, typename THit>
void LowLevelOverlap(const TContainer& Container, const FPhysicsGeometry& Geom, const FTransform& GeomPose, FPhysicsHitCallback<THit>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const ChaosInterface::FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const ChaosInterface::FQueryDebugParams& DebugParams = {});