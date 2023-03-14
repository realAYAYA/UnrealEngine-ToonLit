// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawnMovementComponent)

DEFINE_LOG_CATEGORY_STATIC(LogPawnMovementComponent, Log, All);


//----------------------------------------------------------------------//
// UPawnMovementComponent
//----------------------------------------------------------------------//
void UPawnMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (NewUpdatedComponent)
	{
		if (!ensureMsgf(Cast<APawn>(NewUpdatedComponent->GetOwner()), TEXT("%s must update a component owned by a Pawn"), *GetName()))
		{
			return;
		}
	}

	Super::SetUpdatedComponent(NewUpdatedComponent);

	PawnOwner = UpdatedComponent ? CastChecked<APawn>(UpdatedComponent->GetOwner()) : NULL;
}

void UPawnMovementComponent::Serialize(FArchive& Ar)
{
	APawn* CurrentPawnOwner = PawnOwner;
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		// This was marked Transient so it won't be saved out, but we need still to reject old saved values.
		PawnOwner = CurrentPawnOwner;
	}
}

APawn* UPawnMovementComponent::GetPawnOwner() const
{
	return PawnOwner;
}

bool UPawnMovementComponent::IsMoveInputIgnored() const
{
	if (UpdatedComponent)
	{
		if (PawnOwner)
		{
			return PawnOwner->IsMoveInputIgnored();
		}
	}

	// No UpdatedComponent or Pawn, no movement.
	return true;
}

void UPawnMovementComponent::AddInputVector(FVector WorldAccel, bool bForce /*=false*/)
{
	if (PawnOwner)
	{
		PawnOwner->Internal_AddMovementInput(WorldAccel, bForce);
	}
}

FVector UPawnMovementComponent::GetPendingInputVector() const
{
	return PawnOwner ? PawnOwner->Internal_GetPendingMovementInputVector() : FVector::ZeroVector;
}

FVector UPawnMovementComponent::GetLastInputVector() const
{
	return PawnOwner ? PawnOwner->Internal_GetLastMovementInputVector() : FVector::ZeroVector;
}

FVector UPawnMovementComponent::ConsumeInputVector()
{
	return PawnOwner ? PawnOwner->Internal_ConsumeMovementInputVector() : FVector::ZeroVector;
}

void UPawnMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	if (PawnOwner)
	{
		PawnOwner->Internal_AddMovementInput(MoveInput);
	}
}

void UPawnMovementComponent::OnTeleported()
{
	if (PawnOwner && PawnOwner->IsNetMode(NM_Client) && PawnOwner->IsLocallyControlled())
	{
		MarkForClientCameraUpdate();
	}
}

AController* UPawnMovementComponent::GetController() const
{
	if (PawnOwner)
	{
		return PawnOwner->GetController();
	}

	return nullptr;
}

void UPawnMovementComponent::MarkForClientCameraUpdate()
{

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		APlayerCameraManager* PlayerCameraManager = PlayerController->PlayerCameraManager;
		if (PlayerCameraManager != nullptr && PlayerCameraManager->bUseClientSideCameraUpdates)
		{
			PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
		}
	}
}
