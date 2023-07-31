// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


DECLARE_LOG_CATEGORY_CLASS(LogDMXProtocol, Log, All);

#define UE_LOG_DMXPROTOCOL(Verbosity, Format, ...) \
{ \
	UE_LOG(LogDMXProtocol, Verbosity, TEXT("DMX: %s"), *FString::Printf(Format, ##__VA_ARGS__)); \
}

#define UE_CLOG_DMXPROTOCOL(Conditional, Verbosity, Format, ...) \
{ \
	UE_CLOG(Conditional, LogDMXProtocol, Verbosity, TEXT("DMX: %s"), *FString::Printf(Format, ##__VA_ARGS__)); \
}
