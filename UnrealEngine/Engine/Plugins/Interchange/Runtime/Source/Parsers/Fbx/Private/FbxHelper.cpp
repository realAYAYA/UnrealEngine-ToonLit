// Copyright Epic Games, Inc. All Rights Reserved.

#include "FbxHelper.h"

#include "CoreMinimal.h"
#include "FbxConvert.h"
#include "FbxInclude.h"

#define GeneratedLODNameSuffix "_GeneratedLOD_"

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			FString FFbxHelper::GetMeshName(FbxGeometryBase* Mesh)
			{
				if (!Mesh)
				{
					return {};
				}

				FString DefaultPrefix;
				if (Mesh->GetAttributeType() == FbxNodeAttribute::eMesh)
				{
					DefaultPrefix = TEXT("Mesh");
				}
				else if (Mesh->GetAttributeType() == FbxNodeAttribute::eShape)
				{
					DefaultPrefix = TEXT("Shape");
				}

				return GetNodeAttributeName(Mesh, DefaultPrefix);
			}

			FString FFbxHelper::GetMeshUniqueID(FbxGeometryBase* Mesh)
			{
				if (!Mesh)
				{
					return {};
				}

				FString MeshUniqueID;
				if (Mesh->GetAttributeType() == FbxNodeAttribute::eMesh)
				{
					MeshUniqueID = TEXT("Mesh");
				}
				else if (Mesh->GetAttributeType() == FbxNodeAttribute::eShape)
				{
					MeshUniqueID = TEXT("Shape");
				}

				return GetNodeAttributeUniqueID(Mesh, MeshUniqueID);
			}

			FString FFbxHelper::GetNodeAttributeName(FbxNodeAttribute* NodeAttribute, const FStringView DefaultNamePrefix)
			{
				FString NodeAttributeName = FFbxHelper::GetFbxObjectName(NodeAttribute);
				if (NodeAttributeName.IsEmpty())
				{
					if (NodeAttribute->GetNodeCount() > 0)
					{
						NodeAttributeName = FString(DefaultNamePrefix) + TEXT("_") + FFbxHelper::GetFbxObjectName(NodeAttribute->GetNode(0));
					}
					else
					{
						uint64 UniqueFbxObjectID = NodeAttribute->GetUniqueID();
						NodeAttributeName += GetUniqueIDString(UniqueFbxObjectID);
					}
				}
				return NodeAttributeName;
			}

			FString FFbxHelper::GetNodeAttributeUniqueID(FbxNodeAttribute* NodeAttribute, const FStringView Prefix)
			{
				if (!NodeAttribute)
				{
					return {};
				}

				FString NodeAttributeUniqueID = FString(TEXT("\\")) + Prefix + TEXT("\\");
				FString NodeAttributeName = FFbxHelper::GetFbxObjectName(NodeAttribute);

				if (NodeAttributeName.IsEmpty())
				{
					if (NodeAttribute->GetNodeCount() > 0)
					{
						NodeAttributeName = FFbxHelper::GetFbxNodeHierarchyName(NodeAttribute->GetNode(0));
					}
					else
					{
						NodeAttributeName = GetNodeAttributeName(NodeAttribute, Prefix);
					}
				}

				NodeAttributeUniqueID += NodeAttributeName;

				return NodeAttributeUniqueID;
			}

			FString FFbxHelper::GetFbxPropertyName(const FbxProperty& Property)
			{
				FString PropertyName = FFbxConvert::MakeName(Property.GetName());
				if (PropertyName.Equals(TEXT("none"), ESearchCase::IgnoreCase))
				{
					//Replace None by Null because None clash with NAME_None and the create asset will instead call the object ClassName_X
					PropertyName = TEXT("Null");
				}
				return PropertyName;
			}

			FString FFbxHelper::GetFbxObjectName(const FbxObject* Object)
			{
				if (!Object)
				{
					return FString();
				}
				FString ObjName = FFbxConvert::MakeName(Object->GetName());
				if (ObjName.Equals(TEXT("none"), ESearchCase::IgnoreCase))
				{
					//Replace None by Null because None clash with NAME_None and the create asset will instead call the object ClassName_X
					ObjName = TEXT("Null");
				}
				return ObjName;
			}

			FString FFbxHelper::GetFbxNodeHierarchyName(const FbxNode* Node)
			{
				if (!Node)
				{
					return FString();
				}
				TArray<FString> UniqueIDTokens;
				const FbxNode* ParentNode = Node;
				while (ParentNode)
				{
					UniqueIDTokens.Add(GetFbxObjectName(ParentNode));
					ParentNode = ParentNode->GetParent();
				}
				FString UniqueID;
				for (int32 TokenIndex = UniqueIDTokens.Num() - 1; TokenIndex >= 0; TokenIndex--)
				{
					UniqueID += UniqueIDTokens[TokenIndex];
					if (TokenIndex > 0)
					{
						UniqueID += TEXT(".");
					}
				}
				return UniqueID;
			}

			FString FFbxHelper::GetUniqueIDString(const uint64 UniqueID)
			{
				FStringFormatNamedArguments FormatArguments;
				FormatArguments.Add(TEXT("UniqueID"), UniqueID);
				return FString::Format(TEXT("{UniqueID}"), FormatArguments);
			}
		}//ns Private
	}//ns Interchange
}//ns UE
