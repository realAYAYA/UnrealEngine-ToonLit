// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/Types.h"
#include "CADKernel/UI/Progress.h"

namespace UE::CADKernel
{
extern const char* VerboseLevelConstNames[];
extern const char* VerboseConstDescHelp[];


class CADKERNEL_API FMessage
{
	friend class FProgressManager;

private:
	static int32 NumberOfIndentation;
	static int32 OldPercent;

	static void VPrintf(EVerboseLevel Level, const TCHAR* Text, ...);

	static void VReportPrintF(FString Header, const TCHAR* Text, ...);

public:

	template <typename FmtType, typename... Types>
	static void Printf(EVerboseLevel Level, const FmtType& Text, Types... Args)
	{
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FString::Printf");
		VPrintf(Level, Text, Args...);
	}

	template <typename FmtType, typename... Types>
	static void Error(const FmtType& Text, Types... Args)
	{
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FString::Printf");
		FString CompleteText = TEXT("ERROR: ");
		CompleteText += Text;
		VPrintf(Log, *CompleteText, Args...);
	}

	template <typename FmtType, typename... Types>
	static void Warning(const FmtType& Text, Types... Args)
	{
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FString::Printf");
		FString CompleteText = TEXT("WARNING: ");
		CompleteText += Text;
		VPrintf(Log, *CompleteText, Args...);
	}

	template <typename FmtType, typename... Types>
	static void FillReportFile(const FmtType Header, const FmtType Text, Types... Args)
	{
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FString::Printf");
		VReportPrintF(Header, Text, Args...);
	}

	static void FillReportFile(const TCHAR* Header, FDuration Duration)
	{
#ifdef CADKERNEL_DEV
		int64 Time = FChrono::ConvertInto<std::chrono::microseconds>(Duration);
#else
		int64 Time = Duration;
#endif
		VReportPrintF(Header, TEXT("%lld"), Time);
	}

	static void FillReportFile(const TCHAR* Header, double Count)
	{
		VReportPrintF(Header, TEXT("%f"), Count);
	}

	static void FillReportFile(const TCHAR* Header, int32 Count)
	{
		VReportPrintF(Header, TEXT("%d"), Count);
	}

	static void Indent(int32 InNumberOfIndent = 1)
	{
		NumberOfIndentation += InNumberOfIndent;
	}

	static void Deindent(int32 InNumberOfIndent = 1)
	{
		NumberOfIndentation -= InNumberOfIndent;
		if (NumberOfIndentation < 0)
		{
			NumberOfIndentation = 0;
		}
	}

};
} // namespace UE::CADKernel

#define ERROR_FUNCTION_CALL_NOT_EXPECTED \
{ \
	FMessage::Printf(Log, TEXT("CALL of \" %s \" NOT EXPECTED at line %d of the file %s\n"), __func__, __LINE__, __FILE__); \
}

#define ERROR_NOT_EXPECTED \
{ \
	FMessage::Printf(Log, TEXT("Error not expected in \" %S \" at line %d of the file %S\n"), __func__, __LINE__, __FILE__); \
}

#define NOT_IMPLEMENTED \
{ \
	FMessage::Printf(Log, TEXT("The function \" %s \" at line %d of the file %s is not implemented"), __func__, __LINE__, __FILE__); \
}
