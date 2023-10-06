// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/CString.h"
#include "Math/Color.h"
#include "Math/MathFwd.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Containers/UnrealString.h"
#include "Containers/StringFwd.h"

#include "../TestHarness.h"

namespace {

	bool TestTrue(const TCHAR* What, bool Value)
	{
		if (!Value)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be true."), What));
			return false;
		}
		return true;
	}

	bool TestTrue(const FString& What, bool Value)
	{
		return TestTrue(*What, Value);
	}

	bool TestEqual(const TCHAR* What, const int32 Actual, const int32 Expected)
	{
		if (Actual != Expected)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %d, but it was %d."), What, Expected, Actual));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const int64 Actual, const int64 Expected)
	{
		if (Actual != Expected)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %" "lld" ", but it was %" "lld" "."), What, Expected, Actual));
			return false;
		}
		return true;
	}

#if PLATFORM_64BITS
	bool TestEqual(const TCHAR* What, const SIZE_T Actual, const SIZE_T Expected)
	{
		if (Actual != Expected)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %" "Iu" ", but it was %" "Iu" "."), What, Expected, Actual));
			return false;
		}
		return true;
	}
#endif

	bool TestEqual(const TCHAR* What, const float Actual, const float Expected, float Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		if (!FMath::IsNearlyEqual(Actual, Expected, Tolerance))
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %f, but it was %f within tolerance %f."), What, Expected, Actual, Tolerance));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const double Actual, const double Expected, double Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		if (!FMath::IsNearlyEqual(Actual, Expected, Tolerance))
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %f, but it was %f within tolerance %f."), What, Expected, Actual, Tolerance));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const FVector Actual, const FVector Expected, float Tolerance)
	{
		if (!Expected.Equals(Actual, Tolerance))
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), What, *Expected.ToString(), *Actual.ToString(), Tolerance));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const FTransform Actual, const FTransform Expected, float Tolerance)
	{
		if (!Expected.Equals(Actual, Tolerance))
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), What, *Expected.ToString(), *Actual.ToString(), Tolerance));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const FRotator Actual, const FRotator Expected, float Tolerance)
	{
		if (!Expected.Equals(Actual, Tolerance))
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), What, *Expected.ToString(), *Actual.ToString(), Tolerance));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const FColor Actual, const FColor Expected)
	{
		if (Expected != Actual)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s."), What, *Expected.ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const FLinearColor Actual, const FLinearColor Expected)
	{
		if (Expected != Actual)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s."), What, *Expected.ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}

	bool TestEqual(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
	{
		if (FCString::Strcmp(Actual, Expected) != 0)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be \"%s\", but it was \"%s\"."), What, Expected, Actual));
			return false;
		}
		return true;
	}

	bool TestEqualInsensitive(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
	{
		if (FCString::Stricmp(Actual, Expected) != 0)
		{
			FAIL_CHECK(FString::Printf(TEXT("Expected '%s' to be \"%s\", but it was \"%s\"."), What, Expected, Actual));
			return false;
		}
		return true;
	}

	bool TestEqual(const FString& What, const int32 Actual, const int32 Expected)
	{
		return TestEqual(*What, Actual, Expected);
	}

	bool TestEqual(const FString& What, const float Actual, const float Expected, float Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		return TestEqual(*What, Actual, Expected, Tolerance);
	}

	bool TestEqual(const FString& What, const double Actual, const double Expected, double Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		return TestEqual(*What, Actual, Expected, Tolerance);
	}

	bool TestEqual(const FString& What, const FVector Actual, const FVector Expected, float Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		return TestEqual(*What, Actual, Expected, Tolerance);
	}

	bool TestEqual(const FString& What, const FTransform Actual, const FTransform Expected, float Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		return TestEqual(*What, Actual, Expected, Tolerance);
	}

	bool TestEqual(const FString& What, const FRotator Actual, const FRotator Expected, float Tolerance = UE_KINDA_SMALL_NUMBER)
	{
		return TestEqual(*What, Actual, Expected, Tolerance);
	}

	bool TestEqual(const FString& What, const FColor Actual, const FColor Expected)
	{
		return TestEqual(*What, Actual, Expected);
	}

	bool TestEqual(const FString& What, const TCHAR* Actual, const TCHAR* Expected)
	{
		return TestEqual(*What, Actual, Expected);
	}

	bool TestEqual(const TCHAR* What, const FString& Actual, const TCHAR* Expected)
	{
		return TestEqualInsensitive(What, *Actual, Expected);
	}

	bool TestEqual(const FString& What, const FString& Actual, const TCHAR* Expected)
	{
		return TestEqualInsensitive(*What, *Actual, Expected);
	}

	bool TestEqual(const TCHAR* What, const TCHAR* Actual, const FString& Expected)
	{
		return TestEqualInsensitive(What, Actual, *Expected);
	}

	bool TestEqual(const FString& What, const TCHAR* Actual, const FString& Expected)
	{
		return TestEqualInsensitive(*What, Actual, *Expected);
	}

	bool TestEqual(const TCHAR* What, const FString& Actual, const FString& Expected)
	{
		return TestEqualInsensitive(What, *Actual, *Expected);
	}

	bool TestEqual(const FString& What, const FString& Actual, const FString& Expected)
	{
		return TestEqualInsensitive(*What, *Actual, *Expected);
	}

	template<typename ValueType>
	bool TestEqual(const TCHAR* What, const ValueType& Actual, const ValueType& Expected)
	{
		if (Actual != Expected)
		{
			FAIL_CHECK(FString::Printf(TEXT("%s: The two values are not equal."), What));
			return false;
		}
		return true;
	}

	template<typename ValueType>
	bool TestEqual(const FString& What, const ValueType& Actual, const ValueType& Expected)
	{
		return TestEqual(*What, Actual, Expected);
	}

	template<typename ValueType> bool TestNotEqual(const TCHAR* Description, const ValueType& Actual, const ValueType& Expected)
	{
		if (Actual == Expected)
		{
			FAIL_CHECK(FString::Printf(TEXT("%s: The two values are equal."), Description));
			return false;
		}
		return true;
	}

	template<typename ValueType> bool TestNotEqual(const FString& Description, const ValueType& Actual, const ValueType& Expected)
	{
		return TestNotEqual(*Description, Actual, Expected);
	}

	#define CHECK_EQUALS(What, X, Y) TestEqual(What, X, Y);
	#define CHECK_NOT_EQUALS(What, X, Y) TestNotEqual(What, X, Y);

}