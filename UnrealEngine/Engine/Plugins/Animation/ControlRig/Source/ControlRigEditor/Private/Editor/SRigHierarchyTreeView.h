// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STreeView.h"
#include "Rigs/RigHierarchy.h"
#include "SRigHierarchyTagWidget.h"

class SSearchBox;
class SRigHierarchyTreeView;
class SRigHierarchyItem;
class FRigTreeElement;

struct CONTROLRIGEDITOR_API FRigTreeDisplaySettings
{
	FRigTreeDisplaySettings()
	{
		FilterText = FText();

		bFlattenHierarchyOnFilter = false;
		bHideParentsOnFilter = false;
		bUseShortName = true;
		bShowImportedBones = true;
		bShowBones = true;
		bShowControls = true;
		bShowNulls = true;
		bShowRigidBodies = true;
		bShowReferences = true;
		bShowSockets = true;
		bShowConnectors = true;
		bShowIconColors = true;
	}
	
	FText FilterText;

	/** Flatten when text filtering is active */
	bool bFlattenHierarchyOnFilter;

	/** Hide parents when text filtering is active */
	bool bHideParentsOnFilter;

	/** When true, the elements will show their short name */
	bool bUseShortName;

	/** Whether or not to show imported bones in the hierarchy */
	bool bShowImportedBones;

	/** Whether or not to show bones in the hierarchy */
	bool bShowBones;

	/** Whether or not to show controls in the hierarchy */
	bool bShowControls;

	/** Whether or not to show spaces in the hierarchy */
	bool bShowNulls;

	/** Whether or not to show rigidbodies in the hierarchy */
	bool bShowRigidBodies;

	/** Whether or not to show references in the hierarchy */
	bool bShowReferences;

	/** Whether or not to show sockets in the hierarchy */
	bool bShowSockets;

	/** Whether or not to show connectors in the hierarchy */
	bool bShowConnectors;
	
	/** Whether to tint the icons with the element color */
	bool bShowIconColors;
};

DECLARE_DELEGATE_RetVal(const URigHierarchy*, FOnGetRigTreeHierarchy);
DECLARE_DELEGATE_RetVal(const FRigTreeDisplaySettings&, FOnGetRigTreeDisplaySettings);
DECLARE_DELEGATE_RetVal(const TArray<FRigElementKey>, FOnRigTreeGetSelection);
DECLARE_DELEGATE_RetVal_TwoParams(FName, FOnRigTreeRenameElement, const FRigElementKey& /*OldKey*/, const FString& /*NewName*/);
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnRigTreeVerifyElementNameChanged, const FRigElementKey& /*OldKey*/, const FString& /*NewName*/, FText& /*OutErrorMessage*/);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnRigTreeCompareKeys, const FRigElementKey& /*A*/, const FRigElementKey& /*B*/);
DECLARE_DELEGATE_RetVal_OneParam(FRigElementKey, FOnRigTreeGetResolvedKey, const FRigElementKey&);
DECLARE_DELEGATE_OneParam(FOnRigTreeRequestDetailsInspection, const FRigElementKey&);
DECLARE_DELEGATE_RetVal_OneParam(TOptional<FText>, FOnRigTreeItemGetToolTip, const FRigElementKey&);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnRigTreeIsItemVisible, const FRigElementKey&);

typedef STableRow<TSharedPtr<FRigTreeElement>>::FOnCanAcceptDrop FOnRigTreeCanAcceptDrop;
typedef STableRow<TSharedPtr<FRigTreeElement>>::FOnAcceptDrop FOnRigTreeAcceptDrop;
typedef STreeView<TSharedPtr<FRigTreeElement>>::FOnSelectionChanged FOnRigTreeSelectionChanged;
typedef STreeView<TSharedPtr<FRigTreeElement>>::FOnMouseButtonClick FOnRigTreeMouseButtonClick;
typedef STreeView<TSharedPtr<FRigTreeElement>>::FOnMouseButtonDoubleClick FOnRigTreeMouseButtonDoubleClick;
typedef STreeView<TSharedPtr<FRigTreeElement>>::FOnSetExpansionRecursive FOnRigTreeSetExpansionRecursive;

struct CONTROLRIGEDITOR_API FRigTreeDelegates
{
	FOnGetRigTreeHierarchy OnGetHierarchy;
	FOnGetRigTreeDisplaySettings OnGetDisplaySettings;
	FOnRigTreeRenameElement OnRenameElement;
	FOnRigTreeVerifyElementNameChanged OnVerifyElementNameChanged;
	FOnDragDetected OnDragDetected;
	FOnRigTreeCanAcceptDrop OnCanAcceptDrop;
	FOnRigTreeAcceptDrop OnAcceptDrop;
	FOnRigTreeGetSelection OnGetSelection;
	FOnRigTreeSelectionChanged OnSelectionChanged;
	FOnContextMenuOpening OnContextMenuOpening;
	FOnRigTreeMouseButtonClick OnMouseButtonClick;
	FOnRigTreeMouseButtonDoubleClick OnMouseButtonDoubleClick;
	FOnRigTreeSetExpansionRecursive OnSetExpansionRecursive;
	FOnRigTreeCompareKeys OnCompareKeys;
	FOnRigTreeGetResolvedKey OnGetResolvedKey;
	FOnRigTreeRequestDetailsInspection OnRequestDetailsInspection;
	FOnRigTreeElementKeyTagDragDetected OnRigTreeElementKeyTagDragDetected;
	FOnRigTreeItemGetToolTip OnRigTreeGetItemToolTip;
	FOnRigTreeIsItemVisible OnRigTreeIsItemVisible;

	FRigTreeDelegates()
	{
		bIsChangingRigHierarchy = false;
	}

	const URigHierarchy* GetHierarchy() const
	{
		if(OnGetHierarchy.IsBound())
		{
			return OnGetHierarchy.Execute();
		}
		return nullptr;
	}

	const FRigTreeDisplaySettings& GetDisplaySettings() const
	{
		if(OnGetDisplaySettings.IsBound())
		{
			return OnGetDisplaySettings.Execute();
		}
		return DefaultDisplaySettings;
	}

	TArray<FRigElementKey> GetSelection() const
	{
		if (OnGetSelection.IsBound())
		{
			return OnGetSelection.Execute();
		}
		if (const URigHierarchy* Hierarchy = GetHierarchy())
		{
			return Hierarchy->GetSelectedKeys();
		}
		return {};
	}
	
	FName HandleRenameElement(const FRigElementKey& OldKey, const FString& NewName) const
	{
		if(OnRenameElement.IsBound())
		{
			return OnRenameElement.Execute(OldKey, NewName);
		}
		return OldKey.Name;
	}
	
	bool HandleVerifyElementNameChanged(const FRigElementKey& OldKey, const FString& NewName, FText& OutErrorMessage) const
	{
		if(OnVerifyElementNameChanged.IsBound())
		{
			return OnVerifyElementNameChanged.Execute(OldKey, NewName, OutErrorMessage);
		}
		return false;
	}

	void HandleSelectionChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo)
	{
		if(bIsChangingRigHierarchy)
		{
			return;
		}
		TGuardValue<bool> Guard(bIsChangingRigHierarchy, true);
		OnSelectionChanged.ExecuteIfBound(Selection, SelectInfo);
	}

	FRigElementKey GetResolvedKey(const FRigElementKey& InKey)
	{
		if(OnGetResolvedKey.IsBound())
		{
			return OnGetResolvedKey.Execute(InKey);
		}
		return InKey;
	}

	void RequestDetailsInspection(const FRigElementKey& InKey)
	{
		if(OnRequestDetailsInspection.IsBound())
		{
			return OnRequestDetailsInspection.Execute(InKey);
		}
	}

	static FRigTreeDisplaySettings DefaultDisplaySettings;
	bool bIsChangingRigHierarchy;
};

/** 
 * Order is important here! 
 * This enum is used internally to the filtering logic and represents an ordering of most filtered (hidden) to 
 * least filtered (highlighted).
 */
enum class ERigTreeFilterResult : int32
{
	/** Hide the item */
	Hidden,

	/** Show the item because child items were shown */
	ShownDescendant,

	/** Show the item */
	Shown,
};

/** An item in the tree */
class FRigTreeElement : public TSharedFromThis<FRigTreeElement>
{
public:
	FRigTreeElement(const FRigElementKey& InKey, TWeakPtr<SRigHierarchyTreeView> InTreeView, bool InSupportsRename, ERigTreeFilterResult InFilterResult);
public:
	/** Element Data to display */
	FRigElementKey Key;
	FName ShortName;
	FName ChannelName;
	bool bIsTransient;
	bool bIsAnimationChannel;
	bool bIsProcedural;
	bool bSupportsRename;
	TArray<TSharedPtr<FRigTreeElement>> Children;

	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigTreeElement> InRigTreeElement, TSharedPtr<SRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned);

	void RequestRename();

	void RefreshDisplaySettings(const URigHierarchy* InHierarchy, const FRigTreeDisplaySettings& InSettings);

	FSlateColor GetIconColor() const;
	FSlateColor GetTextColor() const;

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;

	/** The current filter result */
	ERigTreeFilterResult FilterResult;

	/** The brush to use when rendering an icon */
	const FSlateBrush* IconBrush;;

	/** The color to use when rendering an icon */
	FSlateColor IconColor;

	/** The color to use when rendering the label text */
	FSlateColor TextColor;

	/** If true the item is filtered out during a drag */
	bool bFadedOutDuringDragDrop;

	/** The tag arguments for this element */
	TArray<SRigHierarchyTagWidget::FArguments> Tags;
};

class SRigHierarchyItem : public STableRow<TSharedPtr<FRigTreeElement>>
{
public:
	
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigTreeElement> InRigTreeElement, TSharedPtr<SRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned);
 	void OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const;
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);
	static TPair<const FSlateBrush*, FSlateColor> GetBrushForElementType(const URigHierarchy* InHierarchy, const FRigElementKey& InKey);
	static FLinearColor GetColorForControlType(ERigControlType InControlType, UEnum* InControlEnum);

private:
	TWeakPtr<FRigTreeElement> WeakRigTreeElement;
 	FRigTreeDelegates Delegates;

	FText GetNameForUI() const;
	FText GetName(bool bUseShortName) const;
	FText GetItemTooltip() const;

	friend class SRigHierarchyTreeView; 
};

class SRigHierarchyTreeView : public STreeView<TSharedPtr<FRigTreeElement>>
{
public:

	SLATE_BEGIN_ARGS(SRigHierarchyTreeView)
		: _AutoScrollEnabled(false)
	{}
		SLATE_ARGUMENT(FRigTreeDelegates, RigTreeDelegates)
		SLATE_ARGUMENT(bool, AutoScrollEnabled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRigHierarchyTreeView() {}

	/** Performs auto scroll */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override
	{
		FReply Reply = STreeView<TSharedPtr<FRigTreeElement>>::OnFocusReceived(MyGeometry, InFocusEvent);

		LastClickCycles = FPlatformTime::Cycles();

		return Reply;
	}

	uint32 LastClickCycles = 0;

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
	void RestoreSparseItemInfos(TSharedPtr<FRigTreeElement> ItemPtr)
	{
		for (const auto& Pair : OldSparseItemInfos)
		{
			if (Pair.Key->Key == ItemPtr->Key)
			{
				// the SparseItemInfos now reference the new element, but keep the same expansion state
				SparseItemInfos.Add(ItemPtr, Pair.Value);
				break;
			}
		}
	}

	static TSharedPtr<FRigTreeElement> FindElement(const FRigElementKey& InElementKey, TSharedPtr<FRigTreeElement> CurrentItem);
	bool AddElement(FRigElementKey InKey, FRigElementKey InParentKey = FRigElementKey());
	bool AddElement(const FRigBaseElement* InElement);
	void AddSpacerElement();
	bool ReparentElement(FRigElementKey InKey, FRigElementKey InParentKey);
	bool RemoveElement(FRigElementKey InKey);
	void RefreshTreeView(bool bRebuildContent = true);
	void SetExpansionRecursive(TSharedPtr<FRigTreeElement> InElement, bool bTowardsParent, bool bShouldBeExpanded);
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FRigTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable, bool bPinned);
	void HandleGetChildrenForTree(TSharedPtr<FRigTreeElement> InItem, TArray<TSharedPtr<FRigTreeElement>>& OutChildren);
	void OnElementKeyTagDragDetected(const FRigElementKey& InDraggedTag);

	TArray<FRigElementKey> GetSelectedKeys() const;
	const TArray<TSharedPtr<FRigTreeElement>>& GetRootElements() const { return RootElements; }
	FRigTreeDelegates& GetRigTreeDelegates() { return Delegates; }

	/** Given a position, return the item under that position. If nothing is there, return null. */
	const TSharedPtr<FRigTreeElement>* FindItemAtPosition(FVector2D InScreenSpacePosition) const;

private:

	void AddConnectorResolveWarningTag(TSharedPtr<FRigTreeElement> InTreeElement, const FRigBaseElement* InRigElement, const URigHierarchy* InHierarchy);
	FText GetConnectorWarningMessage(TSharedPtr<FRigTreeElement> InTreeElement, TWeakObjectPtr<UControlRig> InControlRigPtr, const FRigElementKey InConnectorKey) const;

	/** A temporary snapshot of the SparseItemInfos in STreeView, used during RefreshTreeView() */
	TSparseItemMap OldSparseItemInfos;

	/** Backing array for tree view */
	TArray<TSharedPtr<FRigTreeElement>> RootElements;
	
	/** A map for looking up items based on their key */
	TMap<FRigElementKey, TSharedPtr<FRigTreeElement>> ElementMap;

	/** A map for looking up a parent based on their key */
	TMap<FRigElementKey, FRigElementKey> ParentMap;

	FRigTreeDelegates Delegates;

	bool bAutoScrollEnabled;
	FVector2D LastMousePosition;
	double TimeAtMousePosition;

	friend class SRigHierarchy;
};

class SSearchableRigHierarchyTreeView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSearchableRigHierarchyTreeView) {}
		SLATE_ARGUMENT(FRigTreeDelegates, RigTreeDelegates)
		SLATE_ARGUMENT(FText, InitialFilterText)
		SLATE_ARGUMENT(float, MaxHeight)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSearchableRigHierarchyTreeView() {}
	TSharedRef<SSearchBox> GetSearchBox() const { return SearchBox.ToSharedRef(); }
	TSharedRef<SRigHierarchyTreeView> GetTreeView() const { return TreeView.ToSharedRef(); }
	const FRigTreeDisplaySettings& GetDisplaySettings();

private:

	void OnFilterTextChanged(const FText& SearchText);

	FOnGetRigTreeDisplaySettings SuperGetRigTreeDisplaySettings;
	FText FilterText;
	FRigTreeDisplaySettings Settings;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SRigHierarchyTreeView> TreeView;
	float MaxHeight = 0.f;
};
