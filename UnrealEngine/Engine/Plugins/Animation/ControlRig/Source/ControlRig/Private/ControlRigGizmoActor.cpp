// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigGizmoActor.h"
#include "ControlRig.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigGizmoActor)

AControlRigShapeActor::AControlRigShapeActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ControlRigIndex(INDEX_NONE)
	, ControlRig(nullptr)
	, ControlName(NAME_None)
	, ShapeName(NAME_None)
	, OverrideColor(0, 0, 0, 0)
	, bSelected(false)
	, bHovered(false)
{

	ActorRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent0"));
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent0"));
	StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	StaticMeshComponent->Mobility = EComponentMobility::Movable;
	StaticMeshComponent->SetGenerateOverlapEvents(false);
	StaticMeshComponent->bUseDefaultCollision = false;
#if WITH_EDITORONLY_DATA
	StaticMeshComponent->HitProxyPriority = HPP_Wireframe;
#endif

	RootComponent = ActorRootComponent;
	StaticMeshComponent->SetupAttachment(RootComponent);
	StaticMeshComponent->bCastStaticShadow = false;
	StaticMeshComponent->bCastDynamicShadow = false;
	StaticMeshComponent->bSelectable = true;
}

void AControlRigShapeActor::SetSelected(bool bInSelected)
{
	if(!IsSelectable() && bInSelected)
	{
		return;
	}
	if(bSelected != bInSelected)
	{
		bSelected = bInSelected;
		FEditorScriptExecutionGuard Guard;
		OnSelectionChanged(bSelected);
	}
}

bool AControlRigShapeActor::IsSelectedInEditor() const
{
	return bSelected;
}

bool AControlRigShapeActor::IsSelectable() const
{
	return StaticMeshComponent->bSelectable;
}

void AControlRigShapeActor::SetSelectable(bool bInSelectable)
{
	if (StaticMeshComponent->bSelectable != bInSelectable)
	{
		StaticMeshComponent->bSelectable = bInSelectable;
		if (!StaticMeshComponent->bSelectable)
		{
			SetSelected(false);
		}

		FEditorScriptExecutionGuard Guard;
		OnEnabledChanged(bInSelectable);
	}
}

void AControlRigShapeActor::SetHovered(bool bInHovered)
{
	bool bOldHovered = bHovered;

	bHovered = bInHovered;

	if(bHovered != bOldHovered)
	{
		FEditorScriptExecutionGuard Guard;
		OnHoveredChanged(bHovered);
	}
}

bool AControlRigShapeActor::IsHovered() const
{
	return bHovered;
}


void AControlRigShapeActor::SetShapeColor(const FLinearColor& InColor)
{
	if (StaticMeshComponent && !ColorParameterName.IsNone())
	{
		if (UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(StaticMeshComponent->GetMaterial(0)))
		{
			MaterialInstance->SetVectorParameterValue(ColorParameterName, FVector(InColor));
		}
	}
}

bool AControlRigShapeActor::UpdateControlSettings(
	ERigHierarchyNotification InNotif,
	UControlRig* InControlRig,
	const FRigControlElement* InControlElement,
	bool bHideManipulators,
	bool bIsInLevelEditor)
{
	check(InControlElement);

	const FRigControlSettings& ControlSettings = InControlElement->Settings;
	
	// if this actor is not supposed to exist
	if(!ControlSettings.SupportsShape())
	{
		return false;
	}

	const bool bShapeNameUpdated = ShapeName != ControlSettings.ShapeName;
	bool bShapeTransformChanged = InNotif == ERigHierarchyNotification::ControlShapeTransformChanged;
	const bool bLookupShape = bShapeNameUpdated || bShapeTransformChanged;
	
	FTransform MeshTransform = FTransform::Identity;

	// update the shape used for the control
	if(bLookupShape)
	{
		const TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries = InControlRig->GetShapeLibraries();
		if (const FControlRigShapeDefinition* ShapeDef = UControlRigShapeLibrary::GetShapeByName(ControlSettings.ShapeName, ShapeLibraries))
		{
			MeshTransform = ShapeDef->Transform;

			if(bShapeNameUpdated)
			{
				if(ShapeDef->StaticMesh.IsValid())
				{
					if(UStaticMesh* StaticMesh = ShapeDef->StaticMesh.Get())
					{
						if(StaticMesh != StaticMeshComponent->GetStaticMesh())
						{
							StaticMeshComponent->SetStaticMesh(StaticMesh);
							bShapeTransformChanged = true;
						}
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
		}
	}

	// update the shape transform
	if(bShapeTransformChanged)
	{
		const FTransform ShapeTransform = InControlElement->Shape.Get(ERigTransformType::CurrentLocal);
		StaticMeshComponent->SetRelativeTransform(MeshTransform * ShapeTransform);
	}
	
	// update the shape color
	SetShapeColor(ControlSettings.ShapeColor);

	return true;
}

// FControlRigShapeHelper START

namespace FControlRigShapeHelper
{
	FActorSpawnParameters GetDefaultSpawnParameter()
	{
		FActorSpawnParameters ActorSpawnParameters;
#if WITH_EDITOR
		ActorSpawnParameters.bTemporaryEditorActor = true;
		ActorSpawnParameters.bHideFromSceneOutliner = true;
#endif
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActorSpawnParameters.ObjectFlags = RF_Transient;
		return ActorSpawnParameters;
	}

	// create shape from custom staticmesh, may deprecate this unless we come up with better usage
	AControlRigShapeActor* CreateShapeActor(UWorld* InWorld, UStaticMesh* InStaticMesh, const FControlShapeActorCreationParam& CreationParam)
	{
		if (InWorld)
		{
			AControlRigShapeActor* ShapeActor = CreateDefaultShapeActor(InWorld, CreationParam);

			if (ShapeActor)
			{
				if (InStaticMesh)
				{
					ShapeActor->StaticMeshComponent->SetStaticMesh(InStaticMesh);
				}

				return ShapeActor;
			}
		}

		return nullptr;
	}

	AControlRigShapeActor* CreateShapeActor(UWorld* InWorld, TSubclassOf<AControlRigShapeActor> InClass, const FControlShapeActorCreationParam& CreationParam)
	{
		AControlRigShapeActor* ShapeActor = InWorld->SpawnActor<AControlRigShapeActor>(InClass, GetDefaultSpawnParameter());
		if (ShapeActor)
		{
			// set transform
			ShapeActor->SetActorTransform(CreationParam.SpawnTransform);
			return ShapeActor;
		}

		return nullptr;
	}

	AControlRigShapeActor* CreateDefaultShapeActor(UWorld* InWorld, const FControlShapeActorCreationParam& CreationParam)
	{
		AControlRigShapeActor* ShapeActor = InWorld->SpawnActor<AControlRigShapeActor>(AControlRigShapeActor::StaticClass(), GetDefaultSpawnParameter());
		if (ShapeActor)
		{
			ShapeActor->ControlRigIndex = CreationParam.ControlRigIndex;
			ShapeActor->ControlRig = CreationParam.ControlRig;
			ShapeActor->ControlName = CreationParam.ControlName;
			ShapeActor->ShapeName = CreationParam.ShapeName;
			ShapeActor->SetSelectable(CreationParam.bSelectable);
			ShapeActor->SetActorTransform(CreationParam.SpawnTransform);
#if WITH_EDITOR
			ShapeActor->SetActorLabel(CreationParam.ControlName.ToString(), false);
#endif // WITH_EDITOR

			UStaticMeshComponent* MeshComponent = ShapeActor->StaticMeshComponent;

			if (!CreationParam.StaticMesh.IsValid())
			{
				CreationParam.StaticMesh.LoadSynchronous();
			}
			if (CreationParam.StaticMesh.IsValid())
			{
				MeshComponent->SetStaticMesh(CreationParam.StaticMesh.Get());
				MeshComponent->SetRelativeTransform(CreationParam.MeshTransform * CreationParam.ShapeTransform);
			}

			if (!CreationParam.Material.IsValid())
			{
				CreationParam.Material.LoadSynchronous();
			}
			if (CreationParam.StaticMesh.IsValid())
			{
				ShapeActor->ColorParameterName = CreationParam.ColorParameterName;
				UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(CreationParam.Material.Get(), ShapeActor);
				MaterialInstance->SetVectorParameterValue(CreationParam.ColorParameterName, FVector(CreationParam.Color));
				MeshComponent->SetMaterial(0, MaterialInstance);
			}
			return ShapeActor;
		}

		return nullptr;
	}
}

void AControlRigShapeActor::SetGlobalTransform(const FTransform& InTransform)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeTransform(InTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FTransform AControlRigShapeActor::GetGlobalTransform() const
{
	if (RootComponent)
	{
		return RootComponent->GetRelativeTransform();
	}

	return FTransform::Identity;
}

// FControlRigShapeHelper END

