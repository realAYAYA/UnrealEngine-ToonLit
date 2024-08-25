// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Constraint.h"
#include "EditorUndoClient.h"
#include "IStructureDetailsView.h"
#include "BakingAnimationKeySettings.h"
#include "Misc/QualifiedFrameTime.h"
#include "ConstraintsManager.h"
#include "Misc/NotifyHook.h"

class UTickableTransformConstraint;
class AActor;
class SConstraintsCreationWidget;
class SConstraintsEditionWidget;
class ISequencer;
DECLARE_DELEGATE(FOnConstraintCreated);

/**
 * FConstraintInfo
 */

class FConstraintInfo
{
public:
	static const FSlateBrush* GetBrush(uint8 InType);
	static int8 GetType(UClass* InClass);
	static UTickableTransformConstraint* GetMutable(ETransformConstraintType InType);
	static UTickableTransformConstraint* GetConfigurable(ETransformConstraintType InType);
	
private:
	static const TArray< const FSlateBrush* >& GetBrushes();
	static const TMap< UClass*, ETransformConstraintType >& GetConstraintToType();
	static const TArray< UTickableTransformConstraint* >& GetMutableDefaults();
	static const TArray< TFunction<UTickableTransformConstraint*()> >& GetConfigurableConstraints();
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
 * SConstraintMenuEntry
 */

class SConstraintMenuEntry : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SConstraintMenuEntry){}

		SLATE_EVENT(FOnConstraintCreated, OnConstraintCreated)
	
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs and the actual tree item. */
	void Construct(
		const FArguments& InArgs,
		const ETransformConstraintType& InType);

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// End of SWidget interface

	// FNotifyHook interface
	/** NotifyPostChange is used to copy the property value from the configurable constraint to the cdo. */
	virtual void NotifyPostChange( const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged ) override;
	
private:

	/** Activates the ActorPickerMode to pick a parent in the viewport. */
	FReply CreateSelectionPicker(const bool bUseDefault) const;

	/** Delegate triggered when a new constraint has been created. */
	FOnConstraintCreated OnConstraintCreated;

	/** Creates a widget to edit the constraint default properties. */
	TSharedRef<SWidget> GenerateConstraintDefaultWidget();
	
	/** Creates the constraint between the current selection and the picked actor. */
	static void CreateConstraint(
		AActor* InParent,
		FOnConstraintCreated InDelegate,
		const ETransformConstraintType InConstraintType,
		const bool bUseDefault);

	/** TSharedPtr to the tree item. */
	TSharedPtr<const FDroppableConstraintItem> ConstraintItem;

	ETransformConstraintType ConstraintType = ETransformConstraintType::Parent;

	/** State on the item. */
	bool bIsPressed = false;

	/** object ptr to the configurable constraint. */
	TObjectPtr<UTickableConstraint> ConfigurableConstraint = nullptr;
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
		UTickableConstraint* InConstraint,
		ETransformConstraintType InType)
	{
		return MakeShareable(new FEditableConstraintItem(InConstraint, InType));
	}
	
	TWeakObjectPtr<UTickableConstraint> Constraint = nullptr;
	ETransformConstraintType Type = ETransformConstraintType::Parent;

	FName GetName() const
	{
		if (Constraint.IsValid())
		{
			return Constraint->GetFName();
		}
		return NAME_None;
	}
	FString GetLabel() const
	{
		if (Constraint.IsValid())
		{
			return Constraint->GetLabel();
		}
		return FString();
	}

private:
	FEditableConstraintItem(UTickableConstraint* InConstraint, ETransformConstraintType InType)
		: Constraint(InConstraint)
		, Type(InType)
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
* Constraint List Shared by Edit Widget and Bake Widget
*/

class CONTROLRIGEDITOR_API FBaseConstraintListWidget : public FEditorUndoClient
{
public:

	/**
	*  Constraints to show in Widget
	*/
	enum class EShowConstraints
	{
		ShowSelected = 0x0,
		ShowLevelSequence = 0x1,
		ShowValid = 0x2,
		ShowAll = 0x3,
	};
public:

	virtual ~FBaseConstraintListWidget() override;
	/* FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess);
	virtual void PostRedo(bool bSuccess);
	/* End FEditorUndoClient interface */

	/** Invalidates the constraint list for further rebuild. */
	virtual void InvalidateConstraintList();

	/** Rebuild the constraint list based on the current selection. */
	virtual int32 RefreshConstraintList();

	/** Triggers a constraint list invalidation when selection in the level viewport. */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	/** Which constraints to show  */
	EShowConstraints GetShowConstraints() const { return ShowConstraints; }
	void SetShowConstraints(EShowConstraints InShowConstraints)
	{
		if (InShowConstraints != ShowConstraints)
		{
			ShowConstraints = InShowConstraints;
			InvalidateConstraintList();
		}
	}
	FText GetShowConstraintsText(EShowConstraints Index) const;
	FText GetShowConstraintsTooltip(EShowConstraints Index) const;

protected:
	/** Types */
	using ItemSharedPtr = TSharedPtr<FEditableConstraintItem>;
	using ConstraintItemListView = SListView< ItemSharedPtr >;


	void RegisterSelectionChanged();
	void UnregisterSelectionChanged();

	/** List view that shows constraint types*/
	TSharedPtr< ConstraintItemListView > ListView;

	/** List of constraint types */
	TArray< ItemSharedPtr > ListItems;

	/** Boolean used to handle the items list refresh. */
	bool bNeedsRefresh = false;

	FDelegateHandle OnSelectionChangedHandle;

public:

	static EShowConstraints ShowConstraints;

};

/**
 * SConstraintsEditionWidget
 */

class CONTROLRIGEDITOR_API SConstraintsEditionWidget : public SCompoundWidget, public FBaseConstraintListWidget
{
public:
	SLATE_BEGIN_ARGS(SConstraintsEditionWidget)	{}
	SLATE_END_ARGS()
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Override for SWidget::Tick */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/**  */
	bool CanMoveUp(const TSharedPtr<FEditableConstraintItem>& Item) const;
	bool CanMoveDown(const TSharedPtr<FEditableConstraintItem>& Item) const;

	/**  */
	void MoveItemUp(const TSharedPtr<FEditableConstraintItem>& Item);
	void MoveItemDown(const TSharedPtr<FEditableConstraintItem>& Item);

	/**  */
	void RemoveItem(const TSharedPtr<FEditableConstraintItem>& Item);

	// FBaseConstraintListWidget overrides
	virtual void InvalidateConstraintList() override;

	/** Notify from sequencer changed. */
	void SequencerChanged(const TWeakPtr<ISequencer>& InNewSequencer);
	
private:

	/** Generates a widget for the specified item */
	TSharedRef<ITableRow> OnGenerateWidgetForItem(ItemSharedPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Types */
	using ItemSharedPtr = TSharedPtr<FEditableConstraintItem>;
	using ConstraintItemListView = SListView< ItemSharedPtr >;

	/** @todo documentation. */
	TSharedPtr< SWidget > CreateContextMenu();
	void OnItemDoubleClicked(ItemSharedPtr InItem);

	FReply OnBakeClicked();

	void UpdateSequencer();

	//sequencer and it's time
	TWeakPtr<ISequencer> WeakSequencer;
	bool SequencerTimeChanged();
	FQualifiedFrameTime SequencerLastTime;

};

/** Widget allowing baking of constraints */
class CONTROLRIGEDITOR_API SConstraintBakeWidget : public SCompoundWidget, public FBaseConstraintListWidget
{
public:

	SLATE_BEGIN_ARGS(SConstraintBakeWidget)
		: _Sequencer(nullptr)
	{}

	SLATE_ARGUMENT(TSharedPtr<ISequencer>, Sequencer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SConstraintBakeWidget() override {}

	FReply OpenDialog(bool bModal = true);
	void CloseDialog();

	//SWidget overrides
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	//FBaseConstraintListWidget overrides
	virtual int32 RefreshConstraintList() override;

private:
	void BakeSelected();

	/** Generates a widget for the specified item */
	TSharedRef<ITableRow> OnGenerateWidgetForItem(ItemSharedPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<ISequencer> Sequencer;
	//static to be reused
	static TOptional<FBakingAnimationKeySettings> BakeConstraintSettings;
	//structonscope for details panel
	TSharedPtr < TStructOnScope<FBakingAnimationKeySettings>> Settings;
	TWeakPtr<SWindow> DialogWindow;
	TSharedPtr<IStructureDetailsView> DetailsView;

};

/**
 * SBakeConstraintItem
 */

class SBakeConstraintItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBakeConstraintItem) {}
	SLATE_END_ARGS()

		/** Constructs this widget with InArgs and the actual tree item. */
		void Construct(
			const FArguments& InArgs,
			const TSharedPtr<FEditableConstraintItem>& InItem,
			TSharedPtr<SConstraintBakeWidget> InConstraintsWidget);

private:
	/** TSharedPtr to the tree item. */
	TSharedPtr<FEditableConstraintItem> ConstraintItem;
	TWeakPtr<SConstraintBakeWidget> ConstraintsWidget;
};