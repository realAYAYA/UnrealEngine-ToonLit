// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** Style data for Insights tools */
class FConcertServerStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	
	static TSharedPtr<FSlateStyleSet> StyleInstance;
	
	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);
	static TSharedRef<FSlateStyleSet> Create();

};
