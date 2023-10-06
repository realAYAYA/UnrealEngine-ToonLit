// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"
#include "SmartObjectAssetEditor.generated.h"

class FBaseAssetToolkit;

UCLASS(Transient)
class USmartObjectAssetEditor : public UAssetEditor
{
	GENERATED_BODY()
public:
	void SetObjectToEdit(UObject* InObject);

protected:
	virtual void GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> ObjectToEdit;
};

