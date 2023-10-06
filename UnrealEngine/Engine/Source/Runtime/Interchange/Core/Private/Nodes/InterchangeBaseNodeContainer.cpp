// Copyright Epic Games, Inc. All Rights Reserved.
#include "Nodes/InterchangeBaseNodeContainer.h"

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectIterator.h"
#include "UObject/CoreRedirects.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeBaseNodeContainer)

UInterchangeBaseNodeContainer::UInterchangeBaseNodeContainer()
{

}

FString UInterchangeBaseNodeContainer::AddNode(UInterchangeBaseNode* Node)
{
	if (!Node)
	{
		return UInterchangeBaseNode::InvalidNodeUid();
	}
	FString NodeUniqueID = Node->GetUniqueID();
	if (NodeUniqueID == UInterchangeBaseNode::InvalidNodeUid())
	{
		return UInterchangeBaseNode::InvalidNodeUid();
	}
		
	//Cannot add an node with the same IDs
	if (Nodes.Contains(NodeUniqueID))
	{
		return NodeUniqueID;
	}

	if (Node->GetDisplayLabel().IsEmpty())
	{
		//Replace None by Null, since None name will be interpret like NAME_None which will not work with UObject creation
		//UObject Creation will name it ClassName_X instead of None
		//TODO Log an warning to the user
		Node->SetDisplayLabel(FString(TEXT("Null")));
	}

	//Copy the node
	Nodes.Add(NodeUniqueID, Node);
	return NodeUniqueID;
}

void UInterchangeBaseNodeContainer::ReplaceNode(const FString& NodeUniqueID, UInterchangeFactoryBaseNode* NewNode)
{
	if (GetFactoryNode(NodeUniqueID)) //Check existance and confirm it is FactoryNode
	{
		Nodes.Remove(NodeUniqueID);
		AddNode(NewNode);
	}
}

bool UInterchangeBaseNodeContainer::IsNodeUidValid(const FString& NodeUniqueID) const
{
	if (NodeUniqueID == UInterchangeBaseNode::InvalidNodeUid())
	{
		return false;
	}
	return Nodes.Contains(NodeUniqueID);
}

void UInterchangeBaseNodeContainer::IterateNodes(TFunctionRef<void(const FString&, UInterchangeBaseNode*)> IterationLambda) const
{
	for (auto& NodeKeyValue : Nodes)
	{
		IterationLambda(NodeKeyValue.Key, NodeKeyValue.Value);
	}
}

void UInterchangeBaseNodeContainer::BreakableIterateNodes(TFunctionRef<bool(const FString&, UInterchangeBaseNode*)> IterationLambda) const
{
	for (auto& NodeKeyValue : Nodes)
	{
		if (IterationLambda(NodeKeyValue.Key, NodeKeyValue.Value))
		{
			break;
		}
	}
}

void UInterchangeBaseNodeContainer::GetRoots(TArray<FString>& RootNodes) const
{
	for (auto& NodeKeyValue : Nodes)
	{
		if (NodeKeyValue.Value->GetParentUid() == UInterchangeBaseNode::InvalidNodeUid())
		{
			RootNodes.Add(NodeKeyValue.Key);
		}
	}
}

void UInterchangeBaseNodeContainer::GetNodes(const UClass* ClassNode, TArray<FString>& OutNodes) const
{
	OutNodes.Empty();
	IterateNodes([&ClassNode, &OutNodes](const FString& NodeUid, UInterchangeBaseNode* Node)
	{
		if(Node->GetClass()->IsChildOf(ClassNode))
		{
			OutNodes.Add(Node->GetUniqueID());
		}
	});
}

const UInterchangeBaseNode* UInterchangeBaseNodeContainer::GetNode(const FString& NodeUniqueID) const
{
	if (NodeUniqueID == UInterchangeBaseNode::InvalidNodeUid())
	{
		return nullptr;
	}
	if (!Nodes.Contains(NodeUniqueID))
	{
		return nullptr;
	}
	UInterchangeBaseNode* Node = Nodes.FindChecked(NodeUniqueID);
	return Node;
}

UInterchangeFactoryBaseNode* UInterchangeBaseNodeContainer::GetFactoryNode(const FString& NodeUniqueID) const
{
	if (NodeUniqueID == UInterchangeBaseNode::InvalidNodeUid())
	{
		return nullptr;
	}
	if (!Nodes.Contains(NodeUniqueID))
	{
		return nullptr;
	}
	UInterchangeFactoryBaseNode* FactoryNode = Cast< UInterchangeFactoryBaseNode>(Nodes.FindChecked(NodeUniqueID));
	return FactoryNode;
}

bool UInterchangeBaseNodeContainer::SetNodeParentUid(const FString& NodeUniqueID, const FString& NewParentNodeUid)
{
	if (!Nodes.Contains(NodeUniqueID))
	{
		return false;
	}
	if (!Nodes.Contains(NewParentNodeUid))
	{
		return false;
	}
	UInterchangeBaseNode* Node = Nodes.FindChecked(NodeUniqueID);
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Node->SetParentUid(NewParentNodeUid);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	//Update the children cache
	TArray<FString>& Children = ChildrenCache.FindOrAdd(NewParentNodeUid);
	Children.Add(NodeUniqueID);

	return true;
}

int32 UInterchangeBaseNodeContainer::GetNodeChildrenCount(const FString& NodeUniqueID) const
{
	TArray<FString> ChildrenUids = GetNodeChildrenUids(NodeUniqueID);
	return ChildrenUids.Num();
}

TArray<FString> UInterchangeBaseNodeContainer::GetNodeChildrenUids(const FString& NodeUniqueID) const
{
	if(TArray<FString>* CacheChildrenPtr = ChildrenCache.Find(NodeUniqueID))
	{
		return *CacheChildrenPtr;
	}

	//Update the cache
	TArray<FString>& CacheChildren = ChildrenCache.Add(NodeUniqueID);
	for (const auto& NodeKeyValue : Nodes)
	{
		if (NodeKeyValue.Value->GetParentUid() == NodeUniqueID)
		{
			CacheChildren.Add(NodeKeyValue.Key);
		}
	}
	return CacheChildren;
}

UInterchangeBaseNode* UInterchangeBaseNodeContainer::GetNodeChildren(const FString& NodeUniqueID, int32 ChildIndex)
{
	return GetNodeChildrenInternal(NodeUniqueID, ChildIndex);
}

const UInterchangeBaseNode* UInterchangeBaseNodeContainer::GetNodeChildren(const FString& NodeUniqueID, int32 ChildIndex) const
{
	return const_cast<UInterchangeBaseNodeContainer*>(this)->GetNodeChildrenInternal(NodeUniqueID, ChildIndex);
}

void UInterchangeBaseNodeContainer::SerializeNodeContainerData(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		Nodes.Reset();
	}
	int32 NodeCount = Nodes.Num();
	Ar << NodeCount;

	if(Ar.IsSaving())
	{
		//The node name is not serialize since its an attribute inside the node that will be serialize by the node itself
		auto SerializeNodePair = [&Ar](UInterchangeBaseNode* BaseNode)
		{
			FString ClassFullName = BaseNode->GetClass()->GetFullName();
			Ar << ClassFullName;
			BaseNode->Serialize(Ar);
		};

		for (auto NodePair : Nodes)
		{
			SerializeNodePair(NodePair.Value);
		}
	}
	else if(Ar.IsLoading())
	{
		//Find all the potential node class
		TMap<FString, UClass*> ClassPerName;
		for (FThreadSafeObjectIterator It(UClass::StaticClass()); It; ++It)
		{
			UClass* Class = Cast<UClass>(*It);
			if (Class->IsChildOf(UInterchangeBaseNode::StaticClass()))
			{
				ClassPerName.Add(Class->GetFullName(), Class);
			}
		}

		for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
		{
			FString ClassFullName;
			Ar << ClassFullName;

			FCoreRedirectObjectName RedirectedObjectName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, FCoreRedirectObjectName(ClassFullName));
			if (RedirectedObjectName.IsValid())
			{
				ClassFullName = RedirectedObjectName.ToString();
			}

			//This cannot fail to make sure we have a healty serialization
			if (!ensure(ClassPerName.Contains(ClassFullName)))
			{
				//We did not successfully serialize the content of the file into the node container
				return;
			}
			UClass* ToCreateClass = ClassPerName.FindChecked(ClassFullName);
			//Create a UInterchangeBaseNode with the proper class
			UInterchangeBaseNode* BaseNode = NewObject<UInterchangeBaseNode>(this, ToCreateClass);
			BaseNode->Serialize(Ar);
			AddNode(BaseNode);
		}
		ComputeChildrenCache();
	}
}

void UInterchangeBaseNodeContainer::SaveToFile(const FString& Filename)
{
	FLargeMemoryWriter Ar;
	SerializeNodeContainerData(Ar);
	uint8* ArchiveData = Ar.GetData();
	int64 ArchiveSize = Ar.TotalSize();
	TArray64<uint8> Buffer(ArchiveData, ArchiveSize);
	FFileHelper::SaveArrayToFile(Buffer, *Filename);
}

void UInterchangeBaseNodeContainer::LoadFromFile(const FString& Filename)
{
	//All sub object should be gone with the reset
	Nodes.Reset();
	TArray64<uint8> Buffer;
	FFileHelper::LoadFileToArray(Buffer, *Filename);
	uint8* FileData = Buffer.GetData();
	int64 FileDataSize = Buffer.Num();
	if (FileDataSize < 1)
	{
		//Nothing to load from this file
		return;
	}
	//Buffer keep the ownership of the data, the large memory reader is use to serialize the TMap
	FLargeMemoryReader Ar(FileData, FileDataSize);
	SerializeNodeContainerData(Ar);
}

void UInterchangeBaseNodeContainer::ComputeChildrenCache()
{
	ResetChildrenCache();
	for (const auto& NodeKeyValue : Nodes)
	{
		//Force all node to have a cache, even if there is no parent
		ChildrenCache.FindOrAdd(NodeKeyValue.Key);

		//Update the parent cache
		const FString ParentUid = NodeKeyValue.Value->GetParentUid();
		if (!ParentUid.IsEmpty())
		{
			TArray<FString>& Children = ChildrenCache.FindOrAdd(ParentUid);
			Children.Add(NodeKeyValue.Key);
		}
	}
}

UInterchangeBaseNode* UInterchangeBaseNodeContainer::GetNodeChildrenInternal(const FString& NodeUniqueID, int32 ChildIndex)
{
	TArray<FString> ChildrenUids = GetNodeChildrenUids(NodeUniqueID);
	if (!ChildrenUids.IsValidIndex(ChildIndex))
	{
		return nullptr;
	}

	if (Nodes.Contains(ChildrenUids[ChildIndex]))
	{
		UInterchangeBaseNode* Node = Nodes.FindChecked(ChildrenUids[ChildIndex]);
		return Node;
	}

	return nullptr;
}

