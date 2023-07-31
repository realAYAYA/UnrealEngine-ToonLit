// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"
#include "Tools/InteractiveToolsCommands.h"


class FMeshPaintEditorModeCommands : public TCommands<FMeshPaintEditorModeCommands>
{
public:
	FMeshPaintEditorModeCommands() : TCommands<FMeshPaintEditorModeCommands>
		(
			"MeshPaint",
			NSLOCTEXT("MeshPaintEditorMode", "MeshPaintingModeCommands", "Mesh Painting Mode"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
			)
	{}

	/**
	* Initialize commands
	*/
	virtual void RegisterCommands() override;

	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands()
	{
		return FMeshPaintEditorModeCommands::Get().Commands;
	}

public:
	TSharedPtr<FUICommandInfo> NextTexture;
	TSharedPtr<FUICommandInfo> PreviousTexture;
	TSharedPtr<FUICommandInfo> CommitTexturePainting;

	TSharedPtr<FUICommandInfo> Copy;
	TSharedPtr<FUICommandInfo> Paste;
	TSharedPtr<FUICommandInfo> Remove;
	TSharedPtr<FUICommandInfo> Fix;
	TSharedPtr<FUICommandInfo> Fill;
	TSharedPtr<FUICommandInfo> Propagate;
	TSharedPtr<FUICommandInfo> Import;
	TSharedPtr<FUICommandInfo> Save;

	TSharedPtr<FUICommandInfo> SwitchForeAndBackgroundColor;
	TSharedPtr<FUICommandInfo> CycleToNextLOD;
	TSharedPtr<FUICommandInfo> CycleToPreviousLOD;

	TSharedPtr<FUICommandInfo> PropagateTexturePaint;
	TSharedPtr<FUICommandInfo> SaveTexturePaint;

	TSharedPtr<FUICommandInfo> PropagateVertexColorsToLODs;

	TSharedPtr<FUICommandInfo> ColorPaint;
	TSharedPtr<FUICommandInfo> WeightPaint;
	TSharedPtr<FUICommandInfo> TexturePaint;
	TSharedPtr<FUICommandInfo> VertexSelect;
	TSharedPtr<FUICommandInfo> TextureSelect;
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};


class FMeshPaintingToolActionCommands : public TInteractiveToolCommands<FMeshPaintingToolActionCommands>
{
public:
	FMeshPaintingToolActionCommands() :
		TInteractiveToolCommands<FMeshPaintingToolActionCommands>(
			"MeshPaintingTools", // Context name for fast lookup
			NSLOCTEXT("MeshPaintEditorMode", "MeshPaintingToolsCommands", "Mesh Painting Tools"), // Localized context name for displaying
			NAME_None, // Parent
			FAppStyle::GetAppStyleSetName() // Icon Style Set
			)
	{
	}

	virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override;


	/**
	 * interface that hides various per-tool action sets
	 */

	 /**
	  * Register all Tool command sets. Call this in module startup
	  */
	static void RegisterAllToolActions();

	/**
	 * Unregister all Tool command sets. Call this from module shutdown.
	 */
	static void UnregisterAllToolActions();

	/**
	 * Add or remove commands relevant to Tool to the given UICommandList.
	 * Call this when the active tool changes (eg on ToolManager.OnToolStarted / OnToolEnded)
	 * @param bUnbind if true, commands are removed, otherwise added
	 */
	static void UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind = false);
};