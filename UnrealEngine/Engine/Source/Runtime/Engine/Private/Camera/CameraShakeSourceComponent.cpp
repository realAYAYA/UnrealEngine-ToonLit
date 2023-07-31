// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraShakeSourceComponent.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "CinematicCameraModule.h"
#include "Components/BillboardComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraShakeSourceComponent)

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif


UCameraShakeSourceComponent::UCameraShakeSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Attenuation(ECameraShakeAttenuation::Quadratic)
	, InnerAttenuationRadius(100.f)
	, OuterAttenuationRadius(1000.f)
	, bAutoStart(false)
{
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;

	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/S_CameraShakeSource"));

		EditorSpriteTexture = StaticTexture.Object;
		EditorSpriteTextureScale = .5f;
	}
#endif
}

void UCameraShakeSourceComponent::OnRegister()
{
	Super::OnRegister();
	UpdateEditorSpriteTexture();
}

void UCameraShakeSourceComponent::UpdateEditorSpriteTexture()
{
#if WITH_EDITORONLY_DATA
	if (SpriteComponent != nullptr)
	{
		SpriteComponent->SetSprite(EditorSpriteTexture);
		SpriteComponent->SetRelativeScale3D(FVector(EditorSpriteTextureScale));
	}
#endif
}

void UCameraShakeSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart)
	{
		Start();
	}
}

void UCameraShakeSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllCameraShakes();

	Super::EndPlay(EndPlayReason);
}

void UCameraShakeSourceComponent::Start()
{
	if (ensureMsgf(CameraShake.Get() != nullptr, TEXT("No camera shake was specified on this source!")))
	{
		StartCameraShake(CameraShake);
	}
}

void UCameraShakeSourceComponent::StartCameraShake(TSubclassOf<UCameraShakeBase> InCameraShake, float Scale, ECameraShakePlaySpace PlaySpace, FRotator UserPlaySpaceRot)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
		{
			PlayerController->PlayerCameraManager->StartCameraShakeFromSource(InCameraShake, this, Scale, PlaySpace, UserPlaySpaceRot);
		}
	}
}

void UCameraShakeSourceComponent::StopAllCameraShakesOfType(TSubclassOf<UCameraShakeBase> InCameraShake, bool bImmediately)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
		{
			PlayerController->PlayerCameraManager->StopAllInstancesOfCameraShakeFromSource(InCameraShake, this, bImmediately);
		}
	}
}

void UCameraShakeSourceComponent::StopAllCameraShakes(bool bImmediately)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
		{
			PlayerController->PlayerCameraManager->StopAllCameraShakesFromSource(this, bImmediately);
		}
	}
}

float UCameraShakeSourceComponent::GetAttenuationFactor(const FVector& Location) const
{
	const FTransform& SourceTransform = GetComponentTransform();
	const FVector LocationToSource = SourceTransform.GetTranslation() - Location;

	float AttFactor = 1.0f;
	switch (Attenuation)
	{
		case ECameraShakeAttenuation::Quadratic:
			AttFactor = 2.0f;
			break;
		default:
			break;
	}

	if (InnerAttenuationRadius < OuterAttenuationRadius)
	{
		float DistFactor = (LocationToSource.Size() - InnerAttenuationRadius) / (OuterAttenuationRadius - InnerAttenuationRadius);
		DistFactor = 1.f - FMath::Clamp(DistFactor, 0.f, 1.f);
		return FMath::Pow(DistFactor, AttFactor);
	}
	else if (OuterAttenuationRadius > 0)
	{
		// Just cut the intensity after the end distance.
		return (LocationToSource.SizeSquared() < FMath::Square(OuterAttenuationRadius)) ? 1.f : 0.f;
	}
	return 1.f;
}

#if WITH_EDITOR

void UCameraShakeSourceComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange != nullptr && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UCameraShakeSourceComponent, CameraShake))
    {
        PreviousCameraShake = CameraShake;
    }

    Super::PreEditChange(PropertyAboutToChange);   
}

void UCameraShakeSourceComponent::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCameraShakeSourceComponent, CameraShake))
    {
        if (CameraShake != nullptr)
        {
            // Single instance shakes can't be localized since they're automatically recycled and merged.
            // If the user is trying to set it, let's notify them and revert to the previous value.
            const UCameraShakeBase* const CameraShakeCDO = GetDefault<UCameraShakeBase>(CameraShake);
            if (CameraShakeCDO != nullptr && CameraShakeCDO->bSingleInstance)
            {
                FText NotificationText = FText::Format(
                        NSLOCTEXT("CameraShakeSourceComponent", "SingleInstanceCameraShakeNotAllowed", "{0} is a single instance shake, and therefore can't be localized to a source actor."),
                        FText::FromName(CameraShake->GetFName()));
                FNotificationInfo NotificationInfo(NotificationText);
                NotificationInfo.ExpireDuration = 5.f;
                FSlateNotificationManager::Get().AddNotification(NotificationInfo);

                CameraShake = PreviousCameraShake;
            }
        }
    }
}

#endif

