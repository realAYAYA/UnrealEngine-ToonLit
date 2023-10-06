// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Slice
{
	void NNERUNTIMERDG_API Apply(const NNE::Internal::FTensor& InputTensor, NNE::Internal::FTensor& OutputTensor, TConstArrayView<int32> Starts);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Slice