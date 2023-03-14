// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingEditorStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr< FSlateStyleSet > FDMXPixelMappingEditorStyle::DMXPixelMappingEditorStyleInstance = nullptr;

void FDMXPixelMappingEditorStyle::Initialize()
{
	if (!DMXPixelMappingEditorStyleInstance.IsValid())
	{
		DMXPixelMappingEditorStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*DMXPixelMappingEditorStyleInstance);
	}
}

void FDMXPixelMappingEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*DMXPixelMappingEditorStyleInstance);
	ensure(DMXPixelMappingEditorStyleInstance.IsUnique());
	DMXPixelMappingEditorStyleInstance.Reset();
}

FName FDMXPixelMappingEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("DMXPixelMappingEditorStyle"));
	return StyleSetName;
}

FString RelativePathToPluginPath(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("DMXPixelMapping"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush( Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__ )
#define IMAGE_CORE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )
#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( RelativePathToPluginPath( RelativePath, ".png" ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_CORE_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon12x12(12.0f, 12.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);
const FVector2D Icon64x64(64.0f, 64.0f);

TSharedRef<FSlateStyleSet> FDMXPixelMappingEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("DMXPixelMappingEditorStyle"));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DMXPixelMapping"));
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Slate")));
	}

	// Class styles
	Style->Set("ClassIcon.DMXPixelMapping", new IMAGE_BRUSH_SVG("DMXPixelMapping_16", Icon16x16));
	Style->Set("ClassThumbnail.DMXPixelMapping", new IMAGE_BRUSH_SVG("DMXPixelMapping_64", Icon64x64));

	// Icons
	Style->Set("Icons.Preview", new IMAGE_BRUSH_SVG("Preview", Icon16x16));
	Style->Set("Icons.ZoomToFit", new IMAGE_BRUSH_SVG("ZoomToFit", Icon16x16));

	// Toolbar Icons
	Style->Set("DMXPixelMappingEditor.AddMapping", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_AddMapping_40x", Icon40x40));
	Style->Set("DMXPixelMappingEditor.AddMapping.Small", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_AddMapping_40x", Icon20x20));

	Style->Set("DMXPixelMappingEditor.PlayDMX", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_PlayDMX_40x", Icon40x40));
	Style->Set("DMXPixelMappingEditor.PlayDMX.Small", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_PlayDMX_40x", Icon20x20));
	Style->Set("DMXPixelMappingEditor.StopPlayingDMX", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_StopPlayingDMX_40x", Icon40x40));
	Style->Set("DMXPixelMappingEditor.StopPlayingDMX.Small", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_StopPlayingDMX_40x", Icon20x20));
	

	return Style;
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG
#undef IMAGE_CORE_BRUSH
#undef IMAGE_PLUGIN_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef BOX_CORE_BRUSH
#undef DEFAULT_FONT

void FDMXPixelMappingEditorStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FDMXPixelMappingEditorStyle::Get()
{
	return *DMXPixelMappingEditorStyleInstance;
}
