// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIFWidgetId.generated.h"

class UUIFrameworkWidget;

/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkWidgetId
{
	GENERATED_BODY()

	FUIFrameworkWidgetId() = default;
	explicit FUIFrameworkWidgetId(UUIFrameworkWidget* InWidget);

	static FUIFrameworkWidgetId MakeNew();
	static FUIFrameworkWidgetId MakeRoot();

private:
	FUIFrameworkWidgetId(int64 InKey)
		: Key(InKey)
	{}

public:
	int64 GetKey() const
	{
		return Key;
	}

	bool IsRoot() const
	{
		return Key == 0;
	}
	
	bool IsValid() const
	{
		return Key != INDEX_NONE;
	}

	bool operator==(const FUIFrameworkWidgetId& Other) const
	{
		return Key == Other.Key;
	}

	bool operator!=(const FUIFrameworkWidgetId& Other) const
	{
		return Key != Other.Key;
	}
	
	FORCEINLINE friend uint32 GetTypeHash(const FUIFrameworkWidgetId& Value)
	{
		return GetTypeHash(Value.Key);
	}

private:
	UPROPERTY()
	int64 Key = INDEX_NONE;

	static int64 KeyGenerator;
};
