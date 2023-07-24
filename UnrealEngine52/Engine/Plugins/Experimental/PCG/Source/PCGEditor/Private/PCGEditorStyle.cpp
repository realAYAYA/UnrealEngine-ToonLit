// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

void FPCGEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FPCGEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FPCGEditorStyle::FPCGEditorStyle() : FSlateStyleSet("PCGEditorStyle")
{
    static const FVector2D Icon20x20(20.0f, 20.0f);
	
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	SetContentRoot(IPluginManager::Get().FindPlugin("PCG")->GetBaseDir() / TEXT("Content"));

	Set("PCG.NodeOverlay.Debug", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", Icon20x20, FSlateColor(FColor::Cyan)));
	Set("PCG.NodeOverlay.Inspect", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", Icon20x20, FSlateColor(FColor::Orange)));

	static const FVector2D PinSize(22.0f, 22.0f);
	Set(PCGEditorStyleConstants::Pin_SD_SC_IN_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_SC_IN", PinSize));
	Set(PCGEditorStyleConstants::Pin_SD_SC_IN_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_SC_IN_Unplugged", PinSize));
	Set(PCGEditorStyleConstants::Pin_SD_SC_OUT_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_SC_OUT", PinSize));
	Set(PCGEditorStyleConstants::Pin_SD_SC_OUT_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_SC_OUT_Unplugged", PinSize));

	Set(PCGEditorStyleConstants::Pin_SD_MC_IN_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_MC_IN", PinSize));
	Set(PCGEditorStyleConstants::Pin_SD_MC_IN_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_MC_IN_Unplugged", PinSize));
	Set(PCGEditorStyleConstants::Pin_SD_MC_OUT_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_MC_OUT", PinSize));
	Set(PCGEditorStyleConstants::Pin_SD_MC_OUT_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_SD_MC_OUT_Unplugged", PinSize));

	Set(PCGEditorStyleConstants::Pin_MD_SC_IN_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_SC_IN", PinSize));
	Set(PCGEditorStyleConstants::Pin_MD_SC_IN_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_SC_IN_Unplugged", PinSize));
	Set(PCGEditorStyleConstants::Pin_MD_SC_OUT_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_SC_OUT", PinSize));
	Set(PCGEditorStyleConstants::Pin_MD_SC_OUT_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_SC_OUT_Unplugged", PinSize));

	Set(PCGEditorStyleConstants::Pin_MD_MC_IN_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_MC_IN", PinSize));
	Set(PCGEditorStyleConstants::Pin_MD_MC_IN_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_MC_IN_Unplugged", PinSize));
	Set(PCGEditorStyleConstants::Pin_MD_MC_OUT_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_MC_OUT", PinSize));
	Set(PCGEditorStyleConstants::Pin_MD_MC_OUT_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_MD_MC_OUT_Unplugged", PinSize));

	Set(PCGEditorStyleConstants::Pin_Param_IN_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Param_IN", PinSize));
	Set(PCGEditorStyleConstants::Pin_Param_IN_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Param_IN_Unplugged", PinSize));
	Set(PCGEditorStyleConstants::Pin_Param_OUT_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Param_OUT", PinSize));
	Set(PCGEditorStyleConstants::Pin_Param_OUT_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Param_OUT_Unplugged", PinSize));

	Set(PCGEditorStyleConstants::Pin_Composite_IN_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Composite_IN", PinSize));
	Set(PCGEditorStyleConstants::Pin_Composite_IN_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Composite_IN_Unplugged", PinSize));
	Set(PCGEditorStyleConstants::Pin_Composite_OUT_C, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Composite_OUT", PinSize));
	Set(PCGEditorStyleConstants::Pin_Composite_OUT_DC, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Composite_OUT_Unplugged", PinSize));

	FInlineEditableTextBlockStyle NodeTitleStyle = FInlineEditableTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.Node.NodeTitleInlineEditableText"));
	FTextBlockStyle GraphNodeItalicTitle = FTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("Graph.Node.NodeTitle"))
		.SetFont(DEFAULT_FONT("BoldCondensedItalic", 10));

	Set("PCG.Node.NodeTitleInlineEditableText", NodeTitleStyle);
	Set("PCG.Node.InstancedNodeTitleInlineEditableText", FInlineEditableTextBlockStyle(NodeTitleStyle)
		.SetTextStyle(GraphNodeItalicTitle));
}

const FPCGEditorStyle& FPCGEditorStyle::Get()
{
	static FPCGEditorStyle Instance;
	return Instance;
}
