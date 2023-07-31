// Copyright Epic Games, Inc. All Rights Reserved.

#include "Movement/MassMovementProcessors.h"
#include "MassCommonUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Math/UnrealMathUtility.h"
#include "MassSimulationLOD.h"

#define UNSAFE_FOR_MT 0
#define MOVEMENT_DEBUGDRAW 0	// Set to 1 to see heading debugdraw

//----------------------------------------------------------------------//
//  UMassApplyMovementProcessor
//----------------------------------------------------------------------//

UMassApplyMovementProcessor::UMassApplyMovementProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UMassApplyMovementProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
}

void UMassApplyMovementProcessor::Execute(FMassEntityManager& EntityManager,
													FMassExecutionContext& Context)
{
	// Clamp max delta time to avoid force explosion on large time steps (i.e. during initialization).
	const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

	QUICK_SCOPE_CYCLE_COUNTER(HighRes);

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();

		const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassForceFragment& Force = ForceList[EntityIndex];
			FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
			FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();

			// Update velocity from steering forces.
			Velocity.Value += Force.Value * DeltaTime;

#if WITH_MASSGAMEPLAY_DEBUG
			if (UE::MassMovement::bFreezeMovement)
			{
				Velocity.Value = FVector::ZeroVector;
			}
#endif // WITH_MASSGAMEPLAY_DEBUG

			FVector CurrentLocation = CurrentTransform.GetLocation();
			CurrentLocation += Velocity.Value * DeltaTime;
			CurrentTransform.SetTranslation(CurrentLocation);

		}
	});
}

#undef UNSAFE_FOR_MT
