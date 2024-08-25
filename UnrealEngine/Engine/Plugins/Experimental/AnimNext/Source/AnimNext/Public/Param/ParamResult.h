// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{

// Results for param access
enum class EParamResult : uint32
{
	Success = 0,

	// Error flags in low bits

	// Requested param is not in scope
	NotInScope = 0x00000001,

	// The requested parameter has an incompatible type
	TypeError = 0x00000002,

	// The requested parameter has an incompatible mutability (parameter is immutable but a mutable request was made of it)
	MutabilityError = 0x00000004,

	ErrorFlags = NotInScope | TypeError | MutabilityError,

	// Info flags in high bits

	// The type is compatible according to the supplied requirements
	TypeCompatible = 0x80000000,

	InfoFlags = TypeCompatible,
};

ENUM_CLASS_FLAGS(EParamResult);

struct FParamResult
{
	FParamResult() = default;

	// Implicit constructor
	FParamResult(EParamResult InResult)
		: Result(InResult)
	{}

	bool IsSuccessful() const
	{
		return !EnumHasAnyFlags(Result, EParamResult::ErrorFlags);
	}

	bool IsInScope() const
	{
		return !EnumHasAnyFlags(Result, EParamResult::NotInScope);
	}

	bool IsOfIncompatibleType() const
	{
		return EnumHasAnyFlags(Result, EParamResult::TypeError);
	}

	bool IsOfCompatibleType() const
	{
		return EnumHasAnyFlags(Result, EParamResult::TypeCompatible);
	}

	bool IsOfCompatibleMutability() const
	{
		return !EnumHasAnyFlags(Result, EParamResult::MutabilityError);
	}

	EParamResult Result = EParamResult::Success;
};

}
