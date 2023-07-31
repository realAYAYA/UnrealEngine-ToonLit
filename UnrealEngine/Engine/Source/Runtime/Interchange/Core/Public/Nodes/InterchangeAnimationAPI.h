// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Types/AttributeStorage.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangeAnimationAPI.generated.h"

class UInterchangeBaseNode;
namespace UE { namespace Interchange { struct FAttributeKey; } }
struct FFrame;

/**
 * UInterchangeAnimationAPI is used to store and retrieve animation data(i.e. DCC node attributes, pipelines will have access to those attributes)
 */
UCLASS(BlueprintType)
class INTERCHANGECORE_API UInterchangeAnimationAPI : public UObject
{
	GENERATED_BODY()
 
public:

	/** Get true if the node transform have animation. Return false if the Attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool GetCustomIsNodeTransformAnimated(const UInterchangeBaseNode* InterchangeBaseNode, bool& AttributeValue);

	/** Set true if the node transform has animation. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool SetCustomIsNodeTransformAnimated(UInterchangeBaseNode* InterchangeBaseNode, const bool& AttributeValue);

	/** Get the node transform animation key count. Return false if the Attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool GetCustomNodeTransformAnimationKeyCount(const UInterchangeBaseNode* InterchangeBaseNode, int32& AttributeValue);

	/** Set the node transform animation key count. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool SetCustomNodeTransformAnimationKeyCount(UInterchangeBaseNode* InterchangeBaseNode, const int32& AttributeValue);

	/**
	 * Get the animation start time in second for this node transform.
	 * Return false if the Attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool GetCustomNodeTransformAnimationStartTime(const UInterchangeBaseNode* InterchangeBaseNode, double& AttributeValue);

	/** Set the animation start time in second for this node transform. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool SetCustomNodeTransformAnimationStartTime(UInterchangeBaseNode* InterchangeBaseNode, const double& AttributeValue);

	/**
	 * Get the animation end time in second for this node transform.
	 * Return false if the Attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool GetCustomNodeTransformAnimationEndTime(const UInterchangeBaseNode* InterchangeBaseNode, double& AttributeValue);

	/** Set the animation end time in second for this node transform. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool SetCustomNodeTransformAnimationEndTime(UInterchangeBaseNode* InterchangeBaseNode, const double& AttributeValue);

	/**
	 * Get the payload key needed to retrieve the animation from the source.
	 * Return false if the Attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool GetCustomNodeTransformPayloadKey(const UInterchangeBaseNode* InterchangeBaseNode, FString& AttributeValue);

	/** Set the payload key needed to retrieve the animation from the source. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Scene | Animation")
	static bool SetCustomNodeTransformPayloadKey(UInterchangeBaseNode* InterchangeBaseNode, const FString& AttributeValue);
	
private:
	static const UE::Interchange::FAttributeKey Macro_CustomIsNodeTransformAnimatedKey;
	static const UE::Interchange::FAttributeKey Macro_CustomNodeTransformAnimationKeyCountKey;
	static const UE::Interchange::FAttributeKey Macro_CustomNodeTransformAnimationStartTimeKey;
	static const UE::Interchange::FAttributeKey Macro_CustomNodeTransformAnimationEndTimeKey;
	static const UE::Interchange::FAttributeKey Macro_CustomNodeTransformPayloadKeyKey;
};
