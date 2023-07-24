// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorStyle.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

const FName FChaosClothAssetEditorStyle::StyleName("ClothStyle");

FString FChaosClothAssetEditorStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ChaosClothAssetEditor"))->GetContentDir();
	FString Path = (ContentDir / RelativePath) + Extension;
	return Path;
}

FChaosClothAssetEditorStyle::FChaosClothAssetEditorStyle()
	: FSlateStyleSet(StyleName)
{
	// Used FUVEditorStyle and FModelingToolsEditorModeStyle and as models

	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	// TODO: Set this once we get our own UI content
	FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ModelingToolsEditorMode"))->GetContentDir();
	SetContentRoot(ContentDir);
	
	// Some standard icon sizes used elsewhere in the editor
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon28x28(28.0f, 28.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon120(120.0f, 120.0f);

	// Icon sizes used in this style set
	const FVector2D ViewportToolbarIconSize = Icon16x16;
	const FVector2D ToolbarIconSize = Icon20x20;

	FString PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::BeginRemeshToolIdentifier;
	Set(*PropertyNameString, new FSlateImageBrush(FChaosClothAssetEditorStyle::InContent("Icons/Remesh_40x", ".png"), ToolbarIconSize ));
	
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::BeginAttributeEditorToolIdentifier;
	Set(*PropertyNameString, new FSlateImageBrush(FChaosClothAssetEditorStyle::InContent("Icons/AttributeEditor_40x", ".png"), ToolbarIconSize));

	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::BeginWeightMapPaintToolIdentifier;
	Set(*PropertyNameString, new FSlateImageBrush(FChaosClothAssetEditorStyle::InContent("Icons/ModelingAttributePaint_x40", ".png"), ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::BeginClothTrainingToolIdentifier;
	Set(*PropertyNameString, new FSlateImageBrush(FChaosClothAssetEditorStyle::InContent("Icons/ModelingAttributePaint_x40", ".png"), ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::BeginTransferSkinWeightsToolIdentifier;
	Set(*PropertyNameString, new FSlateImageBrush(FChaosClothAssetEditorStyle::InContent("Icons/ModelingAttributePaint_x40", ".png"), ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::TogglePatternModeIdentifier;
	Set(*PropertyNameString, new FSlateImageBrush(FChaosClothAssetEditorStyle::InContent("Icons/TogglePatternMode_40x", ".png"), ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::ToggleSimMeshWireframeIdentifier;
	Set(*PropertyNameString, new CORE_IMAGE_BRUSH_SVG("Starship/EditorViewport/grid", ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::ToggleRenderMeshWireframeIdentifier;
	Set(*PropertyNameString, new CORE_IMAGE_BRUSH_SVG("Starship/EditorViewport/grid", ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	// TODO: Toggle back and forth between Play and Pause icons when the button is clicked?
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::ToggleSimulationSuspendedIdentifier;
	Set(*PropertyNameString, new CORE_IMAGE_BRUSH_SVG("Starship/MainToolbar/pause", ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::SoftResetSimulationIdentifier;
	Set(*PropertyNameString, new CORE_IMAGE_BRUSH_SVG("Starship/Common/Reset", ToolbarIconSize));

	// TODO: Get a permanent icon to replace this placeholder
	PropertyNameString = "ChaosClothAssetEditor." + FChaosClothAssetEditorCommands::HardResetSimulationIdentifier;
	Set(*PropertyNameString, new CORE_IMAGE_BRUSH_SVG("Starship/Common/Apply", ToolbarIconSize));
	
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FChaosClothAssetEditorStyle::~FChaosClothAssetEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FChaosClothAssetEditorStyle& FChaosClothAssetEditorStyle::Get()
{
	static FChaosClothAssetEditorStyle Inst;
	return Inst;
}


