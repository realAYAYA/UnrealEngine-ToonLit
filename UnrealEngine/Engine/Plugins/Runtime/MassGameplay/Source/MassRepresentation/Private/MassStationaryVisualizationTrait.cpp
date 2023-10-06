// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStationaryVisualizationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassInstancedStaticMeshComponent.h"


UMassStationaryVisualizationTrait::UMassStationaryVisualizationTrait(const FObjectInitializer& ObjectInitializer)
{
	bAllowServerSideVisualization = true;
}

void UMassStationaryVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	for (FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : StaticMeshInstanceDesc.Meshes)
	{
		MeshDesc.Mobility = EComponentMobility::Stationary;
		MeshDesc.ISMComponentClass = UMassInstancedStaticMeshComponent::StaticClass();
	}

	Super::BuildTemplate(BuildContext, World);

	BuildContext.AddTag<FMassStaticRepresentationTag>();
}

#if WITH_EDITOR
void UMassStationaryVisualizationTrait::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName StaticMeshInstanceDescName = GET_MEMBER_NAME_CHECKED(UMassStationaryVisualizationTrait, StaticMeshInstanceDesc);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == StaticMeshInstanceDescName)
	{
		for (FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : StaticMeshInstanceDesc.Meshes)
		{
			MeshDesc.Mobility = EComponentMobility::Stationary;
			MeshDesc.ISMComponentClass = UMassInstancedStaticMeshComponent::StaticClass();
		}
	}
}
#endif // WITH_EDITOR
