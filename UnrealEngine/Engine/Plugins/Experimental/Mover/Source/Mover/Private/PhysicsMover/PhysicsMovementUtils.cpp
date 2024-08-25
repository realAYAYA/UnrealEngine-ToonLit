// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMovementUtils.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/PhysicsVolume.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "WaterBodyActor.h"

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
#include "Chaos/DebugDrawQueue.h"
#endif

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

void UPhysicsMovementUtils::FloorSweep(
	const FVector& Location,
	const FVector& DeltaPos,
	const UPrimitiveComponent* UpdatedPrimitive,
	const FVector& UpDir,
	float QueryRadius,
	float QueryDistance,
	float MaxWalkSlopeCosine,
	float TargetHeight,
	FFloorCheckResult& OutFloorResult,
	FWaterCheckResult& OutWaterResult
)
{
	if (const UWorld* World = UpdatedPrimitive->GetWorld())
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysicsFloorTest), false, UpdatedPrimitive->GetOwner());
		QueryParams.bTraceIntoSubComponents = false;
		const ECollisionChannel CollisionChannel = UpdatedPrimitive->GetCollisionObjectType();
		FCollisionResponseParams ResponseParams(ECR_Overlap);
		ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_Vehicle, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_Destructible, ECR_Block);
		ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);

		TArray<FHitResult> Hits;

		const float DeltaPosVertLength = DeltaPos.Dot(UpDir);
		const FVector DeltaPosHoriz = DeltaPos - DeltaPosVertLength * UpDir;

		// Make sure the query is long enough to include the vertical movement
		const float AdjustedQueryDistance = FMath::Max(UE_KINDA_SMALL_NUMBER + DeltaPosVertLength + TargetHeight, QueryDistance);

		// The bottom of the query shape should be at the integrated location (ignoring vertical movement)
		FVector Start = Location + DeltaPosHoriz + (QueryRadius + UE_KINDA_SMALL_NUMBER) * UpDir;
		FVector End = Start - AdjustedQueryDistance * UpDir;
		FHitResult OutHit;
		if (World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(QueryRadius), QueryParams, ResponseParams))
		{
			OutHit = Hits.Last();
		}

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
		// Draw full length of query
		if (GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries)
		{
			const FVector Center = 0.5f * (Start + End);
			const float Dist = (Start - End).Size();
			Chaos::FDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, 0.5f * Dist + QueryRadius, QueryRadius, FQuat::Identity, FColor::Silver, false, -1.f, 10, 1.0f);
		}
#endif

		if (OutHit.bBlockingHit)
		{
			bool bWalkable = UFloorQueryUtils::IsHitSurfaceWalkable(OutHit, MaxWalkSlopeCosine);

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
			if (GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries)
			{
				const FVector Center = Start - 0.5f * OutHit.Distance * UpDir;
				const FColor Color = bWalkable ? FColor::Green : FColor::Red;
				Chaos::FDebugDrawQueue::GetInstance().DrawDebugCapsule(Center, 0.5f * OutHit.Distance + QueryRadius, QueryRadius, FQuat::Identity, Color, false, -1.f, 10, 1.0f);
			}
#endif
			OutFloorResult.bBlockingHit = true;
			OutFloorResult.bWalkableFloor = bWalkable;
			OutFloorResult.FloorDist = UpDir.Dot(Location - OutHit.ImpactPoint);
			OutFloorResult.HitResult = OutHit;
		}
		else
		{
			OutFloorResult.Clear();
			OutFloorResult.FloorDist = 1.0e10f;
		}

		GetWaterResultFromHitResults(Hits, Location, TargetHeight, OutWaterResult);
	}
}

const Chaos::FPBDRigidParticleHandle* UPhysicsMovementUtils::GetRigidParticleHandleFromHitResult(const FHitResult& HitResult)
{
	if (IPhysicsComponent* PhysicsComp = Cast<IPhysicsComponent>(HitResult.Component))
	{
		if (Chaos::FPhysicsObjectHandle PhysicsObject = PhysicsComp->GetPhysicsObjectById(HitResult.Item))
		{
			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			if (const Chaos::FGeometryParticleHandle* Particle = Interface.GetParticle(PhysicsObject))
			{
				return Particle->CastToRigidParticle();
			}
		}
	}

	return nullptr;
}

FVector UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds)
{
	FVector GroundVelocity = FVector::ZeroVector;
	if (const Chaos::FPBDRigidParticleHandle* Rigid = GetRigidParticleHandleFromHitResult(FloorHit))
	{
		FVector Offset = CharacterPosition - Rigid->GetX();
		Offset -= Offset.ProjectOnToNormal(FloorHit.ImpactNormal);

		if (Rigid->KinematicTarget().IsSet())
		{
			const FVector LinearDisplacement = Rigid->KinematicTarget().GetTargetPosition() - Rigid->GetX();
			const FQuat RelativeQuat = Rigid->GetR().Inverse() * Rigid->KinematicTarget().GetTargetRotation();
			const FVector AngularDisplacement = RelativeQuat.ToRotationVector();
			GroundVelocity = (LinearDisplacement + AngularDisplacement.Cross(Offset)) / DeltaSeconds;
		}
		else
		{
			GroundVelocity = Rigid->GetV() + Rigid->GetW().Cross(Offset);
		}
	}
	return GroundVelocity;
}

FVector UPhysicsMovementUtils::ComputeIntegratedGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds)
{
	FVector GroundVelocity = FVector::ZeroVector;
	if (const Chaos::FPBDRigidParticleHandle* Rigid = GetRigidParticleHandleFromHitResult(FloorHit))
	{
		FVector Offset = CharacterPosition - Rigid->GetX();
		Offset -= Offset.ProjectOnToNormal(FloorHit.ImpactNormal);

		if (Rigid->KinematicTarget().IsSet())
		{
			const FVector LinearDisplacement = Rigid->KinematicTarget().GetTargetPosition() - Rigid->GetX();
			const FQuat RelativeQuat = Rigid->GetR().Inverse() * Rigid->KinematicTarget().GetTargetRotation();
			const FVector AngularDisplacement = RelativeQuat.ToRotationVector();
			GroundVelocity = (LinearDisplacement + AngularDisplacement.Cross(Offset)) / DeltaSeconds;
		}
		else
		{
			GroundVelocity = Rigid->GetV() + Rigid->GetW().Cross(Offset);
		}

		if (Rigid->IsDynamic() && Rigid->GravityEnabled())
		{
			if (const UPrimitiveComponent* GroundComp = FloorHit.GetComponent())
			{
				if (const APhysicsVolume* PhysVolume = GroundComp->GetPhysicsVolume())
				{
					GroundVelocity += PhysVolume->GetGravityZ() * FVector::UpVector * DeltaSeconds;
				}
			}
		}
	}
	return GroundVelocity;
}

bool UPhysicsMovementUtils::GetWaterResultFromHitResults(const TArray<FHitResult>& Hits, const FVector& Location, const float TargetHeight, FWaterCheckResult& OutWaterResult)
{
	// Find the closet hit that is a water body
	// Note: Relies on ordering of hit results
	for (int32 Idx = 0; Idx < Hits.Num(); ++Idx)
	{
		const FHitResult& Hit = Hits[Idx];

		if (Hit.Component.IsValid())
		{
			if (AActor* Actor = Hit.Component->GetOwner())
			{
				if (Actor->IsA(AWaterBody::StaticClass()))
				{
					AWaterBody* WaterBody = Cast<AWaterBody>(Actor);

					OutWaterResult.HitResult = Hit;
					OutWaterResult.bSwimmableVolume = true;

					OutWaterResult.WaterSplineData.SplineInputKey = WaterBody->GetWaterBodyComponent()->FindInputKeyClosestToWorldLocation(Location);

					OutWaterResult.WaterSplineData.WaterBody = WaterBody;

					FWaterBodyQueryResult QueryResult = WaterBody->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(
							Location, 
							EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::ComputeNormal | EWaterBodyQueryFlags::ComputeImmersionDepth,
							OutWaterResult.WaterSplineData.SplineInputKey
						);

					OutWaterResult.WaterSplineData.ImmersionDepth = QueryResult.GetImmersionDepth();

					OutWaterResult.WaterSplineData.WaterPlaneLocation = QueryResult.GetWaterPlaneLocation();
					OutWaterResult.WaterSplineData.WaterPlaneNormal = QueryResult.GetWaterPlaneNormal();

					const float CapsuleBottom = Location.Z;
					const float CapsuleTop = Location.Z + (TargetHeight * 2);
					OutWaterResult.WaterSplineData.WaterSurfaceLocation = QueryResult.GetWaterSurfaceLocation();
			
					OutWaterResult.WaterSplineData.WaterSurfaceOffset = OutWaterResult.WaterSplineData.WaterSurfaceLocation - Location;
			
					OutWaterResult.WaterSplineData.ImmersionPercent = FMath::Clamp((OutWaterResult.WaterSplineData.WaterSurfaceLocation.Z - CapsuleBottom) / (CapsuleTop - CapsuleBottom), 0.f, 1.f);
			
					OutWaterResult.WaterSplineData.WaterSurfaceNormal = QueryResult.GetWaterSurfaceNormal();

#if PHYSICSDRIVENMOTION_DEBUG_DRAW
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Location, Location - FVector::UpVector * OutWaterResult.WaterSplineData.ImmersionDepth, FColor::Blue, false, -1.f, 10, 1.0f);
					Chaos::FDebugDrawQueue::GetInstance().DrawDebugPoint(OutWaterResult.HitResult.Location, FColor::Blue, false, -1.f, 10, 1.0f);
#endif
					return true;
				}
			}
		}
	}

	return false;
}