// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/StandardPlatformString.h"

// only IOS and Mac were using this file before, but that has all moved over to the FullReplacementPlatformString
#if !PLATFORM_USE_SYSTEM_VSWPRINTF && !PLATFORM_TCHAR_IS_CHAR16

#include "GenericPlatform/StandardPlatformString.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"


DEFINE_LOG_CATEGORY_STATIC(LogStandardPlatformString, Log, All);

#if PLATFORM_IOS || PLATFORM_WINDOWS
	#define VA_LIST_REF va_list&
#else
	#define VA_LIST_REF va_list
#endif


struct FFormatInfo
{
	WIDECHAR Format[32];
	WIDECHAR LengthModifier;
	WIDECHAR Type;
	bool HasDynamicWidth;
};

static int32 GetFormattingInfo(const WIDECHAR* Format, FFormatInfo& OutInfo)
{
	const WIDECHAR* FormatStart = Format++;

	// Skip flags
	while (*Format == LITERAL(WIDECHAR, '#') || *Format == LITERAL(WIDECHAR, '0') || *Format == LITERAL(WIDECHAR, '-')
		|| *Format == LITERAL(WIDECHAR, ' ') || *Format == LITERAL(WIDECHAR, '+') || *Format == LITERAL(WIDECHAR, '\''))
	{
		Format++;
	}

	OutInfo.HasDynamicWidth = false;

	// Skip width
	while ((*Format >= LITERAL(WIDECHAR, '0') && *Format <= LITERAL(WIDECHAR, '9')) || *Format == LITERAL(WIDECHAR, '*'))
	{
		if (*Format == LITERAL(WIDECHAR, '*'))
		{
			OutInfo.HasDynamicWidth = true;
		}
		Format++;
	}

	// Skip precision
	if (*Format == LITERAL(WIDECHAR, '.'))
	{
		Format++;
		while ((*Format >= LITERAL(WIDECHAR, '0') && *Format <= LITERAL(WIDECHAR, '9')) || *Format == LITERAL(WIDECHAR, '*'))
		{
			if (*Format == LITERAL(WIDECHAR, '*'))
			{
				OutInfo.HasDynamicWidth = true;
			}
			Format++;
		}
	}

	OutInfo.LengthModifier = 0;
	if (*Format == LITERAL(WIDECHAR, 'h') || *Format == LITERAL(WIDECHAR, 'l') || *Format == LITERAL(WIDECHAR, 'j')
		|| *Format == LITERAL(WIDECHAR, 'q') || *Format == LITERAL(WIDECHAR, 'L'))
	{
		OutInfo.LengthModifier = *Format++;
		if (*Format == LITERAL(WIDECHAR, 'h'))
		{
			OutInfo.LengthModifier = 'H';
			Format++;
		}
		else if (*Format == LITERAL(WIDECHAR, 'l'))
		{
			OutInfo.LengthModifier = 'L';
			Format++;
		}
	}
	else if (*Format == LITERAL(WIDECHAR, 't') || *Format == LITERAL(WIDECHAR, 'z'))
	{
#if PLATFORM_64BITS
		OutInfo.LengthModifier = LITERAL(WIDECHAR, 'l');
		++Format;
#else
		OutInfo.LengthModifier = *Format++;
#endif
	}

	OutInfo.Type = *Format++;

	// The only valid length modifier for floating point types is L, all other modifiers should be ignored. Length modifier for void pointers should also be ignored.
	if (OutInfo.LengthModifier != LITERAL(WIDECHAR, 'L') &&
		(OutInfo.Type == LITERAL(WIDECHAR, 'f') || OutInfo.Type == LITERAL(WIDECHAR, 'F')
			|| OutInfo.Type == LITERAL(WIDECHAR, 'e') || OutInfo.Type == LITERAL(WIDECHAR, 'E')
			|| OutInfo.Type == LITERAL(WIDECHAR, 'g') || OutInfo.Type == LITERAL(WIDECHAR, 'G')
			|| OutInfo.Type == LITERAL(WIDECHAR, 'a') || OutInfo.Type == LITERAL(WIDECHAR, 'A')
			|| OutInfo.Type == LITERAL(WIDECHAR, 'p')))
	{
		OutInfo.LengthModifier = 0;
	}

	const int32 FormatLength = Format - FormatStart;

	FMemory::Memcpy(OutInfo.Format, FormatStart, FormatLength * sizeof(WIDECHAR));
	int32 OutInfoFormatLength = FormatLength;
	if (OutInfo.HasDynamicWidth)
	{
		if (OutInfo.Type == LITERAL(WIDECHAR, 's'))
		{
			OutInfo.Format[OutInfoFormatLength - 1] = 'l';
			OutInfo.Format[OutInfoFormatLength++] = 's';
		}
		else if (OutInfo.Type == LITERAL(WIDECHAR, 'S'))
		{
			OutInfo.Format[OutInfoFormatLength - 1] = 'h';
			OutInfo.Format[OutInfoFormatLength++] = 's';
		}
	}
	OutInfo.Format[OutInfoFormatLength] = 0;

	// HACKHACKHACK
	// This formatting function expects to understand %s as a string no matter which char width.
	// On mac (and possibly others) this must be fixed up to %S for widechars.
	// So we will do the fixup ONLY if this is a widechar system and the format is given as %s.
	// BUG: This function still doesn't handle char16_t correctly.
	if (sizeof(WIDECHAR) == sizeof(wchar_t))
	{
		if (OutInfo.Type == LITERAL(WIDECHAR, 's'))
		{
			checkSlow(OutInfo.Format[OutInfoFormatLength - 1] == LITERAL(WIDECHAR, 's'));
			OutInfo.Format[OutInfoFormatLength - 1] = LITERAL(WIDECHAR, 'S');
		}
		else if (OutInfo.Type == LITERAL(WIDECHAR, 'S'))
		{
			OutInfo.Format[OutInfoFormatLength - 1] = LITERAL(WIDECHAR, 's');
		}
	}

	return FormatLength;
}

template <typename T1, typename T2>
static int32 FormatString(const FFormatInfo& Info, VA_LIST_REF ArgPtr, WIDECHAR* Formatted, int32 Length)
{
	if (Info.HasDynamicWidth)
	{
		int32 Width = va_arg(ArgPtr, int32);
		if (FChar::ToLower(Info.LengthModifier) == LITERAL(WIDECHAR, 'l'))
		{
			T1 Value = va_arg(ArgPtr, T1);
			return swprintf(Formatted, Length, Info.Format, Width, Value);
		}
		else
		{
			T2 Value = va_arg(ArgPtr, T2);
			return swprintf(Formatted, Length, Info.Format, Width, Value);
		}
	}
	else
	{
		if (FChar::ToLower(Info.LengthModifier) == LITERAL(WIDECHAR, 'l'))
		{
			T1 Value = va_arg(ArgPtr, T1);
			return swprintf(Formatted, Length, Info.Format, Value);
		}
		else
		{
			T2 Value = va_arg(ArgPtr, T2);
			return swprintf(Formatted, Length, Info.Format, Value);
		}
	}
}

template <typename CallableType>
static void FormatArgument(const FFormatInfo& Info, VA_LIST_REF ArgPtr, CallableType Callable)
{
	WIDECHAR Formatted[1024];

	if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 's'))
	{
		if (Info.HasDynamicWidth)
		{
			int32 Width = va_arg(ArgPtr, int32);
			const WIDECHAR* String = va_arg(ArgPtr, WIDECHAR*);
			if (String)
			{
				int32 Length = swprintf(Formatted, UE_ARRAY_COUNT(Formatted), Info.Format, Width, String);
				Callable(Formatted, Length);
			}
			else
			{
				Callable(TEXT("(null)"), UE_ARRAY_COUNT(Formatted));
			}
			return;
		}
		// Is it a plain string?
		else if (Info.Type == LITERAL(WIDECHAR, 's') && Info.Format[1] == LITERAL(WIDECHAR, 'S'))
		{
			const WIDECHAR* String = va_arg(ArgPtr, WIDECHAR*);
			int32 Length = FCString::Strlen(String);
			Callable(String ? String : TEXT("(null)"), Length);
		}
		// Is it a wide string?
		else if (Info.Type == LITERAL(WIDECHAR, 'S') && Info.Format[1] == LITERAL(WIDECHAR, 's'))
		{
			const ANSICHAR* String = va_arg(ArgPtr, ANSICHAR*);
			int32 FormattedLength = FCStringAnsi::Strlen(String);
			Callable(String ? String : "(null)", FormattedLength);
		}
		// Some form of string requiring formatting, such as a left- or right-justified string
		else
		{
			// We call swprintf directly which may expect %S for a widechar string. This will be fixed up
			// by the time we get here (See above in GetFormattingInfo).
			const WIDECHAR* String = va_arg(ArgPtr, WIDECHAR*);
			int32 Length = swprintf(Formatted, UE_ARRAY_COUNT(Formatted), Info.Format, String);
			Callable(Formatted, Length);
		}
		return;
	}

	if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'c'))
	{
		const WIDECHAR Char = (WIDECHAR)va_arg(ArgPtr, int32);
		Formatted[0] = Char;
		Formatted[1] = 0;
		Callable(Formatted, 1);
		return;
	}

	int32 Length = 0;
	if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'a') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'e')
		|| FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'f') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'g'))
	{
		Length = FormatString<long double, double>(Info, ArgPtr, Formatted, UE_ARRAY_COUNT(Formatted));
	}
	else if (Info.Type == LITERAL(WIDECHAR, 'p'))
	{
		void* Value = va_arg(ArgPtr, void*);
		Length = swprintf(Formatted, UE_ARRAY_COUNT(Formatted), Info.Format, Value);
	}
	else if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'd') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'i'))
	{
		Length = FormatString<int64, int32>(Info, ArgPtr, Formatted, UE_ARRAY_COUNT(Formatted));
	}
	else if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'o') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'u') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'x'))
	{
		Length = FormatString<uint64, uint32>(Info, ArgPtr, Formatted, UE_ARRAY_COUNT(Formatted));
	}

	check(Length != -1);

	Callable(Formatted, Length);
}

int32 FStandardPlatformString::GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, const WIDECHAR*& Fmt, va_list ArgPtr )
{
	const WIDECHAR* Format = Fmt;
	const WIDECHAR* DestStart = Dest;

	if (DestSize == 0)
	{
		return -1;
	}

	--DestSize;
	while (*Format)
	{
		if (*Format == LITERAL(WIDECHAR, '%'))
		{
			if (*(Format + 1) == LITERAL(WIDECHAR, '%'))
			{
				if (DestSize == 0)
				{
					*Dest = 0;
					return -1;
				}
				*Dest++ = *Format;
				Format += 2;
				DestSize--;
				continue;
			}

			FFormatInfo Info;
			Format += GetFormattingInfo(Format, Info);

			bool bLengthExceeded = false;
			FormatArgument(Info, ArgPtr, [&Dest, &DestSize, &bLengthExceeded](const auto* FormattedArg, int32 Length)
			{
				if (FormattedArg && Length > 0)
				{
					if (Length > DestSize)
					{
						Length = DestSize;
						bLengthExceeded = true;
					}

					if (sizeof(*FormattedArg) == sizeof(WIDECHAR))
					{
						FMemory::Memcpy(Dest, FormattedArg, Length * sizeof(WIDECHAR));
					}
					else
					{
						for (int32 Index = 0; Index != Length; ++Index)
						{
							Dest[Index] = FormattedArg[Index];
						}
					}

					Dest += Length;
					DestSize -= Length;
				}
			});
			if (bLengthExceeded)
			{
				*Dest = 0;
				return -1;
			}
		}
		else
		{
			if (DestSize == 0)
			{
				*Dest = 0;
				return -1;
			}
			*Dest++ = *Format++;
			DestSize--;
		}
	}

	*Dest = 0;

	return Dest - DestStart;
}

#endif // !PLATFORM_USE_SYSTEM_VSWPRINTF
