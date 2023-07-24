// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
 	

#include "Styling/ISlateStyle.h"
#include "IconsTracker.h"
/**  */
class USERTOOLBOXCORE_API FUserToolBoxStyle
{
public:

	static void Initialize();

	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for the Shooter game */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();
	static void AddExternalImageBrushes(const TArray<FIconInfo>& IconInfos);
	static void ClearExternalImageBrushes();
	static TArray<FString> GetAvailableExternalImageBrushes();
private:

	static TSharedRef< class FSlateStyleSet > Create();



	static TSharedPtr< class FSlateStyleSet > StyleInstance;
	static TArray<FString> ExternalBrushIds;
};