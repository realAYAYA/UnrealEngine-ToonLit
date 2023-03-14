// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"

#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "Templates/Casts.h"


UCustomizableObjectNodeMaterial* FCustomizableObjectNodeParentedMaterial::GetParentMaterialNode() const
{
	if (UCustomizableObjectNode* Node = GetParentNode())
	{
		return CastChecked<UCustomizableObjectNodeMaterial>(Node);
	}
	else
	{
		return nullptr;	
	}
}

TArray<UCustomizableObjectNodeMaterial*> FCustomizableObjectNodeParentedMaterial::GetPossibleParentMaterialNodes() const
{
	TArray<UCustomizableObjectNodeMaterial*> Result;
	
	const UCustomizableObjectNode& Node = GetNode();
	const int32 LOD = Node.GetLOD();

	if (LOD == -1)
	{
		return Result; // Early exit.
	}
	
	TArray<UCustomizableObjectNodeObject*> ParentObjectNodes = Node.GetParentObjectNodes(LOD);
	for (int32 Index = 1; Index < ParentObjectNodes.Num(); ++Index) // Skip first Object node since we do not support directly material siblings.
	{
		const UCustomizableObjectNodeObject* ParentObjectNode = ParentObjectNodes[Index];
	
		for (UCustomizableObjectNodeMaterial* MaterialNode : ParentObjectNode->GetMaterialNodes(LOD))
		{
			if (UCustomizableObjectNodeCopyMaterial* TypedCopyMaterialNode = Cast<UCustomizableObjectNodeCopyMaterial>(MaterialNode))
			{
				if (TypedCopyMaterialNode->GetMaterialNode())
				{
					Result.Add(TypedCopyMaterialNode);
				}
			}
			else if (UCustomizableObjectNodeMaterial* TypedMaterialNode = Cast<UCustomizableObjectNodeMaterial>(MaterialNode))
			{
				Result.Add(TypedMaterialNode);
			}
		}
	}
	
	return Result;
}


UCustomizableObjectNodeMaterial* FCustomizableObjectNodeParentedMaterial::GetParentMaterialNodeIfPath() const
{
	UCustomizableObjectNodeMaterial* ParentMaterialNode = GetParentMaterialNode();
	
	if (GetPossibleParentMaterialNodes().Contains(ParentMaterialNode)) // There is a path to the parent material
	{
		return ParentMaterialNode;
	}
	else
	{
		return nullptr;
	}
}


const UCustomizableObjectNode& FCustomizableObjectNodeParentedMaterial::GetNode() const
{
	return const_cast<FCustomizableObjectNodeParentedMaterial*>(this)->GetNode();
}
