// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"

#if WITH_DEV_AUTOMATION_TESTS

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

#endif // WITH_DEV_AUTOMATION_TESTS
