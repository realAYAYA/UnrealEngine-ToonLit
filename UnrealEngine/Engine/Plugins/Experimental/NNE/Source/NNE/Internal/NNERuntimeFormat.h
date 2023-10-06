// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "NNEAttributeValue.h"
#include "NNETypes.h"

#include "NNERuntimeFormat.generated.h"

UENUM()
enum class ENNEFormatTensorType : uint8
{
	None,	
	Input,
	Output,
	Intermediate,
	Initializer,

	NUM
};

UENUM()
enum class ENNEInferenceFormat : uint8
{
	Invalid,
	ONNX,				//!< ONNX Open Neural Network Exchange
	ORT,				//!< ONNX Runtime (only for CPU)
	NNERT				//!< NNE Runtime format
};

USTRUCT()
struct FNNEModelRaw
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<uint8>		Data;
	
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ENNEInferenceFormat	Format { ENNEInferenceFormat::Invalid };
};

// Required by LoadModel() when loading operators in HLSL and DirectML runtime
USTRUCT()
struct FNNEFormatAttributeDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FNNEAttributeValue Value;
};

USTRUCT()
struct FNNEFormatOperatorDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString TypeName;			//!< For example "Relu"

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<uint32> InTensors;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<uint32> OutTensors;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FNNEFormatAttributeDesc> Attributes;
};

USTRUCT()
struct FNNEFormatTensorDesc
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int32> Shape;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ENNEFormatTensorType	Type = ENNEFormatTensorType::None;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ENNETensorDataType	DataType = ENNETensorDataType::None;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	uint64	DataSize = 0;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	uint64	DataOffset = 0;
};

/// NNE Runtime format
USTRUCT()
struct FNNERuntimeFormat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FNNEFormatTensorDesc> Tensors;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FNNEFormatOperatorDesc> Operators;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<uint8> TensorData;
};