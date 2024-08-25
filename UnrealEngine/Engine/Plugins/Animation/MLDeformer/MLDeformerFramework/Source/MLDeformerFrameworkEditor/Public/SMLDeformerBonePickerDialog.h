// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Dialog/SCustomDialog.h"
#include "Framework/Commands/Commands.h"
#include "MLDeformerEditorStyle.h"

struct FReferenceSkeleton;

namespace UE::MLDeformer
{
	class SMLDeformerBonePickerTreeWidget;
	class SMLDeformerBonePickerDialog;
	class FMLDeformerEditorModel;


	class FMLDeformerBonePickerTreeElement
		: public TSharedFromThis<FMLDeformerBonePickerTreeElement>
	{
	public:
		TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FMLDeformerBonePickerTreeElement> InTreeElement, TSharedPtr<SMLDeformerBonePickerTreeWidget> InTreeWidget);

	public:
		TArray<TSharedPtr<FMLDeformerBonePickerTreeElement>> Children;
		FName Name;
		FSlateColor TextColor;
	};


	class SMLDeformerBonePickerTreeRowWidget 
		: public STableRow<TSharedPtr<FMLDeformerBonePickerTreeElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FMLDeformerBonePickerTreeElement> InTreeElement, TSharedPtr<SMLDeformerBonePickerTreeWidget> InTreeView);

	private:
		TWeakPtr<FMLDeformerBonePickerTreeElement> WeakTreeElement;
		FText GetName() const;

		friend class SMLDeformerBonePickerTreeWidget; 
	};


	class SMLDeformerBonePickerTreeWidget
		: public STreeView<TSharedPtr<FMLDeformerBonePickerTreeElement>>
	{
		SLATE_BEGIN_ARGS(SMLDeformerBonePickerTreeWidget) {}
		SLATE_ARGUMENT(bool, AllowMultiSelect)
		SLATE_ARGUMENT(TSharedPtr<SMLDeformerBonePickerDialog>, PickerWidget)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs);
		const TArray<TSharedPtr<FMLDeformerBonePickerTreeElement>>& GetRootElements() const	{ return RootElements; }
		void AddElement(TSharedPtr<FMLDeformerBonePickerTreeElement> Element, TSharedPtr<FMLDeformerBonePickerTreeElement> ParentElement);
		void Clear();

	private:
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FMLDeformerBonePickerTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		void HandleGetChildrenForTree(TSharedPtr<FMLDeformerBonePickerTreeElement> InItem, TArray<TSharedPtr<FMLDeformerBonePickerTreeElement>>& OutChildren);
		void OnMouseDoubleClicked(TSharedPtr<FMLDeformerBonePickerTreeElement> ClickedItem);

	private:
		TArray<TSharedPtr<FMLDeformerBonePickerTreeElement>> RootElements;
		TSharedPtr<SMLDeformerBonePickerDialog> PickerWidget;
	};


	/**
	 * A bone picker dialog window.
	 * This allows users to select one or more bones.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerBonePickerDialog
		: public SCustomDialog
	{
	public:
		friend class SMLDeformerBonePickerTreeWidget;

		SLATE_BEGIN_ARGS(SMLDeformerBonePickerDialog) {}
		SLATE_ARGUMENT(FReferenceSkeleton*, RefSkeleton)		// The reference skeleton to grab the bone names and hierarchy from.
		SLATE_ARGUMENT(TArray<FName>, HighlightBoneNames)		// Highlight bones in this list.
		SLATE_ARGUMENT(TArray<FName>, InitialSelectedBoneNames)	// Select all bones in this list.
		SLATE_ARGUMENT(FSlateColor, HighlightBoneNamesColor)	// Bones inside the HighlightBoneNames list will use this color.
		SLATE_ARGUMENT(TArray<FName>, IncludeList)				// If empty, all bones can be picked. If not empty, only show bones with names inside this list.
		SLATE_ARGUMENT(FText, Title)							// The window title.
		SLATE_ARGUMENT(bool, AllowMultiSelect)					// Allow multi select, or only allow to pick one bone?
		SLATE_ARGUMENT(TSharedPtr<SWidget>, ExtraWidget)		// An optional extra widget shown on top, next to the search field.
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/**
		 * Get the array of picked bone names.
		 * This will be an empty list when nothing got selected, which means we basically cancelled.
		 * @return The array of selected bone names. Empty if cancelled or if no bones were there to begin with.
		 */
		const TArray<FName>& GetPickedBoneNames() const;

	private:
		TSharedRef<SMLDeformerBonePickerTreeWidget> CreateBoneTree();
		FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		void OnOkClicked();
		void OnCancelClicked();
		void OnFilterTextChanged(const FText& InFilterText);
		void RefreshBoneTreeElements();
		void SelectInitialItems();
		void SelectInitialItemsRecursive(const TSharedPtr<FMLDeformerBonePickerTreeElement>& Item);

	private:
		TSharedPtr<SMLDeformerBonePickerTreeWidget> BoneTreeWidget;
		TSharedPtr<SWidget> ExtraWidget;
		TArray<FName> HighlightBoneNames;
		TArray<FName> InitialSelectedBoneNames;
		TArray<FName> IncludeList;
		FReferenceSkeleton* RefSkeleton = nullptr;
		TArray<FName> PickedBoneNames;
		FSlateColor HighlightColor;
		bool bAllowMultiSelect = true;
		FString FilterText;	
	};
}
