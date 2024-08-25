// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"

#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"


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

	ECustomizableObjectAutomaticLODStrategy LODStrategy = ECustomizableObjectAutomaticLODStrategy::Inherited;

	// Iterate backwards, from the Root CO to the parent CO, to propagate the LODStrategy. 
	for (int32 Index = ParentObjectNodes.Num() - 1; Index > -1; --Index)
	{
		const UCustomizableObjectNodeObject* ParentObjectNode = ParentObjectNodes[Index];
		
		if (ParentObjectNode->AutoLODStrategy != ECustomizableObjectAutomaticLODStrategy::Inherited)
		{
			LODStrategy = ParentObjectNode->AutoLODStrategy;
		}
		
		// When using AutomaticFromMesh find all materials within range [0..LOD].
		int32 LODIndex = LODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh ? 0 : LOD;

		// If LODStrategy is set to AutomaticFromMesh, find MaterialNodes belonging to lower LODs.
		for (; LODIndex <= LOD; ++LODIndex)
		{
			const TArray<UCustomizableObjectNodeMaterial*> MaterialNodes = ParentObjectNode->GetMaterialNodes(LODIndex);

			for (UCustomizableObjectNodeMaterial* MaterialNode : MaterialNodes)
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
