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
	virtual ~FPCGEditorGraphDebugObjectItem() = default;

	void AddChild(TSharedRef<FPCGEditorGraphDebugObjectItem> InChild);
	const TSet<TSharedPtr<FPCGEditorGraphDebugObjectItem>>& GetChildren() const;
	FPCGEditorGraphDebugObjectItemPtr GetParent() const;

	virtual FString GetLabel() const = 0;	
	virtual UPCGComponent* GetPCGComponent() const = 0;
	virtual const FPCGStack* GetPCGStack() const { return nullptr; }
	virtual const UObject* GetObject() const  = 0;
	
protected:
	TWeakPtr<FPCGEditorGraphDebugObjectItem> Parent;
	TSet<TSharedPtr<FPCGEditorGraphDebugObjectItem>> Children;
};

class FPCGEditorGraphDebugObjectItem_Actor : public FPCGEditorGraphDebugObjectItem
{
public:
	FPCGEditorGraphDebugObjectItem_Actor(TWeakObjectPtr<AActor> InActor)
		: Actor(InActor)
	{ }

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return nullptr; }
	virtual const UObject* GetObject() const override { return Actor.Get(); };
	
protected:
	TWeakObjectPtr<AActor> Actor = nullptr;
};

class FPCGEditorGraphDebugObjectItem_PCGComponent : public FPCGEditorGraphDebugObjectItem
{
public:
	FPCGEditorGraphDebugObjectItem_PCGComponent(TWeakObjectPtr<UPCGComponent> InPCGComponent)
		: PCGComponent(InPCGComponent)
	{}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return PCGComponent.Get(); }
	virtual const UObject* GetObject() const override { return PCGComponent.Get(); };
	
protected:
	TWeakObjectPtr<UPCGComponent> PCGComponent = nullptr;
};

class FPCGEditorGraphDebugObjectItem_PCGGraph : public FPCGEditorGraphDebugObjectItem
{
public:
	FPCGEditorGraphDebugObjectItem_PCGGraph(TWeakObjectPtr<const UPCGGraph> InPCGGraph, const FPCGStack& InPCGStack)
		: PCGGraph(InPCGGraph)
		, PCGStack(InPCGStack)
	{}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return GetParent() ? GetParent()->GetPCGComponent() : nullptr; }
	virtual const UObject* GetObject() const override { return PCGGraph.Get(); };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }
	
protected:
	TWeakObjectPtr<const UPCGGraph> PCGGraph = nullptr;
	FPCGStack PCGStack;
};

class FPCGEditorGraphDebugObjectItem_PCGSubgraph : public FPCGEditorGraphDebugObjectItem
{
public:
	FPCGEditorGraphDebugObjectItem_PCGSubgraph(TWeakObjectPtr<const UPCGNode> InPCGNode, TWeakObjectPtr<const UPCGGraph> InPCGGraph, const FPCGStack& InPCGStack)
		: PCGNode(InPCGNode)
		, PCGGraph(InPCGGraph)
		, PCGStack(InPCGStack)
	{}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return GetParent() ? GetParent()->GetPCGComponent() : nullptr; }
	virtual const UObject* GetObject() const override { return PCGNode.Get(); };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }
	
protected:
	TWeakObjectPtr<const UPCGNode> PCGNode = nullptr;
	TWeakObjectPtr<const UPCGGraph> PCGGraph = nullptr;
	FPCGStack PCGStack;
};

class FPCGEditorGraphDebugObjectItem_PCGLoopIndex : public FPCGEditorGraphDebugObjectItem
{
public:
	FPCGEditorGraphDebugObjectItem_PCGLoopIndex(TWeakObjectPtr<const UPCGNode> InPCGNode, int32 InLoopIndex, const FPCGStack& InPCGStack)
		: PCGNode(InPCGNode)
		, LoopIndex(InLoopIndex)
		, PCGStack(InPCGStack)
	{}

	virtual FString GetLabel() const override;
	virtual UPCGComponent* GetPCGComponent() const override { return GetParent() ? GetParent()->GetPCGComponent() : nullptr; }
	virtual const UObject* GetObject() const override { return PCGNode.Get(); };
	virtual const FPCGStack* GetPCGStack() const override { return &PCGStack; }
	virtual int32 GetLoopIndex() const { return LoopIndex; }
	
protected:
	TWeakObjectPtr<const UPCGNode> PCGNode = nullptr;
	int32 LoopIndex = INDEX_NONE;
	FPCGStack PCGStack;
};

class SPCGEditorGraphDebugObjectItemRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphDebugObjectItemRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, FPCGEditorGraphDebugObjectItemPtr InItem);
	
private:
	FPCGEditorGraphDebugObjectItemPtr Item;
};

class SPCGEditorGraphDebugObjectTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphDebugObjectTree) {}
	SLATE_END_ARGS()

	virtual ~SPCGEditorGraphDebugObjectTree();
	
	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	void RequestRefresh() { bNeedsRefresh = true; }

	void AddDynamicStack(TWeakObjectPtr<UPCGComponent> InComponent, const FPCGStack& InvocationStack);
	
private:
	void SelectedDebugObject_OnClicked() const;
	bool IsSelectDebugObjectButtonEnabled() const;
	
	void SetDebugObjectFromSelection_OnClicked();
	bool IsSetDebugObjectFromSelectionButtonEnabled() const;
	
	void RefreshTree();
	
	void OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& InPropertyChain);
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);
	void OnObjectConstructed(UObject* InObject);

	UPCGGraph* GetPCGGraph() const;
	
	TSharedRef<ITableRow> MakeTreeRowWidget(FPCGEditorGraphDebugObjectItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) const;
	void OnGetChildren(FPCGEditorGraphDebugObjectItemPtr InItem, TArray<FPCGEditorGraphDebugObjectItemPtr>& OutChildren) const;
	void OnSelectionChanged(FPCGEditorGraphDebugObjectItemPtr InItem, ESelectInfo::Type InSelectInfo) const;
	void OnSetExpansionRecursive(FPCGEditorGraphDebugObjectItemPtr InItem, bool bInExpand);
	
	TWeakPtr<FPCGEditor> PCGEditor;

	TSharedPtr<STreeView<FPCGEditorGraphDebugObjectItemPtr>> DebugObjectTreeView;
	TArray<FPCGEditorGraphDebugObjectItemPtr> RootItems;
	TArray<FPCGEditorGraphDebugObjectItemPtr> AllGraphItems;

	TMap<const TWeakObjectPtr<UPCGComponent>, TArray<FPCGStack>> DynamicInvocationStacks;

	bool bNeedsRefresh = false;
};
