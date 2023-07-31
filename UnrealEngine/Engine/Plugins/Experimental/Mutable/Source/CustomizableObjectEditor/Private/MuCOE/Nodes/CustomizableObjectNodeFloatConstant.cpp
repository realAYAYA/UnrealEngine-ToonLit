// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeFloatConstant.h"

#include "EdGraph/EdGraphPin.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeFloatConstant::UCustomizableObjectNodeFloatConstant()
	: Super()
{
	Value = 1.0f;
	bCollapsed = true;
}


void UCustomizableObjectNodeFloatConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	//if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs") )
	{
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeFloatConstant::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName("Value"));
	ValuePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeFloatConstant::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Float_Constant", "Float Constant");
}


FLinearColor UCustomizableObjectNodeFloatConstant::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Float);
}


FText UCustomizableObjectNodeFloatConstant::GetTooltipText() const
{
	return LOCTEXT("Float_Constant_Tooltip", "Define a rational number that does not change at runtime.");
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeFloatConstant::CreateVisualWidget()
{
	return SNew(SGraphNodeFloatConstant, this);
}


// SGraphNode Float Constant

void SGraphNodeFloatConstant::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	NodeFloatConstant = Cast< UCustomizableObjectNodeFloatConstant >(InGraphNode);

	WidgetStyle = FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox");

	if (NodeFloatConstant)
	{
		UpdateGraphNode();
	}
}


void SGraphNodeFloatConstant::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();
}


void SGraphNodeFloatConstant::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	// Collapsing arrow of the title area
	DefaultTitleAreaWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	.Padding(FMargin(5))
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this, &SGraphNodeFloatConstant::OnExpressionPreviewChanged)
		.IsChecked(IsExpressionPreviewChecked())
		.Cursor(EMouseCursor::Default)
		.Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(GetExpressionPreviewArrow())
			]
		]
	];
}


void SGraphNodeFloatConstant::OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState)
{
	NodeFloatConstant->bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	UpdateGraphNode();
}


ECheckBoxState SGraphNodeFloatConstant::IsExpressionPreviewChecked() const
{
	return NodeFloatConstant->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}


const FSlateBrush* SGraphNodeFloatConstant::GetExpressionPreviewArrow() const
{
	return FCustomizableObjectEditorStyle::Get().GetBrush(NodeFloatConstant->bCollapsed ? TEXT("Nodes.ArrowDown") : TEXT("Nodes.ArrowUp"));
}


void SGraphNodeFloatConstant::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	MainBox->AddSlot()
	.AutoHeight()
	.Padding(10.0f, 0.0f, 10.0f, 10.0f)
	[
		SNew(SHorizontalBox)
		.Visibility(NodeFloatConstant->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible)
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FloatValueText", "Float Value:"))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		[
			SNew(SSpinBox<float>)
			.MinDesiredWidth(60.0f)
			.Value_Lambda([this]() {return NodeFloatConstant->Value; })
			.OnValueChanged(this,&SGraphNodeFloatConstant::OnSpinBoxValueChanged)
			.OnValueCommitted(this,&SGraphNodeFloatConstant::OnSpinBoxValueCommitted)
			.Style(&WidgetStyle)
			.Delta(0.001f)
		]
	];
}


void SGraphNodeFloatConstant::OnSpinBoxValueChanged(float Value)
{
	NodeFloatConstant->Value = Value;
	NodeFloatConstant->GetCustomizableObjectGraph()->GetOuter()->MarkPackageDirty();
}


void SGraphNodeFloatConstant::OnSpinBoxValueCommitted(float Value, ETextCommit::Type)
{
	NodeFloatConstant->Value = Value;
	NodeFloatConstant->GetCustomizableObjectGraph()->GetOuter()->MarkPackageDirty();
}

#undef LOCTEXT_NAMESPACE
