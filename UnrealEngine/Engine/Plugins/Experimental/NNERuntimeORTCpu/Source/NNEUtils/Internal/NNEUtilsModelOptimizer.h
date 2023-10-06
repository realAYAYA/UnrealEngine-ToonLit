// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NNERuntimeFormat.h"

namespace UE::NNE { class FAttributeMap; }
namespace UE::NNE::Internal { class IModelOptimizer; }

namespace UE::NNEUtils::Internal
{

using FOptimizerOptionsMap = NNE::FAttributeMap;
	
/** Create a model optimizer */
NNEUTILS_API TUniquePtr<NNE::Internal::IModelOptimizer> CreateModelOptimizer(ENNEInferenceFormat InputFormat, ENNEInferenceFormat OutputFormat);

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

} // UE::NNEUtils::Internal
