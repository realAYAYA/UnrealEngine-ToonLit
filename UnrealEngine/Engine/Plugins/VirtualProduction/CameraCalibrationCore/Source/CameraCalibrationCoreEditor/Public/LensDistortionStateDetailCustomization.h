// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

#include "Models/LensModel.h"
#include "Templates/SubclassOf.h"

class CAMERACALIBRATIONCOREEDITOR_API FLensDistortionStateDetailCustomization : public IPropertyTypeCustomization
{
public:
	FLensDistortionStateDetailCustomization(TSubclassOf<ULensModel> InLensModel);

	// Begin IDetailCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IDetailCustomization interface

private:
	/** Customize the FDistortionInfo struct property */
	void CustomizeDistortionInfo(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);

	/** Customize the FFocalLengthInfo struct property */
	void CustomizeFocalLengthInfo(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);

	/** Customize the FImageCenterInfo struct property */
	void CustomizeImageCenterInfo(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);

	/** The LensModel used to customize the names of the properties in the distortion parameter array */
	TSubclassOf<ULensModel> LensModel;
};
