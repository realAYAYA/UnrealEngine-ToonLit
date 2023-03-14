// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;

/** Copy Material node details panel. Hides all properties from the inheret Material node. */
class FCustomizableObjectNodeCopyMaterialDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** Hides details copied from CustomizableObjectNodeMaterial. */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};