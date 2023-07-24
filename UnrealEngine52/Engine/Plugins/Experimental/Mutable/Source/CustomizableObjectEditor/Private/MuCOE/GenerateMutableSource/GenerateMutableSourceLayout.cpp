// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceLayout.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::NodeLayoutPtr GenerateMutableSourceLayout(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceLayout), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeLayout*>(Generated->Node.get());
	}

	mu::NodeLayoutPtr Result;
	
	if (const UCustomizableObjectNodeLayoutBlocks* TypedNodeBlocks = Cast<UCustomizableObjectNodeLayoutBlocks>(Node))
	{
		if (UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(FollowOutputPin(*TypedNodeBlocks->OutputPin())->GetOwningNode()))
		{
			int32 LayoutIndex;
			FString MaterialName;

			if (!SkeletalMeshNode->CheckIsValidLayout(Pin, LayoutIndex, MaterialName))
			{
				FString msg = "Layouts ";
				for (int32 i = 0; i < LayoutIndex; ++i)
				{
					msg += "UV" + FString::FromInt(i);
					if (i < LayoutIndex - 1)
					{
						msg += ", ";
					}
				}
				msg += " of " + MaterialName + " must be also connected to a Layout Blocks Node. ";
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Error);
				return nullptr;
			}
		}

		mu::NodeLayoutBlocksPtr LayoutNode = new mu::NodeLayoutBlocks;
		Result = LayoutNode;

		LayoutNode->SetGridSize(TypedNodeBlocks->Layout->GetGridSize().X, TypedNodeBlocks->Layout->GetGridSize().Y);
		LayoutNode->SetMaxGridSize(TypedNodeBlocks->Layout->GetMaxGridSize().X, TypedNodeBlocks->Layout->GetMaxGridSize().Y);
		LayoutNode->SetBlockCount(TypedNodeBlocks->Layout->Blocks.Num() ? TypedNodeBlocks->Layout->Blocks.Num() : 1);

		mu::EPackStrategy strategy = mu::EPackStrategy::RESIZABLE_LAYOUT;

		switch (TypedNodeBlocks->Layout->GetPackingStrategy())
		{
		case ECustomizableObjectTextureLayoutPackingStrategy::Resizable:
			strategy = mu::EPackStrategy::RESIZABLE_LAYOUT;
			break;
		case ECustomizableObjectTextureLayoutPackingStrategy::Fixed:
			strategy = mu::EPackStrategy::FIXED_LAYOUT;
			break;
		default:
			break;
		}

		LayoutNode->SetLayoutPackingStrategy(strategy);

		if (TypedNodeBlocks->Layout->Blocks.Num())
		{
			for (int BlockIndex = 0; BlockIndex < TypedNodeBlocks->Layout->Blocks.Num(); ++BlockIndex)
			{
				LayoutNode->SetBlock(BlockIndex,
					TypedNodeBlocks->Layout->Blocks[BlockIndex].Min.X,
					TypedNodeBlocks->Layout->Blocks[BlockIndex].Min.Y,
					TypedNodeBlocks->Layout->Blocks[BlockIndex].Max.X - TypedNodeBlocks->Layout->Blocks[BlockIndex].Min.X,
					TypedNodeBlocks->Layout->Blocks[BlockIndex].Max.Y - TypedNodeBlocks->Layout->Blocks[BlockIndex].Min.Y);

				LayoutNode->SetBlockPriority(BlockIndex, TypedNodeBlocks->Layout->Blocks[BlockIndex].Priority);
			}
		}
		else
		{
			FString msg = "Layout without any block found. A grid sized block will be used instead.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Warning);

			LayoutNode->SetBlock(0, 0, 0, TypedNodeBlocks->Layout->GetGridSize().X, TypedNodeBlocks->Layout->GetGridSize().Y);
			LayoutNode->SetBlockPriority(0, 0);
		}
	}
	
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	
	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

