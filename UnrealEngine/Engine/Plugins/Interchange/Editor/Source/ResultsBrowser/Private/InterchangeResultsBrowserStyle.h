// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * Implements the visual style of the text asset editor UI.
 */
class FInterchangeResultsBrowserStyle final : public FSlateStyleSet
{
public:

	FInterchangeResultsBrowserStyle()
		: FSlateStyleSet("InterchangeResultsBrowserStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);

		const FString BaseDir = IPluginManager::Get().FindPlugin("InterchangeEditor")->GetBaseDir();
		SetContentRoot(BaseDir / TEXT("Content"));

		FSlateImageBrush* SuccessBrush = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Result_Success"), TEXT(".png")), Icon16x16);
		Set("InterchangeResultsBrowser.ResultType.Success", SuccessBrush);
		
		FSlateImageBrush* WarningBrush = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Result_Warning"), TEXT(".png")), Icon16x16);
		Set("InterchangeResultsBrowser.ResultType.Warning", WarningBrush);

		FSlateImageBrush* ErrorBrush = new FSlateImageBrush(RootToContentDir(TEXT("Resources/Result_Fail"), TEXT(".png")), Icon16x16);
		Set("InterchangeResultsBrowser.ResultType.Error", ErrorBrush);

		FSlateImageBrush* TabIcon = new FSlateImageBrush(RootToContentDir(TEXT("Resources/TabIcon"), TEXT(".png")), Icon16x16);
		Set("InterchangeResultsBrowser.TabIcon", TabIcon);

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FInterchangeResultsBrowserStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
};
