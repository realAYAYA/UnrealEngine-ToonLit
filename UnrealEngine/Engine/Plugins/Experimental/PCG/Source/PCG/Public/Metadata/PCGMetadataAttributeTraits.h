// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGMetadataAttributeTraits.generated.h"

UENUM(BlueprintType)
enum class EPCGMetadataTypes : uint8
{
	Float = 0,
	Double,
	Integer32,
	Integer64,
	Vector2,
	Vector,
	Vector4,
	Quaternion,
	Transform,
	String,
	Boolean,
	Rotator,
	Name,

	Count UMETA(Hidden),

	Unknown = 255
};

namespace PCG
{
	namespace Private
	{
		template<typename T>
		struct MetadataTypes
		{
			enum { Id = static_cast<uint16>(EPCGMetadataTypes::Unknown) + sizeof(T) };
		};

#define PCGMetadataGenerateDataTypes(Type, TypeEnum) template<> struct MetadataTypes<Type>{ enum { Id = static_cast<uint16>(EPCGMetadataTypes::TypeEnum)}; }

		PCGMetadataGenerateDataTypes(float, Float);
		PCGMetadataGenerateDataTypes(double, Double);
		PCGMetadataGenerateDataTypes(int32, Integer32);
		PCGMetadataGenerateDataTypes(int64, Integer64);
		PCGMetadataGenerateDataTypes(FVector2D, Vector2);
		PCGMetadataGenerateDataTypes(FVector, Vector);
		PCGMetadataGenerateDataTypes(FVector4, Vector4);
		PCGMetadataGenerateDataTypes(FQuat, Quaternion);
		PCGMetadataGenerateDataTypes(FTransform, Transform);
		PCGMetadataGenerateDataTypes(FString, String);
		PCGMetadataGenerateDataTypes(bool, Boolean);
		PCGMetadataGenerateDataTypes(FRotator, Rotator);
		PCGMetadataGenerateDataTypes(FName, Name);

#undef PCGMetadataGenerateDataTypes

		// Verify if the TypeId is a type known by PCG and matches any types provided in the template.
		// Example: IsOfTypes<FVector2D, FVector, FVector4>(TypeId);
		template <typename ...Types>
		constexpr inline bool IsOfTypes(uint16 TypeId)
		{
			return (TypeId < (uint16)EPCGMetadataTypes::Unknown) && ((TypeId == MetadataTypes<Types>::Id) || ...);
		}

		// Verify if T is a type known by PCG and if it matches another type
		// in all provided types (in Types...)
		// Example: IsOfTypes<T, FVector2D, FVector, FVector4>();
		template <typename T, typename ...Types>
		constexpr inline bool IsOfTypes()
		{
			return IsOfTypes<Types...>(MetadataTypes<T>::Id);
		}

		// Wrapper around a standard 2-dimensional CArray that is constexpr, to know if a type is broadcastable to another.
		// First index is the original type, second index is the wanted type. Returns true if we can broadcast first type into second type.
		struct UBroadcastableTypes
		{
			constexpr UBroadcastableTypes() : Values{{false}}
			{
				for (uint8 i = 0; i < (uint8)EPCGMetadataTypes::Count; ++i)
				{
					Values[i][i] = true;
				}

#define PCGMetadataBroadcastable(FirstType, SecondType) Values[(uint8)EPCGMetadataTypes::FirstType][(uint8)EPCGMetadataTypes::SecondType] = true

				// Finally set all cases where it is broadcastable
				PCGMetadataBroadcastable(Float, Double);
				PCGMetadataBroadcastable(Float, Vector2);
				PCGMetadataBroadcastable(Float, Vector);
				PCGMetadataBroadcastable(Float, Vector4);

				PCGMetadataBroadcastable(Double, Vector2);
				PCGMetadataBroadcastable(Double, Vector);
				PCGMetadataBroadcastable(Double, Vector4);

				PCGMetadataBroadcastable(Integer32, Float);
				PCGMetadataBroadcastable(Integer32, Double);
				PCGMetadataBroadcastable(Integer32, Integer64);
				PCGMetadataBroadcastable(Integer32, Vector2);
				PCGMetadataBroadcastable(Integer32, Vector);
				PCGMetadataBroadcastable(Integer32, Vector4);

				PCGMetadataBroadcastable(Integer64, Float);
				PCGMetadataBroadcastable(Integer64, Double);
				PCGMetadataBroadcastable(Integer64, Vector2);
				PCGMetadataBroadcastable(Integer64, Vector);
				PCGMetadataBroadcastable(Integer64, Vector4);

				PCGMetadataBroadcastable(Vector2, Vector);

				PCGMetadataBroadcastable(Quaternion, Rotator);
				PCGMetadataBroadcastable(Rotator, Quaternion);

#undef PCGMetadataBroadcastable
			}

			constexpr const bool* operator[](int i) const { return Values[i]; }

			bool Values[(uint8)EPCGMetadataTypes::Count][(uint8)EPCGMetadataTypes::Count];
		};

		inline static constexpr UBroadcastableTypes BroadcastableTypes{};

		constexpr inline bool IsBroadcastable(uint16 FirstType, uint16 SecondType)
		{
			// Unknown types aren't broadcastable
			if (FirstType >= static_cast<uint16>(EPCGMetadataTypes::Count) || SecondType >= static_cast<uint16>(EPCGMetadataTypes::Count))
			{
				return false;
			}

			return BroadcastableTypes[FirstType][SecondType];
		}

		template<typename T>
		struct DefaultOperationTraits
		{
			enum { CanSubAdd = true };
			enum { CanMulDiv = true };
			
			static bool Equal(const T& A, const T& B)
			{
				return A == B;
			}

			static T Sub(const T& A, const T& B)
			{
				return A - B;
			}

			static T Add(const T& A, const T& B)
			{
				return A + B;
			}

			static T Mul(const T& A, const T& B)
			{
				return A * B;
			}

			static T Div(const T& A, const T& B)
			{
				return A / B;
			}
		};

		template<typename T>
		struct DefaultWeightedSumTraits
		{
			enum { CanInterpolate = true };

			static T WeightedSum(const T& A, const T& B, float Weight)
			{
				return A + B * Weight;
			}

			static T ZeroValue()
			{
				return T{};
			}
		};

		// Common traits for int32, int64, float, double
		template<typename T>
		struct MetadataTraits : DefaultOperationTraits<T>, DefaultWeightedSumTraits<T>
		{
			enum { CompressData = false };
			enum { CanMinMax = true };

			static T Min(const T& A, const T& B)
			{
				return FMath::Min(A, B);
			}

			static T Max(const T& A, const T& B)
			{
				return FMath::Max(A, B);
			}
		};

		template<>
		struct MetadataTraits<bool>
		{
			enum { CompressData = false };
			enum { CanMinMax = true };
			enum { CanSubAdd = true };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };

			static bool Equal(const bool& A, const bool& B)
			{
				return A == B;
			}

			static bool Min(const bool& A, const bool& B)
			{
				return A && B;
			}

			static bool Max(const bool& A, const bool& B)
			{
				return A || B;
			}

			static bool Add(const bool& A, const bool& B)
			{
				return A || B;
			}

			static bool Sub(const bool& A, const bool& B)
			{
				return A && !B;
			}
		};

		// Vector types
		template<typename T>
		struct VectorTraits : DefaultOperationTraits<T>, DefaultWeightedSumTraits<T>
		{
			enum { CompressData = false };
			enum { CanMinMax = true };
			enum { CanInterpolate = true };

			static T ZeroValue()
			{
				return T::Zero();
			}
		};

		template<>
		struct MetadataTraits<FVector2D> : VectorTraits<FVector2D>
		{
			static FVector2D Min(const FVector2D& A, const FVector2D& B)
			{
				return FVector2D::Min(A, B);
			}

			static FVector2D Max(const FVector2D& A, const FVector2D& B)
			{
				return FVector2D::Max(A, B);
			}
		};

		template<>
		struct MetadataTraits<FVector> : VectorTraits<FVector>
		{
			static FVector Min(const FVector& A, const FVector& B)
			{
				return FVector::Min(A, B);
			}

			static FVector Max(const FVector& A, const FVector& B)
			{
				return FVector::Max(A, B);
			}
		};
		
		template<>
		struct MetadataTraits<FVector4> : VectorTraits<FVector4>
		{
			static FVector4 Min(const FVector4& A, const FVector4& B)
			{
				return FVector4(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z), FMath::Min(A.W, B.W));
			}

			static FVector4 Max(const FVector4& A, const FVector4& B)
			{
				return FVector4(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z), FMath::Max(A.W, B.W));
			}
		};

		// Quaternion
		template<>
		struct MetadataTraits<FQuat>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = true };
			enum { CanMulDiv = true };
			enum { CanInterpolate = true };

			static bool Equal(const FQuat& A, const FQuat& B)
			{
				return A == B;
			}

			static FQuat Add(const FQuat& A, const FQuat& B)
			{
				return A * B;
			}

			static FQuat Sub(const FQuat& A, const FQuat& B)
			{
				return A * B.Inverse();
			}

			static FQuat Mul(const FQuat& A, const FQuat& B)
			{
				return A * B;
			}

			static FQuat Div(const FQuat& A, const FQuat& B)
			{
				return A * B.Inverse();
			}

			static FQuat WeightedSum(const FQuat& A, const FQuat& B, float Weight)
			{
				// WARNING: the quaternion won't be normalized
				FQuat BlendQuat = B * Weight;

				if ((A | BlendQuat) >= 0.0f)
					return A + BlendQuat;
				else
					return A - BlendQuat;
			}

			static FQuat ZeroValue()
			{
				return FQuat::Identity;
			}
		};

		// Rotator
		template<>
		struct MetadataTraits<FRotator>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = true };
			enum { CanMulDiv = true };
			enum { CanInterpolate = true };

			static bool Equal(const FRotator& A, const FRotator& B)
			{
				return A == B;
			}

			static FRotator Add(const FRotator& A, const FRotator& B)
			{
				return A + B;
			}

			static FRotator Sub(const FRotator& A, const FRotator& B)
			{
				return A - B;
			}

			static FRotator Mul(const FRotator& A, const FRotator& B)
			{
				return A + B;
			}

			static FRotator Div(const FRotator& A, const FRotator& B)
			{
				return A - B;
			}

			static FRotator WeightedSum(const FRotator& A, const FRotator& B, float Weight)
			{
				// TODO review this, should we use TCustomLerp<UE::Math::TRotator<T>> ?
				return A + (B * Weight);
			}

			static FRotator ZeroValue()
			{
				return FRotator::ZeroRotator;
			}
		};

		// Transform
		template<>
		struct MetadataTraits<FTransform>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = true };
			enum { CanMulDiv = true };
			enum { CanInterpolate = true };

			static bool Equal(const FTransform& A, const FTransform& B)
			{
				return A.GetLocation() == B.GetLocation() &&
					A.GetRotation() == B.GetRotation() &&
					A.GetScale3D() == B.GetScale3D();
			}

			static FTransform Add(const FTransform& A, const FTransform& B)
			{
				return A * B;
			}

			static FTransform Sub(const FTransform& A, const FTransform& B)
			{
				return A * B.Inverse();
			}

			static FTransform Mul(const FTransform& A, const FTransform& B)
			{
				return A * B;
			}

			static FTransform Div(const FTransform& A, const FTransform& B)
			{
				return A * B.Inverse();
			}

			static FQuat WeightedQuatSum(const FQuat& Q, const FQuat& V, float Weight)
			{
				FQuat BlendQuat = V * Weight;

				if ((Q | BlendQuat) >= 0.0f)
					return Q + BlendQuat;
				else
					return Q - BlendQuat;
			}

			static FTransform WeightedSum(const FTransform& A, const FTransform& B, float Weight)
			{
				// WARNING: the rotation won't be normalized
				return FTransform(
					WeightedQuatSum(A.GetRotation(), B.GetRotation(), Weight),
					A.GetLocation() + B.GetLocation() * Weight,
					A.GetScale3D() + B.GetScale3D() * Weight);
			}

			static FTransform ZeroValue()
			{
				return FTransform::Identity;
			}
		};

		// Strings
		template<>
		struct MetadataTraits<FString>
		{
			enum { CompressData = true };
			enum { CanMinMax = false };
			enum { CanSubAdd = false };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };

			static bool Equal(const FString& A, const FString& B)
			{
				return A == B;
			}
		};

		template<>
		struct MetadataTraits<FName>
		{
			enum { CompressData = true };
			enum { CanMinMax = false };
			enum { CanSubAdd = false };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };

			static bool Equal(const FName& A, const FName& B)
			{
				return A == B;
			}
		};
	}
}