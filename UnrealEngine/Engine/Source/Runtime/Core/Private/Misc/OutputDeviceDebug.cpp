// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceDebug.h"

#include "Containers/StringFwd.h"
#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/StringBuilder.h"

void FOutputDeviceDebug::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category, const double Time)
{
	if (Verbosity != ELogVerbosity::SetColor)
	{
		TStringBuilder<512> Line;
		FOutputDeviceHelper::AppendFormatLogLine(Line, Verbosity, Category, Data, GPrintLogTimes, Time);
		Line.Append(LINE_TERMINATOR);
		FPlatformMisc::LowLevelOutputDebugString(*Line);
	}
}

void FOutputDeviceDebug::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category)
{
	Serialize(Data, Verbosity, Category, -1.0);
}

bool FOutputDeviceDebug::CanBeUsedOnMultipleThreads() const
{
	return FPlatformMisc::IsLocalPrintThreadSafe();
}
