// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelInstance.h"
#include "NearestNeighborModel.h"
#include "NearestNeighborModelInputInfo.h"
#include "NeuralNetwork.h"
#include "NearestNeighborOptimizedNetwork.h"
#include "MLDeformerAsset.h"
#include "Components/ExternalMorphSet.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NearestNeighborModelInstance)
CSV_DECLARE_CATEGORY_MODULE_EXTERN(MLDEFORMERFRAMEWORK_API, MLDeformer);

using namespace UE::MLDeformer;
using namespace UE::NearestNeighborModel;

int64 UNearestNeighborModelInstance::SetBoneTransforms(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex)
{
	// Get the transforms for the bones we used during training.
	// These are in the space relative to their parent.
	UpdateBoneTransforms();

    const UNearestNeighborModelInputInfo* InputInfo = static_cast<UNearestNeighborModelInputInfo*>(Model->GetInputInfo());
    const int32 NumBones = InputInfo->GetNumBones();

    // Extract BoneRotations from TrainingBoneTransforms (@todo: optimize)
    TArray<FQuat> BoneRotations; BoneRotations.AddZeroed(NumBones);
    int64 EndIndex = StartIndex + NumBones * 3;
	check(EndIndex <= OutputBufferSize); // Make sure we don't write past the OutputBuffer.
    
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
        BoneRotations[BoneIndex] = TrainingBoneTransforms[BoneIndex].GetRotation();
	}

    // Write the transforms into the output buffer.
    InputInfo->ComputeNetworkInput(BoneRotations, OutputBuffer, StartIndex);

	return EndIndex;
}

bool UNearestNeighborModelInstance::SetupInputs()
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	if (NearestNeighborModel && NearestNeighborModel->DoesUseOptimizedNetwork())
	{
		if (NearestNeighborModel == nullptr ||
			SkeletalMeshComponent == nullptr ||
			SkeletalMeshComponent->GetSkeletalMeshAsset() == nullptr ||
			!bIsCompatible)
		{
			return false;
		}

		const UNearestNeighborOptimizedNetwork* OptimizedNetwork = NearestNeighborModel->GetOptimizedNetwork();
		if (!OptimizedNetwork || !OptimizedNetworkInstance)
		{
			return false;
		}

		int32 NumNeuralNetInputs = 0;
		float* InputDataPointer = nullptr;
		GetInputDataPointer(InputDataPointer, NumNeuralNetInputs);

		// If the neural network expects a different number of inputs, do nothing.
		const int32 NumFloatsPerBone = NearestNeighborModel->GetNumFloatsPerBone();
		const int32 NumFloatsPerCurve = NearestNeighborModel->GetNumFloatsPerCurve();
		const int64 NumDeformerAssetInputs = NearestNeighborModel->GetInputInfo()->CalcNumNeuralNetInputs(NumFloatsPerBone, NumFloatsPerCurve);
		if (NumNeuralNetInputs != NumDeformerAssetInputs)
		{
			return false;
		}

		const int64 NumFloatsWritten = SetNeuralNetworkInputValues(InputDataPointer, NumNeuralNetInputs);
		check(NumFloatsWritten == NumNeuralNetInputs);
	}
	else
	{
		const bool bSuccess = Super::SetupInputs();
		if (!bSuccess)
		{
			return false;
		}
	}

	float* InputDataPointer = nullptr;
	int32 NumNeuralNetInputs = 0;
	GetInputDataPointer(InputDataPointer, NumNeuralNetInputs);
	if (InputDataPointer)
	{
		// Clip network inputs based on training set min and max values.
		NearestNeighborModel->ClipInputs(InputDataPointer, NumNeuralNetInputs);
		return true;
	}
    return false;
}

void UNearestNeighborModelInstance::Init(USkeletalMeshComponent* SkelMeshComponent)
{
	UMLDeformerMorphModelInstance::Init(SkelMeshComponent);
	InitPreviousWeights();
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	if (NearestNeighborModel->DoesUseOptimizedNetwork())
	{
		InitOptimizedNetworkInstance();
	}
}

void UNearestNeighborModelInstance::InitOptimizedNetworkInstance()
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	UNearestNeighborOptimizedNetwork* Network = NearestNeighborModel->GetOptimizedNetwork();
	OptimizedNetworkInstance = Network ? Network->CreateInstance() : nullptr;
}

void UNearestNeighborModelInstance::Execute(float ModelWeight)
{
	CSV_SCOPED_TIMING_STAT(MLDeformer, NearestNeighborExecute);
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	if (NearestNeighborModel->DoesUseOptimizedNetwork())
	{
		UNearestNeighborOptimizedNetwork* OptimizedNetwork = NearestNeighborModel->GetOptimizedNetwork();
		OptimizedNetworkInstance->Run();
	}
	else
	{
		Super::Execute(ModelWeight);
	}
}

void UNearestNeighborModelInstance::Tick(float DeltaTime, float ModelWeight)
{
	// Post init hasn't yet succeeded, try again.
	// This could for example happen when you add an ML Deformer component, but your SkeletalMesh isn't setup yet, but later becomes valid.
	if (Model && !HasPostInitialized())
	{
		PostMLDeformerComponentInit();
	}

	if (ModelWeight > 0.0001f && HasValidTransforms() && SetupInputs())
	{
		// Execute the model instance.
		// For models using neural networks this will perform the inference, 
		// calculate the network outputs and possibly use them, depending on how the model works.
		Execute(ModelWeight);
		RunNearestNeighborModel(DeltaTime, ModelWeight);
	}
	else
	{
		HandleZeroModelWeight();
	}
}

TArray<float> ExtractArray(const TArray<float>& InArr, int32 Start, int32 End)
{
	int32 Num = End - Start;
	TArray<float> Result; Result.SetNum(Num);
	for (int32 i = 0; i < Num; i++)
	{
		Result[i] = InArr[Start + i];
	}
	return Result;
}

constexpr float FastExpMinusOne(float X)
{
	return X + X * X / 2 + X * X * X / 6;
}

void UNearestNeighborModelInstance::RunNearestNeighborModel(float DeltaTime, float ModelWeight)
{
	UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	if (NearestNeighborModel == nullptr)
	{
		return;
	}
	
	/** DecayCoeff = (e ^ {(DecayFactor - 1) * DeltaTime + Delta0} - 1) / (e ^ {Delta0} - 1)
	 * DecayFactor is a user defined value. DecayCoeff is the actual coefficient used to compute decay. 
	 * DecayCoeff is an exponential decay of DeltaTime.
	 * When DecayFactor = 1, DecayCoeff = 1; when DecayFactor = 0 && DeltaTime = Delta0, DecayCoeff = 0
	 */	
	constexpr float Delta0 = 1.0f / 30;
	const float Delta = (NearestNeighborModel->GetDecayFactor() - 1) * DeltaTime;
	const float DecayCoeff = FastExpMinusOne(FMath::Max(Delta + Delta0, 0)) / FastExpMinusOne(Delta0);

	// Grab the weight data for this morph set.
	// This could potentially fail if we are applying this deformer to the wrong skeletal mesh component.
	const int LOD = 0;	// For now we only support LOD 0, as we can't setup an ML Deformer per LOD yet.
	FExternalMorphSetWeights* WeightData = FindWeightData(LOD);
	if (WeightData == nullptr)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	NearestNeighborIds.SetNum(NearestNeighborModel->GetNumParts());
#endif

	float* OutputTensorData = nullptr;
	int32 NumNetworkWeights = 0;
	GetOutputDataPointer(OutputTensorData, NumNetworkWeights);

	if (OutputTensorData)
	{
		const int32 NumMorphTargets = WeightData->Weights.Num();
		const int32 TotalNumNeighbors = NearestNeighborModel->GetTotalNumNeighbors();
		if (NumMorphTargets >= NumNetworkWeights + 1 + TotalNumNeighbors)
		{
			WeightData->Weights[0] = 1.0f * ModelWeight;

			// Update all generated morph target weights with the values calculated by our neural network.
			for (int32 MorphIndex = 0; MorphIndex < NumNetworkWeights; ++MorphIndex)
			{
				const float W = OutputTensorData[MorphIndex] * ModelWeight;
				UpdateWeightWithDecay(WeightData->Weights, MorphIndex + 1, W, DecayCoeff);
			}

			int32 NeighborOffset = NumNetworkWeights + 1;
			for (int32 PartId = 0; PartId < NearestNeighborModel->GetNumParts(); PartId++)
			{
				const int32 NearestNeighborId = FindNearestNeighbor(OutputTensorData, PartId);
	#if WITH_EDITORONLY_DATA
				NearestNeighborIds[PartId] = NearestNeighborId;
	#endif

				const int32 NumNeighbors = NearestNeighborModel->GetNumNeighbors(PartId);
				for (int32 NeighborId = 0; NeighborId < NumNeighbors; NeighborId++)
				{
					const float W = NeighborId == NearestNeighborId ? ModelWeight * NearestNeighborModel->GetNearestNeighborOffsetWeight() : 0;
					const int32 Index = NeighborOffset + NeighborId;
					if (Index < NumMorphTargets)
					{
						UpdateWeightWithDecay(WeightData->Weights, Index, W, DecayCoeff);
					}
				}
				NeighborOffset += NumNeighbors;
			}
		}
	}
	else
	{
		WeightData->ZeroWeights();
	}
}

void UNearestNeighborModelInstance::UpdateWeightWithDecay(TArray<float>& MorphWeights, int32 Index, float W, float DecayCoeff)
{
	const float PreviousW = PreviousWeights[Index];
	const float NewW = (1 - DecayCoeff) * W + DecayCoeff * PreviousW;
	MorphWeights[Index] = NewW;
	PreviousWeights[Index] = NewW; 
}

int32 UNearestNeighborModelInstance::FindNearestNeighbor(const float* PCAData, int32 PartId)
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	const TArray<float> &NeighborCoeffs = NearestNeighborModel->NeighborCoeffs(PartId);
	float MinD2 = std::numeric_limits<float>::max();
	int32 MinId = -1;
	for (int32 NeighborId = 0; NeighborId < NearestNeighborModel->GetNumNeighbors(PartId); NeighborId++)
	{
		float D2 = 0;
		const int32 NumCoeffs = NearestNeighborModel->GetPCACoeffNum(PartId);
		const int32 CoeffStart = NearestNeighborModel->GetPCACoeffStart(PartId);
		const int32 NumNeighbors = NearestNeighborModel->GetNumNeighbors(PartId);
		for (int32 CoeffId = 0; CoeffId < NumCoeffs; CoeffId++)
		{
			const float Coeff = PCAData[CoeffStart + CoeffId];
			const float NeighborCoeff = NeighborCoeffs[NeighborId * NumCoeffs + CoeffId];
			const float D = Coeff - NeighborCoeff;
			D2 += D * D;
		}
		if (D2 < MinD2)
		{
			MinD2 = D2;
			MinId = NeighborId;
		}
	}
	return MinId;
}

void UNearestNeighborModelInstance::InitPreviousWeights()
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	if (NearestNeighborModel && NearestNeighborModel->GetMorphTargetSet())
	{
		const int32 NumWeights = NearestNeighborModel->GetMorphTargetSet()->MorphBuffers.GetNumMorphs();
		PreviousWeights.SetNumZeroed(NumWeights);
	}
}

void UNearestNeighborModelInstance::GetInputDataPointer(float*& OutInputData, int32& OutNumInputFloats) const
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	if (NearestNeighborModel->DoesUseOptimizedNetwork())
	{
		const UNearestNeighborOptimizedNetwork* OptimizedNetwork = NearestNeighborModel->GetOptimizedNetwork();
		if (OptimizedNetwork && OptimizedNetworkInstance)
		{
			OutInputData = OptimizedNetworkInstance->GetInputs().GetData();
			OutNumInputFloats = OptimizedNetwork->GetNumInputs();
			return;
		}
	}
	else
	{
		UNeuralNetwork* NeuralNetwork = NearestNeighborModel->GetNNINetwork();
		if (NeuralNetwork)
		{
			OutInputData = static_cast<float*>(NeuralNetwork->GetInputDataPointerMutableForContext(NeuralNetworkInferenceHandle));
			OutNumInputFloats = NeuralNetwork->GetInputTensorForContext(NeuralNetworkInferenceHandle).Num();
			return;
		}
	}
	OutInputData = nullptr;
	OutNumInputFloats = 0;
}

void UNearestNeighborModelInstance::GetOutputDataPointer(float*& OutOutputData, int32& OutNumOutputFloats) const
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	if (NearestNeighborModel->DoesUseOptimizedNetwork())
	{
		const UNearestNeighborOptimizedNetwork* OptimizedNetwork = NearestNeighborModel->GetOptimizedNetwork();
		if (OptimizedNetwork && OptimizedNetworkInstance)
		{
			OutOutputData = OptimizedNetworkInstance->GetOutputs().GetData();
			OutNumOutputFloats = OptimizedNetwork->GetNumOutputs();
			return;
		}
	}
	else
	{
		UNeuralNetwork* NeuralNetwork = NearestNeighborModel->GetNNINetwork();
		if (NeuralNetwork)
		{
			const FNeuralTensor& OutputTensor = NeuralNetwork->GetOutputTensorForContext(NeuralNetworkInferenceHandle);
			OutNumOutputFloats = OutputTensor.Num();
			OutOutputData = (float*)OutputTensor.GetDataCasted<float>();
			return;
		}
	}
	OutOutputData = nullptr;
	OutNumOutputFloats = 0;
}

FString UNearestNeighborModelInstance::CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool LogIssues)
{
	ErrorText = FString();

	// If we're not compatible, generate a compatibility string.
	USkeletalMesh* SkelMesh = InSkelMeshComponent ? InSkelMeshComponent->GetSkeletalMeshAsset() : nullptr;
	UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();
	if (SkelMesh && !InputInfo->IsCompatible(SkelMesh) && Model->GetDeformerAsset())
	{
		ErrorText += InputInfo->GenerateCompatibilityErrorString(SkelMesh);
		ErrorText += "\n";
		if (LogIssues)
		{
			UE_LOG(LogNearestNeighborModel, Error, TEXT("ML Deformer '%s' isn't compatible with Skeletal Mesh '%s'.\nReason(s):\n%s"), 
				*Model->GetDeformerAsset()->GetName(), 
				*SkelMesh->GetName(), 
				*ErrorText);
		}
	}

	// Verify the number of inputs versus the expected number of inputs.
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	check(NearestNeighborModel);
	const int32 NumFloatsPerBone = NearestNeighborModel->GetNumFloatsPerBone();
	const int32 NumFloatsPerCurve = NearestNeighborModel->GetNumFloatsPerCurve();
	const int64 NumDeformerAssetInputs = static_cast<int64>(Model->GetInputInfo()->CalcNumNeuralNetInputs(NumFloatsPerBone, NumFloatsPerCurve));
	int64 NumNeuralNetInputs = -1;
	if (NearestNeighborModel->DoesUseOptimizedNetwork())
	{
		const UNearestNeighborOptimizedNetwork* OptimizedNetwork = NearestNeighborModel->GetOptimizedNetwork();
		if (OptimizedNetwork)
		{
			NumNeuralNetInputs = OptimizedNetwork->GetNumInputs();
		}

	}
	else
	{
		UNeuralNetwork* NeuralNetwork = NearestNeighborModel->GetNNINetwork();
		if (NeuralNetwork && NeuralNetwork->IsLoaded())
		{
			NumNeuralNetInputs = NeuralNetwork->GetInputTensor().Num();
		}
	}

	// Only check compatibility after the network is loaded. 
	if (NumNeuralNetInputs >= 0 && NumNeuralNetInputs != NumDeformerAssetInputs) 
	{
		const FString InputErrorString = "The number of network inputs doesn't match the asset. Please retrain the asset."; 
		ErrorText += InputErrorString + "\n";
		if (LogIssues && Model->GetDeformerAsset())
		{
			UE_LOG(LogNearestNeighborModel, Error, TEXT("Deformer '%s': %s"), *(Model->GetDeformerAsset()->GetName()), *InputErrorString);
		}
	}

	return ErrorText;
}
