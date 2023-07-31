// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusExecutionDomainProvider.generated.h"

/** A struct to hold onto a single-level domain for controlling a kernel's execution domain. 
  * The reason it's in a struct is so that we can apply a property panel customization for it
  * to make it easier to select from a pre-defined list of execution domains.
*/
USTRUCT()
struct OPTIMUSCORE_API FOptimusExecutionDomain
{
	GENERATED_BODY()

	FOptimusExecutionDomain() = default;
	FOptimusExecutionDomain(FName InExecutionDomainName) : Name(InExecutionDomainName) {}

	// The name of the execution domain that this kernel operates on.
	UPROPERTY(EditAnywhere, Category = Domain)
	FName Name;
};

UINTERFACE()
class OPTIMUSCORE_API UOptimusExecutionDomainProvider :
	public UInterface
{
	GENERATED_BODY()
};


/** An interface that should be implemented by any class that has an FOptimusExecutionDomain member.
 *  This is used by the property detail specialization code to provide a list of possible domains.
 */
class IOptimusExecutionDomainProvider
{
	GENERATED_BODY()

public:
	/**
	 * Returns a list of execution domains that the implementer of this interface provides.
	 */
	virtual TArray<FName> GetExecutionDomains() const = 0;
};
