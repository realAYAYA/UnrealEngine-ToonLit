// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Constraint.h"
#include "EditorUndoClient.h"

class AActor;
class SConstraintsCreationWidget;
class SConstraintsEditionWidget;
class UTickableConstraint;

DECLARE_DELEGATE(FOnConstraintCreated);

/**
 * FConstraintInfo
 */

class FConstraintInfo
{
public:
	static const FSlateBrush* GetBrush(uint8 InType);
	static int8 GetType(UClass* InClass);
private:
	static const TArray< const FSlateBrush* >& GetBrushes();
	static const TMap< UClass*, ETransformConstraintType >& GetConstraintToType();
};

/**
 * The classes below are used to implement a constraint creation widget.
 *
 * It represents the different constraints type that can be created between two objects. At this stage, it only
 * transform constraints (as described in EConstType but can basically represents any type  of constraint.
 * The resulting widget is a tree filled with drag & droppable items that can be dropped on actors.
 * The selection represents the child and the picked actor will be the parent of the constraint. 
 */

/**
 * FDroppableConstraintItem
 */

struct FDroppableConstraintItem
{
	static TSharedRef<FDroppableConstraintItem> Make(ETransformConstraintType InType)
	{
		return MakeShareable(new FDroppableConstraintItem(InType));
	}
	
	ETransformConstraintType Type = ETransformConstraintType::Parent;

private:
	FDroppableConstraintItem(ETransformConstraintType InType)
		: Type(InType)
	{}
	FDroppableConstraintItem() = default;
};

/**
 * SDroppableConstraintItem
 */

class SDroppableConstraintItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDroppableConstraintItem){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs and the actual tree item. */
	void Construct(
		const FArguments& InArgs,
		const TSharedPtr<const FDroppableConstraintItem>& InItem,
		TSharedPtr<SConstraintsCreationWidget> InConstraintsWidget);

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// End of SWidget interface
	
private:

	/** todo documentation */
	FReply CreateSelectionPicker() const;
	
	/** Creates the constraint between the current selection and the picked actor. */
	static void CreateConstraint(
		AActor* InParent,
		FOnConstraintCreated InDelegate,
		const ETransformConstraintType InConstraintType);

	/** TSharedPtr to the tree item. */
	TSharedPtr<const FDroppableConstraintItem> ConstraintItem;

	ETransformConstraintType ConstraintType = ETransformConstraintType::Parent;

	/** State on the item. */
	bool bIsPressed = false;

	TWeakPtr<SConstraintsCreationWidget> ConstraintsWidget;
};

/**
 * SConstraintsCreationWidget
 */

class CONTROLRIGEDITOR_API SConstraintsCreationWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SConstraintsCreationWidget)	{}

		SLATE_EVENT(FOnConstraintCreated, OnConstraintCreated)
	
	SLATE_END_ARGS()

	FOnConstraintCreated OnConstraintCreated;
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	/** Types */
	using ItemSharedPtr = TSharedPtr<FDroppableConstraintItem>;
	using ConstraintItemListView = SListView< ItemSharedPtr >;

	/** Generates a widget for the specified item */
	TSharedRef<ITableRow> OnGenerateWidgetForItem(ItemSharedPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);
	
	/** List view that shows constraint types*/
	TSharedPtr< ConstraintItemListView > ListView;

	/** Static list of constraint types */
	static TArray< ItemSharedPtr > ListItems;
};

/**
 * The classes below are used to display a list of constraints. 
 */

/**
 * FEditableConstraintItem
 */

class FEditableConstraintItem
{
public:
	static TSharedRef<FEditableConstraintItem> Make(
		const FName& InName,
		ETransformConstraintType InType,
		const FString& InLabel)
	{
		return MakeShareable(new FEditableConstraintItem(InName, InType, InLabel));
	}
	
	FName Name = NAME_None;
	ETransformConstraintType Type = ETransformConstraintType::Parent;
	FString Label;

private:
	FEditableConstraintItem(const FName& InName, ETransformConstraintType InType, const FString& InLabel)
		: Name(InName)
		, Type(InType)
		, Label(InLabel)
	{}
	FEditableConstraintItem() {}
};

/**
 * SEditableConstraintItem
 */

class SEditableConstraintItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditableConstraintItem){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs and the actual tree item. */
	void Construct(
		const FArguments& InArgs,
		const TSharedPtr<FEditableConstraintItem>& InItem,
		TSharedPtr<SConstraintsEditionWidget> InConstraintsWidget);
	
private:
	/** TSharedPtr to the tree item. */
	TSharedPtr<FEditableConstraintItem> ConstraintItem;
	TWeakPtr<SConstraintsEditionWidget> ConstraintsWidget;
};

/**
 * SConstraintsEditionWidget
 */

class CONTROLRIGEDITOR_API SConstraintsEditionWidget : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SConstraintsEditionWidget)	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	virtual ~SConstraintsEditionWidget() override;

	/** Override for SWidget::Tick */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Invalidates the constraint list for further rebuild. */
	void InvalidateConstraintList();
	
	/** Rebuild the constraint list based on the current selection. */
	int32 RefreshConstraintList();

	/** Triggers a constraint list invalidation when selection in the level viewport. */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	/**  */
	bool CanMoveUp(const TSharedPtr<FEditableConstraintItem>& Item) const;
	bool CanMoveDown(const TSharedPtr<FEditableConstraintItem>& Item) const;

	/**  */
	void MoveItemUp(const TSharedPtr<FEditableConstraintItem>& Item);
	void MoveItemDown(const TSharedPtr<FEditableConstraintItem>& Item);

	/**  */
	void RemoveItem(const TSharedPtr<FEditableConstraintItem>& Item);

	/* FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess);
	virtual void PostRedo(bool bSuccess);
	/* End FEditorUndoClient interface */

private:
	/** Types */
	using ItemSharedPtr = TSharedPtr<FEditableConstraintItem>;
	using ConstraintItemListView = SListView< ItemSharedPtr >;

	/** Generates a widget for the specified item */
	TSharedRef<ITableRow> OnGenerateWidgetForItem(ItemSharedPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** @todo documentation. */
	TSharedPtr< SWidget > CreateContextMenu();
	void OnItemDoubleClicked(ItemSharedPtr InItem);
	
	void RegisterSelectionChanged();
	void UnregisterSelectionChanged();
	
	/** List view that shows constraint types*/
	TSharedPtr< ConstraintItemListView > ListView;

	/** List of constraint types */
	TArray< ItemSharedPtr > ListItems;

	/** Boolean used to handle the items list refresh. */
	bool bNeedsRefresh = false;

	FDelegateHandle OnSelectionChangedHandle;
};
