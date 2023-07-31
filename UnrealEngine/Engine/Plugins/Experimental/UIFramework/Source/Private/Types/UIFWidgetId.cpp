// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/UIFWidgetId.h"
#include "UIFWidget.h"

/**
 *
 */
int64 FUIFrameworkWidgetId::KeyGenerator = 0;

FUIFrameworkWidgetId::FUIFrameworkWidgetId(UUIFrameworkWidget* InOwner)
	: Key(InOwner->GetWidgetId().Key)
{
}

FUIFrameworkWidgetId FUIFrameworkWidgetId::MakeNew()
{
	FUIFrameworkWidgetId Result{ ++KeyGenerator };
	bool bIsInvalidKey = Result.Key == INDEX_NONE || Result.Key == 0;
	ensure(!bIsInvalidKey);
	if (bIsInvalidKey)
	{
		KeyGenerator = 1;
		Result.Key = KeyGenerator;
	}
	return Result;
}

FUIFrameworkWidgetId FUIFrameworkWidgetId::MakeRoot()
{
	return FUIFrameworkWidgetId{0LL};
}
