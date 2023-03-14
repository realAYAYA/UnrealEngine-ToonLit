// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXPixelMappingRuntimeCommon.h"

#include "Layout/ArrangedWidget.h"

class FDMXPixelMappingToolkit;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMapping;
class FDMXPixelMappingToolkit;
class FDMXPixelMappingComponentReference;
class FMenuBuilder;

class FDragDropEvent;
class SWidget;

/**
 * Shared Pixel Mapping editor functions
 */
class FDMXPixelMappingEditorUtils
{
public:
	/**
	 * Check ability to rename the component.
	 *
	 * @param InToolkit				Pixel Mappint editor toolkit
	 * @param InComponent			Component reference to rename
	 * @param NewName				New name to check
	 * @param OutErrorMessage		Output parameter in case of failed reaming
	 *
	 * @return true if the component can be renamed
	 */
	static bool VerifyComponentRename(TSharedRef<FDMXPixelMappingToolkit> InToolkit, const FDMXPixelMappingComponentReference& InComponent, const FText& NewName, FText& OutErrorMessage);

	/**
	 * Rename Pixel Maping component.
	 *
	 * @param InToolkit				Pixel Mappint editor toolkit
	 * @param NewName				Old name
	 * @param NewName				New name
	 */
	static void RenameComponent(TSharedRef<FDMXPixelMappingToolkit> InToolkit, const FName& OldObjectName, const FString& NewDisplayName);

	/**
	 * Add renderer to Pixel Mapping object
	 *
	 * @param InDMXPixelMapping		Pixel Mapping object
	 *
	 * @return Render Component pointer
	 */
	static UDMXPixelMappingRendererComponent* AddRenderer(UDMXPixelMapping* InPixelMapping);

	/**
	 * Create components commands menu
	 *
	 * @param MenuBuilder			Vertical menu builder
	 * @param InToolkit				Pixel Mappint editor toolkit
	 */
	static void CreateComponentContextMenu(FMenuBuilder& MenuBuilder, TSharedRef<FDMXPixelMappingToolkit> InToolkit);

	/**
	 * Returns the arranged widget of a widget.
	 *
	 * @param InWidget				The widget to get the arranged widget from
	 * @param OutArrangedWidget		The arranged widget
	 * 
	 * @return						True if the arranged widget could be acquired
	 */
	static bool GetArrangedWidget(TSharedRef<SWidget> InWidget, FArrangedWidget& OutArrangedWidget);

	/**
	 * Returns the target component from a drag drop event
	 *
	 * @param WeakToolkit			The toolkit in use
	 * @param DragDropEvent			The DragDropEvent to consider.
	 * 
	 * @return						The component that is target of the drag drop op
	 */
	static UDMXPixelMappingBaseComponent* GetTargetComponentFromDragDropEvent(const TWeakPtr<FDMXPixelMappingToolkit>& WeakToolkit, const FDragDropEvent& DragDropEvent);
};
