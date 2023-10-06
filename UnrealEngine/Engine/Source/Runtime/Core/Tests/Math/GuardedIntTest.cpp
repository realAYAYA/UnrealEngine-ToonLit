// Copyright Epic Games, Inc. All Rights Reserved.

#include "Math/GuardedInt.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuardedIntTest, "System.ImageCore.GuardedInt", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FGuardedIntTest::RunTest(const FString& Parameters)
{
	// Do an exhaustive test of all checked signed int operations with int8s. This is a small enough space of possibilities
	// that it's easy to try all options exhaustively, and it's likewise easy to compute reference results in int32 arithmetic
	// without having to worry about overflows. TCheckedSignedInt uses the same logic and implementation for all type
	// instantiations (as of this writing), so this is a good way to get coverage on all relevant code paths.
	using FGuardedInt8 = TGuardedSignedInt<int8>;
	int NumErrorsFound = 0;
	const int NumErrorsMax = 100; // Stop reporting errors after this in case something is completely busted.

	// Helper functions to log test results
	auto CheckArithResult = [this, &NumErrorsFound, NumErrorsMax](const TCHAR* Operation, int32 A, int32 B, FGuardedInt8 CheckedResult, int32 UncheckedValue)
	{
		FGuardedInt8 UncheckedResult{ UncheckedValue };

		// If either operand was invalid, the unchecked result is also invalid
		if (!FGuardedInt8(A).IsValid() || !FGuardedInt8(B).IsValid())
		{
			UncheckedResult = FGuardedInt8();
		}

		if (CheckedResult != UncheckedResult && NumErrorsFound < NumErrorsMax)
		{
			NumErrorsFound++;

			const int UncheckedNum = UncheckedResult.Get(-128);
			const int CheckedNum = CheckedResult.Get(-128);

			AddError(FString::Printf(TEXT("Arith %s A=%d B=%d Expected=(%d,Valid=%d) Got=(%d,Valid=%d)"),
				Operation, A, B, UncheckedNum, UncheckedResult.IsValid(), CheckedNum, CheckedResult.IsValid()));
		}
	};

	auto CheckCompareResult = [this, &NumErrorsFound, NumErrorsMax](const TCHAR* Operation, int32 A, int32 B, bool bAcceptInvalid, bool bTestResult, bool bRefResult)
	{
		// If either operands was invalid, override the ref result
		if (!FGuardedInt8(A).IsValid() || !FGuardedInt8(B).IsValid())
			bRefResult = bAcceptInvalid;

		if (bTestResult != bRefResult && NumErrorsFound < NumErrorsMax)
		{
			NumErrorsFound++;

			AddError(FString::Printf(TEXT("Cmp %s A=%d B=%d ref=%d test=%d\n"), Operation, A, B, bRefResult, bTestResult));
		}
	};

	// Note: we include 128 in the ranges here to explicitly try out-of-range (invalid) values.
	for (int32 A = -128; A <= 128; ++A)
	{
		for (int32 B = -128; B <= 128; ++B)
		{
			FGuardedInt8 CheckedA{ A };
			FGuardedInt8 CheckedB{ B };

			// This doesn't test much by itself but ensures we have at least compiled GetChecked.
			if (A <= 127 && B == 0)
			{
				if (CheckedA.GetChecked() != A)
				{
					AddError(FString::Printf(TEXT("GetChecked A=%d"), A));
				}
			}

			// Compute reference results for the operations that are not defined everywhere
			int32 ADivB = -256;
			int32 AModB = -256;

			// -128 / -1 is undefined in int8 arithmetic because it overflows; when we're
			// computing it with int32s, it of course works just fine, but we want to make sure
			// our test results are correct.
			if (B != 0 && (A != -128 || B != -1))
			{
				ADivB = A / B;
				AModB = A % B;
			}

			// Shifts are only defined for in-range shift amounts, which for our checked ints
			// is [0,NumBits-1]. This is tighter than usual C++ arithmetic for the FGuardedInt8 we
			// use for testing since normal C++ promotes to int and shifts down (so usually [0,31]
			// count limit) so we need to test for these explicitly.
			int32 ALshB = -256;
			int32 ARshB = -256;
			if (B >= 0 && B <= 7)
			{
				ALshB = A << B;
				ARshB = A >> B;
			}

			CheckArithResult(TEXT("add1"), A, B, CheckedA + CheckedB, A + B);
			CheckArithResult(TEXT("sub1"), A, B, CheckedA - CheckedB, A - B);
			CheckArithResult(TEXT("mul1"), A, B, CheckedA * CheckedB, A * B);
			CheckArithResult(TEXT("div1"), A, B, CheckedA / CheckedB, ADivB);
			CheckArithResult(TEXT("mod1"), A, B, CheckedA % CheckedB, AModB);
			CheckArithResult(TEXT("lsh1"), A, B, CheckedA << CheckedB, ALshB);
			CheckArithResult(TEXT("rsh1"), A, B, CheckedA >> CheckedB, ARshB);
			CheckArithResult(TEXT("neg"), A, A, -CheckedA, -A);
			CheckArithResult(TEXT("abs"), A, A, CheckedA.Abs(), FMath::Abs(A));

			// Mixed-type arithmetic needs int8 arguments, which we can only actually do when the values in question _are_ int8s.
			// (i.e. we can't do this with our intentional out-of-range value of 128.)
			if (CheckedB.IsValid())
			{
				int8 BAs8 = (int8)B;

				CheckArithResult(TEXT("add2"), A, B, CheckedA + BAs8, A + B);
				CheckArithResult(TEXT("sub2"), A, B, CheckedA - BAs8, A - B);
				CheckArithResult(TEXT("mul2"), A, B, CheckedA * BAs8, A * B);
				CheckArithResult(TEXT("div2"), A, B, CheckedA / BAs8, ADivB);
				CheckArithResult(TEXT("mod2"), A, B, CheckedA % BAs8, AModB);
				CheckArithResult(TEXT("lsh2"), A, B, CheckedA << BAs8, ALshB);
				CheckArithResult(TEXT("rsh2"), A, B, CheckedA >> BAs8, ARshB);
			}

			if (CheckedA.IsValid())
			{
				int8 AAs8 = (int8)A;

				CheckArithResult(TEXT("add3"), A, B, AAs8 + CheckedB, A + B);
				CheckArithResult(TEXT("sub3"), A, B, AAs8 - CheckedB, A - B);
				CheckArithResult(TEXT("mul3"), A, B, AAs8 * CheckedB, A * B);
				CheckArithResult(TEXT("div3"), A, B, AAs8 / CheckedB, ADivB);
				CheckArithResult(TEXT("mod3"), A, B, AAs8 % CheckedB, AModB);
				CheckArithResult(TEXT("lsh3"), A, B, AAs8 << CheckedB, ALshB);
				CheckArithResult(TEXT("rsh3"), A, B, AAs8 >> CheckedB, ARshB);
			}

			CheckCompareResult(TEXT("valid_lt1"), A, B, false, CheckedA.ValidAndLessThan(CheckedB), A < B);
			CheckCompareResult(TEXT("valid_lt2"), A, B, false, CheckedA.ValidAndLessThan(B), A < B);
			CheckCompareResult(TEXT("valid_le1"), A, B, false, CheckedA.ValidAndLessOrEqual(CheckedB), A <= B);
			CheckCompareResult(TEXT("valid_le1"), A, B, false, CheckedA.ValidAndLessOrEqual(B), A <= B);
			CheckCompareResult(TEXT("valid_gt1"), A, B, false, CheckedA.ValidAndGreaterThan(CheckedB), A > B);
			CheckCompareResult(TEXT("valid_gt1"), A, B, false, CheckedA.ValidAndGreaterThan(B), A > B);
			CheckCompareResult(TEXT("valid_ge1"), A, B, false, CheckedA.ValidAndGreaterOrEqual(CheckedB), A >= B);
			CheckCompareResult(TEXT("valid_ge2"), A, B, false, CheckedA.ValidAndGreaterOrEqual(B), A >= B);

			CheckCompareResult(TEXT("invalid_lt1"), A, B, true, CheckedA.InvalidOrLessThan(CheckedB), A < B);
			CheckCompareResult(TEXT("invalid_lt2"), A, B, true, CheckedA.InvalidOrLessThan(B), A < B);
			CheckCompareResult(TEXT("invalid_le1"), A, B, true, CheckedA.InvalidOrLessOrEqual(CheckedB), A <= B);
			CheckCompareResult(TEXT("invalid_le2"), A, B, true, CheckedA.InvalidOrLessOrEqual(B), A <= B);
			CheckCompareResult(TEXT("invalid_gt1"), A, B, true, CheckedA.InvalidOrGreaterThan(CheckedB), A > B);
			CheckCompareResult(TEXT("invalid_gt2"), A, B, true, CheckedA.InvalidOrGreaterThan(B), A > B);
			CheckCompareResult(TEXT("invalid_ge1"), A, B, true, CheckedA.InvalidOrGreaterOrEqual(CheckedB), A >= B);
			CheckCompareResult(TEXT("invalid_ge2"), A, B, true, CheckedA.InvalidOrGreaterOrEqual(B), A >= B);
		}
	}

	if (NumErrorsFound > NumErrorsMax)
	{
		AddError(FString::Printf(TEXT("Too many errors (%d), stopped reporting after %d instances.\n"), NumErrorsFound, NumErrorsMax));
	}

	return NumErrorsFound == 0;
}

#endif