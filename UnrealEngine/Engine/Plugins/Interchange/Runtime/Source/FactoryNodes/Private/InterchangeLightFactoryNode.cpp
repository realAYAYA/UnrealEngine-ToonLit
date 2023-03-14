// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeLightFactoryNode.h"

#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"

bool UInterchangeBaseLightFactoryNode::GetCustomLightColor(FColor& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(LightColor, FColor)
}

bool UInterchangeBaseLightFactoryNode::SetCustomLightColor(const FColor& AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeBaseLightFactoryNode, LightColor, FColor, ULightComponent)
}

bool UInterchangeBaseLightFactoryNode::GetCustomIntensity(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(Intensity, float)
}

bool UInterchangeBaseLightFactoryNode::SetCustomIntensity(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeBaseLightFactoryNode, Intensity, float, ULightComponent)
}

bool UInterchangeBaseLightFactoryNode::GetCustomTemperature(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(Temperature, float)
}

bool UInterchangeBaseLightFactoryNode::SetCustomTemperature(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeBaseLightFactoryNode, Temperature, float, ULightComponent)
}

bool UInterchangeBaseLightFactoryNode::GetCustomUseTemperature(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(bUseTemperature, bool)
}

bool UInterchangeBaseLightFactoryNode::SetCustomUseTemperature(bool AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeBaseLightFactoryNode, bUseTemperature, bool, ULightComponent)
}

bool UInterchangeLightFactoryNode::GetCustomIntensityUnits(ELightUnits& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(IntensityUnits, ELightUnits)
}

bool UInterchangeLightFactoryNode::SetCustomIntensityUnits(ELightUnits AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeLightFactoryNode, IntensityUnits, ELightUnits, ULocalLightComponent)
}

bool UInterchangeLightFactoryNode::GetCustomAttenuationRadius(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(AttenuationRadius, float)
}

bool UInterchangeLightFactoryNode::SetCustomAttenuationRadius(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeLightFactoryNode, AttenuationRadius, float, ULocalLightComponent)
}

bool UInterchangeLightFactoryNode::GetCustomIESTexture(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(IESTexture, FString);
}

bool UInterchangeLightFactoryNode::SetCustomIESTexture(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(IESTexture, FString)
}

bool UInterchangePointLightFactoryNode::GetCustomUseInverseSquaredFalloff(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(bUseInverseSquaredFalloff, bool)
}

bool UInterchangePointLightFactoryNode::SetCustomUseInverseSquaredFalloff(bool AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangePointLightFactoryNode, bUseInverseSquaredFalloff, bool, UPointLightComponent)
}

bool UInterchangePointLightFactoryNode::GetCustomLightFalloffExponent(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(LightFalloffExponent, float)
}

bool UInterchangePointLightFactoryNode::SetCustomLightFalloffExponent(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangePointLightFactoryNode, LightFalloffExponent, float, UPointLightComponent)
}

bool UInterchangeSpotLightFactoryNode::GetCustomInnerConeAngle(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(InnerConeAngle, float)
}

bool UInterchangeSpotLightFactoryNode::SetCustomInnerConeAngle(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeSpotLightFactoryNode, InnerConeAngle, float, USpotLightComponent)
}

bool UInterchangeSpotLightFactoryNode::GetCustomOuterConeAngle(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(OuterConeAngle, float)
}

bool UInterchangeSpotLightFactoryNode::SetCustomOuterConeAngle(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeSpotLightFactoryNode, OuterConeAngle, float, USpotLightComponent)
}

bool UInterchangeRectLightFactoryNode::GetCustomSourceWidth(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SourceWidth, float)
}

bool UInterchangeRectLightFactoryNode::SetCustomSourceWidth(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeRectLightFactoryNode, SourceWidth, float, URectLightComponent)
}

bool UInterchangeRectLightFactoryNode::GetCustomSourceHeight(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SourceHeight, float)
}

bool UInterchangeRectLightFactoryNode::SetCustomSourceHeight(float AttributeValue, bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UInterchangeRectLightFactoryNode, SourceHeight, float, URectLightComponent)
}