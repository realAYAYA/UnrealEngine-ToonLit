// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStationaryVisualizationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "VisualLogger/VisualLogger.h"


UMassStationaryVisualizationTrait::UMassStationaryVisualizationTrait(const FObjectInitializer& ObjectInitializer)
{
	bAllowServerSideVisualization = true;
}

void UMassStationaryVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	bool bIssuesFound = false;
	for (FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : StaticMeshInstanceDesc.Meshes)
	{
		bIssuesFound = bIssuesFound || (MeshDesc.Mobility != EComponentMobility::Stationary);
		MeshDesc.Mobility = EComponentMobility::Stationary;
	}
	UE_CVLOG_UELOG(bIssuesFound, this, LogMass, Log, TEXT("%s some Meshes' mobility has been set to non-Stationary. Theese settings will be overridden. "), *GetPathName());

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
		}
	}
}
#endif // WITH_EDITOR
