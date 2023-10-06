// Copyright Epic Games, Inc. All Rights Reserved.
#include "Movement/MassMovementTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassMovementTypes.h"
#include "Engine/World.h"
#include "MassEntityUtils.h"


void UMassMovementTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FMassForceFragment>();

	const FConstSharedStruct MovementFragment = EntityManager.GetOrCreateConstSharedFragment(Movement);
	BuildContext.AddConstSharedFragment(MovementFragment);
}
