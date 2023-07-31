// Copyright Epic Games, Inc. All Rights Reserved.
#include "Avoidance/MassNavigationObstacleTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassNavigationFragments.h"
#include "MassCommonFragments.h"

void UMassNavigationObstacleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FAgentRadiusFragment>();

	BuildContext.AddFragment<FMassNavigationObstacleGridCellLocationFragment>();
}
