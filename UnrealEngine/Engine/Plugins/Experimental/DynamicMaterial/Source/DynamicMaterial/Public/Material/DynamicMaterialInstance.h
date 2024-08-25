// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialInstanceDynamic.h"
#include "DynamicMaterialInstance.generated.h"

class UDynamicMaterialModel;

#if WITH_EDITOR
class UDMMaterialStageInputTextureUV;
class UDMMaterialValue;
#endif

class UMaterial;

UCLASS(ClassGroup = "Material Designer", DefaultToInstanced, BlueprintType, meta = (DisplayThumbnail = "true"))
class DYNAMICMATERIAL_API UDynamicMaterialInstance : public UMaterialInstanceDynamic
{
	GENERATED_BODY()

public:
	UDynamicMaterialInstance();

	UDynamicMaterialModel* GetMaterialModel() { return MaterialModel; }

#if WITH_EDITOR
	void SetMaterialModel(UDynamicMaterialModel* InMaterialModel) { MaterialModel = InMaterialModel; }

	void OnMaterialBuilt(UDynamicMaterialModel* InMaterialModel);
	void InitializeMIDPublic();

	//~ Begin UObject
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	//~ End UObject
#endif

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> BaseMaterial;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDynamicMaterialModel> MaterialModel;
};
