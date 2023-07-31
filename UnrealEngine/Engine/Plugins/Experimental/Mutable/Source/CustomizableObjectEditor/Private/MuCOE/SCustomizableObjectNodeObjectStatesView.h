// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Misc/Optional.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class SComboButton;
class SImage;
class SSearchBox;
class STableViewBase;
class SVerticalBox;
class SWidget;
class UCustomizableObjectNodeObject;
struct FSlateBrush;
template <typename OptionType> class SComboBox;

class SCustomizableObjectRuntimeParameter : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectRuntimeParameter) {}
	SLATE_ARGUMENT(class UCustomizableObjectNodeObject*, Node)
	SLATE_ARGUMENT(int32, StateIndex)
	SLATE_ARGUMENT(int32, RuntimeParameterIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** Creates the content of the Combo Button */
	TSharedRef<SWidget> GetComboButtonContent();

	/** Generates the text of the combobox option */
	FText GetCurrentItemLabel() const;

	/** Callback for the Combo Button selection */
	void OnComboButtonSelectionChanged(TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo);

	/** Generates the lables of the list view for the combo button */
	TSharedRef<ITableRow> RowNameComboButtonGenarateWidget(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Generate the Combo Button selected label */
	FText GetRowNameComboButtonContentText() const;

	/** Callback for the OnTextChanged of the Searchbox */
	void OnSearchBoxFilterTextChanged(const FText& InText);

	/** Callback for the OnTextCommited of the Searchbox */
	void OnSearchBoxFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	// Returns true if the item should be visible in the Combo button
	bool IsItemVisible(TSharedPtr<FString> Item);

private:

	/** Node with all the information */
	class UCustomizableObjectNodeObject* Node;

	/** Index to identify which parameter this widget is modifying */
	int32 StateIndex;

	/** Index to identify which parameter this widget is modifying */
	int32 RuntimeParameterIndex;

	/** Combobox for runtime parameter options */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> RuntimeParamCombobox;

	/** Options shown in the ListView widget */
	TArray<TSharedPtr<FString>> ListViewOptions;

	/** ComboButton Selection */
	TSharedPtr<FString> ComboButtonSelection;

	/** ComboButton Widget */
	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr< SListView< TSharedPtr< FString >>> RowNameComboListView;

	/** Search box of the ComboButton */
	TSharedPtr<SSearchBox> SearchBoxWidget;

	/** Stores the input of the Searchboc widget*/
	FString SearchItem;
};


/* Widget that represents List of Runtime Parameters Widgets */

class SCustomizableObjectRuntimeParameterList : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectRuntimeParameterList) {}
	SLATE_ARGUMENT(class UCustomizableObjectNodeObject*, Node)
	SLATE_ARGUMENT(int32, StateIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void BuildList();

	/** Removes a Runtime parameter and rebuilds the runtimeparameter widgets */
	FReply OnDeleteRuntimeParameter(int32 ParameterIndex);

	bool IsCollapsed() { return bCollapsed; }
	void SetCollapsed(bool Collapse ) { bCollapsed = Collapse; }
	
private:

	/** Node with all the information */
	class UCustomizableObjectNodeObject* Node;

	/** Index to identify which parameter this widget is modifying */
	int32 StateIndex;

	/** Bool that determines if a RuntimeParameter should be collapsed or not*/
	bool bCollapsed;

	/** Vertical box widget for the RuntimeParameter Widgets  */
	TSharedPtr<SVerticalBox> VerticalSlots;
};

class SCustomizableObjectNodeObjectSatesView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectNodeObjectSatesView) {}
	SLATE_ARGUMENT(UCustomizableObjectNodeObject*, Node)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Callback for the collapsing arrow checkbox */
	void OnCollapseChanged(const ECheckBoxState NewCheckedState, int32 StateIndex);

	/** Gets if a runtime parameter widget is collapsed */
	EVisibility GetCollapsed(int32 StateIndex);

	/** Return the brush for the collapsed arrow */
	const FSlateBrush* GetExpressionPreviewArrow(int32 StateIndex) const;

	/** Adds a new runtime parameter and rebuild the runtimeparameterListWidgets */
	FReply OnAddRuntimeParameterPressed(int32 StateIndex);

private:

	/** Pointer to the current Node */
	UCustomizableObjectNodeObject* Node;

	/** Vertical Boxes to store the widget of each state */
	TSharedPtr<SVerticalBox> VerticalSlots;

	/** Arrays to edit widgets when needed */
	TArray<TSharedPtr<SCustomizableObjectRuntimeParameterList>> RuntimeParametersListWidgets;

	/** Array of SImages to control the collapsing buttons */
	TArray<TSharedPtr<SImage>> CollapsedArrows;

};

