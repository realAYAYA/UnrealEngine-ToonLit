// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformString.h"

#include "Misc/CString.h"
#include "Templates/UnrealTemplate.h"
#include "Tests/TestHarnessAdapter.h"

#if WITH_TESTS

template <typename CharType, SIZE_T Size>
static void InvokePlatformStringGetVarArgs(CharType (&Dest)[Size], const CharType* Fmt, ...)
{
	va_list ap;
	va_start(ap, Fmt);
	FPlatformString::GetVarArgs(Dest, Size, Fmt, ap);
	va_end(ap);
}

TEST_CASE_NAMED(FPlatformStringTestGetVarArgs, "System::Core::HAL::PlatformString::GetVarArgs", "[ApplicationContextMask][EngineFilter]")
{
	TCHAR Buffer[128];
	InvokePlatformStringGetVarArgs(Buffer, TEXT("A%.*sZ"), 4, TEXT(" to B"));
	CHECK_MESSAGE(TEXT("GetVarArgs(%.*s)"), FCString::Strcmp(Buffer, TEXT("A to Z")) == 0);
}

TEST_CASE_NAMED(FPlatformStringTestStrnlen, "System::Core::HAL::PlatformString::Strnlen", "[ApplicationContextMask][EngineFilter]")
{
	CHECK_MESSAGE(TEXT("Strnlen(nullptr, 0)"), FPlatformString::Strnlen((const ANSICHAR*)nullptr, 0) == 0); //-V575
	CHECK_MESSAGE(TEXT("Strnlen(\"\", 0)"), FPlatformString::Strnlen("", 0) == 0);  //-V575
	CHECK_MESSAGE(TEXT("Strnlen(\"1\", 0)"), FPlatformString::Strnlen("1", 0) == 0);  //-V575
	CHECK_MESSAGE(TEXT("Strnlen(\"1\", 1)"), FPlatformString::Strnlen("1", 1) == 1);
	CHECK_MESSAGE(TEXT("Strnlen(\"1\", 2)"), FPlatformString::Strnlen("1", 2) == 1);
	CHECK_MESSAGE(TEXT("Strnlen(\"123\", 2)"), FPlatformString::Strnlen("123", 2) == 2);
	ANSICHAR AnsiBuffer[128] = "123456789";
	CHECK_MESSAGE(TEXT("Strnlen(PaddedBuffer)"), FPlatformString::Strnlen(AnsiBuffer, UE_ARRAY_COUNT(AnsiBuffer)) == 9);

	CHECK_MESSAGE(TEXT("Strnlen(nullptr, 0)"), FPlatformString::Strnlen((const TCHAR*)nullptr, 0) == 0); //-V575
	CHECK_MESSAGE(TEXT("Strnlen(\"\", 0)"), FPlatformString::Strnlen(TEXT(""), 0) == 0); //-V575
	CHECK_MESSAGE(TEXT("Strnlen(\"1\", 0)"), FPlatformString::Strnlen(TEXT("1"), 0) == 0); //-V575
	CHECK_MESSAGE(TEXT("Strnlen(\"1\", 1)"), FPlatformString::Strnlen(TEXT("1"), 1) == 1);
	CHECK_MESSAGE(TEXT("Strnlen(\"1\", 2)"), FPlatformString::Strnlen(TEXT("1"), 2) == 1);
	CHECK_MESSAGE(TEXT("Strnlen(\"123\", 2)"), FPlatformString::Strnlen(TEXT("123"), 2) == 2);
	TCHAR Buffer[128] = {};
	FCString::Strcpy(Buffer, TEXT("123456789"));
	CHECK_MESSAGE(TEXT("Strnlen(PaddedBuffer)"), FPlatformString::Strnlen(Buffer, UE_ARRAY_COUNT(Buffer)) == 9);
}

#endif //WITH_TESTS
