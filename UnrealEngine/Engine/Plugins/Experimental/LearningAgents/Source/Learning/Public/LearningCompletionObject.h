// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArrayMap.h"
#include "LearningCompletion.h"

#include "Math/Vector.h"
#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	namespace Completion
	{
		static constexpr float DefaultThresholdPosition = 100.0f; // cm
		static constexpr float DefaultThresholdVelocity = 200.0f; // cm/s
		static constexpr float DefaultThresholdAngle = UE_PI / 2.0f; // rad
		static constexpr float DefaultThresholdAngularVelocity = UE_PI; // rad/s
		static constexpr float DefaultThresholdHeight = 0.0f; // cm
		static constexpr float DefaultThresholdTime = 10.0f; // s
	}

	//------------------------------------------------------------------

	/**
	* Base class for an object which computes if an instance should be completion 
	* from a set of other arrays. Here, all data is assumed to be stored in a 
	* `FArrayMap` object to make the processing of multiple instances efficient.
	*/
	struct LEARNING_API FCompletionObject
	{
		FCompletionObject(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual ~FCompletionObject() {}

		virtual void Evaluate(const FIndexSet Instances) = 0;

		TLearningArrayView<1, ECompletionMode> CompletionBuffer();

		TSharedRef<FArrayMap> InstanceData;
		ECompletionMode CompletionMode;

		TArrayMapHandle<1, ECompletionMode> CompletionHandle;
	};

	//------------------------------------------------------------------

	/**
	* Terminates if any of the provided completions terminate
	*/
	struct LEARNING_API FAnyCompletion : public FCompletionObject
	{
		FAnyCompletion(
			const FName& InIdentifier,
			const TLearningArrayView<1, const TSharedRef<FCompletionObject>> InCompletions,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TLearningArray<1, TSharedRef<FCompletionObject>, TInlineAllocator<32>> Completions;
	};

	//------------------------------------------------------------------

	/**
	* Completion due to some condition being true
	*/
	struct LEARNING_API FConditionalCompletion : public FCompletionObject
	{
		FConditionalCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<1, bool> ConditionHandle;
	};

	//------------------------------------------------------------------

	/**
	* Completion due to time having elapsed
	*/
	struct LEARNING_API FTimeElapsedCompletion : public FCompletionObject
	{
		FTimeElapsedCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const float InThreshold = Completion::DefaultThresholdTime,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<1, float> TimeHandle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	//------------------------------------------------------------------

	/**
	* Completion due to the difference between any two scalar positions exceeding a given threshold
	*/
	struct LEARNING_API FScalarPositionDifferenceCompletion : public FCompletionObject
	{
		FScalarPositionDifferenceCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InPositionNum,
			const float InThreshold = Completion::DefaultThresholdPosition,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> Position0Handle;
		TArrayMapHandle<2, float> Position1Handle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	/**
	* Completion due to the difference between any two planar positions exceeding a given threshold
	*/
	struct LEARNING_API FPlanarPositionDifferenceCompletion : public FCompletionObject
	{
		FPlanarPositionDifferenceCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InPositionNum,
			const float InThreshold = Completion::DefaultThresholdPosition,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual void Evaluate(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<2, FVector> Position0Handle;
		TArrayMapHandle<2, FVector> Position1Handle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	/**
	* Completion due to the similarity between any two planar positions being below a given threshold
	*/
	struct LEARNING_API FPlanarPositionSimilarityCompletion : public FCompletionObject
	{
		FPlanarPositionSimilarityCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InPositionNum,
			const float InThreshold = Completion::DefaultThresholdPosition,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual void Evaluate(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<2, FVector> Position0Handle;
		TArrayMapHandle<2, FVector> Position1Handle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	//------------------------------------------------------------------

	/**
	* Completion due to the difference between any two scalar velocities exceeding a given threshold
	*/
	struct LEARNING_API FScalarVelocityDifferenceCompletion : public FCompletionObject
	{
		FScalarVelocityDifferenceCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InVelocityNum,
			const float InThreshold = Completion::DefaultThresholdVelocity,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> Velocity0Handle;
		TArrayMapHandle<2, float> Velocity1Handle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	//------------------------------------------------------------------

	/**
	* Completion due to the difference between any two scalar rotations exceeding a given threshold
	*/
	struct LEARNING_API FScalarRotationDifferenceCompletion : public FCompletionObject
	{
		FScalarRotationDifferenceCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InRotationNum,
			const float InThreshold = Completion::DefaultThresholdAngle,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> Rotation0Handle;
		TArrayMapHandle<2, float> Rotation1Handle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	//------------------------------------------------------------------

	/**
	* Completion due to the difference between two scalar angular velocities exceeding a given threshold
	*/
	struct LEARNING_API FScalarAngularVelocityDifferenceCompletion : public FCompletionObject
	{
		FScalarAngularVelocityDifferenceCompletion(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngularVelocityNum,
			const float InThreshold = Completion::DefaultThresholdAngularVelocity,
			const ECompletionMode InCompletionMode = ECompletionMode::Terminated);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> AngularVelocity0Handle;
		TArrayMapHandle<2, float> AngularVelocity1Handle;
		TArrayMapHandle<1, float> ThresholdHandle;
	};

	//------------------------------------------------------------------
}
