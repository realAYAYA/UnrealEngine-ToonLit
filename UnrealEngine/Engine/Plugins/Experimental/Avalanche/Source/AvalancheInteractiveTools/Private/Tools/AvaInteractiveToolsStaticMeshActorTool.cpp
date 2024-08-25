// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsStaticMeshActorTool.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Builders/AvaInteractiveToolsStaticMeshActorToolBuilder.h"

UAvaInteractiveToolsStaticMeshActorTool::UAvaInteractiveToolsStaticMeshActorTool()
{
	ActorClass = AStaticMeshActor::StaticClass();
}

FAvaInteractiveToolsToolParameters UAvaInteractiveToolsStaticMeshActorTool::GetToolParameters() const
{
	// This is never really needed, it should work regardless. OOP consistency!
	return UAvaInteractiveToolsStaticMeshActorToolBuilder::CreateToolParameters(
		Category,
		Command,
		Identifier,
		Priority,
		StaticMesh,
		GetClass()
	);
}

AActor* UAvaInteractiveToolsStaticMeshActorTool::SpawnActor(TSubclassOf<AActor> InActorClass, EAvaViewportStatus InViewportStatus,
	const FVector2f& InViewportPosition, bool bInPreview, FString* InActorLabelOverride) const
{
	AActor* Actor = Super::SpawnActor(InActorClass, InViewportStatus, InViewportPosition, bInPreview, InActorLabelOverride);

	if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
	{
		StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
	}

	return Actor;
}
