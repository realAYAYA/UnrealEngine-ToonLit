// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/CEClonerSplineLayout.h"

#include "Cloner/CEClonerActor.h"
#include "Cloner/CEClonerComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraDataInterfaceSpline.h"
#include "NiagaraSystem.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

void UCEClonerSplineLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UCEClonerSplineLayout::SetSplineActorWeak(const TWeakObjectPtr<AActor>& InSplineActor)
{
	if (SplineActorWeak == InSplineActor)
	{
		return;
	}

	SplineActorWeak = InSplineActor;
	UpdateLayoutParameters();
}

void UCEClonerSplineLayout::SetSplineActor(AActor* InSplineActor)
{
	SetSplineActorWeak(InSplineActor);
}

void UCEClonerSplineLayout::SetOrientMesh(bool bInOrientMesh)
{
	if (bOrientMesh == bInOrientMesh)
	{
		return;
	}

	bOrientMesh = bInOrientMesh;
	UpdateLayoutParameters();
}

#if WITH_EDITOR
void UCEClonerSplineLayout::SpawnLinkedSplineActor()
{
	const ACEClonerActor* ClonerActor = GetClonerActor();

	if (!ClonerActor)
	{
		return;
	}

	UWorld* ClonerWorld = ClonerActor->GetWorld();

	FActorSpawnParameters Params;
	Params.bTemporaryEditorActor = false;

	AActor* SpawnedSplineActor = ClonerWorld->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (!SpawnedSplineActor)
	{
		return;
	}

	// Construct the new component and attach as needed
	USplineComponent* const NewComponent = NewObject<USplineComponent>(SpawnedSplineActor
		, USplineComponent::StaticClass()
		, MakeUniqueObjectName(SpawnedSplineActor, USplineComponent::StaticClass(), TEXT("SplineComponent"))
		, RF_Transactional);

	SpawnedSplineActor->SetRootComponent(NewComponent);

	// Add to SerializedComponents array so it gets saved
	SpawnedSplineActor->AddInstanceComponent(NewComponent);
	NewComponent->OnComponentCreated();
	NewComponent->RegisterComponent();

	// Rerun construction scripts
	SpawnedSplineActor->RerunConstructionScripts();

	SpawnedSplineActor->SetActorLocation(ClonerActor->GetActorLocation());
	SpawnedSplineActor->SetActorRotation(ClonerActor->GetActorRotation());

	SetSplineActorWeak(SpawnedSplineActor);
	FActorLabelUtilities::RenameExistingActor(SpawnedSplineActor, TEXT("SplineActor"), true);
}

const TCEPropertyChangeDispatcher<UCEClonerSplineLayout> UCEClonerSplineLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UCEClonerSplineLayout, Count), &UCEClonerSplineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerSplineLayout, SplineActorWeak), &UCEClonerSplineLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerSplineLayout, bOrientMesh), &UCEClonerSplineLayout::OnLayoutPropertyChanged },
};

void UCEClonerSplineLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UCEClonerSplineLayout::OnLayoutInactive()
{
	Super::OnLayoutInactive();

	// unbind
	if (USplineComponent* SplineComponent = SplineComponentWeak.Get())
	{
		SplineComponent->TransformUpdated.RemoveAll(this);
		USceneComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
	}
}

void UCEClonerSplineLayout::OnLayoutParametersChanged(UCEClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("SampleSplineCount"), Count);

	const FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();
	static const FNiagaraVariable SampleSplineVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceSpline::StaticClass()), TEXT("SampleSpline"));
	UNiagaraDataInterfaceSpline* SplineDI = Cast<UNiagaraDataInterfaceSpline>(ExposedParameters.GetDataInterface(SampleSplineVar));

	InComponent->SetBoolParameter(TEXT("MeshOrientAxisEnable"), bOrientMesh);

	// unbind
	SplineDI->SoftSourceActor = nullptr;

	if (USplineComponent* SplineComponent = SplineComponentWeak.Get())
    {
		SplineComponent->TransformUpdated.RemoveAll(this);
		SplineComponentWeak.Reset();
    }

	SplineComponentWeak = nullptr;

	// bind
	if (AActor* SplineActor = SplineActorWeak.Get())
	{
		if (USplineComponent* SplineComponent = SplineActor->FindComponentByClass<USplineComponent>())
		{
			SplineDI->SoftSourceActor = SplineActor;
			SplineComponentWeak = SplineComponent;

			SplineComponent->TransformUpdated.RemoveAll(this);
			SplineComponent->TransformUpdated.AddUObject(this, &UCEClonerSplineLayout::OnSampleSplineTransformed);

			USceneComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
			USceneComponent::MarkRenderStateDirtyEvent.AddUObject(this, &UCEClonerSplineLayout::OnSampleSplineRenderStateUpdated);

			if (ACEClonerActor* ClonerActor = GetClonerActor())
			{
				ClonerActor->SetActorTransform(SplineActor->GetActorTransform());
			}
		}
	}
}

void UCEClonerSplineLayout::OnSampleSplineTransformed(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType)
{
	UpdateLayoutParameters();
}

void UCEClonerSplineLayout::OnSampleSplineRenderStateUpdated(UActorComponent& InComponent)
{
	if (SplineActorWeak.IsValid() && InComponent.GetOwner() == SplineActorWeak.Get())
	{
		UpdateLayoutParameters();
	}
}
