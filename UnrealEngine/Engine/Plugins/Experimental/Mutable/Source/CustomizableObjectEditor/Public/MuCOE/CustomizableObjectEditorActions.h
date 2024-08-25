// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

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
	TSharedPtr< FUICommandInfo > CompileOptions_UseDiskCompilation;
	TSharedPtr< FUICommandInfo > Debug;

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
	
	TSharedPtr< FUICommandInfo > SetDrawUVs;
	TSharedPtr< FUICommandInfo > SetShowGrid;
	TSharedPtr< FUICommandInfo > SetShowSky;
	TSharedPtr< FUICommandInfo > SetShowBounds;
	TSharedPtr< FUICommandInfo > SetShowCollision;
	TSharedPtr< FUICommandInfo > SetCameraLock;
	TSharedPtr< FUICommandInfo > SaveThumbnail;

	TSharedPtr< FUICommandInfo > BakeInstance;
	TSharedPtr< FUICommandInfo > StateChangeShowData;
	TSharedPtr< FUICommandInfo > StateChangeShowGeometryData;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

};
