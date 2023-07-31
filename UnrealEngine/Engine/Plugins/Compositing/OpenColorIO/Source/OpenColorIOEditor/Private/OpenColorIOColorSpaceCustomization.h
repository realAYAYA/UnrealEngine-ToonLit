// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIO/OpenColorIO.h"
#include "Widgets/SWidget.h"


/**
 * Implements a details view customization for the FOpenColorIOConfiguration
 */
class IPropertyTypeCustomizationOpenColorIO : public IPropertyTypeCustomization
{
public:

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override {}


protected:
	TSharedPtr<IPropertyHandle> GetConfigurationFileProperty() const;
	bool LoadConfigurationFile(const FFilePath& InFilePath);
	bool CheckValidConfiguration();

protected:

	/** Pointer to the property handle. */
	TSharedPtr<IPropertyHandle> CachedProperty;

	/** Pointer to the ConfigurationFile property handle. */
	TSharedPtr<IPropertyHandle> ConfigurationFileProperty;

	/** FilePath of the configuration file that was cached */
	FFilePath LoadedFilePath;

	/** Cached configuration file to populate menus and submenus */
	OCIO_NAMESPACE::ConstConfigRcPtr CachedConfigFile;
};


class FOpenColorIOColorSpaceCustomization : public IPropertyTypeCustomizationOpenColorIO
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FOpenColorIOColorSpaceCustomization);
	}

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

private:
	void ProcessColorSpaceForMenuGeneration(FMenuBuilder& InMenuBuilder, const int32 InMenuDepth, const FString& InPreviousFamilyHierarchy, const FOpenColorIOColorSpace& InColorSpace, TArray<FString>& InOutExistingMenuFilter);
	void PopulateSubMenu(FMenuBuilder& InMenuBuilder, const int32 InMenuDepth, FString InPreviousFamilyHierarchy);
	void AddMenuEntry(FMenuBuilder& InMenuBuilder, const FOpenColorIOColorSpace& InColorSpace);

	TSharedRef<SWidget> HandleSourceComboButtonMenuContent();
};

/**
 * Implements a details view customization for the FOpenColorIOConfiguration
 */
class FOpenColorIODisplayViewCustomization : public IPropertyTypeCustomizationOpenColorIO
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FOpenColorIODisplayViewCustomization);
	}

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& PropertyTypeCustomizationUtils) override;

private:
	void PopulateViewSubMenu(FMenuBuilder& InMenuBuilder, FOpenColorIODisplayView InDisplayView);
	void AddMenuEntry(FMenuBuilder& InMenuBuilder, const FOpenColorIODisplayView& InDisplayView);

	TSharedRef<SWidget> HandleSourceComboButtonMenuContent();
};

