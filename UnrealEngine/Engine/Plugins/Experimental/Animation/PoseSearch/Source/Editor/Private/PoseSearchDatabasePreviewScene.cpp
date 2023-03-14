// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDatabaseViewModel.h"

#include "PoseSearch/PoseSearch.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"

namespace UE::PoseSearch
{
	FDatabasePreviewScene::FDatabasePreviewScene(
		ConstructionValues CVs,
		const TSharedRef<FDatabaseEditor>& Editor)
		: FAdvancedPreviewScene(CVs)
		, EditorPtr(Editor)
	{
		// Disable killing actors outside of the world
		AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(true);
		WorldSettings->bEnableWorldBoundsChecks = false;

		// Spawn an owner for FloorMeshComponent so CharacterMovementComponent can detect it as a valid floor and slide 
		// along it
		{
			AActor* FloorActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform());
			check(FloorActor);

			static const FString NewName = FString(TEXT("FloorComponent"));
			FloorMeshComponent->Rename(*NewName, FloorActor);

			FloorActor->SetRootComponent(FloorMeshComponent);
		}
	}

	void FDatabasePreviewScene::Tick(float InDeltaTime)
	{
		FAdvancedPreviewScene::Tick(InDeltaTime);

		// Trigger Begin Play in this preview world.
		// This is needed for the CharacterMovementComponent to be able to switch to falling mode. 
		// See: UCharacterMovementComponent::StartFalling
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

		const FDatabaseViewModel* ViewModel = GetEditor()->GetViewModel();
		const UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();

		if (Database && Database->IsValidForSearch() &&
			ViewModel->IsPoseFeaturesDrawMode(EFeaturesDrawMode::All))
		{
			for (const FDatabasePreviewActor& PreviewActor : ViewModel->GetPreviewActors())
			{
				if (Database->IsValidPoseIndex(PreviewActor.CurrentPoseIndex))
				{
					UE::PoseSearch::FDebugDrawParams DrawParams;
					DrawParams.RootTransform = PreviewActor.Mesh->GetComponentTransform();
					DrawParams.Database = Database;
					DrawParams.World = GetWorld();
					DrawParams.DefaultLifeTime = 0.f;
					DrawParams.PointSize = 5.f;
					DrawParams.Mesh = PreviewActor.Mesh;

					EnumAddFlags(DrawParams.Flags, UE::PoseSearch::EDebugDrawFlags::DrawFast);
					DrawFeatureVector(DrawParams, PreviewActor.CurrentPoseIndex);
				}
			}
		}
	}
}
