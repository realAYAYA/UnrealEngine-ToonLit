// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorCommands.h"

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

	const static FString BeginAttributeEditorToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginAttributeEditorTool;

	const static FString BeginClothTrainingToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginClothTrainingTool;

	const static FString BeginTransferSkinWeightsToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginTransferSkinWeightsTool;

	// Rest space viewport commands
	const static FString TogglePatternModeIdentifier;
	TSharedPtr<FUICommandInfo> TogglePatternMode;

	// Sim viewport commands
	const static FString ToggleSimMeshWireframeIdentifier;
	TSharedPtr<FUICommandInfo> ToggleSimMeshWireframe;

	const static FString ToggleRenderMeshWireframeIdentifier;
	TSharedPtr<FUICommandInfo> ToggleRenderMeshWireframe;


	const static FString SoftResetSimulationIdentifier;
	TSharedPtr<FUICommandInfo> SoftResetSimulation;

	const static FString HardResetSimulationIdentifier;
	TSharedPtr<FUICommandInfo> HardResetSimulation;

	const static FString ToggleSimulationSuspendedIdentifier;
	TSharedPtr<FUICommandInfo> ToggleSimulationSuspended;

};
