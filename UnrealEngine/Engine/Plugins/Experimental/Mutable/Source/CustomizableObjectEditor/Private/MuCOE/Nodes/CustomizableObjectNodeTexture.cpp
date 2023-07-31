// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"

#include "Containers/EnumAsByte.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/Platform.h"
#include "ISinglePropertyView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "PropertyEditorModule.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

class UCustomizableObjectNodeRemapPins;
struct FGeometry;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeTexture::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* PinImagePin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
	PinImagePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeTexture::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Texture)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("TextureName"), FText::FromString(Texture->GetName()));

		return FText::Format(LOCTEXT("Texture_Title", "{TextureName}\nTexture"), Args);
	}
	else
	{
		return LOCTEXT("Texture", "Texture");
	}
}


FLinearColor UCustomizableObjectNodeTexture::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTexture::GetTooltipText() const
{
	return LOCTEXT("Texture_Tooltip", "Defines an image.");
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeTexture::CreateVisualWidget()
{
	return SNew(SGraphNodeTexture, this);
}


void SGraphNodeTexture::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	NodeTexture = Cast< UCustomizableObjectNodeTexture >(GraphNode);

	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FSinglePropertyParams SingleDetails;
	SingleDetails.NamePlacement = EPropertyNamePlacement::Hidden;
	SingleDetails.bHideAssetThumbnail = true;

	TextureSelector = PropPlugin.CreateSingleProperty(NodeTexture, "Texture", SingleDetails);

	TextureBrush.SetResourceObject(NodeTexture->Texture);
	TextureBrush.ImageSize.X = 128.0f;
	TextureBrush.ImageSize.Y = 128.0f;
	TextureBrush.DrawAs = ESlateBrushDrawType::Image;
	
	UpdateGraphNode();
}


void SGraphNodeTexture::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();
}


void SGraphNodeTexture::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	DefaultTitleAreaWidget->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(5))
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SGraphNodeTexture::OnExpressionPreviewChanged)
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


void SGraphNodeTexture::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	LeftNodeBox->AddSlot()
	.AutoHeight()
	[
		SNew(SVerticalBox)
		.Visibility(ExpressionPreviewVisibility())
		
		+ SVerticalBox::Slot()
		.Padding(5.0f,5.0f,0.0f,2.5f)
		.AutoHeight()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(SImage)
			.Image(&TextureBrush)
		]
	];

	MainBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		.Visibility(ExpressionPreviewVisibility())

		+ SHorizontalBox::Slot()
		.Padding(1.0f, 5.0f, 5.0f, 5.0f)
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		[
			TextureSelector.ToSharedRef()
		]
	];
}


void SGraphNodeTexture::OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState)
{
	NodeTexture->bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	UpdateGraphNode();
}


ECheckBoxState SGraphNodeTexture::IsExpressionPreviewChecked() const
{
	return NodeTexture->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}


const FSlateBrush* SGraphNodeTexture::GetExpressionPreviewArrow() const
{
	return FCustomizableObjectEditorStyle::Get().GetBrush(NodeTexture->bCollapsed ? TEXT("Nodes.ArrowDown") : TEXT("Nodes.ArrowUp"));
}


EVisibility SGraphNodeTexture::ExpressionPreviewVisibility() const
{
	return NodeTexture->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}


void SGraphNodeTexture::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UTexture2D* BrushTexture = Cast<UTexture2D>(TextureBrush.GetResourceObject());
	
	if (NodeTexture && BrushTexture)
	{
		if (NodeTexture->Texture != BrushTexture)
		{
			TextureBrush.SetResourceObject(NodeTexture->Texture);
		}
	}
}


#undef LOCTEXT_NAMESPACE
