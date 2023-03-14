// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeActorFactoryNode.h"

#include "InterchangeLightFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeBaseLightFactoryNode : public UInterchangeActorFactoryNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomLightColor(FColor& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomLightColor(const FColor& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomIntensity(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomIntensity(float AttributeValue, bool bAddApplyDelegate = true);
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomTemperature(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomTemperature(float AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomUseTemperature(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomUseTemperature(bool AttributeValue, bool bAddApplyDelegate = true);

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(LightColor)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(Intensity)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(Temperature)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(bUseTemperature)
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeDirectionalLightFactoryNode : public UInterchangeBaseLightFactoryNode
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeLightFactoryNode : public UInterchangeBaseLightFactoryNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomIntensityUnits(ELightUnits& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomIntensityUnits(ELightUnits AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomAttenuationRadius(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomAttenuationRadius(float AttributeValue, bool bAddApplyDelegate = true);
	
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool GetCustomIESTexture(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | LightFactory")
	bool SetCustomIESTexture(const FString& AttributeValue);

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(IntensityUnits)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(AttenuationRadius)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(IESTexture)
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeRectLightFactoryNode : public UInterchangeLightFactoryNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool GetCustomSourceWidth(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool SetCustomSourceWidth(float AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool GetCustomSourceHeight(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | RectLightFactory")
	bool SetCustomSourceHeight(float AttributeValue, bool bAddApplyDelegate = true);

private:
	IMPLEMENT_NODE_ATTRIBUTE_KEY(SourceWidth)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(SourceHeight)
};


UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangePointLightFactoryNode : public UInterchangeLightFactoryNode
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLightFactory")
	bool GetCustomUseInverseSquaredFalloff(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLightFactory")
	bool SetCustomUseInverseSquaredFalloff(bool AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLightFactory")
	bool GetCustomLightFalloffExponent(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | PointLightFactory")
	bool SetCustomLightFalloffExponent(float AttributeValue, bool bAddApplyDelegate = true);

private:
	IMPLEMENT_NODE_ATTRIBUTE_KEY(bUseInverseSquaredFalloff)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(LightFalloffExponent)
};

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeSpotLightFactoryNode : public UInterchangePointLightFactoryNode
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLightFactory")
	bool GetCustomInnerConeAngle(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLightFactory")
	bool SetCustomInnerConeAngle(float AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLightFactory")
	bool GetCustomOuterConeAngle(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SpotLightFactory")
	bool SetCustomOuterConeAngle(float AttributeValue, bool bAddApplyDelegate = true);

private:

	IMPLEMENT_NODE_ATTRIBUTE_KEY(InnerConeAngle)
	IMPLEMENT_NODE_ATTRIBUTE_KEY(OuterConeAngle)
};

