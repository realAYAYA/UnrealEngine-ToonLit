// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassZoneGraphAnnotationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Engine/World.h"
#include "MassZoneGraphAnnotationFragments.h"

void UMassZoneGraphAnnotationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassZoneGraphAnnotationFragment>();
	BuildContext.AddChunkFragment<FMassZoneGraphAnnotationVariableTickChunkFragment>();
}
