// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *		notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *		notice, this list of conditions and the following disclaimer in the
 *		documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"
#include "VerseVM/VVMFloat.h"
#include <cstdint>

namespace Verse
{
struct VHeapInt final : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	using Digit = uint32;

	static VHeapInt& New(FAllocationContext Context, uint32 NumWords)
	{
		return *CreateWithLength(Context, NumWords);
	}

	static VHeapInt& New(FAllocationContext Context, bool Sign, TArrayView<Digit> Digits)
	{
		return *CreateWithDigits(Context, Sign, Digits);
	}

	FORCENOINLINE static VHeapInt& FromInt64(FAllocationContext Context, int64 Int64)
	{
		return *CreateFrom(Context, Int64);
	}

	COREUOBJECT_API bool IsInt32() const;
	COREUOBJECT_API int32 AsInt32() const;

	COREUOBJECT_API bool IsInt64() const;
	COREUOBJECT_API int64 AsInt64() const;

	COREUOBJECT_API VFloat ConvertToFloat() const;

	COREUOBJECT_API static bool Equals(VHeapInt& X, VHeapInt& Y);

	COREUOBJECT_API static VHeapInt* CreateZero(FAllocationContext Context);

	COREUOBJECT_API static VHeapInt* Add(FRunningContext, VHeapInt& X, VHeapInt& Y);
	COREUOBJECT_API static VHeapInt* Sub(FRunningContext, VHeapInt& X, VHeapInt& Y);
	COREUOBJECT_API static VHeapInt* Multiply(FRunningContext, VHeapInt& X, VHeapInt& Y);
	COREUOBJECT_API static VHeapInt* Divide(FRunningContext, VHeapInt& X, VHeapInt& Y, bool* bOutHasNonZeroRemainder = nullptr);
	COREUOBJECT_API static VHeapInt* Modulo(FRunningContext, VHeapInt& X, VHeapInt& Y);
	COREUOBJECT_API static VHeapInt* UnaryMinus(FRunningContext, VHeapInt& X);

	enum class ComparisonResult
	{
		Equal,
		Undefined,
		GreaterThan,
		LessThan
	};

	static ComparisonResult Compare(VHeapInt& X, VHeapInt& Y);

	inline bool IsZero() const
	{
		check(GetLength() || !GetSign());
		return GetLength() == 0;
	}

	// false means positive, true means negative
	bool GetSign() const { return static_cast<bool>(Sign); }
	Digit GetDigit(const uint32 Index) const;
	unsigned int GetLength() const { return Length; }

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

	COREUOBJECT_API static void SerializeImpl(VHeapInt*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	explicit VHeapInt(FAllocationContext Context, uint32 NumWords)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Length(NumWords)
	{
	}

	enum class InitializationType
	{
		None,
		WithZero
	};
	void Initialize(InitializationType);

	static VHeapInt* CreateWithLength(FAllocationContext, uint32 length);
	COREUOBJECT_API static VHeapInt* CreateFrom(FAllocationContext, int64 Value);
	static VHeapInt* CreateWithDigits(FAllocationContext, bool Sign, TArrayView<Digit> Digits);

	void SetSign(bool NewSign) { Sign = NewSign; }

	void SetDigit(const uint32 Index, Digit Value); // Use only when initializing.
	VHeapInt* RightTrim(FRunningContext);

	static VHeapInt* CreateFromImpl(FAllocationContext, uint64 Value, bool sign);

	static constexpr unsigned BitsPerByte = 8;
	static constexpr unsigned DigitBits = sizeof(Digit) * BitsPerByte;
	static constexpr unsigned HalfDigitBits = DigitBits / 2;
	static constexpr Digit HalfDigitMask = (1ull << HalfDigitBits) - 1;
	static constexpr int MaxInt = 0x7FFFFFFF;

	// The maximum length that the current implementation supports would be
	// MaxInt / DigitBits. However, we use A lower limit for now, because
	// raising it later is easier than lowering it.
	// Support up to 1 million bits.
	static constexpr unsigned MaxLengthBits = 1024 * 1024;
	static constexpr unsigned MaxLength = MaxLengthBits / DigitBits;
	static_assert(MaxLengthBits % DigitBits == 0);

	static ComparisonResult AbsoluteCompare(const VHeapInt& X, const VHeapInt& Y);
	static void MultiplyAccumulate(const VHeapInt& Multiplicand, Digit Multiplier, VHeapInt* Accumulator, uint32 AccumulatorIndex);

	// Digit arithmetic helpers.
	static Digit DigitAdd(Digit A, Digit B, Digit& Carry);
	static Digit DigitSub(Digit A, Digit B, Digit& Borrow);
	static Digit DigitMul(Digit A, Digit B, Digit& High);

	static VHeapInt* Copy(FRunningContext, const VHeapInt& X);

	static VHeapInt* AbsoluteAdd(FRunningContext, VHeapInt& X, VHeapInt& Y, bool ResultSign);
	static VHeapInt* AbsoluteSub(FRunningContext, VHeapInt& X, VHeapInt& Y, bool ResultSign);

	Digit AbsoluteInplaceAdd(const VHeapInt& Summand, uint32 StartIndex);
	Digit AbsoluteInplaceSub(const VHeapInt& Subtrahend, uint32 StartIndex);
	void InplaceRightShift(uint32 Shift);

	static bool AbsoluteDivWithDigitDivisor(FRunningContext Context, const VHeapInt& X, Digit Divisor, VHeapInt** Quotient, Digit& Remainder);
	static void AbsoluteDivWithHeapIntDivisor(FRunningContext Context, const VHeapInt& Dividend, const VHeapInt& Divisor, VHeapInt** Quotient, VHeapInt** Remainder, bool* bOutHasNonZeroRemainder = nullptr);
	inline static Digit DigitDiv(Digit high, Digit low, Digit divisor, Digit& remainder);

	enum class LeftShiftMode
	{
		SameSizeResult,
		AlwaysAddOneDigit
	};

	static VHeapInt* AbsoluteLeftShiftAlwaysCopy(FRunningContext Context, const VHeapInt& X, uint32 Shift, LeftShiftMode Mode);
	inline static bool ProductGreaterThan(Digit Factor1, Digit Factor2, Digit High, Digit Low);

	static void InternalMultiplyAdd(const VHeapInt& Source, Digit Factor, Digit Summand, uint32 N, VHeapInt* Result);

	inline Digit* DataStorage()
	{
		return Digits;
	}

	inline const Digit* DataStorage() const
	{
		return Digits;
	}

	const uint32 Length;
	uint8 Sign{false}; // false means positive, true means negative
	Digit Digits[];
};

inline VHeapInt::Digit VHeapInt::GetDigit(const uint32 Index) const
{
	check(Index < GetLength());
	return DataStorage()[Index];
}

inline void VHeapInt::SetDigit(uint32 Index, Digit Value)
{
	check(Index < GetLength());
	DataStorage()[Index] = Value;
}

inline uint32 GetTypeHash(const VHeapInt& HeapInt)
{
	uint32 Result = ::GetTypeHash(HeapInt.GetSign());

	for (uint32 I = 0; I < HeapInt.GetLength(); I++)
	{
		::HashCombineFast(Result, ::GetTypeHash(HeapInt.GetDigit(I)));
	}

	return Result;
}
} // namespace Verse
#endif // WITH_VERSE_VM
