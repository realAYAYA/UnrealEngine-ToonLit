// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWIVisualizationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassLWIRepresentationSubsystem.h"


UMassLWIVisualizationTrait::UMassLWIVisualizationTrait(const FObjectInitializer& ObjectInitializer)
{
	bAllowServerSideVisualization = true;
#if WITH_EDITORONLY_DATA
	bCanModifyRepresentationSubsystemClass = false;
	RepresentationSubsystemClass = UMassLWIRepresentationSubsystem::StaticClass();
#endif // WITH_EDITORONLY_DATA
}
