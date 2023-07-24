// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserToolBoxStyle.h"
#include "Styling/SlateStyle.h"
#include "FileCache.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/StyleColors.h"

TSharedPtr< FSlateStyleSet > FUserToolBoxStyle::StyleInstance = nullptr;
TArray<FString> FUserToolBoxStyle::ExternalBrushIds;
void FUserToolBoxStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FUserToolBoxStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FUserToolBoxStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("UserToolBoxStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon15x15(16.0f, 16.0f);
const FVector2D Icon30x30(16.0f, 16.0f);
const FVector2D Icon60x60(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FUserToolBoxStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("UserToolBoxStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("UserToolBoxCore")->GetBaseDir() / TEXT("Resources"));
	Style->Set("Palette.FirstHeader",new FSlateRoundedBoxBrush(FStyleColors::Dropdown, FVector4(4.0, 4.0, 0.0, 0.0)));
	Style->Set("Palette.LastHeader",new FSlateRoundedBoxBrush(FStyleColors::Dropdown, FVector4(0.0, 0.0, 4.0, 4.0)));
	Style->Set("Palette.Header",new FSlateRoundedBoxBrush(FStyleColors::Dropdown, FVector4(0.0, 0.0, 0.0, 0.0)));
	Style->Set("Palette.UniqueHeader",new FSlateRoundedBoxBrush(FStyleColors::Dropdown, FVector4(4.0, 4.0, 4.0, 4.0)));
	Style->Set("Palette.Body",new FSlateRoundedBoxBrush(FStyleColors::Recessed, FVector4(0.0, 0.0, 0.0, 0.0)));
	
	
	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FUserToolBoxStyle::AddExternalImageBrushes(const TArray<FIconInfo>& IconInfos)
{
	if (StyleInstance==nullptr)
	{
		return;
	}
	for (const FIconInfo& Info:IconInfos)
	{
				StyleInstance->Set(FName(*Info.Id),new FSlateImageBrush(Info.Path,Info.IconSize));
				ExternalBrushIds.Add(Info.Id);
	}
	ReloadTextures();
}

void FUserToolBoxStyle::ClearExternalImageBrushes()
{
	ExternalBrushIds.Empty();
}

TArray<FString> FUserToolBoxStyle::GetAvailableExternalImageBrushes()
{
	return ExternalBrushIds;
}

void FUserToolBoxStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FUserToolBoxStyle::Get()
{
	return *StyleInstance;
}
