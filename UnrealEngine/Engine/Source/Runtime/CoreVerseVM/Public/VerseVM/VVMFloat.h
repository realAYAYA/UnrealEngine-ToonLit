// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h" // IWYU pragma: keep
#include "Templates/TypeCompatibleBytes.h"

namespace Verse
{
struct VValue;

// Encapsulates a Verse float, which has a few deviations from IEEE-754 semantics for the sake of
// providing extensional equality; i.e. that iff two values compare as equal, there's no way to
// distinguish them.
//
// IEEE-754 floats don't have extensional equality for two reasons:
// - Zeroes of opposite signs compare as equal, but when used as a divisor produce different infinities.
// - NaNs compare as unequal to themselves.
// We resolve this by defining division by negative zero to produce positive infinity, and collapsing
// NaNs to a single canonical NaN that compares as equal to itself and unordered with all numbers.
struct VFloat
{
	friend struct VValue;

	// Default initialize to zero.
	constexpr VFloat()
		: Value(0.0)
	{
	}

	explicit VFloat(double InValue)
		: Value(InValue)
	{
	}

	static VFloat NaN()
	{
		return BitCast<VFloat>(0x7ff8'0000'0000'0000);
	}

	static VFloat Infinity()
	{
		return BitCast<VFloat>(0x7ff0'0000'0000'0000);
	}

	double AsDouble() const
	{
		return NormalizeSignedZero().Value;
	}

	uint64 ReinterpretAsUInt64() const
	{
		return BitCast<uint64>(Value);
	}

	// Purify a potentially impure float (as defined by VValue).
	COREVERSEVM_API VFloat Purify() const;

	// Predicates for different FP specials.
	COREVERSEVM_API bool IsFinite() const;
	COREVERSEVM_API bool IsInfinite() const;
	COREVERSEVM_API bool IsNaN() const;

	// Arithmetic operations in a non-fast-math environment (IEEE compliant).
	// These can (mostly) be removed when callers are guaranteed to not be
	// compiled with fast-math or similar enabled.
	friend constexpr VFloat operator+(VFloat Operand)
	{
		return Operand;
	}
	COREVERSEVM_API friend VFloat operator-(VFloat Operand);
	COREVERSEVM_API friend VFloat operator+(VFloat Left, VFloat Right);
	COREVERSEVM_API friend VFloat operator-(VFloat Left, VFloat Right);
	COREVERSEVM_API friend VFloat operator*(VFloat Left, VFloat Right);
	COREVERSEVM_API friend VFloat operator/(VFloat Left, VFloat Right);

	VFloat& operator+=(VFloat Right)
	{
		return *this = *this + Right;
	}
	VFloat& operator-=(VFloat Right)
	{
		return *this = *this - Right;
	}
	VFloat& operator*=(VFloat Right)
	{
		return *this = *this * Right;
	}
	VFloat& operator/=(VFloat Right)
	{
		return *this = *this / Right;
	}

	// We use an ordering relationship different from the default IEEE float
	// ordering (because we require NaNs to compare equal to each other).
	COREVERSEVM_API friend bool operator==(VFloat Left, VFloat Right);
	COREVERSEVM_API friend bool operator<(VFloat Left, VFloat Right);
	COREVERSEVM_API friend bool operator<=(VFloat Left, VFloat Right);

	// The remaining relations can be inferred from the relations above.

	friend inline bool operator!=(VFloat Left, VFloat Right)
	{
		return !(Left == Right);
	}

	friend inline bool operator>(VFloat Left, VFloat Right)
	{
		return Right < Left;
	}

	friend inline bool operator>=(VFloat Left, VFloat Right)
	{
		return Right <= Left;
	}

	// Ranking function that turns a double into an int64 that defines a total order
	// compatible with the ordering implied for floats, to be precise
	//
	//  a <  b  =>  Ranking(a) <  Ranking(b)
	//  a == b  <=> Ranking(a) == Ranking(b)
	//
	// For Less(a,b), we only have implication in one direction because when a single
	// NaN is involved, the strict "less" ordering relationship is partial. In the total
	// order implied by Ranking, NaN compares larger than all other floats. Unlike
	// normal IEEE semantics, NaNs compare equal to each other in our ordering, so for
	// Equal we have full equivalence.
	//
	// Ranking can be used directly or as a key for sorted maps and hashes.
	COREVERSEVM_API int64 Ranking() const;

private:
	double Value;

	COREVERSEVM_API VFloat NormalizeSignedZero() const;
};
} // namespace Verse
