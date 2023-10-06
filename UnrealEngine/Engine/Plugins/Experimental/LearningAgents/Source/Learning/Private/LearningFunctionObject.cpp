// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningFunctionObject.h"

#include "LearningRandom.h"

namespace UE::Learning
{
	FFunctionObject::FFunctionObject(const TSharedRef<FArrayMap>& InInstanceData) : InstanceData(InInstanceData) {}

	FSequentialFunction::FSequentialFunction(
		const TLearningArrayView<1, const TSharedRef<FFunctionObject>> InFunctions,
		const TSharedRef<FArrayMap>& InInstanceData)
		: FFunctionObject(InInstanceData)
		, Functions(InFunctions) {}

	void FSequentialFunction::Evaluate(const FIndexSet Instances)
	{
		for (const TSharedRef<FFunctionObject>& Function : Functions)
		{
			Function->Evaluate(Instances);
		}
	}

	FCopyVectorsFunction::FCopyVectorsFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InDimensionsNum)
		: FFunctionObject(InInstanceData)
	{
		InputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum, InDimensionsNum }, 0.0f);
		OutputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Min") }, { InMaxInstanceNum, InDimensionsNum }, 0.0f);
	}

	void FCopyVectorsFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FCopyVectorsFunction::Evaluate);

		const TLearningArrayView<2, const float> Input = InstanceData->ConstView(InputHandle);
		TLearningArrayView<2, float> Output = InstanceData->View(OutputHandle);

		Array::Copy(Output, Input, Instances);
	}

	FExtractRotationsFromTransformsFunction::FExtractRotationsFromTransformsFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum)
		: FFunctionObject(InInstanceData)
	{
		TransformHandle = InstanceData->Add<1, FTransform>({ InIdentifier, TEXT("Transform") }, { InMaxInstanceNum }, FTransform::Identity);
		RotationHandle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("Rotation") }, { InMaxInstanceNum }, FQuat::Identity);
	}

	void FExtractRotationsFromTransformsFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FExtractRotationsFromTransformsFunction::Evaluate);

		const TLearningArrayView<1, const FTransform> Transform = InstanceData->ConstView(TransformHandle);
		TLearningArrayView<1, FQuat> Rotation = InstanceData->View(RotationHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Rotation[InstanceIdx] = Transform[InstanceIdx].GetRotation();
		}
	}

	FExtractPositionsAndRotationsFromTransformsFunction::FExtractPositionsAndRotationsFromTransformsFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum)
		: FFunctionObject(InInstanceData)
	{
		TransformHandle = InstanceData->Add<1, FTransform>({ InIdentifier, TEXT("Transform") }, { InMaxInstanceNum }, FTransform::Identity);
		RotationHandle = InstanceData->Add<1, FQuat>({ InIdentifier, TEXT("Rotation") }, { InMaxInstanceNum }, FQuat::Identity);
		PositionHandle = InstanceData->Add<1, FVector>({ InIdentifier, TEXT("Position") }, { InMaxInstanceNum }, FVector::ZeroVector);
	}

	void FExtractPositionsAndRotationsFromTransformsFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FExtractPositionsAndRotationsFromTransformsFunction::Evaluate);

		const TLearningArrayView<1, const FTransform> Transform = InstanceData->ConstView(TransformHandle);
		TLearningArrayView<1, FQuat> Rotation = InstanceData->View(RotationHandle);
		TLearningArrayView<1, FVector> Position = InstanceData->View(PositionHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Rotation[InstanceIdx] = Transform[InstanceIdx].GetRotation();
			Position[InstanceIdx] = Transform[InstanceIdx].GetTranslation();
		}
	}

	FRandomUniformFunction::FRandomUniformFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InValueNum,
		const uint32 InSeed,
		const float InMin,
		const float InMax)
		: FFunctionObject(InInstanceData)
	{
		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		MinHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Min") }, { InMaxInstanceNum }, InMin);
		MaxHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Max") }, { InMaxInstanceNum }, InMax);
		ValueHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Value") }, { InMaxInstanceNum, InValueNum }, 0.0f);

		Random::IntArray(InstanceData->View(SeedHandle), InSeed);
	}

	void FRandomUniformFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FRandomUniformFunction::Evaluate);

		const TLearningArrayView<1, const float> Min = InstanceData->ConstView(MinHandle);
		const TLearningArrayView<1, const float> Max = InstanceData->ConstView(MaxHandle);
		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);
		TLearningArrayView<2, float> Values = InstanceData->View(ValueHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Random::SampleUniformArray(
				Values[InstanceIdx],
				Seed[InstanceIdx],
				Min[InstanceIdx],
				Max[InstanceIdx]);
		}
	}

	FRandomPlanarClippedGaussianFunction::FRandomPlanarClippedGaussianFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InValueNum,
		const uint32 InSeed,
		const float InMean,
		const float InStd,
		const float InClip,
		const FVector InAxis0,
		const FVector InAxis1)
		: FFunctionObject(InInstanceData)
		, Axis0(InAxis0)
		, Axis1(InAxis1)
	{
		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		MeanHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Mean") }, { InMaxInstanceNum }, InMean);
		StdHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Std") }, { InMaxInstanceNum }, InStd);
		ClipHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Clip") }, { InMaxInstanceNum }, InClip);
		ValueHandle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Value") }, { InMaxInstanceNum, InValueNum }, FVector::ZeroVector);

		Random::IntArray(InstanceData->View(SeedHandle), InSeed);
	}

	void FRandomPlanarClippedGaussianFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FRandomPlanarClippedGaussianFunction::Evaluate);

		const TLearningArrayView<1, const float> Mean = InstanceData->ConstView(MeanHandle);
		const TLearningArrayView<1, const float> Std = InstanceData->ConstView(StdHandle);
		const TLearningArrayView<1, const float> Clip = InstanceData->ConstView(ClipHandle);
		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);
		TLearningArrayView<2, FVector> Values = InstanceData->View(ValueHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Random::SamplePlanarClippedGaussianArray(
				Values[InstanceIdx],
				Seed[InstanceIdx],
				Mean[InstanceIdx],
				Std[InstanceIdx],
				Clip[InstanceIdx],
				Axis0,
				Axis1);
		}
	}

	FRandomPlanarDirectionFunction::FRandomPlanarDirectionFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InDirectionNum,
		const uint32 InSeed,
		const FVector InAxis0,
		const FVector InAxis1)
		: FFunctionObject(InInstanceData)
		, Axis0(InAxis0)
		, Axis1(InAxis1)
	{
		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		DirectionHandle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Direction") }, { InMaxInstanceNum, InDirectionNum }, FVector::ForwardVector);

		Random::IntArray(InstanceData->View(SeedHandle), InSeed);
	}

	void FRandomPlanarDirectionFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FRandomPlanarDirectionFunction::Evaluate);

		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);
		TLearningArrayView<2, FVector> Direction = InstanceData->View(DirectionHandle);

		for (const int32 InstanceIdx : Instances)
		{
			Random::SamplePlanarDirectionArray(
				Direction[InstanceIdx], 
				Seed[InstanceIdx],
				Axis0,
				Axis1);
		}
	}

	FRandomPlanarDirectionVelocityFunction::FRandomPlanarDirectionVelocityFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const int32 InDirectionVelocityNum,
		const uint32 InSeed,
		const float InVelocityScale,
		const FVector InAxis0,
		const FVector InAxis1)
		: FFunctionObject(InInstanceData)
		, Axis0(InAxis0)
		, Axis1(InAxis1)
	{
		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		VelocityScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("VelocityScale") }, { InMaxInstanceNum }, InVelocityScale);
		DirectionHandle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Direction") }, { InMaxInstanceNum, InDirectionVelocityNum }, FVector::ForwardVector);
		VelocityHandle = InstanceData->Add<2, FVector>({ InIdentifier, TEXT("Velocity") }, { InMaxInstanceNum, InDirectionVelocityNum }, FVector::ZeroVector);

		Random::IntArray(InstanceData->View(SeedHandle), InSeed);
	}

	void FRandomPlanarDirectionVelocityFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FRandomPlanarDirectionVelocityFunction::Evaluate);

		const TLearningArrayView<1, const float> VelocityScale = InstanceData->ConstView(VelocityScaleHandle);
		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);
		TLearningArrayView<2, FVector> Direction = InstanceData->View(DirectionHandle);
		TLearningArrayView<2, FVector> Velocity = InstanceData->View(VelocityHandle);

		const int32 VelocityNum = Velocity.Num<1>();

		for (const int32 InstanceIdx : Instances)
		{
			Random::SamplePlanarDirectionArray(
				Direction[InstanceIdx],
				Seed[InstanceIdx],
				Axis0,
				Axis1);

			for (int32 VelocityIdx = 0; VelocityIdx < VelocityNum; VelocityIdx++)
			{
				Velocity[InstanceIdx][VelocityIdx] = VelocityScale[InstanceIdx] * Direction[InstanceIdx][VelocityIdx];
			}
		}
	}

}

