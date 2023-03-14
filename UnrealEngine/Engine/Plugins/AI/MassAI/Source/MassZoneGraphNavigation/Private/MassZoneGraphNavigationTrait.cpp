// Copyright Epic Games, Inc. All Rights Reserved.
#include "MassZoneGraphNavigationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "Engine/World.h"
#include "MassEntityUtils.h"


void UMassZoneGraphNavigationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassVelocityFragment>();
	BuildContext.RequireFragment<FMassMoveTargetFragment>();

	BuildContext.AddFragment<FMassZoneGraphLaneLocationFragment>();
	BuildContext.AddFragment<FMassZoneGraphPathRequestFragment>();
	BuildContext.AddFragment<FMassZoneGraphShortPathFragment>();
	BuildContext.AddFragment<FMassZoneGraphCachedLaneFragment>();

	const FConstSharedStruct ZGMovementParamsFragment = EntityManager.GetOrCreateConstSharedFragment(NavigationParameters);
	BuildContext.AddConstSharedFragment(ZGMovementParamsFragment);
}
