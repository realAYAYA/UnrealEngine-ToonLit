// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/GameplayCameraSystemComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Logging/MessageLog.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayCameraSystemComponent)

#define LOCTEXT_NAMESPACE "GameplayCameraSystemComponent"

UGameplayCameraSystemComponent::UGameplayCameraSystemComponent(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
#if WITH_EDITORONLY_DATA
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;

	if (GIsEditor && !IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCameraMesh(
				TEXT("/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam"));
		PreviewMesh = EditorCameraMesh.Object;
	}
#endif  // WITH_EDITORONLY_DATA

	Evaluator = ObjectInit.CreateDefaultSubobject<UCameraSystemEvaluator>(this, "CameraSystemEvaluator");
}

void UGameplayCameraSystemComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	FCameraSystemEvaluationUpdateParams UpdateParams;
	UpdateParams.DeltaTime = DeltaTime;
	Evaluator->Update(UpdateParams);

	Evaluator->GetEvaluatedCameraView(DesiredView);
}

void UGameplayCameraSystemComponent::OnRegister()
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
#endif	// WITH_EDITORONLY_DATA

	Super::OnRegister();
}

void UGameplayCameraSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UGameplayCameraSystemComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

#if WITH_EDITORONLY_DATA
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->DestroyComponent();
	}
#endif  // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR

bool UGameplayCameraSystemComponent::GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut)
{
	const bool bIsCameraSystemActive = IsActive();
	if (bIsCameraSystemActive)
	{
		GetCameraView(DeltaTime, ViewOut);
	}
	return bIsCameraSystemActive;
}

#endif  // WITH_EDITOR

void UGameplayCameraSystemComponent::OnBecomeViewTarget()
{
}

void UGameplayCameraSystemComponent::OnEndViewTarget()
{
}

#undef LOCTEXT_NAMESPACE

