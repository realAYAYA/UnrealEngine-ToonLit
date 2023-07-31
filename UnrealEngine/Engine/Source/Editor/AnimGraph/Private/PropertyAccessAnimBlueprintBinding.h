// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyAccessBlueprintBinding.h"
#include "Templates/SharedPointer.h"

class FExtender;

class FPropertyAccessAnimBlueprintBinding : public IPropertyAccessBlueprintBinding
{
public:
	// IPropertyAccessBlueprintBinding interface
	virtual bool CanBindToContext(const FContext& InContext) const override;
	virtual TSharedPtr<FExtender> MakeBindingMenuExtender(const FContext& InContext, const FBindingMenuArgs& InArgs) const override;
};
