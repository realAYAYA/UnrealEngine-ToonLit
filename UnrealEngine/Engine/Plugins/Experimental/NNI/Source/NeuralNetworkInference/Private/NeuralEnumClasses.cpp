// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralEnumClasses.h"
#include "NeuralNetworkInferenceUtils.h"



/* FNeuralNetworkInferenceUtils public functions
 *****************************************************************************/

int64 FNeuralDataTypeUtils::GetByteSize(const ENeuralDataType InDataType)
{
	if (InDataType == ENeuralDataType::Float)
	{
		return sizeof(float);
	}
	else if (InDataType == ENeuralDataType::Int32)
	{
		return sizeof(int32);
	}
	else if (InDataType == ENeuralDataType::Int64)
	{
		return sizeof(int64);
	}
	else if (InDataType == ENeuralDataType::UInt32)
	{
		return sizeof(uint32);
	}
	else if (InDataType == ENeuralDataType::UInt64)
	{
		return sizeof(uint64);
	}
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralDataTypeUtils::GetByteSize(): Unknown InDataType = %d used."), (int32)InDataType);
	return 1;
}

EPixelFormat FNeuralDataTypeUtils::GetPixelFormat(const ENeuralDataType InDataType)
{
	if (InDataType == ENeuralDataType::Float)
	{
		return EPixelFormat::PF_R32_FLOAT;
	}
	else if (InDataType == ENeuralDataType::Int32)
	{
		return EPixelFormat::PF_R32_SINT;
	}
	else if (InDataType == ENeuralDataType::UInt32)
	{
		return EPixelFormat::PF_R32_UINT;
	}
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralDataTypeUtils::GetPixelFormat(): Unknown InDataType = %d used."), (int32)InDataType);
	return EPixelFormat::PF_Unknown;
}

FString FNeuralDataTypeUtils::ToString(const ENeuralDataType InDataType)
{
	if (InDataType == ENeuralDataType::Float)
	{
		return TEXT("Float");
	}
	else if (InDataType == ENeuralDataType::Int32)
	{
		return TEXT("Int32");
	}
	else if (InDataType == ENeuralDataType::Int64)
	{
		return TEXT("Int64");
	}
	else if (InDataType == ENeuralDataType::UInt32)
	{
		return TEXT("UInt32");
	}
	else if (InDataType == ENeuralDataType::UInt64)
	{
		return TEXT("UInt64");
	}
	else if (InDataType == ENeuralDataType::None)
	{
		return TEXT("None");
	}
	UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FNeuralDataTypeUtils::ToString(): Unknown InDataType = %d used."), (int32)InDataType);
	return TEXT("");
}
