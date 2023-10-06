// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UAssetEditor.h"

#include "MoviePipelineAssetEditor.generated.h"


UCLASS()
class UMoviePipelineAssetEditor : public UAssetEditor
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	virtual void GetObjectsToEdit(TArray<UObject*>& InObjectsToEdit) override;
	
	void SetObjectToEdit(UObject* InObject);
private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> ObjectToEdit;
};