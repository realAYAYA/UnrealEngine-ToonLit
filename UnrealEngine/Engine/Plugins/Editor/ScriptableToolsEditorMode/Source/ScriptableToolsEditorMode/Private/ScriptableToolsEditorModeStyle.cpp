// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptableToolsEditorModeStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Misc/Paths.h"
#include "Styling/SlateTypes.h"
#include "Styling/AppStyle.h"
#include "Interfaces/IPluginManager.h"
#include "SlateOptMacros.h"
#include "Styling/SlateStyle.h"


#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FScriptableToolsEditorModeStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )

// This is to fix the issue that SlateStyleMacros like IMAGE_BRUSH look for RootToContentDir but StyleSet->RootToContentDir is how this style is set up
#define RootToContentDir StyleSet->RootToContentDir

FString FScriptableToolsEditorModeStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ScriptableToolsEditorMode"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

TSharedPtr< FSlateStyleSet > FScriptableToolsEditorModeStyle::StyleSet = nullptr;
TSharedPtr< class ISlateStyle > FScriptableToolsEditorModeStyle::Get() { return StyleSet; }

FName FScriptableToolsEditorModeStyle::GetStyleSetName()
{
	static FName ScriptableToolsEditorModeStyleName(TEXT("ScriptableToolsEditorModeStyle"));
	return ScriptableToolsEditorModeStyleName;
}

const FSlateBrush* FScriptableToolsEditorModeStyle::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return Get()->GetBrush(PropertyName, Specifier);
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FScriptableToolsEditorModeStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon28x28(28.0f, 28.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon120(120.0f, 120.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Editor/ScriptableToolsEditorMode/Content"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	{
		StyleSet->Set("LevelEditor.ScriptableToolsEditorMode", new IMAGE_PLUGIN_BRUSH("Icons/ScriptableToolsEditorMode_Icon_40x", FVector2D(20.0f, 20.0f)));

		StyleSet->Set("ScriptableToolsEditorModeToolCommands.DefaultToolIcon", new IMAGE_PLUGIN_BRUSH("Icons/Tool_DefaultIcon_40px", Icon20x20));
		StyleSet->Set("ScriptableToolsEditorModeToolCommands.DefaultToolIcon.Small", new IMAGE_PLUGIN_BRUSH("Icons/Tool_DefaultIcon_40px", Icon20x20));

	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};


bool FScriptableToolsEditorModeStyle::TryRegisterCustomIcon(FName StyleIdentifier, FString FileNameWithPath, FString ExternalRelativePath)
{
	if ( ! ensure(StyleSet.IsValid()) )
	{
		return false;
	}

	const FVector2D Icon20x20(20.0f, 20.0f);

	if (ExternalRelativePath.IsEmpty())
	{
		ExternalRelativePath = FileNameWithPath;
	}

	if (FPaths::FileExists(FileNameWithPath) == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("Custom ScriptableTool Icon file not found at path %s (on-disk path is %s)"), *ExternalRelativePath, *FileNameWithPath);
		return false;
	}

	FString Extension = FPaths::GetExtension(FileNameWithPath);
	if ( !(Extension.Equals("svg", ESearchCase::IgnoreCase) || Extension.Equals("png", ESearchCase::IgnoreCase)) )
	{
		UE_LOG(LogTemp, Warning, TEXT("Custom ScriptableTool Icon at path %s has unsupported type, must be svg or png"), *ExternalRelativePath);
		return false;
	}

	// need to re-register style to be able to modify it
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());

	if (Extension.Equals("svg", ESearchCase::IgnoreCase))
	{
		StyleSet->Set(StyleIdentifier,
			new FSlateVectorImageBrush(FileNameWithPath, Icon20x20));
	}
	else if (Extension.Equals("png", ESearchCase::IgnoreCase))
	{
		StyleSet->Set(StyleIdentifier,
			new FSlateImageBrush(FileNameWithPath, Icon20x20) );
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());

	return true;
}



END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_PLUGIN_BRUSH
#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef DEFAULT_FONT

void FScriptableToolsEditorModeStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}
