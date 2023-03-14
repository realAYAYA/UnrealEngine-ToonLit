// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/Char.h"
#include "Misc/AutomationTest.h"
#include <locale.h>
#include <ctype.h>
#include <wctype.h>

#if WITH_DEV_AUTOMATION_TESTS 

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TCharTest, "System.Core.Misc.Char", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

namespace crt
{
	int tolower(ANSICHAR c) { return ::tolower(c); }
	int toupper(ANSICHAR c) { return ::toupper(c); }

	int tolower(WIDECHAR c) { return ::towlower(c); }
	int toupper(WIDECHAR c) { return ::towupper(c); }
}

template<typename CharType>
void RunCharTests(FAutomationTestBase& Test, uint32 MaxChar)
{
	for (uint32 I = 0; I < MaxChar; ++I)
	{
		CharType C = (CharType)I;
		Test.TestEqual("TChar::ToLower()", TChar<CharType>::ToLower(C), crt::tolower(C));
		Test.TestEqual("TChar::ToUpper()", TChar<CharType>::ToUpper(C), crt::toupper(C));
	}
}

bool TCharTest::RunTest(const FString& Parameters)
{
	const char* CurrentLocale = setlocale(LC_CTYPE, nullptr);
	if (CurrentLocale == nullptr)
	{
		AddError(FString::Printf(TEXT("Locale is null but should be \"C\". Did something call setlocale()?")));
	}
	else if (strcmp("C", CurrentLocale))
	{
		AddError(FString::Printf(TEXT("Locale is \"%s\" but should be \"C\". Did something call setlocale()?"), ANSI_TO_TCHAR(CurrentLocale)));
	}
	else
	{
		RunCharTests<ANSICHAR>(*this, 128);
		RunCharTests<WIDECHAR>(*this, 65536);
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS