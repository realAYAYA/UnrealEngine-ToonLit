// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSpreadsheetView.h"
#include "EditorStyleSet.h"
#include "ISequencer.h"
#include "NiagaraComponent.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet2/DebuggerCommands.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SScrollBox.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SNiagaraSpreadsheetView"
#define ARRAY_INDEX_COLUMN_NAME TEXT("Array Index")
#define OUTPUT_KEY_COLUMN_NAME TEXT("Output Property")
#define INPUT_KEY_COLUMN_NAME TEXT("Input Property")
#define VALUE_COLUMN_NAME TEXT("Value")
#define FILLER_COLUMN_NAME TEXT("__FILLER__")

void SNiagaraSpreadsheetRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	RowIndex = InArgs._RowIndex;
	DataSet = InArgs._DataSet;
	ColumnsAreAttributes = InArgs._ColumnsAreAttributes;

	SupportedFields = InArgs._SupportedFields;
	FieldInfoMap = InArgs._FieldInfoMap;
	UseGlobalOffsets = InArgs._UseGlobalOffsets;
	ParameterStore = InArgs._ParameterStore;

	SMultiColumnTableRow< TSharedPtr<int32> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}


TSharedRef< SWidget > SNiagaraSpreadsheetRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == FILLER_COLUMN_NAME)
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<SWidget> EntryWidget;
	const SNiagaraSpreadsheetView::FieldInfo* FieldInfo = nullptr;
	int32 RealRowIdx = 0;
	if (ColumnsAreAttributes && ColumnName == ARRAY_INDEX_COLUMN_NAME)
	{
		EntryWidget = SNew(STextBlock)
			.Text(FText::AsNumber(RowIndex));
	}
	else if (!ColumnsAreAttributes && (ColumnName == INPUT_KEY_COLUMN_NAME || ColumnName == OUTPUT_KEY_COLUMN_NAME))
	{
		FText Text;
		if (SupportedFields.IsValid())
		{
			Text = FText::FromName((*SupportedFields.Get())[RowIndex]);
		}
		EntryWidget = SNew(STextBlock)
			.Text(Text);
	}
	else if (ColumnsAreAttributes)
	{
		FieldInfo = FieldInfoMap.IsValid() ? FieldInfoMap->Find(ColumnName) : nullptr;
		RealRowIdx = RowIndex;
	}
	else if (!ColumnsAreAttributes && ColumnName == VALUE_COLUMN_NAME)
	{
		FieldInfo = FieldInfoMap.IsValid() ? FieldInfoMap->Find((*SupportedFields.Get())[RowIndex]) : nullptr;
	}

	if (FieldInfo != nullptr && (UseGlobalOffsets ? ParameterStore != nullptr : DataSet != nullptr) && !EntryWidget.IsValid())
	{

		if (FieldInfo->bFloat)
		{

			float Value = 0.0f;

			if (FieldInfo->bHalf)
			{
				FFloat16* Src = nullptr;
				if (UseGlobalOffsets && ParameterStore->GetParameterDataArray().GetData() != nullptr)
				{
					const uint32 CompBufferOffset = FieldInfo->GlobalStartOffset;
					const uint8* SrcU8 = ParameterStore->GetParameterDataArray().GetData();
					Src = SrcU8 ? (FFloat16*)(SrcU8 + CompBufferOffset) : nullptr;
				}
				else if (DataSet != nullptr && DataSet->GetCurrentDataChecked().GetNumInstances() > 0)
				{
					uint32 CompBufferOffset = FieldInfo->FloatStartOffset;
					Src = DataSet->GetCurrentData()->GetInstancePtrHalf(CompBufferOffset, RealRowIdx);
				}

				if (Src != nullptr)
				{
					Value = Src[0];
				}
			}
			else
			{
				float* Src = nullptr;
				if (UseGlobalOffsets && ParameterStore->GetParameterDataArray().GetData() != nullptr)
				{
					const uint32 CompBufferOffset = FieldInfo->GlobalStartOffset;
					const uint8* SrcU8 = ParameterStore->GetParameterDataArray().GetData();
					Src = SrcU8 ? (float*)(SrcU8 + CompBufferOffset) : nullptr;
				}
				else if (DataSet != nullptr && DataSet->GetCurrentDataChecked().GetNumInstances() > 0)
				{
					uint32 CompBufferOffset = FieldInfo->FloatStartOffset;
					Src = DataSet->GetCurrentData()->GetInstancePtrFloat(CompBufferOffset, RealRowIdx);
				}

				if (Src != nullptr)
				{
					Value = Src[0];
				}
			}

			EntryWidget = SNew(STextBlock)
				.Text(FText::AsNumber(Value));

		}
		else if (FieldInfo->bBoolean)
		{
			int32* Src = nullptr;
			if (UseGlobalOffsets)
			{
				const uint32 CompBufferOffset = FieldInfo->GlobalStartOffset;
				const uint8* SrcU8 = ParameterStore->GetParameterDataArray().GetData();
				Src = SrcU8 ? (int32*)(SrcU8 + CompBufferOffset) : nullptr;
			}
			else if (DataSet && DataSet->GetCurrentDataChecked().GetNumInstances() > 0)
			{
				uint32 CompBufferOffset = FieldInfo->IntStartOffset;
				Src = DataSet->GetCurrentDataChecked().GetInstancePtrInt32(CompBufferOffset, RealRowIdx);
			}
			FText ValueText;
			if (Src && Src[0] == 0)
			{
				ValueText = LOCTEXT("NiagaraFalse", "False");
			}
			else if (Src && Src[0] == -1)
			{
				ValueText = LOCTEXT("NiagaraTrue", "True");
			}
			else
			{
				ValueText = FText::Format(LOCTEXT("NiagaraUnknown", "Invalid({0}"), FText::AsNumber(Src[0]));
			}
			EntryWidget = SNew(STextBlock)
				.Text(ValueText);

		}
		else if (FieldInfo->Enum.IsValid())
		{
			int32* Src = nullptr;
			if (UseGlobalOffsets)
			{
				const uint32 CompBufferOffset = FieldInfo->GlobalStartOffset;
				const uint8* SrcU8 = ParameterStore->GetParameterDataArray().GetData();
				Src = SrcU8 ? (int32*)(SrcU8 + CompBufferOffset) : nullptr;
			}
			else if (DataSet != nullptr && DataSet->GetCurrentDataChecked().GetNumInstances() > 0)
			{
				uint32 CompBufferOffset = FieldInfo->IntStartOffset;
				Src = DataSet->GetCurrentDataChecked().GetInstancePtrInt32(CompBufferOffset, RealRowIdx);
			}

			int32 Value = 0;
			if (Src)
			{
				Value = Src[0];
			}
			EntryWidget = SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("EnumValue","{0}({1})"), FieldInfo->Enum->GetDisplayNameTextByValue(Value), FText::AsNumber(Value)));

		}
		else
		{
			int32* Src = nullptr;
			if (UseGlobalOffsets)
			{
				const uint32 CompBufferOffset = FieldInfo->GlobalStartOffset;
				const uint8* SrcU8 = ParameterStore->GetParameterDataArray().GetData();
				Src = SrcU8 ? (int32*)(SrcU8 + CompBufferOffset) : nullptr;
			}
			else if (DataSet && DataSet->GetCurrentDataChecked().GetNumInstances() > 0)
			{
				uint32 CompBufferOffset = FieldInfo->IntStartOffset;
				Src = DataSet->GetCurrentDataChecked().GetInstancePtrInt32(CompBufferOffset, RealRowIdx);
			}

			int32 Value = 0;
			if (Src)
			{
				Value = Src[0];
			}
			EntryWidget = SNew(STextBlock)
				.Text(FText::AsNumber(Value));

		}
	}
	else if (!EntryWidget.IsValid())
	{
		EntryWidget = SNew(STextBlock)
			.Text(LOCTEXT("UnsupportedColumn", "n/a"));
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.Padding(3)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			EntryWidget.ToSharedRef()
		];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SNiagaraSpreadsheetView::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	TabState = UIPerParticleUpdate;
	ScriptEnum = StaticEnum<ENiagaraScriptUsage>();
	TargetComponent = InSystemViewModel->GetPreviewComponent();
	ensure(ScriptEnum);

	CaptureData.SetNum(UIMax);

	SystemViewModel = InSystemViewModel;
	SystemViewModel->GetSelectionViewModel()->OnEmitterHandleIdSelectionChanged().AddSP(this, &SNiagaraSpreadsheetView::SystemSelectionChanged);
	SystemViewModel->OnPostSequencerTimeChanged().AddRaw(this, &SNiagaraSpreadsheetView::OnSequencerTimeChanged);

	bInitialColumns = true;

	for (int32 i = 0; i < (int32)UIMax; i++)
	{
		switch (i)
		{
		case UIPerParticleUpdate:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleUpdate", "Particle Update");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		case UIPerParticleSpawn:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleSpawnScript;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleSpawn", "Particle Spawn");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		case UIPerParticleEvent0:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleEvent0", "Particle Event0");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		case UIPerParticleEvent1:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleEvent1", "Particle Event1");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		case UIPerParticleEvent2:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleEvent2", "Particle Event2");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		case UISystemUpdate:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::SystemUpdateScript;
			CaptureData[i].ColumnName = LOCTEXT("SystemUpdate", "System Update");
			CaptureData[i].bOutputColumnsAreAttributes = false;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		case UIPerParticleGPU:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleGPUComputeScript;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleGPU", "Particle GPU");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		default:
			CaptureData[i].TargetUsage = ENiagaraScriptUsage::Function;
			CaptureData[i].ColumnName = LOCTEXT("PerParticleUnknown", "Particle Unknown");
			CaptureData[i].bOutputColumnsAreAttributes = true;
			CaptureData[i].bInputColumnsAreAttributes = false;
			break;
		}

		CaptureData[i].OutputHorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal)
			.Thickness(FVector2D(12.0f, 12.0f));

		CaptureData[i].OutputVerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical)
			.Thickness(FVector2D(12.0f, 12.0f));

		CaptureData[i].InputHorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal)
			.Thickness(FVector2D(12.0f, 12.0f));

		CaptureData[i].InputVerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical)
			.Thickness(FVector2D(12.0f, 12.0f));

		SAssignNew(CaptureData[i].OutputsListView, STreeView< TSharedPtr<int32> >)
			.IsEnabled(this, &SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle)
			// List view items are this tall
			.ItemHeight(12)
			// Tell the list view where to get its source data
			.TreeItemsSource(&CaptureData[i].SupportedOutputIndices)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow(this, &SNiagaraSpreadsheetView::OnGenerateWidgetForList, (EUITab)i, false)
			// Given some DataItem, this is how we find out if it has any children and what they are.
			.OnGetChildren(this, &SNiagaraSpreadsheetView::OnGetChildrenForList, (EUITab)i, false)
			// Selection mode
			.SelectionMode(ESelectionMode::Single)
			.ExternalScrollbar(CaptureData[i].OutputVerticalScrollBar)
			.ConsumeMouseWheel(EConsumeMouseWheel::Always)
			.AllowOverscroll(EAllowOverscroll::No)
			// Selection callback
			.OnSelectionChanged(this, &SNiagaraSpreadsheetView::OnEventSelectionChanged, (EUITab)i, false)
			.HeaderRow
			(
				SAssignNew(CaptureData[i].OutputHeaderRow, SHeaderRow)
			);

		SAssignNew(CaptureData[i].OutputFilterButton, SComboButton)
			.HasDownArrow(false)
			.OnGetMenuContent(this, &SNiagaraSpreadsheetView::GetOutputFilterMenu, (EUITab)i)
			.ContentPadding(2)
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(LOCTEXT("SpreadSheetOutputFilterBox", "Filter Attributes"))
			];

		SAssignNew(CaptureData[i].InputsListView, STreeView< TSharedPtr<int32> >)
			.IsEnabled(this, &SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle)
			// List view items are this tall
			.ItemHeight(12)
			// Tell the list view where to get its source data
			.TreeItemsSource(&CaptureData[i].SupportedInputIndices)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow(this, &SNiagaraSpreadsheetView::OnGenerateWidgetForList, (EUITab)i, true)
			// Given some DataItem, this is how we find out if it has any children and what they are.
			.OnGetChildren(this, &SNiagaraSpreadsheetView::OnGetChildrenForList, (EUITab)i, true)
			// Selection mode
			.SelectionMode(ESelectionMode::Single)
			.ExternalScrollbar(CaptureData[i].InputVerticalScrollBar)
			.ConsumeMouseWheel(EConsumeMouseWheel::Always)
			.AllowOverscroll(EAllowOverscroll::No)
			// Selection callback
			.OnSelectionChanged(this, &SNiagaraSpreadsheetView::OnEventSelectionChanged, (EUITab)i, true)
			.HeaderRow
			(
				SAssignNew(CaptureData[i].InputHeaderRow, SHeaderRow)
			);

		SAssignNew(CaptureData[i].CheckBox, SCheckBox)
			//.Style(FAppStyle::Get(), "PlacementBrowser.Tab")
			.Style(FAppStyle::Get(), i == 0 ? "Property.ToggleButton.Start" : (i < CaptureData.Num() - 1 ? "Property.ToggleButton.Middle" : "Property.ToggleButton.End"))
			.OnCheckStateChanged(this, &SNiagaraSpreadsheetView::OnTabChanged, (EUITab)i)
			.Visibility(this, &SNiagaraSpreadsheetView::GetTabVisibility, (EUITab)i)
			.IsChecked(this, &SNiagaraSpreadsheetView::GetTabCheckedState, (EUITab)i)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.Padding(FMargin(6, 0, 15, 0))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					//.TextStyle(FAppStyle::Get(), "PlacementBrowser.Tab.Text")
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.AttributeSpreadsheetTabText")
					.Text(CaptureData[i].ColumnName)
				]
			];

		SAssignNew(CaptureData[i].Container, SVerticalBox)
			.Visibility(this, &SNiagaraSpreadsheetView::GetViewVisibility, (EUITab)i)
			+ SVerticalBox::Slot()
			.FillHeight(0.25f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					.ExternalScrollbar(CaptureData[i].InputHorizontalScrollBar)
					+ SScrollBox::Slot()
					[
						CaptureData[i].InputsListView.ToSharedRef()
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CaptureData[i].InputVerticalScrollBar.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CaptureData[i].InputHorizontalScrollBar.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CaptureData[i].OutputFilterButton.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					.ExternalScrollbar(CaptureData[i].OutputHorizontalScrollBar)
					+ SScrollBox::Slot()
					[
						CaptureData[i].OutputsListView.ToSharedRef()
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CaptureData[i].OutputVerticalScrollBar.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CaptureData[i].OutputHorizontalScrollBar.ToSharedRef()
			];
		}

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.OnClicked(this, &SNiagaraSpreadsheetView::OnCaptureRequestPressed)
							.Text(LOCTEXT("CaptureLabel", "Capture"))
							.ToolTipText(LOCTEXT("CaptureToolitp", "Press this button to capture one frame's contents. Can only capture CPU systems."))
							.IsEnabled(this, &SNiagaraSpreadsheetView::CanCapture)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.OnClicked(this, &SNiagaraSpreadsheetView::OnCSVOutputPressed)
							.Text(LOCTEXT("CSVOutput", "Copy For Excel"))
							.ToolTipText(LOCTEXT("CSVOutputToolitp", "Press this button to put the contents of this spreadsheet in the clipboard in an Excel-friendly format."))
							.IsEnabled(this, &SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						// Toolbar
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CaptureTargetLabel", "Target: "))
							.ToolTipText(LOCTEXT("TargetToolitp", "Select the actor that you wish to capture from."))
							.IsEnabled(this, &SNiagaraSpreadsheetView::CanCapture)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(SComboButton)
							.HAlign(HAlign_Center)
							.OnGetMenuContent(this, &SNiagaraSpreadsheetView::OnGetTargetMenuContent)
							.ContentPadding(1)
							.ToolTipText(LOCTEXT("TargetToolitp", "Select the actor that you wish to capture from."))
							.ButtonContent()
							[
								SNew(STextBlock)
								.Text(this, &SNiagaraSpreadsheetView::OnGetTargetButtonText)
							]
							.IsEnabled(this, &SNiagaraSpreadsheetView::CanCapture)
						]
					]
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoDataText", "Please press capture to examine data from a particular frame."))
					.Visibility_Lambda([&]() {
						if (IsPausedAtRightTimeOnRightHandle())
							return EVisibility::Collapsed;
						return EVisibility::Visible;
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SNiagaraSpreadsheetView::LastCapturedInfoText)
					.Visibility_Lambda([&]() {
						if (IsPausedAtRightTimeOnRightHandle())
							return EVisibility::Visible;
						return EVisibility::Collapsed;
					})
				]
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleUpdate].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleSpawn].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleEvent0].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleEvent1].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleEvent2].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UIPerParticleGPU].CheckBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						CaptureData[UISystemUpdate].CheckBox.ToSharedRef()
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleUpdate].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleSpawn].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleEvent0].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleEvent1].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleEvent2].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UIPerParticleGPU].Container.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		[
			CaptureData[UISystemUpdate].Container.ToSharedRef()
		]
	];
}

SNiagaraSpreadsheetView::~SNiagaraSpreadsheetView()
{
	if (SystemViewModel.IsValid())
	{
		if (SystemViewModel->GetSelectionViewModel() != nullptr)
		{
			SystemViewModel->GetSelectionViewModel()->OnEmitterHandleIdSelectionChanged().RemoveAll(this);
		}
		SystemViewModel->OnPostSequencerTimeChanged().RemoveAll(this);
	}
}

void SNiagaraSpreadsheetView::GetNameAndTooltip(const UNiagaraComponent* InComponent, FText& OutText, FText& OutTooltip) const
{
	const UNiagaraComponent* PreviewComponent = SystemViewModel->GetPreviewComponent();

	if (InComponent == nullptr)
	{
		OutText = LOCTEXT("NullComponentLabel", "Unknown");
		OutTooltip = LOCTEXT("NullComponentTooltip", "Unknown");
	}
	else if (PreviewComponent == InComponent)
	{
		OutText = LOCTEXT("PreviewComponentLabel", "Editor Viewport");
		OutTooltip = LOCTEXT("PreviewComponentTooltip", "The instance of the Niagara Component in the Niagara editor viewport.");
	}
	else
	{
		const UWorld* World = InComponent->GetWorld();
		const AActor* Actor = InComponent->GetOwner();
		OutText = FText::Format(LOCTEXT("SourceComponentLabel","World: \"{0}\" Actor: \"{1}\""), World ? FText::FromString(World->GetName()) : FText::GetEmpty(), Actor ? FText::FromString(Actor->GetName()) : FText::GetEmpty());
		OutTooltip = OutText;
	}
}

TSharedRef<SWidget> SNiagaraSpreadsheetView::OnGetTargetMenuContent() const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	const UNiagaraComponent* PreviewComponent = SystemViewModel->GetPreviewComponent();

	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		// Ignore dying or CDO versions of data..
		// No need to check the unreachable flag here as TObjectIterator already does that
		if (!IsValidChecked(*It) || It->HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
		{
			continue;
		}

		// Ignore any component not referencing this system.
		if (It->GetAsset() != &SystemViewModel->GetSystem())
		{
			continue;
		}

		// Ignore non-Niagara editor systems or non-PiE components
		const UWorld* World = It->GetWorld();
		bool bAdd = false;

		if (World && World->IsPlayInEditor())
		{
			if (It->GetForceSolo() == true || (It->GetSystemInstanceController().IsValid() && It->GetSystemInstanceController()->IsSolo()))
			{
				bAdd = true;
			}
		}
		else if (*It == PreviewComponent)
		{
			bAdd = true;
		}

		if (bAdd)
		{
			FText ComponentName;
			FText ComponentTooltip;
			GetNameAndTooltip(*It, ComponentName, ComponentTooltip);

			MenuBuilder.AddMenuEntry(
				ComponentName,
				ComponentTooltip,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(const_cast<SNiagaraSpreadsheetView*>(this), &SNiagaraSpreadsheetView::SetTarget, *It)));
		}
	}

	return MenuBuilder.MakeWidget();
}


void SNiagaraSpreadsheetView::SetTarget(UNiagaraComponent* InComponent)
{
	TargetComponent = InComponent;
	TargetRequestId.Invalidate();
	TargetCaptureData.Empty();

	for (int32 i = 0; i < (int32)UIMax; i++)
	{
		CaptureData[i].DataSet.Init(nullptr);
		CaptureData[i].InputParams.Reset();
		CaptureData[i].CaptureData.Reset();
	}
}

FText SNiagaraSpreadsheetView::OnGetTargetButtonText() const
{
	FText Text;
	FText Tooltip;
	GetNameAndTooltip(TargetComponent.Get(), Text, Tooltip);
	return Text;
}

void SNiagaraSpreadsheetView::OnTabChanged(ECheckBoxState State, EUITab Tab)
{
	if (State == ECheckBoxState::Checked)
	{
		TabState = Tab;
	}
}

ECheckBoxState SNiagaraSpreadsheetView::GetTabCheckedState(EUITab Tab) const
{
	return TabState == Tab ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SNiagaraSpreadsheetView::GetViewVisibility(EUITab Tab) const
{
	return TabState == Tab ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNiagaraSpreadsheetView::GetTabVisibility(EUITab Tab) const
{
	return (CaptureData[(int32)Tab].CaptureData.IsValid()) ? EVisibility::Visible : EVisibility::Collapsed;
}


TSharedRef< ITableRow > SNiagaraSpreadsheetView::OnGenerateWidgetForList(TSharedPtr<int32> InItem, const TSharedRef<STableViewBase>& OwnerTable, EUITab Tab, bool bInputList)
{
	if (CaptureData[(int32)Tab].CaptureData.IsValid() == false)
	{
		return SNew(SNiagaraSpreadsheetRow, OwnerTable)
			.RowIndex(*InItem)
			.ColumnsAreAttributes(CaptureData[(int32)Tab].bOutputColumnsAreAttributes)
			.DataSet(nullptr)
			.SupportedFields(CaptureData[(int32)Tab].SupportedOutputFields)
			.FieldInfoMap(CaptureData[(int32)Tab].OutputFieldInfoMap)
			.UseGlobalOffsets(false)
			.ParameterStore(nullptr);

	}

	if (bInputList)
	{
		return SNew(SNiagaraSpreadsheetRow, OwnerTable)
			.RowIndex(*InItem)
			.ColumnsAreAttributes(CaptureData[(int32)Tab].bInputColumnsAreAttributes)
			.DataSet(nullptr)
			.SupportedFields(CaptureData[(int32)Tab].SupportedInputFields)
			.FieldInfoMap(CaptureData[(int32)Tab].InputFieldInfoMap)
			.UseGlobalOffsets(true)
			.ParameterStore(&CaptureData[(int32)Tab].InputParams);
	}
	else
	{
		return SNew(SNiagaraSpreadsheetRow, OwnerTable)
			.RowIndex(*InItem)
			.ColumnsAreAttributes(CaptureData[(int32)Tab].bOutputColumnsAreAttributes)
			.DataSet(&CaptureData[(int32)Tab].DataSet)
			.SupportedFields(CaptureData[(int32)Tab].SupportedOutputFields)
			.FieldInfoMap(CaptureData[(int32)Tab].OutputFieldInfoMap)
			.UseGlobalOffsets(false)
			.ParameterStore(nullptr);
	}
}

FText SNiagaraSpreadsheetView::LastCapturedInfoText() const
{
	TArray<FGuid> SelectedEmitterHandleIds = SystemViewModel->GetSelectionViewModel()->GetSelectedEmitterHandleIds();
	if (SelectedEmitterHandleIds.Num() == 1 && SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle() && CaptureData[(int32)TabState].DataSet.GetCurrentData())
	{
		TSharedPtr<FNiagaraEmitterHandleViewModel> SelectedEmitterHandle = SystemViewModel->GetEmitterHandleViewModelById(SelectedEmitterHandleIds[0]);
		return FText::Format(LOCTEXT("LastCapturedInfoName", "Captured Emitter: \"{0}\"     # Particles: {1}    Script Type: {2}"),
			SelectedEmitterHandle->GetNameText(),
			FText::AsNumber(CaptureData[(int32)TabState].DataSet.GetCurrentDataChecked().GetNumInstances()),
			ScriptEnum->GetDisplayNameTextByValue((int64)CaptureData[(int32)TabState].TargetUsage));
	}

	return LOCTEXT("LastCapturedHandleNameStale", "Captured Info: Out-of-date");
}

void SNiagaraSpreadsheetView::OnGetChildrenForList(TSharedPtr<int32> InItem, TArray<TSharedPtr<int32>>& OutChildren, EUITab Tab, bool bInputList)
{
	OutChildren.Empty();
}


TSharedRef<SWidget> SNiagaraSpreadsheetView::GetOutputFilterMenu(EUITab Tab)
{
	FMenuBuilder MenuBuilder(false, nullptr);
	MenuBuilder.BeginSection("OutputAttributeActions", LOCTEXT("OutputAttributes", "Output Attributes"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AllOutputAttribute", "Toggle All"),
		LOCTEXT("AllOutputAttributeTooltip", "Toggle visibility for all attributes"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SNiagaraSpreadsheetView::ToggleAllOutputAttributes, Tab),
			FCanExecuteAction::CreateLambda([=] { return true; }),
			FIsActionChecked::CreateSP(this, &SNiagaraSpreadsheetView::AnyOutputAttributeEnabled, Tab)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	TArray<FNiagaraVariable> Variables = CaptureData[(int32)Tab].DataSet.GetVariables();
	for (const FNiagaraVariable& Var : Variables)
	{
		const FNiagaraTypeDefinition& TypeDef = Var.GetType();
		const UScriptStruct* Struct = TypeDef.GetScriptStruct();
		const UEnum* Enum = TypeDef.GetEnum();

		FNiagaraTypeLayoutInfo Layout;
		TArray<FName> PropertyNames;
		TArray<SNiagaraSpreadsheetView::FieldInfo> FieldInfos;

		GenerateLayoutInfo(Layout, Struct, Enum, Var.GetName(), PropertyNames, FieldInfos);

		for (int32 VarIdx = 0; VarIdx < PropertyNames.Num(); VarIdx++)
		{
			const FText PropertyText = FText::FromName(PropertyNames[VarIdx]);
			MenuBuilder.AddMenuEntry(
				PropertyText,
				FText::Format(LOCTEXT("OutputAttributeTooltip", "Toggle {0}"), PropertyText),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SNiagaraSpreadsheetView::ToggleFilterOutputAttribute, Tab, PropertyNames[VarIdx]),
					FCanExecuteAction::CreateLambda([=] { return true; }),
					FIsActionChecked::CreateSP(this, &SNiagaraSpreadsheetView::IsOutputAttributeEnabled, Tab, PropertyNames[VarIdx])),
				NAME_None,
				EUserInterfaceActionType::ToggleButton);
		}
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void SNiagaraSpreadsheetView::ToggleAllOutputAttributes(EUITab Tab)
{
	TArray<FName>& OutputFields = CaptureData[(int32)Tab].FilteredOutputFields;
	if (OutputFields.Num() > 0)
	{
		OutputFields.Empty();
	}
	else if (CaptureData[(int32)Tab].SupportedOutputFields.IsValid())
	{
		OutputFields = *CaptureData[(int32)Tab].SupportedOutputFields.Get();
	}

	ResetColumns(Tab);
}


void SNiagaraSpreadsheetView::ToggleFilterOutputAttribute(EUITab Tab, FName Item)
{
	const int32 Index = CaptureData[(int32)Tab].FilteredOutputFields.Find(Item);
	if (Index == INDEX_NONE)
	{
		CaptureData[(int32)Tab].FilteredOutputFields.AddUnique(Item);
	}
	else
	{
		CaptureData[(int32)Tab].FilteredOutputFields.RemoveAt(Index);
	}

	ResetColumns(Tab);
}

bool SNiagaraSpreadsheetView::AnyOutputAttributeEnabled(EUITab Tab)
{
	return CaptureData[(int32)Tab].FilteredOutputFields.Num() > 0;
}

bool SNiagaraSpreadsheetView::IsOutputAttributeEnabled(EUITab Tab, FName Item)
{
	return CaptureData[(int32)Tab].FilteredOutputFields.Find(Item) != INDEX_NONE;
}

void SNiagaraSpreadsheetView::SystemSelectionChanged()
{
	// Need to reset the attributes list...
	for (int32 i = 0; i < (int32)UIMax; i++)
	{
		CaptureData[i].DataSet.Init(nullptr);
		CaptureData[i].SupportedInputIndices.SetNum(0);
		CaptureData[i].SupportedOutputIndices.SetNum(0);
		CaptureData[i].OutputsListView->RequestTreeRefresh();
		CaptureData[i].InputsListView->RequestTreeRefresh();
	}

}

FReply SNiagaraSpreadsheetView::OnCSVOutputPressed()
{
	if (CaptureData[(int32)TabState].SupportedOutputFields.IsValid() && CaptureData[(int32)TabState].OutputFieldInfoMap.IsValid() && IsPausedAtRightTimeOnRightHandle())
	{
		FString CSVOutput;
		int32 SkipIdx = -1;
		int32 NumWritten = 0;
		TArray<const SNiagaraSpreadsheetView::FieldInfo*> FieldInfos;
		FieldInfos.SetNum(CaptureData[(int32)TabState].SupportedOutputFields->Num());
		FString DelimiterString = TEXT("\t");
		for (int32 i = 0; i < CaptureData[(int32)TabState].SupportedOutputFields->Num(); i++)
		{
			FName Field = (*CaptureData[(int32)TabState].SupportedOutputFields)[i];
			if (Field == ARRAY_INDEX_COLUMN_NAME)
			{
				SkipIdx = i;
				continue;
			}

			if (NumWritten != 0)
			{
				CSVOutput += DelimiterString;
			}

			FieldInfos[i] = CaptureData[(int32)TabState].OutputFieldInfoMap->Find(Field);

			CSVOutput += Field.ToString();
			NumWritten++;
		}

		CSVOutput += "\r\n";

		for (uint32 RowIndex = 0; RowIndex < CaptureData[(int32)TabState].DataSet.GetCurrentDataChecked().GetNumInstances(); RowIndex++)
		{
			NumWritten = 0;
			for (int32 i = 0; i < CaptureData[(int32)TabState].SupportedOutputFields->Num(); i++)
			{
				if (i == SkipIdx)
				{
					continue;
				}

				if (NumWritten != 0)
				{
					CSVOutput += DelimiterString;
				}
				const SNiagaraSpreadsheetView::FieldInfo* FieldInfo = FieldInfos[i];

				if (FieldInfo != nullptr && CaptureData[(int32)TabState].DataSet.GetCurrentDataChecked().GetNumInstances() != 0)
				{
					if (FieldInfo->bFloat)
					{
						if (FieldInfo->bHalf)
						{
							uint32 CompBufferOffset = FieldInfo->FloatStartOffset;
							FFloat16* Src = CaptureData[(int32)TabState].DataSet.GetCurrentDataChecked().GetInstancePtrHalf(CompBufferOffset, RowIndex);
							CSVOutput += FString::Printf(TEXT("%3.9f"), float(Src[0]));
						}
						else
						{
							uint32 CompBufferOffset = FieldInfo->FloatStartOffset;
							float* Src = CaptureData[(int32)TabState].DataSet.GetCurrentDataChecked().GetInstancePtrFloat(CompBufferOffset, RowIndex);
							CSVOutput += FString::Printf(TEXT("%3.9f"), Src[0]);
						}
					}
					else
					{
						uint32 CompBufferOffset = FieldInfo->IntStartOffset;
						int32* Src = CaptureData[(int32)TabState].DataSet.GetCurrentDataChecked().GetInstancePtrInt32(CompBufferOffset, RowIndex);
						CSVOutput += FString::Printf(TEXT("%d"), Src[0]);
					}
				}
				NumWritten++;
			}

			CSVOutput += "\r\n";
		}

		FPlatformApplicationMisc::ClipboardCopy(*CSVOutput);
	}

	return FReply::Handled();
}
void SNiagaraSpreadsheetView::OnSequencerTimeChanged()
{
	HandleTimeChange();
}

void SNiagaraSpreadsheetView::Tick(float DeltaTime)
{
	HandleTimeChange();
}

void SNiagaraSpreadsheetView::HandleTimeChange()
{
	FNiagaraSystemInstanceControllerPtr SystemInstanceController = TargetComponent.IsValid() ? TargetComponent->GetSystemInstanceController() : nullptr;
	FNiagaraSystemInstance* SystemInstance = SystemInstanceController.IsValid() ? SystemInstanceController->GetSoloSystemInstance() : nullptr;
	if (TargetRequestId.IsValid() && SystemInstance)
	{
		if (SystemInstance->QueryCaptureResults(TargetRequestId, TargetCaptureData))
		{
			for (int32 i = 0; i < (int32)UIMax; i++)
			{
				TArray<FGuid> SelectedEmitterHandleIds = SystemViewModel->GetSelectionViewModel()->GetSelectedEmitterHandleIds();
				if (SelectedEmitterHandleIds.Num() == 1)
				{
					TSharedPtr<FNiagaraEmitterHandleViewModel> SelectedEmitterHandle = SystemViewModel->GetEmitterHandleViewModelById(SelectedEmitterHandleIds[0]);
					FName EntryName = NAME_None;
					if (i != UISystemUpdate)
					{
						auto EmitterInstances = SystemInstance->GetEmitters();
						for (auto EmitterInstance : EmitterInstances)
						{
							if (SelectedEmitterHandle->GetEmitterHandle()->GetInstance() == EmitterInstance->GetCachedEmitter())
							{
								EntryName = EmitterInstance->GetCachedIDName();
							}
						}

						if (EntryName.IsNone())
						{
							EntryName = SelectedEmitterHandle->GetEmitterHandle()->GetIdName();
						}
					}
					else //if (i == UISystemUpdate)
					{
						EntryName = NAME_None;
					}

					TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>* FoundEntry = TargetCaptureData.FindByPredicate([&](const TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>& Entry)
					{
						return Entry->HandleName == EntryName && UNiagaraScript::IsEquivalentUsage(Entry->Usage, CaptureData[i].TargetUsage) && Entry->UsageId == CaptureData[i].TargetUsageId;
					});

					if (FoundEntry != nullptr)
					{
						CaptureData[i].CaptureData = *FoundEntry;
						CaptureData[i].CaptureData->Frame.CopyTo(CaptureData[i].DataSet);
						CaptureData[i].InputParams = CaptureData[i].CaptureData->Parameters;
						CaptureData[i].LastCaptureHandleId = SelectedEmitterHandle->GetId();

						ResetColumns((EUITab)i);
						ResetEntries((EUITab)i);
					}
					else
					{
						CaptureData[i].CaptureData.Reset();
						CaptureData[i].DataSet.Init(nullptr);

						ResetColumns((EUITab)i);
						ResetEntries((EUITab)i);
					}
				}
			}
			TargetRequestId.Invalidate();
		}
	}
}

bool SNiagaraSpreadsheetView::IsTickable() const
{
	return TargetRequestId.IsValid();
}

TStatId SNiagaraSpreadsheetView::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(SNiagaraSpreadsheetView, STATGROUP_Tickables);
}

bool SNiagaraSpreadsheetView::CanCapture() const
{
	TArray<FGuid> SelectedEmitterHandleIds = SystemViewModel->GetSelectionViewModel()->GetSelectedEmitterHandleIds();
	if (SelectedEmitterHandleIds.Num() == 1)
	{
		TSharedPtr<FNiagaraEmitterHandleViewModel> SelectedEmitterHandle = SystemViewModel->GetEmitterHandleViewModelById(SelectedEmitterHandleIds[0]);
		FNiagaraEmitterHandle* Handle = SelectedEmitterHandle->GetEmitterHandle();
		if (Handle )
		{
			return true;
		}
	}
	return false;
}


bool SNiagaraSpreadsheetView::IsPausedAtRightTimeOnRightHandle() const
{
	TArray<FGuid> SelectedEmitterHandleIds = SystemViewModel->GetSelectionViewModel()->GetSelectedEmitterHandleIds();
	if (SelectedEmitterHandleIds.Num() == 1)
	{
		TSharedPtr<FNiagaraEmitterHandleViewModel> SelectedEmitterHandle = SystemViewModel->GetEmitterHandleViewModelById(SelectedEmitterHandleIds[0]);
		return SystemViewModel->GetSequencer()->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped &&
			CaptureData[(int32)TabState].CaptureData.IsValid() &&
			CaptureData[(int32)TabState].LastCaptureHandleId == SelectedEmitterHandle->GetId();
	}
	return false;
}

void SNiagaraSpreadsheetView::ResetEntries(EUITab Tab)
{
	{
		{
			int32 NumInstances = CaptureData[(int32)Tab].DataSet.GetCurrentData() ? CaptureData[(int32)Tab].DataSet.GetCurrentDataChecked().GetNumInstances() : 0;
			if (!CaptureData[(int32)Tab].bOutputColumnsAreAttributes && CaptureData[(int32)Tab].SupportedOutputFields.IsValid())
			{
				NumInstances = CaptureData[(int32)Tab].SupportedOutputFields->Num();
			}

			CaptureData[(int32)Tab].SupportedOutputIndices.SetNum(NumInstances);

			for (int32 i = 0; i < NumInstances; i++)
			{
				CaptureData[(int32)Tab].SupportedOutputIndices[i] = MakeShared<int32>(i);
			}

			CaptureData[(int32)Tab].OutputsListView->RequestTreeRefresh();
		}

		{
			int32 NumInstances = CaptureData[(int32)Tab].InputParams.ReadParameterVariables().Num();
			if (!CaptureData[(int32)Tab].bInputColumnsAreAttributes && CaptureData[(int32)Tab].SupportedInputFields.IsValid())
			{
				NumInstances = CaptureData[(int32)Tab].SupportedInputFields->Num();
			}

			CaptureData[(int32)Tab].SupportedInputIndices.SetNum(NumInstances);

			for (int32 i = 0; i < NumInstances; i++)
			{
				CaptureData[(int32)Tab].SupportedInputIndices[i] = MakeShared<int32>(i);
			}

			CaptureData[(int32)Tab].InputsListView->RequestTreeRefresh();
		}
	}
}

void SNiagaraSpreadsheetView::GenerateLayoutInfo(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct, const UEnum* Enum,  FName BaseName, TArray<FName>& PropertyNames, TArray<SNiagaraSpreadsheetView::FieldInfo>& FieldInfo)
{
	TFieldIterator<FProperty> PropertyCountIt(Struct, EFieldIteratorFlags::IncludeSuper);
	int32 NumProperties = 0;
	for (; PropertyCountIt; ++PropertyCountIt)
	{
		NumProperties++;
	}

	for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		FName PropertyName = (NumProperties == 1) ? *(BaseName.ToString()) : *(BaseName.ToString() + "." + Property->GetName());
		if (Property->IsA(FFloatProperty::StaticClass()))
		{
			SNiagaraSpreadsheetView::FieldInfo Info;
			Info.bFloat = true;
			Info.bHalf = false;
			Info.bBoolean = false;
			Info.FloatStartOffset = Layout.FloatComponentRegisterOffsets.Num();
			Info.IntStartOffset = UINT_MAX;
			Info.GlobalStartOffset = sizeof(float) * Layout.FloatComponentRegisterOffsets.Num() + sizeof(int32) * Layout.Int32ComponentByteOffsets.Num();
			Info.Enum = nullptr;
			FieldInfo.Add(Info);

			Layout.FloatComponentRegisterOffsets.Add(Layout.FloatComponentByteOffsets.Num());
			Layout.FloatComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			PropertyNames.Add(PropertyName);
		}
		else if (Property->IsA(FIntProperty::StaticClass()) || Property->IsA(FBoolProperty::StaticClass()))
		{
			SNiagaraSpreadsheetView::FieldInfo Info;
			Info.bFloat = false;
			Info.bHalf = false;
			Info.bBoolean = Property->IsA(FBoolProperty::StaticClass()) || (Struct == FNiagaraTypeDefinition::GetBoolStruct());
			Info.FloatStartOffset = UINT_MAX;
			Info.IntStartOffset = Layout.Int32ComponentRegisterOffsets.Num();
			Info.GlobalStartOffset = sizeof(float) * Layout.FloatComponentRegisterOffsets.Num() + sizeof(int32) * Layout.Int32ComponentByteOffsets.Num();
			Info.Enum = Enum;
			FieldInfo.Add(Info);

			Layout.Int32ComponentRegisterOffsets.Add(Layout.Int32ComponentByteOffsets.Num());
			Layout.Int32ComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			PropertyNames.Add(PropertyName);
		}
		else if (Property->IsA(FUInt16Property::StaticClass()))
		{
			SNiagaraSpreadsheetView::FieldInfo Info;
			Info.bHalf = true;
			Info.bFloat = true;
			Info.bBoolean = false;
			Info.FloatStartOffset = Layout.HalfComponentRegisterOffsets.Num();
			Info.IntStartOffset = UINT_MAX;
			Info.GlobalStartOffset = sizeof(float) * Layout.FloatComponentRegisterOffsets.Num() + sizeof(int32) * Layout.Int32ComponentRegisterOffsets.Num() + sizeof(FFloat16) * Layout.HalfComponentByteOffsets.Num();
			Info.Enum = Enum;
			FieldInfo.Add(Info);

			Layout.HalfComponentRegisterOffsets.Add(Layout.HalfComponentByteOffsets.Num());
			Layout.HalfComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			PropertyNames.Add(PropertyName);
		}
		else if (Property->IsA(FEnumProperty::StaticClass()))
		{
			FEnumProperty* EnumProp = CastFieldChecked<FEnumProperty>(Property);
			GenerateLayoutInfo(Layout, FNiagaraTypeDefinition::GetIntStruct(), EnumProp->GetEnum(), PropertyName, PropertyNames, FieldInfo);
		}
		else if (Property->IsA(FStructProperty::StaticClass()))
		{
			FStructProperty* StructProp = CastFieldChecked<FStructProperty>(Property);
			GenerateLayoutInfo(Layout, FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(StructProp->Struct, ENiagaraStructConversion::Simulation), nullptr, PropertyName, PropertyNames, FieldInfo);
		}
		else
		{
			check(false);
		}
	}
}

void SNiagaraSpreadsheetView::ResetColumns(EUITab Tab)
{
	int32 i = Tab;

	if (CaptureData[i].DataSet.GetCurrentData() && CaptureData[i].DataSet.GetCurrentData()->GetNumInstances() != 0)
	{
		float ManualWidth = 125.0f;

		// Handle output columns
		{
			CaptureData[i].OutputHeaderRow->ClearColumns();

			const TArray<FName>& PreviousSupportedFields = CaptureData[i].SupportedOutputFields.IsValid() ? *CaptureData[i].SupportedOutputFields.Get() : TArray<FName>();
			CaptureData[i].SupportedOutputFields = MakeShared<TArray<FName> >();
			CaptureData[i].OutputFieldInfoMap = MakeShared<TMap<FName, FieldInfo> >();
			uint32 TotalFloatComponents = 0;
			uint32 TotalInt32Components = 0;
			uint32 TotalHalfComponents = 0;

			TArray<FNiagaraVariable> Variables = CaptureData[i].DataSet.GetVariables();

			TArray<FName> ColumnNames;

			if (CaptureData[i].bOutputColumnsAreAttributes)
			{
				ColumnNames.Add(ARRAY_INDEX_COLUMN_NAME);
			}
			else
			{
				ManualWidth = 125.0f;
				ColumnNames.Add(OUTPUT_KEY_COLUMN_NAME);
				ColumnNames.Add(VALUE_COLUMN_NAME);
				ColumnNames.Add(FILLER_COLUMN_NAME);
			}

			for (const FNiagaraVariable& Var : Variables)
			{
				FNiagaraTypeDefinition TypeDef = Var.GetType();
				const UScriptStruct* Struct = TypeDef.GetScriptStruct();
				const UEnum* Enum = TypeDef.GetEnum();

				FNiagaraTypeLayoutInfo Layout;
				TArray<FName> PropertyNames;
				TArray<SNiagaraSpreadsheetView::FieldInfo> FieldInfos;

				uint32 TotalFloatComponentsBeforeStruct = TotalFloatComponents;
				uint32 TotalInt32ComponentsBeforeStruct = TotalInt32Components;
				uint32 TotalHalfComponentsBeforeStruct = TotalHalfComponents;

				GenerateLayoutInfo(Layout, Struct, Enum, Var.GetName(), PropertyNames, FieldInfos);

				for (int32 VarIdx = 0; VarIdx < PropertyNames.Num(); VarIdx++)
				{
					const FName PropertyName = PropertyNames[VarIdx];
					if (FieldInfos[VarIdx].bFloat)
					{
						if (FieldInfos[VarIdx].bHalf)
						{
							FieldInfos[VarIdx].FloatStartOffset += TotalFloatComponentsBeforeStruct;
							TotalFloatComponents++;
						}
						else
						{
							FieldInfos[VarIdx].FloatStartOffset += TotalHalfComponentsBeforeStruct;
							TotalHalfComponents++;
						}
					}
					else
					{
						FieldInfos[VarIdx].IntStartOffset += TotalInt32ComponentsBeforeStruct;
						TotalInt32Components++;
					}

					CaptureData[i].SupportedOutputFields->Add(PropertyName);
					CaptureData[i].OutputFieldInfoMap->Add(PropertyName, FieldInfos[VarIdx]);

					// Show new attributes
					if (!bInitialColumns && PreviousSupportedFields.Find(PropertyName) == INDEX_NONE)
					{
						CaptureData[(int32)Tab].FilteredOutputFields.AddUnique(PropertyName);
					}

					if (CaptureData[i].bOutputColumnsAreAttributes
						&& (bInitialColumns || IsOutputAttributeEnabled(Tab, PropertyName)))
					{
						ColumnNames.Add(PropertyName);
					}
				}
			}

			if (bInitialColumns)
			{
				CaptureData[(int32)Tab].FilteredOutputFields = *CaptureData[(int32)Tab].SupportedOutputFields.Get();
				bInitialColumns = false;
			}

			for (int32 ColIdx = 0; ColIdx < ColumnNames.Num(); ColIdx++)
			{
				FName ColumnName = ColumnNames[ColIdx];
				SHeaderRow::FColumn::FArguments ColumnArgs;
				ColumnArgs
					.ColumnId(ColumnName)
					.SortMode(EColumnSortMode::None)
					.HAlignHeader(HAlign_Center)
					.VAlignHeader(VAlign_Fill)
					.HeaderContentPadding(TOptional<FMargin>(2.0f))
					.HAlignCell(HAlign_Fill)
					.VAlignCell(VAlign_Fill);

				if (ColumnName != FILLER_COLUMN_NAME)
				{
					ColumnArgs.DefaultLabel(FText::FromName(ColumnName));
					ColumnArgs.ManualWidth(ManualWidth);
				}
				else
				{
					ColumnArgs.DefaultLabel(FText::FromString(TEXT(" ")));
					ColumnArgs.ManualWidth(ManualWidth);
				}
				CaptureData[i].OutputHeaderRow->AddColumn(ColumnArgs);
			}

			CaptureData[i].OutputHeaderRow->ResetColumnWidths();
			CaptureData[i].OutputHeaderRow->RefreshColumns();
			CaptureData[i].OutputsListView->RequestTreeRefresh();
		}

		// Handle input columns
		{
			CaptureData[i].InputHeaderRow->ClearColumns();


			CaptureData[i].SupportedInputFields = MakeShared<TArray<FName> >();
			CaptureData[i].InputFieldInfoMap = MakeShared<TMap<FName, FieldInfo> >();

			TArray<FNiagaraVariable> Variables;
			CaptureData[i].InputParams.GetParameters(Variables);

			TArray<FName> ColumnNames;

			if (CaptureData[i].bInputColumnsAreAttributes)
			{
				ColumnNames.Add(ARRAY_INDEX_COLUMN_NAME);
			}
			else
			{
				ManualWidth = 125.0f;
				ColumnNames.Add(INPUT_KEY_COLUMN_NAME);
				ColumnNames.Add(VALUE_COLUMN_NAME);
				ColumnNames.Add(FILLER_COLUMN_NAME);
			}

			for (const FNiagaraVariable& Var : Variables)
			{
				FNiagaraTypeDefinition TypeDef = Var.GetType();
				const UScriptStruct* Struct = TypeDef.GetScriptStruct();
				const UEnum* Enum = TypeDef.GetEnum();

				FNiagaraTypeLayoutInfo Layout;
				TArray<FName> PropertyNames;
				TArray<SNiagaraSpreadsheetView::FieldInfo> FieldInfos;

				int32 ByteOffset = CaptureData[i].InputParams.IndexOf(Var);

				GenerateLayoutInfo(Layout, Struct, Enum, Var.GetName(), PropertyNames, FieldInfos);

				for (int32 VarIdx = 0; VarIdx < PropertyNames.Num(); VarIdx++)
				{

					{

						FieldInfos[VarIdx].GlobalStartOffset += ByteOffset;

						CaptureData[i].SupportedInputFields->Add(PropertyNames[VarIdx]);
						CaptureData[i].InputFieldInfoMap->Add(PropertyNames[VarIdx], FieldInfos[VarIdx]);
					}

					if (CaptureData[i].bInputColumnsAreAttributes)
					{
						ColumnNames.Add(PropertyNames[VarIdx]);
					}
				}
			}


			for (int32 ColIdx = 0; ColIdx < ColumnNames.Num(); ColIdx++)
			{
				FName ColumnName = ColumnNames[ColIdx];
				SHeaderRow::FColumn::FArguments ColumnArgs;
				ColumnArgs
					.ColumnId(ColumnName)
					.SortMode(EColumnSortMode::None)
					.HAlignHeader(HAlign_Center)
					.VAlignHeader(VAlign_Fill)
					.HeaderContentPadding(TOptional<FMargin>(2.0f))
					.HAlignCell(HAlign_Fill)
					.VAlignCell(VAlign_Fill);

				if (ColumnName != FILLER_COLUMN_NAME)
				{
					ColumnArgs.DefaultLabel(FText::FromName(ColumnName));
					ColumnArgs.ManualWidth(ManualWidth);
				}
				else
				{
					ColumnArgs.DefaultLabel(FText::FromString(TEXT(" ")));
					ColumnArgs.ManualWidth(ManualWidth);
				}
				CaptureData[i].InputHeaderRow->AddColumn(ColumnArgs);
			}

			CaptureData[i].InputHeaderRow->ResetColumnWidths();
			CaptureData[i].InputHeaderRow->RefreshColumns();
			CaptureData[i].InputsListView->RequestTreeRefresh();
		}
	}
}

FReply SNiagaraSpreadsheetView::OnCaptureRequestPressed()
{
	FFrameRate TickResolution = SystemViewModel->GetSequencer()->GetFocusedTickResolution();
	float LocalTime = SystemViewModel->GetSequencer()->GetLocalTime().AsSeconds();

	// The preview component in the editor is using the 'DesiredAge' update mode so each frame it determines if the difference
	// between the current age and the desired age is greater then the seek delta and if so it advanced the simulation the correct
	// number of times.  We want to ensure that we simulate a single step so we get the seek delta from the component and add that
	// to the current time.
	float SimulationStep = SystemViewModel->GetPreviewComponent()->GetSeekDelta();
	float TargetCaptureTime = LocalTime + SimulationStep;

	TArray<FGuid> SelectedEmitterHandleIds = SystemViewModel->GetSelectionViewModel()->GetSelectedEmitterHandleIds();
	ensure(SelectedEmitterHandleIds.Num() == 1);

	FNiagaraSystemInstanceControllerPtr SystemInstanceController = TargetComponent.IsValid() ? TargetComponent->GetSystemInstanceController() : nullptr;
	FNiagaraSystemInstance* SystemInstance = SystemInstanceController.IsValid() ? SystemInstanceController->GetSoloSystemInstance() : nullptr;
	if (SystemInstance && SelectedEmitterHandleIds.Num() == 1)
	{
		TargetRequestId = FGuid::NewGuid();
		SystemInstance->RequestCapture(TargetRequestId);

		TSharedPtr<FNiagaraEmitterHandleViewModel> SelectedEmitterHandle = SystemViewModel->GetEmitterHandleViewModelById(SelectedEmitterHandleIds[0]);
		FVersionedNiagaraEmitter Emitter = SelectedEmitterHandle->GetEmitterHandle()->GetInstance();
		FVersionedNiagaraEmitterData* EmitterData = Emitter.GetEmitterData();

		for (int32 i = 0; i < CaptureData.Num(); i++)
		{
			CaptureData[i].DataSource = Emitter.ToWeakPtr();
			switch (i)
			{
				case UIPerParticleUpdate:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
					CaptureData[i].TargetUsageId = FGuid();
					break;
				case UIPerParticleSpawn:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleSpawnScript;
					CaptureData[i].TargetUsageId = FGuid();
					break;
				case UIPerParticleEvent0:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
					CaptureData[i].TargetUsageId = EmitterData->GetEventHandlers().Num() >= 1 ? EmitterData->GetEventHandlers() [0].Script->GetUsageId() : FGuid();
					break;
				case UIPerParticleEvent1:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
					CaptureData[i].TargetUsageId = EmitterData->GetEventHandlers().Num() >= 2 ? EmitterData->GetEventHandlers()[1].Script->GetUsageId() : FGuid();
					break;
				case UIPerParticleEvent2:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleEventScript;
					CaptureData[i].TargetUsageId = EmitterData->GetEventHandlers().Num() >= 3 ? EmitterData->GetEventHandlers()[2].Script->GetUsageId() : FGuid();
					break;
				case UISystemUpdate:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::SystemUpdateScript;
					CaptureData[i].TargetUsageId = FGuid();
					break;
				case UIPerParticleGPU:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::ParticleGPUComputeScript;
					CaptureData[i].TargetUsageId = FGuid();
					break;
				default:
					CaptureData[i].TargetUsage = ENiagaraScriptUsage::Function;
					CaptureData[i].TargetUsageId = FGuid();
					break;
			}
		}

		UWorld* World = TargetComponent->GetWorld();
		if (World && World->IsPlayInEditor())
		{
			if (FPlayWorldCommandCallbacks::IsInPIE())
			{
				if (FPlayWorldCommandCallbacks::IsInPIE_AndRunning())
				{
					// Need to Pause
					if (FPlayWorldCommandCallbacks::HasPlayWorld())
					{
						FPlayWorldCommandCallbacks::PausePlaySession_Clicked();
					}
				}

				// Need to single-step once
				if (FPlayWorldCommandCallbacks::HasPlayWorldAndPaused())
				{
					FPlayWorldCommandCallbacks::SingleFrameAdvance_Clicked();
				}

			}
		}
	}

	if (SystemViewModel->GetSequencer()->GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped)
	{
		SystemViewModel->GetSequencer()->SetLocalTime(TargetCaptureTime * TickResolution, STM_None);
	}
	else
	{
		SystemViewModel->GetSequencer()->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
		SystemViewModel->GetSequencer()->SetLocalTime(TargetCaptureTime * TickResolution, STM_None);
	}

	return FReply::Handled();
}

void SNiagaraSpreadsheetView::OnEventSelectionChanged(TSharedPtr<int32> Selection, ESelectInfo::Type /*SelectInfo*/, EUITab /*Tab*/, bool /*bInputList*/)
{
	if (Selection.IsValid())
	{
		// Do nothing for now
	}
}

#undef LOCTEXT_NAMESPACE
