// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerMorphModel.h" 
#include "NeuralNetwork.h"
#include "Components/SkeletalMeshComponent.h"

TAtomic<int32> UMLDeformerMorphModelInstance::NextFreeMorphSetID(0);

void UMLDeformerMorphModelInstance::Init(USkeletalMeshComponent* SkelMeshComponent)
{
	Super::Init(SkelMeshComponent);

	// Generate a unique ID for our morph target set.
	ExternalMorphSetID = NextFreeMorphSetID++;
}

void UMLDeformerMorphModelInstance::Release()
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

	Super::Release();
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

		// Update the weight information in the Skeletal Mesh.
		SkelMeshComponent->RefreshExternalMorphTargetWeights();

		SetHasPostInitialized(true);
	}
}

FExternalMorphSetWeights* UMLDeformerMorphModelInstance::FindWeightData(int32 LOD) const
{
	const UMLDeformerMorphModel* MorphModel = Cast<UMLDeformerMorphModel>(Model);
	if (MorphModel == nullptr || !SkeletalMeshComponent)
	{
		return nullptr;
	}

	// If we haven't got an external morph set registered yet, let's just return a nullptr.
	if (!SkeletalMeshComponent->HasExternalMorphSet(LOD, ExternalMorphSetID))
	{
		return nullptr;
	}

	// Grab the weight data for this morph set.
	// This could potentially fail if we are applying this deformer to the wrong skeletal mesh component.
	return SkeletalMeshComponent->GetExternalMorphWeights(LOD).MorphSets.Find(ExternalMorphSetID);
}

void UMLDeformerMorphModelInstance::HandleZeroModelWeight()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNeuralMorphModelInstance::HandleZeroModelWeight)

	const int LOD = 0;	// For now we only support LOD 0, as we can't setup an ML Deformer per LOD yet.
	FExternalMorphSetWeights* WeightData = FindWeightData(LOD);
	if (WeightData)
	{
		WeightData->ZeroWeights();
	}
}

// Run the neural network, which calculates its outputs, which are the weights of our morph targets.
void UMLDeformerMorphModelInstance::Execute(float ModelWeight)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNeuralMorphModelInstance::Execute)

	// Grab the weight data for this morph set.
	// This could potentially fail if we are applying this deformer to the wrong skeletal mesh component.
	const int LOD = 0;	// For now we only support LOD 0, as we can't setup an ML Deformer per LOD yet.
	FExternalMorphSetWeights* WeightData = FindWeightData(LOD);
	if (WeightData == nullptr)
	{
		return;
	}

	// If our model is active, we want to run the neural network and update the morph weights
	// with the values that the neural net calculated for us.
	const UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	if (NeuralNetwork)
	{
		// Perform the neural network inference, which updates the output tensor.
		// This takes most CPU time inside this method.
		UMLDeformerModelInstance::Execute(ModelWeight);

		// Get the output tensor, read the values and use them as morph target weights inside the skeletal mesh component.
		const FNeuralTensor& OutputTensor = NeuralNetwork->GetOutputTensorForContext(NeuralNetworkInferenceHandle);
		const int32 NumNetworkWeights = OutputTensor.Num();
		const int32 NumMorphTargets = WeightData->Weights.Num();;

		// If we have the expected amount of weights.
		// +1 because we always have an extra morph target that represents the means, with fixed weight of 1.
		// Therefore, the neural network output tensor will contain one less float than the number of morph targets in our morph set.
		if (NumMorphTargets == NumNetworkWeights + 1)
		{
			// Set the first morph target, which represents the means, to a weight of 1.0, as it always needs to be fully active.
			WeightData->Weights[0] = 1.0f * ModelWeight;

			// Update all generated morph target weights with the values calculated by our neural network.
			for (int32 MorphIndex = 0; MorphIndex < NumNetworkWeights; ++MorphIndex)
			{
				const float OutputTensorValue = OutputTensor.At<float>(MorphIndex);
				WeightData->Weights[MorphIndex + 1] = OutputTensorValue * ModelWeight;
			}
		}
	}
	else
	{
		WeightData->ZeroWeights();
	}
}
