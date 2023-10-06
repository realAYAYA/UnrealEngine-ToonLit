// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHlslHelper.h"
#include "NNETypes.h"
#include "NNETensor.h"
#include "RHI.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	void FillTensorSizeShaderParameters(const NNE::Internal::FTensor& Tensor, TStaticArray<FUintVector4, NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS, 16U>& OutShaderParam, int32 Idx)
	{
		static_assert(NNE::FTensorShape::MaxRank <= NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS);
		for (int32 i = 0; i < Tensor.GetShape().Rank(); ++i)
		{
			OutShaderParam[i][Idx] = Tensor.GetShape().GetData()[i];
		}
	}

	void FillTensorStrideShaderParameters(const NNE::Internal::FTensor& Tensor, TStaticArray<FUintVector4, NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS, 16U>& OutShaderParam, int32 Idx, int32 TargetNumdimensionForBroadcast)
	{
		if (TargetNumdimensionForBroadcast == -1)
		{
			TargetNumdimensionForBroadcast = Tensor.GetShape().Rank();
		}
		checkf(TargetNumdimensionForBroadcast >= Tensor.GetShape().Rank(), TEXT("Can't broadcast tensor from rank %d to rank %d, should be inferior or equal."), Tensor.GetShape().Rank(), TargetNumdimensionForBroadcast);
		int32 Offset = TargetNumdimensionForBroadcast - Tensor.GetShape().Rank();

		static_assert(NNE::FTensorShape::MaxRank <= NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS);
		for (int32 i = 7; i >= 0; --i)
		{
			if (i >= TargetNumdimensionForBroadcast || i < Offset)
			{
				OutShaderParam[i][Idx] = 0;
			}
			else if (i == TargetNumdimensionForBroadcast - 1)
			{
				OutShaderParam[i][Idx] = 1;
			}
			else
			{
				OutShaderParam[i][Idx] = OutShaderParam[i + 1][Idx] * Tensor.GetShape().GetData()[i + 1 - Offset];
			}
		}
	}

	void FillTensorStrideForBroadcastShaderParameters(const NNE::Internal::FTensor& Tensor, int32 OutputNumdimension, TStaticArray<FUintVector4, NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS, 16U>& OutShaderParam, int32 Idx)
	{
		checkf(OutputNumdimension >= Tensor.GetShape().Rank(), TEXT("Can't broadcast tensor from rank %d to rank %d, should be inferior or equal."), Tensor.GetShape().Rank(), OutputNumdimension);
		FillTensorStrideShaderParameters(Tensor, OutShaderParam, Idx, OutputNumdimension);
		int32 Offset = OutputNumdimension - Tensor.GetShape().Rank();
		for (int32 i = Offset; i < OutputNumdimension; ++i)
		{
			// the stride for broadcast dimension is kept as 0
			if (Tensor.GetShape().GetData()[i - Offset] == 1)
			{
				OutShaderParam[i][Idx] = 0;
			}
		}
	}

	FIntVector ComputeElementWiseThreadGroups(uint32 ElementCount, uint32 GroupSizeX)
	{
		FIntVector ThreadGroupCount;
		ThreadGroupCount.X = FMath::DivideAndRoundUp(ElementCount, GroupSizeX);
		ThreadGroupCount.Y = 1;
		ThreadGroupCount.Z = 1;
		if (ThreadGroupCount.X > GRHIMaxDispatchThreadGroupsPerDimension.X)
		{
			ThreadGroupCount.Y = FMath::DivideAndRoundUp(ThreadGroupCount.X, GRHIMaxDispatchThreadGroupsPerDimension.X);
			ThreadGroupCount.X = FMath::DivideAndRoundUp(ThreadGroupCount.X, ThreadGroupCount.Y);
			ensure(ThreadGroupCount.Y <= GRHIMaxDispatchThreadGroupsPerDimension.Y);
		}
		return ThreadGroupCount;
	}
} // UE::NNERuntimeRDG::Private::Hlsl