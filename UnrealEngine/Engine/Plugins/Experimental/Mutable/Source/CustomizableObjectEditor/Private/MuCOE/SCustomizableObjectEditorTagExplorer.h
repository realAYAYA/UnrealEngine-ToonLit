// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

namespace ESelectInfo { enum Type : int; }
template <typename ItemType> class SListView;

class FCustomizableObjectEditor;
class ITableRow;
class STableViewBase;
class SComboButton;
class SWidget;
class UCustomizableObject;
class UCustomizableObjectNode;

class SCustomizableObjectEditorTagExplorer : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectEditorTagExplorer){}
		SLATE_ARGUMENT(FCustomizableObjectEditor*, CustomizableObjectEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** Callback to fill the combobox options */
	TSharedRef<SWidget> OnGetTagsMenuContent();

	/** Fills a list with all the tags found in the nodes of a graph */
	void FillTagInformation(UCustomizableObject* Object, TArray<FString>& Tags);

	/** Generates the combobox with all the tags  */
	TSharedRef<SWidget> MakeComboButtonItemWidget(TSharedPtr<FString> StringItem);

	/** Generates the text of the tags combobox */
	FText GetCurrentItemLabel() const;

	/** Copies the tag name to the clipboard */
	FReply CopyTagToClipboard();

	/** OnSelectionChanged callback of the tags combobox */
	void OnComboBoxSelectionChanged(FString NewValue);

	/** Tags table callbacks*/
	TSharedRef<ITableRow> OnGenerateTableRow(UCustomizableObjectNode* Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnTagTableSelectionChanged(UCustomizableObjectNode* Entry, ESelectInfo::Type SelectInfo) const;

private:

	/** Pointer back to the editor tool that owns us */
	FCustomizableObjectEditor* CustomizableObjectEditorPtr;

	/** Combobox for the Customizable Object Tags */
	TSharedPtr<SComboButton> TagComboBox;

	/** Combobox Selection */
	FString SelectedTag;

	/** List views for the nodes that contain the same tag */
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnMat;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnVar;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnClipMesh;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnClipMorph;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnExtend;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnClipDeform;

	/** Arrays for each type of node that contains a tag */
	TArray<UCustomizableObjectNode*> MaterialNodes;
	TArray<UCustomizableObjectNode*> VariationNodes;
	TArray<UCustomizableObjectNode*> ClipMeshNodes;
	TArray<UCustomizableObjectNode*> ClipMorphNodes;
	TArray<UCustomizableObjectNode*> ExtendNodes;
	TArray<UCustomizableObjectNode*> ClipDeformNodes;

	/** Multimap to relatre the nodes with the tags */
	TMultiMap<FString, UCustomizableObjectNode*> NodeTags;

};
