// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLookAtProcessors.h"
#include "MassEntityView.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassLookAtFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationSubsystem.h"
#include "MassRepresentationFragments.h"
#include "Math/UnrealMathUtility.h"
#include "VisualLogger/VisualLogger.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphQuery.h"
#include "BezierUtilities.h"
#include "Algo/RandomShuffle.h"
#include "Engine/World.h"
#include "MassLODFragments.h"
#include "MassGameplayExternalTraits.h"

#define UNSAFE_FOR_MT 1

namespace UE::MassBehavior
{
	namespace Tweakables
	{
		float TrajectoryLookAhead = 600.f;
	}

	FAutoConsoleVariableRef CVars[] =
	{
		FAutoConsoleVariableRef(TEXT("ai.mass.LookAt.TrajectoryLookAhead"), Tweakables::TrajectoryLookAhead,
								TEXT("Distance (in cm) further along the look at trajectory (based on current path) to lookat while moving."), ECVF_Cheat),
	};

	
	// Clamps direction vector to a cone specified by the cone angle along X-axis
	FVector ClampDirectionToXAxisCone(const FVector Direction, const float ConeAngle)
	{
		float ConeSin = 0.0f, ConeCos = 0.0f;
		FMath::SinCos(&ConeSin, &ConeCos, ConeAngle);
		
		const float AngleCos = Direction.X; // Same as FVector::DotProduct(FVector::ForwardVector, Direction);
		if (AngleCos < ConeCos)
		{
			const float DistToRimSq = FMath::Square(Direction.Y) + FMath::Square(Direction.Z);
			const float InvDistToRim = DistToRimSq > KINDA_SMALL_NUMBER ? (1.0f / FMath::Sqrt(DistToRimSq)) : 0.0f;
			return FVector(ConeCos, Direction.Y * InvDistToRim * ConeSin, Direction.Z * InvDistToRim * ConeSin);
		}
		
		return Direction;
	}

	float GazeEnvelope(const float GazeTime, const float GazeDuration, const EMassLookAtGazeMode Mode)
	{
		if (GazeDuration < KINDA_SMALL_NUMBER || Mode == EMassLookAtGazeMode::None)
		{
			return 0.0f;
		}

		if (Mode == EMassLookAtGazeMode::Constant)
		{
			return 1.0;
		}

		// @todo: mae configurable
		const float SustainTime = GazeDuration * 0.25f;
		const float DecayTime = GazeDuration * 0.45f;
		
		if (GazeTime < SustainTime)
		{
			return 1.0f;
		}
		if (GazeTime > DecayTime)
		{
			return 0.0f;
		}
		
		const float Duration = FMath::Max(KINDA_SMALL_NUMBER, DecayTime - SustainTime);
		const float NormTime = FMath::Clamp((GazeTime - SustainTime) / Duration, 0.0f, 1.0f);
		return 1.0f - NormTime;
	}

}// namespace UE::MassBehavior


//----------------------------------------------------------------------//
// UMassLookAtProcessor
//----------------------------------------------------------------------//
UMassLookAtProcessor::UMassLookAtProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void UMassLookAtProcessor::ConfigureQueries()
{
	EntityQuery_Conditional.AddRequirement<FMassLookAtFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery_Conditional.AddRequirement<FMassLookAtTrajectoryFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery_Conditional.AddRequirement<FMassZoneGraphShortPathFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery_Conditional.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	EntityQuery_Conditional.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	EntityQuery_Conditional.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery_Conditional.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
	EntityQuery_Conditional.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddSubsystemRequirement<UZoneGraphSubsystem>(EMassFragmentAccess::ReadOnly);
}

void UMassLookAtProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(LookAtProcessor_Run);

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	EntityQuery_Conditional.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager, CurrentTime, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
		{
			const UMassNavigationSubsystem& MassNavSystem = Context.GetSubsystemChecked<UMassNavigationSubsystem>(World);
			const UZoneGraphSubsystem& ZoneGraphSubsystem = Context.GetSubsystemChecked<UZoneGraphSubsystem>(World);

			const int32 NumEntities = Context.GetNumEntities();
			const TArrayView<FMassLookAtFragment> LookAtList = Context.GetMutableFragmentView<FMassLookAtFragment>();
			const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
			const TConstArrayView<FMassZoneGraphLaneLocationFragment> ZoneGraphLocationList = Context.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
			const TConstArrayView<FMassZoneGraphShortPathFragment> ShortPathList = Context.GetFragmentView<FMassZoneGraphShortPathFragment>();
			const TArrayView<FMassLookAtTrajectoryFragment> LookAtTrajectoryList = Context.GetMutableFragmentView<FMassLookAtTrajectoryFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				FMassLookAtFragment& LookAt = LookAtList[i];
				const FMassMoveTargetFragment& MoveTarget = MoveTargetList[i];
				const FTransformFragment& TransformFragment = TransformList[i];

				const bool bHasLookAtTrajectory = ZoneGraphLocationList.Num() > 0 && LookAtTrajectoryList.Num() > 0 && ShortPathList.Num() > 0;

				bool bDisplayDebug = false;
				const FMassEntityHandle Entity = Context.GetEntity(i);
	#if WITH_MASSGAMEPLAY_DEBUG
				FColor EntityColor = FColor::White;
				bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(Entity, &EntityColor);
	#endif // WITH_MASSGAMEPLAY_DEBUG

				// Update gaze target when current cycle is finished.
				if (LookAt.RandomGazeMode != EMassLookAtGazeMode::None)
				{
					const float TimeSinceUpdate = CurrentTime - LookAt.GazeStartTime;
					if (TimeSinceUpdate >= LookAt.GazeDuration)
					{
						FindNewGazeTarget(MassNavSystem, EntityManager, CurrentTime, TransformFragment.GetTransform(), LookAt);
					}
				}

				// Update specific look at mode.
				LookAt.Direction = FVector::ForwardVector;

				switch (LookAt.LookAtMode)
				{
				case EMassLookAtMode::LookForward:
					// Empty, forward set already above.
					break;
					
				case EMassLookAtMode::LookAlongPath:
					if (bHasLookAtTrajectory)
					{
						const FMassZoneGraphLaneLocationFragment& ZoneGraphLocation = ZoneGraphLocationList[i];
						FMassLookAtTrajectoryFragment& LookAtTrajectory = LookAtTrajectoryList[i];

						if (MoveTarget.GetCurrentActionID() != LookAt.LastSeenActionID)
						{
							const FMassZoneGraphLaneLocationFragment& LaneLocation = ZoneGraphLocationList[i];
							const FMassZoneGraphShortPathFragment& ShortPath = ShortPathList[i];
							
							BuildTrajectory(ZoneGraphSubsystem, LaneLocation, ShortPath, Entity, bDisplayDebug, LookAtTrajectory);
							LookAt.LastSeenActionID = MoveTarget.GetCurrentActionID();
						}
						
						UpdateLookAtTrajectory(TransformFragment.GetTransform(), ZoneGraphLocation, LookAtTrajectory, bDisplayDebug, LookAt);
					}
					break;

				case EMassLookAtMode::LookAtEntity:
					UpdateLookAtTrackedEntity(EntityManager, TransformFragment.GetTransform(), bDisplayDebug, LookAt);
					break;
					
				default:
					break;
				}

				// Apply gaze
				if (LookAt.RandomGazeMode != EMassLookAtGazeMode::None)
				{
					const float TimeSinceUpdate = CurrentTime - LookAt.GazeStartTime;
					const float GazeStrength = UE::MassBehavior::GazeEnvelope(TimeSinceUpdate, LookAt.GazeDuration, LookAt.RandomGazeMode);

					if (GazeStrength > KINDA_SMALL_NUMBER)
					{
						const bool bHasTarget = UpdateGazeTrackedEntity(EntityManager, TransformFragment.GetTransform(), bDisplayDebug, LookAt);

						if (bHasTarget)
						{
							// Treat target gaze as absolute direction.
							LookAt.Direction = FMath::Lerp(LookAt.Direction, LookAt.GazeDirection, GazeStrength).GetSafeNormal();
						}
						else
						{
							// Treat random offset as relative direction.
							const FQuat GazeRotation = FQuat::FindBetweenNormals(FVector::ForwardVector, FMath::Lerp(FVector::ForwardVector, LookAt.GazeDirection, GazeStrength).GetSafeNormal());
							LookAt.Direction = GazeRotation.RotateVector(LookAt.Direction);
						}
					}
				}

				// Clamp
				LookAt.Direction = UE::MassBehavior::ClampDirectionToXAxisCone(LookAt.Direction, FMath::DegreesToRadians(AngleThresholdInDegrees));

	#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
				if (bDisplayDebug)
				{
					const FVector Origin = TransformFragment.GetTransform().GetLocation() + FVector(0.f,0.f,DebugZOffset);
					const FVector Dest = Origin + 100.f*TransformFragment.GetTransform().TransformVector(LookAt.Direction);
					UE_VLOG_ARROW(this, LogMassBehavior, Display, Origin, Dest, EntityColor, TEXT(""));
				}
	#endif // WITH_MASSGAMEPLAY_DEBUG
			}
		});
}

void UMassLookAtProcessor::FindNewGazeTarget(const UMassNavigationSubsystem& MassNavSystem, const FMassEntityManager& EntityManager, const float CurrentTime, const FTransform& Transform, FMassLookAtFragment& LookAt) const
{
	const FNavigationObstacleHashGrid2D& ObstacleGrid = MassNavSystem.GetObstacleGrid();
	const FMassEntityHandle LastTrackedEntity = LookAt.GazeTrackedEntity;
	
	LookAt.GazeTrackedEntity.Reset();
	LookAt.GazeDirection = FVector::ForwardVector;

	// Search for potential targets in front
	bool bTargetFound = false;
	if (LookAt.bRandomGazeEntities)
	{
		const float CosAngleThreshold = FMath::Cos(FMath::DegreesToRadians(AngleThresholdInDegrees));
		const FVector Extent(QueryExtent, QueryExtent, QueryExtent);
		const FVector QueryOrigin = Transform.TransformPosition(FVector(0.5f*QueryExtent, 0.f, 0.f));
		const FBox QueryBox = FBox(QueryOrigin - 0.5f*Extent, QueryOrigin + 0.5f*Extent);

		TArray<FNavigationObstacleHashGrid2D::ItemIDType> NearbyEntities;
		NearbyEntities.Reserve(16);
		ObstacleGrid.Query(QueryBox, NearbyEntities);

		// We'll pick the first entity that passes, this ensure that it's random one.
		Algo::RandomShuffle(NearbyEntities);

		const FVector Location = Transform.GetLocation();
		
		for (const FNavigationObstacleHashGrid2D::ItemIDType NearbyEntity : NearbyEntities)
		{
			// This can happen if we remove entities in the system.
			if (!EntityManager.IsEntityValid(NearbyEntity.Entity))
			{
				UE_LOG(LogMassBehavior, VeryVerbose, TEXT("Nearby entity is invalid, skipped."));
				continue;
			}

			// Do not select same target twice in a row.
			if (NearbyEntity.Entity == LastTrackedEntity)
			{
				continue;
			}

			FMassEntityView EntityView(EntityManager, NearbyEntity.Entity);
			if (!EntityView.HasTag<FMassLookAtTargetTag>())
			{
				continue;
			}

			// TargetTag is added through the LookAtTargetTrait and Transform was added with it
			const FTransformFragment& TargetTransform = EntityView.GetFragmentData<FTransformFragment>();
			const FVector TargetLocation = TargetTransform.GetTransform().GetLocation();
			FVector Direction = (TargetLocation - Location).GetSafeNormal();
			Direction = Transform.InverseTransformVector(Direction);

			const bool bIsTargetInView = FVector::DotProduct(FVector::ForwardVector, Direction) > CosAngleThreshold;
			if (bIsTargetInView)
			{
				LookAt.GazeDirection = Direction;
				LookAt.GazeTrackedEntity = NearbyEntity.Entity;
				bTargetFound = true;
				break;
			}

			// Allow to pick entities out of view if they are moving towards us.
			if (const FMassVelocityFragment* Velocity = EntityView.GetFragmentDataPtr<FMassVelocityFragment>())
			{
				const FVector MoveDirection = Transform.InverseTransformVector(Velocity->Value.GetSafeNormal());
				
				const bool bIsTargetMovingTowards = FVector::DotProduct(MoveDirection, -Direction) > CosAngleThreshold; // Direction negated as it is from the agent to target, and we want target to agent. 
				if (bIsTargetMovingTowards)
				{
					LookAt.GazeDirection = Direction;
					LookAt.GazeTrackedEntity = NearbyEntity.Entity;
					bTargetFound = true;
					break;
				}
			}
			
		}
	}

	// If no gaze target found, use random angle if specified.
	if (!bTargetFound)
	{
		const FRotator Rot(FMath::FRandRange(-(float)LookAt.RandomGazePitchVariation, (float)LookAt.RandomGazePitchVariation),FMath::FRandRange(-(float)LookAt.RandomGazeYawVariation, (float)LookAt.RandomGazeYawVariation), 0.f);
		LookAt.GazeDirection = UE::MassBehavior::ClampDirectionToXAxisCone(Rot.Vector(), FMath::DegreesToRadians(AngleThresholdInDegrees));
	}

	// @todo: This does not currently carry over time. It's intentional, since there might be big gaps between updates.
	LookAt.GazeStartTime = CurrentTime;
	LookAt.GazeDuration = FMath::FRandRange(FMath::Max(Duration - DurationVariation, 0.f), Duration + DurationVariation);
}

void UMassLookAtProcessor::UpdateLookAtTrajectory(const FTransform& Transform, const FMassZoneGraphLaneLocationFragment& ZoneGraphLocation,
												  const FMassLookAtTrajectoryFragment& LookAtTrajectory, const bool bDisplayDebug, FMassLookAtFragment& LookAt) const
{
	if (LookAtTrajectory.NumPoints > 0 && LookAtTrajectory.LaneHandle == ZoneGraphLocation.LaneHandle)
	{
		// Look at anticipated position in the future when moving.
		const float LookAheadDistanceAlongPath = ZoneGraphLocation.DistanceAlongLane + UE::MassBehavior::Tweakables::TrajectoryLookAhead * (LookAtTrajectory.bMoveReverse ? -1.0f : 1.0f);

		// Calculate lookat direction to the anticipated position.
		const FVector AnticipatedPosition = LookAtTrajectory.GetPointAtDistanceExtrapolated(LookAheadDistanceAlongPath);
		const FVector AgentPosition = Transform.GetLocation();
		const FVector NewGlobalDirection = (AnticipatedPosition - AgentPosition).GetSafeNormal();
		LookAt.Direction = Transform.InverseTransformVector(NewGlobalDirection);
		LookAt.Direction.Z = 0.0f;
						
#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
		if (bDisplayDebug)
		{
			const FVector ZOffset(0.f,0.f,DebugZOffset);
			UE_VLOG_SEGMENT_THICK(this, LogMassBehavior, Display, AgentPosition + ZOffset, AgentPosition + ZOffset + 100.f * NewGlobalDirection, FColor::White, /*Thickness*/3, TEXT("LookAt Trajectory"));
		}
#endif
	}
}

void UMassLookAtProcessor::UpdateLookAtTrackedEntity(const FMassEntityManager& EntityManager, const FTransform& Transform, const bool bDisplayDebug, FMassLookAtFragment& LookAt) const
{
	// Update direction toward target
	if (EntityManager.IsEntityValid(LookAt.TrackedEntity))
	{
		if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(LookAt.TrackedEntity))
		{
			const FVector AgentPosition = Transform.GetLocation();
			const FVector NewGlobalDirection = (TargetTransform->GetTransform().GetLocation() - AgentPosition).GetSafeNormal();
			LookAt.Direction = Transform.InverseTransformVector(NewGlobalDirection);

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
			if (bDisplayDebug)
			{
				const FVector ZOffset(0.f,0.f,DebugZOffset);
				UE_VLOG_SEGMENT_THICK(this, LogMassBehavior, Display, AgentPosition + ZOffset, AgentPosition + ZOffset + 100.f * NewGlobalDirection, FColor::White, /*Thickness*/3, TEXT("LookAt Track"));
			}
#endif
		}
	}
}

bool UMassLookAtProcessor::UpdateGazeTrackedEntity(const FMassEntityManager& EntityManager, const FTransform& Transform, const bool bDisplayDebug, FMassLookAtFragment& LookAt) const
{
	bool bHasTarget = false;
	
	// Update direction toward gaze target
	if (LookAt.GazeTrackedEntity.IsSet() && EntityManager.IsEntityValid(LookAt.GazeTrackedEntity))
	{
		if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(LookAt.GazeTrackedEntity))
		{
			const FVector AgentPosition = Transform.GetLocation();
			const FVector NewGlobalDirection = (TargetTransform->GetTransform().GetLocation() - AgentPosition).GetSafeNormal();
			LookAt.GazeDirection = Transform.InverseTransformVector(NewGlobalDirection);

			bHasTarget = true;
			
#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
			if (bDisplayDebug)
			{
				const FVector ZOffset(0.f,0.f,DebugZOffset);
				UE_VLOG_SEGMENT_THICK(this, LogMassBehavior, Display, AgentPosition + ZOffset, AgentPosition + ZOffset + 100.f * NewGlobalDirection, FColor(160,160,160), /*Thickness*/3, TEXT("Gaze Track"));
			}
#endif
		}
	}

	return bHasTarget;
}

void UMassLookAtProcessor::BuildTrajectory(const UZoneGraphSubsystem& ZoneGraphSubsystem, const FMassZoneGraphLaneLocationFragment& LaneLocation, const FMassZoneGraphShortPathFragment& ShortPath,
											const FMassEntityHandle Entity, const bool bDisplayDebug, FMassLookAtTrajectoryFragment& LookAtTrajectory)
{
	LookAtTrajectory.Reset();

	if (ShortPath.NumPoints < 2)
	{
		return;
	}

	LookAtTrajectory.bMoveReverse = ShortPath.bMoveReverse;
	LookAtTrajectory.LaneHandle = LaneLocation.LaneHandle;

	const float NextLaneLookAheadDistance = UE::MassBehavior::Tweakables::TrajectoryLookAhead;
	
	// Initialize the look at trajectory from the current path.
	const FMassZoneGraphPathPoint& FirstPathPoint = ShortPath.Points[0];
	const FMassZoneGraphPathPoint& LastPathPoint = ShortPath.Points[ShortPath.NumPoints - 1];
	ensure(LookAtTrajectory.AddPoint(FirstPathPoint.Position, FirstPathPoint.Tangent.Get(), FirstPathPoint.DistanceAlongLane.Get()));
	ensure(LookAtTrajectory.AddPoint(LastPathPoint.Position, LastPathPoint.Tangent.Get(), LastPathPoint.DistanceAlongLane.Get()));

	// If the path will lead to next lane, add a point from next lane too.
	if (ShortPath.NextLaneHandle.IsValid())
	{
		const FZoneGraphStorage* ZoneGraphStorage = ZoneGraphSubsystem.GetZoneGraphStorage(LaneLocation.LaneHandle.DataHandle);
		if (ZoneGraphStorage != nullptr)
		{
			if (ShortPath.NextExitLinkType == EZoneLaneLinkType::Outgoing)
			{
				FZoneGraphLaneLocation Location;
				UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, ShortPath.NextLaneHandle, NextLaneLookAheadDistance, Location);

				ensure(LookAtTrajectory.AddPoint(Location.Position, FVector2D(Location.Tangent), LastPathPoint.DistanceAlongLane.Get() + Location.DistanceAlongLane));
			}
			else if (ShortPath.NextExitLinkType == EZoneLaneLinkType::Incoming)
			{
				float LaneLength = 0.0f;
				UE::ZoneGraph::Query::GetLaneLength(*ZoneGraphStorage, ShortPath.NextLaneHandle, LaneLength);

				FZoneGraphLaneLocation Location;
				UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, ShortPath.NextLaneHandle, LaneLength - NextLaneLookAheadDistance, Location);

				// Moving backwards, reverse tangent and distance.
				ensure(LookAtTrajectory.bMoveReverse);
				ensure(LookAtTrajectory.AddPoint(Location.Position, FVector2D(-Location.Tangent), LastPathPoint.DistanceAlongLane.Get() - (LaneLength - Location.DistanceAlongLane)));
			}
			else if (ShortPath.NextExitLinkType == EZoneLaneLinkType::Adjacent)
			{
				// No extra point
			}
			else
			{
				ensureMsgf(false, TEXT("Unhandle NextExitLinkType type %s"), *UEnum::GetValueAsString(ShortPath.NextExitLinkType));
			}
		}
		else
		{
			UE_CVLOG(bDisplayDebug, this, LogMassBehavior, Error, TEXT("%s Could not find ZoneGraph storage for lane %s."),
				*Entity.DebugGetDescription(), *LaneLocation.LaneHandle.ToString());
		}				
	}

	// Ensure that the points are always in ascending distance order (it is, in case of reverse path).
	if (LookAtTrajectory.NumPoints > 1 && LookAtTrajectory.bMoveReverse)
	{
		ensureMsgf(LookAtTrajectory.Points[0].DistanceAlongLane.Get() >= LookAtTrajectory.Points[LookAtTrajectory.NumPoints - 1].DistanceAlongLane.Get(),
			TEXT("Expecting trajectory 0 (%.1f) >= %d (%.1f)"), LookAtTrajectory.Points[0].DistanceAlongLane.Get(),
			LookAtTrajectory.NumPoints - 1, LookAtTrajectory.Points[LookAtTrajectory.NumPoints - 1].DistanceAlongLane.Get());
		
		Algo::Reverse(LookAtTrajectory.Points.GetData(), LookAtTrajectory.NumPoints);
		// Tangents needs to be reversed when the trajectory is reversed.
		for (uint8 PointIndex = 0; PointIndex < LookAtTrajectory.NumPoints; PointIndex++)
		{
			LookAtTrajectory.Points[PointIndex].Tangent.Set(-LookAtTrajectory.Points[PointIndex].Tangent.Get());
		}
	}

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
	if (bDisplayDebug)
	{
		const FVector ZOffset(0,0,35);
		
		for (uint8 PointIndex = 0; PointIndex < LookAtTrajectory.NumPoints - 1; PointIndex++)
		{
			const FMassLookAtTrajectoryPoint& CurrPoint = LookAtTrajectory.Points[PointIndex];
			const FMassLookAtTrajectoryPoint& NextPoint = LookAtTrajectory.Points[PointIndex + 1];

			// Trajectory
			const FVector StartPoint = CurrPoint.Position;
			const FVector StartForward = CurrPoint.Tangent.GetVector();
			const FVector EndPoint = NextPoint.Position;
			const FVector EndForward = NextPoint.Tangent.GetVector();
			const float TangentDistance = FVector::Dist(StartPoint, EndPoint) / 3.0f;
			const FVector StartControlPoint = StartPoint + StartForward * TangentDistance;
			const FVector EndControlPoint = EndPoint - EndForward * TangentDistance;

			static constexpr int32 NumTicks = 6;
			static constexpr float DeltaT = 1.0f / NumTicks;
			
			FVector PrevPoint = StartPoint;
			for (int32 j = 0; j < NumTicks; j++)
			{
				const float T = (j + 1) * DeltaT;
				const FVector Point = UE::CubicBezier::Eval(StartPoint, StartControlPoint, EndControlPoint, EndPoint, T);
				UE_VLOG_SEGMENT_THICK(this, LogMassBehavior, Display, PrevPoint + ZOffset, Point + ZOffset, FColor::White, /*Thickness*/3, TEXT(""));
				PrevPoint = Point;
			}
		}
		
		for (uint8 PointIndex = 0; PointIndex < LookAtTrajectory.NumPoints; PointIndex++)
		{
			const FMassLookAtTrajectoryPoint& CurrPoint = LookAtTrajectory.Points[PointIndex];
			const FVector CurrBase = CurrPoint.Position + ZOffset * 1.1f;
			// Tangents
			UE_VLOG_SEGMENT_THICK(this, LogMassBehavior, Display, CurrBase, CurrBase + CurrPoint.Tangent.GetVector() * 100.0f, FColorList::Grey, /*Thickness*/1,
				TEXT("D:%.1f"), CurrPoint.DistanceAlongLane.Get());
		}
	}
#endif // WITH_MASSGAMEPLAY_DEBUG
}

#undef UNSAFE_FOR_MT