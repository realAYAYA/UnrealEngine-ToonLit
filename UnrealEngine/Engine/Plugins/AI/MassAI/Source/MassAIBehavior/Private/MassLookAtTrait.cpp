// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLookAtTrait.h"
#include "MassLookAtFragments.h"
#include "MassEntityTemplateRegistry.h"

void UMassLookAtTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassLookAtFragment>();
	BuildContext.AddFragment<FMassLookAtTrajectoryFragment>();
}
