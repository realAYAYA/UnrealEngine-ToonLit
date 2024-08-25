// Copyright Epic Games, Inc. All Rights Reserved.

#include "Steering/MassSteeringProcessors.h"
#include "MassCommonUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "Math/UnrealMathUtility.h"
#include "MassSimulationLOD.h"

#define UNSAFE_FOR_MT 0
#define MOVEMENT_DEBUGDRAW 0	// Set to 1 to see heading debugdraw

namespace UE::MassNavigation
{
	/*
	* Calculates speed scale based on agent's forward direction and desired steering direction.
	*/
	static FVector::FReal CalcDirectionalSpeedScale(const FVector ForwardDirection, const FVector SteerDirection)
	{
		// @todo: make these configurable
		constexpr FVector::FReal ForwardSpeedScale = 1.;
		constexpr FVector::FReal BackwardSpeedScale = 0.25;
		constexpr FVector::FReal SideSpeedScale = 0.5;

		const FVector LeftDirection = FVector::CrossProduct(ForwardDirection, FVector::UpVector);
		const FVector::FReal DirX = FVector::DotProduct(LeftDirection, SteerDirection);
		const FVector::FReal DirY = FVector::DotProduct(ForwardDirection, SteerDirection);

		// Calculate intersection between a direction vector and ellipse, where A & B are the size of the ellipse.
		// The direction vector is starting from the center of the ellipse.
		constexpr FVector::FReal SideA = SideSpeedScale;
		const FVector::FReal SideB = DirY > 0. ? ForwardSpeedScale : BackwardSpeedScale;
		const FVector::FReal Disc = FMath::Square(SideA) * FMath::Square(DirY) + FMath::Square(SideB) * FMath::Square(DirX);
		const FVector::FReal Speed = (Disc > SMALL_NUMBER) ? (SideA * SideB / FMath::Sqrt(Disc)) : 0.;

		return Speed;
	}

	/** Speed envelope when approaching a point. NormalizedDistance in range [0..1] */
	static FVector::FReal ArrivalSpeedEnvelope(const FVector::FReal NormalizedDistance)
	{
		return FMath::Sqrt(NormalizedDistance);
	}

} // UE::MassNavigation

//----------------------------------------------------------------------//
//  UMassSteerToMoveTargetProcessor
//----------------------------------------------------------------------//
UMassSteerToMoveTargetProcessor::UMassSteerToMoveTargetProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = int32(EProcessorExecutionFlags::AllNetModes);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UMassSteerToMoveTargetProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStandingSteeringFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovingSteeringParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassStandingSteeringParameters>(EMassFragmentPresence::All);

	// No need for Off LOD to do steering, applying move target directly
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
}

void UMassSteerToMoveTargetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TArrayView<FMassSteeringFragment> SteeringList = Context.GetMutableFragmentView<FMassSteeringFragment>();
		const TArrayView<FMassStandingSteeringFragment> StandingSteeringList = Context.GetMutableFragmentView<FMassStandingSteeringFragment>();
		const TArrayView<FMassGhostLocationFragment> GhostList = Context.GetMutableFragmentView<FMassGhostLocationFragment>();
		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
		const FMassMovingSteeringParameters& MovingSteeringParams = Context.GetConstSharedFragment<FMassMovingSteeringParameters>();
		const FMassStandingSteeringParameters& StandingSteeringParams = Context.GetConstSharedFragment<FMassStandingSteeringParameters>();

		const FVector::FReal SteerK = 1. / MovingSteeringParams.ReactionTime;
		const float DeltaTime = Context.GetDeltaTimeSeconds();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FTransformFragment& TransformFragment = TransformList[EntityIndex];
			FMassSteeringFragment& Steering = SteeringList[EntityIndex];
			FMassStandingSteeringFragment& StandingSteering = StandingSteeringList[EntityIndex];
			FMassGhostLocationFragment& Ghost = GhostList[EntityIndex];
			FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];
			FMassForceFragment& Force = ForceList[EntityIndex];
			FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
			const FMassEntityHandle Entity = Context.GetEntity(EntityIndex);

			const FTransform& Transform = TransformFragment.GetTransform();;

			// Calculate velocity for steering.
			const FVector CurrentLocation = Transform.GetLocation();
			const FVector CurrentForward = Transform.GetRotation().GetForwardVector();

			const FVector::FReal LookAheadDistance = FMath::Max(1.0f, MovingSteeringParams.LookAheadTime * MoveTarget.DesiredSpeed.Get());

			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move)
			{
				// Tune down avoidance and speed when arriving at goal.
				FVector::FReal ArrivalFade = 1.;
				if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
				{
					ArrivalFade = FMath::Clamp(MoveTarget.DistanceToGoal / LookAheadDistance, 0., 1.);
				}
				const FVector::FReal SteeringPredictionDistance = LookAheadDistance * ArrivalFade;

				// Steer towards and along the move target.
				const FVector TargetSide = FVector::CrossProduct(MoveTarget.Forward, FVector::UpVector);
				const FVector Delta = CurrentLocation - MoveTarget.Center;

				const FVector::FReal ForwardOffset = FVector::DotProduct(MoveTarget.Forward, Delta);

				// Calculate steering direction. When far away from the line defined by TargetPosition and TargetTangent,
				// the steering direction is towards the line, the close we get, the more it aligns with the line.
				const FVector::FReal SidewaysOffset = FVector::DotProduct(TargetSide, Delta);
				const FVector::FReal SteerForward = FMath::Sqrt(FMath::Max(0., FMath::Square(SteeringPredictionDistance) - FMath::Square(SidewaysOffset)));

				// The Max() here makes the steering directions behind the TargetPosition to steer towards it directly.
				FVector SteerTarget = MoveTarget.Center + MoveTarget.Forward * FMath::Clamp(ForwardOffset + SteerForward, 0., SteeringPredictionDistance);

				FVector SteerDirection = SteerTarget - CurrentLocation;
				SteerDirection.Z = 0.;
				const FVector::FReal DistanceToSteerTarget = SteerDirection.Length();
				if (DistanceToSteerTarget > KINDA_SMALL_NUMBER)
				{
					SteerDirection *= 1. / DistanceToSteerTarget;
				}
				
				const FVector::FReal DirSpeedScale = UE::MassNavigation::CalcDirectionalSpeedScale(CurrentForward, SteerDirection);
				FVector::FReal DesiredSpeed = MoveTarget.DesiredSpeed.Get() * DirSpeedScale;

				// Control speed based relation to the forward axis of the move target.
				FVector::FReal CatchupDesiredSpeed = DesiredSpeed;
				if (ForwardOffset < 0.)
				{
					// Falling behind, catch up
					const FVector::FReal T = FMath::Min(-ForwardOffset / LookAheadDistance, 1.);
					CatchupDesiredSpeed = FMath::Lerp(DesiredSpeed, MovementParams.MaxSpeed, T);
				}
				else if (ForwardOffset > 0.)
				{
					// Ahead, slow down.
					const FVector::FReal T = FMath::Min(ForwardOffset / LookAheadDistance, 1.);
					CatchupDesiredSpeed = FMath::Lerp(DesiredSpeed, DesiredSpeed * 0., 1. - FMath::Square(1. - T));
				}

				// Control speed based on distance to move target. This allows to catch up even if speed above reaches zero.
				const FVector::FReal DeviantSpeed = FMath::Min(FMath::Abs(SidewaysOffset) / LookAheadDistance, 1.) * DesiredSpeed;

				DesiredSpeed = FMath::Max(CatchupDesiredSpeed, DeviantSpeed);

				// Slow down towards the end of path.
				if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
				{
					const FVector::FReal NormalizedDistanceToSteerTarget = FMath::Clamp(DistanceToSteerTarget / LookAheadDistance, 0., 1.);
					DesiredSpeed *= UE::MassNavigation::ArrivalSpeedEnvelope(FMath::Max(ArrivalFade, NormalizedDistanceToSteerTarget));
				}

				MoveTarget.bSteeringFallingBehind = ForwardOffset < -LookAheadDistance * 0.8;

				// @todo: This current completely overrides steering, we probably should have one processor that resets the steering at the beginning of the frame.
				Steering.DesiredVelocity = SteerDirection * DesiredSpeed;
				Force.Value = SteerK * (Steering.DesiredVelocity - Velocity.Value); // Goal force
			}
			else if (MoveTarget.GetCurrentAction() == EMassMovementAction::Stand)
			{
				// Calculate unique target move threshold so that different agents react a bit differently.
				const FVector::FReal PerEntityScale = UE::RandomSequence::FRand(Entity.Index);
				const FVector::FReal TargetMoveThreshold = StandingSteeringParams.TargetMoveThreshold * (1. - StandingSteeringParams.TargetMoveThresholdVariance + PerEntityScale * StandingSteeringParams.TargetMoveThresholdVariance * 2.);
				
				if (Ghost.LastSeenActionID != MoveTarget.GetCurrentActionID())
				{
					// Reset when action changes. @todo: should reset only when move->stand?
					Ghost.Location = MoveTarget.Center;
					Ghost.Velocity = FVector::ZeroVector;
					Ghost.LastSeenActionID = MoveTarget.GetCurrentActionID();

					StandingSteering.TargetLocation = MoveTarget.Center;
					StandingSteering.TrackedTargetSpeed = 0.0f;
					StandingSteering.bIsUpdatingTarget = false;
					StandingSteering.TargetSelectionCooldown = StandingSteeringParams.TargetSelectionCooldown * FMath::RandRange(1.f - StandingSteeringParams.TargetSelectionCooldownVariance, 1.f + StandingSteeringParams.TargetSelectionCooldownVariance);
					StandingSteering.bEnteredFromMoveAction = MoveTarget.GetPreviousAction() == EMassMovementAction::Move;
				}

				StandingSteering.TargetSelectionCooldown = FMath::Max(0.0f, StandingSteering.TargetSelectionCooldown - DeltaTime);

				if (!StandingSteering.bIsUpdatingTarget)
				{
					// Update the move target if enough time has passed and the target has moved. 
					if (StandingSteering.TargetSelectionCooldown <= 0.0f
						&& FVector::DistSquared(StandingSteering.TargetLocation, Ghost.Location) > FMath::Square(TargetMoveThreshold))
					{
						StandingSteering.TargetLocation = Ghost.Location;
						StandingSteering.TrackedTargetSpeed = 0.0f;
						StandingSteering.bIsUpdatingTarget = true;
						StandingSteering.bEnteredFromMoveAction = false;
					}
				}
				else
				{
					// Updating target
					StandingSteering.TargetLocation = Ghost.Location;
					const FVector::FReal GhostSpeed = Ghost.Velocity.Length();

					if (GhostSpeed > (StandingSteering.TrackedTargetSpeed * StandingSteeringParams.TargetSpeedHysteresisScale))
					{
						const FVector::FReal TrackedTargetSpeed = FMath::Max(StandingSteering.TrackedTargetSpeed, GhostSpeed);
						StandingSteering.TrackedTargetSpeed = static_cast<float>(TrackedTargetSpeed);
					}
					else
					{
						// Speed is dropping, we have found the peak change, stop updating the target and start cooldown.
						StandingSteering.TargetSelectionCooldown = StandingSteeringParams.TargetSelectionCooldown * FMath::RandRange(1.0f - StandingSteeringParams.TargetSelectionCooldownVariance, 1.0f + StandingSteeringParams.TargetSelectionCooldownVariance);
						StandingSteering.bIsUpdatingTarget = false;
					}
				}
				
				// Move directly towards the move target when standing.
				FVector SteerDirection = FVector::ZeroVector;
				FVector::FReal DesiredSpeed = 0.;

				FVector Delta = StandingSteering.TargetLocation - CurrentLocation;
				Delta.Z = 0.;
				const FVector::FReal Distance = Delta.Size();
				if (Distance > StandingSteeringParams.DeadZoneRadius)
				{
					SteerDirection = Delta / Distance;
					if (StandingSteering.bEnteredFromMoveAction)
					{
						// If the current steering target is from approaching a move target, use the same speed logic as movement to ensure smooth transition.
						const FVector::FReal Range = FMath::Max(1., LookAheadDistance - StandingSteeringParams.DeadZoneRadius);
						const FVector::FReal SpeedFade = FMath::Clamp((Distance - StandingSteeringParams.DeadZoneRadius) / Range, 0., 1.);
						DesiredSpeed = MoveTarget.DesiredSpeed.Get() * UE::MassNavigation::CalcDirectionalSpeedScale(CurrentForward, SteerDirection) * UE::MassNavigation::ArrivalSpeedEnvelope(SpeedFade);
					}
					else
					{
						const FVector::FReal Range = FMath::Max(1., LookAheadDistance - StandingSteeringParams.DeadZoneRadius);
						const FVector::FReal SpeedFade = FMath::Clamp((Distance - StandingSteeringParams.DeadZoneRadius) / Range, 0., 1.);
						// Not using the directional scaling so that the steps we take to avoid are done quickly, and the behavior is reactive.
						DesiredSpeed = MoveTarget.DesiredSpeed.Get() * UE::MassNavigation::ArrivalSpeedEnvelope(SpeedFade);
					}
					
					// @todo: This current completely overrides steering, we probably should have one processor that resets the steering at the beginning of the frame.
					Steering.DesiredVelocity = SteerDirection * DesiredSpeed;
					Force.Value = SteerK * (Steering.DesiredVelocity - Velocity.Value); // Goal force

				}
				else
				{
					// When reached destination, clamp small velocities to zero to avoid tiny drifting.
					if (Velocity.Value.SquaredLength() < FMath::Square(StandingSteeringParams.LowSpeedThreshold))
					{
						Velocity.Value = FVector::ZeroVector;
						Force.Value = FVector::ZeroVector;
					}
				}

				MoveTarget.bSteeringFallingBehind = false;
			}
			else if (MoveTarget.GetCurrentAction() == EMassMovementAction::Animate)
			{
				// Stop all movement when animating.
				Steering.Reset();
				MoveTarget.bSteeringFallingBehind = false;
				Force.Value = FVector::ZeroVector;
				Velocity.Value = FVector::ZeroVector;
			}

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
			FColor EntityColor = FColor::White;
			const bool bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(Entity, &EntityColor);
			if (bDisplayDebug)
			{
				const FVector ZOffset(0,0,25);

				const FColor DarkEntityColor = UE::MassNavigation::Debug::MixColors(EntityColor, FColor::Black);
				const FColor LightEntityColor = UE::MassNavigation::Debug::MixColors(EntityColor, FColor::White);
				
				const FVector MoveTargetCenter = MoveTarget.Center + ZOffset;

				// MoveTarget slack boundary
				UE_VLOG_CIRCLE_THICK(this, LogMassNavigation, Log, MoveTargetCenter, FVector::UpVector, LookAheadDistance, EntityColor, /*Thickness*/2,
					TEXT("%s MoveTgt %s"), *Entity.DebugGetDescription(), *UEnum::GetDisplayValueAsText(MoveTarget.IntentAtGoal).ToString());

				// MoveTarget orientation
				UE_VLOG_SEGMENT_THICK(this, LogMassNavigation, Log, MoveTargetCenter, MoveTargetCenter + MoveTarget.Forward * LookAheadDistance, EntityColor, /*Thickness*/2, TEXT(""));

				// MoveTarget - current location relation.
				if (FVector::Dist2D(CurrentLocation, MoveTarget.Center) > LookAheadDistance * 1.5f)
				{
					UE_VLOG_SEGMENT_THICK(this, LogMassNavigation, Log, MoveTargetCenter, CurrentLocation + ZOffset, FColor::Red, /*Thickness*/1, TEXT("LOST"));
				}
				else
				{
					UE_VLOG_SEGMENT_THICK(this, LogMassNavigation, Log, MoveTargetCenter, CurrentLocation + ZOffset, DarkEntityColor, /*Thickness*/1, TEXT(""));
				}

				// Steering
				UE_VLOG_SEGMENT_THICK(this, LogMassNavigation, Log, CurrentLocation + ZOffset, CurrentLocation + Steering.DesiredVelocity + ZOffset, LightEntityColor, /*Thickness*/2,
					TEXT("%s Steer %.1f"), *Entity.DebugGetDescription(), Steering.DesiredVelocity.Length());
			}
#endif // WITH_MASSGAMEPLAY_DEBUG
			
		}
	});
}

#undef UNSAFE_FOR_MT
