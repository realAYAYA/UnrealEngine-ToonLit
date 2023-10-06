// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NNETensor.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Concat
{
	void NNERUNTIMERDG_API Apply(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, NNE::Internal::FTensor& OutputTensor, int32 Axis);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Concat