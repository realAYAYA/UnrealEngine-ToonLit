// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigThumbnailRenderer.h"
#include "ThumbnailHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ControlRig.h"
#include "CanvasTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigThumbnailRenderer)

UControlRigThumbnailRenderer::UControlRigThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RigBlueprint = nullptr;
}

bool UControlRigThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	if (UControlRigBlueprint* InRigBlueprint = Cast<UControlRigBlueprint>(Object))
	{
		if(InRigBlueprint->GetRigModuleIcon())
		{
			return true;
		}
		
		USkeletalMesh* SkeletalMesh = InRigBlueprint->PreviewSkeletalMesh.Get();
		int32 MissingMeshCount = 0;

		for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : InRigBlueprint->ShapeLibraries)
		{
			if (ShapeLibrary.IsValid())
			{
				InRigBlueprint->Hierarchy->ForEach<FRigControlElement>([&](FRigControlElement* ControlElement) -> bool
				{
					if (const FControlRigShapeDefinition* ShapeDef = ShapeLibrary->GetShapeByName(ControlElement->Settings.ShapeName))
					{
						UStaticMesh* StaticMesh = ShapeDef->StaticMesh.Get();
						if (StaticMesh == nullptr) // not yet loaded
						{
							MissingMeshCount++;
						}
					}

					return true; // continue the iteration
				});
			}
		}
		return MissingMeshCount == 0;
	}
	return false;
}

void UControlRigThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	RigBlueprint = nullptr;

	if (UControlRigBlueprint* InRigBlueprint = Cast<UControlRigBlueprint>(Object))
	{
		if(UTexture2D* ModuleIcon = InRigBlueprint->GetRigModuleIcon())
		{
			Canvas->DrawTile(X, Y, Width, Height, 0, 0, 1, 1, FLinearColor::White, ModuleIcon->GetResource(), 1.f);
			return;
		}
		
		UObject* ObjectToDraw = InRigBlueprint->PreviewSkeletalMesh.Get();
		if(ObjectToDraw == nullptr)
		{
			ObjectToDraw = InRigBlueprint;
		}

		RigBlueprint = InRigBlueprint;
		Super::Draw(ObjectToDraw, X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);

		for (auto Pair : ShapeActors)
		{
			if (Pair.Value && Pair.Value->GetOuter())
			{
				Pair.Value->Rename(nullptr, GetTransientPackage());
				Pair.Value->MarkAsGarbage();
			}
		}
		ShapeActors.Reset();
	}
}

void UControlRigThumbnailRenderer::AddAdditionalPreviewSceneContent(UObject* Object, UWorld* PreviewWorld)
{
	TSharedRef<FSkeletalMeshThumbnailScene> ThumbnailScene = ThumbnailSceneCache.EnsureThumbnailScene(Object);
	if (ThumbnailScene->GetPreviewActor() && RigBlueprint && !RigBlueprint->ShapeLibraries.IsEmpty() && RigBlueprint->GeneratedClass)
	{
		if(RigBlueprint->GetRigModuleIcon())
		{
			return;
		}

		UControlRig* ControlRig = nullptr;

		// reuse the current control rig if possible
		UControlRig* CDO = Cast<UControlRig>(RigBlueprint->GeneratedClass->GetDefaultObject(true /* create if needed */));

		TArray<UObject*> ArchetypeInstances;
		CDO->GetArchetypeInstances(ArchetypeInstances);
		for (UObject* ArchetypeInstance : ArchetypeInstances)
		{
			ControlRig = Cast<UControlRig>(ArchetypeInstance);
			break;
		}

		if (ControlRig == nullptr)
		{
			// fall back to the CDO. we only need to pull out
			// the pose of the default hierarchy so the CDO is fine.
			// this case only happens if the editor had been closed
			// and there are no archetype instances left.
			ControlRig = CDO;
		}

		FTransform ComponentToWorld = ThumbnailScene->GetPreviewActor()->GetSkeletalMeshComponent()->GetComponentToWorld();

		if (URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
		{
			Hierarchy->ForEach<FRigControlElement>([&](FRigControlElement* ControlElement) -> bool
			{
				switch (ControlElement->Settings.ControlType)
				{
					case ERigControlType::Float:
					case ERigControlType::ScaleFloat:
					case ERigControlType::Integer:
					case ERigControlType::Vector2D:
					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Rotator:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						if (const FControlRigShapeDefinition* ShapeDef = RigBlueprint->GetControlShapeByName(ControlElement->Settings.ShapeName))
						{
							UStaticMesh* StaticMesh = ShapeDef->StaticMesh.Get();
							if (StaticMesh == nullptr) // not yet loaded
							{
								return true;
							}

							const FTransform ShapeGlobalTransform = ControlRig->GetHierarchy()->GetGlobalControlShapeTransform(ControlElement->GetKey());

							FActorSpawnParameters SpawnInfo;
							SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
							SpawnInfo.bNoFail = true;
							SpawnInfo.ObjectFlags = RF_Transient;
							AStaticMeshActor* ShapeActor = PreviewWorld->SpawnActor<AStaticMeshActor>(SpawnInfo);
							ShapeActor->SetActorEnableCollision(false);

							if (!ShapeDef->Library.IsValid())
							{
								return true;
							}

							UMaterial* DefaultMaterial = ShapeDef->Library.Get()->DefaultMaterial.Get();
							if (DefaultMaterial == nullptr) // not yet loaded
							{
								return true;
							}

							UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(DefaultMaterial, ShapeActor);
							MaterialInstance->SetVectorParameterValue(ShapeDef->Library.Get()->MaterialColorParameter,FVector(ControlElement->Settings.ShapeColor));
							UStaticMeshComponent* MeshComponent = ShapeActor->GetStaticMeshComponent();
							for (int32 i=0; i<MeshComponent->GetNumMaterials(); ++i)
							{
								MeshComponent->SetMaterial(i, MaterialInstance);
							}

							ShapeActors.Add(ControlElement->GetFName(), ShapeActor);

							ShapeActor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
							ShapeActor->SetActorTransform(ShapeDef->Transform * ShapeGlobalTransform);
						}
						break;
					}
					default:
					{
						break;
					}
				}
				return true;
			});
		}
	}
}
