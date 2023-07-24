// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/InterchangeAnimationAPI.h"

#include "Nodes/InterchangeBaseNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeAnimationAPI)

const UE::Interchange::FAttributeKey UInterchangeAnimationAPI::Macro_CustomIsNodeTransformAnimatedKey = UE::Interchange::FAttributeKey(TEXT("IsNodeTransformAnimated"));
const UE::Interchange::FAttributeKey UInterchangeAnimationAPI::Macro_CustomNodeTransformAnimationKeyCountKey = UE::Interchange::FAttributeKey(TEXT("NodeTransformAnimationKeyCount"));
const UE::Interchange::FAttributeKey UInterchangeAnimationAPI::Macro_CustomNodeTransformAnimationStartTimeKey = UE::Interchange::FAttributeKey(TEXT("NodeTransformAnimationStartTime"));
const UE::Interchange::FAttributeKey UInterchangeAnimationAPI::Macro_CustomNodeTransformAnimationEndTimeKey = UE::Interchange::FAttributeKey(TEXT("NodeTransformAnimationEndTime"));
const UE::Interchange::FAttributeKey UInterchangeAnimationAPI::Macro_CustomNodeTransformPayloadKeyKey = UE::Interchange::FAttributeKey(TEXT("NodeTransformPayloadKey"));

bool UInterchangeAnimationAPI::GetCustomIsNodeTransformAnimated(const UInterchangeBaseNode* InterchangeBaseNode, bool& AttributeValue)
{
	return InterchangeBaseNode->GetBooleanAttribute(Macro_CustomIsNodeTransformAnimatedKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::SetCustomIsNodeTransformAnimated(UInterchangeBaseNode* InterchangeBaseNode, const bool& AttributeValue)
{
	return InterchangeBaseNode->AddBooleanAttribute(Macro_CustomIsNodeTransformAnimatedKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::GetCustomNodeTransformAnimationKeyCount(const UInterchangeBaseNode* InterchangeBaseNode, int32& AttributeValue)
{
	return InterchangeBaseNode->GetInt32Attribute(Macro_CustomNodeTransformAnimationKeyCountKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::SetCustomNodeTransformAnimationKeyCount(UInterchangeBaseNode* InterchangeBaseNode, const int32& AttributeValue)
{
	return InterchangeBaseNode->AddInt32Attribute(Macro_CustomNodeTransformAnimationKeyCountKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::GetCustomNodeTransformAnimationStartTime(const UInterchangeBaseNode* InterchangeBaseNode, double& AttributeValue)
{
	return InterchangeBaseNode->GetDoubleAttribute(Macro_CustomNodeTransformAnimationStartTimeKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::SetCustomNodeTransformAnimationStartTime(UInterchangeBaseNode* InterchangeBaseNode, const double& AttributeValue)
{
	return InterchangeBaseNode->AddDoubleAttribute(Macro_CustomNodeTransformAnimationStartTimeKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::GetCustomNodeTransformAnimationEndTime(const UInterchangeBaseNode* InterchangeBaseNode, double& AttributeValue)
{
	return InterchangeBaseNode->GetDoubleAttribute(Macro_CustomNodeTransformAnimationEndTimeKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::SetCustomNodeTransformAnimationEndTime(UInterchangeBaseNode* InterchangeBaseNode, const double& AttributeValue)
{
	return InterchangeBaseNode->AddDoubleAttribute(Macro_CustomNodeTransformAnimationEndTimeKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::GetCustomNodeTransformPayloadKey(const UInterchangeBaseNode* InterchangeBaseNode, FString& AttributeValue)
{
	return InterchangeBaseNode->GetStringAttribute(Macro_CustomNodeTransformPayloadKeyKey.Key, AttributeValue);
}

bool UInterchangeAnimationAPI::SetCustomNodeTransformPayloadKey(UInterchangeBaseNode* InterchangeBaseNode, const FString& AttributeValue)
{
	return InterchangeBaseNode->AddStringAttribute(Macro_CustomNodeTransformPayloadKeyKey.Key, AttributeValue);
}

