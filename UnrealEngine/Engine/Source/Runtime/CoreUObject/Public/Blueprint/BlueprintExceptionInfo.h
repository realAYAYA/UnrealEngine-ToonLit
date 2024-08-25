// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"

// Kinds of Blueprint exceptions
namespace EBlueprintExceptionType
{
	enum Type
	{
		Breakpoint,
		Tracepoint,
		WireTracepoint,
		AccessViolation,
		InfiniteLoop,
		NonFatalError,
		FatalError,
		AbortExecution,
	};
}

// Information about a blueprint exception
struct FBlueprintExceptionInfo
{
public:
	FBlueprintExceptionInfo(EBlueprintExceptionType::Type InEventType)
		: EventType(InEventType)
	{
	}

	FBlueprintExceptionInfo(EBlueprintExceptionType::Type InEventType, const FText& InDescription)
		: EventType(InEventType)
		, Description(InDescription)
	{
	}

	EBlueprintExceptionType::Type GetType() const
	{
		return EventType;
	}

	const FText& GetDescription() const
	{
		return Description;
	}
protected:
	EBlueprintExceptionType::Type EventType;
	FText Description;
};
