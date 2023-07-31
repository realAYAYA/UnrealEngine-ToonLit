// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeLightNode.generated.h"

// This enum is used as a placeholder for ELightUnits, because InterchangeWorker is not compiled against Engine, the LightFactoryNode is not affected
UENUM()
enum class EInterchangeLightUnits : uint8
{
	Unitless,
	Candelas,
	Lumens,
};

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeBaseLightNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName();

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool GetCustomLightColor(FLinearColor& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool SetCustomLightColor(const FLinearColor& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool GetCustomIntensity(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool SetCustomIntensity(float AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool GetCustomTemperature(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool SetCustomTemperature(float AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool GetCustomUseTemperature(bool & AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Light")
	bool SetCustomUseTemperature(bool AttributeValue);

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(LightColor)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(LightIntensity)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(Temperature)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(UseTemperature)
};

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeLightNode : public UInterchangeBaseLightNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LocalLight")
	bool GetCustomIntensityUnits(EInterchangeLightUnits& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LocalLight")
	bool SetCustomIntensityUnits(const EInterchangeLightUnits & AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LocalLight")
	bool GetCustomAttenuationRadius(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LocalLight")
	bool SetCustomAttenuationRadius(float AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LocalLight")
	bool GetCustomIESTexture(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LocalLight")
	bool SetCustomIESTexture(const FString& AttributeValue);

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(IntensityUnits)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(AttenuationRadius)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(IESTexture)
};

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangePointLightNode : public UInterchangeLightNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLight")
	bool GetCustomUseInverseSquaredFalloff(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLight")
	bool SetCustomUseInverseSquaredFalloff(bool AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLight")
	bool GetCustomLightFalloffExponent(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLight")
	bool SetCustomLightFalloffExponent(float AttributeValue);

private:
	IMPLEMENT_NODE_ATTRIBUTE_KEY(UseInverseSquaredFalloff)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(LightFalloffExponent)
};

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeSpotLightNode : public UInterchangePointLightNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLight")
	bool GetCustomInnerConeAngle(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLight")
	bool SetCustomInnerConeAngle(float AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLight")
	bool GetCustomOuterConeAngle(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLight")
	bool SetCustomOuterConeAngle(float AttributeValue);

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(InnerConeAngle)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(OuterConeAngle)
};

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeRectLightNode : public UInterchangeLightNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool GetCustomSourceWidth(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool SetCustomSourceWidth(float AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool GetCustomSourceHeight(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool SetCustomSourceHeight(float AttributeValue);

private:
	IMPLEMENT_NODE_ATTRIBUTE_KEY(SourceWidth)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(SourceHeight)
};

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeDirectionalLightNode : public UInterchangeBaseLightNode
{
	GENERATED_BODY()

public:

	virtual FString GetTypeName() const override;
};
