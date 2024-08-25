// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FUniversalObjectLocator;

class FString;
class UObject;
class IPropertyHandle;

namespace UE::UniversalObjectLocator
{

class IUniversalObjectLocatorCustomization
{
public:
	virtual ~IUniversalObjectLocatorCustomization() = default;

	virtual UObject* GetContext() const = 0;
	virtual UObject* GetSingleObject() const = 0;
	virtual FString GetPathToObject() const = 0;
	virtual void SetValue(FUniversalObjectLocator&& InNewValue) const = 0;

	virtual TSharedPtr<IPropertyHandle> GetProperty() const = 0;
};


} // namespace UE::UniversalObjectLocator