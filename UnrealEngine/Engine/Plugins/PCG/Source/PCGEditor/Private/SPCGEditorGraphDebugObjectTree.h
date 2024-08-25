// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Graph/PCGStackContext.h"

#include "ToolMenus.h"
#include "GameFramework/Actor.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FPCGEditorGraphDebugObjectItem;
class FPCGEditor;

typedef TSharedPtr<FPCGEditorGraphDebugObjectItem> FPCGEditorGraphDebugObjectItemPtr;

class FPCGEditorGraphDebugObjectItem : public TSharedFromThis<FPCGEditorGraphDebugObjectItem>
{
public:
	FPCGEditorGraphDebugObjectItem() = default;

	explicit FPCGEditorGraphDebugObjectItem(bool bInGrayedOut)
		: bGrayedOut(bInGrayedOut)
	{
	}
	
	virtual ~FPCGEditorGraphDebugObjectItem() = default;

	void AddChild(TSharedRef<FPCGEditorGraphDebugObjectItem> InChild);
	const TSet<TSharedPtr<FPCGEditorGraphDebugObjectItem>>& GetChildren() const;
	FPCGEditorGraphDebugObjectItemPtr GetParent() const;
	void SortChildren(bool bIsAscending, bool bIsRecursive);

	bool IsExpanded() const { return bIsExpanded; }
	void SetExpanded(bool bInIsExpanded) { bIsExpanded = bInIsExpanded; }

	bool IsGrayedOut() const { return bGrayedOut; }

	/** Optional sort priority, if returns INDEX_NONE then sort will fall back to alphabetical. */
	virtual int32 GetSortPriority() const { return INDEX_NONE; }

	/** Whether this item represents a currently debuggable object for the current edited graph. */
	virtual bool IsDebuggable() const { return false; }

	virtual FString GetLabel() const = 0;
	virtual UPCGComponent* GetPCGComponent() const = 0;
	virtual const FPCGStack* GetPCGStack() const { return nullptr; }
	virtual const UObject* GetObject() const  = 0;
	virtual const UPCGGraph* GetPCGGraph() const { return nullptr; }
	virtual bool IsLoopIteration() const { return false; }

protected:
	TWeakPtr<FPCGEditorGraphDebugObjectItem> Parent;
	TSet<TSharedPtr<FPCGEditorGraphDebugObjectItem>> Children;
	bool bIsExpanded = false;
	bool bGrayedOut = false;
};

class FPCGEditorGraphDebugObjectItem_Actor : public FPCGEditorGraphDebugObjectItem
{
public:
	explicit FPCGEditorGraphDebugObjectItem_Actor(TWeakObjectPtr<AActor> InActor, bool bInHasInspectionData)
		: FPCGEditorGraphDebugObjectItem(bInHasInspectionData)
		, Actor(InActor)
	{
		PCGStack.PushFrame(InActor.Get());
	}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return nullptr; }
	virtual const UObject* GetObject() const override { return Actor.Get(); };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }

protected:
	TWeakObjectPtr<AActor> Actor = nullptr;

	FPCGStack PCGStack;
};

class FPCGEditorGraphDebugObjectItem_PCGComponent : public FPCGEditorGraphDebugObjectItem
{
public:
	explicit FPCGEditorGraphDebugObjectItem_PCGComponent(
		TWeakObjectPtr<UPCGComponent> InPCGComponent,
		TWeakObjectPtr<const UPCGGraph> InPCGGraph,
		const FPCGStack& InPCGStack,
		bool bInIsDebuggable,
		bool bInHasInspectionData)
		: FPCGEditorGraphDebugObjectItem(bInHasInspectionData)
		, PCGComponent(InPCGComponent)
		, PCGGraph(InPCGGraph)
		, PCGStack(InPCGStack)
		, bIsDebuggable(bInIsDebuggable)
	{}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return PCGComponent.Get(); }
	virtual const UObject* GetObject() const override { return PCGComponent.Get(); };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }
	virtual bool IsDebuggable() const override { return bIsDebuggable; }
	virtual const UPCGGraph* GetPCGGraph() const override { return PCGGraph.Get(); }

protected:
	TWeakObjectPtr<UPCGComponent> PCGComponent = nullptr;
	TWeakObjectPtr<const UPCGGraph> PCGGraph = nullptr;

	FPCGStack PCGStack;
	bool bIsDebuggable = false;
};

class FPCGEditorGraphDebugObjectItem_PCGSubgraph : public FPCGEditorGraphDebugObjectItem
{
public:
	explicit FPCGEditorGraphDebugObjectItem_PCGSubgraph(
		TWeakObjectPtr<const UPCGNode> InPCGNode,
		TWeakObjectPtr<const UPCGGraph> InPCGGraph,
		const FPCGStack& InPCGStack,
		bool bInIsDebuggable,
		bool bInHasInspectionData)
		: FPCGEditorGraphDebugObjectItem(bInHasInspectionData)
		, PCGNode(InPCGNode)
		, PCGGraph(InPCGGraph)
		, PCGStack(InPCGStack)
		, bIsDebuggable(bInIsDebuggable)
	{}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return GetParent() ? GetParent()->GetPCGComponent() : nullptr; }
	virtual const UObject* GetObject() const override { return PCGNode.Get(); };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }
	virtual bool IsDebuggable() const override { return bIsDebuggable; }
	virtual const UPCGGraph* GetPCGGraph() const override { return PCGGraph.Get(); }

protected:
	TWeakObjectPtr<const UPCGNode> PCGNode = nullptr;
	TWeakObjectPtr<const UPCGGraph> PCGGraph = nullptr;

	FPCGStack PCGStack;
	bool bIsDebuggable = false;
};

class FPCGEditorGraphDebugObjectItem_PCGLoopIndex : public FPCGEditorGraphDebugObjectItem
{
public:
	explicit FPCGEditorGraphDebugObjectItem_PCGLoopIndex(
		int32 InLoopIndex,
		TWeakObjectPtr<const UObject> InLoopedPCGGraph,
		const FPCGStack& InPCGStack,
		bool bInIsDebuggable,
		bool bInHasInspectionData)
		: FPCGEditorGraphDebugObjectItem(bInHasInspectionData)
		, LoopIndex(InLoopIndex)
		, LoopedPCGGraph(InLoopedPCGGraph)
		, PCGStack(InPCGStack)
		, bIsDebuggable(bInIsDebuggable)
	{}

	virtual int32 GetLoopIndex() const { return LoopIndex; }

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return GetParent() ? GetParent()->GetPCGComponent() : nullptr; }
	virtual const UObject* GetObject() const override { return nullptr; };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }
	virtual int32 GetSortPriority() const override { return LoopIndex; }
	virtual bool IsDebuggable() const override { return bIsDebuggable; }
	virtual bool IsLoopIteration() const override { return true; }
	virtual const UPCGGraph* GetPCGGraph() const override { return Cast<UPCGGraph>(LoopedPCGGraph.Get()); }

protected:
	int32 LoopIndex = INDEX_NONE;
	TWeakObjectPtr<const UObject> LoopedPCGGraph = nullptr;

	FPCGStack PCGStack;
	bool bIsDebuggable = false;
};

class SPCGEditorGraphDebugObjectItemRow : public SCompoundWidget
{
public:
	using FDoubleClickFunc = TFunction<void(FPCGEditorGraphDebugObjectItemPtr)>;

	SLATE_BEGIN_ARGS(SPCGEditorGraphDebugObjectItemRow)
		: _OnDoubleClickFunc()
	{}
		SLATE_ARGUMENT(FDoubleClickFunc, OnDoubleClickFunc)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, FPCGEditorGraphDebugObjectItemPtr InItem);

	//~Begin SWidget Interface
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	//~End SWidget Interface

private:
	FPCGEditorGraphDebugObjectItemPtr Item;
	
	/** Invoked when the user double clicks on the row. */
	FDoubleClickFunc DoubleClickFunc;
};

class SPCGEditorGraphDebugObjectTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphDebugObjectTree) {}
	SLATE_END_ARGS()

	virtual ~SPCGEditorGraphDebugObjectTree();

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void RequestRefresh() { bNeedsRefresh = true; };

	void SetNodeBeingInspected(const UPCGNode* InPCGNode);

	void SetDebugObjectSelection(const FPCGStack& FullStack);

private:
	void SelectedDebugObject_OnClicked() const;
	bool IsSelectDebugObjectButtonEnabled() const;

	void SetDebugObjectFromSelection_OnClicked();
	bool IsSetDebugObjectFromSelectionButtonEnabled() const;

	void RefreshTree();
	void SortTreeItems(bool bIsAscending = true, bool bIsRecursive = true);
	void RestoreTreeState();

	void AddStacksToTree(
		const TArray<FPCGStack>& Stacks,
		TMap<AActor*, TSharedPtr<FPCGEditorGraphDebugObjectItem_Actor>>& InOutActorItems,
		TMap<const FPCGStack, FPCGEditorGraphDebugObjectItemPtr>& InOutStackToItem);

	void OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& InPropertyChain);
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);
	void OnObjectConstructed(UObject* InObject);

	UPCGGraph* GetPCGGraph() const;

	TSharedRef<ITableRow> MakeTreeRowWidget(FPCGEditorGraphDebugObjectItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);
	void OnGetChildren(FPCGEditorGraphDebugObjectItemPtr InItem, TArray<FPCGEditorGraphDebugObjectItemPtr>& OutChildren) const;
	void OnSelectionChanged(FPCGEditorGraphDebugObjectItemPtr InItem, ESelectInfo::Type InSelectInfo);
	void OnExpansionChanged(FPCGEditorGraphDebugObjectItemPtr InItem, bool bInIsExpanded);
	void OnSetExpansionRecursive(FPCGEditorGraphDebugObjectItemPtr InItem, bool bInExpand) const;

	/** Expand the given row and select the deepest entry as the debug object if it is unambiguous (the only entry at its level in the tree). */
	void ExpandAndSelectDebugObject(FPCGEditorGraphDebugObjectItemPtr InItem);

	TSharedPtr<SWidget> OpenContextMenu();

	/** Jump-to context menu command. */
	void ContextMenu_JumpToGraphInTree();
	bool ContextMenu_JumpToGraphInTree_CanExecute() const;

	TWeakPtr<FPCGEditor> PCGEditor;

	TSharedPtr<STreeView<FPCGEditorGraphDebugObjectItemPtr>> DebugObjectTreeView;
	TArray<FPCGEditorGraphDebugObjectItemPtr> RootItems;
	TArray<FPCGEditorGraphDebugObjectItemPtr> AllGraphItems;

	bool bNeedsRefresh = false;

	/** Set true to avoid broadcasting debug object change notifications when setting the object from code. */
	bool bDisableDebugObjectChangeNotification = false;

	/** Used to retain item expansion state across tree refreshes. */
	TSet<FPCGStack> ExpandedStacks;

	/** Used to retain item selection state across tree refreshes. */
	FPCGStack SelectedStack;

	/** Used to retain item selection state across tree refreshes if the SelectedStack is invalidated (e.g. through BP reconstruction). */
	TWeakObjectPtr<const UPCGGraph> SelectedGraph = nullptr;
	TWeakObjectPtr<const AActor> SelectedOwner = nullptr;
	uint32 SelectedGridSize = PCGHiGenGrid::UnboundedGridSize();
	FIntVector SelectedGridCoord = FIntVector::ZeroValue;
	TWeakObjectPtr<const UPCGComponent> SelectedOriginalComponent = nullptr;

	/** The previous stack that the user selected. */
	FPCGStack PreviouslySelectedStack;

	const UPCGNode* PCGNodeBeingInspected = nullptr;
};
