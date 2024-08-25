// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaEditorExtension.h"

class UDynamicMaterialInstance;
class UDynamicMaterialModel;
struct FDMObjectMaterialProperty;

class FAvaAdvancedRenamerExtension : public FAvaEditorExtension
{
public:
	UE_AVA_INHERITS(FAvaAdvancedRenamerExtension, FAvaEditorExtension);

	//~ Begin IAvaEditorExtension
	virtual void BindCommands(const TSharedRef<FUICommandList>& InCommandList) override;
	//~ End IAvaEditorExtension

private:
	bool CanOpenAdvancedRenamerTool_SelectedActors() const;

	void OpenAdvancedRenamerTool_SelectedActors() const;

	bool CanOpenAdvancedRenamerTool_ClassActors() const;

	void OpenAdvancedRenamerTool_SharedClassActors() const;

	TArray<AActor*> GetSelectedActors() const;

	void OpenAdvancedRenamerTool(const TArray<AActor*>& InActors) const;
};
