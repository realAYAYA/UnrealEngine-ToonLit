// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

/** Custom property layout for the ChaosVD SQ Data wrapper struct */
class FChaosVDQueryDataWrappersCustomizationDetails : public IPropertyTypeCustomization
{
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
};

/** Custom details panel for the ChaosVD SQ Visit Data struct */
class FChaosVDQueryVisitDataCustomization : public IDetailCustomization
{
public:
	FChaosVDQueryVisitDataCustomization(){};
	virtual ~FChaosVDQueryVisitDataCustomization() override {};

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

/** Custom details panel for the ChaosVD SQ Data Wrapper struct */
class FChaosVDQueryDataWrapperCustomization : public IDetailCustomization
{
public:
	FChaosVDQueryDataWrapperCustomization(){};
	virtual ~FChaosVDQueryDataWrapperCustomization() override{};

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
