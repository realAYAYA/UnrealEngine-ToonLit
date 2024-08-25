// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorStyle.h"

#include "PCGCommon.h"

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
	static const FVector2D Icon16x16(16.0f, 16.0f);
	static const FVector2D Icon20x20(20.0f, 20.0f);
	static const FVector2D Icon64x64(64.0f, 64.0f);
	static const FVector2D Icon128x128(128.0f, 128.0f);
	
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	SetContentRoot(IPluginManager::Get().FindPlugin("PCG")->GetBaseDir() / TEXT("Content"));

	Set("PCG.NodeOverlay.Debug", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", Icon20x20, FSlateColor(FColor::Cyan)));
	Set("PCG.NodeOverlay.Inspect", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", Icon20x20, FSlateColor(FColor::Orange)));
	Set("PCG.NodeOverlay.OnInactiveBranch", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/CompileStatus_Fail_Badge", Icon20x20, FSlateColor(FColor::White)));

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

	Set(PCGEditorStyleConstants::Pin_Required, new IMAGE_BRUSH_SVG("Style/PCG_Graph_RequiredPin_IN", FVector2D(8.0f, 22.0f)));

	Set(PCGEditorStyleConstants::Node_Overlay_Inactive, new IMAGE_BRUSH_SVG("Style/PCG_Node_Overlay_Inactive", Icon20x20));

	Set(PCGNodeConstants::Icons::CompactNodeFilter, new IMAGE_BRUSH_SVG("Style/PCG_Graph_Filter", FVector2D(28.0f, 28.0f)));
	Set(PCGNodeConstants::Icons::CompactNodeConvert, new IMAGE_BRUSH_SVG("Style/PCG_Graph_To", PinSize));

	FInlineEditableTextBlockStyle NodeTitleStyle = FInlineEditableTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.Node.NodeTitleInlineEditableText"));
	FTextBlockStyle GraphNodeItalicTitle = FTextBlockStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("Graph.Node.NodeTitle"))
		.SetFont(DEFAULT_FONT("BoldCondensedItalic", 10));

	Set("PCG.Node.NodeTitleInlineEditableText", NodeTitleStyle);
	Set("PCG.Node.InstancedNodeTitleInlineEditableText", FInlineEditableTextBlockStyle(NodeTitleStyle)
		.SetTextStyle(GraphNodeItalicTitle));

	// Styles for higen grid size label. Hand-tweaked values to match UI target mockups.
	Set(PCGEditorStyleConstants::Node_Overlay_GridSizeLabel_Active_Border, new FSlateRoundedBoxBrush(
		FLinearColor::White,
		PCGEditorStyleConstants::Node_Overlay_GridSizeLabel_BorderRadius,
		FLinearColor::Black,
		PCGEditorStyleConstants::Node_Overlay_GridSizeLabel_BorderStroke));

	// Plugin icon/editor/component icons
	Set("ClassIcon.PCGComponent", new IMAGE_BRUSH_SVG("Icons/PCG_16", Icon16x16));
	Set("ClassThumbnail.PCGComponent", new IMAGE_BRUSH_SVG("Icons/PCG_64", Icon64x64));
	Set("PCG.PluginIcon", new IMAGE_BRUSH_SVG("Icons/PCG_128", Icon128x128));
	Set("PCG.EditorIcon", new IMAGE_BRUSH_SVG("Icons/PCG_16", Icon16x16));

	// Asset/Class icons
	Set("ClassIcon.PCGVolume", new IMAGE_BRUSH_SVG("Icons/PCG_16", Icon16x16));
	Set("ClassThumbnail.PCGVolume", new IMAGE_BRUSH_SVG("Icons/PCG_64", Icon64x64));
	Set("ClassIcon.PCGWorldActor", new IMAGE_BRUSH_SVG("Icons/PCG_16", Icon16x16));
	Set("ClassThumbnail.PCGWorldActor", new IMAGE_BRUSH_SVG("Icons/PCG_64", Icon64x64));
	Set("ClassIcon.PCGPartitionActor", new IMAGE_BRUSH_SVG("Icons/PCG_16", Icon16x16));
	Set("ClassThumbnail.PCGPartitionActor", new IMAGE_BRUSH_SVG("Icons/PCG_64", Icon64x64));

	Set("ClassIcon.PCGDataAsset", new IMAGE_BRUSH_SVG("Icons/PCGAsset_16", Icon16x16));
	Set("ClassThumbnail.PCGDataAsset", new IMAGE_BRUSH_SVG("Icons/PCGAsset_64", Icon64x64));
	Set("ClassIcon.PCGGraphInterface", new IMAGE_BRUSH_SVG("Icons/PCGGraph_16", Icon16x16));
	Set("ClassThumbnail.PCGGraphInterface", new IMAGE_BRUSH_SVG("Icons/PCGGraph_64", Icon64x64));
	Set("ClassIcon.PCGGraph", new IMAGE_BRUSH_SVG("Icons/PCGGraph_16", Icon16x16));
	Set("ClassThumbnail.PCGGraph", new IMAGE_BRUSH_SVG("Icons/PCGGraph_64", Icon64x64));
	Set("ClassIcon.PCGGraphInstance", new IMAGE_BRUSH_SVG("Icons/PCGGraphInstance_16", Icon16x16));
	Set("ClassThumbnail.PCGGraphInstance", new IMAGE_BRUSH_SVG("Icons/PCGGraphInstance_64", Icon64x64));
	Set("ClassIcon.PCGSettings", new IMAGE_BRUSH_SVG("Icons/PCGSettings_16", Icon16x16));
	Set("ClassThumbnail.PCGSettings", new IMAGE_BRUSH_SVG("Icons/PCGSettings_64", Icon64x64));

	// Command icons
	Set("PCG.Command.Find", new IMAGE_BRUSH_SVG("Style/PCG_Command_Find", Icon20x20));
	Set("PCG.Command.ForceRegen", new IMAGE_BRUSH_SVG("Style/PCG_Command_ForceRegen", Icon20x20));
	Set("PCG.Command.ForceRegenClearCache", new IMAGE_BRUSH_SVG("Style/PCG_Command_ForceRegenClearCache", Icon20x20));
	Set("PCG.Command.PauseRegen", new IMAGE_BRUSH_SVG("Style/PCG_Command_PauseRegen", Icon20x20));
	Set("PCG.Command.StopRegen", new IMAGE_BRUSH_SVG("Style/PCG_Command_StopRegen", Icon20x20));
	Set("PCG.Command.GraphSettings", new IMAGE_BRUSH_SVG("Style/PCG_Command_GraphSettings", Icon20x20));
	Set("PCG.Command.OpenDebugTreeTab", new IMAGE_BRUSH_SVG("Style/PCG_Command_OpenDebugTreeTab", Icon20x20));
	Set("PCG.Command.RunDeterminismTest", new IMAGE_BRUSH_SVG("Style/PCG_Command_RunDeterminismTest", Icon20x20));
	
}

const FPCGEditorStyle& FPCGEditorStyle::Get()
{
	static FPCGEditorStyle Instance;
	return Instance;
}
