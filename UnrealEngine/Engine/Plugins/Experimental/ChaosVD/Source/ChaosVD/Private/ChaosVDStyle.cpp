// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FChaosVDStyle::StyleInstance = nullptr;

void FChaosVDStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FChaosVDStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FChaosVDStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ChaosVDStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon50x50(50.0f, 50.0f);

TSharedRef< FSlateStyleSet > FChaosVDStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("ChaosVDStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("ChaosVD")->GetBaseDir() / TEXT("Resources"));

	Style->Set("ChaosVD.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

	Style->Set("PlayIcon", new IMAGE_BRUSH(TEXT("Play_16x"), Icon16x16));
	Style->Set("StopIcon",  new IMAGE_BRUSH(TEXT("Stop_16x"), Icon16x16));
	Style->Set("NextIcon",  new IMAGE_BRUSH(TEXT("Next_16x"), Icon16x16));
	Style->Set("PrevIcon",  new IMAGE_BRUSH( TEXT("Prev_16x"), Icon16x16));
	Style->Set("PauseIcon",  new IMAGE_BRUSH( TEXT("generic_pause_16x"), Icon16x16));
	
	Style->Set("TabIconDetailsPanel",  new IMAGE_BRUSH_SVG(TEXT("Details"), Icon16x16));
	Style->Set("TabIconPlaybackViewport",  new IMAGE_BRUSH(TEXT("icon_tab_PlaybackViewport_16x"), Icon16x16));
	Style->Set("TabIconWorldOutliner", new IMAGE_BRUSH_SVG(TEXT("WorldOutliner"), Icon16x16));
	
	Style->Set("LockIcon", new IMAGE_BRUSH_SVG(TEXT("lock"), Icon16x16));
	Style->Set("UnlockedIcon", new IMAGE_BRUSH_SVG(TEXT("lock-unlocked"), Icon16x16));

	return Style;
}

void FChaosVDStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FChaosVDStyle::Get()
{
	return *StyleInstance;
}
