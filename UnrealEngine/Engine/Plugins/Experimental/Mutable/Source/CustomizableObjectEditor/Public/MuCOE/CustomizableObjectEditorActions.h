// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;

/**
 * 
 */
class FCustomizableObjectEditorCommands : public TCommands<FCustomizableObjectEditorCommands>
{

public:
	FCustomizableObjectEditorCommands();
	
	TSharedPtr< FUICommandInfo > Compile;
	TSharedPtr< FUICommandInfo > CompileOnlySelected;
	TSharedPtr< FUICommandInfo > ResetCompileOptions;
	TSharedPtr< FUICommandInfo > CompileOptions_EnableTextureCompression;
	TSharedPtr< FUICommandInfo > CompileOptions_UseParallelCompilation;
	TSharedPtr< FUICommandInfo > CompileOptions_UseDiskCompilation;

	TSharedPtr< FUICommandInfo > Debug;
	TSharedPtr< FUICommandInfo > DebugOptions_OnlySelected;
	TSharedPtr< FUICommandInfo > DebugOptions_EnableTextureCompression;

	TSharedPtr< FUICommandInfo > PerformanceReport;
	TSharedPtr< FUICommandInfo > ResetPerformanceReportOptions;
	TSharedPtr< FUICommandInfo > TextureAnalyzer;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

};



/**
 * 
 */
class FCustomizableObjectEditorViewportCommands : public TCommands<FCustomizableObjectEditorViewportCommands>
{

public:
	FCustomizableObjectEditorViewportCommands();
	
	/**  */
	TSharedPtr< FUICommandInfo > SetDrawUVs;
	TSharedPtr< FUICommandInfo > SetShowGrid;
	TSharedPtr< FUICommandInfo > SetShowSky;
	TSharedPtr< FUICommandInfo > SetShowBounds;
	TSharedPtr< FUICommandInfo > SetShowCollision;
	TSharedPtr< FUICommandInfo > SetCameraLock;
	TSharedPtr< FUICommandInfo > SaveThumbnail;

	// View Menu Commands
	TSharedPtr< FUICommandInfo > SetShowNormals;
	TSharedPtr< FUICommandInfo > SetShowTangents;
	TSharedPtr< FUICommandInfo > SetShowBinormals;
	TSharedPtr< FUICommandInfo > SetShowPivot;

	TSharedPtr< FUICommandInfo > BakeInstance;
	TSharedPtr< FUICommandInfo > StateChangeTest;
	TSharedPtr< FUICommandInfo > StateChangeShowData;
	TSharedPtr< FUICommandInfo > StateChangeShowGeometryData;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

};
