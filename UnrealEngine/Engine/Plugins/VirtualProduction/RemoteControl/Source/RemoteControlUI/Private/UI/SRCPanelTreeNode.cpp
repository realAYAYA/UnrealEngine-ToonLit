// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCPanelTreeNode.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RemoteControlPanelNode"

TSet<FName> SRCPanelTreeNode::DefaultColumns = {
	RemoteControlPresetColumns::DragDropHandle,
	RemoteControlPresetColumns::Description,
	RemoteControlPresetColumns::Value,
	RemoteControlPresetColumns::Reset
};

TSharedRef<SWidget> SRCPanelTreeNode::GetProtocolWidget(const FName ForColumnName, const FName InProtocolName)
{
	return SNullWidget::NullWidget;
}

const bool SRCPanelTreeNode::HasProtocolExtension() const
{
	return false;
}

const bool SRCPanelTreeNode::GetProtocolBindingsNum() const
{
	return false;
}

const bool SRCPanelTreeNode::SupportsProtocol(const FName& InProtocolName) const
{
	return false;
}

TSharedRef<SWidget> SRCPanelTreeNode::GetWidget(const FName ForColumnName, const FName InActiveProtocol)
{
	if (ForColumnName == RemoteControlPresetColumns::DragDropHandle)
	{
		return DragHandleWidget.ToSharedRef();
	}
	else if (ForColumnName == RemoteControlPresetColumns::Description)
	{
		return NodeNameWidget.ToSharedRef();
	}
	else if (ForColumnName == RemoteControlPresetColumns::Reset)
	{
		return ResetValueWidget.ToSharedRef();
	}
	else if (ForColumnName == RemoteControlPresetColumns::Value)
	{
		return NodeValueWidget.ToSharedRef();
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SRCPanelTreeNode::MakeSplitRow(TSharedRef<SWidget> LeftColumn, TSharedRef<SWidget> RightColumn)
{
	TAttribute<float> LeftColumnAttribute;
	LeftColumnAttribute.Bind(this, &SRCPanelTreeNode::GetLeftColumnWidth);
	TAttribute<float> RightColumnAttribute;
	RightColumnAttribute.Bind(this, &SRCPanelTreeNode::GetRightColumnWidth);
	
	return SNew(SSplitter)
		.Style(FAppStyle::Get(), "DetailsView.Splitter")
		.PhysicalSplitterHandleSize(1.0f)
		.HitDetectionSplitterHandleSize(5.0f)
		+ SSplitter::Slot()
		.Value(MoveTemp(LeftColumnAttribute))
		.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SRCPanelTreeNode::OnLeftColumnResized))
		[
			LeftColumn
		]
		+ SSplitter::Slot()
		.Value(MoveTemp(RightColumnAttribute))
		.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SRCPanelTreeNode::SetColumnWidth))
		[
			RightColumn
		];
}

TSharedRef<SWidget> SRCPanelTreeNode::MakeNodeWidget(const FMakeNodeWidgetArgs& Args)
{
	MakeNodeWidgets(Args);

	TSharedRef<SWidget> LeftColumn = SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::OnDemand)
		// Drag and drop handle
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			DragHandleWidget.ToSharedRef()
		]
		// Field name
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			NodeNameWidget.ToSharedRef()
		];

	TSharedRef<SWidget> RightColumn = SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::OnDemand)
		// Node Value
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		[
			SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				NodeValueWidget.ToSharedRef()
			]
		]
		// Reset button
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			ResetValueWidget.ToSharedRef()
		];

	return MakeSplitRow(LeftColumn, RightColumn);
}

void SRCPanelTreeNode::MakeNodeWidgets(const FMakeNodeWidgetArgs& Args)
{
	auto WidgetOrNull = [](const TSharedPtr<SWidget>& Widget) {return Widget ? Widget.ToSharedRef() : SNullWidget::NullWidget; };

	DragHandleWidget = WidgetOrNull(Args.DragHandle);

	NodeNameWidget = WidgetOrNull(Args.NameWidget);

	NodeValueWidget = WidgetOrNull(Args.ValueWidget);

	ResetValueWidget = WidgetOrNull(Args.ResetButton);
}

void SRCPanelTreeNode::OnLeftColumnResized(float) const
{
	// This has to be bound or the splitter will take it upon itself to determine the size
	// We do nothing here because it is handled by the column size data
}

float SRCPanelTreeNode::GetLeftColumnWidth() const
{
	const float Offset = GetRCType() == ENodeType::Group ? SplitterOffset : 0;
	return FMath::Clamp(ColumnSizeData.LeftColumnWidth.Get() + Offset, 0.f, 1.f);
}

float SRCPanelTreeNode::GetRightColumnWidth() const
{
	const float Offset = GetRCType() == ENodeType::Group ? SplitterOffset : 0;
	return  FMath::Clamp(ColumnSizeData.RightColumnWidth.Get() - Offset, 0.f, 1.f);
}

void SRCPanelTreeNode::SetColumnWidth(float InWidth)
{
	const float Offset = GetRCType() == ENodeType::Group ? SplitterOffset : -SplitterOffset;
	ColumnSizeData.SetColumnWidth(FMath::Clamp(InWidth + SplitterOffset, 0.f, 1.f));
}

#undef LOCTEXT_NAMESPACE /*RemoteControlPanelNode*/