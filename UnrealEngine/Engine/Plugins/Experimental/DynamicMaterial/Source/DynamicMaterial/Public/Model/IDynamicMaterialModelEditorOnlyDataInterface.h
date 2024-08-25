// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SharedPointer.h"

#if WITH_EDITOR
#include "Model/IDMMaterialBuildStateInterface.h"
#endif

#include "IDynamicMaterialModelEditorOnlyDataInterface.generated.h"

class UDMMaterialComponent;
class UDMMaterialValue;
class UDMTextureUV;
class UDynamicMaterialModel;
class UMaterial;
enum class EDMUpdateType : uint8;
struct FDMComponentPathSegment;
struct FDMComponentPath;

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UDynamicMaterialModelEditorOnlyDataInterface : public UInterface
{
	GENERATED_BODY()
};

class IDynamicMaterialModelEditorOnlyDataInterface
{
	GENERATED_BODY()

	friend class UDynamicMaterialModel;

public:
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void RequestMaterialBuild() {}

	virtual void PostEditorDuplicate() = 0;

	virtual void OnValueUpdated(UDMMaterialValue* InValue, EDMUpdateType InUpdateType) = 0;

	virtual void OnValueListUpdate() = 0;

	virtual void OnTextureUVUpdated(UDMTextureUV* InTextureUV) = 0;

	virtual void LoadDeprecatedModelData(UDynamicMaterialModel* InMaterialModel) = 0;

#if WITH_EDITOR
	virtual TSharedRef<IDMMaterialBuildStateInterface> CreateBuildStateInterface(UMaterial* InMaterialToBuild) const = 0;
#endif

	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const = 0;

protected:
	virtual void ReinitComponents() = 0;

	virtual void ResetData() = 0;
};