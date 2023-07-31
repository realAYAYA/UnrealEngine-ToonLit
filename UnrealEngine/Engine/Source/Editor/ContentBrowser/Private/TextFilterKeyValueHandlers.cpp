// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextFilterKeyValueHandlers.h"

#include "TextFilterKeyValueHandler.h"
#include "UObject/Class.h"

class FTextFilterString;
struct FContentBrowserItem;

bool UTextFilterKeyValueHandlers::HandleTextFilterKeyValue(const FContentBrowserItem& InContentBrowserItem, const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode, bool& bOutIsMatch)
{
	for (const FTextFilterKeyValueHandlerEntry& Handler : GetDefault<UTextFilterKeyValueHandlers>()->TextFilterKeyValueHandlers)
	{
		if (InKey.IsEqual(Handler.Key) || Handler.Key.IsNone())
		{
			if (UClass* TextFilterKeyValueHandlerClass = Handler.HandlerClass.LoadSynchronous())
			{
				return GetDefault<UTextFilterKeyValueHandler>(TextFilterKeyValueHandlerClass)->HandleTextFilterKeyValue(InContentBrowserItem, InKey, InValue, InComparisonOperation, InTextComparisonMode, bOutIsMatch);
			}
		}
	}

	return false;
}