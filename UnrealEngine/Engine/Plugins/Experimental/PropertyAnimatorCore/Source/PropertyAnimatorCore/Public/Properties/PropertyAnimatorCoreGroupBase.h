// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreData.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "PropertyAnimatorCoreGroupBase.generated.h"

class UPropertyAnimatorCoreBase;
class UPropertyAnimatorCoreContext;

/** Group that controls animated properties in an animator */
UCLASS(MinimalAPI, BlueprintType, Abstract, EditInlineNew)
class UPropertyAnimatorCoreGroupBase : public UObject
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreGroupBase()
		: UPropertyAnimatorCoreGroupBase(NAME_None)
	{}

	UPropertyAnimatorCoreGroupBase(FName InName)
		: GroupName(InName)
	{}

	/** Manages the properties owned by this group */
	virtual void ManageProperties(const UPropertyAnimatorCoreContext* InContext, TArray<FPropertyAnimatorCoreData>& InOutProperties) {}

	/** Checks if this group supports the context */
	virtual bool IsPropertySupported(const UPropertyAnimatorCoreContext* InContext) const
	{
		return false;
	}

private:
	UPROPERTY()
	FName GroupName = NAME_None;
};