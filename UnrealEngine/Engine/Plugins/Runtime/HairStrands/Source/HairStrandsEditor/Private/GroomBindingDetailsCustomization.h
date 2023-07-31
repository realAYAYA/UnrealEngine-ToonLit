// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FGroomBindingDetailsCustomization : public IDetailCustomization
{
public:
	/** Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder) override;
	/** End IDetailCustomization interface */

	static TSharedRef<IDetailCustomization> MakeInstance();
};

class FGroomCreateBindingDetailsCustomization : public IDetailCustomization
{
public:
	/** Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder) override;
	/** End IDetailCustomization interface */

	static TSharedRef<IDetailCustomization> MakeInstance();
protected:
	void OnGroomBindingTypeChanged(IDetailLayoutBuilder* LayoutBuilder);
};
