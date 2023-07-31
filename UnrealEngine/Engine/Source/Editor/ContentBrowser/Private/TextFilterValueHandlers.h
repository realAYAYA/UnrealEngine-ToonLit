// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreFwd.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "TextFilterValueHandlers.generated.h"

class FTextFilterString;
class UTextFilterValueHandler;
struct FContentBrowserItem;

UCLASS(transient, config = Editor)
class UTextFilterValueHandlers : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(config)
	TArray<TSoftClassPtr<UTextFilterValueHandler>> TextFilterValueHandlers;

	static bool HandleTextFilterValue(const FContentBrowserItem& InContentBrowserItem, const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode, bool& bOutIsMatch);
};