// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Templates/UniquePtr.h"

class FGameplayBehaviorsEditorStyle final : public FSlateStyleSet
{
public:
	virtual ~FGameplayBehaviorsEditorStyle();

	static FGameplayBehaviorsEditorStyle& Get();
	static void Shutdown();

	static FColor GameplayTagTypeColor;

private:
	FGameplayBehaviorsEditorStyle();

	static TUniquePtr<FGameplayBehaviorsEditorStyle> Instance;
};