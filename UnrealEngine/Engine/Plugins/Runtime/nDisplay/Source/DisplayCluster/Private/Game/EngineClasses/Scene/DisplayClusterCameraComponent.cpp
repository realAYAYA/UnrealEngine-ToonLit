// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterCameraComponent.h"
#include "Components/BillboardComponent.h"

#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

#include "DisplayClusterConfigurationTypes.h"


UDisplayClusterCameraComponent::UDisplayClusterCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, bEnableGizmo(true)
	, BaseGizmoScale(0.5f, 0.5f, 0.5f)
	, GizmoScaleMultiplier(1.f)
#endif
	, InterpupillaryDistance(6.4f)
	, bSwapEyes(false)
	, StereoOffset(EDisplayClusterEyeStereoOffset::None)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTextureObject = TEXT("/nDisplay/Icons/S_nDisplayViewOrigin");
		SpriteTexture = SpriteTextureObject.Get();
	}
#endif
}

#if WITH_EDITOR
void UDisplayClusterCameraComponent::SetVisualizationScale(float Scale)
{
	GizmoScaleMultiplier = Scale;
	RefreshVisualRepresentation();
}

void UDisplayClusterCameraComponent::SetVisualizationEnabled(bool bEnabled)
{
	bEnableGizmo = bEnabled;
	RefreshVisualRepresentation();
}
#endif

void UDisplayClusterCameraComponent::OnRegister()
{
#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		if (SpriteComponent == nullptr)
		{
			SpriteComponent = NewObject<UBillboardComponent>(this, NAME_None, RF_Transactional | RF_TextExportTransient);
			if (SpriteComponent)
			{
				SpriteComponent->SetupAttachment(this);
				SpriteComponent->SetIsVisualizationComponent(true);
				SpriteComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
				SpriteComponent->SetMobility(EComponentMobility::Movable);
				SpriteComponent->Sprite = SpriteTexture;
				SpriteComponent->SpriteInfo.Category = TEXT("NDisplayViewOrigin");
				SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("DisplayClusterCameraComponent", "NDisplayViewOriginSpriteInfo", "nDisplay View Origin");
				SpriteComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				SpriteComponent->bHiddenInGame = true;
				SpriteComponent->bIsScreenSizeScaled = true;
				SpriteComponent->CastShadow = false;
				SpriteComponent->CreationMethod = CreationMethod;
				SpriteComponent->RegisterComponentWithWorld(GetWorld());
			}
		}
	}

	RefreshVisualRepresentation();
#endif

	Super::OnRegister();
}

#if WITH_EDITOR
void UDisplayClusterCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisualRepresentation();
}

void UDisplayClusterCameraComponent::RefreshVisualRepresentation()
{
	// Update the viz component
	if (SpriteComponent)
	{
		SpriteComponent->SetVisibility(bEnableGizmo);
		SpriteComponent->SetWorldScale3D(BaseGizmoScale * GizmoScaleMultiplier);
		// The sprite components don't get updated in real time without forcing render state dirty
		SpriteComponent->MarkRenderStateDirty();
	}
}
#endif
