// Copyright Epic Games, Inc. All Rights Reserved.
#include "SmoothOrientation/MassSmoothOrientationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "Engine/World.h"
#include "MassEntityUtils.h"


void UMassSmoothOrientationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FMassMoveTargetFragment>();
	BuildContext.RequireFragment<FMassVelocityFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	const FConstSharedStruct OrientationFragment = EntityManager.GetOrCreateConstSharedFragment(Orientation);
	BuildContext.AddConstSharedFragment(OrientationFragment);
}
