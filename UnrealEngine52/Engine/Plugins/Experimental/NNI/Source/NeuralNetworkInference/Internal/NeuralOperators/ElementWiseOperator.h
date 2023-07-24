// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperator.h"
#include "NeuralTensor.h"

enum class EElementWiseOperator : uint8;

class NEURALNETWORKINFERENCE_API IElementWiseOperator : public FNeuralOperator
{
public:
	IElementWiseOperator(const FString& InName, const int32 InVersion, const TSharedPtr<EElementWiseOperator>& InElementWiseOperator, const bool bIsInlinedTensor,
		const TArray<float>& InAttributes = TArray<float>());

	virtual ~IElementWiseOperator();

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override final;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

protected:
	TArray<float> Attributes;

	/**
	 * This is the function that child classes must call on ForwardCPU().
	 */
	virtual void ForwardCPUWithFunction(float InOperatorFunction(const float)) final;

	/**
	 * This is the function that child classes must call on ForwardCPU().
	 */
	virtual void ForwardCPUWithFunction(float InOperatorFunction(const float, const float)) final;

private:
	const TSharedPtr<EElementWiseOperator> ElementWiseOperator;
};
