// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DataprepEditorMenu.generated.h"

UCLASS()
class DATAPREPEDITOR_API UDataprepEditorContextMenuContext : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TArray< TObjectPtr<UObject> > SelectedObjects;

	UPROPERTY()
	TObjectPtr<class UDataprepAssetInterface> DataprepAsset;
};
