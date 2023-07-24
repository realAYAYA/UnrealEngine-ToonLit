// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphAttributeListView.h"

#include "PCGComponent.h"
#include "PCGEditor.h"
#include "PCGEditorGraphNodeBase.h"
#include "PCGParamData.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#include "Fonts/FontMeasure.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphAttributeListView"

static TAutoConsoleVariable<bool> CVarShowAdvancedAttributesFields(
	TEXT("pcg.graph.ShowAdvancedAttributes"),
	false,
	TEXT("Control whether advanced attributes/properties are shown in the PCG graph editor"));

namespace PCGEditorGraphAttributeListView
{
	const FText NoDataAvailableText = LOCTEXT("NoDataAvailableText", "No data available");
	const FText NoNodeInspectedText = LOCTEXT("NoNodeInspectedText", "No node being inspected");
	const FText NoNodeInspectedToolTip = LOCTEXT("NoNodeInspectedToolTip", "Inspect a node using the right click menu");
	
	/** Names of the columns in the attribute list */
	const FName NAME_IndexColumn = FName(TEXT("IndexColumn"));
	const FName NAME_PointPositionX = FName(TEXT("PointPositionX"));
	const FName NAME_PointPositionY = FName(TEXT("PointPositionY"));
	const FName NAME_PointPositionZ = FName(TEXT("PointPositionZ"));
	const FName NAME_PointRotationX = FName(TEXT("PointRotationX"));
	const FName NAME_PointRotationY = FName(TEXT("PointRotationY"));
	const FName NAME_PointRotationZ = FName(TEXT("PointRotationZ"));
	const FName NAME_PointScaleX = FName(TEXT("PointScaleX"));
	const FName NAME_PointScaleY = FName(TEXT("PointScaleY"));
	const FName NAME_PointScaleZ = FName(TEXT("PointScaleZ"));
	const FName NAME_PointBoundsMinX = FName(TEXT("PointBoundsMinX"));
	const FName NAME_PointBoundsMinY = FName(TEXT("PointBoundsMinY"));
	const FName NAME_PointBoundsMinZ = FName(TEXT("PointBoundsMinZ"));
	const FName NAME_PointBoundsMaxX = FName(TEXT("PointBoundsMaxX"));
	const FName NAME_PointBoundsMaxY = FName(TEXT("PointBoundsMaxY"));
	const FName NAME_PointBoundsMaxZ = FName(TEXT("PointBoundsMaxZ"));
	const FName NAME_PointColorR = FName(TEXT("PointColorR"));
	const FName NAME_PointColorG = FName(TEXT("PointColorG"));
	const FName NAME_PointColorB = FName(TEXT("PointColorB"));
	const FName NAME_PointColorA = FName(TEXT("PointColorA"));
	const FName NAME_PointDensity = FName(TEXT("PointDensity"));
	const FName NAME_PointSteepness = FName(TEXT("PointSteepness"));
	const FName NAME_PointSeed = FName(TEXT("PointSeed"));
	const FName NAME_PointMetadataEntry = FName(TEXT("PointMetadataEntry"));
	const FName NAME_PointMetadataEntryParent = FName(TEXT("PointMetadataEntryParent"));

	/** Labels of the columns */
	const FText TEXT_IndexLabel = LOCTEXT("IndexLabel", "Index");
	const FText TEXT_PointPositionLabelX = LOCTEXT("PointPositionLabelX", "PositionX");
	const FText TEXT_PointPositionLabelY = LOCTEXT("PointPositionLabelY", "PositionY");
	const FText TEXT_PointPositionLabelZ = LOCTEXT("PointPositionLabelZ", "PositionZ");
	const FText TEXT_PointRotationLabelX = LOCTEXT("PointRotationLabelX", "RotationX");
	const FText TEXT_PointRotationLabelY = LOCTEXT("PointRotationLabelY", "RotationY");
	const FText TEXT_PointRotationLabelZ = LOCTEXT("PointRotationLabelZ", "RotationZ");
	const FText TEXT_PointScaleLabelX = LOCTEXT("PointScaleLabelX", "ScaleX");
	const FText TEXT_PointScaleLabelY = LOCTEXT("PointScaleLabelY", "ScaleY");
	const FText TEXT_PointScaleLabelZ = LOCTEXT("PointScaleLabelZ", "ScaleZ");
	const FText TEXT_PointBoundsLabelMinX = LOCTEXT("PointBoundsMinX", "BoundsMinX");
	const FText TEXT_PointBoundsLabelMinY = LOCTEXT("PointBoundsMinY", "BoundsMinY");
	const FText TEXT_PointBoundsLabelMinZ = LOCTEXT("PointBoundsMinZ", "BoundsMinZ");
	const FText TEXT_PointBoundsLabelMaxX = LOCTEXT("PointBoundsMaxX", "BoundsMaxX");
	const FText TEXT_PointBoundsLabelMaxY = LOCTEXT("PointBoundsMaxY", "BoundsMaxY");
	const FText TEXT_PointBoundsLabelMaxZ = LOCTEXT("PointBoundsMaxZ", "BoundsMaxZ");
	const FText TEXT_PointColorLabelR = LOCTEXT("PointColorR", "ColorR");
	const FText TEXT_PointColorLabelG = LOCTEXT("PointColorG", "ColorG");
	const FText TEXT_PointColorLabelB = LOCTEXT("PointColorB", "ColorB");
	const FText TEXT_PointColorLabelA = LOCTEXT("PointColorA", "ColorA");
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
		constexpr float ColumnPadding = 12.0f; // TODO: Grab padding from header style
		const float ColumnWidth = TextWidth + ColumnPadding;
		return FMath::Min(ColumnWidth, MaxColumnWidth);
	}
}

void SPCGListViewItemRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const PCGListviewItemPtr& Item)
{
	InternalItem = Item;

	SMultiColumnTableRow<PCGListviewItemPtr>::Construct(
		SMultiColumnTableRow::FArguments()
		.Style(FAppStyle::Get(), "DataTableEditor.CellListViewRow"),
		InOwnerTableView);
}

TSharedRef<SWidget> SPCGListViewItemRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	FText ColumnData = LOCTEXT("ColumnError", "Unrecognized Column");

	if (ColumnId == PCGEditorGraphAttributeListView::NAME_IndexColumn)
	{
		ColumnData = FText::FromString(FString::FromInt(InternalItem->Index));
		return SNew(STextBlock).Text(ColumnData);
	}

	if (const FPCGPoint* PCGPoint = InternalItem->PCGPoint)
	{
		ColumnData = ConvertPointDataToText(PCGPoint, ColumnId);
	}
	else if (const UPCGMetadata* PCGMetadata = InternalItem->PCGMetadata)
	{
		if (const FPCGMetadataInfo* MetadataInfo = InternalItem->MetadataInfos->Find(ColumnId))
		{
			if (const FPCGMetadataAttributeBase* AttributeBase = PCGMetadata->GetConstAttribute((*MetadataInfo).MetadataId))
			{
				ColumnData = ConvertMetadataAttributeToText(AttributeBase, MetadataInfo, InternalItem->MetaDataItemKey);
			}
		}
	}

	return SNew(STextBlock)
		.Text(ColumnData)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		.Margin(FMargin(2.0f, 0.0f));
}

FText SPCGListViewItemRow::ConvertPointDataToText(const FPCGPoint* PCGPoint, const FName& ColumnId) const
{
	check(PCGPoint);

	const FTransform& Transform = PCGPoint->Transform;
	if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointPositionX)
	{
		const FVector& Position = Transform.GetLocation();
		return FText::AsNumber(Position.X);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointPositionY)
	{
		const FVector& Position = Transform.GetLocation();
		return FText::AsNumber(Position.Y);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointPositionZ)
	{
		const FVector& Position = Transform.GetLocation();
		return FText::AsNumber(Position.Z);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointRotationX)
	{
		const FRotator& Rotation = Transform.Rotator();
		return FText::AsNumber(Rotation.Roll);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointRotationY)
	{
		const FRotator& Rotation = Transform.Rotator();
		return FText::AsNumber(Rotation.Pitch);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointRotationZ)
	{
		const FRotator& Rotation = Transform.Rotator();
		return FText::AsNumber(Rotation.Yaw);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointScaleX)
	{
		const FVector& Scale = Transform.GetScale3D();
		return FText::AsNumber(Scale.X);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointScaleY)
	{
		const FVector& Scale = Transform.GetScale3D();
		return FText::AsNumber(Scale.Y);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointScaleZ)
	{
		const FVector& Scale = Transform.GetScale3D();
		return FText::AsNumber(Scale.Z);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointBoundsMinX)
	{
		return FText::AsNumber(PCGPoint->BoundsMin.X);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointBoundsMinY)
	{
		return FText::AsNumber(PCGPoint->BoundsMin.Y);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointBoundsMinZ)
	{
		return FText::AsNumber(PCGPoint->BoundsMin.Z);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointBoundsMaxX)
	{
		return FText::AsNumber(PCGPoint->BoundsMax.X);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointBoundsMaxY)
	{
		return FText::AsNumber(PCGPoint->BoundsMax.Y);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointBoundsMaxZ)
	{
		return FText::AsNumber(PCGPoint->BoundsMax.Z);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointColorR)
	{
		return FText::AsNumber(PCGPoint->Color.X);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointColorG)
	{
		return FText::AsNumber(PCGPoint->Color.Y);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointColorB)
	{
		return FText::AsNumber(PCGPoint->Color.Z);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointColorA)
	{
		return FText::AsNumber(PCGPoint->Color.W);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointDensity)
	{
		const float Density = PCGPoint->Density;
		return FText::AsNumber(Density);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointSteepness)
	{
		const float Steepness = PCGPoint->Steepness;
		return FText::AsNumber(Steepness);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointSeed)
	{
		const int32 Seed = PCGPoint->Seed;
		return FText::AsNumber(Seed);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointMetadataEntry)
	{
		const int64 MetadataEntryKey = PCGPoint->MetadataEntry;
		return FText::AsNumber(MetadataEntryKey);
	}
	else if (ColumnId == PCGEditorGraphAttributeListView::NAME_PointMetadataEntryParent)
	{
		if(InternalItem->PCGMetadata)
		{
			return FText::AsNumber(InternalItem->PCGMetadata->GetParentKey(PCGPoint->MetadataEntry));
		}
		else
		{
			return FText::AsNumber(PCGInvalidEntryKey);
		}
	}
	// None of the default point columns were caught, see if its metadata
	else if (const UPCGMetadata* PCGMetadata = InternalItem->PCGMetadata)
	{
		if (const FPCGMetadataInfo* MetadataInfo = InternalItem->MetadataInfos->Find(ColumnId))
		{
			if (const FPCGMetadataAttributeBase* AttributeBase = PCGMetadata->GetConstAttribute((*MetadataInfo).MetadataId))
			{
				return ConvertMetadataAttributeToText(AttributeBase, MetadataInfo, InternalItem->MetaDataItemKey);
			}
		}
	}

	return PCGEditorGraphAttributeListView::NoDataAvailableText;
}

FText SPCGListViewItemRow::ConvertMetadataAttributeToText(const FPCGMetadataAttributeBase* AttributeBase, const FPCGMetadataInfo* MetadataInfo, int64 ItemKey) const
{
	switch (AttributeBase->GetTypeId())
	{
	case PCG::Private::MetadataTypes<float>::Id:
		{
			const float MetaFloat = static_cast<const FPCGMetadataAttribute<float>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::AsNumber(MetaFloat);
		}
	case PCG::Private::MetadataTypes<double>::Id:
		{
			const double MetaDouble = static_cast<const FPCGMetadataAttribute<double>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::AsNumber(MetaDouble);
		}
	case PCG::Private::MetadataTypes<bool>::Id:
		{
			const bool bMetaBool = static_cast<const FPCGMetadataAttribute<bool>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return bMetaBool ? FText::FromString(TEXT("true")) : FText::FromString(TEXT("false"));
		}
	case PCG::Private::MetadataTypes<FVector2D>::Id:
	{
		const FVector2D MetaVector = static_cast<const FPCGMetadataAttribute<FVector2D>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
		return FText::AsNumber(MetaVector[(*MetadataInfo).Index]);
	}
	case PCG::Private::MetadataTypes<FVector>::Id:
		{
			const FVector MetaVector = static_cast<const FPCGMetadataAttribute<FVector>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::AsNumber(MetaVector[(*MetadataInfo).Index]);
		}
	case PCG::Private::MetadataTypes<FVector4>::Id:
		{
			const FVector4 MetaVector4 = static_cast<const FPCGMetadataAttribute<FVector4>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::AsNumber(MetaVector4[(*MetadataInfo).Index]);
		}
	case PCG::Private::MetadataTypes<int32>::Id:
		{
			const int32 MetaInt32 = static_cast<const FPCGMetadataAttribute<int32>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::AsNumber(MetaInt32);
		}
	case PCG::Private::MetadataTypes<int64>::Id:
		{
			const int64 MetaInt64 = static_cast<const FPCGMetadataAttribute<int64>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::AsNumber(MetaInt64);
		}
	case PCG::Private::MetadataTypes<FString>::Id:
		{
			const FString MetaString = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::FromString(MetaString);
		}
	case PCG::Private::MetadataTypes<FName>::Id:
		{
			const FName MetaName = static_cast<const FPCGMetadataAttribute<FName>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			return FText::FromName(MetaName);
		}
	case PCG::Private::MetadataTypes<FQuat>::Id:
		{
			const FQuat MetaQuat = static_cast<const FPCGMetadataAttribute<FQuat>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			float QuatValue = 0.0f;
			if ((*MetadataInfo).Index == 0)
			{
				QuatValue = MetaQuat.X;
			}
			else if ((*MetadataInfo).Index == 1)
			{
				QuatValue = MetaQuat.Y;
			}
			else if ((*MetadataInfo).Index == 2)
			{
				QuatValue = MetaQuat.Z;
			}
			else if ((*MetadataInfo).Index == 3)
			{
				QuatValue = MetaQuat.W;
			}
			return FText::AsNumber(QuatValue);
		}
	case PCG::Private::MetadataTypes<FRotator>::Id:
		{
			const FRotator MetaRotator = static_cast<const FPCGMetadataAttribute<FRotator>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			float RotatorValue = 0.0f;
			if ((*MetadataInfo).Index == 0)
			{
				RotatorValue = MetaRotator.Roll;
			}
			else if ((*MetadataInfo).Index == 1)
			{
				RotatorValue = MetaRotator.Pitch;
			}
			else if ((*MetadataInfo).Index == 2)
			{
				RotatorValue = MetaRotator.Yaw;
			}
			return FText::AsNumber(RotatorValue);
		}
	case PCG::Private::MetadataTypes<FTransform>::Id:
		{
			const FTransform& MetaTransform = static_cast<const FPCGMetadataAttribute<FTransform>*>(AttributeBase)->GetValueFromItemKey(ItemKey);
			const int8 ComponentIndex = (*MetadataInfo).Index / 3;
			const int8 ValueIndex = (*MetadataInfo).Index % 3;

			if (ComponentIndex == 0)
			{
				const FVector& MetaVector = MetaTransform.GetLocation();
				return FText::AsNumber(MetaVector[ValueIndex]);
			}
			else if (ComponentIndex == 1)
			{
				const FRotator& MetaRotator = MetaTransform.Rotator();
				float RotatorValue = 0.0f;
				if (ValueIndex == 0)
				{
					RotatorValue = MetaRotator.Roll;
				}
				else if (ValueIndex == 1)
				{
					RotatorValue = MetaRotator.Pitch;
				}
				else if (ValueIndex == 2)
				{
					RotatorValue = MetaRotator.Yaw;
				}
				return FText::AsNumber(RotatorValue);
			}
			else if (ComponentIndex == 2)
			{
				const FVector& MetaVector = MetaTransform.GetScale3D();
				return FText::AsNumber(MetaVector[ValueIndex]);
			}
		}
	default:
		return PCGEditorGraphAttributeListView::NoDataAvailableText;
	}
}

SPCGEditorGraphAttributeListView::~SPCGEditorGraphAttributeListView()
{
	if (PCGEditorPtr.IsValid())
	{
		PCGEditorPtr.Pin()->OnDebugObjectChangedDelegate.RemoveAll(this);
		PCGEditorPtr.Pin()->OnInspectedNodeChangedDelegate.RemoveAll(this);
	}
}

void SPCGEditorGraphAttributeListView::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;

	PCGEditorPtr.Pin()->OnDebugObjectChangedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnDebugObjectChanged);
	PCGEditorPtr.Pin()->OnInspectedNodeChangedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnInspectedNodeChanged);

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
			[
				FilterButton->AsShared()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				DataComboBox->AsShared()
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
			+ SHorizontalBox::Slot()
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

TSharedRef<SHeaderRow> SPCGEditorGraphAttributeListView::CreateHeaderRowWidget() const
{
	return SNew(SHeaderRow);
}

void SPCGEditorGraphAttributeListView::OnDebugObjectChanged(UPCGComponent* InPCGComponent)
{
	if (PCGComponent.IsValid())
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
		PCGComponent->DisableInspection();
	}

	PCGComponent = InPCGComponent;

	if (PCGComponent.IsValid())
	{
		PCGComponent->EnableInspection();
		PCGComponent->OnPCGGraphGeneratedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnGenerateUpdated);
		PCGComponent->OnPCGGraphCleanedDelegate.AddSP(this, &SPCGEditorGraphAttributeListView::OnGenerateUpdated);
	}
	else
	{
		RefreshDataComboBox();
		RefreshAttributeList();
	}
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
	
	RefreshDataComboBox();
	RefreshAttributeList();
}

void SPCGEditorGraphAttributeListView::OnGenerateUpdated(UPCGComponent* /*InPCGComponent*/)
{
	RefreshDataComboBox();
	RefreshAttributeList();
}

void SPCGEditorGraphAttributeListView::RefreshAttributeList()
{
	HiddenAttributes = ListViewHeader->GetHiddenColumnIds();
	
	// Swapping to an empty item list to force a widget clear, otherwise the widgets will try to update during add column and access invalid data
	static const TArray<PCGListviewItemPtr> EmptyList;
	ListView->SetItemsSource(&EmptyList);
	
	ListViewItems.Empty();
	ListViewHeader->ClearColumns();
	MetadataInfos.Empty();
	InfoTextBlock->SetText(FText::GetEmpty());
	
	if (!PCGComponent.IsValid())
	{
		return;
	}

	if (!PCGEditorGraphNode.IsValid())
	{
		return;
	}

	const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
	if (!PCGNode)
	{
		return;
	}

	const FPCGDataCollection* InspectionData = PCGComponent->GetInspectionData(PCGNode);
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
			GenerateColumnsFromMetadata(PCGMetadata);

			PCGMetadataEntryKey ItemKeyLowerBound = PCGMetadata->GetItemKeyCountForParent();
			PCGMetadataEntryKey ItemKeyUpperBound = PCGMetadata->GetItemCountForChild();
			for (PCGMetadataEntryKey MetadataItemKey = ItemKeyLowerBound; MetadataItemKey < ItemKeyUpperBound; ++MetadataItemKey)
			{
				PCGListviewItemPtr ListViewItem = MakeShared<FPCGListViewItem>();
				ListViewItem->Index = MetadataItemKey - ItemKeyLowerBound;
				ListViewItem->PCGParamData = PCGParamData;
				ListViewItem->PCGMetadata = PCGMetadata;
				ListViewItem->MetaDataItemKey = MetadataItemKey;
				ListViewItem->MetadataInfos = &MetadataInfos;
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

			AddPointDataColumns();
			GenerateColumnsFromMetadata(PCGMetadata);

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
				ListViewItem->PCGMetadata = PCGMetadata;
				ListViewItem->MetaDataItemKey = PCGPoint.MetadataEntry;
				ListViewItem->MetadataInfos = &MetadataInfos;
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

	ListView->SetItemsSource(&ListViewItems);
}

void SPCGEditorGraphAttributeListView::RefreshDataComboBox()
{
	DataComboBoxItems.Empty();
	DataComboBox->ClearSelection();
	DataComboBox->RefreshOptions();

	if (!PCGComponent.IsValid())
	{
		return;
	}

	if (!PCGEditorGraphNode.IsValid())
	{
		return;
	}

	const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
	if (!PCGNode)
	{
		return;
	}

	const FPCGDataCollection* InspectionData = PCGComponent->GetInspectionData(PCGNode);
	if (!InspectionData)
	{
		return;
	}

	for(int32 TaggedDataIndex = 0; TaggedDataIndex < InspectionData->TaggedData.Num(); ++TaggedDataIndex)
	{
		const FPCGTaggedData& TaggedData = InspectionData->TaggedData[TaggedDataIndex];
		FString ItemName = FString::Format(TEXT("[{0}] {1} - {2}"),
			{ FText::AsNumber(TaggedDataIndex).ToString(), TaggedData.Pin.ToString(), (TaggedData.Data ? TaggedData.Data->GetClass()->GetDisplayNameText().ToString(): TEXT("No Data")) });

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

void SPCGEditorGraphAttributeListView::GenerateColumnsFromMetadata(const UPCGMetadata* PCGMetadata)
{
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;
	PCGMetadata->GetAttributes(AttributeNames, AttributeTypes);

	for (int32 I = 0; I < AttributeNames.Num(); I++)
	{
		const FName& AttributeName = AttributeNames[I];
		const EPCGMetadataTypes AttributeType = AttributeTypes[I];
		FName ColumnName = AttributeName;

		if (ColumnName == NAME_None)
		{
			const FString TypeString = UEnum::GetDisplayValueAsText(AttributeType).ToString();
			ColumnName = *TypeString;
		}

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
				AddMetadataColumn(ColumnName, AttributeName, AttributeType);
				break;
			}
		case EPCGMetadataTypes::Vector2:
		{
			AddMetadataColumn(ColumnName, AttributeName, AttributeType, 0, TEXT("_X"));
			AddMetadataColumn(ColumnName, AttributeName, AttributeType, 1, TEXT("_Y"));
			break;
		}
		case EPCGMetadataTypes::Vector:
			{
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 0, TEXT("_X"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 1, TEXT("_Y"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 2, TEXT("_Z"));
				break;
			}
		case EPCGMetadataTypes::Vector4:
		case EPCGMetadataTypes::Quaternion:
			{
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 0, TEXT("_X"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 1, TEXT("_Y"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 2, TEXT("_Z"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 3, TEXT("_W"));
				break;
			}
		case EPCGMetadataTypes::Transform:
			{
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 0, TEXT("_tX"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 1, TEXT("_tY"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 2, TEXT("_tZ"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 3, TEXT("_rX"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 4, TEXT("_rY"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 5, TEXT("_rZ"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 6, TEXT("_sX"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 7, TEXT("_sY"));
				AddMetadataColumn(ColumnName, AttributeName, AttributeType, 8, TEXT("_sZ"));
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

TSharedRef<ITableRow> SPCGEditorGraphAttributeListView::OnGenerateRow(PCGListviewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SPCGListViewItemRow, OwnerTable, Item);
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

void SPCGEditorGraphAttributeListView::AddColumn(const FName& InColumnId, const FText& ColumnLabel, EHorizontalAlignment HeaderHAlign, EHorizontalAlignment CellHAlign)
{
	const float ColumnWidth = PCGEditorGraphAttributeListView::CalculateColumnWidth(ColumnLabel);

	SHeaderRow::FColumn::FArguments Arguments;
	Arguments.ColumnId(InColumnId);
	Arguments.DefaultLabel(ColumnLabel);
	Arguments.ManualWidth(ColumnWidth);
	Arguments.HAlignHeader(HeaderHAlign);
	Arguments.HAlignCell(CellHAlign);

	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(Arguments);
	NewColumn->bIsVisible = !HiddenAttributes.Contains(InColumnId); 
	ListViewHeader->AddColumn(*NewColumn);
}

void SPCGEditorGraphAttributeListView::RemoveColumn(const FName& InColumnId)
{
	ListViewHeader->RemoveColumn(InColumnId);
}

void SPCGEditorGraphAttributeListView::AddIndexColumn()
{
	AddColumn(PCGEditorGraphAttributeListView::NAME_IndexColumn, PCGEditorGraphAttributeListView::TEXT_IndexLabel);
}

void SPCGEditorGraphAttributeListView::RemoveIndexColumn()
{
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_IndexColumn);
}

void SPCGEditorGraphAttributeListView::AddPointDataColumns()
{
	AddIndexColumn();
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointPositionX, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelX);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointPositionY, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelY);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointPositionZ, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelZ);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointRotationX, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelX);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointRotationY, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelY);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointRotationZ, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelZ);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointScaleX, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelX);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointScaleY, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelY);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointScaleZ, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelZ);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinX, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinX);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinY, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinY);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinZ, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinZ);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxX, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxX);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxY, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxY);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxZ, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxZ);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorR, PCGEditorGraphAttributeListView::TEXT_PointColorLabelR);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorG, PCGEditorGraphAttributeListView::TEXT_PointColorLabelG);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorB, PCGEditorGraphAttributeListView::TEXT_PointColorLabelB);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorA, PCGEditorGraphAttributeListView::TEXT_PointColorLabelA);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointDensity, PCGEditorGraphAttributeListView::TEXT_PointDensityLabel);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointSteepness, PCGEditorGraphAttributeListView::TEXT_PointSteepnessLabel);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointSeed, PCGEditorGraphAttributeListView::TEXT_PointSeedLabel);

	if (CVarShowAdvancedAttributesFields.GetValueOnAnyThread())
	{
		AddColumn(PCGEditorGraphAttributeListView::NAME_PointMetadataEntry, PCGEditorGraphAttributeListView::TEXT_PointMetadataEntryLabel);
		AddColumn(PCGEditorGraphAttributeListView::NAME_PointMetadataEntryParent, PCGEditorGraphAttributeListView::TEXT_PointMetadataEntryParentLabel);
	}	
}

void SPCGEditorGraphAttributeListView::RemovePointDataColumns()
{
	RemoveIndexColumn();
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointPositionX);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointPositionY);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointPositionZ);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointRotationX);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointRotationY);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointRotationZ);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointScaleX);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointScaleY);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointScaleZ);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinX);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinY);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinZ);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxX);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxY);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxZ);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointColorR);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointColorG);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointColorB);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointColorA);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointDensity);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointSteepness);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointSeed);

	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointMetadataEntry);
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_PointMetadataEntryParent);
}

void SPCGEditorGraphAttributeListView::AddMetadataColumn(const FName& InColumnId, const FName& InMetadataId, EPCGMetadataTypes InMetadataType, const int8 InValueIndex, const TCHAR* PostFix)
{
	FString ColumnIdString = InColumnId.ToString();

	if (PostFix)
	{
		ColumnIdString.Append(PostFix);
	}

	const FName ColumnId(ColumnIdString);

	FPCGMetadataInfo MetadataInfo;
	MetadataInfo.MetadataId = InMetadataId;
	MetadataInfo.Index = InValueIndex;
	MetadataInfos.Add(ColumnId, MetadataInfo);

	const FText ColumnLabel = FText::FromName(ColumnId);
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

	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn(ColumnArguments);
	NewColumn->bIsVisible = !HiddenAttributes.Contains(InColumnId);
	ListViewHeader->AddColumn(*NewColumn);
}

void SPCGEditorGraphAttributeListView::RemoveMetadataColumns()
{
	for (const TTuple<FName, FPCGMetadataInfo>& MetadataInfo : MetadataInfos)
	{
		ListViewHeader->RemoveColumn(MetadataInfo.Key);
	}
}

#undef LOCTEXT_NAMESPACE
