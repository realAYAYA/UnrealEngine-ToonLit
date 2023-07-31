// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AutomationDriverTypeDefs.h"

class IElementLocator;

class FSlateWidgetLocatorByPathFactory
{
public:

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FString& Path);

	static TSharedRef<IElementLocator, ESPMode::ThreadSafe> Create(
		const FDriverElementPtr& Root,
		const FString& Path);
};
