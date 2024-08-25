// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaAttribute.h"
#include "AvaTagAttributeBase.generated.h"

struct FAvaTagHandle;

/** Base implementation of an attribute that represent one or more tags in some form */
UCLASS(MinimalAPI, Abstract)
class UAvaTagAttributeBase : public UAvaAttribute
{
	GENERATED_BODY()

public:
	virtual bool ContainsTag(const FAvaTagHandle& InTagHandle) const
	{
		return false;
	}
};
