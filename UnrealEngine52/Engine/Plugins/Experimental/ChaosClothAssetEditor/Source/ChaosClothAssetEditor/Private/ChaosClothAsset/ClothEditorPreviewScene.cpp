// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorPreviewScene.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "AssetEditorModeManager.h"
#include "Elements/Framework/EngineElementsLibrary.h"

#define LOCTEXT_NAMESPACE "UChaosClothEditorPreviewScene"

void UChaosClothPreviewSceneDescription::SetPreviewScene(FChaosClothPreviewScene* InPreviewScene)
{
	PreviewScene = InPreviewScene;
}

void UChaosClothPreviewSceneDescription::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PreviewScene)
	{
		PreviewScene->SceneDescriptionPropertyChanged(PropertyChangedEvent);
	}
}



FChaosClothPreviewScene::FChaosClothPreviewScene(FPreviewScene::ConstructionValues ConstructionValues) :
	FAdvancedPreviewScene(ConstructionValues)
{
	PreviewSceneDescription = NewObject<UChaosClothPreviewSceneDescription>();
	PreviewSceneDescription->SetPreviewScene(this);

	if (PreviewSceneDescription->SkeletalMeshAsset)
	{
		CreateSkeletalMeshActor();
	}
}

FChaosClothPreviewScene::~FChaosClothPreviewScene()
{
	if (SkeletalMeshActor)
	{
		if (SkeletalMeshActor->GetSkeletalMeshComponent())
		{
			SkeletalMeshActor->GetSkeletalMeshComponent()->TransformUpdated.RemoveAll(this);
			SkeletalMeshActor->GetSkeletalMeshComponent()->SelectionOverrideDelegate.Unbind();
		}

		SkeletalMeshActor->UnregisterAllComponents();
	}

	if (ClothComponent)
	{
		ClothComponent->SelectionOverrideDelegate.Unbind();
		ClothComponent->UnregisterComponent();
	}
}

void FChaosClothPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(PreviewSceneDescription);
	Collector.AddReferencedObject(ClothComponent);
	Collector.AddReferencedObject(SkeletalMeshActor);
	Collector.AddReferencedObject(ClothActor);
}

void FChaosClothPreviewScene::SceneDescriptionPropertyChanged(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UChaosClothPreviewSceneDescription, SkeletalMeshAsset))
	{
		if (PreviewSceneDescription->SkeletalMeshAsset)
		{
			CreateSkeletalMeshActor();
		}
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UChaosClothPreviewSceneDescription, SkeletalMeshTransform))
	{
		if (SkeletalMeshActor)
		{
			SkeletalMeshActor->GetSkeletalMeshComponent()->SetComponentToWorld(PreviewSceneDescription->SkeletalMeshTransform);
		}
	}
}

void FChaosClothPreviewScene::SetModeManager(TSharedPtr<FAssetEditorModeManager> InClothPreviewEditorModeManager)
{
	ClothPreviewEditorModeManager = InClothPreviewEditorModeManager;
}

void FChaosClothPreviewScene::SkeletalMeshTransformChanged(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	ensure(UpdatedComponent == SkeletalMeshActor->GetSkeletalMeshComponent());
	PreviewSceneDescription->SkeletalMeshTransform = UpdatedComponent->GetComponentToWorld();
}


void FChaosClothPreviewScene::CreateSkeletalMeshActor()
{
	if (SkeletalMeshActor)
	{
		if (SkeletalMeshActor->GetSkeletalMeshComponent())
		{
			SkeletalMeshActor->GetSkeletalMeshComponent()->TransformUpdated.RemoveAll(this);
			SkeletalMeshActor->GetSkeletalMeshComponent()->SelectionOverrideDelegate.Unbind();
		}

		SkeletalMeshActor->UnregisterAllComponents();
	}

	ensure(PreviewSceneDescription->SkeletalMeshAsset);

	SkeletalMeshActor = GetWorld()->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform::Identity);
	if (SkeletalMeshActor)
	{
		SkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMeshAsset(PreviewSceneDescription->SkeletalMeshAsset);
	}

	SkeletalMeshActor->RegisterAllComponents();

	SkeletalMeshActor->GetSkeletalMeshComponent()->TransformUpdated.AddRaw(this, &FChaosClothPreviewScene::SkeletalMeshTransformChanged);
	SkeletalMeshActor->GetSkeletalMeshComponent()->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FChaosClothPreviewScene::IsComponentSelected);
}

bool FChaosClothPreviewScene::IsComponentSelected(const UPrimitiveComponent* InComponent)
{
	if (const UTypedElementSelectionSet* const TypedElementSelectionSet = ClothPreviewEditorModeManager->GetEditorSelectionSet())
	{
		if (const FTypedElementHandle ComponentElement = UEngineElementsLibrary::AcquireEditorComponentElementHandle(InComponent))
		{
			const bool bElementSelected = TypedElementSelectionSet->IsElementSelected(ComponentElement, FTypedElementIsSelectedOptions());
			return bElementSelected;
		}
	}

	return false;
}

void FChaosClothPreviewScene::CreateClothActor(UChaosClothAsset* Asset)
{
	if (ClothComponent)
	{
		ClothComponent->SelectionOverrideDelegate.Unbind();
	}

	if (ClothActor)
	{
		ClothActor->UnregisterAllComponents();
	}

	ClothActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());

	check(ClothActor);

	ClothComponent = NewObject<UChaosClothComponent>(ClothActor);
	ClothComponent->SetClothAsset(Asset);

	ClothComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FChaosClothPreviewScene::IsComponentSelected);

	ClothActor->SetRootComponent(ClothComponent);
	ClothActor->RegisterAllComponents();
}


#undef LOCTEXT_NAMESPACE

