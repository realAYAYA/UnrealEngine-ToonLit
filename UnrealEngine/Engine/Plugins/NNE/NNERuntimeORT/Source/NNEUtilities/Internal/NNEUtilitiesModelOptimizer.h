// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeFormat.h"

namespace UE::NNE { class FAttributeMap; }

namespace UE::NNEUtilities::Internal
{

using FOptimizerOptionsMap = NNE::FAttributeMap;
	
NNEUTILITIES_API TUniquePtr<NNE::Internal::IModelOptimizer> CreateModelOptimizer(ENNEInferenceFormat InputFormat, ENNEInferenceFormat OutputFormat);

inline TUniquePtr<NNE::Internal::IModelOptimizer> CreateONNXToNNEModelOptimizer()
{
	return CreateModelOptimizer(ENNEInferenceFormat::ONNX, ENNEInferenceFormat::NNERT);
}

inline TUniquePtr<NNE::Internal::IModelOptimizer> CreateONNXToORTModelOptimizer()
{
	return CreateModelOptimizer(ENNEInferenceFormat::ONNX, ENNEInferenceFormat::ORT);
}

inline TUniquePtr<NNE::Internal::IModelOptimizer> CreateONNXToONNXModelOptimizer()
{
	return CreateModelOptimizer(ENNEInferenceFormat::ONNX, ENNEInferenceFormat::ONNX);
}

} // UE::NNEUtilities::Internal
