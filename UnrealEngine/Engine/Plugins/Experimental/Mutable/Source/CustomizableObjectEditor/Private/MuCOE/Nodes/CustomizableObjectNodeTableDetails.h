// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

#include "UObject/WeakObjectPtr.h"

namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class FReply;
class SButton;
class SCustomizableObjectNodeLayoutBlocksEditor;
class STextBlock;
class STextComboBox;
class UCustomizableObjectLayout;
class UCustomizableObjectNodeTable;
struct EVisibility;
struct FSlateColor;

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

	// Generates MutableMetadata columns combobox options
	// Returns the current selected option or a null pointer
	TSharedPtr<FString> GenerateMutableMetaDataColumnComboBoxOptions();

	// Generates MutableMetadata columns combobox options
	// Returns the current selected option or a null pointer
	TSharedPtr<FString> GenerateVersionColumnComboBoxOptions();

	// Callback to regenerate the combobox options
	void OnOpenMutableMetadataComboBox();

	// OnComboBoxSelectionChanged Callback for Layout ComboBox
	void OnMutableMetaDataColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	// Sets the combo box selection color
	FSlateColor GetMetadataUIComboBoxTextColor(TArray<TSharedPtr<FString>>* CurrentOptions) const;

	// OnComboBoxSelectionChanged Callback for Layout ComboBox
	void OnMutableMetaDataColumnComboBoxSelectionReset();

	// Callback to regenerate the combobox options
	void OnOpenVersionColumnComboBox();
	
	// OnComboBoxSelectionChanged Callback for Layout ComboBox
	void OnVersionColumnComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	
	// Sets the combo box selection color
	FSlateColor GetVersionColumnComboBoxTextColor(TArray<TSharedPtr<FString>>* CurrentOptions) const;
	
	// OnComboBoxSelectionChanged Callback for Layout ComboBox
	void OnVersionColumnComboBoxSelectionReset();

	// Layout options visibility
	EVisibility LayoutOptionsVisibility() const;

	/** Returns the visibility of the Fixed layout widgets */
	EVisibility FixedStrategyOptionsVisibility() const;

	/** Fills the combo box arrays sources */
	void FillLayoutComboBoxOptions();

	/** Layout Options Callbacks */
	void OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

private:

	// Pointer to the node represented in this details
	TWeakObjectPtr<UCustomizableObjectNodeTable> Node;

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
	
	// Array with the name of the MutableMetaData columns
	TArray<TSharedPtr<FString>> MutableMetaDataColumnsOptionNames;

	// ComboBox widget to select a MutableMetaDatacolumn from the NodeTable
	TSharedPtr<STextComboBox> MutableMetaDataComboBox;

	// Array with the name of the Version columns
	TArray<TSharedPtr<FString>> VersionColumnsOptionNames;
	
	// ComboBox widget to select a VersionColumn from the NodeTable
	TSharedPtr<STextComboBox> VersionColumnsComboBox;
	
	// Button to clear selections of the animation comboboxes
	TSharedPtr<SButton> ClearButton;

	// Layout block editor widget
	TSharedPtr<SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	TWeakObjectPtr<UCustomizableObjectLayout> SelectedLayout;

	// Pointer to the Detail Builder to force the refresh on recontruct the node
	TWeakPtr<IDetailLayoutBuilder> DetailBuilderPtr = nullptr;

	/** List of available layout grid sizes. */
	TArray< TSharedPtr< FString > > LayoutGridSizes;

	/** List of available layout packing strategies. */
	TArray< TSharedPtr< FString > > LayoutPackingStrategies;

	/** List of available block reduction methods. */
	TArray< TSharedPtr< FString > > BlockReductionMethods;

	// ComboBox widget to select a Grid Size from the Selected Layout
	TSharedPtr<STextComboBox> GridSizeComboBox;
	// ComboBox widget to select a Strategy from the Selected Layout
	TSharedPtr<STextComboBox> StrategyComboBox;
	// ComboBox widget to select a Max Grid Size from the Selected Layout
	TSharedPtr<STextComboBox> MaxGridSizeComboBox;
	// ComboBox widget to select a Reduction Method from the Selected Layout
	TSharedPtr<STextComboBox> ReductionMethodComboBox;
};
