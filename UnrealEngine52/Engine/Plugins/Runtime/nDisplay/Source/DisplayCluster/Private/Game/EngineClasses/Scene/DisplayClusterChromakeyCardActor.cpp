// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterChromakeyCardActor.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterChromakeyCardStageActorComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"


ADisplayClusterChromakeyCardActor::ADisplayClusterChromakeyCardActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(LightCardStageActorComponentName))
{
	StageActorComponent = CreateOptionalDefaultSubobject<UDisplayClusterChromakeyCardStageActorComponent>(TEXT("ChromakeyStageActorComponent"));
}

void ADisplayClusterChromakeyCardActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR
	if (GIsEditor)
	{
		UpdateChromakeySettings();
	}
#endif
}

void ADisplayClusterChromakeyCardActor::AddToRootActor(ADisplayClusterRootActor* InRootActor)
{
	Super::AddToRootActor(InRootActor);
	// @todo uv chromakey cards - determine translucent sort order
}

void ADisplayClusterChromakeyCardActor::RemoveFromRootActor()
{
	Super::RemoveFromRootActor();
}

bool ADisplayClusterChromakeyCardActor::IsReferencedByICVFXCamera(const UDisplayClusterICVFXCameraComponent* InCamera) const
{
	check(InCamera);

	if (const UDisplayClusterChromakeyCardStageActorComponent* ChromakeyCardComponent = Cast<UDisplayClusterChromakeyCardStageActorComponent>(StageActorComponent))
	{
		if (ChromakeyCardComponent->IsReferencedByICVFXCamera(InCamera))
		{
			return true;
		}
	}
	
	const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = InCamera->GetCameraSettingsICVFX();
	if (CameraSettings.Chromakey.ChromakeyRenderTexture.ShowOnlyList.Actors.Contains(this))
	{
		return true;
	}

	for (const FName& ThisLayer : Layers)
	{
		if (const FActorLayer* ExistingLayer = CameraSettings.Chromakey.ChromakeyRenderTexture.ShowOnlyList.ActorLayers.FindByPredicate([ThisLayer](const FActorLayer& Layer)
		{
			return Layer.Name == ThisLayer;
		}))
		{
			return true;
		}
	}

	return false;
}

void ADisplayClusterChromakeyCardActor::UpdateChromakeySettings()
{
	if (const ADisplayClusterRootActor* RootActor = GetRootActorOwner())
	{
		int32 Count = 0;
		FLinearColor ChromaColor(ForceInitToZero);
		TArray<UDisplayClusterICVFXCameraComponent*> ICVFXComponents;
		RootActor->GetComponents(ICVFXComponents);

		for (const UDisplayClusterICVFXCameraComponent* Component : ICVFXComponents)
		{
			if (IsReferencedByICVFXCamera(Component))
			{
				ChromaColor += Component->GetCameraSettingsICVFX().Chromakey.ChromakeyColor;
				Count++;
			}
		}

		Color = Count > 0 ? ChromaColor / Count : FLinearColor::Green;
	}
}
