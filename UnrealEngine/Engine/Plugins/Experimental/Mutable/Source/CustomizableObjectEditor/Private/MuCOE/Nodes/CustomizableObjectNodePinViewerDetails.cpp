// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePinViewerDetails.h"

#include "MuCOE/PinViewer/SPinViewer.h"

class IDetailLayoutBuilder;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


TSharedRef<IDetailCustomization> FCustomizableObjectNodePinViewerDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodePinViewerDetails);
}


void FCustomizableObjectNodePinViewerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PinViewerAttachToDetailCustomization(DetailBuilder);
}

#undef LOCTEXT_NAMESPACE
