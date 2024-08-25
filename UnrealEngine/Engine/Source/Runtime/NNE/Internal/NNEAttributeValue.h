// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEAttributeValueTraits.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/Object.h"

#include "NNEAttributeValue.generated.h"

USTRUCT()
struct FNNEAttributeValue
{
	GENERATED_USTRUCT_BODY()

	FNNEAttributeValue()
	{
		Type = ENNEAttributeDataType::None;
	}

	template<typename T>
	explicit FNNEAttributeValue(T InValue)
	{
		FMemoryWriter writer(Value, true);
		writer << InValue;

		Type = TNNEAttributeValueTraits<T>::GetType();
	}

	template<typename T>
	T GetValue() const
	{
		check((Type == TNNEAttributeValueTraits<T>::GetType()));

		T Result;

		FMemoryReader Reader(Value, true);
		Reader << Result;

		return Result;
	}

	ENNEAttributeDataType GetType() const
	{
		return Type;
	}
	
private:
	
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ENNEAttributeDataType	Type;
	
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<uint8>			Value;
};