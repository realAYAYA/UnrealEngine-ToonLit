// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	// This enables using a SoA array with operator[]
	template <class RotationType, class TranslationType, class ScaleType>
	struct TTransformSoAAdapter
	{
		RotationType& Rotation;
		TranslationType& Translation;
		ScaleType& Scale3D;

		TTransformSoAAdapter(RotationType& InRotation, TranslationType& InTranslation, ScaleType& InScale3D)
			: Rotation(InRotation)
			, Translation(InTranslation)
			, Scale3D(InScale3D)
		{
		}

		FORCEINLINE RotationType& GetRotation() const
		{
			return Rotation;
		}

		FORCEINLINE void SetRotation(const FQuat& InRotation)
		{
			Rotation = InRotation;
		}

		FORCEINLINE TranslationType& GetTranslation() const
		{
			return Translation;
		}

		FORCEINLINE void SetTranslation(const FVector& InTranslation)
		{
			Translation = InTranslation;
		}

		FORCEINLINE ScaleType& GetScale3D() const
		{
			return Scale3D;
		}

		FORCEINLINE void SetScale3D(const FVector& InScale3D)
		{
			Scale3D = InScale3D;
		}

		FORCEINLINE operator FTransform()
		{
			return FTransform(Rotation, Translation, Scale3D);
		}

		FORCEINLINE operator FTransform() const
		{
			return FTransform(Rotation, Translation, Scale3D);
		}

		FORCEINLINE void operator= (const FTransform& Transform)
		{
			Rotation = Transform.GetRotation();
			Translation = Transform.GetTranslation();
			Scale3D = Transform.GetScale3D();
		}

		FORCEINLINE void ScaleTranslation(const FVector::FReal& Scale)
		{
			Translation *= Scale;
			//DiagnosticCheckNaN_Translate();
		}

		FORCEINLINE void NormalizeRotation()
		{
			Rotation.Normalize();
			//DiagnosticCheckNaN_Rotate();
		}
	};

	using FTransformSoAAdapter = TTransformSoAAdapter<FQuat, FVector, FVector>;
	using FTransformSoAAdapterConst = TTransformSoAAdapter<const FQuat, const FVector, const FVector>;

	using FTransformArrayAoSView = TArrayView<FTransform>;
	using FTransformArrayAoSConstView = TArrayView<const FTransform>;

	struct FTransformArraySoAView
	{
		TArrayView<FVector> Translations;
		TArrayView<FQuat> Rotations;
		TArrayView<FVector> Scales3D;

		int32 Num() const { return Rotations.Num(); }

		inline FTransformSoAAdapter operator[](int32 Index)
		{
			return FTransformSoAAdapter(Rotations[Index], Translations[Index], Scales3D[Index]);
		}

		inline const FTransformSoAAdapterConst operator[](int32 Index) const
		{
			return FTransformSoAAdapterConst(Rotations[Index], Translations[Index], Scales3D[Index]);
		}
	};

	struct FTransformArraySoAConstView
	{
		TArrayView<const FVector> Translations;
		TArrayView<const FQuat> Rotations;
		TArrayView<const FVector> Scales3D;

		FTransformArraySoAConstView() = default;
		FTransformArraySoAConstView(const FTransformArraySoAView& Other)	// Safely coerce from mutable view
			: Translations(Other.Translations)
			, Rotations(Other.Rotations)
			, Scales3D(Other.Scales3D)
		{}

		int32 Num() const { return Rotations.Num(); }

		inline const FTransformSoAAdapterConst operator[](int32 Index) const
		{
			return FTransformSoAAdapterConst(Rotations[Index], Translations[Index], Scales3D[Index]);
		}
	};

#define DEFAULT_SOA_VIEW 1
#if DEFAULT_SOA_VIEW
	using FTransformArrayView = FTransformArraySoAView;
	using FTransformArrayConstView = FTransformArraySoAConstView;
#else
	using FTransformArrayView = FTransformArrayAoSView;
	using FTransformArrayConstView = FTransformArrayAoSConstView;
#endif
}
