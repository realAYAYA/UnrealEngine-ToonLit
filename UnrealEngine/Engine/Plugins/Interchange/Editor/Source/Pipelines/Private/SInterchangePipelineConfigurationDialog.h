// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Styling/SlateColor.h"
#include "UObject/GCObject.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

struct FPropertyAndParent;
class IDetailsView;
class SCheckBox;

class FInterchangePipelineStacksTreeNodeItem : protected FGCObject
{
public:
	FInterchangePipelineStacksTreeNodeItem()
	{
		StackName = NAME_None;
		Pipeline = nullptr;
	}

	/* FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FInterchangePipelineStacksTreeNodeItem");
	}

	//This name is use only when this item represent a stack name
	FName StackName;

	//Pipeline is nullptr when the node represent a stack name
	UInterchangePipelineBase* Pipeline;

	TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>> Childrens;
};

DECLARE_DELEGATE_TwoParams(FOnPipelineConfigurationSelectionChanged, TSharedPtr<FInterchangePipelineStacksTreeNodeItem>, ESelectInfo::Type)

class SInterchangePipelineStacksTreeView : public STreeView< TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>
{
public:
	~SInterchangePipelineStacksTreeView();

	SLATE_BEGIN_ARGS(SInterchangePipelineStacksTreeView)
		: _OnSelectionChangedDelegate()
	{}
		SLATE_EVENT(FOnPipelineConfigurationSelectionChanged, OnSelectionChangedDelegate)
		SLATE_ARGUMENT(TWeakObjectPtr<UInterchangeSourceData>, SourceData)
		SLATE_ARGUMENT(bool, bSceneImport)
		SLATE_ARGUMENT(bool, bReimport)
		SLATE_ARGUMENT(TArray<UInterchangePipelineBase*>, PipelineStack)
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowPipelineConfigurationTreeView(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenPipelineConfigurationTreeView(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> InParent, TArray< TSharedPtr<FInterchangePipelineStacksTreeNodeItem> >& OutChildren);

	FReply OnExpandAll();
	FReply OnCollapseAll();

	const TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& GetRootNodeArray() const { return RootNodeArray; }
	TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& GetMutableRootNodeArray() { return RootNodeArray; }

	void SelectDefaultItem();

protected:
	/** Delegate to invoke when selection changes. */
	FOnPipelineConfigurationSelectionChanged OnSelectionChangedDelegate;
	TWeakObjectPtr<UInterchangeSourceData> SourceData;
	bool bSceneImport = false;
	bool bReimport = false;
	TArray<UInterchangePipelineBase*> PipelineStack;

	/** the elements we show in the tree view */
	TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>> RootNodeArray;

	/** Open a context menu for the current selection */
	TSharedPtr<SWidget> OnOpenContextMenu();
	void SetAsDefaultStack(FName NewDefaultStackValue);
	void RecursiveSetExpand(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Node, bool ExpandState);
	void OnTreeViewSelectionChanged(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Item, ESelectInfo::Type SelectionType);
};


enum class ECloseEventType : uint8
{
	Cancel,
	Import,
	WindowClosing
};

class SInterchangePipelineConfigurationDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInterchangePipelineConfigurationDialog)
		: _OwnerWindow()
	{}

		SLATE_ARGUMENT(TWeakPtr<SWindow>, OwnerWindow)
		SLATE_ARGUMENT(TWeakObjectPtr<UInterchangeSourceData>, SourceData)
		SLATE_ARGUMENT(bool, bSceneImport)
		SLATE_ARGUMENT(bool, bReimport)
		SLATE_ARGUMENT(TArray<UInterchangePipelineBase*>, PipelineStack)
	SLATE_END_ARGS()

public:

	SInterchangePipelineConfigurationDialog();
	virtual ~SInterchangePipelineConfigurationDialog();

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	void ClosePipelineConfiguration(const ECloseEventType CloseEventType);

	FReply OnCloseDialog(const ECloseEventType CloseEventType)
	{
		ClosePipelineConfiguration(CloseEventType);
		return FReply::Handled();
	}

	void OnWindowClosed(const TSharedRef<SWindow>& ClosedWindow)
	{
		ClosePipelineConfiguration(ECloseEventType::WindowClosing);
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	bool IsCanceled() const { return bCanceled; }
	bool IsImportAll() const { return bImportAll; }

private:
	TSharedRef<SBox> SpawnPipelineConfiguration();
	void OnSelectionChanged(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Item, ESelectInfo::Type SelectionType);

	bool IsPropertyVisible(const FPropertyAndParent&) const;
	FText GetSourceDescription() const;
	void RecursiveIterateNode(TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, TFunctionRef<void(TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& CurrentNode)> IterationLambda);
	FReply OnResetToDefault();

	bool RecursiveValidatePipelineSettings(const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, TOptional<FText>& OutInvalidReason) const;
	bool IsImportButtonEnabled() const;
	FText GetImportButtonTooltip() const;

	void RecursiveSavePipelineSettings(const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, const int32 PipelineIndex) const;
	void RecursiveLoadPipelineSettings(const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, const int32 PipelineIndex) const;

private:
	TWeakPtr< SWindow > OwnerWindow;
	TWeakObjectPtr<UInterchangeSourceData> SourceData;
	TArray<UInterchangePipelineBase*> PipelineStack;
	
	//Graph Inspector UI elements
	TSharedPtr<SInterchangePipelineStacksTreeView> PipelineConfigurationTreeView;
	TSharedPtr<IDetailsView> PipelineConfigurationDetailsView;
	TSharedPtr<SCheckBox> UseSameSettingsForAllCheckBox;

	bool bSceneImport = false;
	bool bReimport = false;
	bool bCanceled = false;
	bool bImportAll = false;

	FName CurrentStackName = NAME_None;
	TWeakObjectPtr<UInterchangePipelineBase> CurrentSelectedPipeline = nullptr;
};

