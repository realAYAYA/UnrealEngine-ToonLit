// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaInteractiveToolsStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Containers/StringFwd.h"
#include "Interfaces/IPluginManager.h"
#include "Math/MathFwd.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

FAvaInteractiveToolsStyle::FAvaInteractiveToolsStyle()
	: FSlateStyleSet(TEXT("AvaInteractiveTools"))
{
	const FVector2f Icon16x16(16.0f, 16.0f);
	const FVector2f Icon20x20(20.0f, 20.0f);

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());

	SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	Set("Icons.Toolbox", new IMAGE_BRUSH(TEXT("Icons/ToolboxIcons/toolbox"), Icon16x16));

	// Categories
	Set("AvaInteractiveTools.Category_2D",     new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle", Icon20x20));
	Set("AvaInteractiveTools.Category_3D",     new IMAGE_BRUSH("Icons/ToolboxIcons/cube", Icon20x20));
	Set("AvaInteractiveTools.Category_Actor",  new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/Actor_16", Icon16x16));
	Set("AvaInteractiveTools.Category_Layout", new IMAGE_BRUSH("Icons/ToolboxIcons/layoutgrid", Icon20x20));

	// Actor Tools
	Set("AvaInteractiveTools.Tool_Actor_Null", new CORE_IMAGE_BRUSH(TEXT("Icons/SequencerIcons/icon_Sequencer_Move_24x"), Icon16x16));
	Set("AvaInteractiveTools.Tool_Actor_Spline", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Spline", Icon20x20));
	Set("Tool_Actor_Null", new CORE_IMAGE_BRUSH(TEXT("Icons/SequencerIcons/icon_Sequencer_Move_24x"), Icon20x20));
	Set("Tool_Actor_Spline", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Spline", Icon20x20));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaInteractiveToolsStyle::~FAvaInteractiveToolsStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
