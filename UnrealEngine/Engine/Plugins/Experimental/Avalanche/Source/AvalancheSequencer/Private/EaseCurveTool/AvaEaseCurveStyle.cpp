// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurveStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

FAvaEaseCurveStyle::FAvaEaseCurveStyle()
	: FSlateStyleSet(TEXT("EaseCurveTool"))
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());
	
	SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));

	Set("Preset.Selected", new FSlateRoundedBoxBrush(FStyleColors::Transparent, FVector4(4.f, 0.f, 0.f, 4.f), FStyleColors::Select.GetSpecifiedColor(), 1.f));

	Set("EditMode.Background", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.f, FLinearColor(0.1f, 0.1f, 0.1f, 1.f), 1.f));
	Set("EditMode.Background.Highlight", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.f, FLinearColor(0.6f, 0.6f, 0.6f, 1.f), 1.f));
	Set("EditMode.Background.Over", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.f, FStyleColors::AccentBlue.GetSpecifiedColor(), 1.f));

	const FButtonStyle& SimpleButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton"));

	constexpr float ToolButtonPadding = 2.f;
	Set("ToolButton.Padding", ToolButtonPadding);

	constexpr float ToolButtonImageSize = 12.f;
	Set("ToolButton.ImageSize", ToolButtonImageSize);

	const FButtonStyle ToolButtonStyle = FButtonStyle(SimpleButtonStyle)
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetNormalPadding(FMargin(ToolButtonPadding))
		.SetPressedPadding(FMargin(ToolButtonPadding, ToolButtonPadding + (ToolButtonPadding * 0.5f), ToolButtonPadding, ToolButtonPadding - (ToolButtonPadding * 0.5f)));
	Set("ToolButton", ToolButtonStyle);

	const FButtonStyle ToolButtonNoPadStyle = FButtonStyle(ToolButtonStyle)
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));
	Set("ToolButton.NoPad", ToolButtonNoPadStyle);

	const FCheckBoxStyle& ToggleButtonCheckboxStyle = FAppStyle::GetWidgetStyle<FCheckBoxStyle>(TEXT("ToggleButtonCheckbox"));

	const FCheckBoxStyle ToolToggleButtonStyle = FCheckBoxStyle(ToggleButtonCheckboxStyle)
		.SetUncheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetUncheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetCheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::PrimaryHover, 4.0f))
		.SetCheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::PrimaryPress, 4.0f))
		.SetPadding(ToolButtonPadding);
	Set("ToolToggleButton", ToolToggleButtonStyle);

	Set("Editor.LabelFont", FSlateFontInfo(FCoreStyle::GetDefaultFont(), 7, TEXT("Regular")));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaEaseCurveStyle::~FAvaEaseCurveStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
