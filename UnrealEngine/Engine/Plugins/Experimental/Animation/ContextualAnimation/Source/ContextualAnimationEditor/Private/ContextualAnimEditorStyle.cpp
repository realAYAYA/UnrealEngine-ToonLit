// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr< FSlateStyleSet > FContextualAnimEditorStyle::StyleInstance = NULL;

void FContextualAnimEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FContextualAnimEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FContextualAnimEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ContextualAnimEditorStyle"));
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

TSharedRef< FSlateStyleSet > FContextualAnimEditorStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("ContextualAnimEditorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("ContextualAnimation")->GetBaseDir() / TEXT("Content"));

	Style->Set("ContextualAnimEditor.Icon", new IMAGE_BRUSH(TEXT("ButtonIcon_40x"), Icon40x40));

	const FString EngineEditorSlateDir = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	Style->SetContentRoot(EngineEditorSlateDir);
	Style->Set("ContextualAnimEditor.Viewport.Border", new BOX_BRUSH("Old/Window/ViewportDebugBorder", 0.8f, FLinearColor(1.f, 1.f, 1.f, 1.f)));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FContextualAnimEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FContextualAnimEditorStyle::Get()
{
	return *StyleInstance;
}
