// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlasticSourceControlStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#include "PlasticSourceControlModule.h"

TSharedPtr<FSlateStyleSet> FPlasticSourceControlStyle::StyleInstance = nullptr;

void FPlasticSourceControlStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FPlasticSourceControlStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FPlasticSourceControlStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("PlasticSourceControlStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);

TSharedRef<FSlateStyleSet> FPlasticSourceControlStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("PlasticSourceControlStyle"));
	Style->SetContentRoot(FPlasticSourceControlModule::GetPlugin()->GetBaseDir() / TEXT("Resources"));

	Style->Set("PlasticSourceControl.PluginIcon.Small", new FSlateImageBrush(FPlasticSourceControlStyle::InContent("Icon128", ".png"), Icon16x16));

	return Style;
}

FString FPlasticSourceControlStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	auto myself = FPlasticSourceControlModule::GetPlugin();
	check(myself.IsValid());
	static FString ContentDir = myself->GetBaseDir() / TEXT("Resources");
	return (ContentDir / RelativePath) + Extension;
}

void FPlasticSourceControlStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FPlasticSourceControlStyle::Get()
{
	return *StyleInstance;
}