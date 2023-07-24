// Copyright Epic Games, Inc. All Rights Reserved.

#include "Agents/MLAdapterAgent_Inference.h"
#include "HAL/UnrealMemory.h"
#include "MLAdapterTypes.h"
#include "NNECore.h"
#include "NNECoreTypes.h"

void UMLAdapterAgent_Inference::PostInitProperties()
{
	const FString RuntimeName = TEXT("NNERuntimeORTCpu");

	Super::PostInitProperties();
	
	if (ModelData == nullptr)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Neural network asset data not set"));
		return;
	}

	TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNECore::GetRuntime<INNERuntimeCPU>(RuntimeName);
	if (!Runtime.IsValid())
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Runtime %s is not valid!"), *RuntimeName);
		return;
	}

	Brain = Runtime->CreateModelCPU(ModelData);

	TConstArrayView<UE::NNECore::FTensorDesc> InputTensorDescs = Brain->GetInputTensorDescs();
	if (InputTensorDescs.Num() != 1)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Brain accepts only single tensor input."));
		return;
	}
	if (!InputTensorDescs[0].GetShape().IsConcrete())
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Brain does not accept dynamic tensor input shapes."));
		return;
	}

	UE::NNECore::FTensorShape InputTensorShape = UE::NNECore::FTensorShape::MakeFromSymbolic(InputTensorDescs[0].GetShape());

	Brain->SetInputTensorShapes({ InputTensorShape });

	TConstArrayView<UE::NNECore::FTensorDesc> OutputTensorDescs = Brain->GetOutputTensorDescs();
	if (OutputTensorDescs.Num() != 1)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Brain accepts only single tensor output."));
		return;
	}

	TConstArrayView<UE::NNECore::FTensorShape> OutputTensorShapes = Brain->GetOutputTensorShapes();
	if (OutputTensorShapes.Num() != 1)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Brain could not resolve output shapes."));
		return;
	}

	InputTensorSizeInBytes = InputTensorDescs[0].GetElemByteSize() * InputTensorShape.Volume();
	if (InputTensorSizeInBytes == 0)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Input tensor should be of non-zero size."));
		return;
	}

	OutputTensorSizeInBytes = OutputTensorDescs[0].GetElemByteSize() * OutputTensorShapes[0].Volume();
	if (OutputTensorSizeInBytes == 0)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Output tensor should be of non-zero size."));
		return;
	}
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

	if (InputTensorSizeInBytes == 0 || OutputTensorSizeInBytes == 0)
	{
		return;
	}

	bActionDurationElapsed = false;

	TArray<uint8> Buffer;
	FMLAdapterMemoryWriter Writer(Buffer);
	GetObservations(Writer);

	if ((uint32)Buffer.Num() < InputTensorSizeInBytes)
	{
		UE_LOG(LogMLAdapter, Warning, TEXT("Brain received not enough data."));
		return;
	}
	
	TArray<uint8> OutputBuffer;
	OutputBuffer.SetNumUninitialized(OutputTensorSizeInBytes);

	UE::NNECore::FTensorBindingCPU InputTensorBinding{Buffer.GetData(), Buffer.Num()};
	UE::NNECore::FTensorBindingCPU OutputTensorBinding{OutputBuffer.GetData(), OutputBuffer.Num()};

	Brain->RunSync({InputTensorBinding}, {OutputTensorBinding});

	FMLAdapterMemoryReader Reader(OutputBuffer);
	DigestActions(Reader);
}
