// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArrayMap.h"

#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Templates/SharedPointer.h"

namespace UE::Learning
{
	namespace Feature
	{
		static constexpr float DefaultScale = 1.0f;
		static constexpr float DefaultScaleTime = 1.0f; // s
		static constexpr float DefaultScaleDirection = 1.0f;

		static constexpr float DefaultScalePosition = 100.0f; // cm
		static constexpr float DefaultScaleVelocity = 200.0f; // cm/s
		static constexpr float DefaultScaleAcceleration = 400.0f; // cm/s^2

		static constexpr float DefaultScaleAngle = UE_PI / 2.0f; // rad
		static constexpr float DefaultScaleAngularVelocity = UE_PI; // rad/s
		static constexpr float DefaultScaleAngularAcceleration = 2.0f * UE_PI; // rad/s

		static constexpr float DefaultScaleScale = 1.0f;

		namespace Encode
		{
			LEARNING_API void Float(
				float& Output,
				const float Input,
				const float Scale = DefaultScale,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void Time(
				float& Output,
				const float Time,
				const float RelativeTime = 0.0f,
				const float Scale = DefaultScaleTime,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void PlanarDirection(
				TLearningArrayView<1, float> Output,
				const FVector Direction,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleDirection,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void Direction(
				TLearningArrayView<1, float> Output,
				const FVector Direction,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleDirection,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void ScalarPosition(
				float& Output,
				const float Position,
				const float RelativePosition = 0.0f,
				const float Scale = DefaultScalePosition,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void PlanarPosition(
				TLearningArrayView<1, float> Output,
				const FVector Position,
				const FVector RelativePosition = FVector::ZeroVector,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScalePosition,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void Position(
				TLearningArrayView<1, float> Output,
				const FVector Position,
				const FVector RelativePosition = FVector::ZeroVector,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScalePosition,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void ScalarVelocity(
				float& Output,
				const float Velocity,
				const float Scale = DefaultScaleVelocity,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void PlanarVelocity(
				TLearningArrayView<1, float> Output,
				const FVector Velocity,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleVelocity,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void Velocity(
				TLearningArrayView<1, float> Output,
				const FVector Velocity,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleVelocity,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void ScalarAcceleration(
				float& Output,
				const float Acceleration,
				const float Scale = DefaultScaleAcceleration,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void PlanarAcceleration(
				TLearningArrayView<1, float> Output,
				const FVector Acceleration,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAcceleration,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void Acceleration(
				TLearningArrayView<1, float> Output,
				const FVector Acceleration,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAcceleration,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void Angle(
				TLearningArrayView<1, float> Output,
				const float Angle,
				const float RelativeRotation = 0.0f,
				const float Scale = DefaultScaleDirection, // angles are encoded as directions
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void Rotation(
				TLearningArrayView<1, float> Output,
				const FQuat Rotation,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleDirection, // rotations are encoded as directions
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void RotationVector(
				TLearningArrayView<1, float> Output,
				const FVector RotationVector,
				const float Scale = DefaultScaleAngle,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void ScalarAngularVelocity(
				float& Output,
				const float AngularVelocity,
				const float Scale = DefaultScaleAngularVelocity,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void AngularVelocity(
				TLearningArrayView<1, float> Output,
				const FVector AngularVelocity,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAngularVelocity,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void ScalarAngularAcceleration(
				float& Output,
				const float AngularAcceleration,
				const float Scale = DefaultScaleAngularAcceleration,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void PlanarAngularAcceleration(
				float& Output,
				const FVector AngularAcceleration,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAngularAcceleration,
				const FVector Axis = FVector::UpVector,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void AngularAcceleration(
				TLearningArrayView<1, float> Output,
				const FVector AngularAcceleration,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale =DefaultScaleAngularAcceleration,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void Scale(
				float& Output,
				const float InScale,
				const float RelativeScale = 1.0f,
				const float Scale = DefaultScaleScale,
				const float Epsilon = UE_SMALL_NUMBER);
		}

		namespace Decode
		{
			LEARNING_API void Float(
				float& Value,
				const TLearningArrayView<1, const float> Input,
				const float Scale = DefaultScale);


			LEARNING_API void Time(
				float& OutTime,
				const TLearningArrayView<1, const float> Input,
				const float RelativeTime,
				const float Scale = DefaultScaleTime);


			LEARNING_API void PlanarDirection(
				FVector& OutPlanarDirection,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleDirection,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector,
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void Direction(
				FVector& OutDirection,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleDirection,
				const float Epsilon = UE_SMALL_NUMBER);


			LEARNING_API void ScalarPosition(
				float& OutScalarPosition,
				const float Input,
				const float RelativePosition = 0.0f,
				const float Scale = DefaultScalePosition);

			LEARNING_API void PlanarPosition(
				FVector& OutPlanarPosition,
				const TLearningArrayView<1, const float> Input,
				const FVector RelativePosition = FVector::ZeroVector,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScalePosition,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector);

			LEARNING_API void Position(
				FVector& OutPosition,
				const TLearningArrayView<1, const float> Input,
				const FVector RelativePosition = FVector::ZeroVector,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScalePosition);


			LEARNING_API void ScalarVelocity(
				float& OutScalarVelocity,
				const float Input,
				const float Scale = DefaultScaleVelocity);

			LEARNING_API void PlanarVelocity(
				FVector& OutPlanarVelocity,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleVelocity,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector);

			LEARNING_API void Velocity(
				FVector& OutVelocity,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleVelocity);


			LEARNING_API void ScalarAcceleration(
				float& OutScalarAcceleration,
				const float Input,
				const float Scale = DefaultScaleAcceleration);

			LEARNING_API void PlanarAcceleration(
				FVector& OutPlanarAcceleration,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAcceleration,
				const FVector Axis0 = FVector::ForwardVector,
				const FVector Axis1 = FVector::RightVector);

			LEARNING_API void Acceleration(
				FVector& OutAcceleration,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAcceleration);


			LEARNING_API void Angle(
				float& OutScalarRotation,
				const TLearningArrayView<1, const float> Input,
				const float RelativeRotation = 0.0f,
				const float Scale = DefaultScaleDirection); // angles are encoded as directions

			LEARNING_API void Rotation(
				FQuat& OutRotation,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleDirection, // rotations are encoded as directions
				const float Epsilon = UE_SMALL_NUMBER);

			LEARNING_API void RotationVector(
				FVector& OutRotationVector,
				const TLearningArrayView<1, const float> Input,
				const float Scale = DefaultScaleAngle);


			LEARNING_API void ScalarAngularVelocity(
				float& OutScalarVelocity,
				const float Input,
				const float Scale = DefaultScaleAngularVelocity);

			LEARNING_API void AngularVelocity(
				FVector& OutAngularVelocity,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAngularVelocity);


			LEARNING_API void ScalarAngularAcceleration(
				float& OutScalarAngularAcceleration,
				const float Input,
				const float Scale = DefaultScaleAngularAcceleration);

			LEARNING_API void PlanarAngularAcceleration(
				FVector& OutPlanarAngularAcceleration,
				const float Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAngularAcceleration,
				const FVector Axis = FVector::UpVector);

			LEARNING_API void AngularAcceleration(
				FVector& OutAngularAcceleration,
				const TLearningArrayView<1, const float> Input,
				const FQuat RelativeRotation = FQuat::Identity,
				const float Scale = DefaultScaleAngularAcceleration);


			LEARNING_API void Scale(
				float& OutScale,
				const float Input,
				const float RelativeScale = 1.0f,
				const float Scale = DefaultScaleScale,
				const float Epsilon = UE_SMALL_NUMBER);
		}
	}

	/**
	* Base class for an object which can do the encoding or decoding of
	* features from a set of other arrays. Here, all data is assumed
	* to be stored in a `FArrayMap` object to make the processing of 
	* multiple instances efficient.
	*/
	struct LEARNING_API FFeatureObject
	{
		FFeatureObject(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InFeatureDimNum,
			const float InScale = Feature::DefaultScale);

		virtual ~FFeatureObject() {}

		int32 DimNum() const;
		TLearningArrayView<2, float> FeatureBuffer();

		virtual bool IsEncodable() const = 0;
		virtual bool IsDecodable() const = 0;
		virtual void Encode(const FIndexSet Instances) = 0;
		virtual void Decode(const FIndexSet Instances) = 0;

		TSharedRef<FArrayMap> InstanceData;
		int32 FeatureDimNum = 0;
		float Scale = Feature::DefaultScale;
		TArrayMapHandle<2, float> FeatureHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature that concatenates multiple other features
	*/
	struct LEARNING_API FConcatenateFeature : public FFeatureObject
	{
		FConcatenateFeature(
			const FName& InIdentifier,
			const TLearningArrayView<1, const TSharedRef<FFeatureObject>> InFeatures,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const float InScale = Feature::DefaultScale);

		virtual bool IsEncodable() const override final;
		virtual bool IsDecodable() const override final;
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TLearningArray<1, TSharedRef<FFeatureObject>, TInlineAllocator<32>> Features;
		TLearningArray<1, int32, TInlineAllocator<32>> Offsets;
		TLearningArray<1, int32, TInlineAllocator<32>> Sizes;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of floats
	*/
	struct LEARNING_API FFloatFeature : public FFeatureObject
	{
		FFloatFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InFloatNum,
			const float InScale = Feature::DefaultScale);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> ValueHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of times
	*/
	struct LEARNING_API FTimeFeature : public FFeatureObject
	{
		FTimeFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InTimeNum,
			const float InScale = Feature::DefaultScaleTime);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> TimeHandle;
		TArrayMapHandle<1, float> RelativeTimeHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of planar directions
	*/
	struct LEARNING_API FPlanarDirectionFeature : public FFeatureObject
	{
		FPlanarDirectionFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InDirectionNum,
			const float InScale = Feature::DefaultScaleDirection,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<2, FVector> DirectionHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	/**
	* Feature for an array of directions
	*/
	struct LEARNING_API FDirectionFeature : public FFeatureObject
	{
		FDirectionFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InDirectionNum,
			const float InScale = Feature::DefaultScaleDirection);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> DirectionHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of scalar positions
	*/
	struct LEARNING_API FScalarPositionFeature : public FFeatureObject
	{
		FScalarPositionFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InPositionNum,
			const float InScale = Feature::DefaultScalePosition);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> PositionHandle;
		TArrayMapHandle<1, float> RelativePositionHandle;
	};

	/**
	* Feature for an array of planar positions
	*/
	struct LEARNING_API FPlanarPositionFeature : public FFeatureObject
	{
		FPlanarPositionFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InPositionNum,
			const float InScale = Feature::DefaultScalePosition,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<2, FVector> PositionHandle;
		TArrayMapHandle<1, FVector> RelativePositionHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	/**
	* Feature for an array of positions
	*/
	struct LEARNING_API FPositionFeature : public FFeatureObject
	{
		FPositionFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InPositionNum,
			const float InScale = Feature::DefaultScalePosition);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> PositionHandle;
		TArrayMapHandle<1, FVector> RelativePositionHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of scalar velocities
	*/
	struct LEARNING_API FScalarVelocityFeature : public FFeatureObject
	{
		FScalarVelocityFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InVelocityNum,
			const float InScale = Feature::DefaultScaleVelocity);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> VelocityHandle;
	};

	/**
	* Feature for an array of planar velocities
	*/
	struct LEARNING_API FPlanarVelocityFeature : public FFeatureObject
	{
		FPlanarVelocityFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InVelocityNum,
			const float InScale = Feature::DefaultScaleVelocity,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<2, FVector> VelocityHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	/**
	* Feature for an array of velocities
	*/
	struct LEARNING_API FVelocityFeature : public FFeatureObject
	{
		FVelocityFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InVelocityNum,
			const float InScale = Feature::DefaultScaleVelocity);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> VelocityHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of scalar accelerations
	*/
	struct LEARNING_API FScalarAccelerationFeature : public FFeatureObject
	{
		FScalarAccelerationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAccelerationNum,
			const float InScale = Feature::DefaultScaleAcceleration);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> AccelerationHandle;
	};

	/**
	* Feature for an array of planar accelerations
	*/
	struct LEARNING_API FPlanarAccelerationFeature : public FFeatureObject
	{
		FPlanarAccelerationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAccelerationNum,
			const float InScale = Feature::DefaultScaleAcceleration,
			const FVector InAxis0 = FVector::ForwardVector,
			const FVector InAxis1 = FVector::RightVector);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		FVector Axis0 = FVector::ForwardVector;
		FVector Axis1 = FVector::RightVector;

		TArrayMapHandle<2, FVector> AccelerationHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	/**
	* Feature for an array of accelerations
	*/
	struct LEARNING_API FAccelerationFeature : public FFeatureObject
	{
		FAccelerationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAccelerationNum,
			const float InScale = Feature::DefaultScaleAcceleration);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> AccelerationHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of angles
	*/
	struct LEARNING_API FAngleFeature : public FFeatureObject
	{
		FAngleFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngleNum,
			const float InScale = Feature::DefaultScaleDirection); // angles are encoded as directions

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> AngleHandle;
		TArrayMapHandle<1, float> RelativeAngleHandle;
	};

	/**
	* Feature for an array of rotations
	*/
	struct LEARNING_API FRotationFeature : public FFeatureObject
	{
		FRotationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InRotationNum,
			const float InScale = Feature::DefaultScaleDirection); // rotations are encoded as directions

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FQuat> RotationHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	/**
	* Feature for an array of rotation vectors
	*/
	struct LEARNING_API FRotationVectorFeature : public FFeatureObject
	{
		FRotationVectorFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InRotationVectorNum,
			const float InScale = Feature::DefaultScaleAngle);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> RotationVectorsHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of scalar angular velocities
	*/
	struct LEARNING_API FScalarAngularVelocityFeature : public FFeatureObject
	{
		FScalarAngularVelocityFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngularAccelerationNum,
			const float InScale = Feature::DefaultScaleAngularVelocity);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> AngularVelocityHandle;
	};

	/**
	* Feature for an array of angular velocities
	*/
	struct LEARNING_API FAngularVelocityFeature : public FFeatureObject
	{
		FAngularVelocityFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngularAccelerationNum,
			const float InScale = Feature::DefaultScaleAngularVelocity);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> AngularVelocityHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of scalar angular accelerations
	*/
	struct LEARNING_API FScalarAngularAccelerationFeature : public FFeatureObject
	{
		FScalarAngularAccelerationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngularAccelerationNum,
			const float InScale = Feature::DefaultScaleAngularAcceleration);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> AngularAccelerationHandle;
	};

	/**
	* Feature for an array of planar angular accelerations
	*/
	struct LEARNING_API FPlanarAngularAccelerationFeature : public FFeatureObject
	{
		FPlanarAngularAccelerationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngularAccelerationNum,
			const float InScale = Feature::DefaultScaleAngularAcceleration,
			const FVector InAxis = FVector::UpVector);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		FVector Axis = FVector::UpVector;

		TArrayMapHandle<2, FVector> AngularAccelerationHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	/**
	* Feature for an array of angular accelerations
	*/
	struct LEARNING_API FAngularAccelerationFeature : public FFeatureObject
	{
		FAngularAccelerationFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InAngularAccelerationNum,
			const float InScale = Feature::DefaultScaleAngularAcceleration);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, FVector> AngularAccelerationHandle;
		TArrayMapHandle<1, FQuat> RelativeRotationHandle;
	};

	//------------------------------------------------------------------

	/**
	* Feature for an array of scales
	*/
	struct LEARNING_API FScaleFeature : public FFeatureObject
	{
		FScaleFeature(
			const FName& InIdentifier,
			const TSharedRef<FArrayMap>& InInstanceData,
			const int32 InMaxInstanceNum,
			const int32 InScaleNum,
			const float InScale = Feature::DefaultScaleScale);

		virtual bool IsEncodable() const override final { return true; }
		virtual bool IsDecodable() const override final { return true; }
		virtual void Encode(const FIndexSet Instances) override final;
		virtual void Decode(const FIndexSet Instances) override final;

		TArrayMapHandle<2, float> ScaleHandle;
		TArrayMapHandle<1, float> RelativeScaleHandle;
	};

	//------------------------------------------------------------------

}
