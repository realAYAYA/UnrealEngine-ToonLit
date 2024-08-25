// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EnumClassFlags.h"
#include "TraceServices/Model/Callstack.h"
#include "TraceServices/Model/Modules.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EStackFrameFormatFlags : uint8
{
	Module    = 1 << 0, // include module name
	Symbol    = 1 << 1, // include symbol name
	File      = 1 << 2, // include source file name
	Line      = 1 << 3, // include source line number
	Multiline = 1 << 4, // allow formatting on multiple lines

	ModuleAndSymbol         = Module + Symbol,
	ModuleSymbolFileAndLine = Module + Symbol + File + Line,
	FileAndLine             = File + Line,
};
ENUM_CLASS_FLAGS(EStackFrameFormatFlags);

////////////////////////////////////////////////////////////////////////////////////////////////////

inline const TCHAR* GetCallstackNotAvailableString()
{
	return TEXT("Unknown Callstack");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

inline const TCHAR* GetEmptyCallstackString()
{
	return TEXT("Empty Callstack");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

inline void FormatStackFrame(const TraceServices::FStackFrame& Frame, FStringBuilderBase& OutString, EStackFrameFormatFlags FormatFlags)
{
	using namespace TraceServices;
	const ESymbolQueryResult Result = Frame.Symbol->GetResult();
	switch (Result)
	{
		case ESymbolQueryResult::OK:
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Module))
			{
				OutString.Append(Frame.Symbol->Module);
				if (FormatFlags != EStackFrameFormatFlags::Module &&
					FormatFlags != (EStackFrameFormatFlags::Module | EStackFrameFormatFlags::Multiline))
				{
					OutString.AppendChar(EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Multiline) ? TEXT('\n') : TEXT('!'));
				}
			}
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Symbol))
			{
				OutString.Append(Frame.Symbol->Name);
			}
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Symbol) &&
				EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::File | EStackFrameFormatFlags::Line))
			{
				OutString.AppendChar(EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Multiline) ? TEXT('\n') : TEXT(' '));
			}
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::File))
			{
				OutString.Append(Frame.Symbol->File);
			}
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Line))
			{
				OutString.Appendf(TEXT("(%d)"), Frame.Symbol->Line);
			}
			break;

		case ESymbolQueryResult::Mismatch:
		case ESymbolQueryResult::NotFound:
		case ESymbolQueryResult::NotLoaded:
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Module))
			{
				OutString.Append(Frame.Symbol->Module);
				if (FormatFlags != EStackFrameFormatFlags::Module &&
					FormatFlags != (EStackFrameFormatFlags::Module | EStackFrameFormatFlags::Multiline))
				{
					OutString.AppendChar(EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Multiline) ? TEXT('\n') : TEXT('!'));
				}
			}
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Symbol))
			{
				if (Frame.Addr == 0)
				{
					OutString.Append(TEXT("0x00000000"));
				}
				else
				{
					OutString.Appendf(TEXT("0x%llX"), Frame.Addr);
				}
			}
			if (EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::ModuleAndSymbol))
			{
				OutString.AppendChar(EnumHasAnyFlags(FormatFlags, EStackFrameFormatFlags::Multiline) ? TEXT('\n') : TEXT(' '));
			}
			OutString.Appendf(TEXT("(%s)"), QueryResultToString((Result)));
			break;

		case ESymbolQueryResult::Pending:
		default:
			OutString.Append(QueryResultToString(Result));
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
