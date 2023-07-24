// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfiguratorExternalImageTypeCustomization.h"

#include "DisplayClusterConfigurationTypes.h"
#include "Views/OutputMapping/Widgets/SDisplayClusterConfiguratorExternalImagePicker.h"

#include "PropertyHandle.h"
#include "DetailWidgetRow.h"

void FDisplayClusterConfiguratorExternalImageTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ImagePathHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDisplayClusterConfigurationExternalImage, ImagePath));
	check(ImagePathHandle->IsValidHandle());

	FString ImagePath;
	ImagePathHandle->GetValue(ImagePath);

	TArray<FString> ImageExtensions = {
		"png",
		"jpeg",
		"jpg",
		"bmp",
		"ico",
		"icns",
		"exr"
	};

	// Create header row
	InHeaderRow.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SDisplayClusterConfiguratorExternalImagePicker)
		.ImagePath(ImagePath)
		.Extensions(ImageExtensions)
		.OnImagePathPicked_Lambda([=](const FString& NewImagePath) { ImagePathHandle->SetValue(NewImagePath); })
	];
}