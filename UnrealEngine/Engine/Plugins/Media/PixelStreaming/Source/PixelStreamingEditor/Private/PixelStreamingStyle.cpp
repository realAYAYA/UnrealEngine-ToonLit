// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"

namespace UE::EditorPixelStreaming
{
    TSharedPtr<FSlateStyleSet> FPixelStreamingStyle::StyleInstance = NULL;
    void FPixelStreamingStyle::Initialize()
    {
        if(!StyleInstance.IsValid())
        {
            StyleInstance = Create();
            FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
        }
    }

    void FPixelStreamingStyle::Shutdown()
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    	ensure(StyleInstance.IsUnique());
	    StyleInstance.Reset();
    }

    FName FPixelStreamingStyle::GetStyleSetName()
    {
        static FName StyleSetName(TEXT("PixelStreamingStyle"));
        return StyleSetName;
    }

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

    const FVector2D Icon16x16(16.0f, 16.0f);
    const FVector2D Icon20x20(20.0f, 20.0f);
    const FVector2D Icon64x64(64.0f, 64.0f);
    
    TSharedRef<FSlateStyleSet> FPixelStreamingStyle::Create()
    {
        TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("PixelStreamingStyle"));
        Style->SetContentRoot(IPluginManager::Get().FindPlugin("PixelStreaming")->GetBaseDir() / TEXT("Resources"));
        Style->Set("PixelStreaming.Icon", new IMAGE_BRUSH_SVG("PixelStreaming_16", Icon16x16));
        return Style;
    }
#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
    
    void FPixelStreamingStyle::ReloadTextures()
    {
        if(FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
        }
    }
    
    const ISlateStyle& FPixelStreamingStyle::Get()
    {
        return *StyleInstance;
    }
}