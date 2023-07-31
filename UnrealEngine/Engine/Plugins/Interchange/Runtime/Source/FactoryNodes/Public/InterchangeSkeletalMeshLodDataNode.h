// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeSkeletalMeshLodDataNode.generated.h"

//Interchange namespace
namespace UE
{
	namespace Interchange
	{

		struct INTERCHANGEFACTORYNODES_API FSkeletalMeshNodeLodDataStaticData : public FBaseNodeStaticData
		{
			static const FAttributeKey& GetMeshUidsBaseKey();
		};

	}//ns Interchange
}//ns UE

UCLASS(BlueprintType, Experimental)
class INTERCHANGEFACTORYNODES_API UInterchangeSkeletalMeshLodDataNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeSkeletalMeshLodDataNode();

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override;

	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

public:
	/** Query the LOD skeletal mesh factory skeleton reference. Return false if the attribute was not set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool GetCustomSkeletonUid(FString& AttributeValue) const;

	/** Set the LOD skeletal mesh factory skeleton reference. Return false if the attribute cannot be set.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool SetCustomSkeletonUid(const FString& AttributeValue);

	/* Return the number of mesh geometry this LOD will be made of. Mesh uids can be either a scene or a mesh node. If its a scene it mean we want the mesh factory to bake the geo payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	int32 GetMeshUidsCount() const;

	/* Query all mesh geometry this LOD will be made of. Mesh uids can be either a scene or a mesh node. If its a scene it mean we want the mesh factory to bake the geo payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	void GetMeshUids(TArray<FString>& OutMeshNames) const;

	/* Add one mesh geometry use to create this LOD geometry. Mesh uids can be either a scene or a mesh node. If its a scene it mean we want the mesh factory to bake the geo payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool AddMeshUid(const FString& MeshName);

	/* Remove one mesh geometry use to create this LOD geometry. Mesh uids can be either a scene or a mesh node. If its a scene it mean we want the mesh factory to bake the geo payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool RemoveMeshUid(const FString& MeshName);

	/* Remove all mesh geometry use to create this LOD geometry. Mesh uids can be either a scene or a mesh node. If its a scene it mean we want the mesh factory to bake the geo payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool RemoveAllMeshes();

private:

	bool IsEditorOnlyDataDefined();

	const UE::Interchange::FAttributeKey Macro_CustomSkeletonUidKey = UE::Interchange::FAttributeKey(TEXT("__SkeletonUid__Key"));

	UE::Interchange::TArrayAttributeHelper<FString> MeshUids;
protected:
};
