// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerComponent.h"
#include "MLDeformerAsset.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerModel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MeshDeformer.h"
#include "Animation/MeshDeformerInstance.h"

UMLDeformerComponent::UMLDeformerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickInEditor = true;
	bAutoActivate = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;
}

void UMLDeformerComponent::Init()
{
	// If there is no deformer asset linked, release what we currently have.
	if (DeformerAsset == nullptr)
	{
		if (ModelInstance)
		{
			ModelInstance->Release();
			ModelInstance = nullptr;
		}
		return;
	}

	// Try to initialize the deformer model.
	UMLDeformerModel* Model = DeformerAsset->GetModel();
	if (Model)
	{
		if (ModelInstance)
		{
			ModelInstance->Release();
			ModelInstance = nullptr;
		}
		ModelInstance = Model->CreateModelInstance(this);
		ModelInstance->SetModel(Model);
		ModelInstance->Init(SkelMeshComponent);
		ModelInstance->PostMLDeformerComponentInit();
	}
	else
	{
		ModelInstance = nullptr;
		UE_LOG(LogMLDeformer, Warning, TEXT("ML Deformer component on '%s' has a deformer asset that has no ML model setup."), *GetOuter()->GetName());
	}
}

void UMLDeformerComponent::SetupComponent(UMLDeformerAsset* InDeformerAsset, USkeletalMeshComponent* InSkelMeshComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerComponent::SetupComponent)

	RenderCommandFence.BeginFence();
	RenderCommandFence.Wait();

	// Remove the existing skeletal mesh component as tick prerequisite.
	if (SkelMeshComponent)
	{
		RemoveTickPrerequisiteComponent(SkelMeshComponent);
	}

	// Add the new one.
	if (InSkelMeshComponent)
	{		
		AddTickPrerequisiteComponent(InSkelMeshComponent);
	}

	DeformerAsset = InDeformerAsset;
	SkelMeshComponent = InSkelMeshComponent;

	// Initialize and make sure we have a model instance.
	RemoveNeuralNetworkModifyDelegate();
	Init();
	AddNeuralNetworkModifyDelegate();

	// Verify that a mesh deformer has been setup when the used ML model requires one.
	if (!bSuppressMeshDeformerLogWarnings && DeformerAsset && ModelInstance && ModelInstance->GetModel() && SkelMeshComponent)
	{
		const FString DefaultGraph = ModelInstance->GetModel()->GetDefaultDeformerGraphAssetPath();
		if (!DefaultGraph.IsEmpty() && !SkelMeshComponent->HasMeshDeformer())
		{
			UE_LOG(LogMLDeformer,
				Warning, 
				TEXT("ML Deformer Asset '%s' used with skel mesh component '%s' on actor '%s' uses model type '%s', which requires a deformer graph (default='%s'). " \
					"No Mesh Deformer has been set on the mesh or skel mesh component, so linear skinning will be used."),
					*DeformerAsset->GetName(),
					*SkelMeshComponent->GetName(),
					*SkelMeshComponent->GetOuter()->GetName(),
					*ModelInstance->GetModel()->GetDisplayName(),
					*ModelInstance->GetModel()->GetDefaultDeformerGraphAssetPath()
			);
		}
	}
}

void UMLDeformerComponent::AddNeuralNetworkModifyDelegate()
{
	if (DeformerAsset == nullptr)
	{
		return;
	}

	UMLDeformerModel* Model = DeformerAsset->GetModel();
	if (Model)
	{
		NeuralNetworkModifyDelegateHandle = Model->GetNeuralNetworkModifyDelegate().AddLambda
		(
			([this]()
			{
				Init();
			})
		);
	}
}

void UMLDeformerComponent::RemoveNeuralNetworkModifyDelegate()
{
	if (DeformerAsset && 
		NeuralNetworkModifyDelegateHandle != FDelegateHandle() && 
		DeformerAsset->GetModel())
	{
		DeformerAsset->GetModel()->GetNeuralNetworkModifyDelegate().Remove(NeuralNetworkModifyDelegateHandle);
	}
	
	NeuralNetworkModifyDelegateHandle = FDelegateHandle();
}

void UMLDeformerComponent::BeginDestroy()
{
	RemoveNeuralNetworkModifyDelegate();
	Super::BeginDestroy();
}

USkeletalMeshComponent* UMLDeformerComponent::FindSkeletalMeshComponent(UMLDeformerAsset* Asset)
{
	USkeletalMeshComponent* ResultingComponent = nullptr;
	if (Asset != nullptr)
	{
		// First search for a skeletal mesh component that uses the same skeletal mesh as the ML Deformer asset was trained on.
		const UMLDeformerModel* Model = Asset ? Asset->GetModel() : nullptr;
		if (Model && Model->GetSkeletalMesh())
		{
			const USkeletalMesh* ModelSkeletalMesh = Model->GetSkeletalMesh();

			// Get a list of all skeletal mesh components on the actor.
			TArray<USkeletalMeshComponent*> Components;
			AActor* Actor = Cast<AActor>(GetOuter());
			if (Actor)
			{
				Actor->GetComponents<USkeletalMeshComponent>(Components);
		
				// Find a component that uses a mesh with the same vertex count.
				for (USkeletalMeshComponent* Component : Components)
				{
					const USkeletalMesh* ComponentSkeletalMesh = Component->GetSkeletalMeshAsset();
					if (ComponentSkeletalMesh == ModelSkeletalMesh)
					{
						ResultingComponent = Component;
						break;
					}
				}
			}
		}
	}

	if (ResultingComponent == nullptr)
	{
		// Fall back to the first skeletal mesh component.
		const AActor* Actor = Cast<AActor>(GetOuter());
		if (Actor)
		{
			ResultingComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
		}
	}

	return ResultingComponent;
}

void UMLDeformerComponent::UpdateSkeletalMeshComponent()
{
	SkelMeshComponent = FindSkeletalMeshComponent(DeformerAsset.Get());
	SetupComponent(DeformerAsset, SkelMeshComponent);
}

void UMLDeformerComponent::Activate(bool bReset)
{
	UpdateSkeletalMeshComponent();
	SetupComponent(DeformerAsset, SkelMeshComponent);
	Super::Activate(bReset);
}

void UMLDeformerComponent::Deactivate()
{
	RemoveNeuralNetworkModifyDelegate();
	if (ModelInstance)
	{
		ModelInstance->Release();
		ModelInstance = nullptr;
	}
	Super::Deactivate();
}

void UMLDeformerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (TickType != ELevelTick::LEVELTICK_PauseTick)
	{
		if (ModelInstance &&
			SkelMeshComponent && 
			SkelMeshComponent->GetPredictedLODLevel() == 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerComponent::TickComponent)
			ModelInstance->Tick(DeltaTime, Weight);
		}
	}
}

#if WITH_EDITOR
	void UMLDeformerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMLDeformerComponent, DeformerAsset))
		{
			SetDeformerAsset(DeformerAsset);
		}
	}
#endif
