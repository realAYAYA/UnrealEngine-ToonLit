// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassMovableVisualizationTrait.h"
#include "MassEntityTemplateRegistry.h"


void UMassMovableVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	if (World.IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	for (FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : StaticMeshInstanceDesc.Meshes)
	{
		MeshDesc.Mobility = EComponentMobility::Movable;
	}

	Super::BuildTemplate(BuildContext, World);
}
