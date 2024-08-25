// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaAttributeEditorModule.h"

class FAvaAttributeEditorModule : public IAvaAttributeEditorModule
{
public:
	//~ Begin IAvaAttributeEditorModule
	virtual void CustomizeAttributes(const TSharedRef<IPropertyHandle>& InAttributesHandle, IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IAvaAttributeEditorModule
};
