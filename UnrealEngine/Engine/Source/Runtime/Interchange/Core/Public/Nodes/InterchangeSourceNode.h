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
 * This class allow a translator to add general source data that describe the whole source. Pipeline can use this information.
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGECORE_API UInterchangeSourceNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

	UInterchangeSourceNode();

public:
	/**
	 * Initialize the base data of the node
	 * @param UniqueID - The uniqueId for this node
	 * @param DisplayLabel - The name of the node
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | source")
	void InitializeSourceNode(const FString& UniqueID, const FString& DisplayLabel);

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override;

	/* The translators that want to modify the common data should ensure they create the unique common pipeline node. */
	static UInterchangeSourceNode* FindOrCreateUniqueInstance(UInterchangeBaseNodeContainer* NodeContainer);

	/* If the unique instance doesn't exist it will return nullptr. This function should be use by the pipelines, to avoid creating a node. */
	static const UInterchangeSourceNode* GetUniqueInstance(const UInterchangeBaseNodeContainer* NodeContainer);

	/** Query the source frame rate numerator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomSourceFrameRateNumerator(int32& AttributeValue) const;

	/** Store the source frame rate numerator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomSourceFrameRateNumerator(const int32& AttributeValue);

	/** Query the source frame rate denominator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomSourceFrameRateDenominator(int32& AttributeValue) const;

	/** Store the source frame rate denominator. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomSourceFrameRateDenominator(const int32& AttributeValue);

	/** Query the source time line start. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomSourceTimelineStart(double& AttributeValue) const;

	/** Store the source time line start. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomSourceTimelineStart(const double& AttributeValue);

	/** Query the source time line end. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomSourceTimelineEnd(double& AttributeValue) const;

	/** Store the source time line end. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomSourceTimelineEnd(const double& AttributeValue);

	/** Query the source animated time start. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomAnimatedTimeStart(double& AttributeValue) const;

	/** Store the source animated time start. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomAnimatedTimeStart(const double& AttributeValue);

	/** Query the source animated time end. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomAnimatedTimeEnd(double& AttributeValue) const;

	/** Store the source animated time end. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomAnimatedTimeEnd(const double& AttributeValue);

	/** Query the source animated time end. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool GetCustomImportUnusedMaterial(bool& AttributeValue) const;

	/** Store the source animated time end. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Source")
	bool SetCustomImportUnusedMaterial(const bool& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomSourceFrameRateNumeratorKey = UE::Interchange::FAttributeKey(TEXT("SourceFrameRateNumerator"));
	const UE::Interchange::FAttributeKey Macro_CustomSourceFrameRateDenominatorKey = UE::Interchange::FAttributeKey(TEXT("SourceFrameRateDenominator"));
	const UE::Interchange::FAttributeKey Macro_CustomSourceTimelineStartKey = UE::Interchange::FAttributeKey(TEXT("SourceTimelineStart"));
	const UE::Interchange::FAttributeKey Macro_CustomSourceTimelineEndKey = UE::Interchange::FAttributeKey(TEXT("SourceTimelineEnd"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimatedTimeStartKey = UE::Interchange::FAttributeKey(TEXT("AnimatedTimeStart"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimatedTimeEndKey = UE::Interchange::FAttributeKey(TEXT("AnimatedTimeEnd"));
	const UE::Interchange::FAttributeKey Macro_CustomImportUnusedMaterialKey = UE::Interchange::FAttributeKey(TEXT("ImportUnusedMaterial"));
};

