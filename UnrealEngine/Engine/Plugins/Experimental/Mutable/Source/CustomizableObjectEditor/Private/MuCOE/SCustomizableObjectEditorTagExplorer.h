// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FCustomizableObjectEditor;
class ITableRow;
class STableViewBase;
class SWidget;
class UCustomizableObject;

class SCustomizableObjectEditorTagExplorer : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectEditorTagExplorer){}
		SLATE_ARGUMENT(FCustomizableObjectEditor*, CustomizableObjectEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** Button Callback to fill the tag list */
	FReply GetCustomizableObjectTags();

	/** Fills a list with all the tags found in the nodes of a graph */
	void FillTagInformation(UCustomizableObject* Object, TArray<FString>& Tags);

	/** Generates the combobox with all the tags  */
	TSharedRef<SWidget> MakeComboButtonItemWidget(TSharedPtr<FString> StringItem);

	/** Generates the text of the tags combobox */
	FText GetCurrentItemLabel() const;

	/** Copies the tag name to the clipboard */
	FReply CopyTagToClipboard();

	/** OnSelectionChanged callback of the tags combobox */
	void OnComboBoxSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type);

	/** Tags table callbacks*/
	TSharedRef<ITableRow> OnGenerateTableRow(UCustomizableObjectNode* Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnTagTableSelectionChanged(UCustomizableObjectNode* Entry, ESelectInfo::Type SelectInfo) const;

private:

	/** Pointer back to the editor tool that owns us */
	FCustomizableObjectEditor* CustomizableObjectEditorPtr;

	/** Combobox for the Customizable Object Tags */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TagComboBox;

	/** Combobox string container */
	TArray<TSharedPtr<FString>> ComboboxTags;

	/** Combobox Selection */
	TSharedPtr<FString> TagComboBoxItem;

	/** List views for the nodes that contain the same tag */
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnMat;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnVar;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnClipMesh;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnClipMorph;
	TSharedPtr<SListView<UCustomizableObjectNode*>> ColumnExtend;

	/** Arrays for each type of node that contains a tag */
	TArray<UCustomizableObjectNode*> MaterialNodes;
	TArray<UCustomizableObjectNode*> VariationNodes;
	TArray<UCustomizableObjectNode*> ClipMeshNodes;
	TArray<UCustomizableObjectNode*> ClipMorphNodes;
	TArray<UCustomizableObjectNode*> ExtendNodes;

	/** Multimap to relatre the nodes with the tags */
	TMultiMap<FString, UCustomizableObjectNode*> NodeTags;

};