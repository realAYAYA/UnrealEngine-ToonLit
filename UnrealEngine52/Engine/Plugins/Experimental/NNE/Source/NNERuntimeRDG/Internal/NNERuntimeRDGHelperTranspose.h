// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNECore::Internal { class FTensor; }

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Transpose
{
	bool NNERUNTIMERDG_API TransposePreparedData(NNECore::Internal::FTensor& Tensor, TConstArrayView<int32> Perms);
} // UE::NNERuntimeRDG::Internal::CPUHelper::Transpose