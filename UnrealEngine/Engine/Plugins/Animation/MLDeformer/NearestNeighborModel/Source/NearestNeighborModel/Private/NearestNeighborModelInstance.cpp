// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelInstance.h"
#include "NearestNeighborModel.h"
#include "NearestNeighborModelInputInfo.h"
#include "NeuralNetwork.h"
#include "Components/SkeletalMeshComponent.h"

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

BEGIN_SHADER_PARAMETER_STRUCT(FCopyBufferParameters, )
    RDG_BUFFER_ACCESS(Source,  ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

template<class T, class TRDGBuffer>
void AddCopyToCPUPass(FRDGBuilder& GraphBuilder, TRDGBuffer RDGBuffer, TArray<T>& Array)
{
    FCopyBufferParameters* Parameters = GraphBuilder.AllocParameters<FCopyBufferParameters>();
    Parameters->Source = RDGBuffer;
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("CopyToCPU"),
        Parameters,
        ERDGPassFlags::Readback,
		[RDGBuffer, &Array](FRHICommandListImmediate& CmdList)
		{
			const int32 VolumeInBytes = Array.Num() * sizeof(T);
			FRHIBuffer* RHIBuffer = RDGBuffer->GetRHI();
			const void* const BufferData = CmdList.LockBuffer(RHIBuffer, 0, VolumeInBytes, RLM_ReadOnly);
			FMemory::Memcpy((void*)Array.GetData(), BufferData, VolumeInBytes);
			CmdList.UnlockBuffer(RHIBuffer);
		});
}

template<class T, class TRDGBuffer>
void AddCopyToCPUPass(FRDGBuilder& GraphBuilder, TRDGBuffer RDGBuffer, T* Ptr, int NumElements)
{
    FCopyBufferParameters* Parameters = GraphBuilder.AllocParameters<FCopyBufferParameters>();
    Parameters->Source = RDGBuffer;
    GraphBuilder.AddPass(
        RDG_EVENT_NAME("CopyToCPU"),
        Parameters,
        ERDGPassFlags::Readback,
        [RDGBuffer, Ptr, NumElements](FRHICommandListImmediate& CmdList)
        {
            const int32 VolumeInBytes = NumElements * sizeof(T);
            FRHIBuffer* RHIBuffer = RDGBuffer->GetRHI();
            const void* const BufferData = CmdList.LockBuffer(RHIBuffer, 0, VolumeInBytes, RLM_ReadOnly);
            FMemory::Memcpy((void*)Ptr, BufferData, VolumeInBytes);
            CmdList.UnlockBuffer(RHIBuffer);
        });
}

bool UNearestNeighborModelInstance::SetupInputs()
{
    const bool bSuccess = Super::SetupInputs();

    if (bSuccess)
    {
	    UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	    if (NeuralNetwork)
	    {
			UNearestNeighborModel* NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model.Get());
	    	float* InputDataPointer = static_cast<float*>(NeuralNetwork->GetInputDataPointerMutableForContext(NeuralNetworkInferenceHandle));
	    	const int64 NumNeuralNetInputs = NeuralNetwork->GetInputTensorForContext(NeuralNetworkInferenceHandle).Num();
		    // Clip network inputs based on training set min and max
	    	NearestNeighborModel->ClipInputs(InputDataPointer, NumNeuralNetInputs);
	    	return true;
	    }
    }

    return false;
}

void UNearestNeighborModelInstance::Execute(float ModelWeight)
{
	Super::Execute(ModelWeight);
	RunNearestNeighborModel(ModelWeight);
}

void UNearestNeighborModelInstance::RunNearestNeighborModel(float ModelWeight)
{
	UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
	if (NearestNeighborModel == nullptr)
	{
		return;
	}
	
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

	const UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	if (NeuralNetwork)
	{
		const FNeuralTensor& OutputTensor = NeuralNetwork->GetOutputTensorForContext(NeuralNetworkInferenceHandle);
		const int32 NumNetworkWeights = OutputTensor.Num();
		const int32 NumMorphTargets = WeightData->Weights.Num();;
		if (NumMorphTargets >= NumNetworkWeights + 1)
		{
			WeightData->Weights[0] = 1.0f * ModelWeight;

			// Update all generated morph target weights with the values calculated by our neural network.
			for (int32 MorphIndex = 0; MorphIndex < NumNetworkWeights; ++MorphIndex)
			{
				const float W = OutputTensor.At<float>(MorphIndex) * ModelWeight;
				UpdateWeight(WeightData->Weights, MorphIndex + 1, W);
			}

			int32 NeighborOffset = NumNetworkWeights + 1;
			for (int32 PartId = 0; PartId < NearestNeighborModel->GetNumParts(); PartId++)
			{
				const int32 NearestNeighborId = FindNearestNeighbor(OutputTensor, PartId);
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
						UpdateWeight(WeightData->Weights, Index, W);
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

void UNearestNeighborModelInstance::UpdateWeight(TArray<float>& MorphWeights, int32 Index, float W)
{
	UNearestNeighborModel* NearestNeighborModel = static_cast<UNearestNeighborModel*>(Model);
	const float DecayFactor = NearestNeighborModel->GetDecayFactor();
	const float PreviousW = NearestNeighborModel->PreviousWeights[Index];
	const float NewW = (1 - DecayFactor) * W + DecayFactor * PreviousW;
	MorphWeights[Index] = NewW;
	NearestNeighborModel->PreviousWeights[Index] = NewW; 
}

int32 UNearestNeighborModelInstance::FindNearestNeighbor(const FNeuralTensor& PCACoeffTensor, int32 PartId)
{
	const UNearestNeighborModel* NearestNeighborModel = Cast<UNearestNeighborModel>(Model);
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
			float Coeff = PCACoeffTensor.At<float>(CoeffStart + CoeffId);
			float NeighborCoeff = NeighborCoeffs[CoeffId * NumNeighbors + NeighborId];
			float D = Coeff - NeighborCoeff;
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