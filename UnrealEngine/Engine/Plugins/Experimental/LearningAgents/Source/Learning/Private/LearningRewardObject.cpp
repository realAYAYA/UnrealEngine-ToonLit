// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningRewardObject.h"
#include "LearningLog.h"

namespace UE::Learning
{
	namespace Reward
	{
		// Misc

		static inline FVector VectorExp(const FVector X)
		{
			return FVector(FMath::Exp(X[0]), FMath::Exp(X[1]), FMath::Exp(X[2]));
		}

		static inline FVector VectorMax(const FVector X, const float Y)
		{
			return FVector(FMath::Max(X[0], Y), FMath::Max(X[1], Y), FMath::Max(X[2], Y));
		}

		static inline float SpringInertializeHalfLifeToDamping(const float HalfLife, const float Epsilon = UE_SMALL_NUMBER)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(HalfLife > 0.0f);

			const float Ln2 = 0.69314718056f;
			return (4.0f * Ln2) / FMath::Max(HalfLife, Epsilon);
		}

		static inline FVector SpringInertializeDecayIntersectionTime(const FVector Position, const FVector Velocity, const float HalfLife, const float Epsilon = UE_SMALL_NUMBER)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(HalfLife > 0.0f);

			const float Y = SpringInertializeHalfLifeToDamping(HalfLife, Epsilon) / 2.0f;
			return -Position / VectorMax(Velocity + Position * Y, Epsilon);
		}

		static inline FVector SpringInertializeDecayTotalDisplacement(const FVector PositionOffset, const FVector VelocityOffset, const float HalfLife, const float Epsilon = UE_SMALL_NUMBER)
		{
			const FVector X = PositionOffset;
			const FVector V = VelocityOffset;
			const float Y = SpringInertializeHalfLifeToDamping(HalfLife, Epsilon) / 2.0f;
			const FVector T = SpringInertializeDecayIntersectionTime(X, V, HalfLife, Epsilon);

			const FVector Integral0 = (X * Y * 2 + V) / (Y * Y);
			const FVector IntegralT = (VectorExp(-Y * T) / (Y * Y)) * (T * V * Y + X * Y * (T * Y + 2) + V);

			return FVector(
				T[0] > 0.0f ? FMath::Abs(Integral0[0] - IntegralT[0]) + FMath::Abs(IntegralT[0]) : FMath::Abs(Integral0[0]),
				T[1] > 0.0f ? FMath::Abs(Integral0[1] - IntegralT[1]) + FMath::Abs(IntegralT[1]) : FMath::Abs(Integral0[1]),
				T[2] > 0.0f ? FMath::Abs(Integral0[2] - IntegralT[2]) + FMath::Abs(IntegralT[2]) : FMath::Abs(Integral0[2]));
		}

		// Helpers

		float DistanceToReward(const float Distance)
		{
			return SquaredDistanceToReward(FMath::Square(Distance));
		}

		float DistanceToPenalty(const float Distance)
		{
			return -Distance;
		}

		float SquaredDistanceToReward(const float SquaredDistance)
		{
			return FMath::InvExpApprox(SquaredDistance);
		}

		float SquaredDistanceToPenalty(const float SquaredDistance)
		{
			return DistanceToPenalty(FMath::Sqrt(SquaredDistance));
		}

		// Rewards

		float ScalarSimilarityReward(
			const float Scalar0,
			const float Scalar1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToReward(FMath::Max(FMath::Abs(Scalar0 - Scalar1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float ScalarPositionSimilarityReward(
			const float Position0,
			const float Position1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToReward(FMath::Max(FMath::Abs(Position0 - Position1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float PositionSimilarityReward(
			const FVector Position0,
			const FVector Position1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToReward(FMath::Max(FVector::Distance(Position0, Position1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float ScalarRotationSimilarityReward(
			const float Rotation0,
			const float Rotation1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToReward(FMath::Max(
				FMath::Abs(FMath::FindDeltaAngleRadians(Rotation1, Rotation0)) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float ScalarAngularVelocitySimilarityReward(
			const float AngularVelocity0,
			const float AngularVelocity1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToReward(FMath::Max(FMath::Abs(AngularVelocity0 - AngularVelocity1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		LEARNING_API float ScalarVelocityReward(
			const float Velocity,
			const float Scale,
			const float Weight,
			const float Epsilon)
		{
			return Weight * (Velocity / FMath::Max(Scale, Epsilon));
		}

		LEARNING_API float LocalDirectionalVelocityReward(
			const FVector Velocity,
			const FQuat RelativeRotation,
			const float Scale,
			const float Weight,
			const FVector Axis,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * (RelativeRotation.UnrotateVector(Velocity).Dot(Axis) / FMath::Max(Scale, Epsilon));
		}

		// Penalties

		float ScalarDifferencePenalty(
			const float Scalar0,
			const float Scalar1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(FMath::Abs(Scalar0 - Scalar1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float ScalarMagnitudePenalty(
			const float Scalar,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(FMath::Abs(Scalar) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float DirectionAngleDifferencePenalty(
			const FVector Direction0,
			const FVector Direction1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(FMath::Acos(Direction0.Dot(Direction1)) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float PlanarPositionDifferencePenalty(
			const FVector Position0,
			const FVector Position1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const FVector Axis0,
			const FVector Axis1,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			const FVector ProjectedPosition0 = FVector(Axis0.Dot(Position0), Axis1.Dot(Position0), 0.0f);
			const FVector ProjectedPosition1 = FVector(Axis0.Dot(Position1), Axis1.Dot(Position1), 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(FVector::Distance(ProjectedPosition0, ProjectedPosition1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float PositionDifferencePenalty(
			const FVector Position0,
			const FVector Position1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(FVector::Dist(Position0, Position1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float VelocityDifferencePenalty(
			const FVector Velocity0,
			const FVector Velocity1,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(FVector::Dist(Velocity0, Velocity1) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}

		float SpringInertializationPenalty(
			const FVector Position0,
			const FVector Position1,
			const FVector Velocity0,
			const FVector Velocity1,
			const float HalfLife,
			const float Scale,
			const float Threshold,
			const float Weight,
			const float Epsilon)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(Scale > 0.0f);
			UE_LEARNING_ARRAY_VALUE_CHECK(HalfLife > 0.0f);

			return Weight * DistanceToPenalty(FMath::Max(SpringInertializeDecayTotalDisplacement(
				Position0 - Position1,
				Velocity0 - Velocity1,
				HalfLife,
				Epsilon).Length() / FMath::Max(HalfLife, Epsilon) - Threshold, 0.0f) / FMath::Max(Scale, Epsilon));
		}
	}

	//------------------------------------------------------------------

	FRewardObject::FRewardObject(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum)
		: InstanceData(InInstanceData)
	{
		RewardHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Reward") }, { InMaxInstanceNum }, 0.0f);
	}

	TLearningArrayView<1, float> FRewardObject::RewardBuffer()
	{
		return  InstanceData->View(RewardHandle);
	}

	//------------------------------------------------------------------

	FSumReward::FSumReward(
		const FName& InIdentifier,
		const TLearningArrayView<1, const TSharedRef<FRewardObject>> InRewards,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, Rewards(InRewards) {}

	void FSumReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FSumReward::Evaluate);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		const int32 RewardNum = Rewards.Num();

		for (int32 RewardIdx = 0; RewardIdx < RewardNum; RewardIdx++)
		{
			Rewards[RewardIdx]->Evaluate(Instances);
		}

		Array::Zero(Reward, Instances);

		for (int32 RewardIdx = 0; RewardIdx < RewardNum; RewardIdx++)
		{
			const TLearningArrayView<1, const float> Input = InstanceData->ConstView(Rewards[RewardIdx]->RewardHandle);

			for (const int32 InstanceIdx : Instances)
			{
				Reward[InstanceIdx] += Input[InstanceIdx];
			}
		}
	}

	//------------------------------------------------------------------

	FConstantReward::FConstantReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InValue)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, Value(InValue) {}

	void FConstantReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FConstantReward::Evaluate);

		Array::Set(InstanceData->View(RewardHandle), Value, Instances);
	}

	FConstantPenalty::FConstantPenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InValue)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, Value(InValue) {}

	void FConstantPenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FConstantPenalty::Evaluate);

		Array::Set(InstanceData->View(RewardHandle), -Value, Instances);
	}

	FConditionalConstantReward::FConditionalConstantReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InValue)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, Value(InValue)
	{
		ConditionHandle = InstanceData->Add<1, bool>({ InIdentifier, TEXT("Condition") }, { InMaxInstanceNum }, false);
	}

	void FConditionalConstantReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FConditionalConstantReward::Evaluate);

		const TLearningArrayView<1, const bool> Condition = InstanceData->ConstView(ConditionHandle);
		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Condition[InstanceIdx] ? Value : 0.0f;
		}
	}

	FFloatReward::FFloatReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		ValueHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Value") }, { InMaxInstanceNum }, 0.0f);
		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
	}

	void FFloatReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FFloatReward::Evaluate);

		const TLearningArrayView<1, const float> Value = InstanceData->ConstView(ValueHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);
		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Weight[InstanceIdx] * Value[InstanceIdx];
		}
	}

	//------------------------------------------------------------------

	FVectorAverageMagnitudePenalty::FVectorAverageMagnitudePenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InDimNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, DimNum(InDimNum)
	{
		ValueHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Value") }, { InMaxInstanceNum, DimNum }, 1.0f);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FVectorAverageMagnitudePenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FVectorAverageMagnitudePenalty::Evaluate);

		const TLearningArrayView<2, const float> Value = InstanceData->ConstView(ValueHandle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = 0.0f;

			for (int32 DimIdx = 0; DimIdx < DimNum; DimIdx++)
			{
				Reward[InstanceIdx] += Reward::ScalarMagnitudePenalty(
					Value[InstanceIdx][DimIdx],
					Scale[InstanceIdx],
					Threshold[InstanceIdx],
					Weight[InstanceIdx]) / DimNum;
			}
		}
	}

	//------------------------------------------------------------------

	FScalarPositionSimilarityReward::FScalarPositionSimilarityReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Position0Handle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum }, 0.0f);
		Position1Handle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum }, 0.0f);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarPositionSimilarityReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarPositionSimilarityReward::Evaluate);

		const TLearningArrayView<1, const float> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<1, const float> Position1 = InstanceData->ConstView(Position1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::ScalarPositionSimilarityReward(
				Position0[InstanceIdx],
				Position1[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	//------------------------------------------------------------------

	FScalarRotationSimilarityReward::FScalarRotationSimilarityReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Rotation0Handle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Rotation0") }, { InMaxInstanceNum }, 0.0f);
		Rotation1Handle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Rotation1") }, { InMaxInstanceNum }, 0.0f);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarRotationSimilarityReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarRotationSimilarityReward::Evaluate);

		const TLearningArrayView<1, const float> Rotation0 = InstanceData->ConstView(Rotation0Handle);
		const TLearningArrayView<1, const float> Rotation1 = InstanceData->ConstView(Rotation1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::ScalarRotationSimilarityReward(
				Rotation0[InstanceIdx],
				Rotation1[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	//------------------------------------------------------------------

	FScalarAngularVelocitySimilarityReward::FScalarAngularVelocitySimilarityReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		AngularVelocity0Handle = InstanceData->Add<1, float>({ InIdentifier, TEXT("AngularVelocity0") }, { InMaxInstanceNum }, 0.0f);
		AngularVelocity1Handle = InstanceData->Add<1, float>({ InIdentifier, TEXT("AngularVelocity1") }, { InMaxInstanceNum }, 0.0f);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FScalarAngularVelocitySimilarityReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FScalarAngularVelocitySimilarityReward::Evaluate);

		const TLearningArrayView<1, const float> AngularVelocity0 = InstanceData->ConstView(AngularVelocity0Handle);
		const TLearningArrayView<1, const float> AngularVelocity1 = InstanceData->ConstView(AngularVelocity1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::ScalarAngularVelocitySimilarityReward(
				AngularVelocity0[InstanceIdx],
				AngularVelocity1[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	//------------------------------------------------------------------

	FScalarVelocityReward::FScalarVelocityReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		VelocityHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Velocity") }, { InMaxInstanceNum }, 0.0f);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
	}

	void FScalarVelocityReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FPlanarVelocityMagnitudeReward::Evaluate);

		const TLearningArrayView<1, const float> Velocity = InstanceData->ConstView(VelocityHandle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::ScalarVelocityReward(
				Velocity[InstanceIdx],
				Scale[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	FLocalDirectionalVelocityReward::FLocalDirectionalVelocityReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const FVector InAxis)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, Axis(InAxis)
	{
		VelocityHandle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Velocity") }, { InMaxInstanceNum }, FVector::ZeroVector);
		RelativeRotationHandle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("RelativeRotation") }, { InMaxInstanceNum }, FQuat::Identity);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
	}

	void FLocalDirectionalVelocityReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FLocalDirectionalVelocityReward::Evaluate);

		const TLearningArrayView<1, const FVector> Velocity = InstanceData->ConstView(VelocityHandle);
		const TLearningArrayView<1, const FQuat> RelativeRotation = InstanceData->ConstView(RelativeRotationHandle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::LocalDirectionalVelocityReward(
				Velocity[InstanceIdx],
				RelativeRotation[InstanceIdx],
				Scale[InstanceIdx],
				Weight[InstanceIdx],
				Axis);
		}
	}

	//------------------------------------------------------------------

	FPlanarPositionDifferencePenalty::FPlanarPositionDifferencePenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold,
		const FVector InAxis0,
		const FVector InAxis1)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
		, Axis0(InAxis0)
		, Axis1(InAxis1)
	{
		Position0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum }, FVector::ZeroVector);
		Position1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum }, FVector::ZeroVector);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FPlanarPositionDifferencePenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FPlanarPositionDifferencePenalty::Evaluate);

		const TLearningArrayView<1, const FVector> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<1, const FVector> Position1 = InstanceData->ConstView(Position1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::PlanarPositionDifferencePenalty(
				Position0[InstanceIdx],
				Position1[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx],
				Axis0,
				Axis1);
		}
	}

	FPositionDifferencePenalty::FPositionDifferencePenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Position0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum }, FVector::ZeroVector);
		Position1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum }, FVector::ZeroVector);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FPositionDifferencePenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FPositionDifferencePenalty::Evaluate);

		const TLearningArrayView<1, const FVector> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<1, const FVector> Position1 = InstanceData->ConstView(Position1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::PositionDifferencePenalty(
				Position0[InstanceIdx],
				Position1[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}


	//------------------------------------------------------------------

	FVelocityDifferencePenalty::FVelocityDifferencePenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Velocity0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Velocity0") }, { InMaxInstanceNum }, FVector::ZeroVector);
		Velocity1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Velocity1") }, { InMaxInstanceNum }, FVector::ZeroVector);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FVelocityDifferencePenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FVelocityDifferencePenalty::Evaluate);

		const TLearningArrayView<1, const FVector> Velocity0 = InstanceData->ConstView(Velocity0Handle);
		const TLearningArrayView<1, const FVector> Velocity1 = InstanceData->ConstView(Velocity1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::VelocityDifferencePenalty(
				Velocity0[InstanceIdx],
				Velocity1[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	//------------------------------------------------------------------

	FFacingDirectionAngularDifferencePenalty::FFacingDirectionAngularDifferencePenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const FVector InFacingDirection0,
		const FVector InFacingDirection1,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Rotation0Handle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("Rotation0") }, { InMaxInstanceNum }, FQuat::Identity);
		Rotation1Handle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("Rotation1") }, { InMaxInstanceNum }, FQuat::Identity);
		FacingDirection0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("FacingDirection0") }, { InMaxInstanceNum }, InFacingDirection0);
		FacingDirection1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("FacingDirection1") }, { InMaxInstanceNum }, InFacingDirection1);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FFacingDirectionAngularDifferencePenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FFacingDirectionAngularDifferencePenalty::Evaluate);

		const TLearningArrayView<1, const FQuat> Rotation0 = InstanceData->ConstView(Rotation0Handle);
		const TLearningArrayView<1, const FQuat> Rotation1 = InstanceData->ConstView(Rotation1Handle);
		const TLearningArrayView<1, const FVector> FacingDirection0 = InstanceData->ConstView(FacingDirection0Handle);
		const TLearningArrayView<1, const FVector> FacingDirection1 = InstanceData->ConstView(FacingDirection1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::DirectionAngleDifferencePenalty(
				Rotation0[InstanceIdx].RotateVector(FacingDirection0[InstanceIdx]),
				Rotation1[InstanceIdx].RotateVector(FacingDirection1[InstanceIdx]),
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	FFacingTowardsTargetAngularDifferencePenalty::FFacingTowardsTargetAngularDifferencePenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const FVector InFacingDirection,
		const float InMinRadius,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		TargetHandle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Target") }, { InMaxInstanceNum }, FVector::ZeroVector);
		PositionHandle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position") }, { InMaxInstanceNum }, FVector::ZeroVector);
		RotationHandle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("Rotation") }, { InMaxInstanceNum }, FQuat::Identity);
		FacingDirectionHandle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("FacingDirection") }, { InMaxInstanceNum }, InFacingDirection);
		MinRadiusHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("MinRadius") }, { InMaxInstanceNum }, InMinRadius);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FFacingTowardsTargetAngularDifferencePenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FFacingTowardsTargetAngularDifferencePenalty::Evaluate);

		const TLearningArrayView<1, const FVector> Target = InstanceData->ConstView(TargetHandle);
		const TLearningArrayView<1, const FVector> Position = InstanceData->ConstView(PositionHandle);
		const TLearningArrayView<1, const FQuat> Rotation = InstanceData->ConstView(RotationHandle);
		const TLearningArrayView<1, const FVector> FacingDirection = InstanceData->ConstView(FacingDirectionHandle);
		const TLearningArrayView<1, const float> MinRadius = InstanceData->ConstView(MinRadiusHandle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			UE_LEARNING_ARRAY_VALUE_CHECK(MinRadius[InstanceIdx] > 0.0f);

			const FVector TargetDifference = Target[InstanceIdx] - Position[InstanceIdx];

			if (TargetDifference.Length() > FMath::Max(MinRadius[InstanceIdx], UE_SMALL_NUMBER))
			{
				Reward[InstanceIdx] = Reward::DirectionAngleDifferencePenalty(
					Rotation[InstanceIdx].RotateVector(FacingDirection[InstanceIdx]),
					TargetDifference.GetUnsafeNormal(),
					Scale[InstanceIdx],
					Threshold[InstanceIdx],
					Weight[InstanceIdx]);
			}
			else
			{
				Reward[InstanceIdx] = 0.0f;
			}
		}
	}

	//------------------------------------------------------------------

	FSpringInertializationPenalty::FSpringInertializationPenalty(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Position0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position0") }, { InMaxInstanceNum }, FVector::ZeroVector);
		Position1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position1") }, { InMaxInstanceNum }, FVector::ZeroVector);
		Velocity0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Velocity0") }, { InMaxInstanceNum }, FVector::ZeroVector);
		Velocity1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Velocity1") }, { InMaxInstanceNum }, FVector::ZeroVector);

		HalfLifeHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("HalfLife") }, { InMaxInstanceNum }, 0.1f);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FSpringInertializationPenalty::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FSpringInertializationPenalty::Evaluate);

		const TLearningArrayView<1, const FVector> Position0 = InstanceData->ConstView(Position0Handle);
		const TLearningArrayView<1, const FVector> Position1 = InstanceData->ConstView(Position1Handle);
		const TLearningArrayView<1, const FVector> Velocity0 = InstanceData->ConstView(Velocity0Handle);
		const TLearningArrayView<1, const FVector> Velocity1 = InstanceData->ConstView(Velocity1Handle);

		const TLearningArrayView<1, const float> HalfLife = InstanceData->ConstView(HalfLifeHandle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = Reward::SpringInertializationPenalty(
				Position0[InstanceIdx],
				Position1[InstanceIdx],
				Velocity0[InstanceIdx],
				Velocity1[InstanceIdx],
				HalfLife[InstanceIdx],
				Scale[InstanceIdx],
				Threshold[InstanceIdx],
				Weight[InstanceIdx]);
		}
	}

	//------------------------------------------------------------------

	FPositionArraySimilarityReward::FPositionArraySimilarityReward(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InPositionNum,
		const float InWeight,
		const float InScale,
		const float InThreshold)
		: FRewardObject(InIdentifier, InInstanceData, InMaxInstanceNum)
	{
		Positions0Handle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Positions0") }, { InMaxInstanceNum, InPositionNum }, FVector::ZeroVector);
		Positions1Handle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Positions1") }, { InMaxInstanceNum, InPositionNum }, FVector::ZeroVector);
		RelativePosition0Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("RelativePosition0") }, { InMaxInstanceNum }, FVector::ZeroVector);
		RelativePosition1Handle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("RelativePosition1") }, { InMaxInstanceNum }, FVector::ZeroVector);
		RelativeRotation0Handle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("RelativeRotation0") }, { InMaxInstanceNum }, FQuat::Identity);
		RelativeRotation1Handle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("RelativeRotation1") }, { InMaxInstanceNum }, FQuat::Identity);

		WeightHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Weight") }, { InMaxInstanceNum }, InWeight);
		ScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Scale") }, { InMaxInstanceNum }, InScale);
		ThresholdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Threshold") }, { InMaxInstanceNum }, InThreshold);
	}

	void FPositionArraySimilarityReward::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(FPositionArraySimilarityReward::Evaluate);

		const TLearningArrayView<2, const FVector> Positions0 = InstanceData->ConstView(Positions0Handle);
		const TLearningArrayView<2, const FVector> Positions1 = InstanceData->ConstView(Positions1Handle);
		const TLearningArrayView<1, const FVector> RelativePosition0 = InstanceData->ConstView(RelativePosition0Handle);
		const TLearningArrayView<1, const FVector> RelativePosition1 = InstanceData->ConstView(RelativePosition1Handle);
		const TLearningArrayView<1, const FQuat> RelativeRotation0 = InstanceData->ConstView(RelativeRotation0Handle);
		const TLearningArrayView<1, const FQuat> RelativeRotation1 = InstanceData->ConstView(RelativeRotation1Handle);

		const TLearningArrayView<1, const float> Scale = InstanceData->ConstView(ScaleHandle);
		const TLearningArrayView<1, const float> Threshold = InstanceData->ConstView(ThresholdHandle);
		const TLearningArrayView<1, const float> Weight = InstanceData->ConstView(WeightHandle);

		TLearningArrayView<1, float> Reward = InstanceData->View(RewardHandle);

		const int32 PositionNum = Positions0.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Reward[InstanceIdx] = 0.0f;

			for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
			{
				Reward[InstanceIdx] += Reward::PositionSimilarityReward(
					RelativeRotation0[InstanceIdx].UnrotateVector(Positions0[InstanceIdx][PositionIdx] - RelativePosition0[InstanceIdx]),
					RelativeRotation1[InstanceIdx].UnrotateVector(Positions1[InstanceIdx][PositionIdx] - RelativePosition1[InstanceIdx]),
					Scale[InstanceIdx],
					Threshold[InstanceIdx],
					Weight[InstanceIdx]) / PositionNum;
			}
		}
	}

	//------------------------------------------------------------------

}
