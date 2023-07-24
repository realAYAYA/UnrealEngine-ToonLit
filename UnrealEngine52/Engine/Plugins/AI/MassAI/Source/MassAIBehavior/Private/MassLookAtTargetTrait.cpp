// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLookAtTargetTrait.h"
#include "MassLookAtFragments.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"

void UMassLookAtTargetTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FMassLookAtTargetTag>();
	BuildContext.AddFragment<FTransformFragment>();
}
