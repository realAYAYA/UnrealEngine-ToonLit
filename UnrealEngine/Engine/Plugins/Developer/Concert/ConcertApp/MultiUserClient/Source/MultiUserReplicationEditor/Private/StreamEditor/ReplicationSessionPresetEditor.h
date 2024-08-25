// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"
#include "ReplicationSessionPresetEditor.generated.h"

/**
 * 
 */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UReplicationSessionPresetEditor : public UAssetEditor
{
	GENERATED_BODY()
public:

	void SetObjectToEdit(UObject* InObject);

protected:

	//~ Begin UAssetEditor Interface
	virtual void GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	//~ End UAssetEditor Interface

private:
	
	UPROPERTY(Transient)
	TObjectPtr<UObject> ObjectToEdit;
};
