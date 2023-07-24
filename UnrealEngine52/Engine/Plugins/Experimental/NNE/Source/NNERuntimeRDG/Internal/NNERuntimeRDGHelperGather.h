// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNECore::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Gather
{
	void NNERUNTIMERDG_API Apply(const NNECore::Internal::FTensor& DataTensor, const NNECore::Internal::FTensor& IndicesTensor, int32 Axis, NNECore::Internal::FTensor& OutputTensor);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Gather