// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEUtilitiesModelBuilder.h"
#include "Templates/UniquePtr.h"

namespace UE::NNE { class FAttributeMap; }
struct FNNEAttributeValue;
struct FNNEModelRaw;

namespace UE::NNEUtilities::Internal
{

NNEUTILITIES_API TUniquePtr<IModelBuilder> CreateONNXModelBuilder(int64 IrVersion, int64 OpsetVersion);

NNEUTILITIES_API bool CreateONNXModelForOperator(const FString& OperatorName, int32 IrVersion, int32 OpsetVersion, bool bUseVariadicShapeForModel,
	TConstArrayView<NNE::Internal::FTensor> InInputTensors, TConstArrayView<NNE::Internal::FTensor> InOutputTensors,
	TConstArrayView<NNE::Internal::FTensor> InWeightTensors, TConstArrayView<TConstArrayView<uint8>> InWeightTensorsData,
	const UE::NNE::FAttributeMap& Attributes, FNNEModelRaw& ModelData);

} // UE::NNEUtilities::Internal

