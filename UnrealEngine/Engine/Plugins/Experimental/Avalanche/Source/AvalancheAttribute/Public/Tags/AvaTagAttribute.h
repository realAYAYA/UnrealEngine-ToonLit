// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagAttributeBase.h"
#include "AvaTagHandle.h"
#include "AvaTagAttribute.generated.h"

/** Attribute that holds a tag handle, for single tag referencing */
UCLASS(MinimalAPI, DisplayName="Tag Attribute")
class UAvaTagAttribute : public UAvaTagAttributeBase
{
	GENERATED_BODY()

public:
	//~ Begin UAvaAttribute
	AVALANCHEATTRIBUTE_API virtual FText GetDisplayName() const override;
	//~ End UAvaAttribute

	//~ Begin UAvaTagAttributeBase
	AVALANCHEATTRIBUTE_API virtual bool ContainsTag(const FAvaTagHandle& InTagHandle) const override;
	//~ End UAvaTagAttributeBase

	UPROPERTY(EditAnywhere, Category="Attributes")
	FAvaTagHandle Tag;
};
