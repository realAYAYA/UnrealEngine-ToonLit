// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"

class FString;
class IDetailLayoutBuilder;
class STextComboBox;
class FReply;
class SButton;
class SCustomizableObjectNodeLayoutBlocksEditor;

/** Copy Material node details panel. Hides all properties from the inheret Material node. */
class FCustomizableObjectNodeTableDetails : public IDetailCustomization
{
public:
	
	static TSharedRef<IDetailCustomization> MakeInstance();

	// ILayoutDetails interface
	/** Do not use. Add details customization in the other CustomizeDetails signature. */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override {};

	/** Hides details copied from CustomizableObjectNodeMaterial. */
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;

private:

	// Generates Mesh columns combobox options
	void GenerateMeshColumnComboBoxOptions();

	// Generates Animation Instance combobox options
	void GenerateAnimInstanceComboBoxOptions();

	// Generates Animation Slot combobox options
	void GenerateAnimSlotComboBoxOptions();

	// Generates Animation Tags combobox options
	void GenerateAnimTagsComboBoxOptions();

	// OnComboBoxSelectionChanged Callback for Column ComboBox
	void OnColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// OnComboBoxSelectionChanged Callback for Layout ComboBox
	void OnAnimMeshColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// OnComboBoxSelectionChanged Callback for Layout ComboBox
	void OnLayoutMeshColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// OnComboBoxSelectionChanged Callback for AnimInstance ComboBox
	void OnAnimInstanceComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// OnComboBoxSelectionChanged Callback for Anim Slot ComboBox
	void OnAnimSlotComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// OnComboBoxSelectionChanged Callback for Anim Tags ComboBox
	void OnAnimTagsComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// Function called when the table node has been refreshed
	void OnNodePinValueChanged();

	// Callback to clear the animation combobox selections
	FReply OnClearButtonPressed();

private:

	// Pointer to the node represented in this details
	class UCustomizableObjectNodeTable* Node;

	// ComboBox widget to select a column from the NodeTable
	TSharedPtr<STextComboBox> LayoutMeshColumnComboBox;

	// ComboBox widget to select a column from the NodeTable
	TSharedPtr<STextComboBox> AnimMeshColumnComboBox;

	// Array with the name of the table columns as combobox options
	TArray<TSharedPtr<FString>> AnimMeshColumnOptionNames;

	// Array with the name of the table columns as combobox options
	TArray<TSharedPtr<FString>> LayoutMeshColumnOptionNames;

	// ComboBox widget to select an Animation Instance column from the NodeTable
	TSharedPtr<STextComboBox> AnimComboBox;

	// Array with the name of the Animation Instance columns as combobox options
	TArray<TSharedPtr<FString>> AnimOptionNames;

	// ComboBox widget to select an Animation Slot column from the NodeTable
	TSharedPtr<STextComboBox> AnimSlotComboBox;

	// Array with the name of the Animation Slot columns as combobox options
	TArray<TSharedPtr<FString>> AnimSlotOptionNames;

	// ComboBox widget to select an Animation Tags column from the NodeTable
	TSharedPtr<STextComboBox> AnimTagsComboBox;

	// Array with the name of the Animation Tags columns as combobox options
	TArray<TSharedPtr<FString>> AnimTagsOptionNames;
	
	// Button to clear selections of the animation comboboxes
	TSharedPtr<SButton> ClearButton;

	// Layout block editor widget
	TSharedPtr<SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	// Pointer to the Detail Builder to force the refresh on recontruct the node
	TWeakPtr<IDetailLayoutBuilder> DetailBuilderPtr = nullptr;

};