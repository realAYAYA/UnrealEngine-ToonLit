// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/SCustomizableObjectNodeMaterialPinImage.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Math/NumericLimits.h"
#include "Misc/Attribute.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Templates/Casts.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SToolTip.h"

class IToolTip;
class SWidget;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void SCustomizableObjectNodeMaterialPinImage::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);

	// Override previously defined tool tip.
	TSharedPtr<IToolTip> TooltipWidget = SNew(SToolTip)
		.Text(this, &SCustomizableObjectNodeMaterialPinImage::GetTooltipText);

	SetToolTip(TooltipWidget);

	// Cache pin icons.
	CachedPinMutableConnected = FAppStyle::GetBrush(TEXT("Graph.Pin.Connected"));
	CachedPinMutableDisconnected = FAppStyle::GetBrush(TEXT("Graph.Pin.Disconnected"));
	CachedPinPassthroughDisconnected = FAppStyle::GetBrush(TEXT("Graph.ExecPin.Disconnected"));
}


TSharedRef<SWidget>	SCustomizableObjectNodeMaterialPinImage::GetDefaultValueWidget()
{
	LabelAndValue->SetWrapSize(TNumericLimits<float>::Max()); // Remove warping.

	return SNew(SEditableTextBox)
		.Style(FAppStyle::Get(), "Graph.EditableTextBox")
		.Text(this, &SCustomizableObjectNodeMaterialPinImage::GetDefaultValueText)
		.SelectAllTextWhenFocused(false)
		.Visibility(this, &SCustomizableObjectNodeMaterialPinImage::GetDefaultValueVisibility)
		.IsReadOnly(true)
		.ForegroundColor(FSlateColor::UseForeground());
}


const FSlateBrush* SCustomizableObjectNodeMaterialPinImage::GetPinIcon() const
{
	if (CastChecked<UCustomizableObjectNodeMaterial>(GraphPinObj->GetOwningNode())->IsImageMutableMode(*GraphPinObj))
	{
		if (FollowInputPin(*GraphPinObj))
		{
			return CachedPinMutableConnected;
		}
		else
		{
			return CachedPinMutableDisconnected;
		}
	}
	else
	{
		return CachedPinPassthroughDisconnected;
	}
}


FText SCustomizableObjectNodeMaterialPinImage::GetTooltipText() const
{
	if (CastChecked<UCustomizableObjectNodeMaterial>(GraphPinObj->GetOwningNode())->IsImageMutableMode(*GraphPinObj))
	{
		return  LOCTEXT("PinModeMutableTooltip", "Texture Parameter goes through Mutable.");
	}
	else
	{
		return LOCTEXT("PinModePassthroughTooltip", "Texture Parameter is ignored by Mutable.");
	}
}


FText SCustomizableObjectNodeMaterialPinImage::GetDefaultValueText() const
{
	if (CastChecked<UCustomizableObjectNodeMaterial>(GraphPinObj->GetOwningNode())->IsImageMutableMode(*GraphPinObj))
	{
		return LOCTEXT("PinModeMutable", "mutable");
	}
	else 
	{
		return LOCTEXT("PinModePassthrough", "passthrough");
	}
}


EVisibility SCustomizableObjectNodeMaterialPinImage::GetDefaultValueVisibility() const
{
	return EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE