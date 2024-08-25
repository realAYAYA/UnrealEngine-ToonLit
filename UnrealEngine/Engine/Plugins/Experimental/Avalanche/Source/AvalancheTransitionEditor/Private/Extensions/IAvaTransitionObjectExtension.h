// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"

class UObject;
struct FPropertyChangedEvent;

/** Extension for a View Model that represents a UObject */
class IAvaTransitionObjectExtension
{
public:
	UE_AVA_TYPE(IAvaTransitionObjectExtension)

	virtual UObject* GetObject() const = 0;

	virtual void OnPropertiesChanged(const FPropertyChangedEvent& InPropertyChangedEvent) = 0;
};
