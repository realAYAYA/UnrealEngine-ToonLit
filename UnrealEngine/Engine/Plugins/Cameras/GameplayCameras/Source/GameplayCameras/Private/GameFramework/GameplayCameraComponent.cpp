// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/GameplayCameraComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Core/CameraSystemEvaluator.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameplayCameraSystemActor.h"
#include "GameFramework/GameplayCameraSystemComponent.h"
#include "Logging/MessageLog.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayCameraComponent)

#define LOCTEXT_NAMESPACE "GameplayCameraComponent"

UGameplayCameraComponent::UGameplayCameraComponent(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
	PrimaryComponentTick.bCanEverTick = true;

#if WITH_EDITORONLY_DATA
	if (GIsEditor && !IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(
				TEXT("/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam"));
		PreviewMesh = EditorCameraMesh.Object;
	}
#endif  // WITH_EDITORONLY_DATA
}

void UGameplayCameraComponent::ActivateCamera(int32 PlayerIndex)
{
	UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return;
	}

	PlayerIndex = FMath::Max(0, PlayerIndex);
	if (ensure(PlayerIndex < World->GetNumPlayerControllers()))
	{
		FConstPlayerControllerIterator It = World->GetPlayerControllerIterator();
		It += PlayerIndex;
		if (APlayerController* PlayerController = It->Get())
		{
			ActivateCamera(PlayerController);
		}
	}
}

void UGameplayCameraComponent::ActivateCamera(APlayerController* PlayerController)
{
	if (!ensure(PlayerController && PlayerController->PlayerCameraManager))
	{
		return;
	}
	
	AGameplayCameraSystemActor* CameraSystem = Cast<AGameplayCameraSystemActor>(PlayerController->PlayerCameraManager->GetViewTarget());
	if (!ensure(CameraSystem))
	{
		return;
	}

	if (EvaluationContext == nullptr)
	{
		EvaluationContext = NewObject<UGameplayCameraComponentEvaluationContext>(this, TEXT("EvaluationContext"));
		EvaluationContext->Initialize(this);
	}

	UCameraSystemEvaluator* Evaluator = CameraSystem->GetCameraSystemComponent()->GetCameraSystemEvaluator();
	Evaluator->PushEvaluationContext(EvaluationContext);

	Activate();
}

void UGameplayCameraComponent::OnRegister()
{
#if WITH_EDITORONLY_DATA
	if (PreviewMesh && !PreviewMeshComponent)
	{
		PreviewMeshComponent = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transactional | RF_TextExportTransient);
		PreviewMeshComponent->SetupAttachment(this);
		PreviewMeshComponent->SetIsVisualizationComponent(true);
		PreviewMeshComponent->SetStaticMesh(PreviewMesh);
		PreviewMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		PreviewMeshComponent->bHiddenInGame = true;
		PreviewMeshComponent->CastShadow = false;
		PreviewMeshComponent->CreationMethod = CreationMethod;
		PreviewMeshComponent->RegisterComponentWithWorld(GetWorld());
	}

	UpdatePreviewMeshTransform();
#endif	// WITH_EDITORONLY_DATA

	Super::OnRegister();
}

void UGameplayCameraComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (EvaluationContext)
	{
		EvaluationContext->Update(this);
	}
}

void UGameplayCameraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

#if WITH_EDITORONLY_DATA
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->DestroyComponent();
	}
#endif  // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA

void UGameplayCameraComponent::UpdatePreviewMeshTransform()
{
	if (PreviewMeshComponent)
	{
		// CineCam mesh is wrong, adjust like UCineCameraComponent
		PreviewMeshComponent->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		PreviewMeshComponent->SetRelativeLocation(FVector(-46.f, 0, -24.f));
		PreviewMeshComponent->SetRelativeScale3D(FVector::OneVector);
	}
}

#endif

UGameplayCameraComponentEvaluationContext::UGameplayCameraComponentEvaluationContext(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
}

void UGameplayCameraComponentEvaluationContext::Initialize(UGameplayCameraComponent* Owner)
{
	CameraAsset = Owner->Camera;
}

void UGameplayCameraComponentEvaluationContext::Update(UGameplayCameraComponent* Owner)
{
	const FTransform& OwnerTransform = Owner->GetComponentTransform();
	InitialResult.CameraPose.SetTransform(OwnerTransform);
	InitialResult.bIsValid = true;
}

#undef LOCTEXT_NAMESPACE

