// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerComponent.h"
#include "MLDeformerAsset.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerModel.h"
#include "MLDeformerInputInfo.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MeshDeformer.h"
#include "Animation/MeshDeformerInstance.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerComponent)

static float GMLDeformerOverrideWeight = -1;
static FAutoConsoleVariableRef CVarMLDeformerOverrideWeight(
	TEXT("MLDeformer.ForceWeight"),
	GMLDeformerOverrideWeight,
	TEXT("Force the Weight for MLDeformer components.")
	TEXT("1 will force completely on, 0 will force completely off.")
	TEXT("Negative values (the default) will use default weights."),
	ECVF_Default
);

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
		ReleaseModelInstance();
		return;
	}

	// Try to initialize the deformer model.
	UMLDeformerModel* Model = DeformerAsset->GetModel();
	if (Model)
	{
		ReleaseModelInstance();
		ModelInstance = Model->CreateModelInstance(this);
		ModelInstance->SetModel(Model);
		ModelInstance->Init(SkelMeshComponent);
		ModelInstance->PostMLDeformerComponentInit();
		BindDelegates();
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
	UnbindDelegates();
	Init();
	BindDelegates();

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

	#if WITH_EDITOR
		TickPerfCounter.Reset();
	#endif
}

void UMLDeformerComponent::BindDelegates()
{
	UMLDeformerModel* Model = DeformerAsset ? DeformerAsset->GetModel() : nullptr;
	if (Model)
	{
		ReinitModelInstanceDelegateHandle = Model->GetReinitModelInstanceDelegate().AddLambda(
			[this]()
			{
				Init();
			});
	}
}

void UMLDeformerComponent::UnbindDelegates()
{
	UMLDeformerModel* Model = DeformerAsset ? DeformerAsset->GetModel() : nullptr;
	if (Model && ReinitModelInstanceDelegateHandle.IsValid())
	{
		Model->GetReinitModelInstanceDelegate().Remove(ReinitModelInstanceDelegateHandle);	
	}

	ReinitModelInstanceDelegateHandle = FDelegateHandle();
}

void UMLDeformerComponent::BeginDestroy()
{
	UnbindDelegates();
	Super::BeginDestroy();
}

void UMLDeformerComponent::ReleaseModelInstance()
{
	if (ModelInstance)
	{
		ModelInstance->ConditionalBeginDestroy(); // Force destruction immediately instead of waiting for the next GC.
		ModelInstance = nullptr;
	}
}

USkeletalMeshComponent* UMLDeformerComponent::FindSkeletalMeshComponent(const UMLDeformerAsset* const Asset) const
{
	USkeletalMeshComponent* ResultingComponent = nullptr;
	if (Asset != nullptr)
	{
		// First search for a skeletal mesh component that uses the same skeletal mesh as the ML Deformer asset was trained on.
		const UMLDeformerModel* Model = Asset ? Asset->GetModel() : nullptr;
		if (Model && Model->GetSkeletalMesh())
		{
			const FSoftObjectPath& ModelSkeletalMesh = Model->GetInputInfo()->GetSkeletalMesh();

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
					if (FSoftObjectPath(ComponentSkeletalMesh) == ModelSkeletalMesh)
					{
						ResultingComponent = Component;
						break;
					}
				}
			}
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
	Super::Activate(bReset);
}

void UMLDeformerComponent::Deactivate()
{
	#if WITH_EDITOR
		TickPerfCounter.Reset();
	#endif

	UnbindDelegates();
	if (ModelInstance)
	{
		ModelInstance->ConditionalBeginDestroy();
		ModelInstance = nullptr;
	}
	Super::Deactivate();
}

void UMLDeformerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	#if WITH_EDITOR
		TickPerfCounter.BeginSample();
	#endif
	SCOPE_CYCLE_COUNTER(STAT_MLDeformerInference);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (TickType != ELevelTick::LEVELTICK_PauseTick)
	{
		if (ModelInstance &&
			SkelMeshComponent && 
			SkelMeshComponent->GetPredictedLODLevel() == 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerComponent::TickComponent)
			float ApplyWeight = Weight;
			if (GMLDeformerOverrideWeight >= 0.0f)
			{
				ApplyWeight = FMath::Min(GMLDeformerOverrideWeight, 1.0f);
			}
			ModelInstance->Tick(DeltaTime, ApplyWeight);

			#if WITH_EDITOR
				// Update our memory usage if desired.
				// This can be pretty slow.
				UMLDeformerModel* Model = ModelInstance->GetModel();
				if (Model->IsMemUsageInvalidated())
				{
					Model->UpdateMemoryUsage();
				}
			#endif
		}
	}

	#if WITH_EDITOR
		TickPerfCounter.EndSample();
	#endif
}

void UMLDeformerComponent::SetWeightInternal(const float NormalizedWeightValue)
{ 
	Weight = FMath::Clamp<float>(NormalizedWeightValue, 0.0f, 1.0f);
}

void UMLDeformerComponent::SetDeformerAssetInternal(UMLDeformerAsset* const InDeformerAsset)
{ 
	DeformerAsset = InDeformerAsset;
	UpdateSkeletalMeshComponent();
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
