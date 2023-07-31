// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;

class IDetailKeyframeHandler
{
public:
	virtual ~IDetailKeyframeHandler(){}

	virtual bool IsPropertyKeyable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const = 0;

	virtual bool IsPropertyKeyingEnabled() const = 0;

	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) = 0;

	virtual bool IsPropertyAnimated(const IPropertyHandle& PropertyHandle, UObject* ParentObject) const = 0;

};
