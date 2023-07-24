// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NeuralOperators/ConvBaseOperator.h"

class NEURALNETWORKINFERENCE_API FConvOperator : public FConvBaseOperator
{
public:
	FConvOperator(const struct FNodeProto* const InNodeProto);

	FConvOperator(const EAutoPad InAutoPad, const TArray<int64>& InDilations, const int64 InGroup, const TArray<int64>& InKernelShape, const TArray<int64>& InPads, const TArray<int64>& InStrides);

	virtual ~FConvOperator();

protected:
	virtual bool SetAndConfigureStrides(const int32 InNumberConvolutionalDimensions) override final;
};
