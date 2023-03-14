// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeStaticMeshLodDataNode.generated.h"


namespace UE
{
	namespace Interchange
	{
		struct FStaticMeshNodeLodDataStaticData : public FBaseNodeStaticData
		{
			static const FAttributeKey& GetMeshUidsBaseKey();
			static const FAttributeKey& GetBoxCollisionMeshUidsBaseKey();
			static const FAttributeKey& GetCapsuleCollisionMeshUidsBaseKey();
			static const FAttributeKey& GetSphereCollisionMeshUidsBaseKey();
			static const FAttributeKey& GetConvexCollisionMeshUidsBaseKey();
		};
	} // namespace Interchange
} // namespace UE


UCLASS(BlueprintType, Experimental)
class INTERCHANGEFACTORYNODES_API UInterchangeStaticMeshLodDataNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeStaticMeshLodDataNode();

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override;

	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

public:
	/* Mesh Uids: It can be either a scene or a mesh node uid. If its a scene it mean we want the mesh factory to bake the geo payload with the global transform of the scene node. */

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	int32 GetMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	void GetMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool AddMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveAllMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	int32 GetBoxCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	void GetBoxCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool AddBoxCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveBoxCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveAllBoxCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	int32 GetCapsuleCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	void GetCapsuleCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool AddCapsuleCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveCapsuleCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveAllCapsuleCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	int32 GetSphereCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	void GetSphereCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool AddSphereCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveSphereCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveAllSphereCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	int32 GetConvexCollisionMeshUidsCount() const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	void GetConvexCollisionMeshUids(TArray<FString>& OutMeshNames) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool AddConvexCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveConvexCollisionMeshUid(const FString& MeshName);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool RemoveAllConvexCollisionMeshes();

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool GetOneConvexHullPerUCX(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | StaticMeshLodData")
	bool SetOneConvexHullPerUCX(bool AttributeValue);


private:

	bool IsEditorOnlyDataDefined();

	UE::Interchange::TArrayAttributeHelper<FString> MeshUids;
	UE::Interchange::TArrayAttributeHelper<FString> BoxCollisionMeshUids;
	UE::Interchange::TArrayAttributeHelper<FString> CapsuleCollisionMeshUids;
	UE::Interchange::TArrayAttributeHelper<FString> SphereCollisionMeshUids;
	UE::Interchange::TArrayAttributeHelper<FString> ConvexCollisionMeshUids;

	const UE::Interchange::FAttributeKey Macro_CustomOneConvexHullPerUCXKey = UE::Interchange::FAttributeKey(TEXT("__OneConvexHullPerUCX__Key"));
};
