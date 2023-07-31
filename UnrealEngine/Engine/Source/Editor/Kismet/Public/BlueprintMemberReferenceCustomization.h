// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Templates/SharedPointer.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyHandle;
class SMyBlueprint;

// Property type customization for FMemberReference
class KISMET_API FBlueprintMemberReferenceDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FBlueprintMemberReferenceDetails(TWeakPtr<SMyBlueprint>()));
	}
	
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(TWeakPtr<SMyBlueprint> InMyBlueprint)
	{
		return MakeShareable(new FBlueprintMemberReferenceDetails(InMyBlueprint));
	}

private:
	FBlueprintMemberReferenceDetails(TWeakPtr<SMyBlueprint> InMyBlueprint)
		: MyBlueprint(InMyBlueprint)
	{
	}
	
	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override {};

private:
	TWeakPtr<SMyBlueprint> MyBlueprint;
};
