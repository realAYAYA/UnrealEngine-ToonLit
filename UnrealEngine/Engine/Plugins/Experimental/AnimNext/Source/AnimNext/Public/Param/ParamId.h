// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FRigVMDispatch_SetParameter;
struct FRigVMDispatch_GetParameter;

namespace UE::AnimNext::Tests
{
	class FParamStackTest;
}

namespace UE::AnimNext
{

// Global identifier used to index into dense parameter arrays
struct ANIMNEXT_API FParamId
{
	friend struct FParamStack;
	friend class Tests::FParamStackTest;
	friend struct ::FRigVMDispatch_SetParameter;
	friend struct ::FRigVMDispatch_GetParameter;

	static constexpr uint32 InvalidIndex = MAX_uint32;

	FParamId() = delete;
	FParamId(const FParamId& InName) = default;
	FParamId& operator=(const FParamId& InName) = default;


	// Make a parameter ID from an FName
	explicit FParamId(FName InName);

	// Get the index of this param
	uint32 ToInt() const
	{
		return ParameterIndex;
	}

	// Get the name that this parameter was created from
	FName ToName() const;

private:
	// Make a parameter ID from an index (for internal use)
	explicit FParamId(uint32 InParameterIndex)
		: ParameterIndex(InParameterIndex)
	{
	}

	// Get the maximum parameter ID that can exist at present
	// Note that this can change via concurrent modifications when new parameters are created by 
	// different threads
	static FParamId GetMaxParamId();

#if WITH_DEV_AUTOMATION_TESTS
	// Used to isolate global param IDs from automated tests
	static void BeginTestSandbox();
	static void EndTestSandbox();
#endif

private:
	// Stable index
	uint32 ParameterIndex = InvalidIndex;
};

} // end namespace UE::AnimNext
