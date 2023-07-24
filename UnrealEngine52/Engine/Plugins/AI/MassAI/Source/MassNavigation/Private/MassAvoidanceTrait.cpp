// Copyright Epic Games, Inc. All Rights Reserved.

#include "Avoidance/MassAvoidanceTrait.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "Engine/World.h"
#include "MassEntityUtils.h"


void UMassObstacleAvoidanceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.AddFragment<FMassNavigationEdgesFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassVelocityFragment>();
	BuildContext.RequireFragment<FMassForceFragment>();
	BuildContext.RequireFragment<FMassMoveTargetFragment>();

	const FMassMovingAvoidanceParameters MovingValidated = MovingParameters.GetValidated();
	const FConstSharedStruct MovingFragment = EntityManager.GetOrCreateConstSharedFragment(MovingValidated);
	BuildContext.AddConstSharedFragment(MovingFragment);

	const FMassStandingAvoidanceParameters StandingValidated = StandingParameters.GetValidated();
	const FConstSharedStruct StandingFragment = EntityManager.GetOrCreateConstSharedFragment(StandingValidated);
	BuildContext.AddConstSharedFragment(StandingFragment);
}
