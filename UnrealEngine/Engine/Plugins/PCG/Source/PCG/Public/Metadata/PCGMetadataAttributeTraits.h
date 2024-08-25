// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Transform.h" // IWYU pragma: keep
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"

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
	SoftObjectPath,
	SoftClassPath,

	Count UMETA(Hidden),

	Unknown = 255 UMETA(Hidden),
};

// Convenient macro to avoid duplicating a lot of code with all our supported types.
#define PCG_FOREACH_SUPPORTEDTYPES(MACRO) \
	MACRO(int32)           \
	MACRO(int64)           \
	MACRO(float)           \
	MACRO(double)          \
	MACRO(FVector2D)       \
	MACRO(FVector)         \
	MACRO(FVector4)        \
	MACRO(FQuat)           \
	MACRO(FTransform)      \
	MACRO(FString)         \
	MACRO(bool)            \
	MACRO(FRotator)        \
	MACRO(FName)           \
	MACRO(FSoftObjectPath) \
	MACRO(FSoftClassPath)

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
		PCGMetadataGenerateDataTypes(FSoftObjectPath, SoftObjectPath);
		PCGMetadataGenerateDataTypes(FSoftClassPath, SoftClassPath);

#undef PCGMetadataGenerateDataTypes

		constexpr inline bool IsPCGType(uint16 TypeId)
		{
			return TypeId < (uint16)EPCGMetadataTypes::Count;
		}

		template <typename T>
		constexpr inline bool IsPCGType()
		{
			return IsPCGType(MetadataTypes<T>::Id);
		}

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

		inline FString GetTypeName(uint16 InType)
		{
			UEnum* PCGDataTypeEnum = StaticEnum<EPCGMetadataTypes>();
			return PCGDataTypeEnum ? PCGDataTypeEnum->GetNameStringByValue(InType) : FString(TEXT("Unknown"));
		}

		template <typename T>
		FString GetTypeName()
		{
			return GetTypeName(MetadataTypes<T>::Id);
		}

		inline FText GetTypeNameText(uint16 InType)
		{
			return FText::FromString(GetTypeName(InType));
		}

		template <typename T>
		FText GetTypeNameText()
		{
			return FText::FromString(GetTypeName<T>());
		}

		template <typename T>
		void Serialize(FArchive& Ar, const T& A)
		{
			ensure(!Ar.IsLoading());
			Serialize(Ar, const_cast<T&>(A));
		}

		template <typename T>
		void Serialize(FArchive& Ar, T& A)
		{
			Ar << A;
		}

		// Serialize must be called on FSoftObjectPath, and this also covers FSoftClassPath.
		template <>
		inline void Serialize<FSoftObjectPath>(FArchive& Ar, FSoftObjectPath& A)
		{
			A.Serialize(Ar);
		}

		// Wrapper around a standard 2-dimensional CArray that is constexpr, to know if a type is broadcastable to another.
		// First index is the original type, second index is the wanted type. Returns true if we can broadcast first type into second type.
		struct FBroadcastableTypes
		{
			constexpr FBroadcastableTypes() : Values{ {false} }
			{
				// All types are broadcastable to themselves.
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
				PCGMetadataBroadcastable(Vector2, Vector4);

				PCGMetadataBroadcastable(Vector, Vector4);

				PCGMetadataBroadcastable(Quaternion, Rotator);
				PCGMetadataBroadcastable(Rotator, Quaternion);

				PCGMetadataBroadcastable(Float, String);
				PCGMetadataBroadcastable(Double, String);
				PCGMetadataBroadcastable(Integer32, String);
				PCGMetadataBroadcastable(Integer64, String);
				PCGMetadataBroadcastable(Vector2, String);
				PCGMetadataBroadcastable(Vector, String);
				PCGMetadataBroadcastable(Vector4, String);
				PCGMetadataBroadcastable(Quaternion, String);
				PCGMetadataBroadcastable(Transform, String);
				PCGMetadataBroadcastable(Boolean, String);
				PCGMetadataBroadcastable(Rotator, String);
				PCGMetadataBroadcastable(Name, String);
				PCGMetadataBroadcastable(String, Name);

				PCGMetadataBroadcastable(SoftObjectPath, String);
				PCGMetadataBroadcastable(SoftClassPath, String);

#undef PCGMetadataBroadcastable
			}

			constexpr const bool* operator[](int i) const { return Values[i]; }

			bool Values[(uint8)EPCGMetadataTypes::Count][(uint8)EPCGMetadataTypes::Count];
		};

		// Wrapper around a standard 2-dimensional CArray that is constexpr, to know if a type is constructible from another.
		// First index is the wanted type, second index is the original type. Returns true if we can construct first type from second type.
		struct FConstructibleTypes
		{
		private:
			// Empty struct that will hold all our types.
			template <typename... T>
			struct TypeHolder {};

			// Use the macro to have AllTypes that is the Dummy struct templated with all our types.
			// Add void as a sentinel, to know when to stop unrolling (and because TypeHolder<T,U,> is syntax error)
#define PCG_ALL_TYPES(T) T,
			using AllTypes = TypeHolder<PCG_FOREACH_SUPPORTEDTYPES(PCG_ALL_TYPES) void>;
#undef PCG_ALL_TYPES

			// Unroll AllTypes twice, to have all combinations of types <T, U>
			template <typename FirstType> constexpr void UnrollSecond(TypeHolder<void> InTypeHolder) {}

			template <typename FirstType, typename SecondType, typename... SecondTypes>
			constexpr void UnrollSecond(TypeHolder<SecondType, SecondTypes...> InTypeHolder)
			{
				Values[MetadataTypes<FirstType>::Id][MetadataTypes<SecondType>::Id] = std::is_constructible_v<FirstType, SecondType>;
				UnrollSecond<FirstType>(TypeHolder<SecondTypes...>{});
			}

			template <typename InputType, typename... InputTypes>
			constexpr void UnrollFirst(TypeHolder<InputType, InputTypes...> InTypeHolder)
			{
				UnrollSecond<InputType>(AllTypes{});
				UnrollFirst(TypeHolder<InputTypes...>{});
			}

			constexpr void UnrollFirst(TypeHolder<void> InTypeHolder) {}

		public:
			constexpr FConstructibleTypes() : Values{ {false} }
			{
				UnrollFirst(AllTypes{});
			}

			constexpr const bool* operator[](int i) const { return Values[i]; }

			bool Values[(uint8)EPCGMetadataTypes::Count][(uint8)EPCGMetadataTypes::Count];
		};

		inline static constexpr FBroadcastableTypes BroadcastableTypes{};
		inline static constexpr FConstructibleTypes ConstructibleTypes{};

		constexpr inline bool IsBroadcastable(uint16 FirstType, uint16 SecondType)
		{
			// Unknown types aren't broadcastable
			if (FirstType >= static_cast<uint16>(EPCGMetadataTypes::Count) || SecondType >= static_cast<uint16>(EPCGMetadataTypes::Count))
			{
				return false;
			}

			return BroadcastableTypes[FirstType][SecondType];
		}

		template <typename FirstType, typename SecondType>
		constexpr inline bool IsBroadcastable()
		{
			return IsBroadcastable(MetadataTypes<FirstType>::Id, MetadataTypes<SecondType>::Id);
		}

		constexpr inline bool IsConstructible(uint16 FirstType, uint16 SecondType)
		{
			// Unknown types can't be checked for constructible.
			if (FirstType >= static_cast<uint16>(EPCGMetadataTypes::Count) || SecondType >= static_cast<uint16>(EPCGMetadataTypes::Count))
			{
				return false;
			}

			return ConstructibleTypes[FirstType][SecondType];
		}

		// Convenience function for accessor operation, to know if a Get will succeed with AllowBroadcast and AllowConstructible
		// where the SecondType is the target type, and FirstType is the accessor type.
		// ie. Accessor is an int, Accessor->Get<float>(...). First type is int, second type is float.
		constexpr inline bool IsBroadcastableOrConstructible(uint16 FirstType, uint16 SecondType)
		{
			return IsBroadcastable(FirstType, SecondType) || IsConstructible(SecondType, FirstType);
		}

		/**
		* FirstType is more complex than SecondType, in the sense of PCG types, if both types are different and valid
		* and we can broadcast SecondType to FirstType.
		* It is useful to find the most complex type between operands of a given operation.
		*/
		constexpr inline bool IsMoreComplexType(uint16 FirstType, uint16 SecondType)
		{
			return FirstType != SecondType && FirstType <= (uint16)(EPCGMetadataTypes::Count) && SecondType <= (uint16)(EPCGMetadataTypes::Count) && PCG::Private::BroadcastableTypes[SecondType][FirstType];
		}

		/**
		* cf. non templated version of IsMoreComplexType
		*/
		template <typename FirstType, typename SecondType>
		constexpr inline bool IsMoreComplexType()
		{
			return IsMoreComplexType(MetadataTypes<FirstType>::Id, MetadataTypes<SecondType>::Id);
		}

		/**
		* Traits
		* - CanCompare: Supports less than, greater than etc for ordering.
		* - CanSearchString: Can do string search operations like matching substrings.
		* - CompressData: Uses value keys, so if two values are identical, we only store one.
		* - NeedsConstruction: Whether type requires constructor for valid state (i.e. can't simply be zero initialized).
		*/

		template<typename T>
		struct DefaultStringTraits
		{
			static FString ToString(const T& A)
			{
				return A.ToString();
			}
		};

		template<typename T>
		struct LexToStringTraits
		{
			static FString ToString(const T& A)
			{
				return LexToString(A);
			}
		};

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
		struct DefaultCompareTraits
		{
			enum { CanCompare = true };

			static bool Less(const T& A, const T& B)
			{
				return A < B;
			}

			static bool Greater(const T& A, const T& B)
			{
				return A > B;
			}

			static bool LessOrEqual(const T& A, const T& B)
			{
				return A <= B;
			}

			static bool GreaterOrEqual(const T& A, const T& B)
			{
				return A >= B;
			}
		};

		/** "Find Nearest" trait support driven by less/less or equal operation, used for types that do not support distance computation */
		template<typename Traits, typename T>
		struct CompareBasedFindNearestTraits
		{
			enum { CanFindNearest = true};
			enum { CanComputeDistance = false };

			/** Returns true if A is strictly closer to Target than B is. */
			static bool IsCloserTo(const T& A, const T& B, const T& Target)
			{
				// If A is "between" B and Target, then it is closer, e.g. B < A <= Target or Target <= A < B
				return (Traits::Less(B, A) && Traits::LessOrEqual(A, Target)) || (Traits::LessOrEqual(Target, A) && Traits::Less(A, B));
			}
		};

		/** "Find Nearest" trait support driven by distance comparison, used for types that support distance computation */
		template<typename Traits, typename T>
		struct DistanceBasedFindNearestTraits
		{
			enum { CanFindNearest = true };
			enum { CanComputeDistance = true };

			/** Returns true if A is strictly closer to Target than B is. */
			static bool IsCloserTo(const T& A, const T& B, const T& Target)
			{
				return Traits::Distance(A, Target) < Traits::Distance(B, Target);
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

			static T ZeroValueForWeightedSum()
			{
				return ZeroValue();
			}
		};

		template <typename T>
		struct DefaultMinMaxTraits
		{
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

		template <typename T>
		struct DefaultScalarDistanceTraits : DistanceBasedFindNearestTraits<DefaultScalarDistanceTraits<T>, T>
		{
			using DistanceType = T;

			static DistanceType Distance(const T& A, const T& B) 
			{ 
				return FMath::Abs(A - B);
			}
		};

		// Common traits for int32, int64, float, double
		template<typename T>
		struct MetadataTraits : DefaultOperationTraits<T>, DefaultWeightedSumTraits<T>, DefaultMinMaxTraits<T>, DefaultCompareTraits<T>, LexToStringTraits<T>, DefaultScalarDistanceTraits<T>
		{
			enum { CompressData = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };
		};

		template<>
		struct MetadataTraits<float> : DefaultOperationTraits<float>, DefaultWeightedSumTraits<float>, DefaultMinMaxTraits<float>, DefaultCompareTraits<float>, LexToStringTraits<float>, DefaultScalarDistanceTraits<float>
		{
			enum { CompressData = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };

			static bool Equal(const float& A, const float& B)
			{
				return FMath::IsNearlyEqual(A, B);
			}
		};

		template<>
		struct MetadataTraits<double> : DefaultOperationTraits<double>, DefaultWeightedSumTraits<double>, DefaultMinMaxTraits<double>, DefaultCompareTraits<double>, LexToStringTraits<double>, DefaultScalarDistanceTraits<double>
		{
			enum { CompressData = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };

			static bool Equal(const double& A, const double& B)
			{
				return FMath::IsNearlyEqual(A, B);
			}
		};

		template<>
		struct MetadataTraits<bool> : LexToStringTraits<bool>
		{
			enum { CompressData = false };
			enum { CanMinMax = true };
			enum { CanSubAdd = true };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };
			enum { CanCompare = true };
			enum { CanFindNearest = false };
			enum { CanComputeDistance = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };

			static bool ZeroValue()
			{
				return false;
			}

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

			static bool Less(const bool& A, const bool& B)
			{
				return !A && B;
			}

			static bool Greater(const bool& A, const bool& B)
			{
				return A && !B;
			}

			static bool LessOrEqual(const bool& A, const bool& B)
			{
				return !Greater(A, B);
			}

			static bool GreaterOrEqual(const bool& A, const bool& B)
			{
				return !Less(A, B);
			}
		};

		// Vector types
		template<typename T>
		struct VectorTraits : DefaultOperationTraits<T>, DefaultStringTraits<T>
		{
			enum { CompressData = false };
			enum { CanMinMax = true };
			enum { CanInterpolate = true };
			enum { CanCompare = true };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };
			enum { CanFindNearest = true };
			enum { CanComputeDistance = true };

			using DistanceType = typename T::FReal;

			static T ZeroValue()
			{
				return T::Zero();
			}

			static T ZeroValueForWeightedSum()
			{
				return ZeroValue();
			}

			static T WeightedSum(const T& A, const T& B, float Weight)
			{
				return A + B * Weight;
			}

			static DistanceType Distance(const T& A, const T& B) 
			{
				return (A - B).Size();
			}

			/** Returns true if A is strictly closer to Target than B is. Implementation note: we don't use the standard distance-based traits to have a more efficient check here. */
			static bool IsCloserTo(const T& A, const T& B, const T& Target)
			{
				return (A - Target).SizeSquared() < (B - Target).SizeSquared();
			}
		};

		template<>
		struct MetadataTraits<FVector2D> : VectorTraits<FVector2D>
		{
			static bool Equal(const FVector2D& A, const FVector2D& B)
			{
				return A.Equals(B);
			}

			static FVector2D Min(const FVector2D& A, const FVector2D& B)
			{
				return FVector2D::Min(A, B);
			}

			static FVector2D Max(const FVector2D& A, const FVector2D& B)
			{
				return FVector2D::Max(A, B);
			}

			static bool Less(const FVector2D& A, const FVector2D& B)
			{
				return (A.X < B.X) && (A.Y < B.Y);
			}

			static bool Greater(const FVector2D& A, const FVector2D& B)
			{
				return (A.X > B.X) && (A.Y > B.Y);
			}

			static bool LessOrEqual(const FVector2D& A, const FVector2D& B)
			{
				return (A.X <= B.X) && (A.Y <= B.Y);
			}

			static bool GreaterOrEqual(const FVector2D& A, const FVector2D& B)
			{
				return (A.X >= B.X) && (A.Y >= B.Y);
			}
		};

		template<>
		struct MetadataTraits<FVector> : VectorTraits<FVector>
		{
			static bool Equal(const FVector& A, const FVector& B)
			{
				return A.Equals(B);
			}

			static FVector Min(const FVector& A, const FVector& B)
			{
				return FVector::Min(A, B);
			}

			static FVector Max(const FVector& A, const FVector& B)
			{
				return FVector::Max(A, B);
			}

			static bool Less(const FVector& A, const FVector& B)
			{
				return (A.X < B.X) && (A.Y < B.Y) && (A.Z < B.Z);
			}

			static bool Greater(const FVector& A, const FVector& B)
			{
				return (A.X > B.X) && (A.Y > B.Y) && (A.Z > B.Z);
			}

			static bool LessOrEqual(const FVector& A, const FVector& B)
			{
				return (A.X <= B.X) && (A.Y <= B.Y) && (A.Z <= B.Z);
			}

			static bool GreaterOrEqual(const FVector& A, const FVector& B)
			{
				return (A.X >= B.X) && (A.Y >= B.Y) && (A.Z >= B.Z);
			}
		};
		
		template<>
		struct MetadataTraits<FVector4> : VectorTraits<FVector4>
		{
			static bool Equal(const FVector4& A, const FVector4& B)
			{
				return A.Equals(B);
			}

			static FVector4 Min(const FVector4& A, const FVector4& B)
			{
				return FVector4(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z), FMath::Min(A.W, B.W));
			}

			static FVector4 Max(const FVector4& A, const FVector4& B)
			{
				return FVector4(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z), FMath::Max(A.W, B.W));
			}

			static bool Less(const FVector4& A, const FVector4& B)
			{
				return (A.X < B.X) && (A.Y < B.Y) && (A.Z < B.Z) && (A.W < B.W);
			}

			static bool Greater(const FVector4& A, const FVector4& B)
			{
				return (A.X > B.X) && (A.Y > B.Y) && (A.Z > B.Z) && (A.W > B.W);
			}

			static bool LessOrEqual(const FVector4& A, const FVector4& B)
			{
				return (A.X <= B.X) && (A.Y <= B.Y) && (A.Z <= B.Z) && (A.W <= B.W);
			}

			static bool GreaterOrEqual(const FVector4& A, const FVector4& B)
			{
				return (A.X >= B.X) && (A.Y >= B.Y) && (A.Z >= B.Z) && (A.W >= B.W);
			}
		};

		// Quaternion
		template<>
		struct MetadataTraits<FQuat> : DefaultStringTraits<FQuat>, DistanceBasedFindNearestTraits<MetadataTraits<FQuat>, FQuat>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = true };
			enum { CanMulDiv = true };
			enum { CanInterpolate = true };
			enum { CanCompare = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };

			using DistanceType = typename FQuat::FReal;

			static bool Equal(const FQuat& A, const FQuat& B)
			{
				return A.Equals(B);
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

			static FQuat ZeroValueForWeightedSum()
			{
				return FQuat(0.0, 0.0, 0.0, 0.0);
			}

			static FQuat::FReal Distance(const FQuat& A, const FQuat& B)
			{
				return A.AngularDistance(B);
			}
		};

		// Rotator
		template<>
		struct MetadataTraits<FRotator> : DefaultStringTraits<FRotator>, DistanceBasedFindNearestTraits<MetadataTraits<FRotator>, FRotator>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = true };
			enum { CanMulDiv = true };
			enum { CanInterpolate = true };
			enum { CanCompare = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };

			using DistanceType = typename FRotator::FReal;

			static bool Equal(const FRotator& A, const FRotator& B)
			{
				return A.Equals(B);
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

			static FRotator ZeroValueForWeightedSum()
			{
				return ZeroValue();
			}

			static FRotator::FReal Distance(const FRotator& A, const FRotator& B)
			{
				// Use quaternions for distance calculation, but return degrees instead of radians here
				return 180.0 * FQuat(A).AngularDistance(FQuat(B)) / UE_DOUBLE_PI;
			}
		};

		// Transform
		template<>
		struct MetadataTraits<FTransform> : DefaultStringTraits<FTransform>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = false };
			enum { CanMulDiv = true };
			enum { CanInterpolate = true };
			enum { CanCompare = false };
			enum { CanFindNearest = false };
			enum { CanComputeDistance = false };
			enum { CanSearchString = false };
			enum { NeedsConstruction = false };

			static bool Equal(const FTransform& A, const FTransform& B)
			{
				return A.Equals(B);
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

			static FTransform ZeroValueForWeightedSum()
			{
				return FTransform(FQuat(0.0, 0.0, 0.0, 0.0), FVector::ZeroVector, FVector::ZeroVector);
			}
		};

		// Strings
		template<>
		struct MetadataTraits<FString> : DefaultCompareTraits<FString>, DefaultStringTraits<FString>, CompareBasedFindNearestTraits<MetadataTraits<FString>, FString>
		{
			enum { CompressData = true };
			enum { CanMinMax = false };
			enum { CanSubAdd = false };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };
			enum { CanSearchString = true };
			enum { NeedsConstruction = true };

			static bool Equal(const FString& A, const FString& B)
			{
				return A == B;
			}

			static FString ZeroValue()
			{
				return FString();
			}

			static bool Substring(const FString& A, const FString& B)
			{
				return A.Contains(B);
			}

			static bool Matches(const FString& A, const FString& B)
			{
				return A.MatchesWildcard(B);
			}
		};

		template<>
		struct MetadataTraits<FName> : DefaultStringTraits<FName>, CompareBasedFindNearestTraits<MetadataTraits<FName>, FName>
		{
			enum { CompressData = false };
			enum { CanMinMax = false };
			enum { CanSubAdd = false };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };
			enum { CanCompare = true };
			enum { CanSearchString = true };
			enum { NeedsConstruction = false };

			static bool Equal(const FName& A, const FName& B)
			{
				return A == B;
			}

			static bool Less(const FName& A, const FName& B)
			{
				return A.Compare(B) < 0;
			}

			static bool Greater(const FName& A, const FName& B)
			{
				return A.Compare(B) > 0;
			}

			static bool LessOrEqual(const FName& A, const FName& B)
			{
				return A.Compare(B) <= 0;
			}

			static bool GreaterOrEqual(const FName& A, const FName& B)
			{
				return A.Compare(B) >= 0;
			}

			static FName ZeroValue()
			{
				return NAME_None;
			}

			static bool Substring(const FName& A, const FName& B)
			{
				return A.ToString().Contains(B.ToString());
			}

			static bool Matches(const FName& A, const FName& B)
			{
				return A.ToString().MatchesWildcard(B.ToString());
			}
		};

		template<typename T>
		struct SoftObjectPathTraits : CompareBasedFindNearestTraits<SoftObjectPathTraits<T>, T>
		{
			enum { CompressData = true };
			enum { CanMinMax = false };
			enum { CanSubAdd = false };
			enum { CanMulDiv = false };
			enum { CanInterpolate = false };
			enum { CanSearchString = true };
			// Contains FString objects
			enum { NeedsConstruction = true };
			enum { CanCompare = true };

			static bool Equal(const T& A, const T& B)
			{
				return A == B;
			}

			static T ZeroValue()
			{
				return T();
			}

			static bool Substring(const T& A, const T& B)
			{
				return A.ToString().Contains(B.ToString());
			}

			static bool Matches(const T& A, const T& B)
			{
				return A.ToString().MatchesWildcard(B.ToString());
			}

			static bool Less(const T& A, const T& B)
			{
				return A.ToString() < B.ToString();
			}

			static bool Greater(const T& A, const T& B)
			{
				return A.ToString() > B.ToString();
			}

			static bool LessOrEqual(const T& A, const T& B)
			{
				return A.ToString() <= B.ToString();
			}

			static bool GreaterOrEqual(const FSoftObjectPath& A, const FSoftObjectPath& B)
			{
				return A.ToString() >= B.ToString();
			}
		};

		// Soft object paths
		template<>
		struct MetadataTraits<FSoftObjectPath> : SoftObjectPathTraits<FSoftObjectPath>, DefaultStringTraits<FSoftObjectPath>
		{
		};

		// Soft class paths
		template<>
		struct MetadataTraits<FSoftClassPath> : SoftObjectPathTraits<FSoftClassPath>, DefaultStringTraits<FSoftClassPath>
		{
		};

		/**
		* Generic function to broadcast an InType to an OutType.
		* Supports only PCG types
		* @param InValue - The Value to convert
		* @param OutValue - The converted value
		* @returns true if the conversion worked, false otherwise.
		*/
		template <typename InType, typename OutType>
		inline bool GetValueWithBroadcast(const InType& InValue, OutType& OutValue)
		{
			if constexpr (std::is_same_v<OutType, InType>)
			{
				OutValue = InValue;
				return true;
			}
			else
			{
				if constexpr (!IsBroadcastable<InType, OutType>())
				{
					return false;
				}
				else
				{
					if constexpr (std::is_same_v<OutType, FVector4>)
					{
						// TODO: Should W be 0? 1? Something else? Depending on operation?
						// For now, we set Z to 0 (for vec 2) and we set W to 1.
						if constexpr (std::is_same_v<InType, FVector>)
						{
							OutValue = FVector4(InValue, 1.0);
						}
						else if constexpr (std::is_same_v<InType, FVector2D>)
						{
							OutValue = FVector4(InValue.X, InValue.Y, 0.0, 1.0);
						}
						else
						{
							OutValue = FVector4(InValue, InValue, InValue, InValue);
						}
					}
					else
					{
						// Seems like the && condition is not evaluated correctly on Linux, so we cut the condition in two `if constexpr`.
						if constexpr (std::is_same_v<OutType, FVector>)
						{
							if constexpr (std::is_same_v<InType, FVector2D>)
							{
								OutValue = FVector(InValue, 0.0);
							}
							else
							{
								OutValue = OutType(InValue);
							}
						}
						else
						{
							if constexpr (std::is_same_v<OutType, FString>)
							{
								OutValue = MetadataTraits<InType>::ToString(InValue);
							}
							else
							{
								OutValue = OutType(InValue);
							}
						}
					}

					return true;
				}
			}
		}

		template<typename InType, typename OutType>
		inline bool GetValueWithBroadcastAndConstructible(const InType& InValue, OutType& OutValue)
		{
			if constexpr (std::is_same_v<OutType, InType>)
			{
				OutValue = InValue;
				return true;
			}
			else
			{
				// Special case'd because FSoftObjectPath currently has a deprecated constructor from FName which generates compile warnings.
				constexpr bool bNameToSoftObjectPath = std::is_same_v<OutType, FSoftObjectPath>&& std::is_same_v<InType, FName>;

				if constexpr (PCG::Private::IsBroadcastable<InType, OutType>())
				{
					return GetValueWithBroadcast<InType, OutType>(InValue, OutValue);
				}
				else if constexpr (std::is_constructible_v<OutType, InType> && !bNameToSoftObjectPath)
				{
					OutValue = OutType(InValue);
					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}
}
