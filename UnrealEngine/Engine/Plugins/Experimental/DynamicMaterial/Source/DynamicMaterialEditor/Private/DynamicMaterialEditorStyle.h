// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

enum class EDMValueType : uint8;

class FDynamicMaterialEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static FName GetStyleSetName();
	static const ISlateStyle& Get();

	static FLinearColor GetColor(const FName& InName);
	static const FSlateBrush* GetBrush(const FName& InName);

	static FName GetBrushNameForType(EDMValueType InType);

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;

	static TSharedRef<FSlateStyleSet> Create();

	static void SetupStageStyles(const TSharedRef<FSlateStyleSet>& Style);
	static void SetupLayerViewStyles(const TSharedRef<FSlateStyleSet>& Style);
	static void SetupLayerViewItemHandleStyles(const TSharedRef<FSlateStyleSet>& Style);
	static void SetupEffectsViewStyles(const TSharedRef<FSlateStyleSet>& Style);
	static void SetupTextStyles(const TSharedRef<FSlateStyleSet>& Style);
};
