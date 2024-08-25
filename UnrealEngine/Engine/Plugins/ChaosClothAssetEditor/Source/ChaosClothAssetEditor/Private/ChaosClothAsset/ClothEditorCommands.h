// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorCommands.h"

namespace UE::Chaos::ClothAsset
{
class CHAOSCLOTHASSETEDITOR_API FChaosClothAssetEditorCommands : public TBaseCharacterFXEditorCommands<FChaosClothAssetEditorCommands>
{
public:

	FChaosClothAssetEditorCommands();

	// TBaseCharacterFXEditorCommands<> interface
	virtual void RegisterCommands() override;

	// TInteractiveToolCommands<>
	virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override;

	/**
	 * Add or remove commands relevant to Tool to the given UICommandList.
	 * Call this when the active tool changes (eg on ToolManager.OnToolStarted / OnToolEnded)
	 * @param bUnbind if true, commands are removed, otherwise added
	 */
	static void UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind = false);


public:

	TSharedPtr<FUICommandInfo> OpenClothEditor;

	const static FString BeginRemeshToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginRemeshTool;

	const static FString BeginWeightMapPaintToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginWeightMapPaintTool;
	const static FString AddWeightMapNodeIdentifier;
	TSharedPtr<FUICommandInfo> AddWeightMapNode;

	const static FString BeginAttributeEditorToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginAttributeEditorTool;

	const static FString BeginTransferSkinWeightsToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginTransferSkinWeightsTool;
	const static FString AddTransferSkinWeightsNodeIdentifier;
	TSharedPtr<FUICommandInfo> AddTransferSkinWeightsNode;

	const static FString BeginMeshSelectionToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginMeshSelectionTool;
	const static FString AddMeshSelectionNodeIdentifier;
	TSharedPtr<FUICommandInfo> AddMeshSelectionNode;

	// Construction viewport commands
	const static FString ToggleConstructionViewWireframeIdentifier;
	TSharedPtr<FUICommandInfo> ToggleConstructionViewWireframe;

	const static FString ToggleConstructionViewSeamsIdentifier;
	TSharedPtr<FUICommandInfo> ToggleConstructionViewSeams;

	const static FString ToggleConstructionViewSeamsCollapseIdentifier;
	TSharedPtr<FUICommandInfo> ToggleConstructionViewSeamsCollapse;

	TSharedPtr<FUICommandInfo> TogglePatternColor;
	TSharedPtr<FUICommandInfo> ToggleMeshStats;

	TSharedPtr<FUICommandInfo> SetConstructionMode2D;
	TSharedPtr<FUICommandInfo> SetConstructionMode3D;
	TSharedPtr<FUICommandInfo> SetConstructionModeRender;

	// Preview viewport commands
	const static FString TogglePreviewWireframeIdentifier;
	TSharedPtr<FUICommandInfo> TogglePreviewWireframe;

	const static FString SoftResetSimulationIdentifier;
	TSharedPtr<FUICommandInfo> SoftResetSimulation;

	const static FString HardResetSimulationIdentifier;
	TSharedPtr<FUICommandInfo> HardResetSimulation;

	const static FString ToggleSimulationSuspendedIdentifier;
	TSharedPtr<FUICommandInfo> ToggleSimulationSuspended;

	TSharedPtr<FUICommandInfo> LODAuto;
	TSharedPtr<FUICommandInfo> LOD0;
};
} // namespace UE::Chaos::ClothAsset
