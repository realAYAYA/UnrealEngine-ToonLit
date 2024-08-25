// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeUtilities.h"
#include "Types/AttributeStorage.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangeSourceNode.generated.h"

class UInterchangeBaseNodeContainer;
class UObject;
struct FFrame;

/**
 * This class allows a translator to add general source data that describes the whole source. Pipelines can use this information.
 */
UCLASS(BlueprintType, MinimalAPI)
class UInterchangeSourceNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

	UInterchangeSourceNode();

public:
	/**
	 * Initialize the base data of the node.
	 * @param UniqueID - The unique ID for this node.
	 * @param DisplayLabel - The name of the node.
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | source")
	INTERCHANGECORE_API void InitializeSourceNode(const FString& UniqueID, const FString& DisplayLabel);

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	INTERCHANGECORE_API virtual FString GetTypeName() const override;

	/* Translators that want to modify the common data should ensure they create the unique common pipeline node. */
	static INTERCHANGECORE_API UInterchangeSourceNode* FindOrCreateUniqueInstance(UInterchangeBaseNodeContainer* NodeContainer);

	/* This function should be use by pipelines to avoid creating a node. If the unique instance doesn't exist, returns nullptr. */
	static INTERCHANGECORE_API const UInterchangeSourceNode* GetUniqueInstance(const UInterchangeBaseNodeContainer* NodeContainer);

	/** Query the source frame rate numerator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomSourceFrameRateNumerator(int32& AttributeValue) const;

	/** Set the source frame rate numerator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomSourceFrameRateNumerator(const int32& AttributeValue);

	/** Query the source frame rate denominator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomSourceFrameRateDenominator(int32& AttributeValue) const;

	/** Set the source frame rate denominator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomSourceFrameRateDenominator(const int32& AttributeValue);

	/** Query the start of the source timeline. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomSourceTimelineStart(double& AttributeValue) const;

	/** Set the start of the source timeline. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomSourceTimelineStart(const double& AttributeValue);

	/** Query the end of the source timeline. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomSourceTimelineEnd(double& AttributeValue) const;

	/** Set the end of the source timeline. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomSourceTimelineEnd(const double& AttributeValue);

	/** Query the start of the source animated time. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomAnimatedTimeStart(double& AttributeValue) const;

	/** Set the start of the source animated time. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomAnimatedTimeStart(const double& AttributeValue);

	/** Query the end of the source animated time. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomAnimatedTimeEnd(double& AttributeValue) const;

	/** Set the end of the source animated time. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomAnimatedTimeEnd(const double& AttributeValue);

	/** Query whether to import materials that aren't used. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool GetCustomImportUnusedMaterial(bool& AttributeValue) const;

	/** Set whether to import materials that aren't used. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	INTERCHANGECORE_API bool SetCustomImportUnusedMaterial(const bool& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomSourceFrameRateNumeratorKey = UE::Interchange::FAttributeKey(TEXT("SourceFrameRateNumerator"));
	const UE::Interchange::FAttributeKey Macro_CustomSourceFrameRateDenominatorKey = UE::Interchange::FAttributeKey(TEXT("SourceFrameRateDenominator"));
	const UE::Interchange::FAttributeKey Macro_CustomSourceTimelineStartKey = UE::Interchange::FAttributeKey(TEXT("SourceTimelineStart"));
	const UE::Interchange::FAttributeKey Macro_CustomSourceTimelineEndKey = UE::Interchange::FAttributeKey(TEXT("SourceTimelineEnd"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimatedTimeStartKey = UE::Interchange::FAttributeKey(TEXT("AnimatedTimeStart"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimatedTimeEndKey = UE::Interchange::FAttributeKey(TEXT("AnimatedTimeEnd"));
	const UE::Interchange::FAttributeKey Macro_CustomImportUnusedMaterialKey = UE::Interchange::FAttributeKey(TEXT("ImportUnusedMaterial"));
};

