// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if TEXTURESHARECORE_SDK==1

/**
 * SDK log helpers
 */

#ifdef UE_LOG
// SDK override UE log system
#undef UE_LOG
#endif

#if TEXTURESHARECORE_DEBUGLOG

//////////////////////////////////////////////////////////////////////////////////////////////
// Enable extra log in VS output window
namespace LogHelpers
{
	static void WriteLine(const FString& In);
	static void WriteLineOnce(const FString& In);
};

#define UE_LOG(CategoryName, Verbosity, Format, ...)\
	LogHelpers::WriteLine(FString::Printf(Format, ##__VA_ARGS__));

#define UE_TS_LOG(CategoryName, Verbosity, Format, ...)\
	LogHelpers::WriteLineOnce(FString::Printf(Format, ##__VA_ARGS__));

#else
// Disable all log
#define UE_LOG(CategoryName, Verbosity, Format, ...)
#define UE_TS_LOG(CategoryName, Verbosity, Format, ...)
#endif

#endif

#if TEXTURESHARECORE_SDK==0

/**
 * UE log helpers
 */

// Enable extra log
#if TEXTURESHARECORE_DEBUGLOG

#define UE_TS_LOG(CategoryName, Verbosity, Format, ...)\
	UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__)

#else
// Disable extra log
#define UE_TS_LOG(CategoryName, Verbosity, Format, ...)
#endif


#endif
