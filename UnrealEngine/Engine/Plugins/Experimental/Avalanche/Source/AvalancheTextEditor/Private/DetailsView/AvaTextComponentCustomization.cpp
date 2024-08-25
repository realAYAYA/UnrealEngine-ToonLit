// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextComponentCustomization.h"

#include "AvaText3DComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "AvaTextComponentCustomization"

void FAvaTextComponentCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// UAvaText3DComponent adds 
	//
	// - FAvaFont property for fonts (additional functionalities for font selection)
	// - FAvaTextAlignment Alignment for both Horizontal and Vertical alignment (custom widget for alignment)
	//
	// Therefore, to avoid showing duplicate properties with the same meaning, we hide the base class ones, as follows:
	
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, Font), UText3DComponent::StaticClass());
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, HorizontalAlignment), UText3DComponent::StaticClass());
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAvaText3DComponent, VerticalAlignment), UText3DComponent::StaticClass());
}

#undef LOCTEXT_NAMESPACE
