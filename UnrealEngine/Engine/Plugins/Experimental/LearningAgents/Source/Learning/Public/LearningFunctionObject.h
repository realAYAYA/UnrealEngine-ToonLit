// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArrayMap.h"

#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	/**
	* Base class for an object which outputs to some arrays stored in
	* a `FArrayMap`, using other arrays in the `FArrayMap` as input. 
	* Here, all data is assumed to be stored in a `FArrayMap` object 
	* to make the processing of multiple instances efficient.
	*/
	struct LEARNING_API FFunctionObject
	{
		FFunctionObject(const TSharedRef<FArrayMap>& InInstanceData);
		virtual ~FFunctionObject() {}

		virtual void Evaluate(const FIndexSet Instances) = 0;

		TSharedRef<FArrayMap> InstanceData;
	};

	/**
	* Executes another set of function objects in sequence
	*/
	struct LEARNING_API FSequentialFunction : public FFunctionObject
	{
		FSequentialFunction(
			const TLearningArrayView<1, const TSharedRef<FFunctionObject>> InFunctions,
			const TSharedRef<FArrayMap>& InInstanceData);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TLearningArray<1, TSharedRef<FFunctionObject>, TInlineAllocator<32>> Functions;
	};

	/**
	* Copies vector data from one array to another
	*/
	struct LEARNING_API FCopyVectorsFunction : public FFunctionObject
	{
		FCopyVectorsFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InDimensionsNum);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> InputHandle;
		TArrayMapHandle<2, float> OutputHandle;
	};

	/**
	* Computes rotations given transforms
	*/
	struct LEARNING_API FExtractRotationsFromTransformsFunction : public FFunctionObject
	{
		FExtractRotationsFromTransformsFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<1, FTransform> TransformHandle;
		TArrayMapHandle<1, FQuat> RotationHandle;
	};

	/**
	* Computes positions and rotations given transforms
	*/
	struct LEARNING_API FExtractPositionsAndRotationsFromTransformsFunction : public FFunctionObject
	{
		FExtractPositionsAndRotationsFromTransformsFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<1, FTransform> TransformHandle;
		TArrayMapHandle<1, FQuat> RotationHandle;
		TArrayMapHandle<1, FVector> PositionHandle;
	};

	/**
	* Samples scalar values from a uniform distribution
	*/
	struct LEARNING_API FRandomUniformFunction : public FFunctionObject
	{
		FRandomUniformFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InValueNum,
			const uint32 InSeed,
			const float InMin = 0.0f,
			const float InMax = 1.0f);

		virtual void Evaluate(const FIndexSet Instances) override final;

		TArrayMapHandle<1, uint32> SeedHandle;
		TArrayMapHandle<1, float> MinHandle;
		TArrayMapHandle<1, float> MaxHandle;
		TArrayMapHandle<2, float> ValueHandle;
	};

	/**
	* Samples values from a planar clipped Gaussian distribution.
	*/
	struct LEARNING_API FRandomPlanarClippedGaussianFunction : public FFunctionObject
	{
		FRandomPlanarClippedGaussianFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InValueNum,
			const uint32 InSeed,
			const float InMean = 0.0f,
			const float InStd = 1.0f,
			const float InClip = 10.0f,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual void Evaluate(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<1, uint32> SeedHandle;
		TArrayMapHandle<1, float> MeanHandle;
		TArrayMapHandle<1, float> StdHandle;
		TArrayMapHandle<1, float> ClipHandle;
		TArrayMapHandle<2, FVector> ValueHandle;
	};

	/**
	* Samples planar directions.
	*/
	struct LEARNING_API FRandomPlanarDirectionFunction : public FFunctionObject
	{
		FRandomPlanarDirectionFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InDirectionNum,
			const uint32 InSeed,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual void Evaluate(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<1, uint32> SeedHandle;
		TArrayMapHandle<2, FVector> DirectionHandle;
	};

	/**
	* Samples planar directions with corresponding velocities.
	*/
	struct LEARNING_API FRandomPlanarDirectionVelocityFunction : public FFunctionObject
	{
		FRandomPlanarDirectionVelocityFunction(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InDirectionVelocityNum,
			const uint32 InSeed,
			const float InVelocityScale = 1.0f,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual void Evaluate(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<1, uint32> SeedHandle;
		TArrayMapHandle<1, float> VelocityScaleHandle;
		TArrayMapHandle<2, FVector> DirectionHandle;
		TArrayMapHandle<2, FVector> VelocityHandle;
	};

}




