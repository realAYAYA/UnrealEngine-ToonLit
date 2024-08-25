// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericWidePlatformString.h"

#if PLATFORM_USE_GENERIC_STRING_IMPLEMENTATION

#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Templates/UnrealTemplate.h"

#if WITH_LOW_LEVEL_TESTS
#include "TestHarness.h"
#include <catch2/generators/catch_generators.hpp>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogStandardPlatformString, Log, All);

WIDECHAR* FGenericWidePlatformString::Strcpy(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
{
	TCHAR *BufPtr = Dest;
	
	while (*Src && --DestCount)
	{
		*BufPtr++ = *Src++;
	}
	
	*BufPtr = 0;
	
	return Dest;
}

WIDECHAR* FGenericWidePlatformString::Strncpy(WIDECHAR* Dest, const WIDECHAR* Src, SIZE_T MaxLen)
{
	TCHAR *BufPtr = Dest;
	
	// the spec says that strncpy should fill the buffer with zeroes
	// we break the spec by enforcing a trailing zero, so we do --MaxLen instead of MaxLen--
	bool bFillWithZero = false;
	while (--MaxLen)
	{
		if (bFillWithZero)
		{
			*BufPtr++ = 0;
		}
		else
		{
			if (*Src == 0)
			{
				bFillWithZero = true;
			}
			*BufPtr++ = *Src++;
		}
	}
	
	// always have trailing zero
	*BufPtr = 0;
	
	return Dest;
}

WIDECHAR* FGenericWidePlatformString::Strcat(WIDECHAR* Dest, SIZE_T DestCount, const WIDECHAR* Src)
{
	TCHAR *String = Dest;
	
	while (*String != 0 && DestCount > 1)
	{
		String++;
		// remove how much we can copy in the lower loop
		DestCount--;
	}
	
	while (*Src != 0 && DestCount > 1)
	{
		*String++ = *Src++;
		DestCount--;
	}
	
//	// rewind 1 for trailing zero
//	String--;
	*String = 0;
	
	return Dest;
}

int32 FGenericWidePlatformString::Strtoi( const WIDECHAR* Start, WIDECHAR** End, int32 Base )
{
#if PLATFORM_TCHAR_IS_4_BYTES || PLATFORM_TCHAR_IS_UTF8CHAR
	unimplemented();
#else
	static_assert(sizeof(TCHAR) == 2, "TCHAR is expected to be 16-bit");
#endif

	if (End == nullptr)
	{
		return Strtoi(TCHAR_TO_UTF8(Start), nullptr, Base);
	}

#if UE_BUILD_DEBUG
	// make sure we aren't using high byte characters
	// @todo this!
#endif
	
	// convert to ANSI, and remember the end to get an offset
	auto Ansi = StringCast<ANSICHAR>(Start);
	ANSICHAR* AnsiEnd;
	int32 Result = Strtoi(Ansi.Get(), &AnsiEnd, Base);
	
	// the end is the offset from
	*End = const_cast<WIDECHAR*>(Start) + (AnsiEnd - Ansi.Get());

	return Result;
}

int64 FGenericWidePlatformString::Strtoi64( const WIDECHAR* Start, WIDECHAR** End, int32 Base )
{
#if PLATFORM_TCHAR_IS_4_BYTES || PLATFORM_TCHAR_IS_UTF8CHAR
	unimplemented();
#else
	static_assert(sizeof(TCHAR) == 2, "TCHAR is expected to be 16-bit");
#endif

	if (End == nullptr)
	{
		return Strtoi64(TCHAR_TO_UTF8(Start), nullptr, Base);
	}

#if UE_BUILD_DEBUG
	// make sure we aren't using high byte characters
	// @todo this!
#endif
	
	// convert to ANSI, and remember the end to get an offset
	auto Ansi = StringCast<ANSICHAR>(Start);
	ANSICHAR* AnsiEnd;
	int64 Result = Strtoi64(Ansi.Get(), &AnsiEnd, Base);
	
	// the end is the offset from
	*End = const_cast<WIDECHAR*>(Start) + (AnsiEnd - Ansi.Get());

	return Result;
}

uint64 FGenericWidePlatformString::Strtoui64( const WIDECHAR* Start, WIDECHAR** End, int32 Base )
{
	if (End == nullptr)
	{
		return Strtoui64(TCHAR_TO_UTF8(Start), nullptr, Base);
	}

#if UE_BUILD_DEBUG
	// make sure we aren't using high byte characters
	// @todo this!
#endif
	
	// convert to ANSI, and remember the end to get an offset
	auto Ansi = StringCast<ANSICHAR>(Start);
	ANSICHAR* AnsiEnd;
	uint64 Result = Strtoui64(Ansi.Get(), &AnsiEnd, Base);
	
	// the end is the offset from
	*End = const_cast<WIDECHAR*>(Start) + (AnsiEnd - Ansi.Get());

	return Result;
}

WIDECHAR* FGenericWidePlatformString::Strtok(WIDECHAR* StrToken, const WIDECHAR* Delim, WIDECHAR** Context)
{
	check(Context);
	check(Delim);

	WIDECHAR* SearchString = StrToken;
	if (!SearchString)
	{
		check(*Context);
		SearchString = *Context;
	}

	WIDECHAR* TokenStart = SearchString;
	while (*TokenStart && Strchr(Delim, *TokenStart))
	{
		++TokenStart;
	}

	if (*TokenStart == 0)
	{
		return nullptr;
	}

	WIDECHAR* TokenEnd = TokenStart;
	while (*TokenEnd && !Strchr(Delim, *TokenEnd))
	{
		++TokenEnd;
	}

	*TokenEnd = 0;
	*Context = TokenEnd + 1;

	return TokenStart;
}

float FGenericWidePlatformString::Atof(const WIDECHAR* String)
{
	return Atof(TCHAR_TO_UTF8(String));
}

double FGenericWidePlatformString::Atod(const WIDECHAR* String)
{
	return Atod(TCHAR_TO_UTF8(String));
}


#if PLATFORM_ANDROID
// This is a full copy of iswspace function from Android sources
// For some reason function from libc does not work correctly for some korean characters like: 0xBE0C
int iswspace(wint_t wc)
{
	static const wchar_t spaces[] = {
		' ', '\t', '\n', '\r', 11, 12,  0x0085,
		0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
		0x2006, 0x2008, 0x2009, 0x200a,
		0x2028, 0x2029, 0x205f, 0x3000, 0
	};
	return wc && wcschr(spaces, wc);
}
#endif


#if WITH_LOW_LEVEL_TESTS
constexpr int OUTPUT_SIZE = 256;
int32 TestGetVarArgs(WIDECHAR* OutputString, const WIDECHAR* Format, ...)
{
	va_list ArgPtr;
	va_start( ArgPtr, Format );
	int32 Result = FGenericWidePlatformString::GetVarArgs(OutputString, OUTPUT_SIZE, Format, ArgPtr);
	va_end(ArgPtr);
	return Result;
}

TEST_CASE("Core::PlatformString::GetVarArgs", "[Core][String][Smoke]")
{
	WIDECHAR OutputString[OUTPUT_SIZE];

	TestGetVarArgs(OutputString, TEXT("Test A|%-20s|%20s|%10.2f|%-10.2f|"), TEXT("LEFT"), TEXT("RIGHT"), 33.333333, 66.666666);
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test A|LEFT                |               RIGHT|     33.33|66.67     |")));

	TestGetVarArgs(OutputString, TEXT("Test B|Percents:%%%%%%%d|"), 3);
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test B|Percents:%%%3|")));

	TestGetVarArgs(OutputString, TEXT("Test C|%d|%i|%X|%x|%u|"), 12345, 54321, 0x123AbC, 15, 99);
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test C|12345|54321|123ABC|f|99|")));

	TestGetVarArgs(OutputString, TEXT("Test D|%p|"), 0x12345);
#ifdef _MSC_VER
	// MSVC's standard library formats pointers differently
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test D|0000000000012345|")));
#else
	// Pointer format (which we get from snprintf) varies from platform to platform.
	// Call it to make sure it puts the output in the proper place, but don't test for the exact format
//	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test D|0x12345|")));
	{
		FString OutputStringStr(OutputString);
		CHECK_MESSAGE(OutputString, OutputStringStr.StartsWith(TEXT("Test D|"))
			&& OutputStringStr.EndsWith(TEXT("|")) && OutputStringStr.Len() < TEXTVIEW("Test D||").Len() + 20);
	}
#endif

	TestGetVarArgs(OutputString, TEXT("Test E|%" INT64_FMT "|"), int64(12345678912345LL));
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test E|12345678912345|")));

	TestGetVarArgs(OutputString, TEXT("Test F|%f|%e|%g|"), 123.456, 123.456, 123.456);
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test F|123.456000|1.234560e+02|123.456|")));

	TestGetVarArgs(OutputString, TEXT("Test G|%" UPTRINT_X_FMT"|"), UPTRINT(49374));
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test G|C0DE|")));

	TestGetVarArgs(OutputString, TEXT("Test H|%" UPTRINT_x_FMT "|"), UPTRINT(49374));
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test H|c0de|")));

	TestGetVarArgs(OutputString, TEXT("Test I|%" UINT64_FMT "|"), MAX_uint64);
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test I|18446744073709551615|")));

	TestGetVarArgs(OutputString, TEXT("Test J|%*c|"), 10, 'J');
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test J|         J|")));

	TestGetVarArgs(OutputString, TEXT("Test K|%-5c|"), 'K');
	CHECK_MESSAGE(OutputString, FString(OutputString) == FString(TEXT("Test K|K    |")));
}
#endif // WITH_LOW_LEVEL_TESTS

namespace
{
	// Output iterator which will not overflow the destination buffer but will keep track of how many characters have been written.
	// It will also null terminate the output when destructed.
	struct FSafeDestIterator
	{
		// Non-copyable
		FSafeDestIterator(const FSafeDestIterator&) = delete;
		FSafeDestIterator& operator=(const FSafeDestIterator&) = delete;

		explicit FSafeDestIterator(WIDECHAR* InPtr, SIZE_T InEnd)
			: NumCharsWritten(0)
			, Ptr            (InPtr)
			, EndMinusOne    (InPtr + InEnd - 1)
		{
			check(InPtr);
		}

		FORCENOINLINE ~FSafeDestIterator()
		{
			// Auto-terminate buffer
			*Ptr = 0;
		}

		// Writes Count instances of Ch to the destination buffer
		FORCENOINLINE FSafeDestIterator& Write(TCHAR Ch, int32 Count = 1)
		{
			// Ensure we're not in the error state
			if (EndMinusOne)
			{
				NumCharsWritten += Count;

				int32 NumToWrite = FPlatformMath::Min(Count, (int32)(EndMinusOne - Ptr));
				for (int32 Val = NumToWrite; Val; --Val)
				{
					*Ptr++ = Ch;
				}

				if (NumToWrite != Count)
				{
					EndMinusOne = 0;
				}
			}

			return *this;
		}

		// Writes Count characters from Src to the destination buffer
		template <typename CharType>
		FORCENOINLINE FSafeDestIterator& Write(const CharType* Src, int32 Count)
		{
			// Ensure we're not in the error state
			if (EndMinusOne)
			{
				NumCharsWritten += Count;

				int32 NumToWrite = FPlatformMath::Min(Count, (int32)(EndMinusOne - Ptr));
				for (int32 Val = NumToWrite; Val; --Val)
				{
					*Ptr++ = *Src++;
				}

				if (NumToWrite != Count)
				{
					EndMinusOne = 0;
				}
			}

			return *this;
		}

		explicit operator bool() const
		{
			return !!EndMinusOne;
		}

		int32 GetNumCharsWritten() const
		{
			return NumCharsWritten;
		}

	private:
		int32     NumCharsWritten;
		WIDECHAR* Ptr;
		WIDECHAR* EndMinusOne; // when null, this means the iterator has already moved past the writable area of the buffer
	};

	static inline bool CharIsIntegerFormatSpecifier(TCHAR Char)
	{
		return (Char == 'i') | (Char == 'd') | (Char == 'u') | (Char == 'X') | (Char == 'x');
	}

	template <typename CharType, typename VaListType>
	void ProcessStringArg(FSafeDestIterator& DestIter, const TCHAR*& Src, int FieldLen, int PrecisionLen, VaListType& ArgPtr)
	{
		// ArgPtr is taken as a templated parameter, despite being a va_list, because on some
		// architectures it could be an array type, and when an array is used as a function parameter,
		// the named value is treated as a pointer and won't bind to an array reference.

		Src++;
		static const CharType* Null = LITERAL(CharType, "(null)");
		const CharType* Val = va_arg(ArgPtr, CharType*);
		if (Val == nullptr)
		{
			Val = Null;
		}

		int RetCnt = PrecisionLen < 0 ? FGenericWidePlatformString::Strlen(Val) : FGenericWidePlatformString::Strnlen(Val, PrecisionLen);
		int Spaces = FPlatformMath::Max(FPlatformMath::Abs(FieldLen) - RetCnt, 0);
		if (Spaces > 0 && FieldLen > 0)
		{
			DestIter.Write(TEXT(' '), Spaces);
		}
		DestIter.Write(Val, RetCnt);
		if (Spaces > 0 && FieldLen < 0)
		{
			DestIter.Write(TEXT(' '), Spaces);
		}
	}
}

int32 FGenericWidePlatformString::GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, const WIDECHAR*& Fmt, va_list ArgPtr )
{
#if PLATFORM_TCHAR_IS_4_BYTES || PLATFORM_TCHAR_IS_UTF8CHAR
	unimplemented();
#else
	static_assert(sizeof(TCHAR) == 2, "TCHAR is expected to be 16-bit");
#endif

	if (Fmt == nullptr)
	{
		if ((DestSize > 0) && (Dest != nullptr))
		{
			*Dest = 0;
		}
		return 0;
	}

	const TCHAR *Src = Fmt;

	FSafeDestIterator DestIter(Dest, DestSize);
	while (*Src)
	{
		if (*Src != '%')
		{
			if (!DestIter.Write(*Src++))
			{
				return -1;
			}
			continue;
		}

		const TCHAR *Percent = Src;
		int FieldLen = 0;
		bool bVariablePrecision = false;
		int PrecisionLen = -1;

		Src++; // skip the '%' char...

		// Skip modifier flags that don't need additional processing;
		// they still get passed to snprintf() below based on the conversion.
		while (*Src == '+' || *Src == '#' || *Src == TEXT(' ') || *Src == TEXT('0'))
		{
			Src++;
		}

		// check for field width requests...
		if ((*Src == '-') || ((*Src >= '0') && (*Src <= '9')))
		{
			const TCHAR *Cur = Src + 1;
			while ((*Cur >= '0') && (*Cur <= '9'))
			{
				Cur++;
			}

			FieldLen = Atoi(Src);
			Src = Cur;
		}

		// check for dynamic field requests
		if (*Src == '*')
		{
			FieldLen = va_arg(ArgPtr, int32);
			Src++;
		}

		if (*Src == '.')
		{
			const TCHAR *Cur = Src + 1;
			if (*Cur == '*')
			{
				bVariablePrecision = true;
				PrecisionLen = va_arg(ArgPtr, int32);
				Cur++;
			}
			else
			{
				while ((*Cur >= '0') && (*Cur <= '9'))
				{
					Cur++;
				}

				PrecisionLen = Atoi(Src + 1);
			}
			Src = Cur;
		}

		// Check for 'ls' field, change to 's'
		if ((Src[0] == 'l' && Src[1] == 's'))
		{
			Src++;
		}

		// Check for 'hs' field, change to 's' and remember
		bool bIsNarrowString = false;
		if ((Src[0] == 'h' && Src[1] == 's'))
		{
			bIsNarrowString = true;
			Src++;
		}

		switch (*Src)
		{
			case '%':
			{
				Src++;
				if (!DestIter.Write('%'))
				{
					return -1;
				}
				break;
			}

			case 'c':
			{
				TCHAR Val = (TCHAR) va_arg(ArgPtr, int);
				Src++;
				bool bSuccess = true;
				if (FieldLen > 1)
				{
					bSuccess = !!DestIter.Write(TCHAR(' '), FieldLen - 1);
				}
				bSuccess = bSuccess && !!DestIter.Write(Val);
				if (FieldLen < -1)
				{
					bSuccess = bSuccess && !!DestIter.Write(TCHAR(' '), FPlatformMath::Abs(FieldLen + 1));
				}
				if (!bSuccess)
				{
					return -1;
				}
				break;
			}

			case 'd':
			case 'i':
			case 'X':
			case 'x':
			case 'u':
			{
				Src++;
				int Val = va_arg(ArgPtr, int);
				ANSICHAR AnsiNum[64];
				ANSICHAR FmtBuf[30];

				// limit width to the buffer size
				FieldLen = FMath::Min(static_cast<int>(sizeof(AnsiNum)) - 1, FieldLen);

				// Yes, this is lame.
				int CpyIdx = 0;
				while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
				{
					if (LIKELY(*Percent != '*'))
					{
						FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
						++CpyIdx;
					}
					else
					{
						snprintf(&FmtBuf[CpyIdx], sizeof(FmtBuf) - CpyIdx, "%d", FieldLen);
						while (CpyIdx < UE_ARRAY_COUNT(FmtBuf) && FmtBuf[CpyIdx] != 0)
						{
							++CpyIdx;
						}
					}
					++Percent;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
				if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
				{
					return -1;
				}
				break;
			}

			case 'z':
			case 'Z':
			{
				if (Src[1] == 0)
				{
					printf("Unknown percent [%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], TCHAR_TO_ANSI(Fmt));
					Src++;  // skip it, I guess.
					break;
				}

				Src += 2;

				size_t Val = va_arg(ArgPtr, size_t);

				ANSICHAR AnsiNum[64];
				ANSICHAR FmtBuf[30];

				// limit width to the buffer size
				FieldLen = FMath::Min(static_cast<int>(sizeof(AnsiNum)) - 1, FieldLen);

				// Yes, this is lame.
				int CpyIdx = 0;
				while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
				{
					if (LIKELY(*Percent != '*'))
					{
						FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
						++CpyIdx;
					}
					else
					{
						snprintf(&FmtBuf[CpyIdx], sizeof(FmtBuf) - CpyIdx, "%d", FieldLen);
						while (CpyIdx < UE_ARRAY_COUNT(FmtBuf) && FmtBuf[CpyIdx] != 0)
						{
							++CpyIdx;
						}
					}
					++Percent;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
				if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
				{
					return -1;
				}
				break;
			}

			case 'p':
			{
				Src++;
				void* Val = va_arg(ArgPtr, void*);
				ANSICHAR AnsiNum[30];
				ANSICHAR FmtBuf[30];

				// Yes, this is lame.
				int CpyIdx = 0;
				while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
				{
					FmtBuf[CpyIdx] = (ANSICHAR)*Percent;
					Percent++;
					CpyIdx++;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
				if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
				{
					return -1;
				}
				break;
			}

			case 'l':
			case 'I':
			case 'h':
			{
				int RemainingSize = Strlen(Src);

				if (RemainingSize < 2)
				{
					printf("Unknown percent [%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], TCHAR_TO_ANSI(Fmt));
					Src++;  // skip it, I guess.
					break;
				}

				// treat %ld as %d. Also shorts for %h will be promoted to ints. This path also handles %li, %lu, %lx and %lX.
				if ((Src[0] == 'l' || Src[0] == 'h') && CharIsIntegerFormatSpecifier(Src[1]))
				{
					Src+=2;
					long int Val = va_arg(ArgPtr, long int);
					ANSICHAR AnsiNum[30];
					ANSICHAR FmtBuf[30];

					// Yes, this is lame.
					int CpyIdx = 0;
					while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
					{
						FmtBuf[CpyIdx] = (ANSICHAR)*Percent;
						Percent++;
						CpyIdx++;
					}
					FmtBuf[CpyIdx] = 0;

					int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
					if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
					{
						return -1;
					}
					break;
				}
				// Treat %lf as a %f
				else if (Src[0] == 'l' && Src[1] == 'f')
				{
					Src += 2;
					double Val = va_arg(ArgPtr, double);
					ANSICHAR AnsiNum[340]; // make sure buffer is large enough to handle the longest double values, such as the largest number which is 316 characters including decimal point and 6 decimals
					ANSICHAR FmtBuf[30];

					// Yes, this is lame.
					int CpyIdx = 0;
					while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
					{
						FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
						Percent++;
						CpyIdx++;
					}
					FmtBuf[CpyIdx] = 0;

					int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
					if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
					{
						return -1;
					}
					break;
				}

				// At this point we expect RemainingSize >= 3, but we know it's at least 2.
				if (RemainingSize < 3)
				{
					printf("Unknown percent [%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], TCHAR_TO_ANSI(Fmt));
					Src++;  // skip it, I guess.
					break;
				}

				// Deal with half-half formats - %hhd, %hhi, %hhu, %hhx and %hhX.
				if (Src[0] == 'h')
				{
					if (Src[1] != 'h' || !CharIsIntegerFormatSpecifier(Src[2]))
					{
						printf("Unknown percent [%lc%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], Src[2], TCHAR_TO_ANSI(Fmt));
						Src++;  // skip it, I guess.
						break;
					}
					Src += 3;
					int Val = va_arg(ArgPtr, int);
					ANSICHAR AnsiNum[8];
					ANSICHAR FmtBuf[30];

					// Yes, this is lame.
					int CpyIdx = 0;
					while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
					{
						FmtBuf[CpyIdx] = (ANSICHAR)*Percent;
						Percent++;
						CpyIdx++;
					}
					FmtBuf[CpyIdx] = 0;

					int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
					if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
					{
						return -1;
					}
					break;
				}
				else if (Src[0] == 'l')
				{
					if (Src[1] != 'l' || !CharIsIntegerFormatSpecifier(Src[2]))
					{
						printf("Unknown percent [%lc%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], Src[2], TCHAR_TO_ANSI(Fmt));
						Src++;  // skip it, I guess.
						break;
					}
				}
				else if (Src[0] == 'I')
				{
					if (RemainingSize < 4)
					{
						printf("Unknown percent [%lc%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], Src[2], TCHAR_TO_ANSI(Fmt));
						Src++;  // skip it, I guess.
						break;
					}
					else if (Src[1] != '6' || Src[2] != '4' || !CharIsIntegerFormatSpecifier(Src[3]))
					{
						printf("Unknown percent [%lc%lc%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], Src[2], Src[3], TCHAR_TO_ANSI(Fmt));
						Src++;  // skip it, I guess.
						break;
					}
				}
				else
				{
					printf("Unknown percent [%lc%lc%lc] in FGenericWidePlatformString::GetVarArgs() [%s]\n.", Src[0], Src[1], Src[2], TCHAR_TO_ANSI(Fmt));
					Src++;  // skip it, I guess.
					break;
				}

				// Yes, this is lame.
				unsigned long long Val = va_arg(ArgPtr, unsigned long long);
				ANSICHAR AnsiNum[60];
				ANSICHAR FmtBuf[30];

				int CpyIdx;
				if (Src[0] == 'I')
				{
					// Convert I64 syntax to ll 
					Src += 4;
					Percent += 4;
					CpyIdx = 3;
					FmtBuf[0] = '%';
					FmtBuf[1] = 'l';
					FmtBuf[2] = 'l';
				}
				else
				{
					Src += 3;
					CpyIdx = 0;
				}

				while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
				{
					FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
					Percent++;
					CpyIdx++;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt = snprintf(AnsiNum, sizeof(AnsiNum), FmtBuf, Val);
				if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
				{
					return -1;
				}
				break;
			}

			case 'f':
			case 'e':
			case 'g':
			{
				Src++;
				double Val = va_arg(ArgPtr, double);
				ANSICHAR AnsiNum[340]; // make sure buffer is large enough to handle the longest double values, such as the largest number which is 316 characters including decimal point and 6 decimals
				ANSICHAR FmtBuf[30];

				// Yes, this is lame.
				int CpyIdx = 0;
				while (Percent < Src && CpyIdx < UE_ARRAY_COUNT(FmtBuf))
				{
					FmtBuf[CpyIdx] = (ANSICHAR) *Percent;
					Percent++;
					CpyIdx++;
				}
				FmtBuf[CpyIdx] = 0;

				int RetCnt;
				if (bVariablePrecision)
				{
					RetCnt = snprintf(AnsiNum, sizeof (AnsiNum), FmtBuf, PrecisionLen, Val);
				}
				else
				{
					RetCnt = snprintf(AnsiNum, sizeof (AnsiNum), FmtBuf, Val);
				}
				if (RetCnt >= UE_ARRAY_COUNT(AnsiNum))
				{
					// We should print what we have written into AnsiNum but ensure we null terminate before printing
					AnsiNum[UE_ARRAY_COUNT(AnsiNum) - 1] = '\0';
					checkf(0, TEXT("Attempting to read past the size our buffer. Buffer Size: %d Size to read: %d. Current contents: '%s'\n"), UE_ARRAY_COUNT(AnsiNum), RetCnt, UTF8_TO_TCHAR(AnsiNum));
				}
				if (RetCnt < 0 || !DestIter.Write(AnsiNum, RetCnt))
				{
					return -1;
				}
				break;
			}

			case 's':
			{
				if (bIsNarrowString)
				{
					ProcessStringArg<ANSICHAR>(DestIter, Src, FieldLen, PrecisionLen, ArgPtr);
				}
				else
				{
					ProcessStringArg<TCHAR>(DestIter, Src, FieldLen, PrecisionLen, ArgPtr);
				}
				if (!DestIter)
				{
					return -1;
				}
				break;
			}

			case 'S':
			{
				// The %S format represents a string which is the opposite of %s - wide if TCHAR is narrow, or narrow if TCHAR is wide
				using OtherCharType = std::conditional_t<std::is_same_v<TCHAR, ANSICHAR>, WIDECHAR, ANSICHAR>;

				ProcessStringArg<OtherCharType>(DestIter, Src, FieldLen, PrecisionLen, ArgPtr);
				if (!DestIter)
				{
					return -1;
				}
				break;
			}

			default:
				printf("Unknown percent [%%%c] in FGenericWidePlatformString::GetVarArgs().\n", *Src);
				Src++;  // skip char, I guess.
				break;
		}
	}

	int32 Result = DestIter.GetNumCharsWritten();
	return Result;
}

#endif // PLATFORM_USE_GENERIC_STRING_IMPLEMENTATION
