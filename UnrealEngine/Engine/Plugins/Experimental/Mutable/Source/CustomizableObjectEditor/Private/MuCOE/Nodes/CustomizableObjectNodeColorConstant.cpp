// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeColorConstant.h"

#include "EdGraph/EdGraphPin.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/Platform.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Math/Vector2D.h"
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
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

class UCustomizableObjectNodeRemapPins;
struct FGeometry;
struct FPropertyChangedEvent;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeColorConstant::UCustomizableObjectNodeColorConstant()
	: Super()
{
	Value = FLinearColor(1,1,1,1);
	bCollapsed = true;
}


void UCustomizableObjectNodeColorConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	//if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs") )
	//{
	//}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeColorConstant::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName("Value"));
	ValuePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeColorConstant::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Color_Constant", "Color Constant");
}


FLinearColor UCustomizableObjectNodeColorConstant::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Color);
}


FText UCustomizableObjectNodeColorConstant::GetTooltipText() const
{
	return LOCTEXT("Color_Constant_Tooltip", "Define a constant color value.");
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeColorConstant::CreateVisualWidget()
{
	return SNew(SGraphNodeColorConstant, this);
}


// SGraph Node Color Constant 

void SGraphNodeColorConstant::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	NodeColorConstant = Cast< UCustomizableObjectNodeColorConstant >(InGraphNode);

	WidgetStyle = FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox");

	if (NodeColorConstant)
	{
		UpdateGraphNode();
	}
}

void SGraphNodeColorConstant::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();
}

void SGraphNodeColorConstant::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	// Collapsing arrow of the title area
	DefaultTitleAreaWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	.Padding(FMargin(5))
	[
		SNew(SCheckBox)
		.OnCheckStateChanged(this, &SGraphNodeColorConstant::OnExpressionPreviewChanged)
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


void SGraphNodeColorConstant::OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState)
{
	NodeColorConstant->bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	UpdateGraphNode();
}


ECheckBoxState SGraphNodeColorConstant::IsExpressionPreviewChecked() const
{
	return NodeColorConstant->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}


const FSlateBrush* SGraphNodeColorConstant::GetExpressionPreviewArrow() const
{
	return FCustomizableObjectEditorStyle::Get().GetBrush(NodeColorConstant->bCollapsed ? TEXT("Nodes.ArrowDown") : TEXT("Nodes.ArrowUp"));
}


void SGraphNodeColorConstant::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	LeftNodeBox->AddSlot()
	[
		SNew(SVerticalBox)
		.Visibility(NodeColorConstant->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f,0.0f,0.0f,5.0f)
		[
			SNew(SColorBlock)
			.Color_Lambda([&]() { return NodeColorConstant->Value; })
			.ShowBackgroundForAlpha(true)
			//.IgnoreAlpha(false)
			.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
			.Size(FVector2D(50.0f,50.0f))
			.OnMouseButtonDown(this, &SGraphNodeColorConstant::OnColorPreviewClicked)
		]
	];
	
	MainBox->AddSlot()
	[
		SAssignNew(ColorEditor,SVerticalBox)
		.Visibility(EVisibility::Collapsed)

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(NodeColorConstant->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RedText", "R:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSpinBox<float>)
				.MinDesiredWidth(100.0f)
				.Value_Lambda([this]() {return NodeColorConstant->Value.R; })
				.OnValueChanged(this, &SGraphNodeColorConstant::OnSpinBoxValueChanged,ColorChannel::RED)
				.OnValueCommitted(this, &SGraphNodeColorConstant::OnSpinBoxValueCommitted, ColorChannel::RED)
				.Style(&WidgetStyle)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Delta(0.01f)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f, 10.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(NodeColorConstant->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GreenText", "G:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSpinBox<float>)
				.MinDesiredWidth(100.0f)
				.Value_Lambda([this]() {return NodeColorConstant->Value.G; })
				.OnValueChanged(this, &SGraphNodeColorConstant::OnSpinBoxValueChanged, ColorChannel::GREEN)
				.OnValueCommitted(this, &SGraphNodeColorConstant::OnSpinBoxValueCommitted, ColorChannel::GREEN)
				.Style(&WidgetStyle)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Delta(0.01f)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f, 10.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(NodeColorConstant->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BlueText", "B:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSpinBox<float>)
				.MinDesiredWidth(100.0f)
				.Value_Lambda([this]() {return NodeColorConstant->Value.B; })
				.OnValueChanged(this, &SGraphNodeColorConstant::OnSpinBoxValueChanged, ColorChannel::BLUE)
				.OnValueCommitted(this, &SGraphNodeColorConstant::OnSpinBoxValueCommitted, ColorChannel::BLUE)
				.Style(&WidgetStyle)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Delta(0.01f)
			]
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.0f, 5.0f, 10.0f, 10.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(NodeColorConstant->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AlphaText", "A:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSpinBox<float>)
				.MinDesiredWidth(100.0f)
				.Value_Lambda([this]() {return NodeColorConstant->Value.A; })
				.OnValueChanged(this, &SGraphNodeColorConstant::OnSpinBoxValueChanged, ColorChannel::ALPHA)
				.OnValueCommitted(this, &SGraphNodeColorConstant::OnSpinBoxValueCommitted, ColorChannel::ALPHA)
				.Style(&WidgetStyle)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Delta(0.01f)
			]
		]
	];
}


void SGraphNodeColorConstant::OnSpinBoxValueChanged(float Value, ColorChannel channel)
{
	switch (channel)
	{
	case ColorChannel::RED:
		NodeColorConstant->Value.R = Value;
		break;
	case ColorChannel::GREEN:
		NodeColorConstant->Value.G = Value;
		break;
	case ColorChannel::BLUE:
		NodeColorConstant->Value.B = Value;
		break;
	case ColorChannel::ALPHA:
		NodeColorConstant->Value.A = Value;
		break;
	default:
		break;
	}

	NodeColorConstant->GetCustomizableObjectGraph()->GetOuter()->MarkPackageDirty();
}


void SGraphNodeColorConstant::OnSpinBoxValueCommitted(float Value, ETextCommit::Type, ColorChannel channel)
{
	switch (channel)
	{
	case ColorChannel::RED:
		NodeColorConstant->Value.R = Value;
		break;
	case ColorChannel::GREEN:
		NodeColorConstant->Value.G = Value;
		break;
	case ColorChannel::BLUE:
		NodeColorConstant->Value.B = Value;
		break;
	case ColorChannel::ALPHA:
		NodeColorConstant->Value.A = Value;
		break;
	default:
		break;
	}

	NodeColorConstant->GetCustomizableObjectGraph()->GetOuter()->MarkPackageDirty();
}


FReply SGraphNodeColorConstant::OnColorPreviewClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (ColorEditor->GetVisibility() == EVisibility::Collapsed)
		{
			ColorEditor->SetVisibility(EVisibility::Visible);
		}
		else
		{
			ColorEditor->SetVisibility(EVisibility::Collapsed);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
