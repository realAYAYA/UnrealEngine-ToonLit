// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class INiagaraDistributionAdapter;

class FNiagaraDistributionIntPropertyCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeIntInstance(UObject* OptionalOuter);
	static TSharedRef<IPropertyTypeCustomization> MakeIntInstance() { return MakeIntInstance(nullptr); }

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	FNiagaraDistributionIntPropertyCustomization(UObject* InOuterObject) : WeakOuterObject(InOuterObject) { }

private:
	TWeakObjectPtr<UObject> WeakOuterObject;
};
