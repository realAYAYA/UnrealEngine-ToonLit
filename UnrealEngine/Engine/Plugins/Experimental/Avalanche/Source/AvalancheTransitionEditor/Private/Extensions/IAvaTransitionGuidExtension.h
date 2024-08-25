// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"

struct FGuid;

/** Extension for a View Model that can be handles an underlying Guid */
class IAvaTransitionGuidExtension
{
public:
	UE_AVA_TYPE(IAvaTransitionGuidExtension)

	virtual const FGuid& GetGuid() const = 0;
};
