// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectPtr.h"
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

private:
	UPROPERTY()
	TObjectPtr<UObject> Parent = nullptr;

	bool bIsParentAWidget = false;
};
