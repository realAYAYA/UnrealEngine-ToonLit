// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Cast
{
	void NNERUNTIMERDG_API Apply(const NNE::Internal::FTensor& Tensor, NNE::Internal::FTensor& OutputTensor);

} // UE::NNERuntimeRDG::Internal::CPUHelper::Cast