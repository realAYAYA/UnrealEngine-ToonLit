// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMDefs.h"
#include "DMEDefs.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DMBlueprintFunctionLibrary.generated.h"

class AActor;
class UDMMaterialStage;
class UDMMaterialStageInputValue;
class UDynamicMaterialModel;
class UTexture;
struct FDMObjectMaterialProperty;

/**
 * Material Designer Blueprint Function Library
 */
UCLASS()
class UDMBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UDMMaterialStageInputValue* FindDefaultStageOpacityInputValue(UDMMaterialStage* InStage);

	static void SetDefaultStageSourceTexture(UDMMaterialStage* InStage, UTexture* InTexture);

	static TArray<FDMObjectMaterialProperty> GetActorMaterialProperties(AActor* InActor);

	static UDynamicMaterialModel* CreateDynamicMaterialInObject(FDMObjectMaterialProperty& InMaterialProperty);

	static bool ExportMaterialInstance(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath);

	static bool ExportGeneratedMaterial(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath);
};
