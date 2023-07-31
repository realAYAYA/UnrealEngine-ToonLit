// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNaniteAudit.h"
#include "SNaniteTools.h"
#include "NaniteToolCommands.h"
#include "NaniteToolsArguments.h"
#include "DetailsViewArgs.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Styling/StyleColors.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Materials/Material.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "NaniteTools"

namespace NaniteAuditView
{
	static const FName ColumnID_Asset("Asset");
	static const FName ColumnID_Instances("Instances");
	static const FName ColumnID_Triangles("Triangles");
	static const FName ColumnID_Errors("Errors");
	static const FName ColumnID_LODs("LODs");
}

class SNaniteErrorRow : public SMultiColumnTableRow<TSharedPtr<FNaniteAuditRow>>
{
public:
	SLATE_BEGIN_ARGS(SNaniteErrorRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FNaniteAuditRow> InItem)
	{
		Item = InItem;
		SMultiColumnTableRow<TSharedPtr<FNaniteAuditRow>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName)
	{
		if (ColumnName == NaniteAuditView::ColumnID_Asset)
		{
			return 
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HeightOverride(20.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked(Item->SelectionState)
						.OnCheckStateChanged(this, &SNaniteErrorRow::OnCheckBoxCheckStateChanged)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Record->StaticMesh->GetFullName()))
				];
		}
		else if (ColumnName == NaniteAuditView::ColumnID_Instances)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(Item->Record->InstanceCount));
		}
		else if (ColumnName == NaniteAuditView::ColumnID_Triangles)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(Item->Record->StaticMesh->GetNumNaniteTriangles()));
		}
		else if (ColumnName == NaniteAuditView::ColumnID_Errors)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(Item->Record->ErrorCount));
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FNaniteAuditRow> Item;

	void OnCheckBoxCheckStateChanged(ECheckBoxState NewState)
	{
		Item->SelectionState = NewState;
	}
};

class SNaniteOptimizeRow : public SMultiColumnTableRow<TSharedPtr<FNaniteAuditRow>>
{
public:
	SLATE_BEGIN_ARGS(SNaniteOptimizeRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<FNaniteAuditRow> InItem)
	{
		Item = InItem;
		SMultiColumnTableRow<TSharedPtr<FNaniteAuditRow>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName)
	{
		if (ColumnName == NaniteAuditView::ColumnID_Asset)
		{
			return 
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HeightOverride(20.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked(Item->SelectionState)
						.OnCheckStateChanged(this, &SNaniteOptimizeRow::OnCheckBoxCheckStateChanged)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Record->StaticMesh->GetFullName()))
				];
		}
		else if (ColumnName == NaniteAuditView::ColumnID_Instances)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(Item->Record->InstanceCount));
		}
		else if (ColumnName == NaniteAuditView::ColumnID_Triangles)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(Item->Record->StaticMesh->GetNumTriangles(0)));
		}
		else if (ColumnName == NaniteAuditView::ColumnID_LODs)
		{
			return SNew(STextBlock)
				.Text(FText::AsNumber(Item->Record->StaticMesh->GetNumLODs()));
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FNaniteAuditRow> Item;

	void OnCheckBoxCheckStateChanged(ECheckBoxState NewState)
	{
		Item->SelectionState = NewState;
	}
};

void SNaniteAudit::Construct(const FArguments& Args, SNaniteAudit::AuditMode InMode, SNaniteTools* InParent)
{
	Mode = InMode;
	Parent = InParent;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FNaniteToolCommands& ToolCommands = FNaniteToolCommands::Get();

	CommandList = MakeShareable(new FUICommandList);
	
	CommandList->MapAction(
		ToolCommands.ShowInContentBrowser,
		FExecuteAction::CreateSP(this, &SNaniteAudit::OnShowInContentBrowser));

	CommandList->MapAction(
		ToolCommands.EnableNanite,
		FExecuteAction::CreateSP(this, &SNaniteAudit::OnEnableNanite));

	CommandList->MapAction(
		ToolCommands.DisableNanite,
		FExecuteAction::CreateSP(this, &SNaniteAudit::OnDisableNanite));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.bShowOptions = true;
	DetailsViewArgs.bShowModifiedPropertiesOption = true;

	if (Mode == SNaniteAudit::AuditMode::Optimize)
	{
		AuditOptimizeArguments = TStrongObjectPtr<UNaniteAuditOptimizeArguments>(NewObject<UNaniteAuditOptimizeArguments>());
		AuditOptimizeArguments->LoadEditorConfig();

		AuditArgumentsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		AuditArgumentsDetailsView->SetObject(AuditOptimizeArguments.Get());

		TSharedRef<SHeaderRow> HeaderOptimizeRowWidget =
		SNew(SHeaderRow)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Asset)
			.DefaultLabel(LOCTEXT("Column_AssetName", "Asset"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.5f)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Triangles)
			.DefaultLabel(LOCTEXT("Column_Triangles", "Triangles"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.25f)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Instances)
			.DefaultLabel(LOCTEXT("Column_Instances", "Instances"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.25f)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_LODs)
			.DefaultLabel(LOCTEXT("Column_LODs", "LODs"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.25f);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FStyleColors::WindowBorder)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(5.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						.Padding(5.0f)
						[
							AuditArgumentsDetailsView->AsShared()
						]
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(5.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(0.5f)
					.Padding(5.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(0, 0, 0, 3))
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
								.BorderBackgroundColor(FStyleColors::ForegroundHeader)
								.Padding(3.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("NonNaniteMeshes", "Non-Nanite Meshes"))
									.Font(FAppStyle::GetFontStyle("BoldFont"))
									.ShadowOffset(FVector2D(1.0f, 1.0f))
								]
							]

							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SAssignNew(NaniteAuditList, SListView<TSharedPtr<FNaniteAuditRow>>)
								.ListItemsSource(GetNaniteAuditRows())
								.OnGenerateRow(this, &SNaniteAudit::OnGenerateRow)
								.OnContextMenuOpening(this, &SNaniteAudit::OnConstructContextMenu)
								.HeaderRow(HeaderOptimizeRowWidget)
								.SelectionMode(ESelectionMode::Single)
								.ClearSelectionOnClick(true)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.Padding(5.0f)
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(FOnClicked::CreateSP(this, &SNaniteAudit::OnBatchEnableNanite))
					.ToolTipText(LOCTEXT("PopulateAssetListTooltipLocEnableNaniteAll", "Enable Nanite on all selected meshes"))
					.Text(LOCTEXT("PopulateAssetListLocEnableNanite", "Enable Nanite"))
				]
			]
		];
	}
	else
	{
		AuditErrorArguments = TStrongObjectPtr<UNaniteAuditErrorArguments>(NewObject<UNaniteAuditErrorArguments>());
		AuditErrorArguments->LoadEditorConfig();

		AuditArgumentsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		AuditArgumentsDetailsView->SetObject(AuditErrorArguments.Get());

		TSharedRef<SHeaderRow> HeaderErrorRowWidget =
		SNew(SHeaderRow)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Asset)
			.DefaultLabel(LOCTEXT("Column_AssetName", "Asset"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.5f)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Triangles)
			.DefaultLabel(LOCTEXT("Column_Triangles", "Triangles"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.25f)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Instances)
			.DefaultLabel(LOCTEXT("Column_Instances", "Instances"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.25f)
		+ SHeaderRow::Column(NaniteAuditView::ColumnID_Errors)
			.DefaultLabel(LOCTEXT("Column_Errors", "Errors"))
			.HAlignHeader(EHorizontalAlignment::HAlign_Left)
			.FillWidth(0.25f);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FStyleColors::WindowBorder)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(5.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						.Padding(5.0f)
						[
							AuditArgumentsDetailsView->AsShared()
						]
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(5.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(0.5f)
					.Padding(5.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(0, 0, 0, 3))
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
								.BorderBackgroundColor(FStyleColors::ForegroundHeader)
								.Padding(3.0f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("NaniteMeshErrors", "Nanite Mesh Errors"))
									.Font(FAppStyle::GetFontStyle("BoldFont"))
									.ShadowOffset(FVector2D(1.0f, 1.0f))
								]
							]

							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SAssignNew(NaniteAuditList, SListView<TSharedPtr<FNaniteAuditRow>>)
								.ListItemsSource(GetNaniteAuditRows())
								.OnGenerateRow(this, &SNaniteAudit::OnGenerateRow)
								.OnContextMenuOpening(this, &SNaniteAudit::OnConstructContextMenu)
								.HeaderRow(HeaderErrorRowWidget)
								.SelectionMode(ESelectionMode::Single)
								.ClearSelectionOnClick(true)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.Padding(5.0f)
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(FOnClicked::CreateSP(this, &SNaniteAudit::OnBatchDisableNanite))
					.ToolTipText(LOCTEXT("PopulateAssetListTooltipLocDisableNaniteAll", "Disable Nanite on all selected meshes"))
					.Text(LOCTEXT("PopulateAssetListLocDisableNanite", "Disable Nanite"))
				]
			]
		];
	}
}

SNaniteAudit::~SNaniteAudit()
{
	if (AuditErrorArguments)
	{
		AuditErrorArguments->SaveEditorConfig();
	}

	if (AuditOptimizeArguments)
	{
		AuditOptimizeArguments->SaveEditorConfig();
	}
}

TSharedRef<ITableRow> SNaniteAudit::OnGenerateRow(TSharedPtr<FNaniteAuditRow> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Mode == SNaniteAudit::AuditMode::Optimize)
		return SNew(SNaniteOptimizeRow, OwnerTable, InItem).Visibility(EVisibility::Visible);
	else
		return SNew(SNaniteErrorRow, OwnerTable, InItem).Visibility(EVisibility::Visible);
}

TSharedPtr<SWidget> SNaniteAudit::OnConstructContextMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	const FNaniteToolCommands& ToolCommands = FNaniteToolCommands::Get();

	MenuBuilder.BeginSection("Navigation", LOCTEXT("NavigationMenuHeading", "Navigation"));
	{
		MenuBuilder.AddMenuEntry(ToolCommands.ShowInContentBrowser, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Action", LOCTEXT("ActionMenuHeading", "Action"));
	{
		if (Mode == SNaniteAudit::AuditMode::Optimize)
		{
			MenuBuilder.AddMenuEntry(ToolCommands.EnableNanite, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon());
		}
		else
		{
			MenuBuilder.AddMenuEntry(ToolCommands.DisableNanite, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon());
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SNaniteAudit::OnShowInContentBrowser()
{
	if (GEditor)
	{
		TArray<TSharedPtr<FNaniteAuditRow>> SelectedItems = NaniteAuditList->GetSelectedItems();
		TArray<UObject*> ObjectsToSync;

		for (TSharedPtr<FNaniteAuditRow>& Selection : SelectedItems)
		{
			if (Selection->Record->StaticMesh.IsValid())
			{
				ObjectsToSync.Add(Selection->Record->StaticMesh.Get());
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(ObjectsToSync);
		}
	}
}

void SNaniteAudit::OnEnableNanite()
{
	if (GEditor)
	{
		TArray<TSharedPtr<FNaniteAuditRow>> SelectedItems = NaniteAuditList->GetSelectedItems();
		TArray<TWeakObjectPtr<UStaticMesh>> MeshesToProcess;

		for (TSharedPtr<FNaniteAuditRow>& Selection : SelectedItems)
		{
			if (Selection->Record->StaticMesh.IsValid())
			{
				MeshesToProcess.Add(Selection->Record->StaticMesh);
			}
		}

		if (MeshesToProcess.Num() > 0)
		{
			ModifyNaniteEnable(MeshesToProcess, true /* Enable */);
			Parent->Audit();
		}
	}
}

void SNaniteAudit::OnDisableNanite()
{
	if (GEditor)
	{
		TArray<TSharedPtr<FNaniteAuditRow>> SelectedItems = NaniteAuditList->GetSelectedItems();
		TArray<TWeakObjectPtr<UStaticMesh>> MeshesToProcess;

		for (TSharedPtr<FNaniteAuditRow>& Selection : SelectedItems)
		{
			if (Selection->Record->StaticMesh.IsValid())
			{
				MeshesToProcess.Add(Selection->Record->StaticMesh);
			}
		}

		if (MeshesToProcess.Num() > 0)
		{
			ModifyNaniteEnable(MeshesToProcess, false /* Disable */);
			Parent->Audit();
		}
	}
}

FReply SNaniteAudit::OnBatchEnableNanite()
{
	if (GEditor)
	{
		TArray<TWeakObjectPtr<UStaticMesh>> MeshesToProcess;

		for (TSharedPtr<FNaniteAuditRow>& AuditRow : NaniteAuditRows)
		{
			if (AuditRow->SelectionState == ECheckBoxState::Checked && AuditRow->Record->StaticMesh.IsValid())
			{
				MeshesToProcess.Add(AuditRow->Record->StaticMesh);
			}
		}

		ModifyNaniteEnable(MeshesToProcess, true /* Enable */);
		Parent->Audit();
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SNaniteAudit::OnBatchDisableNanite()
{
	if (GEditor)
	{
		TArray<TWeakObjectPtr<UStaticMesh>> MeshesToProcess;

		for (TSharedPtr<FNaniteAuditRow>& AuditRow : NaniteAuditRows)
		{
			if (AuditRow->SelectionState == ECheckBoxState::Checked && AuditRow->Record->StaticMesh.IsValid())
			{
				MeshesToProcess.Add(AuditRow->Record->StaticMesh);
			}
		}

		ModifyNaniteEnable(MeshesToProcess, false /* Disable */);
		Parent->Audit();
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SNaniteAudit::PreAudit()
{
	NaniteAuditList->ClearHighlightedItems();
	NaniteAuditList->ClearSelection();
	NaniteAuditRows.Empty();
	NaniteAuditList->RebuildList();
}

void SNaniteAudit::PostAudit(TSharedPtr<FNaniteAuditRegistry> AuditRegistry)
{
	if (AuditRegistry)
	{
		auto& Records = (Mode == SNaniteAudit::AuditMode::Optimize)
			? AuditRegistry->GetOptimizeRecords() : AuditRegistry->GetErrorRecords();

		for (auto& Record : Records)
		{
			bool bIgnoreRecord = false;

			if (Mode == SNaniteAudit::AuditMode::Optimize)
			{
				// Ignore records if the static mesh triangle count is below the specified threshold
				if (Record->TriangleCount < AuditOptimizeArguments->TriangleThreshold)
				{
					bIgnoreRecord = true;
				}

				// Here we want to remove any records that have material features that are 
				// disallowed by Nanite, as we don't want to enable Nanite in these cases.
				for (const FStaticMeshComponentRecord& SMCRecord : Record->StaticMeshComponents)
				{
					if (bIgnoreRecord)
					{
						break;
					}

					for (const Nanite::FMaterialAuditEntry& MaterialEntry : SMCRecord.MaterialAudit.Entries)
					{
						bIgnoreRecord |= (AuditOptimizeArguments->DisallowUnsupportedBlendMode && MaterialEntry.bHasUnsupportedBlendMode);
						bIgnoreRecord |= (AuditOptimizeArguments->DisallowVertexInterpolator   && MaterialEntry.bHasVertexInterpolator);
						bIgnoreRecord |= (AuditOptimizeArguments->DisallowPixelDepthOffset     && MaterialEntry.bHasPixelDepthOffset);
						bIgnoreRecord |= (AuditOptimizeArguments->DisallowWorldPositionOffset  && MaterialEntry.bHasWorldPositionOffset);

						if (bIgnoreRecord)
						{
							break;
						}
					}
				}
			}
			else
			{
				Record->ErrorCount = 0;

				// Here we want to remove any records that do not have any errors, and
				// also build up a list of detailed error messages for each record.
				for (const FStaticMeshComponentRecord& SMCRecord : Record->StaticMeshComponents)
				{
					for (const Nanite::FMaterialAuditEntry& MaterialEntry : SMCRecord.MaterialAudit.Entries)
					{
						if (MaterialEntry.bHasNullMaterial)
						{
							// Skip null materials
							continue;
						}

						const bool bUnsupportedBlendModeError	= (AuditErrorArguments->ProhibitUnsupportedBlendMode	&& MaterialEntry.bHasUnsupportedBlendMode);
						const bool bVertexInterpolatorError		= (AuditErrorArguments->ProhibitVertexInterpolator		&& MaterialEntry.bHasVertexInterpolator);
						const bool bPixelDepthOffsetError		= (AuditErrorArguments->ProhibitPixelDepthOffset		&& MaterialEntry.bHasPixelDepthOffset);
						const bool bWorldPositionOffsetError	= (AuditErrorArguments->ProhibitWorldPositionOffset		&& MaterialEntry.bHasWorldPositionOffset);

						if (bUnsupportedBlendModeError || bVertexInterpolatorError || bPixelDepthOffsetError || bWorldPositionOffsetError)
						{
							const UMaterial* Material = MaterialEntry.Material->GetMaterial_Concurrent();

							FNaniteMaterialError* ExistingErrors = Record->MaterialErrors.FindByPredicate([=](const FNaniteMaterialError& Entry) { return Entry.Material.Get() == Material; });
							if (ExistingErrors)
							{
								ExistingErrors->bUnsupportedBlendModeError	|= bUnsupportedBlendModeError;
								ExistingErrors->bVertexInterpolatorError	|= bVertexInterpolatorError;
								ExistingErrors->bPixelDepthOffsetError		|= bPixelDepthOffsetError;
								ExistingErrors->bWorldPositionOffsetError	|= bWorldPositionOffsetError;
								ExistingErrors->ReferencingSMCs.Add(SMCRecord.Component);
							}
							else
							{
								FNaniteMaterialError& NewErrors = Record->MaterialErrors.AddDefaulted_GetRef();
								NewErrors.bUnsupportedBlendModeError	= bUnsupportedBlendModeError;
								NewErrors.bVertexInterpolatorError		= bVertexInterpolatorError;
								NewErrors.bPixelDepthOffsetError		= bPixelDepthOffsetError;
								NewErrors.bWorldPositionOffsetError		= bWorldPositionOffsetError;
								NewErrors.Material = Material;
								NewErrors.ReferencingSMCs.Add(SMCRecord.Component);
							}
						}
					}
				}

				// Tally up unique per-material errors associated with this static mesh that has Nanite erroneously enabled.
				for (const FNaniteMaterialError& MaterialError : Record->MaterialErrors)
				{
					Record->ErrorCount += MaterialError.bUnsupportedBlendModeError	? 1u : 0u;
					Record->ErrorCount += MaterialError.bVertexInterpolatorError	? 1u : 0u;
					Record->ErrorCount += MaterialError.bPixelDepthOffsetError		? 1u : 0u;
					Record->ErrorCount += MaterialError.bWorldPositionOffsetError	? 1u : 0u;
				}

				if (Record->ErrorCount == 0)
				{
					bIgnoreRecord = true;
				}
			}

			if (!bIgnoreRecord)
			{
				NaniteAuditRows.Add(MakeShareable(new FNaniteAuditRow(Record)));
			}
		}
	}

	NaniteAuditList->RebuildList();
}

void SNaniteAudit::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	Parent->Audit();
}

#undef LOCTEXT_NAMESPACE
