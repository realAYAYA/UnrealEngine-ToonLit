// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningCompletionObject.h"
#include "LearningLog.h"

namespace UE::Learning
{
	FCompletionObject::FCompletionObject(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const ECompletionMode InCompletionMode)
		: InstanceData(InInstanceData)
		, CompletionMode(InCompletionMode)
	{
		CompletionHandle = InstanceData->Add<1, ECompletionMode>({ InIdentifier, TEXT("Completion") }, { InMaxInstanceNum }, ECompletionMode::Running);
	}

	TLearningArrayView<1, ECompletionMode> FCompletionObject::CompletionBuffer()
	{
		return InstanceData->View(CompletionHandle);
	}

	//------------------------------------------------------------------

	FAnyCompletion::FAnyCompletion(
		const FName& InIdentifier,
		const TLearningArrayView<1, const TSharedRef<FCompletionObject>> InCompletions,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, ECompletionMode::Terminated)
		, Completions(InCompletions) {}

	void FAnyCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FAnyCompletion::Evaluate);

		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 CompletionNum = Completions.Num();

		for (int32 CompletionIdx = 0; CompletionIdx < CompletionNum; CompletionIdx++)
		{
			Completions[CompletionIdx]->Evaluate(Instances);
		}

		Array::Set(Completion, ECompletionMode::Running, Instances);

		for (int32 CompletionIdx = 0; CompletionIdx < CompletionNum; CompletionIdx++)
		{
			const TLearningArrayView<1, const ECompletionMode> Input = InstanceData->ConstView(Completions[CompletionIdx]->CompletionHandle);

			for (const int32 InstanceIdx : Instances)
			{
				Completion[InstanceIdx] = Completion::Or(Completion[InstanceIdx], Input[InstanceIdx]);
			}
		}
	}

	//------------------------------------------------------------------

	FConditionalCompletion::FConditionalCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const ECompletionMode InCompletionMode)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
	{
		ConditionHandle = InstanceData->Add<1, bool>({ InIdentifier, TEXT("Condition") }, { InMaxInstanceNum }, 0.0f);
	}

	void FConditionalCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FConditionalCompletion::Evaluate);

		const TLearningArrayView<1, const bool> Condition = InstanceData->ConstView(ConditionHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = Condition[InstanceIdx] ? CompletionMode : ECompletionMode::Running;
		}
	}

	//------------------------------------------------------------------

	FTimeElapsedCompletion::FTimeElapsedCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
	{
		TimeHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Time") }, { InMaxInstanceNum }, 0.0f);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FTimeElapsedCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FTimeElapsedCompletion::Evaluate);

		const TLearningArrayView<1, const float> Time = InstanceData->ConstView(TimeHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = Time[InstanceIdx] > Threshold[InstanceIdx] ? CompletionMode : ECompletionMode::Running;
		}
	}

	//------------------------------------------------------------------

	FScalarPositionDifferenceCompletion::FScalarPositionDifferenceCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InPositionNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
	{
		Position0Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum, InPositionNum }, 0.0f);
		Position1Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum, InPositionNum }, 0.0f);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarPositionDifferenceCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarPositionDifferenceCompletion::Evaluate);

		const TLearningArrayView<2, const float> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<2, const float> Position1 = InstanceData->ConstView(Position1Handle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 PositionNum = Position0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = ECompletionMode::Running;

			for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
			{
				if (FMath::Abs(Position0[InstanceIdx][PositionIdx] - Position1[InstanceIdx][PositionIdx]) > Threshold[InstanceIdx])
				{
					Completion[InstanceIdx] = CompletionMode;
					break;
				}
			}
		}
	}

	FPlanarPositionDifferenceCompletion::FPlanarPositionDifferenceCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InPositionNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode,
		const FVector InAxis0,
		const FVector InAxis1)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
		, Axis0(InAxis0)
		, Axis1(InAxis1)
	{
		Position0Handle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum, InPositionNum }, FVector::ZeroVector);
		Position1Handle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum, InPositionNum }, FVector::ZeroVector);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FPlanarPositionDifferenceCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FPlanarPositionDifferenceCompletion::Evaluate);

		const TLearningArrayView<2, const FVector> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<2, const FVector> Position1 = InstanceData->ConstView(Position1Handle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 PositionNum = Position0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = ECompletionMode::Running;

			for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
			{
				const FVector ProjectedPosition0 = FVector(Axis0.Dot(Position0[InstanceIdx][PositionIdx]), Axis1.Dot(Position0[InstanceIdx][PositionIdx]), 0.0f);
				const FVector ProjectedPosition1 = FVector(Axis0.Dot(Position1[InstanceIdx][PositionIdx]), Axis1.Dot(Position1[InstanceIdx][PositionIdx]), 0.0f);

				if (FVector::Distance(ProjectedPosition0, ProjectedPosition1) > Threshold[InstanceIdx])
				{
					Completion[InstanceIdx] = CompletionMode;
					break;
				}
			}
		}
	}

	FPlanarPositionSimilarityCompletion::FPlanarPositionSimilarityCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InPositionNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode,
		const FVector InAxis0,
		const FVector InAxis1)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
		, Axis0(InAxis0)
		, Axis1(InAxis1)
	{
		Position0Handle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum, InPositionNum }, FVector::ZeroVector);
		Position1Handle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum, InPositionNum }, FVector::ZeroVector);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FPlanarPositionSimilarityCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FPlanarPositionSimilarityCompletion::Evaluate);

		const TLearningArrayView<2, const FVector> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<2, const FVector> Position1 = InstanceData->ConstView(Position1Handle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 PositionNum = Position0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = ECompletionMode::Running;

			for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
			{
				const FVector ProjectedPosition0 = FVector(Axis0.Dot(Position0[InstanceIdx][PositionIdx]), Axis1.Dot(Position0[InstanceIdx][PositionIdx]), 0.0f);
				const FVector ProjectedPosition1 = FVector(Axis0.Dot(Position1[InstanceIdx][PositionIdx]), Axis1.Dot(Position1[InstanceIdx][PositionIdx]), 0.0f);

				if (FVector::Distance(ProjectedPosition0, ProjectedPosition1) < Threshold[InstanceIdx])
				{
					Completion[InstanceIdx] = CompletionMode;
					break;
				}
			}
		}
	}

	//------------------------------------------------------------------

	FScalarVelocityDifferenceCompletion::FScalarVelocityDifferenceCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InVelocityNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
	{
		Velocity0Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Velocity0") }, { InMaxInstanceNum, InVelocityNum }, 0.0f);
		Velocity1Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Velocity1") }, { InMaxInstanceNum, InVelocityNum }, 0.0f);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarVelocityDifferenceCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarVelocityDifferenceCompletion::Evaluate);

		const TLearningArrayView<2, const float> Velocity0 = InstanceData->ConstView(Velocity0Handle);
		const TLearningArrayView<2, const float> Velocity1 = InstanceData->ConstView(Velocity1Handle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 VelocityNum = Velocity0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = ECompletionMode::Running;

			for (int32 VelocityIdx = 0; VelocityIdx < VelocityNum; VelocityIdx++)
			{
				if (FMath::Abs(Velocity0[InstanceIdx][VelocityIdx] - Velocity1[InstanceIdx][VelocityIdx]) > Threshold[InstanceIdx])
				{
					Completion[InstanceIdx] = CompletionMode;
					break;
				}
			}
		}
	}

	//------------------------------------------------------------------

	FScalarRotationDifferenceCompletion::FScalarRotationDifferenceCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InRotationNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
	{
		Rotation0Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Rotation0") }, { InMaxInstanceNum, InRotationNum }, 0.0f);
		Rotation1Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Rotation1") }, { InMaxInstanceNum, InRotationNum }, 0.0f);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarRotationDifferenceCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarRotationDifferenceCompletion::Evaluate);

		const TLearningArrayView<2, const float> Rotation0 = InstanceData->ConstView(Rotation0Handle);
		const TLearningArrayView<2, const float> Rotation1 = InstanceData->ConstView(Rotation1Handle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 RotationNum = Rotation0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = ECompletionMode::Running;

			for (int32 RotationIdx = 0; RotationIdx < RotationNum; RotationIdx++)
			{
				if (FMath::Abs(FMath::FindDeltaAngleRadians(Rotation1[InstanceIdx][RotationIdx], Rotation0[InstanceIdx][RotationIdx])) > Threshold[InstanceIdx])
				{
					Completion[InstanceIdx] = CompletionMode;
					break;
				}
			}
		}
	}

	//------------------------------------------------------------------

	FScalarAngularVelocityDifferenceCompletion::FScalarAngularVelocityDifferenceCompletion(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InAngularVelocityNum,
		const float InThreshold,
		const ECompletionMode InCompletionMode)
		: FCompletionObject(InIdentifier, InInstanceData, InMaxInstanceNum, InCompletionMode)
	{
		AngularVelocity0Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("AngularVelocity0") }, { InMaxInstanceNum, InAngularVelocityNum }, 0.0f);
		AngularVelocity1Handle = InstanceData->Add<2, float>({ InIdentifier, TEXT("AngularVelocity1") }, { InMaxInstanceNum, InAngularVelocityNum }, 0.0f);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarAngularVelocityDifferenceCompletion::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarAngularVelocityDifferenceCompletion::Evaluate);

		const TLearningArrayView<2, const float> AngularVelocity0 = InstanceData->ConstView(AngularVelocity0Handle);
		const TLearningArrayView<2, const float> AngularVelocity1 = InstanceData->ConstView(AngularVelocity1Handle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		TLearningArrayView<1, ECompletionMode> Completion = InstanceData->View(CompletionHandle);

		const int32 AngularVelocityNum = AngularVelocity0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Completion[InstanceIdx] = ECompletionMode::Running;

			for (int32 AngularVelocityIdx = 0; AngularVelocityIdx < AngularVelocityNum; AngularVelocityIdx++)
			{
				if (FMath::Abs(AngularVelocity0[InstanceIdx][AngularVelocityIdx] - AngularVelocity1[InstanceIdx][AngularVelocityIdx]) > Threshold[InstanceIdx])
				{
					Completion[InstanceIdx] = CompletionMode;
					break;
				}
			}
		}
	}

	//------------------------------------------------------------------
}
