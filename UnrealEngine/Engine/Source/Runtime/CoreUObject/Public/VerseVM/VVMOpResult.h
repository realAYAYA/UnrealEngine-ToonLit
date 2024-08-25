// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "VVMValue.h"

// Helper macros for converting/marshaling VM arguments
#define V_REQUIRE_CONCRETE(Value)                        \
	if ((Value).IsPlaceholder())                         \
	{                                                    \
		return {Verse::FOpResult::ShouldSuspend, Value}; \
	}
#define V_FAIL_IF(Condition)               \
	if (Condition)                         \
	{                                      \
		return {Verse::FOpResult::Failed}; \
	}
#define V_RETURN(Value)                 \
	return                              \
	{                                   \
		Verse::FOpResult::Normal, Value \
	}
#define V_RUNTIME_ERROR(Context, Message)                                         \
	return                                                                        \
	{                                                                             \
		Verse::FOpResult::RuntimeError, Verse::VUTF8String::New(Context, Message) \
	}
#define V_RUNTIME_ERROR_IF(Condition, Context, Message) \
	if (Condition)                                      \
	{                                                   \
		V_RUNTIME_ERROR(Context, Message);              \
	}

namespace Verse
{

// Represents the result of a single VM operation
struct FOpResult
{
	enum EKind
	{
		Normal,        // All went well, Value is the result
		Failed,        // Something went wrong, Value is undefined
		ShouldSuspend, // A placeholder was encountered among the arguments and Value is this placeholder
		RuntimeError   // A runtime error occurred, and Value holds a VUTF8String with an error message
	};

	FOpResult(EKind Kind, VValue Value = VValue())
		: Kind(Kind)
		, Value(Value)
	{
	}

	EKind Kind;
	VValue Value;
};

} // namespace Verse
#endif // WITH_VERSE_VM
