// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectLayout.h"

#include "Engine/StaticMesh.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuT/NodeLayout.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

UCustomizableObjectLayout::UCustomizableObjectLayout()
{
	GridSize = FIntPoint(4, 4);
	MaxGridSize = FIntPoint(4, 4);

	FCustomizableObjectLayoutBlock Block;
	Block.Min = FIntPoint(0, 0);
	Block.Max = FIntPoint(4, 4);
	Block.Id = FGuid::NewGuid();
	Block.Priority = 0;
	Block.bUseSymmetry = false;
	Blocks.Add(Block);

	PackingStrategy = ECustomizableObjectTextureLayoutPackingStrategy::Resizable;
	BlockReductionMethod = ECustomizableObjectLayoutBlockReductionMethod::Halve;
}


void UCustomizableObjectLayout::SetLayout(UObject* InMesh, int32 LODIndex, int32 MatIndex, int32 UVIndex)
{
	Mesh = InMesh;
	LOD = LODIndex;
	Material = MatIndex;
	UVChannel = UVIndex;
}


void UCustomizableObjectLayout::SetPackingStrategy(ECustomizableObjectTextureLayoutPackingStrategy Strategy)
{
	PackingStrategy = Strategy;
}


void UCustomizableObjectLayout::SetGridSize(FIntPoint Size)
{
	GridSize = Size;
}


void UCustomizableObjectLayout::SetMaxGridSize(FIntPoint Size)
{
	MaxGridSize = Size;
}


void UCustomizableObjectLayout::SetLayoutName(FString Name)
{
	LayoutName = Name;
}


void UCustomizableObjectLayout::GenerateBlocksFromUVs()
{
	mu::NodeLayoutBlocksPtr Layout = nullptr;

	UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(GetOuter());

	if (Node && Mesh)
	{
		//Creating a GenerationContext
		TUniquePtr<FCustomizableObjectCompiler> Compiler( new FCustomizableObjectCompiler() );
		UCustomizableObject* Object = Node->GetGraphEditor()->GetCustomizableObject();
		FCompilationOptions Options = Object->CompileOptions;
	
		FMutableGraphGenerationContext GenerationContext(Object, Compiler.Get(), Options);
	
		//Transforming skeletalmesh to mutable mesh
		mu::MeshPtr	MutableMesh = nullptr;
		
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
		{
			// We don't need all the data to generate the blocks
			const EMutableMeshConversionFlags ShapeFlags = 
					EMutableMeshConversionFlags::IgnoreSkinning | 
					EMutableMeshConversionFlags::IgnorePhysics;

			GenerationContext.MeshGenerationFlags.Push(ShapeFlags);

			GenerationContext.ComponentInfos.Add(SkeletalMesh);
			MutableMesh = ConvertSkeletalMeshToMutable(SkeletalMesh, TSoftClassPtr<UAnimInstance>(), LOD, Material, LOD, Material, GenerationContext, Node);
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
		{
			MutableMesh = ConvertStaticMeshToMutable(StaticMesh, LOD, Material, GenerationContext, Node);
		}
	
		if (MutableMesh)
		{
			// Generating blocks with the mutable mesh
			Layout = mu::NodeLayoutBlocks::GenerateLayoutBlocks(MutableMesh, UVChannel, GridSize.X, GridSize.Y);
		}
	
		Compiler.Reset();
	
		if (Layout)
		{
			Blocks.Empty();
		
			// Generating the layout blocks with the mutable layout
			for (int i = 0; i < Layout->GetBlockCount(); ++i)
			{
				uint16 minX, minY, sizeX, sizeY;
		
				Layout->GetLayout()->GetBlock(i, &minX, &minY, &sizeX, &sizeY);
		
				FCustomizableObjectLayoutBlock block;
				block.Min = FIntPoint(minX, minY);
				block.Max = FIntPoint(minX + sizeX, minY + sizeY);
				block.Id = FGuid::NewGuid();
				block.Priority = 0;
				block.bUseSymmetry = false;
				Blocks.Add(block);
			}
		
			Node->PostEditChange();
			Node->GetGraph()->MarkPackageDirty();
		}
	}
}


void UCustomizableObjectLayout::GetUVChannel(TArray<FVector2f>& UVs, int32 UVChannelIndex) const
{
	if (UObject* Node = GetOuter())
	{
		if (const UCustomizableObjectNodeLayoutBlocks* TypedNodeLayout = Cast<UCustomizableObjectNodeLayoutBlocks>(Node))
		{
			if (const UEdGraphPin* ConnectedPin = FollowOutputPin(*TypedNodeLayout->OutputPin()))
			{
				if (const UCustomizableObjectNodeMesh* MeshNode = Cast<UCustomizableObjectNodeMesh>(ConnectedPin->GetOwningNode()))
				{
					MeshNode->GetUVChannelForPin(ConnectedPin, UVs, UVChannelIndex);
				}
			}
		}
		else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
		{
			TypedNodeTable->GetUVChannel(this, UVs);
		}
	}
}


int32 UCustomizableObjectLayout::FindBlock(const FGuid& InId) const
{
	for (int Index = 0; Index < Blocks.Num(); ++Index)
	{
		if (Blocks[Index].Id == InId)
		{
			return Index;
		}
	}

	return -1;
}


void UCustomizableObjectLayout::SetIgnoreVertexLayoutWarnings(bool bValue)
{
	bIgnoreUnassignedVertexWarning = bValue;
}


void UCustomizableObjectLayout::SetIgnoreWarningsLOD(int32 LODValue)
{
	FirstLODToIgnore = LODValue;
}


void UCustomizableObjectLayout::SetBlockReductionMethod(ECustomizableObjectLayoutBlockReductionMethod Method)
{
	BlockReductionMethod = Method;
}
#undef LOCTEXT_NAMESPACE
