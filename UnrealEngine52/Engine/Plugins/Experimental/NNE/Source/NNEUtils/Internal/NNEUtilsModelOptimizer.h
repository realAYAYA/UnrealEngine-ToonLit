// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NNECoreRuntimeFormat.h"

namespace UE::NNECore { class FAttributeMap; }
namespace UE::NNECore::Internal { class IModelOptimizer; }

namespace UE::NNEUtils::Internal
{

using FOptimizerOptionsMap = NNECore::FAttributeMap;
	
/** Create a model optimizer */
NNEUTILS_API TUniquePtr<NNECore::Internal::IModelOptimizer> CreateModelOptimizer(ENNEInferenceFormat InputFormat, ENNEInferenceFormat OutputFormat);

inline TUniquePtr<NNECore::Internal::IModelOptimizer> CreateONNXToNNEModelOptimizer()
{
	return CreateModelOptimizer(ENNEInferenceFormat::ONNX, ENNEInferenceFormat::NNERT);
}

inline TUniquePtr<NNECore::Internal::IModelOptimizer> CreateONNXToORTModelOptimizer()
{
	return CreateModelOptimizer(ENNEInferenceFormat::ONNX, ENNEInferenceFormat::ORT);
}

inline TUniquePtr<NNECore::Internal::IModelOptimizer> CreateONNXToONNXModelOptimizer()
{
	return CreateModelOptimizer(ENNEInferenceFormat::ONNX, ENNEInferenceFormat::ONNX);
}

} // UE::NNEUtils::Internal
