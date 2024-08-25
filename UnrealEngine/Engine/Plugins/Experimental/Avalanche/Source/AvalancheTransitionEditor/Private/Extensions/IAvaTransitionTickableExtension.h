// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"

/** Extension for a View Model that supports ticking */
class IAvaTransitionTickableExtension
{
public:
	UE_AVA_TYPE(IAvaTransitionTickableExtension)

	virtual void Tick(float InDeltaTime) = 0;
};
