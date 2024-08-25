// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailPropertyExtensionHandler.h"

class IPropertyHandle;
class IDetailLayoutBuilder;
class IPropertyAccessEditor;

namespace UE::SmartObject::PropertyBinding
{
	extern const FName DataIDName;

} // UE::SmartObject::PropertyBinding

class FSmartObjectDefinitionBindingExtension : public IDetailPropertyExtensionHandler
{
public:
	// IDetailPropertyExtensionHandler interface
	virtual bool IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& PropertyHandle) const override;
	virtual void ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> PropertyHandle) override;
};
