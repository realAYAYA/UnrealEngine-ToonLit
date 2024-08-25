// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "SkeletalMeshNotifier.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Misc/Change.h"
#include "ReferenceSkeleton.h"

class FTextFilterExpressionEvaluator;
class FUICommandList;
class USkeletonModifier;

namespace ReferenceSkeletonTreeLocals
{

class FSkeletonModifierChange : public FCommandChange
{
public:
	FSkeletonModifierChange(const USkeletonModifier* InModifier);

	virtual FString ToString() const override
	{
		return FString(TEXT("Edit Skeleton"));
	}

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;

	void StoreSkeleton(const USkeletonModifier* InModifier);

private:
	FReferenceSkeleton PreChangeSkeleton;
	TArray<int32> PreBoneTracker;
	FReferenceSkeleton PostChangeSkeleton;
	TArray<int32> PostBoneTracker;
};

}

class FBoneElement : public TSharedFromThis<FBoneElement>
{
public:
	FBoneElement(const FName& InBoneName, TWeakObjectPtr<USkeletonModifier> InModifier);

	FName BoneName;

	bool bIsHidden = false;
	
	TSharedPtr<FBoneElement> UnFilteredParent;
	TSharedPtr<FBoneElement> Parent;
	TArray<TSharedPtr<FBoneElement>> Children;

	void RequestRename() const;
	
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;

	DECLARE_DELEGATE_TwoParams(FOnRenamed, const FName, const FName);
	FOnRenamed OnRenamed;

private:
	TWeakObjectPtr<USkeletonModifier> WeakModifier;
};

using FOnRefSkeletonTreeCanAcceptDrop = STableRow<TSharedPtr<FBoneElement>>::FOnCanAcceptDrop;
using FOnRefSkeletonTreeAcceptDrop = STableRow<TSharedPtr<FBoneElement>>::FOnAcceptDrop;
using FOnBoneRenamed = FBoneElement::FOnRenamed;

struct FRefSkeletonTreeDelegates
{
	FOnDragDetected					OnDragDetected;
	FOnRefSkeletonTreeCanAcceptDrop	OnCanAcceptDrop;
	FOnRefSkeletonTreeAcceptDrop	OnAcceptDrop;
	FOnBoneRenamed					OnBoneRenamed;
	FOnTextCommitted				OnBoneNameCommitted;
};

class SBoneItem : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SBoneItem) {}
		SLATE_ARGUMENT(TWeakPtr<FBoneElement>, TreeElement)
		SLATE_ARGUMENT(TWeakObjectPtr<USkeletonModifier>, WeakModifier)
		SLATE_ARGUMENT(FRefSkeletonTreeDelegates, Delegates)
		SLATE_EVENT( FIsSelected, IsSelected )
	SLATE_END_ARGS()

	// Slate construction function
	void Construct(const FArguments& InArgs);
	
private:

	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage) const;

	static FSlateColor GetTextColor(FIsSelected InIsSelected);
	
	FText GetName() const;
	
	TWeakPtr<FBoneElement> WeakTreeElement;
	TWeakObjectPtr<USkeletonModifier> WeakModifier;
};

class FBoneItemDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FBoneItemDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FBoneItemDragDropOp> New(const TWeakPtr<FBoneElement>& InElement);

	// FDecoratedDragDropOp interface
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	// End FDecoratedDragDropOp interface
	
	TWeakPtr<FBoneElement> Element;
};

class SReferenceSkeletonRow : public SMultiColumnTableRow<TSharedPtr<FBoneElement>>
{
public:
	SLATE_BEGIN_ARGS(SReferenceSkeletonRow) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USkeletonModifier>, WeakModifier)
		SLATE_ARGUMENT(TSharedPtr<FBoneElement>, TreeElement)
		SLATE_ARGUMENT(FRefSkeletonTreeDelegates, Delegates)
	SLATE_END_ARGS()

	// Slate construction function
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);

	// SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;
	// End SMultiColumnTableRow interface

private:
	TWeakPtr<FBoneElement> WeakTreeElement;
	TWeakObjectPtr<USkeletonModifier> WeakModifier;
	FRefSkeletonTreeDelegates Delegates;
};

typedef STreeView< TSharedPtr<FBoneElement> > SRefSkeletonTreeView;

class SReferenceSkeletonTree;

class FReferenceSkeletonWidgetNotifier: public ISkeletalMeshNotifier
{
public:
	FReferenceSkeletonWidgetNotifier(TSharedRef<SReferenceSkeletonTree> InTree);
	virtual void HandleNotification(const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType) override;
	
private:
	TWeakPtr<SReferenceSkeletonTree> Tree;
};

class SReferenceSkeletonTree : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SReferenceSkeletonTree) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USkeletonModifier>, Modifier)
		SLATE_ARGUMENT(FRefSkeletonTreeDelegates, Delegates)
	SLATE_END_ARGS()

	SReferenceSkeletonTree();
    virtual ~SReferenceSkeletonTree() override;

	// Slate construction function
	void Construct(const FArguments& InArgs);

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	
	// Selection
	void GetSelectedBoneNames(TArray<FName>& OutSelectedBoneNames) const;
	TArray<TSharedPtr<FBoneElement>> GetSelectedItems() const;
	bool HasSelectedItems() const;
	void SelectItemFromNames(const TArray<FName>& InBoneNames, bool bFrameSelection = false);

	ISkeletalMeshNotifier& GetNotifier();

private:
	// SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	// END SWidget interface
	
	void BindCommands();

	void AddItemToSelection(const TSharedPtr<FBoneElement>& InItem);
	void RemoveItemFromSelection(const TSharedPtr<FBoneElement>& InItem);
	void ReplaceItemInSelection(const FText& OldName, const FText& NewName);

	// Create, Rename, Delete, Parent bones
	void HandleNewBone();
	bool CanAddNewBone() const;
	void HandleRenameBone() const;
	bool CanRenameBone() const;
	void HandleDeleteBone();
	bool CanDeleteBone() const;
	void HandleUnParentBone();
	bool CanUnParentBone() const;

	// Copy & Paste
	void HandleCopyBones() const;
	bool CanCopyBones() const;
	void HandlePasteBones();
	bool CanPasteBones() const;
	void HandleDuplicateBones();
	bool CanDuplicateBones() const;

	// Filter
	void OnFilterTextChanged(const FText& SearchText);
	
	// Callbacks
	void RefreshTreeView(bool IsInitialSetup=false);
	void HandleGetChildrenForTree(TSharedPtr<FBoneElement> InItem, TArray<TSharedPtr<FBoneElement>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FBoneElement> InItem, ESelectInfo::Type InSelectInfo);
	
	TSharedRef< SWidget > CreateAddNewMenu();
	TSharedPtr< SWidget > CreateContextMenu();

	void OnItemDoubleClicked(TSharedPtr<FBoneElement> InItem);
	void OnSetExpansionRecursive(TSharedPtr<FBoneElement> InItem, bool bShouldBeExpanded);
	void SetExpansionRecursive(TSharedPtr<FBoneElement> InElement, bool bTowardsParent, bool bShouldBeExpanded);

	// Delegates
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FBoneElement> TargetItem);
	FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FBoneElement> TargetItem);
	void OnBoneRenamed(const FName InOldName, const FName InNewName) const;
	
	void OnNewBoneNameCommitted(const FText& InText, ETextCommit::Type InCommitType);

	// The modifier that actually does modifications on the reference skeleton
	TWeakObjectPtr<USkeletonModifier> Modifier;
	
	TUniquePtr<FReferenceSkeletonWidgetNotifier> Notifier;

	// undo
	void BeginChange();
	void EndChange();
	void CancelChange();
	TUniquePtr<ReferenceSkeletonTreeLocals::FSkeletonModifierChange> ActiveChange;
	
	// Commands
	TSharedPtr<FUICommandList> CommandList;
	
	// Tree view
	TSharedPtr<SRefSkeletonTreeView> TreeView;
	TArray<TSharedPtr<FBoneElement>> RootElements;
	TArray<TSharedPtr<FBoneElement>> AllElements;

	// filter
	TSharedPtr<FTextFilterExpressionEvaluator> TextFilter;

	friend class FReferenceSkeletonWidgetNotifier;
};

