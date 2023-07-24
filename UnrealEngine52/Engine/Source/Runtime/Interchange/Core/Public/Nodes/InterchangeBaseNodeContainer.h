// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/FileHelper.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Templates/Casts.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangeBaseNodeContainer.generated.h"

class FArchive;
class UClass;
class UInterchangeBaseNode;
class UInterchangeFactoryBaseNode;
struct FFrame;
template <typename T> struct TObjectPtr;


/**
 * Interchange UInterchangeBaseNode graph is a format used to feed asset/scene import/reimport/export factories/writer.
 * This container hold a flat list of all nodes that have been translated from the source data.
 *
 * Translators are filling this container and the Import/Export managers are reading it to execute the import/export process
 */
 UCLASS(BlueprintType)
class INTERCHANGECORE_API UInterchangeBaseNodeContainer : public UObject
{
	GENERATED_BODY()
public:
	UInterchangeBaseNodeContainer();

	/**
	 * Add a node in the container, the node will be add into a TMap.
	 *
	 * @param Node - a pointer on the node you want to add
	 * @return: return the node unique ID of the added item. If the node already exist it will return the existing ID. Return InvalidNodeUid if the node cannot be added.
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	FString AddNode(UInterchangeBaseNode* Node);

	/** Return true if the node unique ID exist in the container */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	bool IsNodeUidValid(const FString& NodeUniqueID) const;

	/** Unordered iteration of the all nodes */
	void IterateNodes(TFunctionRef<void(const FString&, UInterchangeBaseNode*)> IterationLambda) const;

	template <typename T>
	void IterateNodesOfType(TFunctionRef<void(const FString&, T*)> IterationLambda) const
	{
		for (const TPair<FString, TObjectPtr<UInterchangeBaseNode>>& NodeKeyValue : Nodes)
		{
			if (T* Node = Cast<T>(NodeKeyValue.Value))
			{
				IterationLambda(NodeKeyValue.Key, Node);
			}
		}
	}

	/**
	 * Recursively traverse the hierarchy starting with the specified node unique ID.
	 * If the iteration lambda return true the iteration will stop. If the iteration lambda return false the iteration will continue.
	 */
	void IterateNodeChildren(const FString& NodeUniqueID, TFunctionRef<void(const UInterchangeBaseNode*)> IterationLambda) const
	{

		if (const UInterchangeBaseNode* Node = GetNode(NodeUniqueID))
		{
			IterationLambda(Node);
			const TArray<FString> ChildrenIds = GetNodeChildrenUids(NodeUniqueID);
			for (int32 ChildIndex = 0; ChildIndex < ChildrenIds.Num(); ++ChildIndex)
			{
				IterateNodeChildren(ChildrenIds[ChildIndex], IterationLambda);
			}
		}
	}

	/**
	 * Recursively traverse the hierarchy starting with the specified node unique ID.
	 * If the iteration lambda return true the iteration will stop. If the iteration lambda return false the iteration will continue.
	 * 
	 * @return - Return true if the iteration was break, false otherwise
	 */
	bool BreakableIterateNodeChildren(const FString& NodeUniqueID, TFunctionRef<bool(const UInterchangeBaseNode*)> IterationLambda) const
	{
		if (const UInterchangeBaseNode* Node = GetNode(NodeUniqueID))
		{
			if (IterationLambda(Node))
			{
				return true;
			}
			const TArray<FString> ChildrenIds = GetNodeChildrenUids(NodeUniqueID);
			for (int32 ChildIndex = 0; ChildIndex < ChildrenIds.Num(); ++ChildIndex)
			{
				if (BreakableIterateNodeChildren(ChildrenIds[ChildIndex], IterationLambda))
				{
					return true;
				}
			}
		}
		return false;
	}

	/** Unordered iteration of the all nodes, but I can be stop early by returning true */
	void BreakableIterateNodes(TFunctionRef<bool(const FString&, UInterchangeBaseNode*)> IterationLambda) const;

	template <typename T>
	void BreakableIterateNodesOfType(TFunctionRef<bool(const FString&, T*)> IterationLambda) const
	{
		for (const TPair<FString, TObjectPtr<UInterchangeBaseNode>>& NodeKeyValue : Nodes)
		{
			if (T* Node = Cast<T>(NodeKeyValue.Value))
			{
				if (IterationLambda(NodeKeyValue.Key, Node))
				{
					break;
				}
			}
		}
	}

	/** Return all nodes that do not have any parent */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	void GetRoots(TArray<FString>& RootNodes) const;

	/** Return all nodes that are of the ClassNode type*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	void GetNodes(const UClass* ClassNode, TArray<FString>& OutNodes) const;

	/** Get a node pointer. Once added to the container, nodes are considered const */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	const UInterchangeBaseNode* GetNode(const FString& NodeUniqueID) const;

	/** Get a factory node pointer */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	UInterchangeFactoryBaseNode* GetFactoryNode(const FString& NodeUniqueID) const;

	/** Set node ParentUid */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	bool SetNodeParentUid(const FString& NodeUniqueID, const FString& NewParentNodeUid);

	/** Get the node children count */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	int32 GetNodeChildrenCount(const FString& NodeUniqueID) const;

	/** Get all children Uid */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	TArray<FString> GetNodeChildrenUids(const FString& NodeUniqueID) const;

	/** Get the node nth const children */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	UInterchangeBaseNode* GetNodeChildren(const FString& NodeUniqueID, int32 ChildIndex);

	/** Get the node nth const children. Const version */
	const UInterchangeBaseNode* GetNodeChildren(const FString& NodeUniqueID, int32 ChildIndex) const;

	/**
	 * This function serialize the node container and all node sub-objects point by it.
	 * Out of process translator like fbx will dump a file containing this data and the editor
	 * will read the file and regenerate the container from the saved data.
	 */
	void SerializeNodeContainerData(FArchive& Ar);

	/* Serialize the node container into the specified file.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	void SaveToFile(const FString& Filename);

	/* Serialize the node container from the specified file.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	void LoadFromFile(const FString& Filename);

	/* Fill the children uids cache to optimize the GetNodeChildrenUids call.*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	void ComputeChildrenCache();

	/* Reset the children uids cache. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node Container")
	void ResetChildrenCache()
	{
		ChildrenCache.Reset();
	}

private:

	UInterchangeBaseNode* GetNodeChildrenInternal(const FString& NodeUniqueID, int32 ChildIndex);

	/** Flat List of the nodes. Since the nodes are variable size, we store a pointer. */
	UPROPERTY(VisibleAnywhere, Category = "Interchange | Node Container")
	TMap<FString, TObjectPtr<UInterchangeBaseNode> > Nodes;

	mutable TMap<FString, TArray<FString> > ChildrenCache;
};
