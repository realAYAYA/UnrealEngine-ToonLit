// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNECore::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Slice
{
	void NNERUNTIMERDG_API Apply(const NNECore::Internal::FTensor& InputTensor, NNECore::Internal::FTensor& OutputTensor, TConstArrayView<int32> Starts);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Slice