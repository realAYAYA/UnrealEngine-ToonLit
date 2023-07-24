// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NNECoreOperator.h"

namespace UE::NNECore::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary
{
	void NNERUNTIMERDG_API Apply(NNECore::Internal::EElementWiseUnaryOperatorType OpType, const NNECore::Internal::FTensor& Tensor, float Alpha, float Beta, float Gamma, NNECore::Internal::FTensor& OutputTensor);
} // UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary