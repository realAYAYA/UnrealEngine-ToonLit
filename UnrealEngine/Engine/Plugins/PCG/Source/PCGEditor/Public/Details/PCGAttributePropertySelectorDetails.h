// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Layout/Visibility.h"

enum class EPCGPointProperties : uint8;
enum class EPCGExtraProperties : uint8;
namespace ETextCommit { enum Type : int; }

struct FPCGAttributePropertySelector;
class SWidget;

class FPCGAttributePropertySelectorDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGAttributePropertySelectorDetails);
	}

	/** ~Begin IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {};
	/** ~End IPropertyTypeCustomization interface */

protected:
	FPCGAttributePropertySelector* GetStruct();
	const FPCGAttributePropertySelector* GetStruct() const;

	TSharedRef<SWidget> GenerateExtraMenu();
	EVisibility ExtraMenuVisibility() const;
	bool IsEnabled() const;

	FText GetText() const;
	void SetText(const FText& NewText, ETextCommit::Type CommitInfo);
	void SetPointProperty(EPCGPointProperties EnumValue);
	void SetAttributeName(FName NewName);
	void SetExtraProperty(EPCGExtraProperties EnumValue);

	TSharedPtr<IPropertyHandle> PropertyHandle;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGPoint.h"
#endif
