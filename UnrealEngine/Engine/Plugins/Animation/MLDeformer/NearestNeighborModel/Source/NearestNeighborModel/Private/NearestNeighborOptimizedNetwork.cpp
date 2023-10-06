// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborOptimizedNetwork.h"
#if NEARESTNEIGHBORMODEL_USE_ISPC
#include "NearestNeighborOptimizedNetwork.ispc.generated.h"
#endif
//--------------------------------------------------------------------------
// UNearestNeighborNetworkLayer
//--------------------------------------------------------------------------
void UNearestNeighborNetworkLayer::AddParameter(const TArray<float>& Values, const TArray<int32>& Shape)
{
	Parameters.Add({Values, Shape});
}

void UNearestNeighborNetworkLayer::Run(const float* RESTRICT InputBuffer, float* RESTRICT OutputBuffer) const
{
}

void UNearestNeighborNetworkLayer_Gemm_Prelu::Run(const float* RESTRICT InputBuffer, float* RESTRICT OutputBuffer) const
{
	const float* Gemm_Weights = Parameters[0].Values.GetData();
	const float* Gemm_Bias = Parameters[1].Values.GetData();
	const float PRelu_Slope = Parameters[2].Values[0];
#if NEARESTNEIGHBORMODEL_USE_ISPC
	ispc::Gemm_PRelu(OutputBuffer, InputBuffer, Gemm_Weights, Gemm_Bias, PRelu_Slope, NumInputs, NumOutputs);
#endif
}

void UNearestNeighborNetworkLayer_Gemm::Run(const float* RESTRICT InputBuffer, float* RESTRICT OutputBuffer) const
{
	const float* Gemm_Weights = Parameters[0].Values.GetData();
	const float* Gemm_Bias = Parameters[1].Values.GetData();
#if NEARESTNEIGHBORMODEL_USE_ISPC
	ispc::Gemm(OutputBuffer, InputBuffer, Gemm_Weights, Gemm_Bias, NumInputs, NumOutputs);
#endif
}

//--------------------------------------------------------------------------
// UNearestNeighborOptimizedNetwork
//--------------------------------------------------------------------------
void UNearestNeighborOptimizedNetwork::Empty()
{
	Layers.Empty();
}

bool UNearestNeighborOptimizedNetwork::IsEmpty() const
{
	return Layers.IsEmpty();
}

int32 UNearestNeighborOptimizedNetwork::GetNumInputs() const
{
	return !Layers.IsEmpty() ? Layers[0]->NumInputs : 0;
}

int32 UNearestNeighborOptimizedNetwork::GetNumOutputs() const
{
	return !Layers.IsEmpty() ? Layers[Layers.Num()-1]->NumOutputs : 0;
}

bool UNearestNeighborOptimizedNetwork::Load(const FString& Filename)
{
	return true;
}

UNearestNeighborOptimizedNetworkInstance* UNearestNeighborOptimizedNetwork::CreateInstance()
{
	UNearestNeighborOptimizedNetworkInstance* Instance = NewObject<UNearestNeighborOptimizedNetworkInstance>(this);
	Instance->Init(this);
	return Instance;
}

const int32 UNearestNeighborOptimizedNetwork::GetNumLayers() const
{
	return Layers.Num();
}

UNearestNeighborNetworkLayer* UNearestNeighborOptimizedNetwork::GetLayer(int32 Index) const
{
	return Layers[Index].Get();
}

UNearestNeighborNetworkLayer* UNearestNeighborOptimizedNetwork::AddLayer(const int32 LayerType)
{
	UNearestNeighborNetworkLayer* Layer = nullptr;
	switch ((ENearestNeighborNetworkLayerType)LayerType)
	{
		case ENearestNeighborNetworkLayerType::Gemm_Prelu:
			Layer = NewObject<UNearestNeighborNetworkLayer_Gemm_Prelu>(this);
			break;
		case ENearestNeighborNetworkLayerType::Gemm:
			Layer = NewObject<UNearestNeighborNetworkLayer_Gemm>(this);
			break;
		default:
			break;
	}
	if (Layer)
	{
		Layers.Add(Layer);
	}
	return Layer;
}

//--------------------------------------------------------------------------
// UNearestNeighborOptimizedNetworkInstance
//--------------------------------------------------------------------------

TArrayView<float> UNearestNeighborOptimizedNetworkInstance::GetInputs()
{ 
	return TArrayView<float>(Inputs.GetData(), Inputs.Num());
}

TArrayView<const float> UNearestNeighborOptimizedNetworkInstance::GetInputs() const
{ 
	return TArrayView<const float>(Inputs.GetData(), Inputs.Num());
}

TArrayView<float> UNearestNeighborOptimizedNetworkInstance::GetOutputs()
{ 
	return TArrayView<float>(Outputs.GetData(), Outputs.Num());
}

TArrayView<const float> UNearestNeighborOptimizedNetworkInstance::GetOutputs() const
{ 
	return TArrayView<const float>(Outputs.GetData(), Outputs.Num());
}

const UNearestNeighborOptimizedNetwork* UNearestNeighborOptimizedNetworkInstance::GetNeuralNetwork() const
{ 
	return Network.Get();
}

void UNearestNeighborOptimizedNetworkInstance::Init(UNearestNeighborOptimizedNetwork* InNeuralNetwork)
{
	Network = InNeuralNetwork;
	Inputs.SetNumZeroed(Network->GetNumInputs());
	Outputs.SetNumZeroed(Network->GetNumOutputs());

	int32 MaxNumUnits = 0;
	const int32 NumLayers = Network->GetNumLayers();
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const UNearestNeighborNetworkLayer* CurLayer = Network->GetLayer(LayerIndex);
		const int32 NumInputUnits = CurLayer->NumInputs;
		const int32 NumOutputUnits = CurLayer->NumOutputs;
		MaxNumUnits = FMath::Max3<int32>(NumInputUnits, NumOutputUnits, MaxNumUnits);
	}

	TempInputArray.SetNumZeroed(MaxNumUnits);
	TempOutputArray.SetNumZeroed(MaxNumUnits);
}


void UNearestNeighborOptimizedNetworkInstance::Run()
{	
	TRACE_CPUPROFILER_EVENT_SCOPE(UNearestNeighborOptimizedNetwork::Run)

	// Setup the buffer pointers.
	FRunSettings RunSettings;
	RunSettings.TempInputBuffer  = TempInputArray.GetData();
	RunSettings.TempOutputBuffer = TempOutputArray.GetData();
	RunSettings.InputBuffer		 = Inputs.GetData();
	RunSettings.OutputBuffer	 = Outputs.GetData();

	// Run the network.
	const int32 NumLayers = Network->GetNumLayers();
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const UNearestNeighborNetworkLayer* CurLayer = Network->GetLayer(LayerIndex);
		if (LayerIndex == 0)
		{
			const float* const RESTRICT NetworkInputs = RunSettings.InputBuffer;
			CurLayer->Run(RunSettings.InputBuffer, RunSettings.TempInputBuffer);
		}
		else if (LayerIndex == NumLayers - 1)
		{
			CurLayer->Run(RunSettings.TempInputBuffer, RunSettings.OutputBuffer);
		}
		else
		{
			CurLayer->Run(RunSettings.TempInputBuffer, RunSettings.TempOutputBuffer);
			Swap(RunSettings.TempInputBuffer, RunSettings.TempOutputBuffer);
		}
	}
}
