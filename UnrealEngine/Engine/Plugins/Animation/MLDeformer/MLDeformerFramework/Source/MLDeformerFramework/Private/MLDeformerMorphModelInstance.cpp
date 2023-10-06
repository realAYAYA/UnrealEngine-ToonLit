// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerMorphModel.h" 
#include "MLDeformerComponent.h" 
#include "Components/ExternalMorphSet.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerMorphModelInstance)

TAtomic<int32> UMLDeformerMorphModelInstance::NextFreeMorphSetID(0);

void UMLDeformerMorphModelInstance::Init(USkeletalMeshComponent* SkelMeshComponent)
{
	Super::Init(SkelMeshComponent);

	// Generate a unique ID for our morph target set.
	ExternalMorphSetID = NextFreeMorphSetID++;
}

bool UMLDeformerMorphModelInstance::IsValidForDataProvider() const
{
	return true;
}

int32 UMLDeformerMorphModelInstance::GetExternalMorphSetID() const
{ 
	return ExternalMorphSetID;
}

void UMLDeformerMorphModelInstance::BeginDestroy()
{
	// Try to unregister the morph target and morph target set.
	if (SkeletalMeshComponent)
	{
		const UMLDeformerMorphModel* MorphModel = Cast<UMLDeformerMorphModel>(Model);
		if (MorphModel)
		{
			const int32 LOD = 0;
			SkeletalMeshComponent->RemoveExternalMorphSet(LOD, ExternalMorphSetID);
			SkeletalMeshComponent->RefreshExternalMorphTargetWeights();
		}
	}

	Super::BeginDestroy();
}

void UMLDeformerMorphModelInstance::PostMLDeformerComponentInit()
{
	if (HasPostInitialized())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerMorphModelInstance::PostMLDeformerComponentInit)

	Super::PostMLDeformerComponentInit();

	// Register the external morph targets buffer to the skinned mesh component.
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (SkelMeshComponent && SkelMeshComponent->GetSkeletalMeshAsset())
	{	
		// Get the morph model and its morph target set.
		UMLDeformerMorphModel* MorphModel = Cast<UMLDeformerMorphModel>(Model);
		check(MorphModel);
		TSharedPtr<FExternalMorphSet> MorphTargetSet = MorphModel->GetMorphTargetSet();

		// Register the morph set. This overwrites the existing one for this model, if it already exists.
		// Only add to LOD 0 for now.
		const int32 LOD = 0;
		SkelMeshComponent->AddExternalMorphSet(LOD, ExternalMorphSetID, MorphTargetSet);

		// Update the weight information in the Skeletal Mesh.
		SkelMeshComponent->RefreshExternalMorphTargetWeights();

		// When we're in editor mode, keep the CPU data around, so we can re-initialize when needed.
#if WITH_EDITOR
		MorphTargetSet->MorphBuffers.SetEmptyMorphCPUDataOnInitRHI(false);
#else
		MorphTargetSet->MorphBuffers.SetEmptyMorphCPUDataOnInitRHI(true);
#endif

		// Release the render resources, but only in an editor build.
		// The non-editor build shouldn't do this, as then it can't initialize again. The non-editor build assumes
		// that the data doesn't change and we don't need to re-init.
		// In the editor build we have to re-initialize the render resources as the morph targets can change after (re)training, so
		// that is why we release them here, and intialize them again after.
		FMorphTargetVertexInfoBuffers& MorphBuffers = MorphTargetSet->MorphBuffers;
#if WITH_EDITOR
		BeginReleaseResource(&MorphBuffers);
#endif

		// Reinitialize the GPU compressed buffers.
		if (MorphBuffers.IsMorphCPUDataValid() && MorphBuffers.GetNumMorphs() > 0)
		{
			// In a non-editor build this will clear the CPU data.
			// That also means it can't re-init the resources later on again.
			BeginInitResource(&MorphBuffers);
		}

		SetHasPostInitialized(true);
	}
}

void UMLDeformerMorphModelInstance::Tick(float DeltaTime, float ModelWeight)
{
	// Detect changes in quality level.
	const int32 CurrentQualityLevel = GetMLDeformerComponent()->GetQualityLevel();
	if (CurrentQualityLevel != LastQualityLevel)
	{
		LastQualityLevel = CurrentQualityLevel;
		MorphLerpAlpha = 0.0f;

		const int LOD = 0;	// For now we only support LOD 0, as we can't setup an ML Deformer per LOD yet.
		FExternalMorphSetWeights* WeightData = FindWeightData(LOD);
		if (WeightData)
		{
			StartMorphWeights = WeightData->Weights;
		}
	}

	if (StartMorphWeights.IsEmpty())
	{
		const UMLDeformerMorphModel* MorphModel = Cast<UMLDeformerMorphModel>(Model);
		StartMorphWeights.SetNumZeroed(MorphModel->GetNumMorphTargets());
	}

	Super::Tick(DeltaTime, ModelWeight);

	// Update the morph lerp towards the target.
	MorphLerpAlpha += DeltaTime * 10.0f;
	if (MorphLerpAlpha > 1.0f)
	{
		MorphLerpAlpha = 1.0f;
	}
}

FExternalMorphSetWeights* UMLDeformerMorphModelInstance::FindWeightData(int32 LOD) const
{
	// Check if our LOD index is valid first, as we might not have registered yet.
	USkeletalMeshComponent* SkelMeshComponent = SkeletalMeshComponent.Get();
	if (SkelMeshComponent == nullptr || !SkelMeshComponent->IsValidExternalMorphSetLODIndex(LOD))
	{
		return nullptr;
	}

	// Grab the weight data for this morph set.
	// This could potentially fail if we are applying this deformer to the wrong skeletal mesh component.
	return SkelMeshComponent->GetExternalMorphWeights(LOD).MorphSets.Find(ExternalMorphSetID);
}

void UMLDeformerMorphModelInstance::HandleZeroModelWeight()
{
	const int LOD = 0;	// For now we only support LOD 0, as we can't setup an ML Deformer per LOD yet.
	FExternalMorphSetWeights* WeightData = FindWeightData(LOD);
	if (WeightData)
	{
		WeightData->ZeroWeights();
	}
}
