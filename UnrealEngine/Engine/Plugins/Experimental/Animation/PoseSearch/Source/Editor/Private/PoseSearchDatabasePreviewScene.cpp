// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabasePreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDatabaseViewModel.h"

#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<float> CVarDatabasePreviewDebugDrawSamplerSize(TEXT("a.DatabasePreview.DebugDrawSamplerSize"), 0.f, TEXT("Debug Draw Sampler Positions Size"));
#endif

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

		GetWorld()->Tick(LEVELTICK_All, InDeltaTime);

		FDatabaseViewModel* ViewModel = GetEditor()->GetViewModel();
		const UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();

		if (ViewModel->IsPoseFeaturesDrawMode(EFeaturesDrawMode::All | EFeaturesDrawMode::Detailed) && !ViewModel->GetPreviewActors().IsEmpty() &&
			FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
#if ENABLE_ANIM_DEBUG
			// memory marker required for PreviewActor.Sampler.ExtractPose
			FMemMark Mark(FMemStack::Get());
#endif // ENABLE_ANIM_DEBUG

			bool bDrawQueryVector = ViewModel->ShouldDrawQueryVector();
			for (FDatabasePreviewActor& PreviewActor : ViewModel->GetPreviewActors())
			{
				if (Database->GetSearchIndex().IsValidPoseIndex(PreviewActor.CurrentPoseIndex))
				{
					if (UDebugSkelMeshComponent* Mesh = PreviewActor.GetDebugSkelMeshComponent())
					{
						UE::PoseSearch::FDebugDrawParams DrawParams(GetWorld(), Mesh, PreviewActor.QuantizedTimeRootTransform, Database);
						DrawParams.DrawFeatureVector(PreviewActor.CurrentPoseIndex);

						if (bDrawQueryVector)
						{
							DrawParams.DrawFeatureVector(ViewModel->GetQueryVector());
							bDrawQueryVector = false;
						}

#if ENABLE_ANIM_DEBUG
						const float DebugDrawSamplerSize = CVarDatabasePreviewDebugDrawSamplerSize.GetValueOnAnyThread();
						if (DebugDrawSamplerSize > UE_KINDA_SMALL_NUMBER)
						{
							// drawing the pose extracted from the Sampler to visually compare with the pose features and the mesh drawing
							FCompactPose Pose;
							Pose.SetBoneContainer(&PreviewActor.GetAnimPreviewInstance()->GetRequiredBonesOnAnyThread());
							PreviewActor.Sampler.ExtractPose(PreviewActor.CurrentTime, Pose);

							const FTransform RootTransform = PreviewActor.Sampler.ExtractRootTransform(PreviewActor.CurrentTime);

							FCSPose<FCompactPose> ComponentSpacePose;
							ComponentSpacePose.InitPose(Pose);

							for (int32 BoneIndex = 0; BoneIndex < Pose.GetNumBones(); ++BoneIndex)
							{
								const FTransform BoneWorldTransforms = ComponentSpacePose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)) * RootTransform;
								DrawParams.DrawPoint(BoneWorldTransforms.GetTranslation(), FColor::Red, DebugDrawSamplerSize);
							}
						}
#endif // ENABLE_ANIM_DEBUG
					}
				}
			}
		}
	}
}
