// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameterDetails.h"

#include "DetailLayoutBuilder.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameter.h"
#include "PropertyHandle.h"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeGroupProjectorParameterDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeGroupProjectorParameterDetails);
}


void FCustomizableObjectNodeGroupProjectorParameterDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	Node = Cast<UCustomizableObjectNodeGroupProjectorParameter>(DetailBuilder->GetSelectedObjects()[0].Get());
	Node->PinConnectionListChangedDelegate.AddSP(this, &FCustomizableObjectNodeGroupProjectorParameterDetails::OnPinConnectionListChanged);

	LayoutBuilder = DetailBuilder;

	if (FollowInputPin(Node->GetImagePin()))
	{
		DetailBuilder->HideProperty(DetailBuilder->GetProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeGroupProjectorParameter, OptionImages)));
		DetailBuilder->HideProperty(DetailBuilder->GetProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeGroupProjectorParameter, OptionImagesDataTable)));
	}
}


void FCustomizableObjectNodeGroupProjectorParameterDetails::OnPinConnectionListChanged(UEdGraphPin* Pin)
{
	if (IDetailLayoutBuilder* Layout = LayoutBuilder.Pin().Get()) // Raw because we don't want to keep alive the details builder when calling the force refresh details
	{
		if (Pin == &Node->GetImagePin())
		{
			Layout->ForceRefreshDetails();
		}
	}
}
