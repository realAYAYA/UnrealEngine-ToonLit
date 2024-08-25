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

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeSkeletalMeshLodDataNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeSkeletalMeshLodDataNode();

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override;

#if WITH_EDITOR
	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
#endif //WITH_EDITOR

public:
	/** Query the LOD skeletal mesh factory skeleton reference. Return false if the attribute was not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool GetCustomSkeletonUid(FString& AttributeValue) const;

	/** Set the LOD skeletal mesh factory skeleton reference. Return false if the attribute could not be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool SetCustomSkeletonUid(const FString& AttributeValue);

	/* Return the number of mesh geometries this LOD will be made from. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	int32 GetMeshUidsCount() const;

	/* Query all mesh geometry this LOD will be made from. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	void GetMeshUids(TArray<FString>& OutMeshNames) const;

	/* Add a mesh geometry used to create this LOD geometry. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool AddMeshUid(const FString& MeshName);

	/* Remove a mesh geometry used to create this LOD geometry. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool RemoveMeshUid(const FString& MeshName);

	/* Remove all mesh geometry used to create this LOD geometry. A mesh UID can represent either a scene node or a mesh node. If it is a scene node, the mesh factory bakes the geometry payload with the global transform of the scene node. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalMeshLodData")
	bool RemoveAllMeshes();

private:

	bool IsEditorOnlyDataDefined();

	const UE::Interchange::FAttributeKey Macro_CustomSkeletonUidKey = UE::Interchange::FAttributeKey(TEXT("__SkeletonUid__Key"));

	UE::Interchange::TArrayAttributeHelper<FString> MeshUids;
protected:
};
