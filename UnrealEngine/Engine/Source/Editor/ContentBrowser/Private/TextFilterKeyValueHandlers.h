// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreFwd.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "TextFilterKeyValueHandlers.generated.h"

class FTextFilterString;
class UTextFilterKeyValueHandler;
struct FContentBrowserItem;

USTRUCT()
struct FTextFilterKeyValueHandlerEntry
{
	GENERATED_BODY()

	UPROPERTY(config)
	FName Key;

	UPROPERTY(config)
	TSoftClassPtr<UTextFilterKeyValueHandler> HandlerClass;
};

UCLASS(transient, config = Editor)
class UTextFilterKeyValueHandlers : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(config)
	TArray<FTextFilterKeyValueHandlerEntry> TextFilterKeyValueHandlers;

	static bool HandleTextFilterKeyValue(const FContentBrowserItem& InContentBrowserItem, const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode, bool& bOutIsMatch);
};