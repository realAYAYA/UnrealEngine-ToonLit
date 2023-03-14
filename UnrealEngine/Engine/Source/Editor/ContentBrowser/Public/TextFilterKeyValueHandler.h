// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserItem.h"

#include "TextFilterKeyValueHandler.generated.h"

UCLASS(abstract, transient, MinimalAPI)
class UTextFilterKeyValueHandler : public UObject
{
	GENERATED_BODY()
public:
	virtual bool HandleTextFilterKeyValue(const FContentBrowserItem& InContentBrowserItem, const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode, bool& bOutIsMatch) const { return false; }
};