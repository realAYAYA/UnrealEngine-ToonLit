// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Transpose
{
	bool NNERUNTIMERDG_API Apply(const NNE::Internal::FTensor& DataTensor, TConstArrayView<int32> Perms, NNE::Internal::FTensor& OutputTensor);
	bool NNERUNTIMERDG_API TransposePreparedData(NNE::Internal::FTensor& Tensor, TConstArrayView<int32> Perms);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Transpose