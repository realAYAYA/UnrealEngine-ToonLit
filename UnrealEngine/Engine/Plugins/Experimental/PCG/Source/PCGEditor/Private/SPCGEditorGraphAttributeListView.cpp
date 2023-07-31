// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphAttributeListView.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGEditor.h"
#include "PCGNode.h"
#include "PCGParamData.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphAttributeListView"

namespace PCGEditorGraphAttributeListView
{
	const FText NoDataAvailableText = LOCTEXT("NoDataAvailableText", "No data available");

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

	return SNew(STextBlock).Text(ColumnData);
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

	SAssignNew(ListView, SListView<PCGListviewItemPtr>)
		.ListItemsSource(&ListViewItems)
		.HeaderRow(ListViewHeader)
		.OnGenerateRow(this, &SPCGEditorGraphAttributeListView::OnGenerateRow)
		.AllowOverscroll(EAllowOverscroll::No)
		.ExternalScrollbar(VerticalScrollBar)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always);

	SAssignNew(DataComboBox, SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&DataComboBoxItems)
		.OnGenerateWidget(this, &SPCGEditorGraphAttributeListView::OnGenerateDataWidget)
		.OnSelectionChanged(this, &SPCGEditorGraphAttributeListView::OnSelectionChanged)
		[
			SNew(STextBlock)
			.Text(this, &SPCGEditorGraphAttributeListView::OnGenerateSelectedDataText)
		];

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			DataComboBox->AsShared()
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
	return SNew(SHeaderRow)
			.ResizeMode(ESplitterResizeMode::FixedPosition)
			.CanSelectGeneratedColumn(true);
}

void SPCGEditorGraphAttributeListView::OnDebugObjectChanged(UPCGComponent* InPCGComponent)
{
	if (PCGComponent)
	{
		PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCGComponent->OnPCGGraphCleanedDelegate.RemoveAll(this);
		PCGComponent->DisableInspection();
	}

	PCGComponent = InPCGComponent;

	if (PCGComponent)
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

void SPCGEditorGraphAttributeListView::OnInspectedNodeChanged(UPCGNode* /*InPCGNode*/)
{
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
	// Swapping to an empty item list to force a widget clear, otherwise the widgets will try to update during add column and access invalid data
	static const TArray<PCGListviewItemPtr> EmptyList;
	ListView->SetListItemsSource(EmptyList);
	
	ListViewItems.Empty();
	ListViewHeader->ClearColumns();
	MetadataInfos.Empty();

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (!PCGEditor.IsValid())
	{
		return;
	}

	if (!PCGComponent)
	{
		return;
	}

	const UPCGNode* PCGNode = PCGEditor->GetPCGNodeBeingInspected();
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
			ListViewItems.Reserve(PCGPoints.Num());
			for (int32 PointIndex = 0; PointIndex < PCGPoints.Num(); PointIndex++)
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
		}
	}

	ListView->SetListItemsSource(ListViewItems);
}

void SPCGEditorGraphAttributeListView::RefreshDataComboBox()
{
	DataComboBoxItems.Empty();
	DataComboBox->ClearSelection();
	DataComboBox->RefreshOptions();

	if (!PCGComponent)
	{
		return;
	}

	const TSharedPtr<FPCGEditor> PCGEditor = PCGEditorPtr.Pin();
	if (!PCGEditor.IsValid())
	{
		return;
	}

	const UPCGNode* PCGNode = PCGEditor->GetPCGNodeBeingInspected();
	if (!PCGNode)
	{
		return;
	}

	const FPCGDataCollection* InspectionData = PCGComponent->GetInspectionData(PCGNode);
	if (!InspectionData)
	{
		return;
	}

	for (const FPCGTaggedData& TaggedData : InspectionData->TaggedData)
	{
		DataComboBoxItems.Add(MakeShared<FName>(TaggedData.Pin));
	}

	if (DataComboBoxItems.Num() > 0)
	{
		DataComboBox->SetSelectedItem(DataComboBoxItems[0]);
	}
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
		FName ColumnName = AttributeName;

		if (ColumnName == NAME_None)
		{
			const FString TypeString = UEnum::GetDisplayValueAsText(AttributeTypes[I]).ToString();
			ColumnName = *TypeString;
		}

		switch (AttributeTypes[I])
		{
		case EPCGMetadataTypes::Float:
		case EPCGMetadataTypes::Double:
		case EPCGMetadataTypes::Integer32:
		case EPCGMetadataTypes::Integer64:
		case EPCGMetadataTypes::Boolean:
		case EPCGMetadataTypes::String:
		case EPCGMetadataTypes::Name:
			{
				AddMetadataColumn(ColumnName, AttributeName);
				break;
			}
		case EPCGMetadataTypes::Vector2:
		{
			AddMetadataColumn(ColumnName, AttributeName, 0, TEXT("_X"));
			AddMetadataColumn(ColumnName, AttributeName, 1, TEXT("_Y"));
			break;
		}
		case EPCGMetadataTypes::Vector:
			{
				AddMetadataColumn(ColumnName, AttributeName, 0, TEXT("_X"));
				AddMetadataColumn(ColumnName, AttributeName, 1, TEXT("_Y"));
				AddMetadataColumn(ColumnName, AttributeName, 2, TEXT("_Z"));
				break;
			}
		case EPCGMetadataTypes::Vector4:
		case EPCGMetadataTypes::Quaternion:
			{
				AddMetadataColumn(ColumnName, AttributeName, 0, TEXT("_X"));
				AddMetadataColumn(ColumnName, AttributeName, 1, TEXT("_Y"));
				AddMetadataColumn(ColumnName, AttributeName, 2, TEXT("_Z"));
				AddMetadataColumn(ColumnName, AttributeName, 3, TEXT("_W"));
				break;
			}
		case EPCGMetadataTypes::Transform:
			{
				AddMetadataColumn(ColumnName, AttributeName, 0, TEXT("_tX"));
				AddMetadataColumn(ColumnName, AttributeName, 1, TEXT("_tY"));
				AddMetadataColumn(ColumnName, AttributeName, 2, TEXT("_tZ"));
				AddMetadataColumn(ColumnName, AttributeName, 3, TEXT("_rX"));
				AddMetadataColumn(ColumnName, AttributeName, 4, TEXT("_rY"));
				AddMetadataColumn(ColumnName, AttributeName, 5, TEXT("_rZ"));
				AddMetadataColumn(ColumnName, AttributeName, 6, TEXT("_sX"));
				AddMetadataColumn(ColumnName, AttributeName, 7, TEXT("_sY"));
				AddMetadataColumn(ColumnName, AttributeName, 8, TEXT("_sZ"));
				break;
			}
		default:
			break;
		}
	}
}

TSharedRef<ITableRow> SPCGEditorGraphAttributeListView::OnGenerateRow(PCGListviewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SPCGListViewItemRow, OwnerTable, Item);
}

void SPCGEditorGraphAttributeListView::AddColumn(const FName& InColumnID, const FText& ColumnLabel, float ColumnWidth, EHorizontalAlignment HeaderHAlign, EHorizontalAlignment CellHAlign)
{
	SHeaderRow::FColumn::FArguments Arguments;
	Arguments.ColumnId(InColumnID);
	Arguments.DefaultLabel(ColumnLabel);
	Arguments.ManualWidth(ColumnWidth);
	Arguments.HAlignHeader(HeaderHAlign);
	Arguments.HAlignCell(CellHAlign);
	ListViewHeader->AddColumn(Arguments);
}

void SPCGEditorGraphAttributeListView::RemoveColumn(const FName& InColumnID)
{
	ListViewHeader->RemoveColumn(InColumnID);
}

void SPCGEditorGraphAttributeListView::AddIndexColumn()
{
	AddColumn(PCGEditorGraphAttributeListView::NAME_IndexColumn, PCGEditorGraphAttributeListView::TEXT_IndexLabel, 44);
}

void SPCGEditorGraphAttributeListView::RemoveIndexColumn()
{
	RemoveColumn(PCGEditorGraphAttributeListView::NAME_IndexColumn);
}

void SPCGEditorGraphAttributeListView::AddPointDataColumns()
{
	AddIndexColumn();
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointPositionX, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelX, 94);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointPositionY, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelY, 94);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointPositionZ, PCGEditorGraphAttributeListView::TEXT_PointPositionLabelZ, 94);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointRotationX, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelX, 68);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointRotationY, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelY, 68);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointRotationZ, PCGEditorGraphAttributeListView::TEXT_PointRotationLabelZ, 68);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointScaleX, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelX, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointScaleY, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelY, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointScaleZ, PCGEditorGraphAttributeListView::TEXT_PointScaleLabelZ, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinX, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinX, 80);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinY, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinY, 80);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMinZ, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMinZ, 80);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxX, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxX, 88);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxY, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxY, 88);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointBoundsMaxZ, PCGEditorGraphAttributeListView::TEXT_PointBoundsLabelMaxZ, 88);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorR, PCGEditorGraphAttributeListView::TEXT_PointColorLabelR, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorG, PCGEditorGraphAttributeListView::TEXT_PointColorLabelG, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorB, PCGEditorGraphAttributeListView::TEXT_PointColorLabelB, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointColorA, PCGEditorGraphAttributeListView::TEXT_PointColorLabelA, 50);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointDensity, PCGEditorGraphAttributeListView::TEXT_PointDensityLabel, 54);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointSteepness, PCGEditorGraphAttributeListView::TEXT_PointSteepnessLabel, 73);
	AddColumn(PCGEditorGraphAttributeListView::NAME_PointSeed, PCGEditorGraphAttributeListView::TEXT_PointSeedLabel, 88);
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
}

void SPCGEditorGraphAttributeListView::AddMetadataColumn(const FName& InColumnId, const FName& InMetadataId, const int8 InValueIndex, const TCHAR* PostFix)
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

	SHeaderRow::FColumn::FArguments ColumnArguments;
	ColumnArguments.ColumnId(ColumnId);
	ColumnArguments.DefaultLabel(FText::FromName(ColumnId));
	ColumnArguments.HAlignHeader(EHorizontalAlignment::HAlign_Center);
	ColumnArguments.HAlignCell(EHorizontalAlignment::HAlign_Right);
	ColumnArguments.FillWidth(1.0f);
	ListViewHeader->AddColumn(ColumnArguments);
}

void SPCGEditorGraphAttributeListView::RemoveMetadataColumns()
{
	for (const TTuple<FName, FPCGMetadataInfo>& MetadataInfo : MetadataInfos)
	{
		ListViewHeader->RemoveColumn(MetadataInfo.Key);
	}
}

#undef LOCTEXT_NAMESPACE
