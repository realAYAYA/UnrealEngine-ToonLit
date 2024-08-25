// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaEditorExtension.h"

class UDynamicMaterialInstance;
class UDynamicMaterialModel;
struct FDMObjectMaterialProperty;

class FAvaMaterialDesignerExtension : public FAvaEditorExtension
{
public:
	UE_AVA_INHERITS(FAvaMaterialDesignerExtension, FAvaEditorExtension);

	void OpenEditor();

	//~ Begin IAvaEditorExtension
	virtual void Activate() override;
	virtual void ExtendToolbarMenu(UToolMenu& InMenu) override;
	//~ End IAvaEditorExtension

private:
	bool IsDynamicMaterialModelValid(UDynamicMaterialModel* InMaterialModel);

	bool SetDynamicMaterialValue(const FDMObjectMaterialProperty& InObjectMaterialProperty, UDynamicMaterialInstance* InMaterial);

	void InitWorldSubsystem();
};
