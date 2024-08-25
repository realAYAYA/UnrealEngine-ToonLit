// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/UI/Message.h"

#include "CADKernel/Core/Chrono.h"
#include "CADKernel/Core/System.h"
#include "CADKernel/UI/Display.h"
#include "CADKernel/Utils/Util.h"

#include "Misc/VarArgs.h"

namespace UE::CADKernel
{
const char* VerboseLevelConstNames[] = {
	("NO_VERBOSE"),
	("SPY"),
	("LOG"),
	("DBG"),
	nullptr
};

const char* VerboseConstDescHelp[] = {
	": nothing is printed",
	": only commands and arguments will be showed",
	": SPY + messages",
	": debug mode, for developers only",
	nullptr
};

int32 FMessage::NumberOfIndentation = 0;
int32 FMessage::OldPercent = -1;

void FMessage::VPrintf(EVerboseLevel Level, const TCHAR* Text, ...)
{
	EVerboseLevel VerboseLevel = (EVerboseLevel)(int32)FSystem::Get().GetVerboseLevel();
	TSharedPtr<FArchive> LogFile = FSystem::Get().GetLogFile();
	EVerboseLevel LogLevel = FSystem::Get().GetLogLevel();
	TSharedPtr<FArchive> SpyFile = FSystem::Get().GetSpyFile();

	FString Indentation;
	TCHAR* Buffer = NULL;
	int32 Result = -1;
	if (VerboseLevel >= Level || (LogFile.IsValid() && Level <= EVerboseLevel::Debug) || (SpyFile.IsValid() && Level == EVerboseLevel::Spy))
	{
		int32 BufferSize = 1024;

		// do the usual VARARGS shenanigans
		while (Result == -1)
		{
			FMemory::Free(Buffer);
			Buffer = (TCHAR*)FMemory::Malloc(BufferSize * sizeof(TCHAR));
			GET_TYPED_VARARGS_RESULT(TCHAR, Buffer, BufferSize, BufferSize - 1, Text, Text, Result);
			BufferSize *= 2;
		};
		Buffer[Result] = 0;

		Indentation.Reserve(NumberOfIndentation * 3);
		for (int32 iIndent = 0; iIndent < NumberOfIndentation; iIndent++)
		{
			Indentation += TEXT(" - ");
		}
	}

	if (VerboseLevel >= Level)
	{
		FSystem::Get().GetConsole().Print(*Indentation, Level);
		FSystem::Get().GetConsole().Print(Buffer, Level);
	}

	if ((LogFile.IsValid() && Level <= LogLevel) || (SpyFile.IsValid() && Level <= Spy))
	{
		FTCHARToUTF8 UTF8Indentation(*Indentation, Indentation.Len());
		FTCHARToUTF8 UTF8String(Buffer, Result);
		if (LogFile.IsValid() && Level <= LogLevel)
		{
			LogFile->Serialize((UTF8CHAR*)UTF8Indentation.Get(), UTF8Indentation.Length() * sizeof(UTF8CHAR));
			LogFile->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR));
			LogFile->Flush();
		}

		if (SpyFile.IsValid() && Level <= Spy)
		{
			SpyFile->Serialize((UTF8CHAR*)UTF8Indentation.Get(), UTF8Indentation.Length() * sizeof(UTF8CHAR));
			SpyFile->Serialize((UTF8CHAR*)UTF8String.Get(), UTF8String.Length() * sizeof(UTF8CHAR));
			SpyFile->Flush();
		}
	}

	if (Buffer)
	{
		FMemory::Free(Buffer);
	}
}

void FMessage::VReportPrintF(FString Header, const TCHAR* Text, ...)
{
#ifdef CADKERNEL_DEV
	TSharedPtr<FArchive> ReportFile = FSystem::Get().GetReportFile();
	TSharedPtr<FArchive> ReportHeaderFile = FSystem::Get().GetReportHeaderFile();
	if (!ReportFile.IsValid())
	{
		return;
	}

	TCHAR* Buffer = NULL;
	int32 Result = -1;
	{
		int32 BufferSize = 1024;

		// do the usual VARARGS shenanigans
		while (Result == -1)
		{
			FMemory::Free(Buffer);
			Buffer = (TCHAR*)FMemory::Malloc(BufferSize * sizeof(TCHAR));
			GET_TYPED_VARARGS_RESULT(TCHAR, Buffer, BufferSize, BufferSize - 1, Text, Text, Result);
			BufferSize *= 2;
		};
		Buffer[Result] = 0;
	}

	//FSystem::Get().GetConsole().Print(*Header, Log);
	//FSystem::Get().GetConsole().Print(TEXT(": "), Log);
	//FSystem::Get().GetConsole().Print(Buffer, Log);
	//FSystem::Get().GetConsole().Print(TEXT("\n"), Log);

	FTCHARToUTF8 BufferUtf8(Buffer, Result);
	ReportFile->Serialize((UTF8CHAR*)BufferUtf8.Get(), BufferUtf8.Length() * sizeof(UTF8CHAR));
	ReportFile->Serialize(",", sizeof(UTF8CHAR));
	ReportFile->Flush();

	if (ReportHeaderFile.IsValid())
	{
		FTCHARToUTF8 HeaderUtf8(*Header, Header.Len());
		ReportHeaderFile->Serialize((UTF8CHAR*)HeaderUtf8.Get(), HeaderUtf8.Length() * sizeof(UTF8CHAR));
		ReportHeaderFile->Serialize(",", sizeof(UTF8CHAR));
		ReportHeaderFile->Flush();
	}

	if (Buffer)
	{
		FMemory::Free(Buffer);
	}
#endif
}

void FChrono::PrintClockElapse(EVerboseLevel Level, const TCHAR* Indent, const TCHAR* Process, FDuration Duration, ETimeUnit Unit)
{
#ifdef CADKERNEL_DEV
	if (FSystem::Get().GetVerboseLevel() < Level)
	{
		return;
	}

	TCHAR* SUnit = TEXT("seconds");
	long long Time = 0;
	switch (Unit)
	{
	case ETimeUnit::NanoSeconds:
		SUnit = TEXT("nanoseconds");
		Time = FChrono::ConvertInto<std::chrono::nanoseconds>(Duration);
		break;
	case ETimeUnit::MicroSeconds:
		SUnit = TEXT("microseconds");
		Time = FChrono::ConvertInto<std::chrono::microseconds>(Duration);
		break;
	case ETimeUnit::MilliSeconds:
		SUnit = TEXT("milliseconds");
		Time = FChrono::ConvertInto<std::chrono::milliseconds>(Duration);
		break;
	default:
		Time = FChrono::ConvertInto<std::chrono::seconds>(Duration);
	}
	FMessage::Printf(Level, TEXT("%sSpeed test (%s) -----> %lld %s\n"), Indent, Process, Time, SUnit);
#endif
}
}