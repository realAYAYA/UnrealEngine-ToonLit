// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEUtilitiesModelOptimizer.h"

#include "NNEUtilitiesModelOptimizerNNE.h"
#include "NNEUtilitiesModelOptimizerONNX.h"

namespace UE::NNEUtilities::Internal
{

TUniquePtr<NNE::Internal::IModelOptimizer> CreateModelOptimizer(ENNEInferenceFormat InputFormat, ENNEInferenceFormat OutputFormat)
{
	if (InputFormat == ENNEInferenceFormat::ONNX)
	{
		if (OutputFormat == ENNEInferenceFormat::NNERT)
		{
			return MakeUnique<FModelOptimizerONNXToNNERT>();
		}
		else if (OutputFormat == ENNEInferenceFormat::ONNX)
		{
			return MakeUnique<FModelOptimizerONNXToONNX>();
		}
		else
		{
			return MakeUnique<FModelOptimizerONNXToORT>();
		}
	}

	return nullptr;
}

} // namespace UE::NNEUtilities::Internal
