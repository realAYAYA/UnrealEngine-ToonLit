// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralOperator.h"

class NEURALNETWORKINFERENCE_API FGemmOperator : public FNeuralOperator
{
public:
	FGemmOperator(const struct FNodeProto* const InNodeProto);

	FGemmOperator(const float InAlpha = 1.f, const float InBeta = 1.f, const bool bInTransA = false, const bool bInTransB = false);

	virtual ~FGemmOperator();

	virtual bool ConfigureOutputAndInternalVariablesAndSanityChecks() override final;

	virtual void ForwardCPU() override final;

	virtual void ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder) override final;

private:
	float Alpha;
	float Beta;
	bool bTransA;
	bool bTransB;
	/**
	 * GEMM-required params
	 */
	uint32 OutputRows;
	uint32 OutputColumns;
	uint32 AColsOrBRows;
	uint32 AStrideX;
	uint32 AStrideY;
	uint32 BStrideX;
	uint32 BStrideY;
	uint32 OutputStride;
	/**
	 * C is unidirectional broadcastable to the output. We need this to account for edge cases.
	 * In the usual case (output size == C size), CSize = OutputSize2D
	 */
	uint32 CSizeX;
	uint32 CSizeY;
};
