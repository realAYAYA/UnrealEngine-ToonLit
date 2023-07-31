// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeShaderGraphNode.h"

#if WITH_ENGINE
#include "Materials/MaterialInterface.h"
#endif

#include "InterchangeMaterialFactoryNode.generated.h"

UCLASS(Abstract)
class INTERCHANGEFACTORYNODES_API UInterchangeBaseMaterialFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	static FString GetMaterialFactoryNodeUidFromMaterialNodeUid(const FString& TranslatedNodeUid);
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeMaterialFactoryNode : public UInterchangeBaseMaterialFactoryNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	virtual class UClass* GetObjectClass() const override;

// Material Inputs
public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetBaseColorConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToBaseColor(const FString& AttributeValue);
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToBaseColor(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetMetallicConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToMetallic(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToMetallic(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetSpecularConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToSpecular(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToSpecular(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetRoughnessConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToRoughness(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToRoughness(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetAnisotropyConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToAnisotropy(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToAnisotropy(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetEmissiveColorConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToEmissiveColor(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToEmissiveColor(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetNormalConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToNormal(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToNormal(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetTangentConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToTangent(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToTangent(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetSubsurfaceConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToSubsurface(const FString& ExpressionNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToSubsurface(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetOpacityConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToOpacity(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToOpacity(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetOcclusionConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToOcclusion(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToOcclusion(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetRefractionConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToRefraction(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToRefraction(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetClearCoatConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToClearCoat(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToClearCoat(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetClearCoatRoughnessConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToClearCoatRoughness(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToClearCoatRoughness(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetClearCoatNormalConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToClearCoatNormal(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToClearCoatNormal(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetTransmissionColorConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToTransmissionColor(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToTransmissionColor(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetFuzzColorConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToFuzzColor(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToFuzzColor(const FString& ExpressionNodeUid, const FString& OutputName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetClothConnection(FString& ExpressionNodeUid, FString& OutputName) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectToCloth(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool ConnectOutputToCloth(const FString& ExpressionNodeUid, const FString& OutputName);

// Material parameters
public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomShadingModel(TEnumAsByte<EMaterialShadingModel>& AttributeValue) const;
 
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomShadingModel(const TEnumAsByte<EMaterialShadingModel>& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomTranslucencyLightingMode(TEnumAsByte<ETranslucencyLightingMode>& AttributeValue) const;
 
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomTranslucencyLightingMode(const TEnumAsByte<ETranslucencyLightingMode>& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomBlendMode(TEnumAsByte<EBlendMode>& AttributeValue) const;
 
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomBlendMode(const TEnumAsByte<EBlendMode>& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomTwoSided(bool& AttributeValue) const;
 
	/** Sets if this shader graph should be rendered two sided or not. Defaults to off. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomTwoSided(const bool& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomOpacityMaskClipValue(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomOpacityMaskClipValue(const float& AttributeValue, bool bAddApplyDelegate = true);

private:
	const UE::Interchange::FAttributeKey Macro_CustomShadingModelKey = UE::Interchange::FAttributeKey(TEXT("ShadingModel"));
	const UE::Interchange::FAttributeKey Macro_CustomTranslucencyLightingModeKey = UE::Interchange::FAttributeKey(TEXT("TranslucencyLightingMode"));
	const UE::Interchange::FAttributeKey Macro_CustomBlendModeKey = UE::Interchange::FAttributeKey(TEXT("BlendMode"));
	const UE::Interchange::FAttributeKey Macro_CustomTwoSidedKey = UE::Interchange::FAttributeKey(TEXT("TwoSided"));
	const UE::Interchange::FAttributeKey Macro_CustomOpacityMaskClipValueKey = UE::Interchange::FAttributeKey(TEXT("OpacityMaskClipValue"));
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeMaterialExpressionFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	virtual FString GetTypeName() const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomExpressionClassName(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomExpressionClassName(const FString& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomExpressionClassNameKey = UE::Interchange::FAttributeKey(TEXT("ExpressionClassName"));

};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeMaterialInstanceFactoryNode : public UInterchangeBaseMaterialFactoryNode
{
	GENERATED_BODY()

public:
	virtual FString GetTypeName() const override;
	virtual UClass* GetObjectClass() const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialInstanceFactory")
	bool GetCustomInstanceClassName(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialInstanceFactory")
	bool SetCustomInstanceClassName(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialInstanceFactory")
	bool GetCustomParent(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialInstanceFactory")
	bool SetCustomParent(const FString& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomInstanceClassNameKey = UE::Interchange::FAttributeKey(TEXT("InstanceClassName"));
	const UE::Interchange::FAttributeKey Macro_CustomParentKey = UE::Interchange::FAttributeKey(TEXT("Parent"));
	
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeMaterialFunctionCallExpressionFactoryNode : public UInterchangeMaterialExpressionFactoryNode
{
	GENERATED_BODY()

public:
	virtual FString GetTypeName() const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomMaterialFunctionDependency(FString& AttributeValue) const;

	/**
	 * Set the unique ID of the material function that the function call expression
	 * is referring to.
	 * Note that a call to AddFactoryDependencyUid is made to guarantee that
	 * the material function is created before the function call expression
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomMaterialFunctionDependency(const FString& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomMaterialFunctionDependencyKey = UE::Interchange::FAttributeKey(TEXT("MaterialFunctionDependency"));

};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeMaterialFunctionFactoryNode : public UInterchangeBaseMaterialFactoryNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	virtual class UClass* GetObjectClass() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | MaterialFactory")
	bool GetInputConnection(const FString& InputName, FString& ExpressionNodeUid, FString& OutputName) const;

private:
};
