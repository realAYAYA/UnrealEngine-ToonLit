// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVGJoinedShapesActor.h"
#include "ProceduralMeshes/JoinedSVGDynamicMeshComponent.h"

ASVGJoinedShapesActor::ASVGJoinedShapesActor()
{
	JointDynamicMeshComponent = CreateDefaultSubobject<UJoinedSVGDynamicMeshComponent>("JointDynamicMeshComponent");
	SetRootComponent(JointDynamicMeshComponent);
}

TArray<UDynamicMeshComponent*> ASVGJoinedShapesActor::GetSVGDynamicMeshes()
{
	return { GetDynamicMeshComponent() };
}

