// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectProxyFactory.h"

namespace UE::AnimNext
{
	struct FClassProxy;
}

namespace UE::AnimNext
{

// Cached information about how to access an object's data and map it to AnimNext parameters 
struct FObjectAccessor
{
	FObjectAccessor(FName InAccessorName, FObjectAccessorFunction&& InFunction, TSharedRef<FClassProxy> InClassProxy);

	// The name of the accessor
	FName AccessorName;

	// Function called to access the object given a context
	FObjectAccessorFunction Function;

	// Class proxy defining the 'layout' of the object
	TSharedRef<FClassProxy> ClassProxy;

	// All parameters mapped from class -> accessor's 'namespace'
	TArray<FName> RemappedParameters;

	// Index lookup for accessor's 'namespace' parameters
	TMap<FName, int32> RemappedParametersMap;
};

}