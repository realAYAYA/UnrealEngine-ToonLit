// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/CString.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/StringBuilder.h"

// This class is a workaround for clang compilers causing error "'va_start' used in function with fixed args" when using it in a lambda in RunTest()
class FCStringGetVarArgsTestBase : public FAutomationTestBase
{
public:
	FCStringGetVarArgsTestBase(const FString& InName, const bool bInComplexTask)
	: FAutomationTestBase(InName, bInComplexTask)
	{
	}

protected:
	void DoTest(const TCHAR* ExpectedOutput, const TCHAR* Format, ...)
	{
		constexpr SIZE_T OutputBufferCharacterCount = 512;
		TCHAR OutputBuffer[OutputBufferCharacterCount];
		va_list ArgPtr;
		va_start(ArgPtr, Format);
		const int32 Result = FCString::GetVarArgs(OutputBuffer, OutputBufferCharacterCount, Format, ArgPtr);
		va_end(ArgPtr);

		if (Result < 0)
		{
			this->AddError(FString::Printf(TEXT("'%s' could not be parsed."), Format));
			return;
		}

		if (FCString::Strcmp(OutputBuffer, ExpectedOutput) != 0)
		{
			this->AddError(FString::Printf(TEXT("'%s' resulted in '%s', expected '%s'."), Format, OutputBuffer, ExpectedOutput));
			return;
		}
	}
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FCStringGetVarArgsTest, FCStringGetVarArgsTestBase, "System.Core.Misc.CString.GetVarArgs", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCStringGetVarArgsTest::RunTest(const FString& Parameters)
{
#if PLATFORM_64BITS
	DoTest(TEXT("SIZE_T_FMT |18446744073709551615|"), TEXT("SIZE_T_FMT |%" SIZE_T_FMT "|"), SIZE_T(MAX_uint64));
	DoTest(TEXT("SIZE_T_x_FMT |ffffffffffffffff|"), TEXT("SIZE_T_x_FMT |%" SIZE_T_x_FMT "|"), UPTRINT(MAX_uint64));
	DoTest(TEXT("SIZE_T_X_FMT |FFFFFFFFFFFFFFFF|"), TEXT("SIZE_T_X_FMT |%" SIZE_T_X_FMT "|"), UPTRINT(MAX_uint64));

	DoTest(TEXT("SSIZE_T_FMT |-9223372036854775808|"), TEXT("SSIZE_T_FMT |%" SSIZE_T_FMT "|"), SSIZE_T(MIN_int64));
	DoTest(TEXT("SSIZE_T_x_FMT |ffffffffffffffff|"), TEXT("SSIZE_T_x_FMT |%" SSIZE_T_x_FMT "|"), SSIZE_T(-1));
	DoTest(TEXT("SSIZE_T_X_FMT |FFFFFFFFFFFFFFFF|"), TEXT("SSIZE_T_X_FMT |%" SSIZE_T_X_FMT "|"), SSIZE_T(-1));

	DoTest(TEXT("PTRINT_FMT |-9223372036854775808|"), TEXT("PTRINT_FMT |%" PTRINT_FMT "|"), PTRINT(MIN_int64));
	DoTest(TEXT("PTRINT_x_FMT |ffffffffffffffff|"), TEXT("PTRINT_x_FMT |%" PTRINT_x_FMT "|"), PTRINT(-1));
	DoTest(TEXT("PTRINT_X_FMT |FFFFFFFFFFFFFFFF|"), TEXT("PTRINT_X_FMT |%" PTRINT_X_FMT "|"), PTRINT(-1));

	DoTest(TEXT("UPTRINT_FMT |18446744073709551615|"), TEXT("UPTRINT_FMT |%" UPTRINT_FMT "|"), UPTRINT(MAX_uint64));
	DoTest(TEXT("UPTRINT_x_FMT |ffffffffffffffff|"), TEXT("UPTRINT_x_FMT |%" UPTRINT_x_FMT "|"), UPTRINT(MAX_uint64));
	DoTest(TEXT("UPTRINT_X_FMT |FFFFFFFFFFFFFFFF|"), TEXT("UPTRINT_X_FMT |%" UPTRINT_X_FMT "|"), UPTRINT(MAX_uint64));
#else
	DoTest(TEXT("SIZE_T_FMT |4294967295|"), TEXT("SIZE_T_FMT |%" SIZE_T_FMT "|"), SIZE_T(MAX_uint32));
	DoTest(TEXT("SIZE_T_x_FMT |ffffffff|"), TEXT("SIZE_T_x_FMT |%" SIZE_T_x_FMT "|"), UPTRINT(MAX_uint32));
	DoTest(TEXT("SIZE_T_X_FMT |FFFFFFFF|"), TEXT("SIZE_T_X_FMT |%" SIZE_T_X_FMT "|"), UPTRINT(MAX_uint32));

	DoTest(TEXT("SSIZE_T_FMT |-2147483648|"), TEXT("SSIZE_T_FMT |%" SSIZE_T_FMT "|"), SSIZE_T(MIN_int32));
	DoTest(TEXT("SSIZE_T_x_FMT |ffffffff|"), TEXT("SSIZE_T_x_FMT |%" SSIZE_T_x_FMT "|"), SSIZE_T(-1));
	DoTest(TEXT("SSIZE_T_X_FMT |FFFFFFFF|"), TEXT("SSIZE_T_X_FMT |%" SSIZE_T_X_FMT "|"), SSIZE_T(-1));

	DoTest(TEXT("PTRINT_FMT |-2147483648|"), TEXT("PTRINT_FMT |%" PTRINT_FMT "|"), PTRINT(MIN_int32));
	DoTest(TEXT("PTRINT_x_FMT |ffffffff|"), TEXT("PTRINT_x_FMT |%" PTRINT_x_FMT "|"), PTRINT(-1));
	DoTest(TEXT("PTRINT_X_FMT |FFFFFFFF|"), TEXT("PTRINT_X_FMT |%" PTRINT_X_FMT "|"), PTRINT(-1));

	DoTest(TEXT("UPTRINT_FMT |4294967295|"), TEXT("UPTRINT_FMT |%" UPTRINT_FMT "|"), UPTRINT(MAX_uint32));
	DoTest(TEXT("UPTRINT_x_FMT |ffffffff|"), TEXT("UPTRINT_x_FMT |%" UPTRINT_x_FMT "|"), UPTRINT(MAX_uint32));
	DoTest(TEXT("UPTRINT_X_FMT |FFFFFFFF|"), TEXT("UPTRINT_X_FMT |%" UPTRINT_X_FMT "|"), UPTRINT(MAX_uint32));
#endif

	DoTest(TEXT("INT64_FMT |-9223372036854775808|"), TEXT("INT64_FMT |%" INT64_FMT "|"), MIN_int64);
	DoTest(TEXT("INT64_x_FMT |ffffffffffffffff|"), TEXT("INT64_x_FMT |%" INT64_x_FMT "|"), int64(-1));
	DoTest(TEXT("INT64_X_FMT |FFFFFFFFFFFFFFFF|"), TEXT("INT64_X_FMT |%" INT64_X_FMT "|"), int64(-1));

	DoTest(TEXT("UINT64_FMT |18446744073709551615|"), TEXT("UINT64_FMT |%" UINT64_FMT "|"), MAX_uint64);
	DoTest(TEXT("UINT64_x_FMT |ffffffffffffffff|"), TEXT("UINT64_x_FMT |%" UINT64_x_FMT "|"), MAX_uint64);
	DoTest(TEXT("UINT64_X_FMT |FFFFFFFFFFFFFFFF|"), TEXT("UINT64_X_FMT |%" UINT64_X_FMT "|"), MAX_uint64);

	DoTest(TEXT("|LEFT                |               RIGHT|     33.33|66.67     |"), TEXT("|%-20s|%20s|%10.2f|%-10.2f|"), TEXT("LEFT"), TEXT("RIGHT"), 33.333333, 66.666666);

	DoTest(TEXT("Percents|%%%3|"), TEXT("Percents|%%%%%%%d|"), 3);

	DoTest(TEXT("Integer arguments|12345|54321|123ABC|f|99|"), TEXT("Integer arguments|%d|%i|%X|%x|%u|"), 12345, 54321, 0x123AbC, 15, 99);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCStringStrstrTest, "System.Core.Misc.CString.Strstr", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCStringStrstrTest::RunTest(const FString& Parameters)
{
	auto RunTest = [this](const TCHAR* Search, const TCHAR* Find, int32 ExpectedSensitiveIndex, int32 ExpectedInsensitiveIndex)
	{
		const TCHAR* ExpectedSensitive = ExpectedSensitiveIndex == INDEX_NONE ? nullptr :
			(Search + ExpectedSensitiveIndex);
		const TCHAR* ExpectedInsensitive = ExpectedInsensitiveIndex == INDEX_NONE ? nullptr :
			(Search + ExpectedInsensitiveIndex);
		if (FCString::Strstr(Search, Find) != ExpectedSensitive)
		{
			AddError(FString::Printf(TEXT("Strstr(\"%s\", \"%s\") did not equal index \"%d\"."),
				Search, Find, ExpectedSensitiveIndex));
		}
		if (FCString::Stristr(Search, Find) != ExpectedInsensitive)
		{
			AddError(FString::Printf(TEXT("Stristr(\"%s\", \"%s\") did not equal index \"%d\"."),
				Search, Find, ExpectedInsensitiveIndex));
		}
	};
	const TCHAR* ABACADAB = TEXT("ABACADAB");

	RunTest(ABACADAB, TEXT("A"), 0, 0);
	RunTest(ABACADAB, TEXT("a"), INDEX_NONE, 0);
	RunTest(ABACADAB, TEXT("BAC"), 1, 1);
	RunTest(ABACADAB, TEXT("BaC"), INDEX_NONE, 1);
	RunTest(ABACADAB, TEXT("BAC"), 1, 1);
	RunTest(ABACADAB, TEXT("BaC"), INDEX_NONE, 1);
	RunTest(ABACADAB, TEXT("DAB"), 5, 5);
	RunTest(ABACADAB, TEXT("dab"), INDEX_NONE, 5);
	RunTest(ABACADAB, ABACADAB, 0, 0);
	RunTest(ABACADAB, TEXT("abacadab"), INDEX_NONE, 0);
	RunTest(ABACADAB, TEXT("F"), INDEX_NONE, INDEX_NONE);
	RunTest(ABACADAB, TEXT("DABZ"), INDEX_NONE, INDEX_NONE);
	RunTest(ABACADAB, TEXT("ABACADABA"), INDEX_NONE, INDEX_NONE);
	RunTest(ABACADAB, TEXT("NoMatchLongerString"), INDEX_NONE, INDEX_NONE);
	RunTest(TEXT(""), TEXT("FindText"), INDEX_NONE, INDEX_NONE);
	RunTest(TEXT(""), TEXT(""), 0, 0);
	RunTest(ABACADAB, TEXT(""), 0, 0);

	// Passing in nullpt r is not allowed by StrStr so we do not test it

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCStringStrnstrTest, "System.Core.Misc.CString.Strnstr", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCStringStrnstrTest::RunTest(const FString& Parameters)
{
	auto RunTest = [this](FStringView Search, FStringView Find, int32 ExpectedSensitiveIndex, int32 ExpectedInsensitiveIndex)
	{
		TStringBuilder<128> SearchWithoutNull;
		TStringBuilder<128> FindWithoutNull;
		// TODO: If we could add a way to assert the trailing characters are not read, that would be a better test
		SearchWithoutNull << Search << TEXT("SearchTrailing");
		FindWithoutNull << Find << TEXT("FindTrailing");
		const TCHAR* ExpectedSensitive = ExpectedSensitiveIndex == INDEX_NONE ? nullptr :
			(Search.GetData() + ExpectedSensitiveIndex);
		const TCHAR* ExpectedInsensitive = ExpectedInsensitiveIndex == INDEX_NONE ? nullptr :
			(Search.GetData() + ExpectedInsensitiveIndex);
		const TCHAR* ExpectedSensitiveWithoutNull = ExpectedSensitiveIndex == INDEX_NONE ? nullptr :
			(SearchWithoutNull.GetData() + ExpectedSensitiveIndex);
		const TCHAR* ExpectedInsensitiveWithoutNull = ExpectedInsensitiveIndex == INDEX_NONE ? nullptr :
			(SearchWithoutNull.GetData() + ExpectedInsensitiveIndex);
		if (FCString::Strnstr(Search.GetData(), Search.Len(), Find.GetData(), Find.Len()) != ExpectedSensitive)
		{
			AddError(FString::Printf(TEXT("Strnstr(\"%.*s\", %d, \"%.*s\", %d)\" did not equal index \"%d\"."),
				Search.Len(), Search.GetData(), Search.Len(), Find.Len(), Find.GetData(), Find.Len(), ExpectedSensitiveIndex));
		}
		if (FCString::Strnstr(SearchWithoutNull.GetData(), Search.Len(), FindWithoutNull.GetData(), Find.Len()) != ExpectedSensitiveWithoutNull)
		{
			AddError(FString::Printf(TEXT("Strnstr(\"%.*s\", %d, \"%.*s\", %d)\" did not equal index \"%d\", when embedded in a string without a nullterminator."),
				Search.Len(), Search.GetData(), Search.Len(), Find.Len(), Find.GetData(), Find.Len(), ExpectedSensitiveIndex));
		}
		if (FCString::Strnistr(Search.GetData(), Search.Len(), Find.GetData(), Find.Len()) != ExpectedInsensitive)
		{
			AddError(FString::Printf(TEXT("Strnistr(\"%.*s\", %d, \"%.*s\", %d)\" did not equal index \"%d\"."),
				Search.Len(), Search.GetData(), Search.Len(), Find.Len(), Find.GetData(), Find.Len(), ExpectedInsensitiveIndex));
		}
		if (FCString::Strnistr(SearchWithoutNull.GetData(), Search.Len(), FindWithoutNull.GetData(), Find.Len()) != ExpectedInsensitiveWithoutNull)
		{
			AddError(FString::Printf(TEXT("Strnistr(\"%.*s\", %d, \"%.*s\", %d)\" did not equal index \"%d\", when embedded in a string without a nullterminator."),
				Search.Len(), Search.GetData(), Search.Len(), Find.Len(), Find.GetData(), Find.Len(), ExpectedInsensitiveIndex));
		}
	};
	FStringView ABACADAB(TEXTVIEW("ABACADAB"));

	RunTest(ABACADAB, TEXTVIEW("A"), 0, 0);
	RunTest(ABACADAB, TEXTVIEW("a"), INDEX_NONE, 0);
	RunTest(ABACADAB, TEXTVIEW("BAC"), 1, 1);
	RunTest(ABACADAB, TEXTVIEW("BaC"), INDEX_NONE, 1);
	RunTest(ABACADAB, TEXTVIEW("BAC"), 1, 1);
	RunTest(ABACADAB, TEXTVIEW("BaC"), INDEX_NONE, 1);
	RunTest(ABACADAB, TEXTVIEW("DAB"), 5, 5);
	RunTest(ABACADAB, TEXTVIEW("dab"), INDEX_NONE, 5);
	RunTest(ABACADAB, ABACADAB, 0, 0);
	RunTest(ABACADAB, TEXTVIEW("abacadab"), INDEX_NONE, 0);
	RunTest(ABACADAB, TEXTVIEW("F"), INDEX_NONE, INDEX_NONE);
	RunTest(ABACADAB, TEXTVIEW("DABZ"), INDEX_NONE, INDEX_NONE);
	RunTest(ABACADAB, TEXTVIEW("ABACADABA"), INDEX_NONE, INDEX_NONE);
	RunTest(ABACADAB, TEXTVIEW("NoMatchLongerString"), INDEX_NONE, INDEX_NONE);
	RunTest(TEXTVIEW(""), TEXTVIEW("FindText"), INDEX_NONE, INDEX_NONE);
	RunTest(TEXTVIEW(""), TEXTVIEW(""), 0, 0);
	RunTest(ABACADAB, TEXTVIEW(""), 0, 0);

	// Tests that pass in nullptr
	const TCHAR* NullString = nullptr;
	const TCHAR* EmptyString = TEXT("");
	if (FCString::Strnstr(NullString, 0, NullString, 0) != nullptr ||
		FCString::Strnistr(NullString, 0, NullString, 0) != nullptr)
	{
		AddError(TEXT("Strnstr(nullptr, 0, nullptr, 0) did not equal nullptr."));
	}
	if (FCString::Strnstr(EmptyString, 0, NullString, 0) != EmptyString ||
		FCString::Strnistr(EmptyString, 0, NullString, 0) != EmptyString)
	{
		AddError(TEXT("Strnstr(EmptyString, 0, nullptr, 0) did not equal EmptyString."));
	}
	if (FCString::Strnstr(NullString, 0, EmptyString, 0) != nullptr ||
		FCString::Strnistr(NullString, 0, EmptyString, 0) != nullptr)
	{
		AddError(TEXT("Strnstr(nullptr, 0, EmptyString, 0) did not equal nullptr."));
	}

	// Negative lengths are not allowed so we do not test them.

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
