// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextFilterValueHandlers.h"

#include "TextFilterValueHandler.h"
#include "UObject/Class.h"

class FTextFilterString;
struct FContentBrowserItem;

bool UTextFilterValueHandlers::HandleTextFilterValue(const FContentBrowserItem& InContentBrowserItem, const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode, bool& bOutIsMatch)
{
	for (const TSoftClassPtr<UTextFilterValueHandler>& HandlerClass : GetDefault<UTextFilterValueHandlers>()->TextFilterValueHandlers)
	{
		if (UClass* TextFilterValueHandlerClass = HandlerClass.LoadSynchronous())
		{
			if (GetDefault<UTextFilterValueHandler>(TextFilterValueHandlerClass)->HandleTextFilterValue(InContentBrowserItem, InValue, InTextComparisonMode, bOutIsMatch))
			{
				return true;
			}
		}
	}

	return false;
}