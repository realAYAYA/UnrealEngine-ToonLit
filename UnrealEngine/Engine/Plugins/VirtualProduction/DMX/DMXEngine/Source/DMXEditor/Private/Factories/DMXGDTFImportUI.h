// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DMXGDTFImportUI.generated.h"

UCLASS(config = Editor, HideCategories=Object, MinimalAPI)
class UDMXGDTFImportUI 
    : public UObject
{
	GENERATED_BODY()

public:
    UDMXGDTFImportUI();

	void ResetToDefault();

public:
    UPROPERTY(EditAnywhere, Category = "DMX")
    bool bUseSubDirectory;

    UPROPERTY(EditAnywhere, Category = "DMX")
    bool bImportXML;

    UPROPERTY(EditAnywhere, Category = "DMX")
    bool bImportTextures;

    UPROPERTY(EditAnywhere, Category = "DMX")
    bool bImportModels;
};


