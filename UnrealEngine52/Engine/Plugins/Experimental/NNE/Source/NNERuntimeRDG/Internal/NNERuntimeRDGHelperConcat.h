// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NNECoreTensor.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Concat
{
	void NNERUNTIMERDG_API Apply(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, NNECore::Internal::FTensor& OutputTensor, int32 Axis);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Concat