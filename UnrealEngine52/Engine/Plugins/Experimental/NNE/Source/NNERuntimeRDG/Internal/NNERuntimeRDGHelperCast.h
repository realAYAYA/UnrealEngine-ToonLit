// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNECore::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Cast
{
	void NNERUNTIMERDG_API Apply(const NNECore::Internal::FTensor& Tensor, NNECore::Internal::FTensor& OutputTensor);

} // UE::NNERuntimeRDG::Internal::CPUHelper::Cast