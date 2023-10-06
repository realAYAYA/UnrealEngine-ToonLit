// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NNEOperator.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseBinary
{
	void NNERUNTIMERDG_API Apply(NNE::Internal::EElementWiseBinaryOperatorType OpType, const NNE::Internal::FTensor& LHSTensor, const NNE::Internal::FTensor& RHSTensor, NNE::Internal::FTensor& OutputTensor);
	
} // UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseBinary