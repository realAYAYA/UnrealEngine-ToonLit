// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;

/** Manages the style that provides resources for Shotgrid integration */
class FShotgridStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();
	static FName GetContextName();

	/** Adds an icon (.png in the given ResourcePath) with the given style name to this style set */
	static void SetIcon(const FString& StyleName, const FString& ResourcePath);

private:
	static TUniquePtr< FSlateStyleSet > Create();
	static TUniquePtr< FSlateStyleSet > ShotgridStyleInstance;
};