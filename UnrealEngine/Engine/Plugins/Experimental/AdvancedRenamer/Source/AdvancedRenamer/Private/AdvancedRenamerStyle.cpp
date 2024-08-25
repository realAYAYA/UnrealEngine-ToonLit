// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamerStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

TSharedPtr<FSlateStyleSet> FAdvancedRenamerStyle::StyleInstance = nullptr;

void FAdvancedRenamerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAdvancedRenamerStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

FName FAdvancedRenamerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AdvancedRenamerStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FAdvancedRenamerStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>("AdvancedRenamer");

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AdvancedRenamer"));
	check(Plugin.IsValid());

	if (Plugin.IsValid())
	{
		Style->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
	}

	Style->Set("AdvancedRenamer.Image.Radio.BlackBackground", new FSlateVectorImageBrush(
		FPaths::Combine(Style->GetContentRootDir(), "/Icons/radio-background.svg"), 
		FVector2D(16.f, 16.f), FStyleColors::Foldout.GetSpecifiedColor()
	));

	FCheckBoxStyle BlackRadioButton = FAppStyle::GetWidgetStyle<FCheckBoxStyle>("RadioButton");
	BlackRadioButton.SetBackgroundImage(*Style->GetBrush("AdvancedRenamer.Image.Radio.BlackBackground"));

	Style->Set("AdvancedRenamer.Style.BlackRadioButton", BlackRadioButton);

	const FButtonStyle DarkButton = FButtonStyle(FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.SetNormal(FSlateColorBrush(FStyleColors::Recessed))
		.SetHovered(FSlateColorBrush(FStyleColors::Hover))
		.SetPressed(FSlateColorBrush(FStyleColors::Header))
		.SetDisabled(FSlateColorBrush(FStyleColors::Dropdown));

	Style->Set("AdvancedRenamer.Style.DarkButton", DarkButton);

	return Style;
}

const ISlateStyle& FAdvancedRenamerStyle::Get()
{
	if (!StyleInstance.IsValid())
	{
		Initialize();
	}

	return *StyleInstance;
}
