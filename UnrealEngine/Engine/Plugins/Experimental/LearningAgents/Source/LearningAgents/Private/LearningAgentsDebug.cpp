// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsDebug.h"

namespace UE::Learning::Agents::Debug
{
	FMatrix PlaneMatrix(const FQuat Rotation, const FVector Position, const FVector Axis0, const FVector Axis1)
	{
		// Here we reconstruct the transformation matrix from the plane axes assuming the first axis is forward, and second is to the right.
		const FVector ForwardAxis = Rotation.RotateVector(Axis0.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
		const FVector InitialRightAxis = Rotation.RotateVector(Axis1.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
		const FVector UpAxis = ForwardAxis.Cross(InitialRightAxis).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector RightAxis = UpAxis.Cross(ForwardAxis).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		return FMatrix(ForwardAxis, RightAxis, UpAxis, Position);
	}

	FVector GridOffsetForIndex(const int32 Idx, const int32 Num, const float Spacing, const float Height)
	{
		const int32 RowNum = FMath::Max(FMath::CeilToInt(FMath::Sqrt((float)Num)), 1);

		return Spacing * (FVector((Idx / RowNum), (Idx % RowNum), 0.0f) - FVector(RowNum - 1, RowNum - 1, 0.0f) / 2.0f) + FVector(0.0f, 0.0f, Height);
	}

	FString FloatArrayToStatsString(const TLearningArrayView<1, const float> Array)
	{
		const int32 ItemNum = Array.Num();

		float Min = +FLT_MAX, Max = -FLT_MAX, Mean = 0.0f;
		for (int32 Idx = 0; Idx < ItemNum; Idx++)
		{
			Min = FMath::Min(Min, Array[Idx]);
			Max = FMath::Max(Max, Array[Idx]);
			Mean += Array[Idx] / ItemNum;
		}

		float Var = 0.0f;
		for (int32 Idx = 0; Idx < ItemNum; Idx++)
		{
			Var += FMath::Square(Array[Idx] - Mean) / ItemNum;
		}

		return FString::Printf(TEXT("[% 6.3f/% 6.3f/% 6.3f/% 6.3f]"),
			Min, Max, Mean, FMath::Sqrt(Var));
	}
}
