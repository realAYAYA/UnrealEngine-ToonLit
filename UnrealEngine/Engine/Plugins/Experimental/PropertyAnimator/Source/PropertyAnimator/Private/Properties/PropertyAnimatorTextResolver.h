// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreData.h"
#include "Properties/PropertyAnimatorCoreResolver.h"
#include "PropertyAnimatorTextResolver.generated.h"

/**
 * Text characters properties resolver
 * Since each character in text are transient and regenerated on change
 * We need to have a resolvable property that will be resolved to each characters in the text we needed
 * We manipulate that resolvable property that underneath means we manipulate all text characters properties
 */
UCLASS(Transient)
class UPropertyAnimatorTextResolver : public UPropertyAnimatorCoreResolver
{
	GENERATED_BODY()

public:
	UPropertyAnimatorTextResolver()
		: UPropertyAnimatorCoreResolver(TEXT("TextCharacters"), /** IsRange */ true)
	{}

	//~ Begin UPropertyAnimatorCoreResolver
	virtual void GetResolvableProperties(const FPropertyAnimatorCoreData& InParentProperty, TSet<FPropertyAnimatorCoreData>& OutProperties) override;
	virtual void ResolveProperties(const FPropertyAnimatorCoreData& InTemplateProperty, TArray<FPropertyAnimatorCoreData>& OutProperties) override;
	//~ End UPropertyAnimatorCoreResolver
};