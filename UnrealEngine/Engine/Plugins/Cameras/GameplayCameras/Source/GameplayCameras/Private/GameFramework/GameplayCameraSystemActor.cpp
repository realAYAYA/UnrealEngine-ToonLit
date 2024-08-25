// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/GameplayCameraSystemActor.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameplayCameraSystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayCameraSystemActor)

#define LOCTEXT_NAMESPACE "GameplayCameraSystemActor"

AGameplayCameraSystemActor::AGameplayCameraSystemActor(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
	CameraSystemComponent = CreateDefaultSubobject<UGameplayCameraSystemComponent>(TEXT("CameraSystemComponent"));
	RootComponent = CameraSystemComponent;
}

void AGameplayCameraSystemActor::BeginPlay()
{
	Super::BeginPlay();

	if (AutoActivateForPlayer != EAutoReceiveInput::Disabled && GetNetMode() != NM_Client)
	{
		const int32 PlayerIndex = AutoActivateForPlayer.GetIntValue() - 1;
		ActivateForPlayer(PlayerIndex);
	}
}

void AGameplayCameraSystemActor::BecomeViewTarget(APlayerController* PC)
{
	Super::BecomeViewTarget(PC);

	CameraSystemComponent->OnBecomeViewTarget();
}

void AGameplayCameraSystemActor::CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult)
{
	CameraSystemComponent->GetCameraView(DeltaTime, OutResult);
}

void AGameplayCameraSystemActor::EndViewTarget(APlayerController* PC)
{
	CameraSystemComponent->OnEndViewTarget();

	Super::EndViewTarget(PC);
}

void AGameplayCameraSystemActor::ActivateForPlayer(int32 PlayerIndex)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex);
	if (PC)
	{
		PC->SetViewTarget(this);
	}
}

#undef LOCTEXT_NAMESPACE

