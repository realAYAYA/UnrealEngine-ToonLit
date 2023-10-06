// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Gather
{
	void NNERUNTIMERDG_API Apply(const NNE::Internal::FTensor& DataTensor, const NNE::Internal::FTensor& IndicesTensor, int32 Axis, NNE::Internal::FTensor& OutputTensor);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Gather