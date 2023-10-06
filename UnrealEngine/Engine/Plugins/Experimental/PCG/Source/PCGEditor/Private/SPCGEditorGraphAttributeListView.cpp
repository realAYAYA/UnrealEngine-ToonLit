// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphAttributeListView.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGNode.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

#include "PCGEditor.h"
#include "PCGEditorGraphNodeBase.h"

#include "Fonts/FontMeasure.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Images/SLayeredImage.h"
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
	const FName NAME_IndexColumn = FName(TEXT("IndexColumn"));
	const FName NAME_PointPositionX = FName(TEXT("$Position.X"));
	const FName NAME_PointPositionY = FName(TEXT("$Position.Y"));
	const FName NAME_PointPositionZ = FName(TEXT("$Position.Z"));
	const FName NAME_PointRotationX = FName(TEXT("$Rotation.X"));
	const FName NAME_PointRotationY = FName(TEXT("$Rotation.Y"));
	const FName NAME_PointRotationZ = FName(TEXT("$Rotation.Z"));
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
	const FText TEXT_PointRotationLabelX = LOCTEXT("PointRotationLabelX", "Rotation.X");
	const FText TEXT_PointRotationLabelY = LOCTEXT("PointRotationLabelY", "Rotation.Y");
	const FText TEXT_PointRotationLabelZ = LOCTEXT("PointRotationLabelZ", "Rotation.Z");
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

	if (ColumnId == PCGEditorGraphAttributeListView::NAME_IndexColumn)
	{
		RowText = FText::FromString(FString::FromInt(InternalItem->Index));
		return SNew(STextBlock).Text(RowText);
	}

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
						return false;
					}
					
					const FTextFilterString PointValue(TextValue.ToString());
					return TextFilterUtils::TestComplexExpression(PointValue, InValue, InComparisonOperation, InTextComparisonMode);
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
		PCGEditorPtr.Pin()->OnInspectedNodeChangedDelegate.RemoveAll(this);
	}
}

void SPCGEditorGraphAttributeListView::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;
	SortingColumn = PCGEditorGraphAttributeListView::NAME_IndexColumn;

	PCGEditorPtr.Pin()->OnInspectedComponentChangedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnInspectedComponentChanged);
	PCGEditorPtr.Pin()->OnInspectedStackChangedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnInspectedStackChanged);
	PCGEditorPtr.Pin()->OnInspectedNodeChangedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnInspectedNodeChanged);

	TextFilter = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::Complex));

	ListViewHeader = CreateHeaderRowWidget();

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
				SAssignNew(NodeNameTextBlock, STextBlock)
				.Text(PCGEditorGraphAttributeListView::NoNodeInspectedText)
				.ToolTipText(PCGEditorGraphAttributeListView::NoNodeInspectedToolTip)
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
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+SScrollBox::Slot()
				[
					ListView->AsShared()
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
}

TSharedRef<SHeaderRow> SPCGEditorGraphAttributeListView::CreateHeaderRowWidget() const
{
	return SNew(SHeaderRow);
}

void SPCGEditorGraphAttributeListView::OnInspectedComponentChanged(UPCGComponent* InPCGComponent)
{
	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
	}

	PCGComponent = InPCGComponent;

	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnGenerateUpdated);
		PCGComponent->OnPCGGraphCleanedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnGenerateUpdated);

		// Refresh if PCGComponent is being inspected already since we wont get a refresh after generation
		if (PCGComponent->IsInspecting())
		{
			RequestRefresh();
		}
	}
	else
	{
		// Refresh if PCGComponent is cleared since we wont get a refresh after generate/cleaned
		RequestRefresh();
	}
}

void SPCGEditorGraphAttributeListView::OnInspectedStackChanged(const FPCGStack& InPCGStack)
{
	RequestRefresh();
}

void SPCGEditorGraphAttributeListView::OnInspectedNodeChanged(UPCGEditorGraphNodeBase* InPCGEditorGraphNode)
{
	if (PCGEditorGraphNode == InPCGEditorGraphNode)
	{
		return;
	}
	
	PCGEditorGraphNode = InPCGEditorGraphNode;
	
	if (PCGEditorGraphNode.IsValid())
	{
		NodeNameTextBlock->SetText(PCGEditorGraphNode->GetNodeTitle(ENodeTitleType::FullTitle));
		NodeNameTextBlock->SetToolTipText(PCGEditorGraphNode->GetTooltipText());
	}
	else
	{
		NodeNameTextBlock->SetText(PCGEditorGraphAttributeListView::NoNodeInspectedText);
		NodeNameTextBlock->SetToolTipText(PCGEditorGraphAttributeListView::NoNodeInspectedToolTip);
	}

	RequestRefresh();
}

void SPCGEditorGraphAttributeListView::OnGenerateUpdated(UPCGComponent* /*InPCGComponent*/)
{
	RequestRefresh();
}

const FPCGDataCollection* SPCGEditorGraphAttributeListView::GetInspectionData()
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

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	const FPCGStack& PCGStack = PCGEditor->GetStackBeingInspected();

	const int32 SelectedIndex = GetSelectedPinIndex();
	if (SelectedIndex == INDEX_NONE)
	{
		return nullptr;
	}

	// Selected pin is an output if it's at the beginning of the list, otherwise it's an input
	const UPCGPin* Pin = nullptr;
	const int32 InputPinCount = PCGNode->GetInputPins().Num();
	const int32 OutputPinCount = PCGNode->GetOutputPins().Num();
	if (SelectedIndex < (InputPinCount + OutputPinCount))
	{
		Pin = (SelectedIndex < OutputPinCount) ? PCGNode->GetOutputPins()[SelectedIndex] : PCGNode->GetInputPins()[SelectedIndex - OutputPinCount];
	}
	if (!Pin)
	{
		return nullptr;
	}

	// Create a temporary stack with Node+Pin to query the exact DataCollection we are inspecting
	FPCGStack Stack = PCGStack;
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

	if (const UPCGParamData* PCGParamData = Cast<const UPCGParamData>(PCGData))
	{
		if (const UPCGMetadata* PCGMetadata = PCGParamData->ConstMetadata())
		{
			AddIndexColumn();
			GenerateColumnsFromMetadata(PCGData, PCGMetadata);

			PCGMetadataEntryKey ItemKeyLowerBound = PCGMetadata->GetItemKeyCountForParent();
			PCGMetadataEntryKey ItemKeyUpperBound = PCGMetadata->GetItemCountForChild();
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
				InfoTextBlock->SetText(FText::Format(LOCTEXT("MetadataInfoTextBlockWithCrcFmt", "Number of metadata: {0}  CRC: {1}"), ItemKeyUpperBound - ItemKeyLowerBound, InspectionData->Crc.GetValue()));
			}
		}
	}
	else if (const UPCGSpatialData* PCGSpatialData = Cast<const UPCGSpatialData>(PCGData))
	{
		if (const UPCGPointData* PCGPointData = PCGSpatialData->ToPointData())
		{
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
				InfoTextBlock->SetText(FText::Format(LOCTEXT("PointInfoTextBlockWithCrcFmt", "Number of points: {0}, CRC: {1}"), NumPoints, InspectionData->Crc.GetValue()));
			}
		}
	}

	if (PCGData && PCGData->HasCachedLastSelector())
	{
		const FText LastSelector = PCGData->GetCachedLastSelector().GetDisplayText();
		InfoTextBlock->SetText(FText::Format(PCGEditorGraphAttributeListView::LastLabelFormat, LastSelector, InfoTextBlock->GetText()));
	}

	RefreshSorting();
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
		const TArray<TObjectPtr<UPCGPin>> InPins,
		const FString& InFormatText,
		TArray<TSharedPtr<FPinComboBoxItem>>& InOutItems,
		int32* OutFirstConnectedItemIndex)
	{
		for (int32 i = 0; i < InPins.Num(); ++i)
		{
			// Pin is included in list if it is connected, or if it is an output pin.
			if (InPins[i] && (InPins[i]->IsConnected() || InPins[i]->IsOutputPin()))
			{
				FString ItemName = FString::Format(*InFormatText, { InPins[i]->Properties.Label.ToString() });
				InOutItems.Add(MakeShared<FPinComboBoxItem>(FName(ItemName), i));

				// Look for first connected, null pointer once found so only first is taken.
				if (OutFirstConnectedItemIndex && InPins[i]->IsConnected())
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

void SPCGEditorGraphAttributeListView::ApplyRowFilter()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPCGEditorGraphAttributeListView::ApplyRowFilter);

	FilteredListViewItems.Empty();

	const FString& FilterString = ActiveFilterText.ToString();
	if (!FilterString.IsEmpty())
	{
		for (const PCGListviewItemPtr& ListViewItem : ListViewItems)
		{
			const FPCGPointFilterExpressionContext PointFilterContext(ListViewItem.Get(), &PCGColumnData);
			if (TextFilter->TestTextFilter(PointFilterContext))
			{
				FilteredListViewItems.Add(ListViewItem);
			}
		}
		
		ListView->SetItemsSource(&FilteredListViewItems);
	}
	else
	{
		ListView->SetItemsSource(&ListViewItems);
	}

	ListView->RequestListRefresh();
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
		if (Column.ColumnId == PCGEditorGraphAttributeListView::NAME_IndexColumn)
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

int32 SPCGEditorGraphAttributeListView::GetSelectedPinIndex() const
{
	int32 Index = INDEX_NONE;
	if (const TSharedPtr<FPinComboBoxItem> SelectedPin = PinComboBox->GetSelectedItem())
	{
		PinComboBoxItems.Find(SelectedPin, Index);
	}

	return Index;
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
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Rotation.X"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Rotation.Y"));
				AddMetadataColumn(InPCGData, ColumnName, AttributeType, TEXT(".Rotation.Z"));
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
			if (Column.ColumnId != PCGEditorGraphAttributeListView::NAME_IndexColumn)
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
		if (Column.ColumnId == PCGEditorGraphAttributeListView::NAME_IndexColumn)
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

	RefreshSorting();
}

EColumnSortMode::Type SPCGEditorGraphAttributeListView::GetColumnSortMode(const FName InColumnId) const
{
	if (SortingColumn != InColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

void SPCGEditorGraphAttributeListView::RefreshSorting()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPCGEditorGraphAttributeListView::ColumnSort);
	
	if (SortingColumn == PCGEditorGraphAttributeListView::NAME_IndexColumn || SortMode == EColumnSortMode::None)
	{
		if (SortMode == EColumnSortMode::Ascending || SortMode == EColumnSortMode::None)
		{
			ListViewItems.Sort([](const PCGListviewItemPtr& LHS, const PCGListviewItemPtr& RHS)
			{
				return  LHS->Index < RHS->Index;
			});
		}
		else
		{
			ListViewItems.Sort([](const PCGListviewItemPtr& LHS, const PCGListviewItemPtr& RHS)
			{
				return  LHS->Index > RHS->Index;
			});
		}
	}
	else if (FPCGColumnData* ColumnData = PCGColumnData.Find(SortingColumn))
	{
		if (ColumnData->DataAccessor.IsValid() && ColumnData->DataKeys.IsValid())
		{
			auto Callback = [this, &ColumnData](auto Dummy)
			{
				using ValueType = decltype(Dummy);

				if constexpr (PCG::Private::MetadataTraits<ValueType>::CanCompare)
				{
					TArray<ValueType> CachedValues;
					// Strings need initialized values for later assignment
					if constexpr (PCG::Private::MetadataTraits<ValueType>::NeedsConstruction)
					{
						CachedValues.SetNum(ListViewItems.Num());
					}
					else
					{
						CachedValues.SetNumUninitialized(ListViewItems.Num());
					}

					ColumnData->DataAccessor->GetRange(TArrayView<ValueType>(CachedValues), 0, *ColumnData->DataKeys);

					auto SortAscending = [&CachedValues] (const PCGListviewItemPtr& LHS, const PCGListviewItemPtr& RHS)
					{
						const ValueType& LHSValue = CachedValues[LHS->Index];
						const ValueType& RHSValue = CachedValues[RHS->Index];

						if (PCG::Private::MetadataTraits<ValueType>::Equal(LHSValue, RHSValue))
						{
							return LHS->Index < RHS->Index;
						}

						return PCG::Private::MetadataTraits<ValueType>::Less(LHSValue, RHSValue);
					};

					auto SortDescending = [&CachedValues] (const PCGListviewItemPtr& LHS, const PCGListviewItemPtr& RHS)
					{
						const ValueType& LHSValue = CachedValues[LHS->Index];
						const ValueType& RHSValue = CachedValues[RHS->Index];

						if (PCG::Private::MetadataTraits<ValueType>::Equal(LHSValue, RHSValue))
						{
							return LHS->Index > RHS->Index;
						}

						return PCG::Private::MetadataTraits<ValueType>::Greater(LHSValue, RHSValue);
					};

					if (SortMode == EColumnSortMode::Ascending)
					{
						ListViewItems.Sort(SortAscending);
					}
					else if (SortMode == EColumnSortMode::Descending)
					{
						ListViewItems.Sort(SortDescending);
					}
				}
			};

			PCGMetadataAttribute::CallbackWithRightType(ColumnData->DataAccessor->GetUnderlyingType(), Callback);
		}
	}

	ApplyRowFilter();
}

void SPCGEditorGraphAttributeListView::OnFilterTextChanged(const FText& InFilterText)
{
	ActiveFilterText = InFilterText;
	TextFilter->SetFilterText(InFilterText);

	ApplyRowFilter();
	SearchBoxWidget->SetError(TextFilter->GetFilterErrorText());
}

void SPCGEditorGraphAttributeListView::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnCleared)
	{
		SearchBoxWidget->SetText(FText::GetEmpty());
		OnFilterTextChanged(FText::GetEmpty());
	}
}

void SPCGEditorGraphAttributeListView::AddColumn(const UPCGPointData* InPCGPointData, const FName& InColumnId, const FText& ColumnLabel, EHorizontalAlignment HeaderHAlign, EHorizontalAlignment CellHAlign)
{
	if (InPCGPointData)
	{
		const FString ColumnIdString = InColumnId.ToString();
		
		FPCGAttributePropertyInputSelector TargetSelector;
		TargetSelector.Update(ColumnIdString);
	
		FPCGColumnData& ColumnData = PCGColumnData.Add(InColumnId);
		
		if (InColumnId == PCGEditorGraphAttributeListView::NAME_PointMetadataEntry)
		{
			ColumnData.DataAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(GET_MEMBER_NAME_CHECKED(FPCGPoint, MetadataEntry), FPCGPoint::StaticStruct());
			ColumnData.DataKeys = MakeUnique<FPCGAttributeAccessorKeysPoints>(InPCGPointData->GetPoints());
		}
		else if (InColumnId == PCGEditorGraphAttributeListView::NAME_PointMetadataEntryParent)
		{
			ColumnData.DataAccessor = MakeUnique<FPCGCustomPointAccessor<int64>>([InPCGPointData](const FPCGPoint& Point, void* OutValue)
			{
				if (const UPCGMetadata* Metadata = InPCGPointData->Metadata)
				{
					*reinterpret_cast<int64*>(OutValue) = Metadata->GetParentKey(Point.MetadataEntry);
					return true;
				}
				return false;
			}, nullptr);
			ColumnData.DataKeys = MakeUnique<FPCGAttributeAccessorKeysPoints>(InPCGPointData->GetPoints());
		}
		else
		{	
			ColumnData.DataAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPCGPointData, TargetSelector);
			ColumnData.DataKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InPCGPointData, TargetSelector);
		}
	}

	const float ColumnWidth = PCGEditorGraphAttributeListView::CalculateColumnWidth(ColumnLabel);
	
	SHeaderRow::FColumn::FArguments Arguments;
	Arguments.ColumnId(InColumnId);
	Arguments.DefaultLabel(ColumnLabel);
	Arguments.ManualWidth(ColumnWidth);
	Arguments.HAlignHeader(HeaderHAlign);
	Arguments.HAlignCell(CellHAlign);
	Arguments.SortMode(this, &SPCGEditorGraphAttributeListView::GetColumnSortMode, InColumnId);
	Arguments.OnSort(this, &SPCGEditorGraphAttributeListView::OnColumnSortModeChanged);
	Arguments.OverflowPolicy(ETextOverflowPolicy::Ellipsis);

	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(Arguments);
	NewColumn->bIsVisible = !HiddenAttributes.Contains(InColumnId); 
	ListViewHeader->AddColumn(*NewColumn);
}

void SPCGEditorGraphAttributeListView::AddIndexColumn()
{
	AddColumn(nullptr, PCGEditorGraphAttributeListView::NAME_IndexColumn, PCGEditorGraphAttributeListView::TEXT_IndexLabel);
}

void SPCGEditorGraphAttributeListView::AddPointDataColumns(const UPCGPointData* InPCGPointData)
{
	AddIndexColumn();
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointPositionX, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelX);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointPositionY, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelY);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointPositionZ, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelZ);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointRotationX, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelX);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointRotationY, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelY);
	AddColumn(InPCGPointData, PCGEditorGraphAttributeListView::NAME_PointRotationZ, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelZ);
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
		ColumnData.DataAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPCGData, TargetSelector);
		ColumnData.DataKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InPCGData, TargetSelector);
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
	ColumnArguments.HAlignHeader(EHorizontalAlignment::HAlign_Center);
	ColumnArguments.HAlignCell(CellAlignment);
	ColumnArguments.ManualWidth(ColumnWidth);
	ColumnArguments.SortMode(this, &SPCGEditorGraphAttributeListView::GetColumnSortMode, ColumnId);
	ColumnArguments.OnSort(this, &SPCGEditorGraphAttributeListView::OnColumnSortModeChanged);

	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(ColumnArguments);
	NewColumn->bIsVisible = !HiddenAttributes.Contains(ColumnId);
	ListViewHeader->AddColumn(*NewColumn);
}

#undef LOCTEXT_NAMESPACE
