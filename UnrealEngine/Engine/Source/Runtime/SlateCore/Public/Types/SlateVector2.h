// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

namespace UE::Slate
{

FORCEINLINE static FVector2f CastToVector2f(FVector2d InValue)
{
	const float X = UE_REAL_TO_FLOAT(InValue.X);
	const float Y = UE_REAL_TO_FLOAT(InValue.Y);
#if 0
	ensureAlways(FMath::IsNearlyEqual((double)X, InValue.X));
	ensureAlways(FMath::IsNearlyEqual((double)Y, InValue.Y));
#endif
	return FVector2f(X, Y);
}

/**
 * Structure for deprecating FVector2D to FVector2f
 */
struct FDeprecateVector2D
{
public:
	FDeprecateVector2D() = default;
	explicit FDeprecateVector2D(FVector2f InValue)
		: X(InValue.X)
		, Y(InValue.Y)
	{
	}
	explicit FDeprecateVector2D(float InX, float InY)
		: X(InX)
		, Y(InY)
	{
	}

public:
	union
	{
		struct
		{
			float X;
			float Y;
		};

		UE_DEPRECATED(all, "For internal use only")
		float XY[2];
	};

public:
	operator FVector2d() const
	{
		return FVector2d(X, Y);
	}

	operator FVector2f() const
	{
		return AsVector2f();
	}

public:
	/**  Get the length (magnitude) of this vector. */
	float Size() const
	{
		return AsVector2f().Size();
	}

	/** Get the length (magnitude) of this vector. */
	float Length() const
	{
		return AsVector2f().Length();
	}

	/** Get the squared length of this vector. */
	float SizeSquared() const
	{
		return AsVector2f().SizeSquared();
	}

	/**  Get the squared length of this vector. */
	float SquaredLength() const
	{
		return AsVector2f().SquaredLength();
	}

	/** Checks whether all components of the vector are exactly zero. */
	bool IsZero() const
	{
		return AsVector2f().IsZero();
	}

	/** Get this vector as an Int Point. */
	FIntPoint IntPoint() const
	{
		return AsVector2f().IntPoint();
	}

	/** Get this vector as a vector where each component has been rounded to the nearest int. */
	FDeprecateVector2D RoundToVector() const
	{
		return FDeprecateVector2D(AsVector2f().RoundToVector());
	}

	/** Gets a specific component of the vector. */
	float Component(int32 Index) const
	{
		return AsVector2f().Component(Index);
	}

public:
	FORCEINLINE FDeprecateVector2D operator+(const FVector2f V) const
	{
		return FDeprecateVector2D(AsVector2f() + V);
	}
	FORCEINLINE FDeprecateVector2D operator+(const FVector2d V) const
	{
		return FDeprecateVector2D(AsVector2f() + CastToVector2f(V));
	}
	FORCEINLINE FDeprecateVector2D operator+(const FDeprecateVector2D V) const
	{
		return FDeprecateVector2D(AsVector2f() + V.AsVector2f());
	}

	FORCEINLINE FDeprecateVector2D operator-(const FVector2f V) const
	{
		return FDeprecateVector2D(AsVector2f() - V);
	}
	FORCEINLINE FDeprecateVector2D operator-(const FVector2d V) const
	{
		return FDeprecateVector2D(AsVector2f() - CastToVector2f(V));
	}
	FORCEINLINE FDeprecateVector2D operator-(const FDeprecateVector2D V) const
	{
		return FDeprecateVector2D(AsVector2f() - V.AsVector2f());
	}

	FORCEINLINE FDeprecateVector2D operator*(const FVector2f V) const
	{
		return FDeprecateVector2D(AsVector2f() * V);
	}
	FORCEINLINE FDeprecateVector2D operator*(const FVector2d V) const
	{
		return FDeprecateVector2D(AsVector2f() * CastToVector2f(V));
	}
	FORCEINLINE FDeprecateVector2D operator*(const FDeprecateVector2D V) const
	{
		return FDeprecateVector2D(AsVector2f() * V.AsVector2f());
	}

	FORCEINLINE FDeprecateVector2D operator/(const FVector2f V) const
	{
		return FDeprecateVector2D(AsVector2f() / V);
	}
	FORCEINLINE FDeprecateVector2D operator/(const FVector2d V) const
	{
		return FDeprecateVector2D(AsVector2f() / CastToVector2f(V));
	}
	FORCEINLINE FDeprecateVector2D operator/(const FDeprecateVector2D V) const
	{
		return FDeprecateVector2D(AsVector2f() / V.AsVector2f());
	}

	FORCEINLINE FDeprecateVector2D operator+(float Scale) const
	{
		return FDeprecateVector2D(AsVector2f() + Scale);
	}

	FORCEINLINE FDeprecateVector2D operator-(float Scale) const
	{
		return FDeprecateVector2D(AsVector2f() - Scale);
	}

	FORCEINLINE FDeprecateVector2D operator*(float Scale) const
	{
		return FDeprecateVector2D(AsVector2f() * Scale);
	}

	FORCEINLINE FDeprecateVector2D operator/(float Scale) const
	{
		return FDeprecateVector2D(AsVector2f() / Scale);
	}

public:
	bool operator==(FVector2f V) const
	{
		return V == AsVector2f();
	}
	bool operator==(FVector2d V) const
	{
		return CastToVector2f(V) == AsVector2f();
	}
	bool operator==(FDeprecateVector2D V) const
	{
		return X == V.X && Y == V.Y;
	}

	bool operator!=(FVector2f V) const
	{
		return V != AsVector2f();
	}
	bool operator!=(FVector2d V) const
	{
		return CastToVector2f(V) != AsVector2f();
	}
	bool operator!=(FDeprecateVector2D V) const
	{
		return X != V.X || Y != V.Y;
	}

private:
	FVector2f AsVector2f() const
	{
		return FVector2f(X, Y);
	}
};

} // namespace
