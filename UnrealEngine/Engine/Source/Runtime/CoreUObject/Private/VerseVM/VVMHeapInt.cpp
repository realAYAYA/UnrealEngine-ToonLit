// Copyright Epic Games, Inc. All Rights Reserved.

/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Parts of the implementation below:
 *
 * Copyright 2017 the V8 project authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 *
 * Copyright (c) 2014 the Dart project authors.  Please see the AUTHORS file [1]
 * for details. All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE file [2].
 *
 * [1] https://github.com/dart-lang/sdk/blob/master/AUTHORS
 * [2] https://github.com/dart-lang/sdk/blob/master/LICENSE
 *
 * Copyright 2009 The Go Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file [3].
 *
 * [3] https://golang.org/LICENSE
 */

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMHeapInt.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

// We're assuming 32-bits for Digit and this needs to match that (but is signed unlike our Digit's).
using SignedDigit = int32_t;

DEFINE_DERIVED_VCPPCLASSINFO(VHeapInt);
TGlobalTrivialEmergentTypePtr<&VHeapInt::StaticCppClassInfo> VHeapInt::GlobalTrivialEmergentType;

namespace
{
void ToString(FStringBuilderBase& Builder, const VHeapInt& Value)
{
	Builder.AppendChar(Value.GetSign() ? TCHAR('-') : TCHAR('+'));

	if (Value.IsZero())
	{
		Builder.AppendChar(TCHAR('0'));
	}
	else
	{
		for (int32 I = Value.GetLength() - 1; I >= 0; --I)
		{
			Builder.Appendf(TEXT(" %08X"), Value.GetDigit(I));
		}
	}

	Builder.Append(TEXT("h"));
}

VHeapInt& Parse(FAllocationContext Context, FStringView Text)
{
	Text = Text.TrimStartAndEnd();

	// Strip the sign
	bool Sign = false;
	if (Text.Len() > 0)
	{
		if (Text[0] == '-')
		{
			Sign = true;
			Text = Text.RightChop(1).TrimStart();
		}
		else if (Text[0] == '+')
		{
			Text = Text.RightChop(1).TrimStart();
		}
	}

	// Strip the ending 'h'
	if (Text.Len() > 0 && Text[Text.Len() - 1] == 'h')
	{
		Text = Text.LeftChop(1).TrimEnd();
	}

	// Check for zero
	if (Text.Len() == 1 && Text[0] == '0')
	{
		return *VHeapInt::CreateZero(Context);
	}

	// Fetch the digits
	TArray<VHeapInt::Digit> Digits;
	Digits.Reserve(8);
	while (Text.Len() > 0)
	{
		int32 DigitEnd;
		if (!Text.FindChar(' ', DigitEnd))
		{
			DigitEnd = Text.Len();
		}
		FStringView Digit = Text.Left(DigitEnd);
		Text = Text.RightChop(DigitEnd).TrimStart();

		FString NullText(Digit);
		TCHAR* End;
		uint64 DigitValue = FCString::Strtoui64(*NullText, &End, 16);
		Digits.Add(static_cast<VHeapInt::Digit>(DigitValue));
	}

	// The digits are most significant to least significant.  However, internally they
	// are least to most.
	for (int32 Lhs = 0, Rhs = Digits.Num() - 1; Lhs < Rhs; ++Lhs, --Rhs)
	{
		Swap(Digits[Lhs], Digits[Rhs]);
	}

	// Create the int
	return VHeapInt::New(Context, Sign, TArrayView<VHeapInt::Digit>(Digits));
}

} // namespace

template <typename TVisitor>
void VHeapInt::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		if (Visitor.IsLoading())
		{
			V_DIE("VHeapInt isn't mutable and can not be loaded through the abstract visitors, use the Serialization method");
		}
		else
		{
			TStringBuilder<128> Builder;
			ToString(Builder, *this);
			FString ScratchString(Builder.ToView());
			Visitor.Visit(ScratchString, TEXT("Value"));
		}
	}
}

void VHeapInt::SerializeImpl(VHeapInt*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		FString ScratchString;
		Visitor.Visit(ScratchString, TEXT("Value"));
		This = &Parse(Context, ScratchString);
	}
	else
	{
		TStringBuilder<128> Builder;
		ToString(Builder, *This);
		FString ScratchString(Builder.ToView());
		Visitor.Visit(ScratchString, TEXT("Value"));
	}
}

void VHeapInt::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	ToString(Builder, *this);
}

bool VHeapInt::IsInt32() const
{
	static_assert(sizeof(Digit) == 4);
	if (!IsInt64())
	{
		return false;
	}
	const int64 I64 = AsInt64();
	return I64 >= INT32_MIN && I64 <= INT32_MAX;
}

int32 VHeapInt::AsInt32() const
{
	static_assert(sizeof(Digit) == 4);
	check(IsInt32());
	const int64 I64 = AsInt64();
	return static_cast<int32>(I64);
}

bool VHeapInt::IsInt64() const
{
	static_assert(sizeof(Digit) == 4);

	switch (GetLength())
	{
		case 0:
		case 1:
			return true;
		case 2:
			if (GetSign())
			{
				return (GetDigit(1) < 0x80000000) || (GetDigit(1) == 0x80000000 && GetDigit(0) == 0x00000000);
			}
			return GetDigit(1) < 0x80000000;
		default:
			return false;
	}
}

int64 VHeapInt::AsInt64() const
{
	static_assert(sizeof(Digit) == 4);
	checkSlow(IsInt64());

	if (GetLength() == 0)
	{
		return 0;
	}

	if (GetLength() == 1)
	{
		int64 Result64 = int64(GetDigit(0));

		return GetSign() ? -Result64 : Result64;
	}

	if (GetSign())
	{
		const uint64 U64 = uint64(GetDigit(0)) | (uint64(GetDigit(1)) << 32);
		return -static_cast<int64>(U64 - 1) - 1;
	}

	return int64(GetDigit(0)) | (int64(GetDigit(1)) << 32);
}

// This function implements the IEEE round-ties-to-even rounding mode
VFloat VHeapInt::ConvertToFloat() const
{
	static_assert(sizeof(Digit) == 4, "We assume a digit size of 32 bit in the code below or else it will malfunction.");

	constexpr uint32 IEEEMantissaBits = 53;  // Including the implicit bit, only 52 are stored
	constexpr uint32 IEEEMaxValidExp = 2046; // 2047 is reserved for inf & NaN
	constexpr uint32 IEEEExpBias = 1023;     // 1023 means 0

	// Statistically it is very likely that we fit into an int64 so handle this special case upfront
	if (IsInt64())
	{
		return VFloat(double(AsInt64()));
	}

	checkSlow(Length >= 2); // Since we handled the int64 case above

	const Digit* TopDigit = &Digits[Length];

	// Get highest digit
	uint64 Top1 = *--TopDigit;

	// Compute shift required to bring Top1 into mantissa position _plus 1_ (so we can examine the bit below the LSB)
	constexpr int32 ShiftOffset = IEEEMantissaBits - 32;
	int32 Shift = int32(FMath::CountLeadingZeros(int32(Top1))) + (ShiftOffset + 1);

	// Compute exponent
	uint64 Exponent = uint64(Length) * 32 + (IEEEExpBias + ShiftOffset) - Shift;
	if (Exponent > IEEEMaxValidExp)
	{
		// This number exceeds the maximum that a double can represent
		return VFloat(Sign ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
	}

	// Fetch additional digits
	uint64 Top2 = *--TopDigit;
	uint64 Top3 = 0;
	if (Length > 2 && Shift > 32)
	{
		Top3 = *--TopDigit;
	}

	// Compute (shifted) mantissa
	uint64 Mantissa = Top1 << Shift;
	if (Shift <= 32)
	{
		Mantissa |= Top2 >> (32 - Shift);
	}
	else
	{
		Mantissa |= (Top2 << (Shift - 32)) | (Top3 >> (64 - Shift));
	}

	// Shall we round up or down?
	// (Mantissa here contains the actual mantissa shifted up by one bit so we can examine the bit below its LSB)
	bool bRoundUp = ((Mantissa & 1) != 0);
	// Do we need to check for a tie?
	// (we only need to check if the mantissa is even and the bit below its LSB is 1)
	if ((Mantissa & 3) == 1)
	{
		// Test the lower bits that we already fetched from the Digit array
		bool bHasBitsFollowingTheBitBelowLSB;
		if (Shift <= 32)
		{
			bHasBitsFollowingTheBitBelowLSB = (Top2 & ((1ull << (32 - Shift)) - 1)) != 0;
		}
		else
		{
			bHasBitsFollowingTheBitBelowLSB = (Top3 & ((1ull << (64 - Shift)) - 1)) != 0;
		}
		// If any of those bits are non-zero we round up, otherwise we have to check further
		if (!bHasBitsFollowingTheBitBelowLSB)
		{
			// Look at all remaining digits we didn't check yet
			while (TopDigit > Digits)
			{
				if (*--TopDigit != 0)
				{
					bHasBitsFollowingTheBitBelowLSB = true;
					break;
				}
			}
			// If we found any more bits, we round up, otherwise down
			bRoundUp = bHasBitsFollowingTheBitBelowLSB;
		}
	}

	// Now that we know which way we want to round, get rid of that extra bit to yield the actual mantissa
	Mantissa >>= 1;

	if (bRoundUp)
	{
		// Round up by incrementing the mantissa
		++Mantissa;
		// In case the mantissa was all 1s, it might have overflown, check for that:
		if (Mantissa == (1ull << IEEEMantissaBits))
		{
			// It overflowed, correct for that
			Mantissa >>= 1;
			++Exponent;
			if (Exponent > IEEEMaxValidExp)
			{
				// This number exceeds the maximum that a double can represent
				return VFloat(Sign ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
			}
		}
	}

	// Combine into result
	uint64 BitResult = (Mantissa - (uint64(1) << 52)) | (Exponent << 52) | (uint64(Sign) << 63);
	double Result = BitCast<double>(BitResult);
	return VFloat(Result);
}

void VHeapInt::Initialize(InitializationType InitType)
{
	if (InitType == InitializationType::WithZero)
	{
		memset(DataStorage(), 0, GetLength() * sizeof(Digit));
	}
}

VHeapInt* VHeapInt::CreateZero(FAllocationContext Context)
{
	return CreateWithLength(Context, 0);
}

VHeapInt* VHeapInt::CreateWithLength(FAllocationContext Context, uint32 Length)
{
	check(Length <= MaxLength);

	VHeapInt* BigInt =
		new (Context.AllocateFastCell(offsetof(VHeapInt, Digits)
									  + sizeof(Digits[0]) * Length))
			VHeapInt{Context, Length};
	check(BigInt);

	return BigInt;
}

VHeapInt* VHeapInt::CreateWithDigits(FAllocationContext Context, bool Sign, TArrayView<Digit> Digits)
{
	VHeapInt* BigInt = CreateWithLength(Context, Digits.Num());
	BigInt->SetSign(Sign);
	for (uint32 Index = 0, EIndex = Digits.Num(); Index < EIndex; ++Index)
	{
		BigInt->SetDigit(Index, Digits[Index]);
	}
	return BigInt;
}

VHeapInt* VHeapInt::CreateFromImpl(FAllocationContext Context,
	uint64 Value,
	bool Sign)
{
	if (!Value)
	{
		return CreateZero(Context);
	}

	// This path is not just an optimization: because we do not call RightTrim
	// at the end of this function,
	// it would be a bug to create a BigInt with Length=2 in this case.
	if (sizeof(Digit) == 8 || Value <= UINT32_MAX)
	{
		VHeapInt* BigInt = CreateWithLength(Context, 1);
		BigInt->SetDigit(0, static_cast<Digit>(Value));
		BigInt->SetSign(Sign);
		return BigInt;
	}

	check(sizeof(Digit) == 4);
	VHeapInt* BigInt = CreateWithLength(Context, 2);

	Digit LowBits = static_cast<Digit>(Value & 0xffffffff);
	Digit HighBits = static_cast<Digit>((Value >> 32) & 0xffffffff);

	check(HighBits);

	BigInt->SetDigit(0, LowBits);
	BigInt->SetDigit(1, HighBits);
	BigInt->SetSign(Sign);

	return BigInt;
}

VHeapInt* VHeapInt::CreateFrom(FAllocationContext Context, int64 Value)
{
	uint64_t UnsignedValue;
	bool Sign = false;
	if (Value < 0)
	{
		UnsignedValue = static_cast<uint64_t>(-(Value + 1)) + 1;
		Sign = true;
	}
	else
	{
		UnsignedValue = Value;
	}
	return CreateFromImpl(Context, UnsignedValue, Sign);
}

static VHeapInt* Zero(FAllocationContext Context)
{
	return VHeapInt::CreateZero(Context);
}

VHeapInt* VHeapInt::Multiply(FRunningContext Context, VHeapInt& X, VHeapInt& Y)
{
	if (X.IsZero())
	{
		return &X;
	}
	if (Y.IsZero())
	{
		return &Y;
	}

	uint32 ResultLength = X.GetLength() + Y.GetLength();
	VHeapInt* Result = VHeapInt::CreateWithLength(Context, ResultLength);

	Result->Initialize(InitializationType::WithZero);

	for (uint32 I = 0; I < X.GetLength(); I++)
	{
		MultiplyAccumulate(Y, X.GetDigit(I), Result, I);
	}

	Result->SetSign(X.GetSign() != Y.GetSign());
	return Result->RightTrim(Context);
}

VHeapInt* VHeapInt::Divide(FRunningContext Context, VHeapInt& X, VHeapInt& Y, bool* bOutHasNonZeroRemainder /*= nullptr*/)
{
	// Division by 0 is a failure
	if (Y.IsZero())
	{
		// Should be unreachable from Verse as the divide by zero is handled as explicit failure
		if (bOutHasNonZeroRemainder)
		{
			*bOutHasNonZeroRemainder = false; // Set to _some_ consistent value
		}
		return nullptr;
	}

	if (VHeapInt::AbsoluteCompare(X, Y) == ComparisonResult::LessThan)
	{
		if (bOutHasNonZeroRemainder)
		{
			*bOutHasNonZeroRemainder = !X.IsZero();
		}
		return CreateZero(Context);
	}

	VHeapInt* Result = nullptr;
	bool ResultSign = X.GetSign() != Y.GetSign();
	if (Y.GetLength() == 1)
	{
		Digit Divisor = Y.GetDigit(0);
		if (Divisor == 1)
		{
			if (bOutHasNonZeroRemainder)
			{
				*bOutHasNonZeroRemainder = false; // Division by +/-1 is always exact
			}
			if (ResultSign == X.GetSign())
			{
				return Copy(Context, X);
			}
			return VHeapInt::UnaryMinus(Context, X);
		}

		Digit Remainder;
		AbsoluteDivWithDigitDivisor(Context, X, Divisor, &Result, Remainder);
		if (bOutHasNonZeroRemainder)
		{
			*bOutHasNonZeroRemainder = (Remainder != 0);
		}
	}
	else
	{
		AbsoluteDivWithHeapIntDivisor(Context, X, Y, &Result, nullptr, bOutHasNonZeroRemainder);
	}

	Result->SetSign(ResultSign);

	// Return a VHeapInt representing Result rounded towards 0 to the next integral value.
	return Result->RightTrim(Context);
}

// Divides {x} by {divisor}, returning the result in {quotient} and {remainder}.
// Mathematically, the contract is:
// quotient = (x - remainder) / divisor, with 0 <= remainder < divisor.
// If {quotient} is an empty handle, an appropriately sized BigInt will be
// allocated for it; otherwise the caller must ensure that it is big enough.
// {quotient} can be the same as {x} for an in-place division. {quotient} can
// also be nullptr if the caller is only interested in the remainder.
bool VHeapInt::AbsoluteDivWithDigitDivisor(FRunningContext Context, const VHeapInt& X, Digit Divisor, VHeapInt** Quotient, Digit& Remainder)
{
	Remainder = 0;
	if (Divisor == 1)
	{
		if (Quotient)
		{
			VHeapInt* Result = Copy(Context, X);
			if (UNLIKELY(!Result))
				return false;
			*Quotient = Result;
		}
		return true;
	}

	uint32 Length = X.GetLength();
	if (Quotient)
	{
		if (*Quotient == nullptr)
		{
			VHeapInt* Result = CreateWithLength(Context, Length);
			if (UNLIKELY(!Result))
				return false;
			*Quotient = Result;
		}

		for (int32 I = Length - 1; I >= 0; I--)
		{
			Digit Q = DigitDiv(Remainder, X.GetDigit(I), Divisor, Remainder);
			(*Quotient)->SetDigit(I, Q);
		}
	}
	else
	{
		for (int I = Length - 1; I >= 0; I--)
			DigitDiv(Remainder, X.GetDigit(I), Divisor, Remainder);
	}
	return true;
}

// Divides {dividend} by {divisor}, returning the result in {quotient} and
// {remainder}. Mathematically, the contract is:
// quotient = (dividend - remainder) / divisor, with 0 <= remainder < divisor.
// Both {quotient} and {remainder} are optional, for callers that are only
// interested in one of them.
// See Knuth, Volume 2, section 4.3.1, Algorithm D.
void VHeapInt::AbsoluteDivWithHeapIntDivisor(FRunningContext Context, const VHeapInt& Dividend, const VHeapInt& Divisor, VHeapInt** Quotient, VHeapInt** Remainder, bool* bOutHasNonZeroRemainder /*= nullptr*/)
{
	check(Divisor.GetLength() >= 2);
	check(Dividend.GetLength() >= Divisor.GetLength());

	// The unusual variable names inside this function are consistent with
	// Knuth'S book, as well as with Go'S implementation of this algorithm.
	// Maintaining this consistency is probably more useful than trying to
	// come up with more descriptive names for them.
	uint32 N = Divisor.GetLength();
	uint32 M = Dividend.GetLength() - N;

	// The quotient to be computed.
	VHeapInt* Q = nullptr;
	if (Quotient != nullptr)
	{
		Q = CreateWithLength(Context, M + 1);
	}

	// In each iteration, {QHatV} holds {divisor} * {current quotient digit}.
	// "V" is the book'S name for {divisor}, "QHat" the current quotient digit.
	VHeapInt* QHatV = CreateWithLength(Context, N + 1);

	// Left-shift inputs so that the divisor'S MSB is set. This is necessary
	// to prevent the digit-wise divisions (see digit_div call below) from
	// overflowing (they take a two digits wide input, and return a one digit
	// result).
	Digit LastDigit = Divisor.GetDigit(N - 1);
	uint32 Shift = FMath::CountLeadingZeros(LastDigit);
	const VHeapInt* ShiftedDivisor = &Divisor;
	if (Shift > 0)
	{
		ShiftedDivisor = AbsoluteLeftShiftAlwaysCopy(Context, Divisor, Shift, LeftShiftMode::SameSizeResult);
	}

	// Holds the (continuously updated) remaining part of the dividend, which
	// eventually becomes the remainder.
	VHeapInt* U = AbsoluteLeftShiftAlwaysCopy(Context, Dividend, Shift, LeftShiftMode::AlwaysAddOneDigit);

	// Iterate over the dividend'S digit (like the "grad school" algorithm).
	// {VN1} is the divisor'S most significant digit.
	Digit Vn1 = ShiftedDivisor->GetDigit(N - 1);
	for (int J = M; J >= 0; J--)
	{
		// Estimate the current iteration'S quotient digit (see Knuth for details).
		// {QHat} is the current quotient digit.
		Digit QHat = std::numeric_limits<Digit>::max();

		// {Ujn} is the dividend'S most significant remaining digit.
		Digit Ujn = U->GetDigit(J + N);
		if (Ujn != Vn1)
		{
			// {RHat} is the current iteration'S remainder.
			Digit Rhat = 0;

			// Estimate the current quotient digit by dividing the most significant
			// digits of dividend and divisor. The result will not be too small,
			// but could be a bit too large.
			QHat = DigitDiv(Ujn, U->GetDigit(J + N - 1), Vn1, Rhat);

			// Decrement the quotient estimate as needed by looking at the next
			// Digit, i.e. by testing whether
			// QHat * V_{n-2} > (Rhat << DigitBits) + u_{j+n-2}.
			Digit Vn2 = ShiftedDivisor->GetDigit(N - 2);
			Digit Ujn2 = U->GetDigit(J + N - 2);
			while (ProductGreaterThan(QHat, Vn2, Rhat, Ujn2))
			{
				QHat--;
				Digit PrevRhat = Rhat;
				Rhat += Vn1;
				// V[N-1] >= 0, so this tests for overflow.
				if (Rhat < PrevRhat)
				{
					break;
				}
			}
		}

		// Multiply the divisor with the current quotient digit, and subtract
		// it from the dividend. If there was "borrow", then the quotient digit
		// was one too high, so we must correct it and undo one subtraction of
		// the (shifted) divisor.
		InternalMultiplyAdd(*ShiftedDivisor, QHat, 0, N, QHatV);
		Digit C = U->AbsoluteInplaceSub(*QHatV, J);
		if (C)
		{
			C = U->AbsoluteInplaceAdd(*ShiftedDivisor, J);
			U->SetDigit(J + N, U->GetDigit(J + N) + C);
			QHat--;
		}

		if (Quotient != nullptr)
		{
			Q->SetDigit(J, QHat);
		}
	}

	if (Quotient != nullptr)
	{
		// Caller will right-trim.
		*Quotient = Q;
	}

	if (Remainder != nullptr)
	{
		U->InplaceRightShift(Shift);
		*Remainder = U;
	}

	if (bOutHasNonZeroRemainder != nullptr)
	{
		*bOutHasNonZeroRemainder = !U->IsZero();
	}
}

VHeapInt* VHeapInt::Modulo(FRunningContext Context, VHeapInt& X, VHeapInt& Y)
{
	if (Y.IsZero())
	{
		return nullptr;
	}

	if (VHeapInt::AbsoluteCompare(X, Y) == ComparisonResult::LessThan)
	{
		return CreateZero(Context);
	}

	VHeapInt* Remainder = nullptr;
	VHeapInt* Result = nullptr;
	if (Y.GetLength() == 1)
	{
		Digit Divisor = Y.GetDigit(0);
		if (Divisor == 1)
		{
			return CreateZero(Context);
		}

		Digit RemainderDigit;
		AbsoluteDivWithDigitDivisor(Context, X, Divisor, &Result, RemainderDigit);
		Remainder = CreateWithLength(Context, 1);
		Remainder->SetDigit(0, RemainderDigit);
	}
	else
	{
		AbsoluteDivWithHeapIntDivisor(Context, X, Y, &Result, &Remainder);
	}

	Remainder->SetSign(X.GetSign());
	return Remainder->RightTrim(Context);
}

// Returns whether (factor1 * factor2) > (high << digitBits) + low.
bool VHeapInt::ProductGreaterThan(Digit Factor1, Digit Factor2, Digit High, Digit Low)
{
	Digit ResultHigh;
	Digit ResultLow = DigitMul(Factor1, Factor2, ResultHigh);
	return ResultHigh > High || (ResultHigh == High && ResultLow > Low);
}

// Adds {summand} onto {this}, starting with {summand}'S 0th digit
// at {this}'S {startIndex}'th digit. Returns the "carry" (0 or 1).
VHeapInt::Digit VHeapInt::AbsoluteInplaceAdd(const VHeapInt& Summand, uint32 StartIndex)
{
	Digit Carry = 0;
	uint32 N = Summand.GetLength();
	check(GetLength() >= StartIndex + N);
	for (uint32 I = 0; I < N; I++)
	{
		Digit NewCarry = 0;
		Digit Sum = DigitAdd(GetDigit(StartIndex + I), Summand.GetDigit(I), NewCarry);
		Sum = DigitAdd(Sum, Carry, NewCarry);
		SetDigit(StartIndex + I, Sum);
		Carry = NewCarry;
	}

	return Carry;
}

// Subtracts {subtrahend} from {this}, starting with {subtrahend}'S 0th digit
// at {this}'S {startIndex}-th digit. Returns the "borrow" (0 or 1).
VHeapInt::Digit VHeapInt::AbsoluteInplaceSub(const VHeapInt& Subtrahend, uint32 StartIndex)
{
	Digit Borrow = 0;
	uint32 N = Subtrahend.GetLength();
	check(GetLength() >= StartIndex + N);
	for (uint32 I = 0; I < N; I++)
	{
		Digit NewBorrow = 0;
		Digit Difference = DigitSub(GetDigit(StartIndex + I), Subtrahend.GetDigit(I), NewBorrow);
		Difference = DigitSub(Difference, Borrow, NewBorrow);
		SetDigit(StartIndex + I, Difference);
		Borrow = NewBorrow;
	}

	return Borrow;
}

void VHeapInt::InplaceRightShift(uint32 Shift)
{
	check(Shift < DigitBits);
	check(!(GetDigit(0) & ((static_cast<Digit>(1) << Shift) - 1)));

	if (!Shift)
		return;

	Digit Carry = GetDigit(0) >> Shift;
	uint32 Last = GetLength() - 1;
	for (uint32 I = 0; I < Last; I++)
	{
		Digit D = GetDigit(I + 1);
		SetDigit(I, (D << (DigitBits - Shift)) | Carry);
		Carry = D >> Shift;
	}
	SetDigit(Last, Carry);
}

// Multiplies {source} with {factor} and adds {summand} to the result.
// {result} and {source} may be the same BigInt for inplace modification.
void VHeapInt::InternalMultiplyAdd(const VHeapInt& Source, Digit Factor, Digit Summand, uint32 N, VHeapInt* Result)
{
	check(Source.GetLength() >= N);
	check(Result->GetLength() >= N);

	Digit Carry = Summand;
	Digit High = 0;
	for (uint32 I = 0; I < N; I++)
	{
		Digit Current = Source.GetDigit(I);
		Digit NewCarry = 0;

		// Compute this round'S multiplication.
		Digit NewHigh = 0;
		Current = DigitMul(Current, Factor, NewHigh);

		// Add last round'S carryovers.
		Current = DigitAdd(Current, High, NewCarry);
		Current = DigitAdd(Current, Carry, NewCarry);

		// Store result and prepare for next round.
		Result->SetDigit(I, Current);
		Carry = NewCarry;
		High = NewHigh;
	}

	if (Result->GetLength() > N)
	{
		Result->SetDigit(N++, Carry + High);

		// Current callers don't pass in such large results, but let'S be robust.
		while (N < Result->GetLength())
			Result->SetDigit(N++, 0);
	}
	else
	{
		check(!(Carry + High));
	}
}

// Always copies the input, even when {shift} == 0.
VHeapInt* VHeapInt::AbsoluteLeftShiftAlwaysCopy(FRunningContext Context, const VHeapInt& X, uint32 Shift, LeftShiftMode Mode)
{
	check(Shift < DigitBits);
	check(!X.IsZero());

	uint32 N = X.GetLength();
	uint32 ResultLength = Mode == LeftShiftMode::AlwaysAddOneDigit ? N + 1 : N;
	VHeapInt* Result = CreateWithLength(Context, ResultLength);

	if (!Shift)
	{
		for (uint32 I = 0; I < N; I++)
			Result->SetDigit(I, X.GetDigit(I));
		if (Mode == LeftShiftMode::AlwaysAddOneDigit)
			Result->SetDigit(N, 0);

		return Result;
	}

	Digit Carry = 0;
	for (uint32 I = 0; I < N; I++)
	{
		Digit D = X.GetDigit(I);
		Result->SetDigit(I, (D << Shift) | Carry);
		Carry = D >> (DigitBits - Shift);
	}

	if (Mode == LeftShiftMode::AlwaysAddOneDigit)
	{
		Result->SetDigit(N, Carry);
	}
	else
	{
		check(Mode == LeftShiftMode::SameSizeResult);
		check(!Carry);
	}

	return Result;
}

// Returns the Quotient.
// Quotient = (high << DigitBits + Low - Remainder) / Divisor
VHeapInt::Digit VHeapInt::DigitDiv(Digit High, Digit Low, Digit Divisor, Digit& Remainder)
{
	static constexpr Digit HalfDigitBase = 1ull << HalfDigitBits;
	// Adapted from Warren, Hacker'S Delight, p. 152.
	uint32 S = FMath::CountLeadingZeros(Divisor);
	// If {S} is digitBits here, it causes an undefined behavior.
	// But {S} is never digitBits since {divisor} is never zero here.
	check(S != DigitBits);
	Divisor <<= S;

	Digit VN1 = Divisor >> HalfDigitBits;
	Digit VN0 = Divisor & HalfDigitMask;

	// {sZeroMask} which is 0 if S == 0 and all 1-bits otherwise.
	// {S} can be 0. If {S} is 0, performing "Low >> (DigitBits - S)" must not be done since it causes an undefined behavior
	// since `>> DigitBits` is undefied in C++. Quoted from C++ spec, "The type of the result is that of the promoted left operand.
	// The behavior is undefined if the right operand is negative, or greater than or equal to the length in bits of the promoted
	// left operand". We mask the right operand of the shift by {ShiftMask} (`DigitBits - 1`), which makes `DigitBits - 0` zero.
	// This shifting produces a value which covers 0 < {S} <= (DigitBits - 1) cases. {S} == DigitBits never happen as we asserted.
	// Since {sZeroMask} clears the value in the case of {S} == 0, {S} == 0 case is also covered.
	static_assert(sizeof(SignedDigit) == sizeof(Digit));
	Digit sZeroMask = static_cast<Digit>((-static_cast<SignedDigit>(S)) >> (DigitBits - 1));
	static constexpr uint32 ShiftMask = DigitBits - 1;
	Digit UN32 = (High << S) | ((Low >> ((DigitBits - S) & ShiftMask)) & sZeroMask);

	Digit UN10 = Low << S;
	Digit UN1 = UN10 >> HalfDigitBits;
	Digit UN0 = UN10 & HalfDigitMask;
	Digit Q1 = UN32 / VN1;
	Digit Rhat = UN32 - Q1 * VN1;

	while (Q1 >= HalfDigitBase || Q1 * VN0 > Rhat * HalfDigitBase + UN1)
	{
		Q1--;
		Rhat += VN1;
		if (Rhat >= HalfDigitBase)
			break;
	}

	Digit Un21 = UN32 * HalfDigitBase + UN1 - Q1 * Divisor;
	Digit Q0 = Un21 / VN1;
	Rhat = Un21 - Q0 * VN1;

	while (Q0 >= HalfDigitBase || Q0 * VN0 > Rhat * HalfDigitBase + UN0)
	{
		Q0--;
		Rhat += VN1;
		if (Rhat >= HalfDigitBase)
			break;
	}

	Remainder = (Un21 * HalfDigitBase + UN0 - Q0 * Divisor) >> S;
	return Q1 * HalfDigitBase + Q0;
}

VHeapInt* VHeapInt::Copy(FRunningContext Context, const VHeapInt& X)
{
	check(!X.IsZero());

	VHeapInt* Result = CreateWithLength(Context, X.GetLength());

	for (uint32 I = 0; I < Result->GetLength(); ++I)
	{
		Result->SetDigit(I, X.GetDigit(I));
	}
	Result->SetSign(X.GetSign());
	return Result;
}

VHeapInt* VHeapInt::UnaryMinus(FRunningContext Context, VHeapInt& X)
{
	if (X.IsZero())
	{
		return Zero(Context);
	}

	VHeapInt* Result = Copy(Context, X);
	Result->SetSign(!X.GetSign());
	return Result;
}

VHeapInt* VHeapInt::Add(FRunningContext Context, VHeapInt& X, VHeapInt& Y)
{
	bool XSign = X.GetSign();

	// X + Y == X + Y
	// -X + -Y == -(X + Y)
	if (XSign == Y.GetSign())
	{
		return AbsoluteAdd(Context, X, Y, XSign);
	}

	// X + -Y == X - Y == -(Y - X)
	// -X + Y == Y - X == -(X - Y)
	ComparisonResult ComparisonResult = AbsoluteCompare(X, Y);
	if (ComparisonResult == ComparisonResult::GreaterThan || ComparisonResult == ComparisonResult::Equal)
	{
		return AbsoluteSub(Context, X, Y, XSign);
	}

	return AbsoluteSub(Context, Y, X, !XSign);
}

VHeapInt* VHeapInt::Sub(FRunningContext Context, VHeapInt& X, VHeapInt& Y)
{
	bool XSign = X.GetSign();
	if (XSign != Y.GetSign())
	{
		// X - (-Y) == X + Y
		// (-X) - Y == -(X + Y)
		return AbsoluteAdd(Context, X, Y, XSign);
	}
	// X - Y == -(Y - X)
	// (-X) - (-Y) == Y - X == -(X - Y)
	ComparisonResult ComparisonResult = AbsoluteCompare(X, Y);
	if (ComparisonResult == ComparisonResult::GreaterThan || ComparisonResult == ComparisonResult::Equal)
	{
		return AbsoluteSub(Context, X, Y, XSign);
	}

	return AbsoluteSub(Context, Y, X, !XSign);
}

static_assert(sizeof(VHeapInt::Digit) == 4);
using TwoDigit = uint64_t;

// {Carry} must point to an initialized Digit and will either be incremented
// by one or left alone.
VHeapInt::Digit VHeapInt::DigitAdd(Digit A, Digit B, Digit& Carry)
{
	Digit Result = A + B;
	Carry += static_cast<bool>(Result < A);
	return Result;
}

// {Borrow} must point to an initialized Digit and will either be incremented
// by one or left alone.
VHeapInt::Digit VHeapInt::DigitSub(Digit A, Digit B, Digit& Borrow)
{
	Digit Result = A - B;
	Borrow += static_cast<bool>(Result > A);
	return Result;
}

// Returns the Low half of the Result. High half is in {High}.
VHeapInt::Digit VHeapInt::DigitMul(Digit A, Digit B, Digit& High)
{
	TwoDigit Result = static_cast<TwoDigit>(A) * static_cast<TwoDigit>(B);
	High = static_cast<Digit>(Result >> DigitBits);
	return static_cast<Digit>(Result);
}

void VHeapInt::MultiplyAccumulate(const VHeapInt& Multiplicand, Digit Multiplier, VHeapInt* Accumulator, uint32 AccumulatorIndex)
{
	check(Accumulator->GetLength() > Multiplicand.GetLength() + AccumulatorIndex);
	if (!Multiplier)
	{
		return;
	}

	Digit Carry = 0;
	Digit High = 0;
	for (uint32 I = 0; I < Multiplicand.GetLength(); I++, AccumulatorIndex++)
	{
		Digit Acc = Accumulator->GetDigit(AccumulatorIndex);
		Digit NewCarry = 0;

		// Add last round'S carryovers.
		Acc = DigitAdd(Acc, High, NewCarry);
		Acc = DigitAdd(Acc, Carry, NewCarry);

		// Compute this round'S multiplication.
		Digit MultiplicandDigit = Multiplicand.GetDigit(I);
		Digit Low = DigitMul(Multiplier, MultiplicandDigit, High);
		Acc = DigitAdd(Acc, Low, NewCarry);

		// Store Result and prepare for next round.
		Accumulator->SetDigit(AccumulatorIndex, Acc);
		Carry = NewCarry;
	}

	while (Carry || High)
	{
		check(AccumulatorIndex < Accumulator->GetLength());
		Digit Acc = Accumulator->GetDigit(AccumulatorIndex);
		Digit NewCarry = 0;
		Acc = DigitAdd(Acc, High, NewCarry);
		High = 0;
		Acc = DigitAdd(Acc, Carry, NewCarry);
		Accumulator->SetDigit(AccumulatorIndex, Acc);
		Carry = NewCarry;
		AccumulatorIndex++;
	}
}

bool VHeapInt::Equals(VHeapInt& X, VHeapInt& Y)
{
	if (X.GetSign() != Y.GetSign())
	{
		return false;
	}

	if (X.GetLength() != Y.GetLength())
	{
		return false;
	}

	for (uint32 I = 0; I < X.GetLength(); I++)
	{
		if (X.GetDigit(I) != Y.GetDigit(I))
		{
			return false;
		}
	}

	return true;
}

VHeapInt::ComparisonResult VHeapInt::AbsoluteCompare(const VHeapInt& X, const VHeapInt& Y)
{
	check(!X.GetLength() || X.GetDigit(X.GetLength() - 1));
	check(!Y.GetLength() || Y.GetDigit(Y.GetLength() - 1));

	int Diff = X.GetLength() - Y.GetLength();
	if (Diff)
	{
		return Diff < 0 ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
	}

	int I = X.GetLength() - 1;
	while (I >= 0 && X.GetDigit(I) == Y.GetDigit(I))
	{
		I--;
	}

	if (I < 0)
	{
		return ComparisonResult::Equal;
	}

	return X.GetDigit(I) > Y.GetDigit(I) ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
}

VHeapInt::ComparisonResult VHeapInt::Compare(VHeapInt& X, VHeapInt& Y)
{
	bool XSign = X.GetSign();

	if (XSign != Y.GetSign())
	{
		return XSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
	}

	ComparisonResult Result = AbsoluteCompare(X, Y);
	if (Result == ComparisonResult::GreaterThan)
	{
		return XSign ? ComparisonResult::LessThan : ComparisonResult::GreaterThan;
	}
	if (Result == ComparisonResult::LessThan)
	{
		return XSign ? ComparisonResult::GreaterThan : ComparisonResult::LessThan;
	}

	return ComparisonResult::Equal;
}

VHeapInt* VHeapInt::AbsoluteAdd(FRunningContext Context,
	VHeapInt& X,
	VHeapInt& Y,
	bool ResultSign)
{
	if (X.GetLength() < Y.GetLength())
	{
		return AbsoluteAdd(Context, Y, X, ResultSign);
	}

	if (X.IsZero())
	{
		check(Y.IsZero());
		return &X;
	}

	if (Y.IsZero())
	{
		if (ResultSign == X.GetSign())
		{
			return &X;
		}
		return UnaryMinus(Context, X);
	}

	VHeapInt* Result = CreateWithLength(Context, X.GetLength() + 1);

	Digit Carry = 0;
	uint32 I = 0;
	for (; I < Y.GetLength(); I++)
	{
		Digit NewCarry = 0;
		Digit Sum = DigitAdd(X.GetDigit(I), Y.GetDigit(I), NewCarry);
		Sum = DigitAdd(Sum, Carry, NewCarry);
		Result->SetDigit(I, Sum);
		Carry = NewCarry;
	}

	for (; I < X.GetLength(); I++)
	{
		Digit NewCarry = 0;
		Digit Sum = DigitAdd(X.GetDigit(I), Carry, NewCarry);
		Result->SetDigit(I, Sum);
		Carry = NewCarry;
	}

	Result->SetDigit(I, Carry);
	Result->SetSign(ResultSign);

	return Result->RightTrim(Context);
}

VHeapInt* VHeapInt::AbsoluteSub(FRunningContext Context,
	VHeapInt& X, VHeapInt& Y, bool ResultSign)
{
	ComparisonResult ComparisonResult = AbsoluteCompare(X, Y);
	check(X.GetLength() >= Y.GetLength());
	check(ComparisonResult == ComparisonResult::GreaterThan || ComparisonResult == ComparisonResult::Equal);

	if (X.IsZero())
	{
		check(Y.IsZero());
		return &X;
	}

	if (Y.IsZero())
	{
		if (ResultSign == X.GetSign())
		{
			return &X;
		}
		return VHeapInt::UnaryMinus(Context, X);
	}

	if (ComparisonResult == ComparisonResult::Equal)
	{
		return Zero(Context);
	}

	VHeapInt* Result = CreateWithLength(Context, X.GetLength());

	Digit Borrow = 0;
	uint32 I = 0;
	for (; I < Y.GetLength(); I++)
	{
		Digit NewBorrow = 0;
		Digit Difference = DigitSub(X.GetDigit(I), Y.GetDigit(I), NewBorrow);
		Difference = DigitSub(Difference, Borrow, NewBorrow);
		Result->SetDigit(I, Difference);
		Borrow = NewBorrow;
	}

	for (; I < X.GetLength(); I++)
	{
		Digit NewBorrow = 0;
		Digit Difference = DigitSub(X.GetDigit(I), Borrow, NewBorrow);
		Result->SetDigit(I, Difference);
		Borrow = NewBorrow;
	}

	check(!Borrow);
	Result->SetSign(ResultSign);
	return Result->RightTrim(Context);
}

VHeapInt* VHeapInt::RightTrim(FRunningContext Context)
{
	if (IsZero())
	{
		check(!GetSign());
		return this;
	}

	int NonZeroIndex = Length - 1;
	while (NonZeroIndex >= 0 && !GetDigit(NonZeroIndex))
	{
		NonZeroIndex--;
	}

	if (NonZeroIndex < 0)
	{
		return CreateZero(Context);
	}

	if (NonZeroIndex == static_cast<int>(Length - 1))
	{
		return this;
	}

	uint32 NewLength = NonZeroIndex + 1;
	VHeapInt* TrimmedBigInt = CreateWithLength(Context, NewLength);

	memcpy(TrimmedBigInt->DataStorage(), DataStorage(), NewLength * sizeof(DataStorage()[0]));

	TrimmedBigInt->SetSign(this->GetSign());

	return TrimmedBigInt;
}
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)