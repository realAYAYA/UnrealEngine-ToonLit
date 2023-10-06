// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NNEOperator.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary
{
	void NNERUNTIMERDG_API Apply(NNE::Internal::EElementWiseUnaryOperatorType OpType, const NNE::Internal::FTensor& Tensor, float Alpha, float Beta, float Gamma, NNE::Internal::FTensor& OutputTensor);
} // UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary