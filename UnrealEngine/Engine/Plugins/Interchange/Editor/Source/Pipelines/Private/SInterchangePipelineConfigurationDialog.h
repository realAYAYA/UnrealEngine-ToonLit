// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangePipelineBase.h"
#include "InterchangePipelineConfigurationBase.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Views/STreeView.h"

class SBox;

struct FPropertyAndParent;
struct FSlateBrush;
class IDetailsView;
class SCheckBox;

struct FInterchangePipelineItemType
{
public:
	FString DisplayName;

	UInterchangePipelineBase* Pipeline;
};

class SInterchangePipelineItem : public STableRow<TSharedPtr<FInterchangePipelineItemType>>
{
public:
	void Construct(
		const FArguments& InArgs,
		const TSharedRef<STableViewBase>& OwnerTable,
		TSharedPtr<FInterchangePipelineItemType> InPipelineElement);
private:
	const FSlateBrush* GetImageItemIcon() const;

	TSharedPtr<FInterchangePipelineItemType> PipelineElement;
};

typedef SListView< TSharedPtr<FInterchangePipelineItemType> > SPipelineListViewType;

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
		SLATE_ARGUMENT(TArray<FInterchangeStackInfo>, PipelineStacks)
		SLATE_ARGUMENT(TArray<UInterchangePipelineBase*>*, OutPipelines)
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

	bool IsPropertyVisible(const FPropertyAndParent&) const;
	FText GetSourceDescription() const;
	FReply OnResetToDefault();

	bool ValidateAllPipelineSettings(TOptional<FText>& OutInvalidReason) const;
	bool IsImportButtonEnabled() const;
	FText GetImportButtonTooltip() const;

	void SaveAllPipelineSettings() const;

	/** Internal utility function to properly display pipeline's name */
	static FString GetPipelineDisplayName(const UInterchangePipelineBase* Pipeline);

private:
	TWeakPtr< SWindow > OwnerWindow;
	TWeakObjectPtr<UInterchangeSourceData> SourceData;
	TArray<FInterchangeStackInfo> PipelineStacks;
	TArray<UInterchangePipelineBase*>* OutPipelines;

	// The available stacks
	TArray<TSharedPtr<FString>> AvailableStacks;
	void OnStackSelectionChanged(TSharedPtr<FString> String, ESelectInfo::Type);

	//////////////////////////////////////////////////////////////////////////
	// the pipelines list view
	
	TSharedPtr<SPipelineListViewType> PipelinesListView;
	TArray< TSharedPtr< FInterchangePipelineItemType > > PipelineListViewItems;

	/** list view generate row callback */
	TSharedRef<ITableRow> MakePipelineListRowWidget(TSharedPtr<FInterchangePipelineItemType> InElement, const TSharedRef<STableViewBase>& OwnerTable);

	void OnPipelineSelectionChanged(TSharedPtr<FInterchangePipelineItemType> InItem, ESelectInfo::Type SelectInfo);

	//
	//////////////////////////////////////////////////////////////////////////

	TSharedPtr<IDetailsView> PipelineConfigurationDetailsView;
	TSharedPtr<SCheckBox> UseSameSettingsForAllCheckBox;

	bool bSceneImport = false;
	bool bReimport = false;
	bool bCanceled = false;
	bool bImportAll = false;

	FName CurrentStackName = NAME_None;
	TObjectPtr<UInterchangePipelineBase> CurrentSelectedPipeline = nullptr;
};
