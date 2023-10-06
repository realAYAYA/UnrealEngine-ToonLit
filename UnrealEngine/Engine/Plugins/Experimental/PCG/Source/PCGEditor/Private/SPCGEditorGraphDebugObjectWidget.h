// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/PCGStackContext.h"
#include "Widgets/SCompoundWidget.h"

namespace ESelectInfo { enum Type : int; }
template <typename OptionType> class SComboBox;

class FPCGEditor;
class UPCGComponent;
class UPCGGraph;

class FPCGEditorGraphDebugObjectInstance
{
public:
	FPCGEditorGraphDebugObjectInstance();
	FPCGEditorGraphDebugObjectInstance(TWeakObjectPtr<UPCGComponent> InPCGComponent, const FPCGStack& InPCGStack);

	FText GetDebugObjectText() const { return Label; }

	TWeakObjectPtr<UPCGComponent> GetPCGComponent() const { return PCGComponent; }
	const FPCGStack& GetStack() const { return PCGStack; }
	
private:
	FText Label;

	/** Component containing the inspection cache */
	TWeakObjectPtr<UPCGComponent> PCGComponent = nullptr;

	/** Stack to identify graph or subgraph instance */
	FPCGStack PCGStack;
};


class SPCGEditorGraphDebugObjectWidget: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphDebugObjectWidget) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

	void RefreshDebugObjects();

	void OnLevelActorDeleted(const AActor* InActor);

	void AddDynamicStack(const TWeakObjectPtr<UPCGComponent> InComponent, const FPCGStack& InvocationStack);

private:
	void OnComboBoxOpening();
	void OnSelectionChanged(TSharedPtr<FPCGEditorGraphDebugObjectInstance> NewSelection, ESelectInfo::Type SelectInfo) const;
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FPCGEditorGraphDebugObjectInstance> InDebugObjectInstance) const;

	UPCGGraph* GetPCGGraph() const;
	
	FText GetSelectedDebugObjectText() const;
	void SelectedDebugObject_OnClicked() const;
	bool IsSelectDebugObjectButtonEnabled() const;
	
	void SetDebugObjectFromSelection_OnClicked();
	bool IsSetDebugObjectFromSelectionButtonEnabled() const;
	
	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	TArray<TSharedPtr<FPCGEditorGraphDebugObjectInstance>> DebugObjects;
	TSharedPtr<SComboBox<TSharedPtr<FPCGEditorGraphDebugObjectInstance>>> DebugObjectsComboBox;

	TMap<const TWeakObjectPtr<UPCGComponent>, TArray<FPCGStack>> DynamicInvocationStacks;
};
