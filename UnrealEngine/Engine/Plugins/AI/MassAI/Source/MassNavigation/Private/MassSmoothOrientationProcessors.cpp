// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmoothOrientation/MassSmoothOrientationProcessors.h"
#include "SmoothOrientation/MassSmoothOrientationFragments.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "MassMovementFragments.h"
#include "Math/UnrealMathUtility.h"
#include "MassSimulationLOD.h"
#include "MassNavigationUtils.h"

#define UNSAFE_FOR_MT 0
#define MOVEMENT_DEBUGDRAW 0	// Set to 1 to see heading debugdraw

//----------------------------------------------------------------------//
//  UMassSmoothOrientationProcessor
//----------------------------------------------------------------------//
UMassSmoothOrientationProcessor::UMassSmoothOrientationProcessor()
	: HighResEntityQuery(*this)
	, LowResEntityQuery_Conditional(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void UMassSmoothOrientationProcessor::ConfigureQueries()
{
	HighResEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	HighResEntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	HighResEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	HighResEntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	HighResEntityQuery.AddConstSharedRequirement<FMassSmoothOrientationParameters>(EMassFragmentPresence::All);

	LowResEntityQuery_Conditional.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	LowResEntityQuery_Conditional.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	LowResEntityQuery_Conditional.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::All);
	LowResEntityQuery_Conditional.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	LowResEntityQuery_Conditional.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);
}

void UMassSmoothOrientationProcessor::Execute(FMassEntityManager& EntityManager,
													FMassExecutionContext& Context)
{
	// Clamp max delta time to avoid force explosion on large time steps (i.e. during initialization).
	const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

	{
		QUICK_SCOPE_CYCLE_COUNTER(HighRes);

		HighResEntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const FMassSmoothOrientationParameters& OrientationParams = Context.GetConstSharedFragment<FMassSmoothOrientationParameters>();

			const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
			const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
			const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];

				// Do not touch transform at all when animating
				if (MoveTarget.GetCurrentAction() == EMassMovementAction::Animate)
				{
					continue;
				}

				const FMassVelocityFragment& CurrentVelocity = VelocityList[EntityIndex];
				FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
				const FVector CurrentForward = CurrentTransform.GetRotation().GetForwardVector();
				const float CurrentHeading = UE::MassNavigation::GetYawFromDirection(CurrentForward);

				const float EndOfPathAnticipationDistance = OrientationParams.EndOfPathDuration * MoveTarget.DesiredSpeed.Get();
				
				float MoveTargetWeight = 0.5f;
				float VelocityWeight = 0.5f;
				
				if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move)
				{
					if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand && MoveTarget.DistanceToGoal < EndOfPathAnticipationDistance)
					{
						// Fade towards the movement target direction at the end of the path.
						const float Fade = FMath::Square(FMath::Clamp(MoveTarget.DistanceToGoal / EndOfPathAnticipationDistance, 0.0f, 1.0f)); // zero at end of the path

						MoveTargetWeight = FMath::Lerp(OrientationParams.Standing.MoveTargetWeight, OrientationParams.Moving.MoveTargetWeight, Fade);
						VelocityWeight = FMath::Lerp(OrientationParams.Standing.VelocityWeight, OrientationParams.Moving.VelocityWeight, Fade);
					}
					else
					{
						MoveTargetWeight = OrientationParams.Moving.MoveTargetWeight;
						VelocityWeight = OrientationParams.Moving.VelocityWeight;
					}
				}
				else // Stand
				{
					MoveTargetWeight = OrientationParams.Standing.MoveTargetWeight;
					VelocityWeight = OrientationParams.Standing.VelocityWeight;
				}
				
				const float VelocityHeading = UE::MassNavigation::GetYawFromDirection(CurrentVelocity.Value);
				const float MovementHeading = UE::MassNavigation::GetYawFromDirection(MoveTarget.Forward);

				const float Ratio = MoveTargetWeight / (MoveTargetWeight + VelocityWeight);
				const float DesiredHeading = UE::MassNavigation::LerpAngle(VelocityHeading, MovementHeading,Ratio);
				
				const float NewHeading = UE::MassNavigation::ExponentialSmoothingAngle(CurrentHeading, DesiredHeading, DeltaTime, OrientationParams.OrientationSmoothingTime);

				FQuat Rotation(FVector::UpVector, NewHeading);
				CurrentTransform.SetRotation(Rotation);
			}
		});
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(LowRes);

		LowResEntityQuery_Conditional.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
			const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
				const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];

				// Snap position to move target directly
				CurrentTransform.SetRotation(FQuat::FindBetweenNormals(FVector::ForwardVector, MoveTarget.Forward));
			}
		});
	}
}

#undef UNSAFE_FOR_MT
