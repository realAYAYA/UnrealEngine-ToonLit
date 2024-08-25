// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreData.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "PropertyAnimatorCoreEditorMenuContext.generated.h"

/** Context passed in UToolMenu when generating entries with selected items */
UCLASS()
class UPropertyAnimatorCoreEditorMenuContext : public UObject
{
	GENERATED_BODY()

public:
	const FPropertyAnimatorCoreData& GetPropertyData() const
	{
		return PropertyData;
	}

	void SetPropertyData(const FPropertyAnimatorCoreData& InPropertyData)
	{
		PropertyData = InPropertyData;
	}

protected:
	/** The item this menu should apply to */
	FPropertyAnimatorCoreData PropertyData;
};