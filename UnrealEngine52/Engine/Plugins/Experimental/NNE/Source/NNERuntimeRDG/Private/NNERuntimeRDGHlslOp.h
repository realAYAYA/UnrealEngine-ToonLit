// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNERuntimeRDG.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	/**
	 * Base class for all Hlsl ML operators
	 */
	struct FOperatorHlsl : public IOperatorRDG, public IPrepareOperator
	{
		virtual ~FOperatorHlsl() = default;
		virtual bool Initialize(TConstArrayView<NNECore::FTensorDesc> InputTensorDescs, TConstArrayView<NNECore::FTensorDesc> OutputTensorDescs, const NNECore::FAttributeMap& Attributes) = 0;
		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InInputTensors, TConstArrayView<FTensorRDGRef> InOutputTensors) = 0;
		virtual void OptimizeInputsWeights(TArrayView<FTensorRDGRef> InputWeights) {};
	};

	/**
	  * HLSL ML operator registry
	*/
	typedef TOperatorRegistryRDG<FOperatorHlsl> FOperatorRegistryHlsl;
	typedef TModelValidatorRDG<FOperatorHlsl> FModelValidatorHlsl;

} // UE::NNERuntimeRDG::Private::Hlsl