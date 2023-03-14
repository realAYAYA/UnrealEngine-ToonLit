// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json/GLTFJsonEnums.h"
#include "MaterialTypes.h"
#include "UObject/ObjectMacros.h"

class UMaterial;
class UMaterialInterface;
class UMaterialInstanceConstant;
class UTexture;

class GLTFEXPORTER_API FGLTFProxyMaterialUtilities
{
public:

#if WITH_EDITOR
	static UMaterialInstanceConstant* CreateProxyMaterial(EGLTFJsonShadingModel ShadingModel, UObject* Outer = GetTransientPackage(), FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags);
#endif

	static bool IsProxyMaterial(const UMaterial* Material);
	static bool IsProxyMaterial(const UMaterialInterface* Material);

	static UMaterial* GetBaseMaterial(EGLTFJsonShadingModel ShadingModel);

	static UMaterialInterface* GetProxyMaterial(UMaterialInterface* OriginalMaterial);
	static void SetProxyMaterial(UMaterialInterface* OriginalMaterial, UMaterialInterface* ProxyMaterial);

	static bool GetParameterValue(const UMaterialInterface* Material, const FHashedMaterialParameterInfo& ParameterInfo, float& OutValue, bool NonDefaultOnly = false);
	static bool GetParameterValue(const UMaterialInterface* Material, const FHashedMaterialParameterInfo& ParameterInfo, FLinearColor& OutValue, bool NonDefaultOnly = false);
	static bool GetParameterValue(const UMaterialInterface* Material, const FHashedMaterialParameterInfo& ParameterInfo, UTexture*& OutValue, bool NonDefaultOnly = false);

#if WITH_EDITOR
	static void SetParameterValue(UMaterialInstanceConstant* Material, const FHashedMaterialParameterInfo& ParameterInfo, float Value, bool NonDefaultOnly = false);
	static void SetParameterValue(UMaterialInstanceConstant* Material, const FHashedMaterialParameterInfo& ParameterInfo, const FLinearColor& Value, bool NonDefaultOnly = false);
	static void SetParameterValue(UMaterialInstanceConstant* Material, const FHashedMaterialParameterInfo& ParameterInfo, UTexture* Value, bool NonDefaultOnly = false);
#endif

	static bool GetTwoSided(const UMaterialInterface* Material, bool& OutValue, bool NonDefaultOnly = false);
	static bool GetBlendMode(const UMaterialInterface* Material, EBlendMode& OutValue, bool NonDefaultOnly = false);
	static bool GetOpacityMaskClipValue(const UMaterialInterface* Material, float& OutValue, bool NonDefaultOnly = false);

#if WITH_EDITOR
	static void SetTwoSided(UMaterialInstanceConstant* Material, bool Value, bool NonDefaultOnly = false);
	static void SetBlendMode(UMaterialInstanceConstant* Material, EBlendMode Value, bool NonDefaultOnly = false);
	static void SetOpacityMaskClipValue(UMaterialInstanceConstant* Material, float Value, bool NonDefaultOnly = false);
#endif
};
