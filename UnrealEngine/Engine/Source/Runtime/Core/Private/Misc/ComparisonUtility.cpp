// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/ComparisonUtility.h"

#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "Misc/CString.h"

namespace UE { namespace ComparisonUtility {

int32 CompareWithNumericSuffix(FName A, FName B)
{
	return CompareWithNumericSuffix(FNameBuilder(A).ToView(), FNameBuilder(B).ToView());
}

int32 CompareWithNumericSuffix(FStringView A, FStringView B)
{
	auto SplitNumericSuffix = [](FStringView Full, FStringView& OutPrefix, int64& OutSuffix)
	{
		auto IsDigit = [](const TCHAR C)
		{
			return C >= TEXT('0') && C <= TEXT('9');
		};

		auto CharToDigit = [](const TCHAR C) -> int32
		{
			return C - '0';
		};

		int32 NumDigits = 0;
		OutSuffix = 0;
		{
			int32 Magnitude = 1;
			const TCHAR* FullStart = Full.GetData();
			for (const TCHAR* C = FullStart + Full.Len() - 1; C >= FullStart && IsDigit(*C); --C)
			{
				OutSuffix += CharToDigit(*C) * Magnitude;
				Magnitude *= 10;
				++NumDigits;
			}
		}

		static constexpr int32 MaxDigitsInt64 = 19;
		if (NumDigits > MaxDigitsInt64)
		{
			// Too large to process as a number
			NumDigits = 0;
		}

		if (NumDigits > 0)
		{
			OutPrefix = Full.LeftChop(NumDigits);
			// OutSuffix has already been set by the parsing loop above
		}
		else
		{
			OutPrefix = Full;
			OutSuffix = INDEX_NONE;
		}
	};

	FStringView APrefix;
	int64 ASuffix = 0;
	SplitNumericSuffix(A, APrefix, ASuffix);

	FStringView BPrefix;
	int64 BSuffix = 0;
	SplitNumericSuffix(B, BPrefix, BSuffix);

	// Are the prefixes identical?
	const int32 PrefixResult = APrefix.Compare(BPrefix, ESearchCase::IgnoreCase);
	if (PrefixResult != 0)
	{
		return PrefixResult;
	}

	// If so, compare the suffixes too
	if (ASuffix < BSuffix)
	{
		return -1;
	}
	if (ASuffix > BSuffix)
	{
		return 1;
	}
	return 0;
}

} } // namespace UE::ComparisonUtility
