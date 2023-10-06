// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIFParentWidget.generated.h"

class UObject;
class UUIFrameworkPlayerComponent;
class UUIFrameworkWidget;

/**
 *
 */
USTRUCT(BlueprintType)
struct UIFRAMEWORK_API FUIFrameworkParentWidget
{
	GENERATED_BODY()

	FUIFrameworkParentWidget() = default;

	FUIFrameworkParentWidget(UUIFrameworkWidget* InWidget);
	FUIFrameworkParentWidget(UUIFrameworkPlayerComponent* InPlayer);

public:
	bool IsParentValid() const
	{
		return Parent != nullptr;
	}

	bool IsWidget() const
	{
		check(IsParentValid());
		return bIsParentAWidget;
	}

	bool IsPlayerComponent() const
	{
		check(IsParentValid());
		return !bIsParentAWidget;
	}

	UUIFrameworkWidget* AsWidget() const;
	UUIFrameworkPlayerComponent* AsPlayerComponent() const;

	bool operator== (const UUIFrameworkWidget* Other) const;

	bool operator!= (const UUIFrameworkWidget* Other) const
	{
		return !((*this) == Other);
	}

	bool operator== (const FUIFrameworkParentWidget& Other) const
	{
		return Other.Parent == Parent;
	}

	bool operator!= (const FUIFrameworkParentWidget& Other) const
	{
		return Other.Parent != Parent;
	}

private:
	UPROPERTY()
	TObjectPtr<UObject> Parent = nullptr;

	bool bIsParentAWidget = false;
};
