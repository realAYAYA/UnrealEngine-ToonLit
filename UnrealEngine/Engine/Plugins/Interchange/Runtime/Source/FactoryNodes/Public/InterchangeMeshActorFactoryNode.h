// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeActorFactoryNode.h"

#include "InterchangeMeshActorFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeMeshActorFactoryNode : public UInterchangeActorFactoryNode
{
	GENERATED_BODY()

public:
	UInterchangeMeshActorFactoryNode();

	/**
	 * Override Serialize() to restore SlotMaterialDependencies on load.
	 */
	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bIsInitialized)
		{
			SlotMaterialDependencies.RebuildCache();
		}
	}

	/**
	 * Retrieve the correspondence table between slot names and assigned materials for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	void GetSlotMaterialDependencies(TMap<FString, FString>& OutMaterialDependencies) const;

	/**
	 * Retrieve the Material dependency for the specified slot of this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool GetSlotMaterialDependencyUid(const FString& SlotName, FString& OutMaterialDependency) const;

	/**
	 * Add a Material dependency to the specified slot of this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool SetSlotMaterialDependencyUid(const FString& SlotName, const FString& MaterialDependencyUid);

	/**
	 * Remove the Material dependency associated with the specified slot name from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool RemoveSlotMaterialDependencyUid(const FString& SlotName);

	/** Set the animation asset for this scene node to play. (This is only relevant for SkeletalMeshActors: scene nodes that are instantiating skeletal meshes.) */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool SetCustomAnimationAssetUidToPlay(const FString& AttributeValue);
	/** Get the animation asset set for this scene node to play. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMesh")
	bool GetCustomAnimationAssetUidToPlay(FString& AttributeValue) const;

	/** Get the geometric offset. Any mesh attached to this scene node will be offset using this transform. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool GetCustomGeometricTransform(FTransform& AttributeValue) const;

	/** Set the geometric offset. Any mesh attached to this scene node will be offset using this transform. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMesh")
	bool SetCustomGeometricTransform(const FTransform& AttributeValue);

	bool GetCustomKeepSectionsSeparate(bool& AttributeValue) const
	{
		//Scene import do not support this options so we set the value to false and return false
		AttributeValue = false;
		return false;
	}

private:
	UE::Interchange::TMapAttributeHelper<FString, FString> SlotMaterialDependencies;

	//A scene node can reference an animation asset on top of base asset:
	const UE::Interchange::FAttributeKey Macro_CustomAnimationAssetUidToPlayKey = UE::Interchange::FAttributeKey(TEXT("AnimationAssetUidToPlay"));

	//A scene node can have a transform apply to the mesh it reference.
	const UE::Interchange::FAttributeKey Macro_CustomGeometricTransformKey = UE::Interchange::FAttributeKey(TEXT("GeometricTransform"));
};