// Copyright Epic Games, Inc. All Rights Reserved.
/**
* View for holding ControlRig Animation Outliner
*/
#pragma once

#include "CoreMinimal.h"
#include "EditMode/ControlRigBaseDockableView.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Editor/SRigHierarchyTreeView.h"
#include "Widgets/SBoxPanel.h"


class ISequencer;
class SExpandableArea;
class SSearchableRigHierarchyTreeView;
class UControlRig;

class SMultiRigHierarchyTreeView;
class SMultiRigHierarchyItem;
class FMultiRigTreeElement;
class FMultiRigData;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnMultiRigTreeCompareKeys, const FMultiRigData& /*A*/, const FMultiRigData& /*B*/);

typedef STreeView<TSharedPtr<FMultiRigTreeElement>>::FOnSelectionChanged FOnMultiRigTreeSelectionChanged;
typedef STreeView<TSharedPtr<FMultiRigTreeElement>>::FOnMouseButtonClick FOnMultiRigTreeMouseButtonClick;
typedef STreeView<TSharedPtr<FMultiRigTreeElement>>::FOnMouseButtonDoubleClick FOnMultiRigTreeMouseButtonDoubleClick;
typedef STreeView<TSharedPtr<FMultiRigTreeElement>>::FOnSetExpansionRecursive FOnMultiRigTreeSetExpansionRecursive;

uint32 GetTypeHash(const FMultiRigData& Data);

struct CONTROLRIGEDITOR_API FMultiRigTreeDelegates
{
	FOnGetRigTreeDisplaySettings OnGetDisplaySettings;
	FOnMultiRigTreeSelectionChanged OnSelectionChanged;
	FOnContextMenuOpening OnContextMenuOpening;
	FOnMultiRigTreeMouseButtonClick OnMouseButtonClick;
	FOnMultiRigTreeMouseButtonDoubleClick OnMouseButtonDoubleClick;
	FOnMultiRigTreeSetExpansionRecursive OnSetExpansionRecursive;
	FOnRigTreeCompareKeys OnCompareKeys;

	FMultiRigTreeDelegates()
	{
		bIsChangingRigHierarchy = false;
	}


	FORCEINLINE const FRigTreeDisplaySettings& GetDisplaySettings() const
	{
		if (OnGetDisplaySettings.IsBound())
		{
			return OnGetDisplaySettings.Execute();
		}
		return DefaultDisplaySettings;
	}

	FORCEINLINE void HandleSelectionChanged(TSharedPtr<FMultiRigTreeElement> Selection, ESelectInfo::Type SelectInfo)
	{
		if (bIsChangingRigHierarchy)
		{
			return;
		}
		TGuardValue<bool> Guard(bIsChangingRigHierarchy, true);
		OnSelectionChanged.ExecuteIfBound(Selection, SelectInfo);
	}

	static FRigTreeDisplaySettings DefaultDisplaySettings;
	bool bIsChangingRigHierarchy;
};

/** Data for the tree*/
class FMultiRigData
{

public:
	FMultiRigData() {};
	FMultiRigData(UControlRig* InControlRig, FRigElementKey InKey) : ControlRig(InControlRig), Key(InKey) {};
	FText GetName() const;
	FText GetDisplayName() const;
	bool operator == (const FMultiRigData & Other) const;
	bool IsValid() const;
	URigHierarchy* GetHierarchy() const;
public:
	TWeakObjectPtr<UControlRig> ControlRig;
	TOptional<FRigElementKey> Key;
};

/** An item in the tree */
class FMultiRigTreeElement : public TSharedFromThis<FMultiRigTreeElement>
{
public:
	FMultiRigTreeElement(const FMultiRigData& InData, TWeakPtr<SMultiRigHierarchyTreeView> InTreeView,ERigTreeFilterResult InFilterResult);
public:
	/** Element Data to display */
	FMultiRigData Data;
	TArray<TSharedPtr<FMultiRigTreeElement>> Children;

	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FMultiRigTreeElement> InRigTreeElement, TSharedPtr<SMultiRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned);

	void RefreshDisplaySettings(const URigHierarchy* InHierarchy, const FRigTreeDisplaySettings& InSettings);

	/** The current filter result */
	ERigTreeFilterResult FilterResult;

	/** The brush to use when rendering an icon */
	const FSlateBrush* IconBrush;;

	/** The color to use when rendering an icon */
	FSlateColor IconColor;

	/** The color to use when rendering the label text */
	FSlateColor TextColor;
};

class SMultiRigHierarchyItem : public STableRow<TSharedPtr<FMultiRigTreeElement>>
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FMultiRigTreeElement> InRigTreeElement, TSharedPtr<SMultiRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned);
	static TPair<const FSlateBrush*, FSlateColor> GetBrushForElementType(const URigHierarchy* InHierarchy, const FMultiRigData& InData);

private:
	TWeakPtr<FMultiRigTreeElement> WeakRigTreeElement;
	FMultiRigTreeDelegates Delegates;

	FText GetName() const;
	FText GetDisplayName() const;
	FReply OnGetSelectedClicked();

};

class SMultiRigHierarchyTreeView : public STreeView<TSharedPtr<FMultiRigTreeElement>>
{
public:

	SLATE_BEGIN_ARGS(SMultiRigHierarchyTreeView) {}
	SLATE_ARGUMENT(FMultiRigTreeDelegates, RigTreeDelegates)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMultiRigHierarchyTreeView() {}

	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override
	{
		FReply Reply = STreeView<TSharedPtr<FMultiRigTreeElement>>::OnFocusReceived(MyGeometry, InFocusEvent);
		return Reply;
	}

	/** Save a snapshot of the internal map that tracks item expansion before tree reconstruction */
	void SaveAndClearSparseItemInfos()
	{
		// Only save the info if there is something to save (do not overwrite info with an empty map)
		if (!SparseItemInfos.IsEmpty())
		{
			OldSparseItemInfos = SparseItemInfos;
		}
		ClearExpandedItems();
	}

	/** Restore the expansion infos map from the saved snapshot after tree reconstruction */
	void RestoreSparseItemInfos(TSharedPtr<FMultiRigTreeElement> ItemPtr)
	{
		for (const auto& Pair : OldSparseItemInfos)
		{
			if (Pair.Key->Data == ItemPtr->Data)
			{
				// the SparseItemInfos now reference the new element, but keep the same expansion state
				SparseItemInfos.Add(ItemPtr, Pair.Value);
				break;
			}
		}
	}

	static TSharedPtr<FMultiRigTreeElement> FindElement(const FMultiRigData& InData, TSharedPtr<FMultiRigTreeElement> CurrentItem);

	bool AddElement(const FMultiRigData& InData, const FMultiRigData& InParentData);
	bool AddElement(UControlRig* InControlRig, const FRigBaseElement* InElement);
	bool ReparentElement(const FMultiRigData& InData, const FMultiRigData& InParentData);
	bool RemoveElement(const FMultiRigData& InData);
	void RefreshTreeView(bool bRebuildContent = true);
	void SetExpansionRecursive(TSharedPtr<FMultiRigTreeElement> InElement, bool bTowardsParent, bool bShouldBeExpanded);
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FMultiRigTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable, bool bPinned);
	void HandleGetChildrenForTree(TSharedPtr<FMultiRigTreeElement> InItem, TArray<TSharedPtr<FMultiRigTreeElement>>& OutChildren);

	TArray<FMultiRigData> GetSelectedData() const;
	const TArray<TSharedPtr<FMultiRigTreeElement>>& GetRootElements() const { return RootElements; }
	FMultiRigTreeDelegates& GetTreeDelegates() { return Delegates; }

	TSharedPtr<FMultiRigTreeElement> FindItemAtPosition(FVector2D InScreenSpacePosition) const;
	TArray<URigHierarchy*> GetHierarchy() const;
	void SetControlRigs(TArrayView < TWeakObjectPtr<UControlRig>>& InControlRigs);

private:

	/** A temporary snapshot of the SparseItemInfos in STreeView, used during RefreshTreeView() */
	TSparseItemMap OldSparseItemInfos;

	/** Backing array for tree view */
	TArray<TSharedPtr<FMultiRigTreeElement>> RootElements;

	/** A map for looking up items based on their key */
	TMap<FMultiRigData, TSharedPtr<FMultiRigTreeElement>> ElementMap;

	/** A map for looking up a parent based on their key */
	TMap<FMultiRigData, FMultiRigData> ParentMap;

	FMultiRigTreeDelegates Delegates;

	friend class SRigHierarchy;

	TArray <TWeakObjectPtr<UControlRig>> ControlRigs;
};

class SSearchableMultiRigHierarchyTreeView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSearchableMultiRigHierarchyTreeView) {}
	SLATE_ARGUMENT(FMultiRigTreeDelegates, RigTreeDelegates)
		SLATE_ARGUMENT(FText, InitialFilterText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSearchableMultiRigHierarchyTreeView() {}
	TSharedRef<SMultiRigHierarchyTreeView> GetTreeView() const { return TreeView.ToSharedRef(); }
	const FRigTreeDisplaySettings& GetDisplaySettings();

private:

	void OnFilterTextChanged(const FText& SearchText);

	FOnGetRigTreeDisplaySettings SuperGetRigTreeDisplaySettings;
	FText FilterText;
	FRigTreeDisplaySettings Settings;
	TSharedPtr<SMultiRigHierarchyTreeView> TreeView;
};

class SControlRigOutliner : public FControlRigBaseDockableView, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SControlRigOutliner){}
	SLATE_END_ARGS()
	SControlRigOutliner();
	~SControlRigOutliner();

	void Construct(const FArguments& InArgs, FControlRigEditMode& InEditMode);

	//FControlRigBaseDockableView overrides
	virtual void SetEditMode(FControlRigEditMode& InEditMode) override;
private:
	virtual void HandleControlAdded(UControlRig* ControlRig, bool bIsAdded) override;
	virtual void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected) override;

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	const URigHierarchy* GetHierarchy() const;
	void HandleSelectionChanged(TSharedPtr<FMultiRigTreeElement> Selection, ESelectInfo::Type SelectInfo);

	/** Hierarchy picker for controls*/
	TSharedPtr<SSearchableMultiRigHierarchyTreeView> HierarchyTreeView;
	FRigTreeDisplaySettings DisplaySettings;
	const FRigTreeDisplaySettings& GetDisplaySettings() const { return DisplaySettings; }
	bool bIsChangingRigHierarchy = false;
	TSharedPtr<SExpandableArea> PickerExpander;


};


