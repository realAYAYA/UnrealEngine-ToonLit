// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariantManagerStyle.h"

#include "Styling/AppStyle.h"
#include "Interfaces/IPluginManager.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleMacros.h"

FVariantManagerStyle::FVariantManagerStyle() : FSlateStyleSet("VariantManagerEditorStyle")
{
	SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("VariantManager"))->GetContentDir());

	// Const icon sizes
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	/** Color used for the background of the entire variant manager as well as the spacer border */
	Set( "VariantManager.Panels.LightBackgroundColor", FLinearColor( FColor( 96, 96, 96, 255 ) ) );

	/** Color used as background for variant nodes, and background of properties and dependencies panels */
	Set( "VariantManager.Panels.ContentBackgroundColor", FLinearColor( FColor( 62, 62, 62, 255 ) ) );

	/** Color used for background of variant set nodes and panel headers, like Properties or Dependencies headers */
	Set( "VariantManager.Panels.HeaderBackgroundColor", FLinearColor( FColor( 48, 48, 48, 255 ) ) );

	/** Thickness of the light border around the entire variant manager tab and between items */
	Set( "VariantManager.Spacings.BorderThickness", 4.0f );

	/** The amount to indent child nodes of the layout tree */
	Set( "VariantManager.Spacings.IndentAmount", 10.0f );

	Set( "VariantManager.Icon", new IMAGE_BRUSH_SVG("VariantManager", Icon16x16) );

	Set("VariantManager.AutoCapture.Icon", new IMAGE_BRUSH_SVG("AutoCapture", Icon20x20));

	SetContentRoot(FPaths::EngineContentDir());
	const FCheckBoxStyle RadioButtonStyle = FCheckBoxStyle()
		.SetUncheckedImage(IMAGE_BRUSH_SVG("/Slate/Starship/CoreWidgets/CheckBox/radio-off", Icon16x16))
		.SetUncheckedHoveredImage(IMAGE_BRUSH_SVG("/Slate/Starship/CoreWidgets/CheckBox/radio-off", Icon16x16, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
		.SetUncheckedPressedImage(IMAGE_BRUSH_SVG("/Slate/Starship/CoreWidgets/CheckBox/radio-off", Icon16x16, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
		.SetCheckedImage(IMAGE_BRUSH_SVG("/Slate/Starship/CoreWidgets/CheckBox/radio-on", Icon16x16))
		.SetCheckedHoveredImage(IMAGE_BRUSH_SVG("/Slate/Starship/CoreWidgets/CheckBox/radio-on", Icon16x16, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
		.SetCheckedPressedImage(IMAGE_BRUSH_SVG("/Slate/Starship/CoreWidgets/CheckBox/radio-on", Icon16x16, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
	Set("VariantManager.VariantRadioButton", RadioButtonStyle);

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}
