// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassNavigationProcessors.h"
#include "MassCommonUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationSubsystem.h"
#include "MassSimulationLOD.h"
#include "MassMovementTypes.h"
#include "MassMovementFragments.h"
#include "MassEntityView.h"
#include "Engine/World.h"


#define UNSAFE_FOR_MT 0
#define MOVEMENT_DEBUGDRAW 0	// Set to 1 to see heading debugdraw

//----------------------------------------------------------------------//
//  UMassOffLODNavigationProcessor
//----------------------------------------------------------------------//

UMassOffLODNavigationProcessor::UMassOffLODNavigationProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance); // @todo: remove this direct dependency
}

void UMassOffLODNavigationProcessor::ConfigureQueries()
{
	EntityQuery_Conditional.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddChunkRequirement<FMassSimulationVariableTickChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery_Conditional.SetChunkFilter(&FMassSimulationVariableTickChunkFragment::ShouldTickChunkThisFrame);
}

void UMassOffLODNavigationProcessor::Execute(FMassEntityManager& EntityManager,
													FMassExecutionContext& Context)
{
	EntityQuery_Conditional.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
#if WITH_MASSGAMEPLAY_DEBUG
		if (UE::MassMovement::bFreezeMovement)
		{
			return;
		}
#endif // WITH_MASSGAMEPLAY_DEBUG
		const int32 NumEntities = Context.GetNumEntities();

		const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];

			// Snap position to move target directly
			CurrentTransform.SetLocation(MoveTarget.Center);
		}
	});
}


//----------------------------------------------------------------------//
//  UMassNavigationSmoothHeightProcessor
//----------------------------------------------------------------------//

UMassNavigationSmoothHeightProcessor::UMassNavigationSmoothHeightProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void UMassNavigationSmoothHeightProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
}

void UMassNavigationSmoothHeightProcessor::Execute(FMassEntityManager& EntityManager,
													FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
#if WITH_MASSGAMEPLAY_DEBUG
		if (UE::MassMovement::bFreezeMovement)
		{
			return;
		}
#endif // WITH_MASSGAMEPLAY_DEBUG
		const int32 NumEntities = Context.GetNumEntities();
		const float DeltaTime = Context.GetDeltaTimeSeconds();

		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
		const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FTransform& CurrentTransform = LocationList[EntityIndex].GetMutableTransform();
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];

			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move || MoveTarget.GetCurrentAction() == EMassMovementAction::Stand)
			{
				// Set height smoothly to follow current move targets height.
				FVector CurrentLocation = CurrentTransform.GetLocation();
				FMath::ExponentialSmoothingApprox(CurrentLocation.Z, MoveTarget.Center.Z, DeltaTime, MovementParams.HeightSmoothingTime);
				CurrentTransform.SetLocation(CurrentLocation);
			}
		}
	});
}

//----------------------------------------------------------------------//
//  UMassMoveTargetFragmentInitializer
//----------------------------------------------------------------------//

UMassMoveTargetFragmentInitializer::UMassMoveTargetFragmentInitializer()
	: InitializerQuery(*this)
{
	ObservedType = FMassMoveTargetFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
}

void UMassMoveTargetFragmentInitializer::ConfigureQueries()
{
	InitializerQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	InitializerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UMassMoveTargetFragmentInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	InitializerQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];
			const FTransformFragment& Location = LocationList[EntityIndex];

			MoveTarget.Center = Location.GetTransform().GetLocation();
			MoveTarget.Forward = Location.GetTransform().GetRotation().Vector();
			MoveTarget.DistanceToGoal = 0.0f;
			MoveTarget.SlackRadius = 0.0f;
		}
	});
}


//----------------------------------------------------------------------//
//  UMassNavigationObstacleGridProcessor
//----------------------------------------------------------------------//
UMassNavigationObstacleGridProcessor::UMassNavigationObstacleGridProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void UMassNavigationObstacleGridProcessor::ConfigureQueries()
{
	FMassEntityQuery BaseEntityQuery;
	BaseEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BaseEntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	BaseEntityQuery.AddRequirement<FMassNavigationObstacleGridCellLocationFragment>(EMassFragmentAccess::ReadWrite);
	BaseEntityQuery.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadWrite);

	AddToGridEntityQuery = BaseEntityQuery;
	AddToGridEntityQuery.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	AddToGridEntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	AddToGridEntityQuery.AddTagRequirement<FMassInNavigationObstacleGridTag>(EMassFragmentPresence::None);
	AddToGridEntityQuery.RegisterWithProcessor(*this);

	UpdateGridEntityQuery = BaseEntityQuery;
	UpdateGridEntityQuery.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	UpdateGridEntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	UpdateGridEntityQuery.AddTagRequirement<FMassInNavigationObstacleGridTag>(EMassFragmentPresence::All);
	UpdateGridEntityQuery.RegisterWithProcessor(*this);

	RemoveFromGridEntityQuery = BaseEntityQuery;
	RemoveFromGridEntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::All);
	RemoveFromGridEntityQuery.AddTagRequirement<FMassInNavigationObstacleGridTag>(EMassFragmentPresence::All);
	RemoveFromGridEntityQuery.RegisterWithProcessor(*this);
}

void UMassNavigationObstacleGridProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// can't be ParallelFor due to MovementSubsystem->GetGridMutable().Move not being thread-safe
	AddToGridEntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		FNavigationObstacleHashGrid2D& HashGrid = Context.GetMutableSubsystemChecked<UMassNavigationSubsystem>(World).GetObstacleGridMutable();
		const int32 NumEntities = Context.GetNumEntities();

		TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		TConstArrayView<FAgentRadiusFragment> RadiiList = Context.GetFragmentView<FAgentRadiusFragment>();
		TArrayView<FMassNavigationObstacleGridCellLocationFragment> NavigationObstacleCellLocationList = Context.GetMutableFragmentView<FMassNavigationObstacleGridCellLocationFragment>();
		const bool bHasColliderData = Context.GetFragmentView<FMassAvoidanceColliderFragment>().Num() > 0;

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			// Add to the grid
			const FVector NewPos = LocationList[EntityIndex].GetTransform().GetLocation();
			const float Radius = RadiiList[EntityIndex].Radius;

			FMassNavigationObstacleItem ObstacleItem;
			ObstacleItem.Entity = Context.GetEntity(EntityIndex);
			ObstacleItem.ItemFlags |= bHasColliderData ? EMassNavigationObstacleFlags::HasColliderData : EMassNavigationObstacleFlags::None;
			
			const FBox NewBounds(NewPos - FVector(Radius, Radius, 0.f), NewPos + FVector(Radius, Radius, 0.f));
			NavigationObstacleCellLocationList[EntityIndex].CellLoc = HashGrid.Add(ObstacleItem, NewBounds);

			Context.Defer().AddTag<FMassInNavigationObstacleGridTag>(ObstacleItem.Entity);
		}
	});

	UpdateGridEntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		FNavigationObstacleHashGrid2D& HashGrid = Context.GetMutableSubsystemChecked<UMassNavigationSubsystem>(World).GetObstacleGridMutable();
		const int32 NumEntities = Context.GetNumEntities();

		TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		TConstArrayView<FAgentRadiusFragment> RadiiList = Context.GetFragmentView<FAgentRadiusFragment>();
		TArrayView<FMassNavigationObstacleGridCellLocationFragment> NavigationObstacleCellLocationList = Context.GetMutableFragmentView<FMassNavigationObstacleGridCellLocationFragment>();
		const bool bHasColliderData = Context.GetFragmentView<FMassAvoidanceColliderFragment>().Num() > 0;

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			// Update position in grid
			const FVector NewPos = LocationList[EntityIndex].GetTransform().GetLocation();
			const float Radius = RadiiList[EntityIndex].Radius;
			FMassNavigationObstacleItem ObstacleItem;
			ObstacleItem.Entity = Context.GetEntity(EntityIndex);
			ObstacleItem.ItemFlags |= bHasColliderData ? EMassNavigationObstacleFlags::HasColliderData : EMassNavigationObstacleFlags::None;

			const FBox NewBounds(NewPos - FVector(Radius, Radius, 0.f), NewPos + FVector(Radius, Radius, 0.f));
			NavigationObstacleCellLocationList[EntityIndex].CellLoc = HashGrid.Move(ObstacleItem, NavigationObstacleCellLocationList[EntityIndex].CellLoc, NewBounds);

#if WITH_MASSGAMEPLAY_DEBUG && 0
			const FDebugContext BaseDebugContext(this, LogAvoidance, nullptr, ObstacleItem.Entity);
			if (DebugIsSelected(ObstacleItem.Entity))
			{
				FBox Box = MovementSubsystem->GetGridMutable().CalcCellBounds(AvoidanceObstacleCellLocationList[EntityIndex].CellLoc);
				Box.Max.Z += 200.f;
				DebugDrawBox(BaseDebugContext, Box, FColor::Yellow);
			}
#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});

	RemoveFromGridEntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		FNavigationObstacleHashGrid2D& HashGrid = Context.GetMutableSubsystemChecked<UMassNavigationSubsystem>(World).GetObstacleGridMutable();
		const int32 NumEntities = Context.GetNumEntities();

		TArrayView<FMassNavigationObstacleGridCellLocationFragment> AvoidanceObstacleCellLocationList = Context.GetMutableFragmentView<FMassNavigationObstacleGridCellLocationFragment>();
		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassNavigationObstacleItem ObstacleItem;
			ObstacleItem.Entity = Context.GetEntity(EntityIndex);
			HashGrid.Remove(ObstacleItem, AvoidanceObstacleCellLocationList[EntityIndex].CellLoc);
			AvoidanceObstacleCellLocationList[EntityIndex].CellLoc = FNavigationObstacleHashGrid2D::FCellLocation();

			Context.Defer().RemoveTag<FMassInNavigationObstacleGridTag>(ObstacleItem.Entity);
		}
	});
}

//----------------------------------------------------------------------//
//  UMassNavigationObstacleRemoverProcessor
//----------------------------------------------------------------------//
UMassNavigationObstacleRemoverProcessor::UMassNavigationObstacleRemoverProcessor()
	: EntityQuery(*this)
{
	ObservedType = FMassNavigationObstacleGridCellLocationFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
}

void UMassNavigationObstacleRemoverProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassNavigationObstacleGridCellLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassNavigationSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassNavigationObstacleRemoverProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, World = EntityManager.GetWorld()](FMassExecutionContext& Context)
	{
		FNavigationObstacleHashGrid2D& HashGrid = Context.GetMutableSubsystemChecked<UMassNavigationSubsystem>(World).GetObstacleGridMutable();
		const int32 NumEntities = Context.GetNumEntities();
		const TArrayView<FMassNavigationObstacleGridCellLocationFragment> AvoidanceObstacleCellLocationList = Context.GetMutableFragmentView<FMassNavigationObstacleGridCellLocationFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassNavigationObstacleItem ObstacleItem;
			ObstacleItem.Entity = Context.GetEntity(i);
			HashGrid.Remove(ObstacleItem, AvoidanceObstacleCellLocationList[i].CellLoc);
		}
	});
}

#undef UNSAFE_FOR_MT
