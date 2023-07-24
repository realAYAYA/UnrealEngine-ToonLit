// Copyright Epic Games, Inc. All Rights Reserved.

#include "VertexDeltaModelInstance.h"
#include "VertexDeltaModel.h"
#include "MLDeformerAsset.h"
#include "NeuralNetwork.h"
#include "RenderGraphBuilder.h"
#include "Components/SkeletalMeshComponent.h"


UVertexDeltaModel* UVertexDeltaModelInstance::GetVertexDeltaModel() const
{
	return Cast<UVertexDeltaModel>(Model);
}

UNeuralNetwork* UVertexDeltaModelInstance::GetNNINetwork() const
{
	return GetVertexDeltaModel()->GetNNINetwork();
}

void UVertexDeltaModelInstance::Release()
{
	Super::Release();

	// Destroy the neural network instance.
	if (GetVertexDeltaModel())
	{
		UNeuralNetwork* NNINetwork = GetNNINetwork();
		if (NNINetwork && NeuralNetworkInferenceHandle != -1)
		{
			NNINetwork->DestroyInferenceContext(NeuralNetworkInferenceHandle);
			NeuralNetworkInferenceHandle = -1;
		}
	}
}

FString UVertexDeltaModelInstance::CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool bLogIssues)
{
	FString ErrorString = Super::CheckCompatibility(InSkelMeshComponent, bLogIssues);

	// Verify the number of inputs versus the expected number of inputs.	
	const UNeuralNetwork* NNINetwork = GetNNINetwork();
	if (NNINetwork && NNINetwork->IsLoaded() && Model->GetDeformerAsset())
	{
		const int64 NumNeuralNetInputs = NNINetwork->GetInputTensor().Num();
		const int64 NumDeformerAssetInputs = static_cast<int64>(Model->GetInputInfo()->CalcNumNeuralNetInputs(Model->GetNumFloatsPerBone(), Model->GetNumFloatsPerCurve()));
		if (NumNeuralNetInputs != NumDeformerAssetInputs)
		{
			const FString InputErrorString = "The number of network inputs doesn't match the asset. Please retrain the asset."; 
			ErrorText += InputErrorString + "\n";
			if (bLogIssues)
			{
				UE_LOG(LogVertexDeltaModel, Error, TEXT("Deformer '%s': %s"), *(Model->GetDeformerAsset()->GetName()), *InputErrorString);
			}
		}
	}

	return ErrorString;
}

bool UVertexDeltaModelInstance::IsValidForDataProvider() const
{
	UNeuralNetwork* NNINetwork = GetNNINetwork();
	if (NNINetwork == nullptr || !NNINetwork->IsLoaded())
	{
		return false;
	}

	// We expect to run on the GPU when using a data provider for the deformer graph system (Optimus).
	// Make sure we're actually running the network on the GPU.
	// Inputs are expected to come from the CPU though.
	if (NNINetwork->GetDeviceType() != ENeuralDeviceType::GPU || NNINetwork->GetOutputDeviceType() != ENeuralDeviceType::GPU || NNINetwork->GetInputDeviceType() != ENeuralDeviceType::CPU)
	{
		return false;
	}

	return (Model->GetVertexMapBuffer().ShaderResourceViewRHI != nullptr) && (GetNeuralNetworkInferenceHandle() != -1);
}

void UVertexDeltaModelInstance::Execute(float ModelWeight)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerModelInstance::Execute)

	UNeuralNetwork* NNINetwork = GetNNINetwork();
	if (NNINetwork == nullptr)
	{
		return;
	}

	// Even if the model needs the GPU it is possible that the hardware does not support GPU evaluation
	if (NNINetwork->GetDeviceType() == ENeuralDeviceType::GPU)
	{
		// NOTE: Inputs still come from the CPU.
		check(NNINetwork->GetDeviceType() == ENeuralDeviceType::GPU && NNINetwork->GetInputDeviceType() == ENeuralDeviceType::CPU && NNINetwork->GetOutputDeviceType() == ENeuralDeviceType::GPU);
		ENQUEUE_RENDER_COMMAND(RunNeuralNetwork)
			(
				[NNINetwork, Handle = NeuralNetworkInferenceHandle](FRHICommandListImmediate& RHICmdList)
				{
					// Output deltas will be available on GPU for DeformerGraph via UMLDeformerDataProvider.
					FRDGBuilder GraphBuilder(RHICmdList);
					NNINetwork->Run(GraphBuilder, Handle);
					GraphBuilder.Execute();
				}
		);
	}
}

bool UVertexDeltaModelInstance::SetupInputs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerModelInstance::SetupInputs)

	// Some safety checks.
	if (Model == nullptr ||
		SkeletalMeshComponent == nullptr ||
		SkeletalMeshComponent->GetSkeletalMeshAsset() == nullptr ||
		!bIsCompatible)
	{
		return false;
	}

	// Get the network and make sure it's loaded.
	UNeuralNetwork* NNINetwork = GetNNINetwork();
	if (NNINetwork == nullptr || !NNINetwork->IsLoaded())
	{
		return false;
	}

	// Allocate an inference context if none has already been allocated.
	if (NeuralNetworkInferenceHandle == -1)
	{
		NeuralNetworkInferenceHandle = NNINetwork->CreateInferenceContext();
		if (NeuralNetworkInferenceHandle == -1)
		{
			return false;
		}
	}

	// If the neural network expects a different number of inputs, do nothing.
	const int64 NumNeuralNetInputs = NNINetwork->GetInputTensorForContext(NeuralNetworkInferenceHandle).Num();
	const int64 NumDeformerAssetInputs = Model->GetInputInfo()->CalcNumNeuralNetInputs(Model->GetNumFloatsPerBone(), Model->GetNumFloatsPerCurve());
	if (NumNeuralNetInputs != NumDeformerAssetInputs)
	{
		return false;
	}

	// Update and write the input values directly into the input tensor.
	float* InputDataPointer = static_cast<float*>(NNINetwork->GetInputDataPointerMutableForContext(NeuralNetworkInferenceHandle));
	const int64 NumFloatsWritten = SetNeuralNetworkInputValues(InputDataPointer, NumNeuralNetInputs);
	check(NumFloatsWritten == NumNeuralNetInputs);

	return true;
}

