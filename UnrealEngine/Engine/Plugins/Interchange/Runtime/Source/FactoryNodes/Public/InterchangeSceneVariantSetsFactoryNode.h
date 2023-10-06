// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeSceneVariantSetsFactoryNode.generated.h"

UCLASS(BlueprintType, Experimental)
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
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("SceneVariantSetFactoryNode");
		return TypeName;
	}

	/** Get the class this node want to create */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	virtual class UClass* GetObjectClass() const override;

	/**
	 * This function allow to retrieve the number of translated VariantSets' unique ids for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	int32 GetCustomVariantSetUidCount() const;

	/**
	 * This function allow to retrieve all the translated VariantSets' unique ids for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	void GetCustomVariantSetUids(TArray<FString>& OutVariantUids) const;

	/**
	 * This function allow to retrieve one translated VariantSet's unique id for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	void GetCustomVariantSetUid(const int32 Index, FString& OutVariantUid) const;

	/**
	 * Add one translated VariantSet's unique id to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	bool AddCustomVariantSetUid(const FString& VariantUid);

	/**
	 * Remove one translated VariantSet's unique id from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SceneVariantSetFactory")
	bool RemoveCustomVariantSetUid(const FString& VariantUid);

private:
	UE::Interchange::TArrayAttributeHelper<FString> CustomVariantSetUids;
};
