// Copyright Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewWindowCoreStyle.h"
#include "Styling/CoreStyle.h"
#include "PIEPreviewWindowStyle.h"
#include "SlateGlobals.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "PIEPreviewDeviceSpecification.h"

#if WITH_EDITOR

TSharedPtr< ISlateStyle > FPIEPreviewWindowCoreStyle::Instance = nullptr;

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush(Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__)

TSharedRef<class ISlateStyle> FPIEPreviewWindowCoreStyle::Create(const FName& InStyleSetName)
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(InStyleSetName));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FButtonStyle Button = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");

	const FButtonStyle ScreenRotationButtonStyle = FButtonStyle(Button)
		.SetDisabled(IMAGE_BRUSH("Icons/PIEWindow/WindowButton_Screen_Rotation_Disabled", FVector2D(23.0f, 18.0f)))
		.SetNormal(IMAGE_BRUSH("Icons/PIEWindow/WindowButton_Screen_Rotation_Normal", FVector2D(23.0f, 18.0f)))
		.SetHovered(IMAGE_BRUSH("Icons/PIEWindow/WindowButton_Screen_Rotation_Hovered", FVector2D(23.0f, 18.0f)))
		.SetPressed(IMAGE_BRUSH("Icons/PIEWindow/WindowButton_Screen_Rotation_Pressed", FVector2D(23.0f, 18.0f)));

	Style->Set("PIEWindow", FPIEPreviewWindowStyle()
		.SetScreenRotationButtonStyle(ScreenRotationButtonStyle));

	Style->Set("PIEWindow.Font", DEFAULT_FONT("Bold", 9));

	Style->Set("PIEWindow.MenuButton", FButtonStyle(Button)
		.SetNormal(BOX_BRUSH("Icons/PIEWindow/SmallRoundedButton", FMargin(7.f / 16.f), FLinearColor(1, 1, 1, 0.75f)))
		.SetHovered(BOX_BRUSH("Icons/PIEWindow/SmallRoundedButton", FMargin(7.f / 16.f), FLinearColor(1, 1, 1, 1.0f)))
		.SetPressed(BOX_BRUSH("Icons/PIEWindow/SmallRoundedButton", FMargin(7.f / 16.f))));

	Style->Set("ComboButton.Arrow", new IMAGE_BRUSH("Common/ComboArrow", FVector2D(8.0f, 8.0f)));

	return Style;
}

#undef IMAGE_BRUSH
#undef DEFAULT_FONT
#undef BOX_BRUSH

void FPIEPreviewWindowCoreStyle::InitializePIECoreStyle()
{
	if (!Instance.IsValid())
	{
		Instance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*Instance.Get());
	}
}
#endif