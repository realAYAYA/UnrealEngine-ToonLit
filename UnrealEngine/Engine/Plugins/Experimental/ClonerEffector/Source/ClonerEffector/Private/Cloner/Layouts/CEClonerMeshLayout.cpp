// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/CEClonerMeshLayout.h"

#include "Cloner/CEClonerActor.h"
#include "Cloner/CEClonerComponent.h"
#include "DataInterface/NiagaraDataInterfaceActorComponent.h"
#include "DataInterface/NiagaraDataInterfaceStaticMesh.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraSystem.h"

void UCEClonerMeshLayout::SetCount(int32 InCount)
{
	if (Count == InCount)
	{
		return;
	}

	Count = InCount;
	UpdateLayoutParameters();
}

void UCEClonerMeshLayout::SetAsset(ECEClonerMeshAsset InAsset)
{
	if (Asset == InAsset)
	{
		return;
	}

	Asset = InAsset;
	UpdateLayoutParameters();
}

void UCEClonerMeshLayout::SetSampleData(ECEClonerMeshSampleData InSampleData)
{
	if (SampleData == InSampleData)
	{
		return;
	}

	SampleData = InSampleData;
	UpdateLayoutParameters();
}

void UCEClonerMeshLayout::SetSampleActorWeak(const TWeakObjectPtr<AActor>& InSampleActor)
{
	if (SampleActorWeak == InSampleActor)
	{
		return;
	}

	SampleActorWeak = InSampleActor;
	UpdateLayoutParameters();
}

void UCEClonerMeshLayout::SetSampleActor(AActor* InActor)
{
	SetSampleActorWeak(InActor);
}

#if WITH_EDITOR
const TCEPropertyChangeDispatcher<UCEClonerMeshLayout> UCEClonerMeshLayout::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UCEClonerMeshLayout, Count), &UCEClonerMeshLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerMeshLayout, Asset), &UCEClonerMeshLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerMeshLayout, SampleData), &UCEClonerMeshLayout::OnLayoutPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerMeshLayout, SampleActorWeak), &UCEClonerMeshLayout::OnLayoutPropertyChanged },
};

void UCEClonerMeshLayout::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void UCEClonerMeshLayout::OnLayoutInactive()
{
	Super::OnLayoutInactive();

	// unbind
	if (USceneComponent* SceneComponent = SceneComponentWeak.Get())
	{
		SceneComponent->TransformUpdated.RemoveAll(this);
	}
}

void UCEClonerMeshLayout::OnLayoutParametersChanged(UCEClonerComponent* InComponent)
{
	Super::OnLayoutParametersChanged(InComponent);

	InComponent->SetIntParameter(TEXT("SampleMeshCount"), Count);

	FNiagaraUserRedirectionParameterStore& ExposedParameters = InComponent->GetAsset()->GetExposedParameters();

	static const FNiagaraVariable SampleMeshAssetVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerMeshAsset>()), TEXT("SampleMeshAsset"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(Asset), SampleMeshAssetVar);

	static const FNiagaraVariable SampleMeshDataVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerMeshSampleData>()), TEXT("SampleMeshData"));
	ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SampleData), SampleMeshDataVar);

	static const FNiagaraVariable SampleMeshActorVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceActorComponent::StaticClass()), TEXT("SampleMeshActor"));
	UNiagaraDataInterfaceActorComponent* ActorMeshDI = Cast<UNiagaraDataInterfaceActorComponent>(ExposedParameters.GetDataInterface(SampleMeshActorVar));

	// unbind
	if (USceneComponent* SceneComponent = SceneComponentWeak.Get())
	{
		SceneComponent->TransformUpdated.RemoveAll(this);
	}
	SceneComponentWeak = nullptr;

	// bind
	AActor* SampleActor = SampleActorWeak.Get();
	if (SampleActor && SampleActor->GetRootComponent())
	{
		ActorMeshDI->SourceActor = SampleActor;
		SceneComponentWeak = SampleActor->GetRootComponent();
		SceneComponentWeak->TransformUpdated.AddUObject(this, &UCEClonerMeshLayout::OnSampleMeshTransformed);
	}
	else
	{
		SampleActorWeak.Reset();
		SceneComponentWeak.Reset();
	}

	if (Asset == ECEClonerMeshAsset::StaticMesh)
	{
		static const FNiagaraVariable SampleMeshStaticVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceStaticMesh::StaticClass()), TEXT("SampleMeshStatic"));
		UNiagaraDataInterfaceStaticMesh* StaticMeshDI = Cast<UNiagaraDataInterfaceStaticMesh>(ExposedParameters.GetDataInterface(SampleMeshStaticVar));
		if (SampleActorWeak.IsValid())
		{
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SampleActorWeak->GetComponentByClass(UStaticMeshComponent::StaticClass())))
			{
				StaticMeshDI->SetSourceComponentFromBlueprints(StaticMeshComponent);
			}
		}
	}

	if (Asset == ECEClonerMeshAsset::SkeletalMesh)
	{
		static const FNiagaraVariable SampleMeshSkeletalVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceSkeletalMesh::StaticClass()), TEXT("SampleMeshSkeletal"));
		UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshDI = Cast<UNiagaraDataInterfaceSkeletalMesh>(ExposedParameters.GetDataInterface(SampleMeshSkeletalVar));
		if (SampleActorWeak.IsValid())
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(SampleActorWeak->GetComponentByClass(USkeletalMeshComponent::StaticClass())))
			{
				SkeletalMeshDI->SetSourceComponentFromBlueprints(SkeletalMeshComponent);
			}
		}
	}
}

void UCEClonerMeshLayout::OnSampleMeshTransformed(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType)
{
	UpdateLayoutParameters();
}
