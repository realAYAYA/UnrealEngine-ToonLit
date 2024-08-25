// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEAttributeValue.h"
#include "NNETypes.h"
#include "UObject/Object.h"

#include "NNERuntimeFormat.generated.h"

UENUM()
enum class ENNEFormatTensorType : uint8
{
	None,	
	Input,
	Output,
	Intermediate,
	Initializer,
	Empty,

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

struct FNNEModelRaw
{
	TArray<uint8> Data;
	
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
	FString DomainName;			//!< For example "onnx"

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TOptional<uint32> Version;	//!< For example 7

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

	uint64 DataSize;
	TArray<uint8> TensorData;
	
	bool Serialize(FArchive& Ar)
	{
		// Serialize normal UPROPERTY tagged data
		UScriptStruct* Struct = FNNERuntimeFormat::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, reinterpret_cast<uint8*>(this), Struct, nullptr);

		if (Ar.IsLoading())
		{
			Ar << DataSize;
			TensorData.SetNumUninitialized(DataSize);
			Ar.Serialize((void*)TensorData.GetData(), DataSize);
		}
		else if (Ar.IsSaving())
		{
			DataSize = TensorData.Num();
			Ar << DataSize;
			Ar.Serialize((void*)TensorData.GetData(), DataSize);
		}

		return true;
	}

};

template<>
struct TStructOpsTypeTraits<FNNERuntimeFormat> : public TStructOpsTypeTraitsBase2<FNNERuntimeFormat>
{
	enum
	{
		WithSerializer = true
	};
};