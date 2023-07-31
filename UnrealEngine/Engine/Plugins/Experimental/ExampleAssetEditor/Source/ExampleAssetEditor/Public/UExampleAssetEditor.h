// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"

#include "UExampleAssetEditor.generated.h"
class FBaseAssetToolkit;

UCLASS(Transient)
class UExampleAssetEditor : public UAssetEditor
{
	GENERATED_BODY()

public:
	void GetObjectsToEdit(TArray<UObject*>& InObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
};
