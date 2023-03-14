// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformString.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

template <typename CharType, SIZE_T Size>
static void InvokePlatformStringGetVarArgs(CharType (&Dest)[Size], const CharType* Fmt, ...)
{
	va_list ap;
	va_start(ap, Fmt);
	FPlatformString::GetVarArgs(Dest, Size, Fmt, ap);
	va_end(ap);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformStringTestGetVarArgs, "System.Core.HAL.PlatformString.GetVarArgs", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPlatformStringTestGetVarArgs::RunTest(const FString& Parameters)
{
	TCHAR Buffer[128];
	InvokePlatformStringGetVarArgs(Buffer, TEXT("A%.*sZ"), 4, TEXT(" to B"));
	TestEqual(TEXT("GetVarArgs(%.*s)"), Buffer, TEXT("A to Z"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformStringTestStrnlen, "System.Core.HAL.PlatformString.Strnlen", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPlatformStringTestStrnlen::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Strnlen(nullptr, 0)"), FPlatformString::Strnlen((const ANSICHAR*)nullptr, 0), 0);
	TestEqual(TEXT("Strnlen(\"\", 0)"), FPlatformString::Strnlen("", 0), 0);
	TestEqual(TEXT("Strnlen(\"1\", 0)"), FPlatformString::Strnlen("1", 0), 0);
	TestEqual(TEXT("Strnlen(\"1\", 1)"), FPlatformString::Strnlen("1", 1), 1);
	TestEqual(TEXT("Strnlen(\"1\", 2)"), FPlatformString::Strnlen("1", 2), 1);
	TestEqual(TEXT("Strnlen(\"123\", 2)"), FPlatformString::Strnlen("123", 2), 2);
	ANSICHAR AnsiBuffer[128] = "123456789";
	TestEqual(TEXT("Strnlen(PaddedBuffer)"), FPlatformString::Strnlen(AnsiBuffer, UE_ARRAY_COUNT(AnsiBuffer)), 9);

	TestEqual(TEXT("Strnlen(nullptr, 0)"), FPlatformString::Strnlen((const TCHAR*)nullptr, 0), 0); //-V575
	TestEqual(TEXT("Strnlen(\"\", 0)"), FPlatformString::Strnlen(TEXT(""), 0), 0); //-V575
	TestEqual(TEXT("Strnlen(\"1\", 0)"), FPlatformString::Strnlen(TEXT("1"), 0), 0); //-V575
	TestEqual(TEXT("Strnlen(\"1\", 1)"), FPlatformString::Strnlen(TEXT("1"), 1), 1);
	TestEqual(TEXT("Strnlen(\"1\", 2)"), FPlatformString::Strnlen(TEXT("1"), 2), 1);
	TestEqual(TEXT("Strnlen(\"123\", 2)"), FPlatformString::Strnlen(TEXT("123"), 2), 2);
	TCHAR Buffer[128] = {};
	FCString::Strcpy(Buffer, TEXT("123456789"));
	TestEqual(TEXT("Strnlen(PaddedBuffer)"), FPlatformString::Strnlen(Buffer, UE_ARRAY_COUNT(Buffer)), 9);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
