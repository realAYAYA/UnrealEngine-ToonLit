// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorPaletteStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"

TSharedPtr< FSlateStyleSet > FActorPaletteStyle::StyleInstance = NULL;

void FActorPaletteStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FActorPaletteStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FActorPaletteStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ActorPaletteStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FActorPaletteStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("ActorPaletteStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("ActorPalette"))->GetBaseDir() / TEXT("Resources"));

	Style->Set("ActorPalette.OpenPluginWindow", new IMAGE_BRUSH(TEXT("ButtonIcon_40x"), Icon40x40));

	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	Style->Set("ActorPalette.ViewportTitleTextStyle", FTextBlockStyle(NormalText)
		.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18))
		.SetColorAndOpacity(FLinearColor(1.0, 1.0f, 1.0f, 0.5f))
		);

	Style->Set("ActorPalette.Palette", new IMAGE_BRUSH(TEXT("Palette_40x"), Icon40x40));
	Style->Set("ActorPalette.Palette.Small", new IMAGE_BRUSH(TEXT("Palette_40x"), Icon20x20));
	Style->Set("ActorPalette.TabIcon", new IMAGE_BRUSH(TEXT("Palette_16x"), Icon16x16));

	Style->Set("ActorPalette.ViewportTitleBackground", new BOX_BRUSH("GraphTitleBackground", FMargin(0)));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FActorPaletteStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FActorPaletteStyle::Get()
{
	return *StyleInstance;
}
