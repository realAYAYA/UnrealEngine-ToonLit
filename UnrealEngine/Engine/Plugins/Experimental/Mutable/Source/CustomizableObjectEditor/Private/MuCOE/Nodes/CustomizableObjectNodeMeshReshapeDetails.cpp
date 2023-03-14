// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeDetails.h"

#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;

#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeMeshReshapeDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeMeshReshapeDetails);
}


void FCustomizableObjectNodeMeshReshapeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}


void FCustomizableObjectNodeMeshReshapeDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
}


#undef LOCTEXT_NAMESPACE
