// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreData.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "PropertyAnimatorCoreResolver.generated.h"

/**
 * Base class to find properties hidden or not reachable,
 * allows to discover resolvable properties for specific actors/components/objects
 * that we cannot reach or are transient, will be resolved when needed
 * Resolvers should remain transient and stateless
 */
UCLASS(MinimalAPI, Abstract, Transient)
class UPropertyAnimatorCoreResolver : public UObject
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreResolver()
		: UPropertyAnimatorCoreResolver(NAME_None, false)
	{}

	UPropertyAnimatorCoreResolver(FName InResolverName, bool bInIsRange)
		: ResolverName(InResolverName)
		, bIsRange(bInIsRange)
	{}

	/** Get properties found inside parent property */
	virtual void GetResolvableProperties(const FPropertyAnimatorCoreData& InParentProperty, TSet<FPropertyAnimatorCoreData>& OutProperties) {}

	/** Called when we actually need the underlying properties */
	virtual void ResolveProperties(const FPropertyAnimatorCoreData& InTemplateProperty, TArray<FPropertyAnimatorCoreData>& OutProperties) {}

	FName GetResolverName() const
	{
		return ResolverName;
	}

	/** Whether this property when resolved should be treated as a range */
	bool IsRange() const
	{
		return bIsRange;
	}

private:
	UPROPERTY()
	FName ResolverName = NAME_None;

	UPROPERTY()
	bool bIsRange = false;
};