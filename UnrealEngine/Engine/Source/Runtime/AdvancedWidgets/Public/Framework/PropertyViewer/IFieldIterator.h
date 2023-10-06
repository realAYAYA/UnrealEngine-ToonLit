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
class IFieldIterator
{
public:
	virtual TArray<FFieldVariant> GetFields(const UStruct*) const = 0;
	virtual ~IFieldIterator() = default;
};

class FFieldIterator_BlueprintVisible : public IFieldIterator
{
	ADVANCEDWIDGETS_API virtual TArray<FFieldVariant> GetFields(const UStruct*) const override;
};

} //namespace
