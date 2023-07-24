// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphNode.h"

#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorStyle.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"

#include "SGraphPin.h"
#include "SLevelOfDetailBranchNode.h"
#include "SPinTypeSelector.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"

/** PCG pin primarily to give more control over pin coloring. */
class SPCGEditorGraphNodePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNodePin)
		: _PinLabelStyle(NAME_DefaultPinLabelStyle)
		, _UsePinColorForText(false)
		, _SideToSideMargin(5.0f)
		{}
		SLATE_ARGUMENT(FName, PinLabelStyle)
		SLATE_ARGUMENT(bool, UsePinColorForText)
		SLATE_ARGUMENT(float, SideToSideMargin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

	virtual FSlateColor GetPinColor() const override;
};

void SPCGEditorGraphNodePin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	// TODO: replace this with base class when we have sufficient controls to change the padding
	// IMPLEMENTATION NOTE: this is the code from SGraphPin::Construct
	// e.g. SGraphPin::Construct(SGraphPin::FArguments().SideToSideMargin(0.0f), InPin);
	// with additional padding exposed

	bUsePinColorForText = InArgs._UsePinColorForText;
	this->SetCursor(EMouseCursor::Default);

	SetVisibility(MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetPinVisiblity));

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	checkf(
		Schema, 
		TEXT("Missing schema for pin: %s with outer: %s of type %s"), 
		*(GraphPinObj->GetName()),
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetName()) : TEXT("NULL OUTER"), 
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetClass()->GetName()) : TEXT("NULL OUTER")
	);

	const bool bIsInput = (GetDirection() == EGPD_Input);

	// Create the pin icon widget
	TSharedRef<SWidget> PinWidgetRef = SPinTypeSelector::ConstructPinTypeImage(
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetPinIcon ),
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetPinColor),
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetSecondaryPinIcon),
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetSecondaryPinColor));
	PinImage = PinWidgetRef;

	PinWidgetRef->SetCursor( 
		TAttribute<TOptional<EMouseCursor::Type> >::Create (
			TAttribute<TOptional<EMouseCursor::Type> >::FGetter::CreateRaw( this, &SPCGEditorGraphNodePin::GetPinCursor )
		)
	);

	// Create the pin indicator widget (used for watched values)
	static const FName NAME_NoBorder("NoBorder");
	TSharedRef<SWidget> PinStatusIndicator =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.Visibility(this, &SPCGEditorGraphNodePin::GetPinStatusIconVisibility)
		.ContentPadding(0)
		.OnClicked(this, &SPCGEditorGraphNodePin::ClickedOnPinStatusIcon)
		[
			SNew(SImage)
			.Image(this, &SPCGEditorGraphNodePin::GetPinStatusIcon)
		];

	TSharedRef<SWidget> LabelWidget = GetLabelWidget(InArgs._PinLabelStyle);

	// Create the widget used for the pin body (status indicator, label, and value)
	LabelAndValue =
		SNew(SWrapBox)
		.PreferredSize(150.f);

	if (!bIsInput)
	{
		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				PinStatusIndicator
			];

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				LabelWidget
			];
	}
	else
	{
		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				LabelWidget
			];

		ValueWidget = GetDefaultValueWidget();

		if (ValueWidget != SNullWidget::NullWidget)
		{
			TSharedPtr<SBox> ValueBox;
			LabelAndValue->AddSlot()
				.Padding(bIsInput ? FMargin(InArgs._SideToSideMargin, 0, 0, 0) : FMargin(0, 0, InArgs._SideToSideMargin, 0))
				.VAlign(VAlign_Center)
				[
					SAssignNew(ValueBox, SBox)
					.Padding(0.0f)
					[
						ValueWidget.ToSharedRef()
					]
				];

			if (!DoesWidgetHandleSettingEditingEnabled())
			{
				ValueBox->SetEnabled(TAttribute<bool>(this, &SPCGEditorGraphNodePin::IsEditingEnabled));
			}
		}

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				PinStatusIndicator
			];
	}

	TSharedPtr<SHorizontalBox> PinContent;
	if (bIsInput)
	{
		// Input pin
		FullPinHorizontalRowWidget = PinContent = 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, InArgs._SideToSideMargin, 0)
			[
				PinWidgetRef
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				LabelAndValue.ToSharedRef()
			];
	}
	else
	{
		// Output pin
		FullPinHorizontalRowWidget = PinContent = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				LabelAndValue.ToSharedRef()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(InArgs._SideToSideMargin, 0, 0, 0)
			[
				PinWidgetRef
			];
	}

	// Set up a hover for pins that is tinted the color of the pin.
	
	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SPCGEditorGraphNodePin::GetPinBorder)
		.BorderBackgroundColor(this, &SPCGEditorGraphNodePin::GetHighlightColor)
		.OnMouseButtonDown(this, &SPCGEditorGraphNodePin::OnPinNameMouseDown)
		.Padding(0) // NOTE: This is different from base class implementation
		[
			SNew(SBorder)
			.BorderImage(CachedImg_Pin_DiffOutline)
			.BorderBackgroundColor(this, &SPCGEditorGraphNodePin::GetPinDiffColor)
			.Padding(0) // NOTE: This is different from base class implementation
			[
				SNew(SLevelOfDetailBranchNode)
				.UseLowDetailSlot(this, &SPCGEditorGraphNodePin::UseLowDetailPinNames)
				.LowDetail()
				[
					//@TODO: Try creating a pin-colored line replacement that doesn't measure text / call delegates but still renders
					PinWidgetRef
				]
				.HighDetail()
				[
					PinContent.ToSharedRef()
				]
			]
		]
	);

	TSharedPtr<IToolTip> TooltipWidget = SNew(SToolTip)
		.Text(this, &SPCGEditorGraphNodePin::GetTooltipText);

	SetToolTip(TooltipWidget);

}

// Adapted from SGraphPin::GetPinColor
FSlateColor SPCGEditorGraphNodePin::GetPinColor() const
{
	FSlateColor Color = SGraphPin::GetPinColor();

	UEdGraphPin* GraphPin = GetPinObj();
	if (GraphPin && !GraphPin->IsPendingKill())
	{
		const UPCGEditorGraphNodeBase* EditorNode = CastChecked<const UPCGEditorGraphNodeBase>(GraphPinObj->GetOwningNode());
		const UPCGNode* PCGNode = EditorNode ? EditorNode->GetPCGNode() : nullptr;
		const UPCGPin* PCGPin = PCGNode ? PCGNode->GetInputPin(GraphPin->GetFName()) : nullptr;

		// Desaturate if pin is unused - intended to happen whether disabled or not
		if (PCGPin && !PCGNode->IsPinUsedByNodeExecution(PCGPin))
		{
			Color = Color.GetSpecifiedColor().Desaturate(0.7f);
		}
	}

	return Color;
}

void SPCGEditorGraphNode::Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode)
{
	GraphNode = InNode;
	PCGEditorGraphNode = InNode;

	if (InNode)
	{
		InNode->OnNodeChangedDelegate.BindSP(this, &SPCGEditorGraphNode::OnNodeChanged);
	}

	UpdateGraphNode();
}

const FSlateBrush* SPCGEditorGraphNode::GetNodeBodyBrush() const
{
	if (PCGEditorGraphNode && PCGEditorGraphNode->GetPCGNode() && PCGEditorGraphNode->GetPCGNode()->IsInstance())
	{
		return FAppStyle::GetBrush("Graph.Node.TintedBody");
	}
	else
	{
		return FAppStyle::GetBrush("Graph.Node.Body");
	}
}

TSharedRef<SWidget> SPCGEditorGraphNode::CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle)
{
	// Reimplementation of the SGraphNode::CreateTitleWidget so we can control the style
	const bool bIsInstanceNode = (PCGEditorGraphNode && PCGEditorGraphNode->GetPCGNode() && PCGEditorGraphNode->GetPCGNode()->IsInstance());

	SAssignNew(InlineEditableText, SInlineEditableTextBlock)
		.Style(FPCGEditorStyle::Get(), bIsInstanceNode ? "PCG.Node.InstancedNodeTitleInlineEditableText" : "PCG.Node.NodeTitleInlineEditableText")
		.Text(InNodeTitle.Get(), &SNodeTitle::GetHeadTitle)
		.OnVerifyTextChanged(this, &SPCGEditorGraphNode::OnVerifyNameTextChanged)
		.OnTextCommitted(this, &SPCGEditorGraphNode::OnNameTextCommited)
		.IsReadOnly(this, &SPCGEditorGraphNode::IsNameReadOnly)
		.IsSelected(this, &SPCGEditorGraphNode::IsSelectedExclusively);
	InlineEditableText->SetColorAndOpacity(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateSP(this, &SPCGEditorGraphNode::GetNodeTitleTextColor)));

	return InlineEditableText.ToSharedRef();
}

TSharedPtr<SGraphPin> SPCGEditorGraphNode::CreatePinWidget(UEdGraphPin* Pin) const
{
	return SNew(SPCGEditorGraphNodePin, Pin);
}

void SPCGEditorGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	check(PCGEditorGraphNode);
	UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();

	if (PCGNode && PinToAdd->GetPinObj())
	{
		const bool bIsInPin = PinToAdd->GetPinObj()->Direction == EEdGraphPinDirection::EGPD_Input;
		const FName& PinName = PinToAdd->GetPinObj()->PinName;

		if(UPCGPin* Pin = (bIsInPin ? PCGNode->GetInputPin(PinName) : PCGNode->GetOutputPin(PinName)))
		{
			const bool bIsMultiData = Pin->Properties.bAllowMultipleData;
			const bool bIsMultiConnections = Pin->AllowMultipleConnections();

			// Check for special types
			if (Pin->Properties.AllowedTypes == EPCGDataType::Param)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Param_IN_C : PCGEditorStyleConstants::Pin_Param_OUT_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Param_IN_DC : PCGEditorStyleConstants::Pin_Param_OUT_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else if (Pin->Properties.AllowedTypes == EPCGDataType::Spatial)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Composite_IN_C : PCGEditorStyleConstants::Pin_Composite_OUT_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Composite_IN_DC : PCGEditorStyleConstants::Pin_Composite_OUT_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else
			{
				static const FName* PinBrushes[] =
				{
					&PCGEditorStyleConstants::Pin_SD_SC_IN_C,
					&PCGEditorStyleConstants::Pin_SD_SC_IN_DC,
					&PCGEditorStyleConstants::Pin_SD_MC_IN_C,
					&PCGEditorStyleConstants::Pin_SD_MC_IN_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_IN_C,
					&PCGEditorStyleConstants::Pin_MD_SC_IN_DC,
					&PCGEditorStyleConstants::Pin_MD_MC_IN_C,
					&PCGEditorStyleConstants::Pin_MD_MC_IN_DC,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_SD_MC_OUT_C,
					&PCGEditorStyleConstants::Pin_SD_MC_OUT_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_MD_MC_OUT_C,
					&PCGEditorStyleConstants::Pin_MD_MC_OUT_DC
				};

				const int32 ConnectedIndex = (bIsInPin ? 0 : 8) + (bIsMultiData ? 4 : 0) + (bIsMultiConnections ? 2 : 0);
				const int32 DisconnectedIndex = ConnectedIndex + 1;

				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(*PinBrushes[ConnectedIndex]);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(*PinBrushes[DisconnectedIndex]);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
		}
	}

	SGraphNode::AddPin(PinToAdd);
}

void SPCGEditorGraphNode::GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
{
	check(PCGEditorGraphNode);
	
	const FSlateBrush* DebugBrush = FPCGEditorStyle::Get().GetBrush(TEXT("PCG.NodeOverlay.Debug"));
	const FSlateBrush* InspectBrush = FPCGEditorStyle::Get().GetBrush(TEXT("PCG.NodeOverlay.Inspect"));
	
	const FVector2D HalfDebugBrushSize = DebugBrush->GetImageSize() / 2.0;
	const FVector2D HalfInspectBrushSize = InspectBrush->GetImageSize() / 2.0;
	
	FVector2D OverlayOffset(0.0, 0.0);

	if (const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode())
	{
		if (PCGNode->GetSettingsInterface() && PCGNode->GetSettingsInterface()->bDebug)
		{
			FOverlayBrushInfo BrushInfo;
			BrushInfo.Brush = DebugBrush;
			BrushInfo.OverlayOffset = OverlayOffset - HalfDebugBrushSize;
			Brushes.Add(BrushInfo);
			
			OverlayOffset.Y += HalfDebugBrushSize.Y + HalfInspectBrushSize.Y;
		}
	}

	if (PCGEditorGraphNode->GetInspected())
	{
		FOverlayBrushInfo BrushInfo;
		BrushInfo.Brush = InspectBrush;
		BrushInfo.OverlayOffset = OverlayOffset - HalfInspectBrushSize;
		Brushes.Add(BrushInfo);	
	}
}

void SPCGEditorGraphNode::OnNodeChanged()
{
	UpdateGraphNode();
}
