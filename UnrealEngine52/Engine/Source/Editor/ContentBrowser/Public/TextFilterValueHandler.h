// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserItem.h"

#include "TextFilterValueHandler.generated.h"

UCLASS(abstract, transient, MinimalAPI)
class UTextFilterValueHandler : public UObject
{
	GENERATED_BODY()
public:
	virtual bool HandleTextFilterValue(const FContentBrowserItem& InContentBrowserItem, const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode, bool& bOutIsMatch) const { return false; }
};