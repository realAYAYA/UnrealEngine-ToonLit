// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimPreviewScene.h"
#include "ContextualAnimAssetEditorToolkit.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"

FContextualAnimPreviewScene::FContextualAnimPreviewScene(ConstructionValues CVS, const TSharedRef<FContextualAnimAssetEditorToolkit>& EditorToolkit)
	: FAdvancedPreviewScene(CVS)
	, EditorToolkitPtr(EditorToolkit)
{
	// Disable killing actors outside of the world
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(true);
	WorldSettings->bEnableWorldBoundsChecks = false;

	// Spawn an owner for FloorMeshComponent so CharacterMovementComponent can detect it as a valid floor and slide along it
	{
		AActor* FloorActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform());
		check(FloorActor);

		static const FString NewName = FString(TEXT("FloorComponent"));
		FloorMeshComponent->Rename(*NewName, FloorActor);

		FloorActor->SetRootComponent(FloorMeshComponent);
	}
}

void FContextualAnimPreviewScene::Tick(float InDeltaTime)
{
	FAdvancedPreviewScene::Tick(InDeltaTime);

	// Trigger Begin Play in this preview world.
	// This is needed for the CharacterMovementComponent to be able to switch to falling mode. See: UCharacterMovementComponent::StartFalling
	if (PreviewWorld && !PreviewWorld->bBegunPlay)
	{
		for (FActorIterator It(PreviewWorld); It; ++It)
		{
			It->DispatchBeginPlay();
		}

		PreviewWorld->bBegunPlay = true;
	}

	if (!GIntraFrameDebuggingGameThread)
	{
		GetWorld()->Tick(LEVELTICK_All, InDeltaTime);
	}
}