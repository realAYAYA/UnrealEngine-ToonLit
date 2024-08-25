// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Crc.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Serialization/StructuredArchive.h"
#include "Misc/LargeWorldCoordinatesSerializer.h"

namespace UE::Math
{

/**
 * Structure for integer vectors in 3-d space.
 */
template <typename InIntType>
struct TIntVector3
{
	using IntType = InIntType;
	static_assert(std::is_integral_v<IntType>, "Only an integer types are supported.");

	union
	{
		struct
		{
			/** Holds the point's x-coordinate. */
			IntType X;

			/** Holds the point's y-coordinate. */
			IntType Y;

			/**  Holds the point's z-coordinate. */
			IntType Z;
		};

		UE_DEPRECATED(all, "For internal use only")
		IntType XYZ[3];
	};

	/** An int point with zeroed values. */
	static const TIntVector3 ZeroValue;

	/** An int point with INDEX_NONE values. */
	static const TIntVector3 NoneValue;

	/**
	 * Default constructor (no initialization).
	 */
	TIntVector3() = default;

	/**
	 * Creates and initializes a new instance with the specified coordinates.
	 *
	 * @param InX The x-coordinate.
	 * @param InY The y-coordinate.
	 * @param InZ The z-coordinate.
	 */
	TIntVector3(IntType InX, IntType InY, IntType InZ )
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{
	}

	/**
	 * Constructor
	 *
	 * @param InValue replicated to all components
	 */
	explicit TIntVector3(IntType InValue )
		: X(InValue)
		, Y(InValue)
		, Z(InValue)
	{
	}

	/**
	 * Constructor
	 *
	 * @param InVector float vector converted to int
	 */
	explicit TIntVector3( FVector InVector  );

	/**
	 * Constructor
	 *
	 * @param EForceInit Force init enum
	 */

	explicit TIntVector3( EForceInit )
		: X(0)
		, Y(0)
		, Z(0)
	{
	}

	/**
	 * Converts to another int type. Checks that the cast will succeed.
	 */
	template <typename OtherIntType>
	explicit TIntVector3(TIntVector3<OtherIntType> Other)
		: X(IntCastChecked<IntType>(Other.X))
		, Y(IntCastChecked<IntType>(Other.Y))
		, Z(IntCastChecked<IntType>(Other.Z))
	{
	}

	// Workaround for clang deprecation warnings for deprecated XYZ member in implicitly-defined special member functions
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TIntVector3(TIntVector3&&) = default;
	TIntVector3(const TIntVector3&) = default;
	TIntVector3& operator=(TIntVector3&&) = default;
	TIntVector3& operator=(const TIntVector3&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return const reference to component.
	 */
	const IntType& operator()(int32 ComponentIndex) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return reference to component.
	 */
	IntType& operator()(int32 ComponentIndex)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return const reference to component.
	 */
	const IntType& operator[](int32 ComponentIndex) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return reference to component.
	 */
	IntType& operator[](int32 ComponentIndex)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZ[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Compares points for equality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are equal, false otherwise..
	 */
	bool operator==(const TIntVector3& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	/**
	 * Compares points for inequality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are not equal, false otherwise..
	 */
	bool operator!=(const TIntVector3& Other) const
	{
		return X != Other.X || Y != Other.Y || Z != Other.Z;
	}

	/**
	 * Multiplies this vector with another vector, using component-wise multiplication.
	 *
	 * @param Other The point to multiply with.
	 * @return Reference to this point after multiplication.
	 */
	TIntVector3& operator*=(const TIntVector3& Other)
	{
		X *= Other.X;
		Y *= Other.Y;
		Z *= Other.Z;

		return *this;
	}

	/**
	 * Scales this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return Reference to this point after multiplication.
	 */
	TIntVector3& operator*=(IntType Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;

		return *this;
	}

	/**
	 * Divides this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after division.
	 */
	TIntVector3& operator/=(IntType Divisor)
	{
		X /= Divisor;
		Y /= Divisor;
		Z /= Divisor;

		return *this;
	}

	/**
	 * Remainder of division of this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after remainder.
	 */
	TIntVector3& operator%=(IntType Divisor)
	{
		X %= Divisor;
		Y %= Divisor;
		Z %= Divisor;

		return *this;
	}

	/**
	 * Adds to this point.
	 *
	 * @param Other The point to add to this point.
	 * @return Reference to this point after addition.
	 */
	TIntVector3& operator+=(const TIntVector3& Other)
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;

		return *this;
	}

	/**
	 * Subtracts from this point.
	 *
	 * @param Other The point to subtract from this point.
	 * @return Reference to this point after subtraction.
	 */
	TIntVector3& operator-=(const TIntVector3& Other)
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;

		return *this;
	}

	/**
	 * Gets the result of component-wise multiplication of this point by another.
	 *
	 * @param Other The point to multiply with.
	 * @return The result of multiplication.
	 */
	TIntVector3 operator*(const TIntVector3& Other) const
	{
		return TIntVector3(*this) *= Other;
	}

	/**
	 * Gets the result of scaling on this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return A new scaled int point.
	 */
	TIntVector3 operator*(IntType Scale) const
	{
		return TIntVector3(*this) *= Scale;
	}

	/**
	 * Gets the result of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new divided int point.
	 */
	TIntVector3 operator/(IntType Divisor) const
	{
		return TIntVector3(*this) /= Divisor;
	}

	/**
	 * Gets the remainder of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new remainder int point.
	 */
	TIntVector3 operator%(IntType Divisor) const
	{
		return TIntVector3(*this) %= Divisor;
	}

	/**
	 * Gets the result of addition on this point.
	 *
	 * @param Other The other point to add to this.
	 * @return A new combined int point.
	 */
	TIntVector3 operator+(const TIntVector3& Other) const
	{
		return TIntVector3(*this) += Other;
	}

	/**
	 * Gets the result of subtraction from this point.
	 *
	 * @param Other The other point to subtract from this.
	 * @return A new subtracted int point.
	 */
	TIntVector3 operator-(const TIntVector3& Other) const
	{
		return TIntVector3(*this) -= Other;
	}

	/**
	 * Shifts all components to the right.
	 *
	 * @param Shift The number of bits to shift.
	 * @return A new shifted int point.
	 */
	TIntVector3 operator>>(IntType Shift) const
	{
		return TIntVector3(X >> Shift, Y >> Shift, Z >> Shift);
	}

	/**
	 * Shifts all components to the left.
	 *
	 * @param Shift The number of bits to shift.
	 * @return A new shifted int point.
	 */
	TIntVector3 operator<<(IntType Shift) const
	{
		return TIntVector3(X << Shift, Y << Shift, Z << Shift);
	}

	/**
	 * Component-wise AND.
	 *
	 * @param Value Number to AND with the each component.
	 * @return A new shifted int point.
	 */
	TIntVector3 operator&(IntType Value) const
	{
		return TIntVector3(X & Value, Y & Value, Z & Value);
	}

	/**
	 * Component-wise OR.
	 *
	 * @param Value Number to OR with the each component.
	 * @return A new shifted int point.
	 */
	TIntVector3 operator|(IntType Value) const
	{
		return TIntVector3(X | Value, Y | Value, Z | Value);
	}

	/**
	 * Component-wise XOR.
	 *
	 * @param Value Number to XOR with the each component.
	 * @return A new shifted int point.
	 */
	TIntVector3 operator^(IntType Value) const
	{
		return TIntVector3(X ^ Value, Y ^ Value, Z ^ Value);
	}

	/**
	 * Is vector equal to zero.
	 * @return is zero
	*/
	bool IsZero() const
	{
		return *this == ZeroValue;
	}

	/**
	 * Gets the maximum value in the point.
	 *
	 * @return The maximum value in the point.
	 */
	IntType GetMax() const
	{
		return FMath::Max(FMath::Max(X, Y), Z);
	}

	/**
	 * Gets the minimum value in the point.
	 *
	 * @return The minimum value in the point.
	 */
	IntType GetMin() const
	{
		return FMath::Min(FMath::Min(X, Y), Z);
	}

	/**
	 * Gets the distance of this point from (0,0,0).
	 *
	 * @return The distance of this point from (0,0,0).
	 */
	IntType Size() const
	{
		int64 LocalX64 = (int64)X;
		int64 LocalY64 = (int64)Y;
		int64 LocalZ64 = (int64)Z;
		return IntType(FMath::Sqrt(double(LocalX64 * LocalX64 + LocalY64 * LocalY64 + LocalZ64 * LocalZ64)));
	}

	/**
	 * Get a textual representation of this vector.
	 *
	 * @return A string describing the vector.
	 */
	FString ToString() const
	{
		return FString::Printf(TEXT("X=%s Y=%s Z=%s"), *LexToString(X), *LexToString(Y), *LexToString(Z));
	}

	/**
	 * Divide an int point and round up the result.
	 *
	 * @param lhs The int point being divided.
	 * @param Divisor What to divide the int point by.
	 * @return A new divided int point.
	 */
	static TIntVector3 DivideAndRoundUp(TIntVector3 lhs, IntType Divisor)
	{
		return TIntVector3(FMath::DivideAndRoundUp(lhs.X, Divisor), FMath::DivideAndRoundUp(lhs.Y, Divisor), FMath::DivideAndRoundUp(lhs.Z, Divisor));
	}

	static TIntVector3 DivideAndRoundUp(TIntVector3 lhs, TIntVector3 Divisor)
	{
		return TIntVector3(FMath::DivideAndRoundUp(lhs.X, Divisor.X), FMath::DivideAndRoundUp(lhs.Y, Divisor.Y), FMath::DivideAndRoundUp(lhs.Z, Divisor.Z));
	}

	/**
	 * Gets the number of components a point has.
	 *
	 * @return Number of components point has.
	 */
	static int32 Num()
	{
		return 3;
	}

	/**
	 * Serializes the Vector3.
	 *
	 * @param Ar The archive to serialize into.
	 * @param Vector The vector to serialize.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, TIntVector3& Vector )
	{
		return Ar << Vector.X << Vector.Y << Vector.Z;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, TIntVector3& Vector)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("X"), Vector.X);
		Record << SA_VALUE(TEXT("Y"), Vector.Y);
		Record << SA_VALUE(TEXT("Z"), Vector.Z);
	}

	bool Serialize( FArchive& Ar )
	{
		Ar << *this;
		return true;
	}

	bool SerializeFromMismatchedTag(FName StructTag, FArchive& Ar);
};

template <>
inline FString TIntVector3<int64>::ToString() const
{
	return FString::Printf(TEXT("X=%lld Y=%lld Z=%lld"), X, Y, Z);
}

template <>
inline FString TIntVector3<int32>::ToString() const
{
	return FString::Printf(TEXT("X=%d Y=%d Z=%d"), X, Y, Z);
}

template <>
inline FString TIntVector3<int16>::ToString() const
{
	return FString::Printf(TEXT("X=%d Y=%d Z=%d"), X, Y, Z);
}

template <>
inline FString TIntVector3<int8>::ToString() const
{
	return FString::Printf(TEXT("X=%d Y=%d Z=%d"), X, Y, Z);
}

template <>
inline FString TIntVector3<uint64>::ToString() const
{
	return FString::Printf(TEXT("X=%llu Y=%llu Z=%llu"), X, Y, Z);
}

template <>
inline FString TIntVector3<uint32>::ToString() const
{
	return FString::Printf(TEXT("X=%u Y=%u Z=%u"), X, Y, Z);
}

template <>
inline FString TIntVector3<uint16>::ToString() const
{
	return FString::Printf(TEXT("X=%u Y=%u Z=%u"), X, Y, Z);
}

template <>
inline FString TIntVector3<uint8>::ToString() const
{
	return FString::Printf(TEXT("X=%u Y=%u Z=%u"), X, Y, Z);
}

template <typename IntType>
const TIntVector3<IntType> TIntVector3<IntType>::ZeroValue(0, 0, 0);

template <typename IntType>
const TIntVector3<IntType> TIntVector3<IntType>::NoneValue(INDEX_NONE, INDEX_NONE, INDEX_NONE);

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename InIntType>
struct TIntVector2
{
	using IntType = InIntType;
	static_assert(std::is_integral_v<IntType>, "Only an integer types are supported.");

	union
	{
		struct
		{
			IntType X, Y;
		};

		UE_DEPRECATED(all, "For internal use only")
		IntType XY[2];
	};

	/** An int point with zeroed values. */
	static const TIntVector2 ZeroValue;

	/** An int point with INDEX_NONE values. */
	static const TIntVector2 NoneValue;

	TIntVector2() = default;

	TIntVector2(IntType InX, IntType InY)
		: X(InX)
		, Y(InY)
	{
	}

	explicit TIntVector2(IntType InValue)
		: X(InValue)
		, Y(InValue)
	{
	}

	TIntVector2(EForceInit)
		: X(0)
		, Y(0)
	{
	}

	/**
	 * Converts to another int type. Checks that the cast will succeed.
	 */
	template <typename OtherIntType>
	explicit TIntVector2(TIntVector2<OtherIntType> Other)
		: X(IntCastChecked<IntType>(Other.X))
		, Y(IntCastChecked<IntType>(Other.Y))
	{
	}

	// Workaround for clang deprecation warnings for deprecated XY member in implicitly-defined special member functions
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TIntVector2(TIntVector2&&) = default;
	TIntVector2(const TIntVector2&) = default;
	TIntVector2& operator=(TIntVector2&&) = default;
	TIntVector2& operator=(const TIntVector2&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	const IntType& operator[](int32 ComponentIndex) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XY[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	IntType& operator[](int32 ComponentIndex)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XY[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	bool operator==(const TIntVector2& Other) const
	{
		return X==Other.X && Y==Other.Y;
	}

	bool operator!=(const TIntVector2& Other) const
	{
		return X!=Other.X || Y!=Other.Y;
	}

	/**
	 * Multiplies this vector with another vector, using component-wise multiplication.
	 *
	 * @param Other The point to multiply with.
	 * @return Reference to this point after multiplication.
	 */
	TIntVector2& operator*=(const TIntVector2& Other)
	{
		X *= Other.X;
		Y *= Other.Y;

		return *this;
	}

	/**
	 * Scales this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return Reference to this point after multiplication.
	 */
	TIntVector2& operator*=(IntType Scale)
	{
		X *= Scale;
		Y *= Scale;

		return *this;
	}

	/**
	 * Divides this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after division.
	 */
	TIntVector2& operator/=(IntType Divisor)
	{
		X /= Divisor;
		Y /= Divisor;

		return *this;
	}

	/**
	 * Remainder of division of this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after remainder.
	 */
	TIntVector2& operator%=(IntType Divisor)
	{
		X %= Divisor;
		Y %= Divisor;

		return *this;
	}

	/**
	 * Adds to this point.
	 *
	 * @param Other The point to add to this point.
	 * @return Reference to this point after addition.
	 */
	TIntVector2& operator+=(const TIntVector2& Other)
	{
		X += Other.X;
		Y += Other.Y;

		return *this;
	}

	/**
	 * Subtracts from this point.
	 *
	 * @param Other The point to subtract from this point.
	 * @return Reference to this point after subtraction.
	 */
	TIntVector2& operator-=(const TIntVector2& Other)
	{
		X -= Other.X;
		Y -= Other.Y;

		return *this;
	}

	/**
	 * Gets the result of component-wise multiplication of this point by another.
	 *
	 * @param Other The point to multiply with.
	 * @return The result of multiplication.
	 */
	TIntVector2 operator*(const TIntVector2& Other) const
	{
		return TIntVector2(*this) *= Other;
	}

	/**
	 * Gets the result of scaling on this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return A new scaled int point.
	 */
	TIntVector2 operator*(IntType Scale) const
	{
		return TIntVector2(*this) *= Scale;
	}

	/**
	 * Gets the result of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new divided int point.
	 */
	TIntVector2 operator/(IntType Divisor) const
	{
		return TIntVector2(*this) /= Divisor;
	}

	/**
	 * Gets the remainder of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new remainder int point.
	 */
	TIntVector2 operator%(IntType Divisor) const
	{
		return TIntVector2(*this) %= Divisor;
	}

	/**
	 * Gets the result of addition on this point.
	 *
	 * @param Other The other point to add to this.
	 * @return A new combined int point.
	 */
	TIntVector2 operator+(const TIntVector2& Other) const
	{
		return TIntVector2(*this) += Other;
	}

	/**
	 * Gets the result of subtraction from this point.
	 *
	 * @param Other The other point to subtract from this.
	 * @return A new subtracted int point.
	 */
	TIntVector2 operator-(const TIntVector2& Other) const
	{
		return TIntVector2(*this) -= Other;
	}

	/**
	 * Serializes the Vector2.
	 *
	 * @param Ar The archive to serialize into.
	 * @param Vector The vector to serialize.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, TIntVector2& Vector)
	{
		return Ar << Vector.X << Vector.Y;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	bool SerializeFromMismatchedTag(FName StructTag, FArchive& Ar);
};

template <typename IntType>
const TIntVector2<IntType> TIntVector2<IntType>::ZeroValue(0, 0);

template <typename IntType>
const TIntVector2<IntType> TIntVector2<IntType>::NoneValue(INDEX_NONE, INDEX_NONE);

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename InIntType>
struct TIntVector4
{
	using IntType = InIntType;
	static_assert(std::is_integral_v<IntType>, "Only an integer types are supported.");

	union
	{
		struct
		{
			IntType X, Y, Z, W;
		};

		UE_DEPRECATED(all, "For internal use only")
		IntType XYZW[4];
	};

	TIntVector4() = default;

	TIntVector4(IntType InX, IntType InY, IntType InZ, IntType InW)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{
	}

	explicit TIntVector4(IntType InValue)
		: X(InValue)
		, Y(InValue)
		, Z(InValue)
		, W(InValue)
	{
	}

	explicit TIntVector4(const TIntVector3<IntType>& InValue, IntType InW = 0)
		: X(InValue.X)
		, Y(InValue.Y)
		, Z(InValue.Z)
		, W(InW)
	{
	}

	TIntVector4(EForceInit)
		: X(0)
		, Y(0)
		, Z(0)
		, W(0)
	{
	}

	/**
	 * Converts to another int type. Checks that the cast will succeed.
	 */
	template <typename OtherIntType>
	explicit TIntVector4(TIntVector4<OtherIntType> Other)
		: X(IntCastChecked<IntType>(Other.X))
		, Y(IntCastChecked<IntType>(Other.Y))
		, Z(IntCastChecked<IntType>(Other.Z))
		, W(IntCastChecked<IntType>(Other.W))
	{
	}

	// Workaround for clang deprecation warnings for deprecated XYZW member in implicitly-defined special member functions
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TIntVector4(TIntVector4&&) = default;
	TIntVector4(const TIntVector4&) = default;
	TIntVector4& operator=(TIntVector4&&) = default;
	TIntVector4& operator=(const TIntVector4&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return const reference to component.
	 */
	const IntType& operator()(int32 ComponentIndex) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZW[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return reference to component.
	 */
	IntType& operator()(int32 ComponentIndex)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZW[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return const reference to component.
	 */
	const IntType& operator[](int32 ComponentIndex) const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZW[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return reference to component.
	 */
	IntType& operator[](int32 ComponentIndex)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return XYZW[ComponentIndex];
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * Compares points for equality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are equal, false otherwise..
	 */
	bool operator==(const TIntVector4& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
	}

	/**
	 * Compares points for inequality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are not equal, false otherwise..
	 */
	bool operator!=(const TIntVector4& Other) const
	{
		return X != Other.X || Y != Other.Y || Z != Other.Z || W != Other.W;
	}

	/**
	 * Multiplies this vector with another vector, using component-wise multiplication.
	 *
	 * @param Other The point to multiply with.
	 * @return Reference to this point after multiplication.
	 */
	TIntVector4& operator*=(const TIntVector4& Other)
	{
		X *= Other.X;
		Y *= Other.Y;
		Z *= Other.Z;
		W *= Other.W;

		return *this;
	}

	/**
	 * Scales this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return Reference to this point after multiplication.
	 */
	TIntVector4& operator*=(IntType Scale)
	{
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		W *= Scale;

		return *this;
	}

	/**
	 * Divides this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after division.
	 */
	TIntVector4& operator/=(IntType Divisor)
	{
		X /= Divisor;
		Y /= Divisor;
		Z /= Divisor;
		W /= Divisor;

		return *this;
	}

	/**
	 * Remainder of division of this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after remainder.
	 */
	TIntVector4& operator%=(IntType Divisor)
	{
		X %= Divisor;
		Y %= Divisor;
		Z %= Divisor;
		W %= Divisor;

		return *this;
	}

	/**
	 * Adds to this point.
	 *
	 * @param Other The point to add to this point.
	 * @return Reference to this point after addition.
	 */
	TIntVector4& operator+=(const TIntVector4& Other)
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;
		W += Other.W;

		return *this;
	}

	/**
	 * Subtracts from this point.
	 *
	 * @param Other The point to subtract from this point.
	 * @return Reference to this point after subtraction.
	 */
	TIntVector4& operator-=(const TIntVector4& Other)
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;
		W -= Other.W;

		return *this;
	}

	/**
	 * Gets the result of component-wise multiplication of this point by another.
	 *
	 * @param Other The point to multiply with.
	 * @return The result of multiplication.
	 */
	TIntVector4 operator*(const TIntVector4& Other) const
	{
		return TIntVector4(*this) *= Other;
	}

	/**
	 * Gets the result of scaling on this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return A new scaled int point.
	 */
	TIntVector4 operator*(IntType Scale) const
	{
		return TIntVector4(*this) *= Scale;
	}

	/**
	 * Gets the result of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new divided int point.
	 */
	TIntVector4 operator/(IntType Divisor) const
	{
		return TIntVector4(*this) /= Divisor;
	}

	/**
	 * Gets the remainder of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new remainder int point.
	 */
	TIntVector4 operator%(IntType Divisor) const
	{
		return TIntVector4(*this) %= Divisor;
	}

	/**
	 * Gets the result of addition on this point.
	 *
	 * @param Other The other point to add to this.
	 * @return A new combined int point.
	 */
	TIntVector4 operator+(const TIntVector4& Other) const
	{
		return TIntVector4(*this) += Other;
	}

	/**
	 * Gets the result of subtraction from this point.
	 *
	 * @param Other The other point to subtract from this.
	 * @return A new subtracted int point.
	 */
	TIntVector4 operator-(const TIntVector4& Other) const
	{
		return TIntVector4(*this) -= Other;
	}

	/**
	 * Shifts all components to the right.
	 *
	 * @param Shift The number of bits to shift.
	 * @return A new shifted int point.
	 */
	TIntVector4 operator>>(IntType Shift) const
	{
		return TIntVector4(X >> Shift, Y >> Shift, Z >> Shift, W >> Shift);
	}

	/**
	 * Shifts all components to the left.
	 *
	 * @param Shift The number of bits to shift.
	 * @return A new shifted int point.
	 */
	TIntVector4 operator<<(IntType Shift) const
	{
		return TIntVector4(X << Shift, Y << Shift, Z << Shift, W << Shift);
	}

	/**
	 * Component-wise AND.
	 *
	 * @param Value Number to AND with the each component.
	 * @return A new shifted int point.
	 */
	TIntVector4 operator&(IntType Value) const
	{
		return TIntVector4(X & Value, Y & Value, Z & Value, W & Value);
	}

	/**
	 * Component-wise OR.
	 *
	 * @param Value Number to OR with the each component.
	 * @return A new shifted int point.
	 */
	TIntVector4 operator|(IntType Value) const
	{
		return TIntVector4(X | Value, Y | Value, Z | Value, W | Value);
	}

	/**
	 * Component-wise XOR.
	 *
	 * @param Value Number to XOR with the each component.
	 * @return A new shifted int point.
	 */
	TIntVector4 operator^(IntType Value) const
	{
		return TIntVector4(X ^ Value, Y ^ Value, Z ^ Value, W ^ Value);
	}

	/**
	* Serializes the Vector4.
	*
	* @param Ar The archive to serialize into.
	* @param Vector The vector to serialize.
	* @return Reference to the Archive after serialization.
	*/
	friend FArchive& operator<<(FArchive& Ar, TIntVector4& Vector)
	{
		return Ar << Vector.X << Vector.Y << Vector.Z << Vector.W;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, TIntVector4& Vector)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("X"), Vector.X);
		Record << SA_VALUE(TEXT("Y"), Vector.Y);
		Record << SA_VALUE(TEXT("Z"), Vector.Z);
		Record << SA_VALUE(TEXT("W"), Vector.W);
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	bool SerializeFromMismatchedTag(FName StructTag, FArchive& Ar);
};

/**
 * Creates a hash value from an IntVector2.
 *
 * @param Vector the vector to create a hash value for
 * @return The hash value from the components
 */
template<typename T>
uint32 GetTypeHash(const TIntVector2<T>& Vector)
{
	// Note: this assumes there's no padding in Vector that could contain uncompared data.
	return FCrc::MemCrc32(&Vector, sizeof(Vector));
}

/**
 * Creates a hash value from an IntVector3.
 *
 * @param Vector the vector to create a hash value for
 * @return The hash value from the components
 */
template<typename T>
uint32 GetTypeHash(const TIntVector3<T>& Vector)
{
	// Note: this assumes there's no padding in Vector that could contain uncompared data.
	return FCrc::MemCrc_DEPRECATED(&Vector, sizeof(Vector));
}

/**
 * Creates a hash value from an IntVector4.
 *
 * @param Vector the vector to create a hash value for
 * @return The hash value from the components
 */
template<typename T>
uint32 GetTypeHash(const TIntVector4<T>& Vector)
{
	// Note: this assumes there's no padding in Vector that could contain uncompared data.
	return FCrc::MemCrc32(&Vector, sizeof(Vector));
}

} //! namespace UE::Math

template <> struct TIsPODType<FInt32Vector2>  { enum { Value = true }; };
template <> struct TIsPODType<FUint32Vector2> { enum { Value = true }; };
template <> struct TIsPODType<FInt32Vector3>  { enum { Value = true }; };
template <> struct TIsPODType<FUint32Vector3> { enum { Value = true }; };
template <> struct TIsPODType<FInt32Vector4>  { enum { Value = true }; };
template <> struct TIsPODType<FUint32Vector4> { enum { Value = true }; };

template <> struct TIsUECoreVariant<FInt32Vector2>  { enum { Value = true }; };
template <> struct TIsUECoreVariant<FUint32Vector2> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FInt32Vector3>  { enum { Value = true }; };
template <> struct TIsUECoreVariant<FUint32Vector3> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FInt32Vector4>  { enum { Value = true }; };
template <> struct TIsUECoreVariant<FUint32Vector4> { enum { Value = true }; };

template <> struct TIsPODType<FInt64Vector2> { enum { Value = true }; };
template <> struct TIsPODType<FUint64Vector2> { enum { Value = true }; };
template <> struct TIsPODType<FInt64Vector3> { enum { Value = true }; };
template <> struct TIsPODType<FUint64Vector3> { enum { Value = true }; };
template <> struct TIsPODType<FInt64Vector4> { enum { Value = true }; };
template <> struct TIsPODType<FUint64Vector4> { enum { Value = true }; };

template <> struct TIsUECoreVariant<FInt64Vector2> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FUint64Vector2> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FInt64Vector3> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FUint64Vector3> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FInt64Vector4> { enum { Value = true }; };
template <> struct TIsUECoreVariant<FUint64Vector4> { enum { Value = true }; };


template<>
inline bool FInt32Vector2::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, IntVector2, Int32Vector2, Int64Vector2);
}

template<>
inline bool FInt64Vector2::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, IntVector2, Int64Vector2, Int32Vector2);
}

template<>
inline bool FUint32Vector2::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, UintVector2, Uint32Vector2, Uint64Vector2);
}

template<>
inline bool FUint64Vector2::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, UintVector2, Uint64Vector2, Uint32Vector2);
}

template<>
inline bool FInt32Vector3::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, IntVector, Int32Vector, Int64Vector);
}

template<>
inline bool FInt64Vector3::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, IntVector, Int64Vector, Int32Vector);
}

template<>
inline bool FUint32Vector3::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, UintVector, Uint32Vector, Uint64Vector);
}

template<>
inline bool FUint64Vector3::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, UintVector, Uint64Vector, Uint32Vector);
}

template<>
inline bool FInt32Vector4::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, IntVector4, Int32Vector4, Int64Vector4);
}

template<>
inline bool FInt64Vector4::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, IntVector4, Int64Vector4, Int32Vector4);
}

template<>
inline bool FUint32Vector4::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, UintVector4, Uint32Vector4, Uint64Vector4);
}

template<>
inline bool FUint64Vector4::SerializeFromMismatchedTag(FName StructTag, FArchive& Ar)
{
	return UE_SERIALIZE_VARIANT_FROM_MISMATCHED_TAG(Ar, UintVector4, Uint64Vector4, Uint32Vector4);
}
