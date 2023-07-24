// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeVariantSetNode.generated.h"

/**
 * Class to represent a set of variants
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeVariantSetNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

	UInterchangeVariantSetNode();

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("VariantSet");
	}

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("VariantSetNode");
		return TypeName;
	}

	/**
	 * This function allow to retrieve the text which is displayed in the UI for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | VariantSet")
	bool GetCustomDisplayText(FString& AttributeValue) const;

	/**
	 * This function allow to set the text to be displayed in the UI for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | VariantSet")
	bool SetCustomDisplayText(const FString& AttributeValue);

	/**
	 * Get the payload key needed to retrieve the variants for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | VariantSet")
	bool GetCustomVariantsPayloadKey(FString& PayloadKey) const;

	/**
	 * Set the payload key needed to retrieve the variants for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | VariantSet")
	bool SetCustomVariantsPayloadKey(const FString& PayloadKey);

	/**
	 * This function allow to retrieve the number of translated node's unique ids for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	int32 GetCustomDependencyUidCount() const;

	/**
	 * This function allow to retrieve all the translated node's unique ids for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	void GetCustomDependencyUids(TArray<FString>& OutDependencyUids) const;

	/**
	 * This function allow to retrieve a specific translated node's unique id for this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	void GetCustomDependencyUid(const int32 Index, FString& OutDependencyUid) const;

	/**
	 * Add one translated node's unique id to this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	bool AddCustomDependencyUid(const FString& DependencyUid);

	/**
	 * Remove one translated node's unique id from this VariantSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	bool RemoveCustomDependencyUid(const FString& DependencyUid);

private:
	const UE::Interchange::FAttributeKey Macro_CustomDisplayTextKey = UE::Interchange::FAttributeKey(TEXT("DisplayText"));
	const UE::Interchange::FAttributeKey Macro_CustomVariantsPayloadKey = UE::Interchange::FAttributeKey(TEXT("VariantsPayload"));
	UE::Interchange::TArrayAttributeHelper<FString> CustomDependencyUids;
};

/**
 * Class to represent a set of VariantSet nodes
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeSceneVariantSetsNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

	UInterchangeSceneVariantSetsNode();

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("SceneVariantSet");
	}

	/**
	* Return the node type name of the class, we use this when reporting errors
	*/
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("SceneVariantSetNode");
		return TypeName;
	}

	/**
	 * This function allow to retrieve the number of VariantSets for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	int32 GetCustomVariantSetUidCount() const;

	/**
	 * This function allow to retrieve all the VariantSets' unique ids for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	void GetCustomVariantSetUids(TArray<FString>& OutVariantUids) const;

	/**
	 * This function allow to retrieve one VariantSet's unique id for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	void GetCustomVariantSetUid(const int32 Index, FString& OutVariantUid) const;

	/**
	 * Add one VariantSet's unique id to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	bool AddCustomVariantSetUid(const FString& VariantUid);

	/**
	 * Remove one VariantSet's unique id from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSet")
	bool RemoveCustomVariantSetUid(const FString& VariantUid);

private:
	UE::Interchange::TArrayAttributeHelper<FString> CustomVariantSetUids;
};

