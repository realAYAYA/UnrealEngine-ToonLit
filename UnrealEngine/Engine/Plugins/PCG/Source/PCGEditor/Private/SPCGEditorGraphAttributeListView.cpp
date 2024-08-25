// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphAttributeListView.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

#include "PCGEditor.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNodeBase.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphAttributeListView"

static TAutoConsoleVariable<bool> CVarShowAdvancedAttributesFields(
	TEXT("pcg.graph.ShowAdvancedAttributes"),
	false,
	TEXT("Control whether advanced attributes/properties are shown in the PCG graph editor"));

namespace PCGEditorGraphAttributeListView
{
	const FString NoneAttributeId = TEXT("@None");
	const FText NoPinAvailableText = LOCTEXT("NoPinAvailableText", "No pins");
	const FText LastLabelFormat = LOCTEXT("LastLabelFormat", "Last attribute: {0} / {1}");
	const FText NoDataAvailableText = LOCTEXT("NoDataAvailableText", "No data available");
	const FText NoNodeInspectedText = LOCTEXT("NoNodeInspectedText", "No node being inspected");
	const FText NoNodeInspectedToolTip = LOCTEXT("NoNodeInspectedToolTip", "Inspect a node using the right click menu");

	/** Names of the columns in the attribute list */
	const FName NAME_Index = FName(TEXT("$Index"));
	const FName NAME_PointPositionX = FName(TEXT("$Position.X"));
	const FName NAME_PointPositionY = FName(TEXT("$Position.Y"));
	const FName NAME_PointPositionZ = FName(TEXT("$Position.Z"));
	const FName NAME_PointRotationRoll = FName(TEXT("$Rotation.Roll"));
	const FName NAME_PointRotationPitch = FName(TEXT("$Rotation.Pitch"));
	const FName NAME_PointRotationYaw = FName(TEXT("$Rotation.Yaw"));
	const FName NAME_PointScaleX = FName(TEXT("$Scale.X"));
	const FName NAME_PointScaleY = FName(TEXT("$Scale.Y"));
	const FName NAME_PointScaleZ = FName(TEXT("$Scale.Z"));
	const FName NAME_PointBoundsMinX = FName(TEXT("$BoundsMin.X"));
	const FName NAME_PointBoundsMinY = FName(TEXT("$BoundsMin.Y"));
	const FName NAME_PointBoundsMinZ = FName(TEXT("$BoundsMin.Z"));
	const FName NAME_PointBoundsMaxX = FName(TEXT("$BoundsMax.X"));
	const FName NAME_PointBoundsMaxY = FName(TEXT("$BoundsMax.Y"));
	const FName NAME_PointBoundsMaxZ = FName(TEXT("$BoundsMax.Z"));
	const FName NAME_PointColorR = FName(TEXT("$Color.R"));
	const FName NAME_PointColorG = FName(TEXT("$Color.G"));
	const FName NAME_PointColorB = FName(TEXT("$Color.B"));
	const FName NAME_PointColorA = FName(TEXT("$Color.A"));
	const FName NAME_PointDensity = FName(TEXT("$Density"));
	const FName NAME_PointSteepness = FName(TEXT("$Steepness"));
	const FName NAME_PointSeed = FName(TEXT("$Seed"));
	const FName NAME_PointMetadataEntry = FName(TEXT("MetadataEntry"));
	const FName NAME_PointMetadataEntryParent = FName(TEXT("PointMetadataEntryParent"));

	/** Labels of the columns */
	const FText TEXT_IndexLabel = LOCTEXT("IndexLabel", "Index");
	const FText TEXT_PointPositionLabelX = LOCTEXT("PointPositionLabelX", "Position.X");
	const FText TEXT_PointPositionLabelY = LOCTEXT("PointPositionLabelY", "Position.Y");
	const FText TEXT_PointPositionLabelZ = LOCTEXT("PointPositionLabelZ", "Position.Z");
	const FText TEXT_PointRotationLabelRoll = LOCTEXT("PointRotationLabelRoll", "Rotation.Roll");
	const FText TEXT_PointRotationLabelPitch = LOCTEXT("PointRotationLabelPitch", "Rotation.Pitch");
	const FText TEXT_PointRotationLabelYaw = LOCTEXT("PointRotationLabelYaw", "Rotation.Yaw");
	const FText TEXT_PointScaleLabelX = LOCTEXT("PointScaleLabelX", "Scale.X");
	const FText TEXT_PointScaleLabelY = LOCTEXT("PointScaleLabelY", "Scale.Y");
	const FText TEXT_PointScaleLabelZ = LOCTEXT("PointScaleLabelZ", "Scale.Z");
	const FText TEXT_PointBoundsLabelMinX = LOCTEXT("PointBoundsMinX", "BoundsMin.X");
	const FText TEXT_PointBoundsLabelMinY = LOCTEXT("PointBoundsMinY", "BoundsMin.Y");
	const FText TEXT_PointBoundsLabelMinZ = LOCTEXT("PointBoundsMinZ", "BoundsMin.Z");
	const FText TEXT_PointBoundsLabelMaxX = LOCTEXT("PointBoundsMaxX", "BoundsMax.X");
	const FText TEXT_PointBoundsLabelMaxY = LOCTEXT("PointBoundsMaxY", "BoundsMax.Y");
	const FText TEXT_PointBoundsLabelMaxZ = LOCTEXT("PointBoundsMaxZ", "BoundsMax.Z");
	const FText TEXT_PointColorLabelR = LOCTEXT("PointColorR", "Color.R");
	const FText TEXT_PointColorLabelG = LOCTEXT("PointColorG", "Color.G");
	const FText TEXT_PointColorLabelB = LOCTEXT("PointColorB", "Color.B");
	const FText TEXT_PointColorLabelA = LOCTEXT("PointColorA", "Color.A");
	const FText TEXT_PointDensityLabel = LOCTEXT("PointDensityLabel", "Density");
	const FText TEXT_PointSteepnessLabel = LOCTEXT("PointSteepnessLabel", "Steepness");
	const FText TEXT_PointSeedLabel = LOCTEXT("PointSeedLabel", "Seed");
	const FText TEXT_PointMetadataEntryLabel = LOCTEXT("PointMetadataEntryLabel", "Entry Key");
	const FText TEXT_PointMetadataEntryParentLabel = LOCTEXT("PointMetadataEntryParentLabel", "Parent Key");

	constexpr float MaxColumnWidth = 200.0f;

	bool IsGraphCacheDebuggingEnabled()
	{
		UWorld* World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
		UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World);
		return Subsystem && Subsystem->IsGraphCacheDebuggingEnabled();
	}

	float CalculateColumnWidth(const FText& InText)
	{
		check(FSlateApplication::Get().GetRenderer());
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateFontInfo FontInfo = FAppStyle::GetFontStyle(TEXT("NormalText"));

		const float TextWidth = FontMeasure->Measure(InText, FontInfo).X;
		constexpr float ColumnPadding = 22.0f; // TODO: Grab padding from header style
		const float ColumnWidth = TextWidth + ColumnPadding;
		return FMath::Min(ColumnWidth, MaxColumnWidth);
	}
}

bool FPCGListViewUpdater::IsCompleted() const
{
	return UpdateTask.IsCompleted();
}

void FPCGListViewUpdater::Launch()
{
	// Passing a shared pointer to this in order for the task to keep the object alive even if we discard it in the attribute list view
	UpdateTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [SharedContext = SharedThis(this)]()
	{
		SharedContext->AsyncFilter();
		SharedContext->AsyncSort();
	});
}

void FPCGListViewUpdater::AsyncSort()
{
	if (const FPCGColumnData* Data = ColumnData.Find(SortMode == EColumnSortMode::None ? PCGEditorGraphAttributeListView::NAME_Index : SortingColumn))
	{
		if (Data->DataAccessor.IsValid() && Data->DataKeys.IsValid() && Data->DataKeys->GetNum() == ListViewItems.Num())
		{
			//lambda used here to get the index value of an item in the array for sorting
			PCGAttributeAccessorHelpers::SortByAttribute(*Data->DataAccessor, *Data->DataKeys, ListViewItems, !(SortMode & EColumnSortMode::Descending), [this](int Index) { return ListViewItems[Index]->Index; });
		}
	}
}

void FPCGListViewUpdater::AsyncFilter()
{
	TArray<PCGListviewItemPtr> FilteredListViewItems;
	FilteredListViewItems.Reserve(ListViewItems.Num());

	for (const PCGListviewItemPtr& ListViewItem : ListViewItems)
	{
		const FPCGPointFilterExpressionContext PointFilterContext(ListViewItem.Get(), &ColumnData);
		if (TextFilter->TestTextFilter(PointFilterContext))
		{
			FilteredListViewItems.Add(ListViewItem);
		}
	}

	ListViewItems = MoveTemp(FilteredListViewItems);
}

void SPCGListViewItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	check(InArgs._ListViewItem && InArgs._AttributeListView);
	InternalItem = InArgs._ListViewItem;
	AttributeListView = InArgs._AttributeListView;

	SMultiColumnTableRow<PCGListviewItemPtr>::Construct(
		SMultiColumnTableRow::FArguments()
		.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
		InOwnerTableView);
}

TSharedRef<SWidget> SPCGListViewItemRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	FText RowText = LOCTEXT("ColumnError", "Unrecognized Column");

	const TSharedPtr<SPCGEditorGraphAttributeListView> SharedAttributeListView = AttributeListView.Pin();
	check(SharedAttributeListView.IsValid());

	if (FPCGColumnData* PCGColumnData = SharedAttributeListView->PCGColumnData.Find(ColumnId))
	{
		if (PCGColumnData->DataAccessor.IsValid() && PCGColumnData->DataKeys.IsValid())
		{
			int32 Index = InternalItem->Index;
			auto Callback = [&PCGColumnData, Index, &RowText] (auto Dummy)
			{
				using ValueType = decltype(Dummy);
				ValueType Value{};
				if (PCGColumnData->DataAccessor->Get<ValueType>(Value, Index, *PCGColumnData->DataKeys))
				{
					if constexpr (PCG::Private::IsOfTypes<ValueType, bool>())
					{
						RowText = FText::FromString(LexToString(Value));
					}
					else if constexpr (PCG::Private::IsOfTypes<ValueType, FString>())
					{
						RowText = FText::FromString(Value);
					}
					else if constexpr (PCG::Private::IsOfTypes<ValueType, FName>())
					{
						RowText = FText::FromName(Value);
					}
					else if constexpr (FTextAsNumberIsValid<ValueType>::value)
					{
						RowText = FText::AsNumber(Value);
					}
					else if constexpr (PCG::Private::IsOfTypes<ValueType, FSoftObjectPath>())
					{
						RowText = FText::FromString(Value.ToString());
					}
					else if constexpr (PCG::Private::IsOfTypes<ValueType, FSoftClassPath>())
					{
						RowText = FText::FromString(Value.ToString());
					}
					else
					{
						ensureMsgf(false, TEXT("Unsupported Data Type"));
						RowText = LOCTEXT("UnsupportedDataTypeError", "Unsupported Data Type");
					}
				}
			};

			PCGMetadataAttribute::CallbackWithRightType(PCGColumnData->DataAccessor->GetUnderlyingType(), Callback);
		}
	}

	return SNew(STextBlock)
		.Text(RowText)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		.Margin(FMargin(2.0f, 0.0f));
}

FPCGPointFilterExpressionContext::FPCGPointFilterExpressionContext(const FPCGListViewItem* InRowItem, const TMap<FName, FPCGColumnData>* InPCGColumnData)
	: RowItem(InRowItem)
	, PCGColumnData(InPCGColumnData)
{
	check(InRowItem);
}

bool FPCGPointFilterExpressionContext::TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	// Basic string search is disabled as it would require us to search the entire attribute table at once and it's not very useful.
	return false;
}

bool FPCGPointFilterExpressionContext::TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	const int32 Index = RowItem->Index;

	if (PCGEditorGraphAttributeListView::TEXT_IndexLabel.EqualToCaseIgnored(FText::FromName(InKey)))
	{
		const FTextFilterString PointValue(FString::FromInt(Index));
		return TextFilterUtils::TestComplexExpression(PointValue, InValue, InComparisonOperation, InTextComparisonMode);
	}
	else if(const FPCGColumnData* PCGColumnInfo = PCGColumnData->Find(InKey))
	{
		if (PCGColumnInfo->DataAccessor.IsValid() && PCGColumnInfo->DataKeys.IsValid())
		{
			auto Callback = [&PCGColumnInfo, Index, &InValue, &InComparisonOperation, &InTextComparisonMode] (auto Dummy)
			{
				using ValueType = decltype(Dummy);
				ValueType Value{};
				if (PCGColumnInfo->DataAccessor->Get<ValueType>(Value, Index, *PCGColumnInfo->DataKeys))
				{
					FText TextValue;
					bool bInvalid = false;
					if constexpr (PCG::Private::IsOfTypes<ValueType, bool>())
					{
						TextValue = FText::FromString(LexToString(Value));
					}
					else if constexpr (PCG::Private::IsOfTypes<ValueType, FString>())
					{
						TextValue = FText::FromString(Value);
					}
					else if constexpr (PCG::Private::IsOfTypes<ValueType, FName>())
					{
						TextValue = FText::FromName(Value);
					}
					else if constexpr (FTextAsNumberIsValid<ValueType>::value)
					{
						TextValue = FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping());
					}
					else
					{
						ensureMsgf(false, TEXT("Unsupported Data Type"));
						bInvalid = true;
					}

					if (!bInvalid)
					{
						const FTextFilterString PointValue(TextValue.ToString());
						return TextFilterUtils::TestComplexExpression(PointValue, InValue, InComparisonOperation, InTextComparisonMode);
					}
				}

				return false;
			};

			return PCGMetadataAttribute::CallbackWithRightType(PCGColumnInfo->DataAccessor->GetUnderlyingType(), Callback);
		}
	}

	return true;
}

SPCGEditorGraphAttributeListView::~SPCGEditorGraphAttributeListView()
{
	if (PCGEditorPtr.IsValid())
	{
		PCGEditorPtr.Pin()->OnInspectedStackChangedDelegate.RemoveAll(this);
	}

	CollapsedPointData.Reset();
}

void SPCGEditorGraphAttributeListView::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;
	SortingColumn = PCGEditorGraphAttributeListView::NAME_Index;

	PCGEditorPtr.Pin()->OnInspectedStackChangedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnInspectedStackChanged);

	TextFilter = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::Complex));

	ListViewHeader = CreateHeaderRowWidget();

	ListViewCommands = MakeShareable(new FUICommandList);
	ListViewCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SPCGEditorGraphAttributeListView::CopySelectionToClipboard),
		FCanExecuteAction::CreateSP(this, &SPCGEditorGraphAttributeListView::CanCopySelectionToClipboard));

	const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	auto VisibilityTest = [this]()
	{
		return (ListViewItems.IsEmpty() && ListViewHeader->GetColumns().IsEmpty()) ? EVisibility::Hidden : EVisibility::Visible;
	};

	SAssignNew(ListView, SListView<PCGListviewItemPtr>)
		.ListItemsSource(&ListViewItems)
		.HeaderRow(ListViewHeader)
		.OnGenerateRow(this, &SPCGEditorGraphAttributeListView::OnGenerateRow)
		.OnMouseButtonDoubleClick(this, &SPCGEditorGraphAttributeListView::OnItemDoubleClicked)
		.AllowOverscroll(EAllowOverscroll::No)
		.ExternalScrollbar(VerticalScrollBar)
		.Visibility_Lambda(VisibilityTest)
		.OnKeyDownHandler(this, &SPCGEditorGraphAttributeListView::OnListViewKeyDown)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always);

	SAssignNew(PinComboBox, SComboBox<TSharedPtr<FPinComboBoxItem>>)
		.OptionsSource(&PinComboBoxItems)
		.OnGenerateWidget(this, &SPCGEditorGraphAttributeListView::OnGeneratePinWidget)
		.OnSelectionChanged(this, &SPCGEditorGraphAttributeListView::OnSelectionChangedPin)
		[
			SNew(STextBlock)
			.Text(this, &SPCGEditorGraphAttributeListView::OnGenerateSelectedPinText)
		];

	SAssignNew(DataComboBox, SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&DataComboBoxItems)
		.OnGenerateWidget(this, &SPCGEditorGraphAttributeListView::OnGenerateDataWidget)
		.OnSelectionChanged(this, &SPCGEditorGraphAttributeListView::OnSelectionChanged)
		[
			SNew(STextBlock)
			.Text(this, &SPCGEditorGraphAttributeListView::OnGenerateSelectedDataText)
		];

	TSharedPtr<SLayeredImage> FilterImage = SNew(SLayeredImage)
		.Image(FAppStyle::Get().GetBrush("Icons.Filter"))
		.ColorAndOpacity(FSlateColor::UseForeground());

	FilterImage->AddLayer(TAttribute<const FSlateBrush*>(this, &SPCGEditorGraphAttributeListView::GetFilterBadgeIcon));

	SAssignNew(LockButton, SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked(this, &SPCGEditorGraphAttributeListView::OnLockClick)
		.ContentPadding(FMargin(4, 2))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("LockSelectionButton_ToolTip", "Locks the current attribute list view to this selection"))
		[
			SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(this, &SPCGEditorGraphAttributeListView::OnGetLockButtonImageResource)
		];

	SAssignNew(FilterButton, SComboButton)
		.ForegroundColor(FSlateColor::UseStyle())
		.HasDownArrow(false)
		.OnGetMenuContent(this, &SPCGEditorGraphAttributeListView::OnGenerateFilterMenu)
		.ContentPadding(1)
		.ButtonContent()
		[
			FilterImage.ToSharedRef()
		];

	SAssignNew(SearchBoxWidget, SSearchBox)
		.MinDesiredWidth(300.0f)
		.InitialText(ActiveFilterText)
		.OnTextChanged(this, &SPCGEditorGraphAttributeListView::OnFilterTextChanged)
		.OnTextCommitted(this, &SPCGEditorGraphAttributeListView::OnFilterTextCommitted)
		.DelayChangeNotificationsWhileTyping(true)
		.DelayChangeNotificationsWhileTypingSeconds(0.5f);

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(1.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 0.0f)
			[
				LockButton->AsShared()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 0.0f)
			[
				FilterButton->AsShared()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 0.0f)
			[
				PinComboBox->AsShared()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 0.0f)
			[
				DataComboBox->AsShared()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 0.0f)
			[
				SearchBoxWidget->AsShared()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SPCGEditorGraphAttributeListView::OnNodeNameClicked)
				[
					SAssignNew(NodeNameTextBlock, STextBlock)
					.Text(PCGEditorGraphAttributeListView::NoNodeInspectedText)
					.ToolTipText(PCGEditorGraphAttributeListView::NoNodeInspectedToolTip)
				]
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Right)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SAssignNew(InfoTextBlock, STextBlock)
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					.ExternalScrollbar(HorizontalScrollBar)
					+SScrollBox::Slot()
					[
						ListView->AsShared()
					]
				]
				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SCircularThrobber)
					.Radius(12.0f)
					.Visibility_Lambda([this](){return CurrentUpdateTask.IsValid() && !CurrentUpdateTask->IsCompleted() ? EVisibility::Visible : EVisibility::Hidden; })
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				HorizontalScrollBar
			]
		]
	];
}

void SPCGEditorGraphAttributeListView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;

		RefreshPinComboBox();
		RefreshDataComboBox();
		RefreshAttributeList();
	}

	if (CurrentUpdateTask.IsValid() && CurrentUpdateTask->IsCompleted())
	{
		FilteredListViewItems = MoveTemp(CurrentUpdateTask->ListViewItems);
		if (ListView.IsValid())
		{
			ListView->SetItemsSource(&FilteredListViewItems);
			ListView->RequestListRefresh();
		}

		CurrentUpdateTask.Reset();
	}
}

TSharedRef<SHeaderRow> SPCGEditorGraphAttributeListView::CreateHeaderRowWidget() const
{
	return SNew(SHeaderRow);
}

void SPCGEditorGraphAttributeListView::OnInspectedStackChanged(const FPCGStack& InPCGStack)
{
	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
	}

	PCGComponent = const_cast<UPCGComponent*>(InPCGStack.GetRootComponent());

	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnGenerateUpdated);
		PCGComponent->OnPCGGraphCleanedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnGenerateUpdated);
	}

	RequestRefresh();
}

UPCGEditorGraphNodeBase* SPCGEditorGraphAttributeListView::GetNodeBeingInspected() const
{
	return PCGEditorGraphNode.Get();
}

void SPCGEditorGraphAttributeListView::SetNodeBeingInspected(UPCGEditorGraphNodeBase* InPCGEditorGraphNode)
{
	if (PCGEditorGraphNode == InPCGEditorGraphNode)
	{
		return;
	}

	PCGEditorGraphNode = InPCGEditorGraphNode;

	if (PCGEditorGraphNode.IsValid())
	{
		NodeNameTextBlock->SetText(PCGEditorGraphNode->GetNodeTitle(ENodeTitleType::ListView));
		NodeNameTextBlock->SetToolTipText(PCGEditorGraphNode->GetTooltipText());
	}
	else
	{
		NodeNameTextBlock->SetText(PCGEditorGraphAttributeListView::NoNodeInspectedText);
		NodeNameTextBlock->SetToolTipText(PCGEditorGraphAttributeListView::NoNodeInspectedToolTip);
	}

	// Always unlock when changing the node, to make sure we unlock when removing the inspected node
	bIsLocked = false;

	RequestRefresh();
}

void SPCGEditorGraphAttributeListView::OnGenerateUpdated(UPCGComponent* /*InPCGComponent*/)
{
	RequestRefresh();
}

const FPCGDataCollection* SPCGEditorGraphAttributeListView::GetInspectionData() const
{
	if (!PCGComponent.IsValid())
	{
		return nullptr;
	}

	const UPCGNode* PCGNode = PCGEditorGraphNode.IsValid() ? PCGEditorGraphNode->GetPCGNode() : nullptr;
	if (!PCGNode)
	{
		return nullptr;
	}

	const UPCGPin* Pin = nullptr;
	if (const TSharedPtr<FPinComboBoxItem> SelectedPin = PinComboBox->GetSelectedItem())
	{
		const TArray<TObjectPtr<UPCGPin>>& Pins = SelectedPin->bIsOutputPin ? PCGNode->GetOutputPins() : PCGNode->GetInputPins();
		if (Pins.IsValidIndex(SelectedPin->PinIndex))
		{
			Pin = Pins[SelectedPin->PinIndex];
		}
	}

	if (!Pin)
	{
		return nullptr;
	}

	PCGEditorGraphUtils::GetInspectablePin(PCGNode, Pin, PCGNode, Pin);

	if (!PCGNode || !Pin)
	{
		return nullptr;
	}

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	const FPCGStack* PCGStack = PCGEditor->GetStackBeingInspected();
	if (!PCGStack)
	{
		return nullptr;
	}

	// Create a temporary stack with Node+Pin to query the exact DataCollection we are inspecting
	FPCGStack Stack = *PCGStack;
	TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFramesMutable();
	StackFrames.Reserve(StackFrames.Num() + 2);
	StackFrames.Emplace(PCGNode);
	StackFrames.Emplace(Pin);

	return PCGComponent->GetInspectionData(Stack);
}

void SPCGEditorGraphAttributeListView::RefreshAttributeList()
{
	HiddenAttributes = ListViewHeader->GetHiddenColumnIds();

	// Swapping to an empty item list to force a widget clear, otherwise the widgets will try to update during add column and access invalid data
	static const TArray<PCGListviewItemPtr> EmptyList;
	ListView->SetItemsSource(&EmptyList);

	PCGColumnData.Empty();
	ListViewItems.Empty();
	ListViewHeader->ClearColumns();
	InfoTextBlock->SetText(FText::GetEmpty());
	CollapsedPointData.Reset();

	const FPCGDataCollection* InspectionData = GetInspectionData();
	if (!InspectionData)
	{
		return;
	}

	const int32 DataIndex = GetSelectedDataIndex();
	if (!InspectionData->TaggedData.IsValidIndex(DataIndex))
	{
		return;
	}

	const FPCGTaggedData& TaggedData = InspectionData->TaggedData[DataIndex];
	const UPCGData* PCGData = TaggedData.Data;
	const FPCGCrc Crc = InspectionData->DataCrcs.IsValidIndex(DataIndex) ? InspectionData->DataCrcs[DataIndex] : FPCGCrc(0);

	if (const UPCGParamData* PCGParamData = Cast<const UPCGParamData>(PCGData))
	{
		if (const UPCGMetadata* PCGMetadata = PCGParamData->ConstMetadata())
		{
			AddColumn(PCGData, PCGEditorGraphAttributeListView::NAME_Index, PCGEditorGraphAttributeListView::TEXT_IndexLabel);
			GenerateColumnsFromMetadata(PCGData, PCGMetadata);

			const PCGMetadataEntryKey ItemKeyLowerBound = PCGMetadata->GetItemKeyCountForParent();
			const PCGMetadataEntryKey ItemKeyUpperBound = PCGMetadata->GetItemCountForChild();
			for (PCGMetadataEntryKey MetadataItemKey = ItemKeyLowerBound; MetadataItemKey < ItemKeyUpperBound; ++MetadataItemKey)
			{
				PCGListviewItemPtr ListViewItem = MakeShared<FPCGListViewItem>();
				ListViewItem->Index = MetadataItemKey - ItemKeyLowerBound;
				ListViewItems.Add(ListViewItem);
			}

			if (!PCGEditorGraphAttributeListView::IsGraphCacheDebuggingEnabled())
			{
				InfoTextBlock->SetText(FText::Format(LOCTEXT("MetadataInfoTextBlockFmt", "Number of metadata: {0}"), ItemKeyUpperBound - ItemKeyLowerBound));
			}
			else
			{
				// If cache debugging enabled, write CRC to help diagnose missed-dependency issues
				InfoTextBlock->SetText(FText::Format(LOCTEXT("MetadataInfoTextBlockWithCrcFmt", "Number of metadata: {0}  CRC: {1}"), ItemKeyUpperBound - ItemKeyLowerBound, Crc.GetValue()));
			}
		}
	}
	else if (const UPCGSpatialData* PCGSpatialData = Cast<const UPCGSpatialData>(PCGData))
	{
		if (const UPCGPointData* PCGPointData = PCGSpatialData->ToPointData())
		{
			CollapsedPointData.Reset(PCGPointData);

			const UPCGMetadata* PCGMetadata = PCGPointData->ConstMetadata();

			AddPointDataColumns(PCGPointData);
			GenerateColumnsFromMetadata(PCGPointData, PCGMetadata);

			const TArray<FPCGPoint>& PCGPoints = PCGPointData->GetPoints();
			const int32 NumPoints = PCGPoints.Num();
			ListViewItems.Reserve(NumPoints);
			for (int32 PointIndex = 0; PointIndex < NumPoints; PointIndex++)
			{
				const FPCGPoint& PCGPoint = PCGPoints[PointIndex];
				// TODO: Investigate swapping out the shared ptr's for better performance on huge data sets
				PCGListviewItemPtr ListViewItem = MakeShared<FPCGListViewItem>();
				ListViewItem->Index = PointIndex;
				ListViewItem->PCGPoint = &PCGPoint;
				ListViewItems.Add(ListViewItem);
			}

			if (!PCGEditorGraphAttributeListView::IsGraphCacheDebuggingEnabled())
			{
				InfoTextBlock->SetText(FText::Format(LOCTEXT("PointInfoTextBlockFmt", "Number of points: {0}"), NumPoints));
			}
			else
			{
				// If cache debugging enabled, write CRC to help diagnose missed-dependency issues
				InfoTextBlock->SetText(FText::Format(LOCTEXT("PointInfoTextBlockWithCrcFmt", "Number of points: {0}, CRC: {1}"), NumPoints, Crc.GetValue()));
			}
		}
	}

	if (PCGData && PCGData->HasCachedLastSelector())
	{
		const FText LastSelector = PCGData->GetCachedLastSelector().GetDisplayText();
		InfoTextBlock->SetText(FText::Format(PCGEditorGraphAttributeListView::LastLabelFormat, LastSelector, InfoTextBlock->GetText()));
	}

	ListView->SetItemsSource(&ListViewItems);
	ListView->RequestListRefresh();

	LaunchUpdateTask();
}

void SPCGEditorGraphAttributeListView::RefreshPinComboBox()
{
	PinComboBoxItems.Empty();
	PinComboBox->ClearSelection();
	PinComboBox->RefreshOptions();

	const UPCGNode* PCGNode = PCGEditorGraphNode.IsValid() ? PCGEditorGraphNode->GetPCGNode() : nullptr;
	if (!PCGNode)
	{
		return;
	}

	// Add output and then input pins to list. Optionally output the first connected item - useful for initializing
	// the selected item to the first connected output pin.
	auto PopulatePins = [](
		const TArray<TObjectPtr<UPCGPin>>& InPins,
		const FString& InFormatText,
		TArray<TSharedPtr<FPinComboBoxItem>>& InOutItems,
		int32* OutFirstConnectedItemIndex)
	{
		for (int32 PinIndex = 0; PinIndex < InPins.Num(); ++PinIndex)
		{
			const UPCGPin* PCGPin = InPins[PinIndex];
			const bool bIsOutputPin = PCGPin->IsOutputPin();
			// Pin is included in list if it is connected, or if it is an output pin.
			if (PCGPin->IsConnected() || bIsOutputPin)
			{
				FString ItemName = FString::Format(*InFormatText, { PCGPin->Properties.Label.ToString() });
				InOutItems.Add(MakeShared<FPinComboBoxItem>(FName(ItemName), PinIndex, bIsOutputPin));

				// Look for first connected, null pointer once found so only first is taken.
				if (OutFirstConnectedItemIndex && PCGPin->IsConnected())
				{
					*OutFirstConnectedItemIndex = InOutItems.Num() - 1;
					OutFirstConnectedItemIndex = nullptr;
				}
			}
		}
	};

	// Pick first connected output pin by default if there is one, otherwise default to first output pin.
	int32 FirstConnectedItemIndex = 0;
	PopulatePins(PCGNode->GetOutputPins(), TEXT("Output: {0}"), PinComboBoxItems, &FirstConnectedItemIndex);
	PopulatePins(PCGNode->GetInputPins(), TEXT("Input: {0}"), PinComboBoxItems, nullptr);

	if (PinComboBoxItems.Num() > 0 && ensure(PinComboBoxItems.IsValidIndex(FirstConnectedItemIndex)))
	{
		PinComboBox->SetSelectedItem(PinComboBoxItems[FirstConnectedItemIndex]);
	}
}

void SPCGEditorGraphAttributeListView::RefreshDataComboBox()
{
	DataComboBoxItems.Empty();
	DataComboBox->ClearSelection();
	DataComboBox->RefreshOptions();

	const FPCGDataCollection* InspectionData = GetInspectionData();
	if (!InspectionData)
	{
		return;
	}

	for(int32 TaggedDataIndex = 0; TaggedDataIndex < InspectionData->TaggedData.Num(); ++TaggedDataIndex)
	{
		const FPCGTaggedData& TaggedData = InspectionData->TaggedData[TaggedDataIndex];
		FString ItemName = FString::Format(
			TEXT("[{0}] {1}"),
			{ FText::AsNumber(TaggedDataIndex).ToString(), (TaggedData.Data ? TaggedData.Data->GetClass()->GetDisplayNameText().ToString(): TEXT("No Data")) });

		if (!TaggedData.Tags.IsEmpty())
		{
			ItemName.Append(FString::Format(TEXT(": ({0})"), { FString::Join(TaggedData.Tags, TEXT(", ")) }));
		}

		DataComboBoxItems.Add(MakeShared<FName>(ItemName));
	}

	if (DataComboBoxItems.Num() > 0)
	{
		DataComboBox->SetSelectedItem(DataComboBoxItems[0]);
	}
}

void SPCGEditorGraphAttributeListView::LaunchUpdateTask()
{
	// Discarding any currently running updater, task will still run and keep the old object alive but we wont care about the result.
	// This is done because we cant afford to wait for task completion before starting a new task.
	CurrentUpdateTask.Reset();
	CurrentUpdateTask = MakeShared<FPCGListViewUpdater>(ListViewItems,
		PCGColumnData,
		SortMode,
		SortingColumn,
		TextFilter);
	CurrentUpdateTask->Launch();
}

const FSlateBrush* SPCGEditorGraphAttributeListView::GetFilterBadgeIcon() const
{
	for (const SHeaderRow::FColumn& Column : ListViewHeader->GetColumns())
	{
		if (!Column.bIsVisible)
		{
			return FAppStyle::Get().GetBrush("Icons.BadgeModified");
		}
	}

	return nullptr;
}

TSharedRef<SWidget> SPCGEditorGraphAttributeListView::OnGenerateFilterMenu()
{
	FMenuBuilder MenuBuilder(false, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ToggleAllAttributes", "Toggle All"),
		LOCTEXT("ToggleAllAttributesTooltip", "Toggle visibility for all attributes"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SPCGEditorGraphAttributeListView::ToggleAllAttributes),
			FCanExecuteAction(),
			FGetActionCheckState::CreateSP(this, &SPCGEditorGraphAttributeListView::GetAnyAttributeEnabledState)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddSeparator();

	const TIndirectArray<SHeaderRow::FColumn>& Columns = ListViewHeader->GetColumns();
	TArray<FName> HiddenColumns = ListViewHeader->GetHiddenColumnIds();

	for (const SHeaderRow::FColumn& Column : Columns)
	{
		if (Column.ColumnId == PCGEditorGraphAttributeListView::NAME_Index)
		{
			continue;
		}

		MenuBuilder.AddMenuEntry(
			Column.DefaultText,
			Column.DefaultTooltip,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SPCGEditorGraphAttributeListView::ToggleAttribute, Column.ColumnId),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SPCGEditorGraphAttributeListView::IsAttributeEnabled, Column.ColumnId)),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}

	return MenuBuilder.MakeWidget();
}

FText SPCGEditorGraphAttributeListView::OnGenerateSelectedPinText() const
{
	if (const TSharedPtr<FPinComboBoxItem> SelectedPin = PinComboBox->GetSelectedItem())
	{
		return FText::FromName(SelectedPin->Name);
	}
	else
	{
		return PCGEditorGraphAttributeListView::NoPinAvailableText;
	}
}

void SPCGEditorGraphAttributeListView::OnSelectionChangedPin(TSharedPtr<FPinComboBoxItem> InItem, ESelectInfo::Type InSelectInfo)
{
	RefreshDataComboBox();

	if (InSelectInfo != ESelectInfo::Direct)
	{
		RefreshAttributeList();
	}
}

TSharedRef<SWidget> SPCGEditorGraphAttributeListView::OnGeneratePinWidget(TSharedPtr<FPinComboBoxItem> InItem) const
{
	return SNew(STextBlock).Text(FText::FromName(InItem.IsValid() ? InItem->Name : NAME_None));
}

TSharedRef<SWidget> SPCGEditorGraphAttributeListView::OnGenerateDataWidget(TSharedPtr<FName> InItem) const
{
	return SNew(STextBlock).Text(FText::FromName(InItem.IsValid() ? *InItem : NAME_None));
}

void SPCGEditorGraphAttributeListView::OnSelectionChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		RefreshAttributeList();
	}
}

FText SPCGEditorGraphAttributeListView::OnGenerateSelectedDataText() const
{
	if (const TSharedPtr<FName> SelectedDataName = DataComboBox->GetSelectedItem())
	{
		return FText::FromName(*SelectedDataName);
	}
	else
	{
		return PCGEditorGraphAttributeListView::NoDataAvailableText;
	}
}

int32 SPCGEditorGraphAttributeListView::GetSelectedDataIndex() const
{
	int32 Index = INDEX_NONE;
	if (const TSharedPtr<FName> SelectedItem = DataComboBox->GetSelectedItem())
	{
		DataComboBoxItems.Find(SelectedItem, Index);
	}

	return Index;
}

void SPCGEditorGraphAttributeListView::GenerateColumnsFromMetadata(const UPCGData* InPCGData, const UPCGMetadata* PCGMetadata)
{
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;
	PCGMetadata->GetAttributes(AttributeNames, AttributeTypes);

	for (int32 I = 0; I < AttributeNames.Num(); I++)
	{
		const FName& AttributeName = AttributeNames[I];
		const EPCGMetadataTypes AttributeType = AttributeTypes[I];
		FName ColumnName = AttributeName;

		switch (AttributeType)
		{
		case EPCGMetadataTypes::Float:
		case EPCGMetadataTypes::Double:
		case EPCGMetadataTypes::Integer32:
		case EPCGMetadataTypes::Integer64:
		case EPCGMetadataTypes::Boolean:
		case EPCGMetadataTypes::String:
		case EPCGMetadataTypes::Name:
		case EPCGMetadataTypes::SoftObjectPath:
		case EPCGMetadataTypes::SoftClassPath:
			{
				AddMetadataColumn(InPCGData, ColumnName, AttributeType);
				break;
			}
		case EPCGMetadataTypes::Vector2:
		{
			AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".X"));
			AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Y"));
			break;
		}
		case EPCGMetadataTypes::Vector:
			{
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".X"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Y"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Z"));
				break;
			}
		case EPCGMetadataTypes::Vector4:
		case EPCGMetadataTypes::Quaternion:
			{
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".X"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Y"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Z"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".W"));
				break;
			}
		case EPCGMetadataTypes::Rotator:
			{
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Roll"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Pitch"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Yaw"));
				break;
			}
		case EPCGMetadataTypes::Transform:
			{
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Position.X"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Position.Y"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Position.Z"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Rotation.Roll"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Rotation.Pitch"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Rotation.Yaw"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Scale.X"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Scale.Y"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Scale.Z"));
				break;
			}
		default:
			break;
		}
	}
}

void SPCGEditorGraphAttributeListView::ToggleAllAttributes()
{
	const TArray<FName> HiddenColumns = ListViewHeader->GetHiddenColumnIds();
	if (HiddenColumns.Num() > 0)
	{
		for (const FName& HiddenColumn : HiddenColumns)
		{
			ListViewHeader->SetShowGeneratedColumn(HiddenColumn, /*InShow=*/true);
		}
	}
	else
	{
		const TIndirectArray<SHeaderRow::FColumn>& Columns = ListViewHeader->GetColumns();
		for (const SHeaderRow::FColumn& Column : Columns)
		{
			if (Column.ColumnId != PCGEditorGraphAttributeListView::NAME_Index)
			{
				ListViewHeader->SetShowGeneratedColumn(Column.ColumnId, /*InShow=*/false);
			}
		}
	}
}

void SPCGEditorGraphAttributeListView::ToggleAttribute(FName InAttributeName)
{
	ListViewHeader->SetShowGeneratedColumn(InAttributeName, !ListViewHeader->IsColumnVisible(InAttributeName));
}

ECheckBoxState SPCGEditorGraphAttributeListView::GetAnyAttributeEnabledState() const
{
	bool bAllEnabled = true;
	bool bAnyEnabled = false;

	for (const SHeaderRow::FColumn& Column : ListViewHeader->GetColumns())
	{
		if (Column.ColumnId == PCGEditorGraphAttributeListView::NAME_Index)
		{
			continue;
		}

		bAllEnabled &= Column.bIsVisible;
		bAnyEnabled |= Column.bIsVisible;
	}

	if (bAllEnabled)
	{
		return ECheckBoxState::Checked;
	}
	else if (bAnyEnabled)
	{
		return ECheckBoxState::Undetermined;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

bool SPCGEditorGraphAttributeListView::IsAttributeEnabled(FName InAttributeName) const
{
	return ListViewHeader->IsColumnVisible(InAttributeName);
}

TSharedRef<ITableRow> SPCGEditorGraphAttributeListView::OnGenerateRow(PCGListviewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPCGListViewItemRow, OwnerTable)
		.AttributeListView(SharedThis(this))
		.ListViewItem(Item);
}

void SPCGEditorGraphAttributeListView::OnItemDoubleClicked(PCGListviewItemPtr Item) const
{
	check(Item);

	if (const FPCGPoint* Point = Item->PCGPoint)
	{
		const FBox BoundingBox = Point->GetLocalBounds().TransformBy(Point->Transform.ToMatrixWithScale());
		GEditor->MoveViewportCamerasToBox(BoundingBox, true, 2.5f);
	}
}

void SPCGEditorGraphAttributeListView::OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InNewSortMode)
{
	if (SortingColumn == InColumnId)
	{
		// Cycling
		SortMode = static_cast<EColumnSortMode::Type>((SortMode + 1) % 3);
	}
	else
	{
		SortingColumn = InColumnId;
		SortMode = InNewSortMode;
	}

	LaunchUpdateTask();
}

EColumnSortMode::Type SPCGEditorGraphAttributeListView::GetColumnSortMode(const FName InColumnId) const
{
	if (SortingColumn != InColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

void SPCGEditorGraphAttributeListView::OnFilterTextChanged(const FText& InFilterText)
{
	ActiveFilterText = InFilterText;
	TextFilter->SetFilterText(InFilterText);

	const FText ErrorText = TextFilter->GetFilterErrorText();
	if(ErrorText.IsEmpty())
	{
		LaunchUpdateTask();
	}

	SearchBoxWidget->SetError(ErrorText);
}

void SPCGEditorGraphAttributeListView::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnCleared)
	{
		SearchBoxWidget->SetText(FText::GetEmpty());
		OnFilterTextChanged(FText::GetEmpty());
	}
}

FReply SPCGEditorGraphAttributeListView::OnListViewKeyDown(const FGeometry& /*InGeometry*/, const FKeyEvent& InKeyEvent) const
{
	if (ListViewCommands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SPCGEditorGraphAttributeListView::AddColumn(const UPCGData* InPCGData, const FName& InColumnId, const FText& ColumnLabel)
{
	FText ToolTip;

	if (InPCGData)
	{
		const FString ColumnIdString = InColumnId.ToString();

		FPCGAttributePropertyInputSelector TargetSelector;
		TargetSelector.Update(ColumnIdString);

		FPCGColumnData& ColumnData = PCGColumnData.Add(InColumnId);

		if (InColumnId == PCGEditorGraphAttributeListView::NAME_PointMetadataEntry)
		{
			if (const UPCGPointData* PCGPointData = Cast<const UPCGPointData>(InPCGData))
			{
				TUniquePtr<const IPCGAttributeAccessor> DataAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(GET_MEMBER_NAME_CHECKED(FPCGPoint, MetadataEntry), FPCGPoint::StaticStruct());
				ColumnData.DataAccessor = TSharedPtr<const IPCGAttributeAccessor>(DataAccessor.Release());
				ColumnData.DataKeys = MakeShared<FPCGAttributeAccessorKeysPoints>(PCGPointData->GetPoints());
			}
		}
		else if (InColumnId == PCGEditorGraphAttributeListView::NAME_PointMetadataEntryParent)
		{
			if (const UPCGPointData* PCGPointData = Cast<const UPCGPointData>(InPCGData))
			{
				ColumnData.DataAccessor = MakeShared<FPCGCustomPointAccessor<int64>>([PCGPointData](const FPCGPoint& Point, void* OutValue)
				{
					if (const UPCGMetadata* Metadata = PCGPointData->Metadata)
					{
						*reinterpret_cast<int64*>(OutValue) = Metadata->GetParentKey(Point.MetadataEntry);
						return true;
					}
					return false;
				}, nullptr);
				ColumnData.DataKeys = MakeShared<FPCGAttributeAccessorKeysPoints>(PCGPointData->GetPoints());
			}
		}
		else
		{
			ColumnData.DataAccessor = TSharedPtr<const IPCGAttributeAccessor>(PCGAttributeAccessorHelpers::CreateConstAccessor(InPCGData, TargetSelector).Release());
			ColumnData.DataKeys = TSharedPtr<const IPCGAttributeAccessorKeys>(PCGAttributeAccessorHelpers::CreateConstKeys(InPCGData, TargetSelector).Release());
		}

		ToolTip = FText::FromString(PCG::Private::GetTypeName(ColumnData.DataAccessor->GetUnderlyingType()));
	}

	const float ColumnWidth = PCGEditorGraphAttributeListView::CalculateColumnWidth(ColumnLabel);

	SHeaderRow::FColumn::FArguments Arguments;
	Arguments.ColumnId(InColumnId);
	Arguments.DefaultLabel(ColumnLabel);
	Arguments.DefaultTooltip(ToolTip);
	Arguments.ManualWidth(ColumnWidth);
	Arguments.HAlignHeader(HAlign_Center);
	Arguments.HAlignCell(HAlign_Right);
	Arguments.SortMode(this, &SPCGEditorGraphAttributeListView::GetColumnSortMode, InColumnId);
	Arguments.OnSort(this, &SPCGEditorGraphAttributeListView::OnColumnSortModeChanged);
	Arguments.OverflowPolicy(ETextOverflowPolicy::Ellipsis);

	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(Arguments);
	NewColumn->bIsVisible = !HiddenAttributes.Contains(InColumnId);
	ListViewHeader->AddColumn(*NewColumn);
}

void SPCGEditorGraphAttributeListView::AddPointDataColumns(const UPCGPointData* InPCGPointData)
{
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_Index, PCGEditorGraphAttributeListView::TEXT_IndexLabel);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointPositionX, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelX);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointPositionY, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelY);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointPositionZ, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelZ);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointRotationRoll, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelRoll);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointRotationPitch, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelPitch);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointRotationYaw, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelYaw);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointScaleX, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelX);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointScaleY, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelY);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointScaleZ, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelZ);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointBoundsMinX, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinX);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointBoundsMinY, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinY);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointBoundsMinZ, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinZ);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointBoundsMaxX, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxX);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointBoundsMaxY, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxY);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointBoundsMaxZ, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxZ);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointColorR, PCGEditorGraphAttributeListView::TEXT_PointColorLabelR);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointColorG, PCGEditorGraphAttributeListView::TEXT_PointColorLabelG);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointColorB, PCGEditorGraphAttributeListView::TEXT_PointColorLabelB);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointColorA, PCGEditorGraphAttributeListView::TEXT_PointColorLabelA);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointDensity, PCGEditorGraphAttributeListView::TEXT_PointDensityLabel);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointSteepness, PCGEditorGraphAttributeListView::TEXT_PointSteepnessLabel);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointSeed, PCGEditorGraphAttributeListView::TEXT_PointSeedLabel);

	if (CVarShowAdvancedAttributesFields.GetValueOnAnyThread())
	{
		AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointMetadataEntry, PCGEditorGraphAttributeListView::TEXT_PointMetadataEntryLabel);
		AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointMetadataEntryParent, PCGEditorGraphAttributeListView::TEXT_PointMetadataEntryParentLabel);
	}
}

void SPCGEditorGraphAttributeListView::AddMetadataColumn(const UPCGData* InPCGData, const FName& InColumnId, EPCGMetadataTypes InMetadataType, const TCHAR* PostFix)
{
	FString OriginalColumnIdString = InColumnId.ToString();
	if (PostFix)
	{
		OriginalColumnIdString.Append(PostFix);
	}

	FText ColumnLabel;
	FText ToolTip;
	FString ColumnIdString(OriginalColumnIdString);
	if (InColumnId == NAME_None)
	{
		ColumnIdString = FString::Printf(TEXT("%s%s"), *PCGEditorGraphAttributeListView::NoneAttributeId, PostFix);
		ColumnLabel = FText::Format(LOCTEXT("NoneLabelFormat", "{0}{1}"), UEnum::GetDisplayValueAsText(InMetadataType), FText::FromString(PostFix));
	}

	const FName ColumnId(ColumnIdString);

	if (InPCGData)
	{
		FPCGAttributePropertyInputSelector TargetSelector;
		TargetSelector.Update(OriginalColumnIdString);

		FPCGColumnData& ColumnData = PCGColumnData.Add(ColumnId);
		ColumnData.DataAccessor = TSharedPtr<const IPCGAttributeAccessor>(PCGAttributeAccessorHelpers::CreateConstAccessor(InPCGData, TargetSelector).Release());
		ColumnData.DataKeys = TSharedPtr<const IPCGAttributeAccessorKeys>(PCGAttributeAccessorHelpers::CreateConstKeys(InPCGData, TargetSelector).Release());

		ToolTip = FText::FromString(PCG::Private::GetTypeName(ColumnData.DataAccessor->GetUnderlyingType()));
	}

	if (ColumnLabel.IsEmpty())
	{
		ColumnLabel = FText::FromName(ColumnId);
	}

	float ColumnWidth = 0.0f;

	EHorizontalAlignment CellAlignment = EHorizontalAlignment::HAlign_Right;

	if (InMetadataType == EPCGMetadataTypes::String)
	{
		ColumnWidth = PCGEditorGraphAttributeListView::MaxColumnWidth;
		CellAlignment = EHorizontalAlignment::HAlign_Left;
	}
	else
	{
		ColumnWidth = PCGEditorGraphAttributeListView::CalculateColumnWidth(ColumnLabel);
	}

	SHeaderRow::FColumn::FArguments ColumnArguments;
	ColumnArguments.ColumnId(ColumnId);
	ColumnArguments.DefaultLabel(ColumnLabel);
	ColumnArguments.DefaultTooltip(ToolTip);
	ColumnArguments.HAlignHeader(EHorizontalAlignment::HAlign_Center);
	ColumnArguments.HAlignCell(CellAlignment);
	ColumnArguments.ManualWidth(ColumnWidth);
	ColumnArguments.SortMode(this, &SPCGEditorGraphAttributeListView::GetColumnSortMode, ColumnId);
	ColumnArguments.OnSort(this, &SPCGEditorGraphAttributeListView::OnColumnSortModeChanged);
	ColumnArguments.OverflowPolicy(ETextOverflowPolicy::Ellipsis);

	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(ColumnArguments);
	NewColumn->bIsVisible = !HiddenAttributes.Contains(ColumnId);
	ListViewHeader->AddColumn(*NewColumn);
}

void SPCGEditorGraphAttributeListView::CopySelectionToClipboard() const
{
	constexpr TCHAR Delimiter = TEXT(',');
	constexpr TCHAR LineEnd = TEXT('\n');

	const TArray<FName> HiddenColumnIds = ListViewHeader->GetHiddenColumnIds();

	TArray<TPair<FName, FPCGColumnData>> FilteredPCGColumnData;
	for (const TPair<FName, FPCGColumnData>& Data : PCGColumnData)
	{
		if (!HiddenColumnIds.Contains(Data.Key))
		{
			FilteredPCGColumnData.Add(Data);
		}
	}

	TStringBuilder<2048> CSVExport;

	// Write column header row
	for (int ColumnIndex = 0; ColumnIndex < FilteredPCGColumnData.Num(); ColumnIndex++)
	{
		const TPair<FName, FPCGColumnData>& Data = FilteredPCGColumnData[ColumnIndex];

		if (ColumnIndex > 0)
		{
			CSVExport += Delimiter;
		}
		CSVExport += Data.Key.ToString();
	}

	// Gather selected rows and sort them to match the displayed order instead of selection order
	TArray<PCGListviewItemPtr> SelectedListViewItems = ListView->GetSelectedItems();
	if (const FPCGColumnData* ColumnData = PCGColumnData.Find(SortMode == EColumnSortMode::None ? PCGEditorGraphAttributeListView::NAME_Index : SortingColumn))
	{
		if (ColumnData->DataAccessor.IsValid() && ColumnData->DataKeys.IsValid())
		{
			// Lambda used here to get the index value of an item in the array for sorting
			PCGAttributeAccessorHelpers::SortByAttribute(*ColumnData->DataAccessor, *ColumnData->DataKeys, SelectedListViewItems, !(SortMode & EColumnSortMode::Descending), [&SelectedListViewItems](int Index) { return SelectedListViewItems[Index]->Index; });
		}
	}

	// Write each row
	for (const PCGListviewItemPtr&  ListViewItem : SelectedListViewItems)
	{
		CSVExport += LineEnd;

		for (int ColumnIndex = 0; ColumnIndex < FilteredPCGColumnData.Num(); ColumnIndex++)
		{
			if (ColumnIndex > 0)
			{
				CSVExport += Delimiter;
			}

			const TPair<FName, FPCGColumnData>& Data = FilteredPCGColumnData[ColumnIndex];
			const FPCGColumnData& ColumnData = Data.Value;
			if (ColumnData.DataAccessor.IsValid() && ColumnData.DataKeys.IsValid())
			{
				FString RowString;
				if (ColumnData.DataAccessor->Get<FString>(RowString, ListViewItem->Index, *ColumnData.DataKeys, EPCGAttributeAccessorFlags::AllowBroadcast))
				{
					CSVExport += RowString;
				}
			}
		}
	}

	FPlatformApplicationMisc::ClipboardCopy(*CSVExport);
}

bool SPCGEditorGraphAttributeListView::CanCopySelectionToClipboard() const
{
	return ListView->GetNumItemsSelected() > 0;
}

const FSlateBrush* SPCGEditorGraphAttributeListView::OnGetLockButtonImageResource() const
{
	return FAppStyle::GetBrush(bIsLocked ? TEXT("PropertyWindow.Locked") : TEXT("PropertyWindow.Unlocked"));
}

FReply SPCGEditorGraphAttributeListView::OnLockClick()
{
	bIsLocked = !bIsLocked;
	return FReply::Handled();
}

FReply SPCGEditorGraphAttributeListView::OnNodeNameClicked()
{
	if (PCGEditorGraphNode.Get() && PCGEditorPtr.IsValid())
	{
		PCGEditorPtr.Pin()->JumpToNode(PCGEditorGraphNode.Get());
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
