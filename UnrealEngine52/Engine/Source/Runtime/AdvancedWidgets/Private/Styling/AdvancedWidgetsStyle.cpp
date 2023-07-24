// Copyright Epic Games, Inc. All Rights Reserved.

#include "Styling/AdvancedWidgetsStyle.h"

#include "Framework/PropertyViewer/FieldIconFinder.h"
#include "Math/ColorList.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "UObject/Class.h"


#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Instance->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

namespace UE::AdvancedWidgets
{

TUniquePtr<FSlateStyleSet> FAdvancedWidgetsStyle::Instance;
::UE::PropertyViewer::FFieldColorSettings FAdvancedWidgetsStyle::ColorSettings;


const ISlateStyle& FAdvancedWidgetsStyle::Get()
{
	return *(Instance.Get());
}

void FAdvancedWidgetsStyle::Create()
{
	Instance = MakeUnique<FSlateStyleSet>("AdvancedWidgets");
	Instance->SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	Instance->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	Instance->Set("PropertyValue.SpinBox", FSpinBoxStyle(FAppStyle::GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
		.SetTextPadding(FMargin(0.f))
		.SetBackgroundBrush(BORDER_BRUSH("Old/HyperlinkDotted", FMargin(0.f, 0.f, 0.f, 3.f / 16.f), FSlateColor::UseSubduedForeground()))
		.SetHoveredBackgroundBrush(FSlateNoResource())
		.SetInactiveFillBrush(FSlateNoResource())
		.SetActiveFillBrush(FSlateNoResource())
		.SetForegroundColor(FSlateColor::UseSubduedForeground())
		.SetArrowsImage(FSlateNoResource())
	);

	FButtonStyle Button = FAppStyle::GetWidgetStyle<FButtonStyle>("Button");
	Instance->Set("PropertyValue.ComboButton", FComboButtonStyle(FAppStyle::GetWidgetStyle<FComboButtonStyle>("ComboButton"))
		.SetButtonStyle(
			Button.SetNormal(FSlateNoResource())
			.SetNormalForeground(FSlateColor::UseSubduedForeground())
			.SetDisabledForeground(FSlateColor::UseSubduedForeground())
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f))
		)
		.SetMenuBorderPadding(FMargin(0.0f))
		.SetDownArrowPadding(FMargin(0.0f))
		);

	FLinearColor& VectorColor = ColorSettings.StructColors.FindOrAdd(TBaseStructure<FVector>::Get()->GetStructPathName().ToString());
	VectorColor = FColorList::Yellow;
	FLinearColor& RotatorColor = ColorSettings.StructColors.FindOrAdd(TBaseStructure<FRotator>::Get()->GetStructPathName().ToString());
	RotatorColor = FColorList::DarkTurquoise;


	FSlateStyleRegistry::RegisterSlateStyle(*Instance.Get());
}

void FAdvancedWidgetsStyle::Destroy()
{
	if (Instance)
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance.Get());
	}
}

} //namespace

#undef BORDER_BRUSH
