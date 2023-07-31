// Copyright Epic Games, Inc. All Rights Reserved.

#include "Agents/MLAdapterAgent_Inference.h"
#include "MLAdapterTypes.h"
#include "HAL/UnrealMemory.h"

void UMLAdapterAgent_Inference::PostInitProperties()
{
	UNeuralNetwork* NeuralNetwork = (UNeuralNetwork*)NeuralNetworkPath.TryLoad();
	
	if (NeuralNetwork != nullptr)
	{
		Brain = NeuralNetwork;
	}

	Super::PostInitProperties();
}

void UMLAdapterAgent_Inference::Think(const float DeltaTime)
{
	if (Brain == nullptr)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Agent beginning to Think but Brain is null, Skipping."));
		return;
	}

	if (bEnableActionDuration && !bActionDurationElapsed)
	{
		// Action duration is enabled and it's not time to think yet.
		return;
	}

	bActionDurationElapsed = false;

	TArray<uint8> Buffer;
	FMLAdapterMemoryWriter Writer(Buffer);
	GetObservations(Writer);

	const float* DataPtr = (float*)Buffer.GetData();
	FMemory::Memcpy(Brain->GetInputDataPointerMutable(), DataPtr, Buffer.Num());

	Brain->Run();

	const FNeuralTensor& Tensor = Brain->GetOutputTensor();
	const TArray<uint8>& Data = Tensor.GetUnderlyingUInt8ArrayRef();
	FMLAdapterMemoryReader Reader(Data);
	DigestActions(Reader);
}
