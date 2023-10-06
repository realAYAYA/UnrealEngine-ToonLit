// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningNeuralNetwork.h"

namespace UE::Learning
{
	const TCHAR* GetActivationFunctionString(const EActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case EActivationFunction::ReLU: return TEXT("ReLU");
		case EActivationFunction::ELU: return TEXT("ELU");
		case EActivationFunction::TanH: return TEXT("TanH");
		default: UE_LEARNING_NOT_IMPLEMENTED(); return TEXT("Unknown");
		}
	}

	void FNeuralNetwork::Resize(
		const int32 InputNum,
		const int32 OutputNum,
		const int32 HiddenNum,
		const int32 LayerNum)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetwork::Resize);
		UE_LEARNING_CHECKF(LayerNum >= 2, TEXT("At least two layers required (input and output layers)"));

		Weights.SetNum(LayerNum);
		Biases.SetNum(LayerNum);

		Weights[0].SetNumUninitialized({ InputNum, HiddenNum });
		Biases[0].SetNumUninitialized({ HiddenNum });

		for (int32 LayerIdx = 0; LayerIdx < LayerNum - 2; LayerIdx++)
		{
			Weights[LayerIdx + 1].SetNumUninitialized({ HiddenNum, HiddenNum });
			Biases[LayerIdx + 1].SetNumUninitialized({ HiddenNum });
		}

		Weights[LayerNum - 1].SetNumUninitialized({ HiddenNum, OutputNum });
		Biases[LayerNum - 1].SetNumUninitialized({ OutputNum });

		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			Array::Zero(Weights[LayerIdx]);
			Array::Zero(Biases[LayerIdx]);
		}
	}

	int32 FNeuralNetwork::GetInputNum() const
	{
		return Weights[0].Num<0>();
	}

	int32 FNeuralNetwork::GetOutputNum() const
	{
		return Weights[Weights.Num() - 1].Num<1>();
	}

	int32 FNeuralNetwork::GetLayerNum() const
	{
		return Weights.Num();
	}

	int32 FNeuralNetwork::GetHiddenNum() const
	{
		return Weights[0].Num<1>();
	}

	void FNeuralNetwork::DeserializeFromBytes(int32& InOutOffset, const TLearningArrayView<1, const uint8> RawBytes)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetwork::DeserializeFromBytes);

		int32 ActivationFunctionInt;
		UE::Learning::DeserializeFromBytes(InOutOffset, RawBytes, ActivationFunctionInt);
		ActivationFunction = (EActivationFunction)ActivationFunctionInt;

		int32 LayerNum;
		UE::Learning::DeserializeFromBytes(InOutOffset, RawBytes, LayerNum);

		Weights.SetNum(LayerNum);
		Biases.SetNum(LayerNum);

		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			UE::Learning::Array::DeserializeFromBytes(InOutOffset, RawBytes, Weights[LayerIdx]);
			UE::Learning::Array::DeserializeFromBytes(InOutOffset, RawBytes, Biases[LayerIdx]);
		}
	}

	void FNeuralNetwork::SerializeToBytes(int32& InOutOffset, TLearningArrayView<1, uint8> OutRawBytes) const
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetwork::SerializeToBytes);

		UE::Learning::SerializeToBytes(InOutOffset, OutRawBytes, (int32)ActivationFunction);
		UE::Learning::SerializeToBytes(InOutOffset, OutRawBytes, GetLayerNum());

		for (int32 LayerIdx = 0; LayerIdx < GetLayerNum(); LayerIdx++)
		{
			UE::Learning::Array::SerializeToBytes(InOutOffset, OutRawBytes, Weights[LayerIdx]);
			UE::Learning::Array::SerializeToBytes(InOutOffset, OutRawBytes, Biases[LayerIdx]);
		}
	}

	int32 FNeuralNetwork::GetSerializationByteNum(
		const int32 InputNum,
		const int32 OutputNum,
		const int32 HiddenNum,
		const int32 LayerNum)
	{
		int32 Total = 0;
			
		Total += sizeof(int32);  // Activation
		Total += sizeof(int32);  // Layer Num
		Total += UE::Learning::Array::SerializationByteNum<2, float>({ InputNum, HiddenNum });
		Total += UE::Learning::Array::SerializationByteNum<1, float>({ HiddenNum });

		for (int32 LayerIdx = 0; LayerIdx < LayerNum - 2; LayerIdx++)
		{
			Total += UE::Learning::Array::SerializationByteNum<2, float>({ HiddenNum, HiddenNum });
			Total += UE::Learning::Array::SerializationByteNum<1, float>({ HiddenNum });
		}

		Total += UE::Learning::Array::SerializationByteNum<2, float>({ HiddenNum, OutputNum });
		Total += UE::Learning::Array::SerializationByteNum<1, float>({ OutputNum });

		return Total;
	}

}
