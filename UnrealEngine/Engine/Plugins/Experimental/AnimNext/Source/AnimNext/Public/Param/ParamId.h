// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimNextConfig;
struct FRigVMDispatch_SetLayerParameter;
struct FRigVMDispatch_GetLayerParameter;
struct FRigVMDispatch_GetParameter;

namespace UE::AnimNext
{
	struct FParamDefinition;
	class FModule;
	struct FRemappedLayer;
}

namespace UE::AnimNext::Tests
{
	class FParamStackTest;
}

namespace UE::AnimNext
{

// Global identifier used to avoid re-hashing parameter names
struct ANIMNEXT_API FParamId
{
	friend struct FParamStack;
	friend class Tests::FParamStackTest;
	friend struct ::FRigVMDispatch_SetLayerParameter;
	friend struct ::FRigVMDispatch_GetLayerParameter;
	friend struct ::FRigVMDispatch_GetParameter;
	friend class ::UAnimNextConfig;
	friend struct FParamDefinition;
	friend class FModule;
	friend struct FRemappedLayer;

	FParamId() = default;
	FParamId(const FParamId& InName) = default;
	FParamId& operator=(const FParamId& InName) = default;

	// Make a parameter ID from an FName, generating the hash
	explicit FParamId(FName InName)
		: Name(InName)
		, Hash(GetTypeHash(InName))
	{
	}

	// Make a parameter ID from a name and hash
	explicit FParamId(FName InName, uint32 InHash)
		: Name(InName)
		, Hash(InHash)
	{
		checkSlow(Hash == GetTypeHash(Name));
	}

	// Get the name of this param
	FName GetName() const
	{
		return Name;
	}

	// Get the hash of this param
	uint32 GetHash() const
	{
		return Hash;
	}

	// Check if this ID represents a valid parameter
	bool IsValid() const
	{
		return Hash != 0;
	}

private:
	// Parameter name
	FName Name;

	// Name hash
	uint32 Hash = 0;
};

} // end namespace UE::AnimNext
