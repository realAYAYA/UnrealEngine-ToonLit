// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StaticArray.h"

namespace UE::NNECore::Internal { class FTensor; }

#define NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS 8

namespace UE::NNERuntimeRDG::Private::Hlsl
{

	void FillTensorSizeShaderParameters(const NNECore::Internal::FTensor& Tensor, TStaticArray<FUintVector4, NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS, 16U>& OutShaderParam, int32 Idx);
	void FillTensorStrideShaderParameters(const NNECore::Internal::FTensor& Tensor, TStaticArray<FUintVector4, NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS, 16U>& OutShaderParam, int32 Idx, int32 TargetNumdimensionForBroadcast = -1);
	void FillTensorStrideForBroadcastShaderParameters(const NNECore::Internal::FTensor& Tensor, int32 OutputNumdimension, TStaticArray<FUintVector4, NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS, 16U>& OutShaderParam, int32 Idx);
	FIntVector ComputeElementWiseThreadGroups(uint32 ElementCount, uint32 GroupSizeX);
} // namespace UE::NNERuntimeRDG::Private::Hlsl