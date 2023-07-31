// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FExtender;
class FToolBarBuilder;
class FDMXPixelMappingToolkit;
class SWidget;

/**
 * Custom Toolbar for DMX Pixel Mapping Editor
 */
class FDMXPixelMappingToolbar :
	public TSharedFromThis<FDMXPixelMappingToolbar>
{
public:
	/** Default Constructor */
	FDMXPixelMappingToolbar(TSharedPtr<FDMXPixelMappingToolkit> InToolkit);

	/** Virtual Destructor */
	virtual ~FDMXPixelMappingToolbar() {}

	/** Builds the toolbar */
	void BuildToolbar(TSharedPtr<FExtender> Extender);

private:
	/** Callback, raised when the menu extender requests to build the toolbar */
	void BuildToolbarCallback(FToolBarBuilder& ToolbarBuilder);

	/** Generates a widget with play options (play, stop) */
	TSharedRef<SWidget> GeneratesPlayOptionsWidget();

	/** Generates a layout settings menu for the pixelmapping toolbar */
	TSharedRef<SWidget> GenerateLayoutMenu();

public:
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;
};

