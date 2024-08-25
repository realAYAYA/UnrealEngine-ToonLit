// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeSceneVariantSetsFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeSceneVariantSetsFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

	UInterchangeSceneVariantSetsFactoryNode();

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("SceneVariantSetFactory");
	}

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("SceneVariantSetFactoryNode");
		return TypeName;
	}

	/** Get the class this node creates. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	virtual class UClass* GetObjectClass() const override;

	/**
	 * Retrieve the number of unique IDs of all translated VariantSets for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	int32 GetCustomVariantSetUidCount() const;

	/**
	 * Retrieve the unique IDs of all translated VariantSets for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	void GetCustomVariantSetUids(TArray<FString>& OutVariantUids) const;

	/**
	 * Retrieve the UID of the VariantSet with the specified index.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	void GetCustomVariantSetUid(const int32 Index, FString& OutVariantUid) const;

	/**
	 * Add a unique id of a translated VariantSet for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	bool AddCustomVariantSetUid(const FString& VariantUid);

	/**
	 * Remove the specified unique ID of a translated VariantSet from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	bool RemoveCustomVariantSetUid(const FString& VariantUid);

private:
	UE::Interchange::TArrayAttributeHelper<FString> CustomVariantSetUids;
};
