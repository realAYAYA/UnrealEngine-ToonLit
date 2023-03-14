// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "UObject/Field.h"

class FFieldVariant;
class FProperty;
class UFunction;
class UStruct;

namespace UE::PropertyViewer
{

/** */
class ADVANCEDWIDGETS_API IFieldIterator
{
public:
	virtual TArray<FFieldVariant> GetFields(const UStruct*) const = 0;
	virtual ~IFieldIterator() = default;
};

class ADVANCEDWIDGETS_API FFieldIterator_BlueprintVisible : public IFieldIterator
{
	virtual TArray<FFieldVariant> GetFields(const UStruct*) const override;
};

} //namespace
