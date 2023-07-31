// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"

#include "Containers/ArrayView.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Math/Vector2D.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class UCustomizableObjectNodeRemapPins;
class UObject;
struct FPropertyChangedEvent;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeLayoutBlocks::UCustomizableObjectNodeLayoutBlocks()
	: Super()
{
	Layout = CreateDefaultSubobject<UCustomizableObjectLayout>(FName("CustomizableObjectLayout"));
}


void UCustomizableObjectNodeLayoutBlocks::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	if (UEdGraphPin* Pin = OutputPin())
	{
		LinkPostEditChangePropertyDelegate(Pin);
	}

	// Make sure we have IDs for all blocks. Generate fake ids for old blocks so that it is deterministic
	// and new references to these blocks work even if this node is not saved.
	for (int BlockIndex=0; BlockIndex< Layout->Blocks.Num(); ++BlockIndex)
	{
		FCustomizableObjectLayoutBlock& Block = Layout->Blocks[BlockIndex];
		if (!Block.Id.IsValid())
		{
			FGuid FakeId;
			FakeId[0] = BlockIndex + 1;
			FakeId[1] = BlockIndex + 1;
			FakeId[2] = BlockIndex + 1;
			FakeId[3] = BlockIndex + 1;
			Block.Id = FakeId;
		}
	}
}


void UCustomizableObjectNodeLayoutBlocks::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Layout, FName("Layout"));
	OutputPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeLayoutBlocks::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Mesh_Layout", "Mesh Layout");
}


FLinearColor UCustomizableObjectNodeLayoutBlocks::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Layout);
}


void UCustomizableObjectNodeLayoutBlocks::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Layout && Pin == OutputPin())
	{
		LinkPostEditChangePropertyDelegate(Pin);
	}
}


void UCustomizableObjectNodeLayoutBlocks::AddAttachedErrorData(const FAttachedErrorDataView& AttachedErrorData)
{
	if (Layout)
	{
		Layout->UnassignedUVs.Emplace();

		const int32 UnassignedUVsCount = AttachedErrorData.UnassignedUVs.Num() / 2;
		for (int32 Index = 0; Index < UnassignedUVsCount; ++Index)
		{
			Layout->UnassignedUVs.Last().Emplace(AttachedErrorData.UnassignedUVs[Index * 2 + 0],
				AttachedErrorData.UnassignedUVs[Index * 2 + 1]);
		}
	}
}


void UCustomizableObjectNodeLayoutBlocks::ResetAttachedErrorData()
{
	if (Layout)
	{
		Layout->UnassignedUVs.Empty();
	}
}


FText UCustomizableObjectNodeLayoutBlocks::GetTooltipText() const
{
	return LOCTEXT("Layout_Block_Tooltip", "Defines of how the material UV layout is cut into logical pieces.\nWhen destined to a Material Node, it also defines the resolution per layout block of that material for any extension that is made to that material.");

}


bool UCustomizableObjectNodeLayoutBlocks::IsSingleOutputNode() const
{
	return true;
}


void UCustomizableObjectNodeLayoutBlocks::MeshPostEditChangeProperty(UObject* Node, FPropertyChangedEvent& FPropertyChangedEvent)
{
	SetLayoutSkeletalMesh();
}


void UCustomizableObjectNodeLayoutBlocks::LinkPostEditChangePropertyDelegate(UEdGraphPin* Pin)
{
	if (LastMeshNodeConnected.IsValid())
	{
		LastMeshNodeConnected->PostEditChangePropertyRegularDelegate.AddUObject(this, &UCustomizableObjectNodeLayoutBlocks::MeshPostEditChangeProperty);
	}

	if (const UEdGraphPin* ConnectedPin = FollowOutputPin(*Pin))
	{
		UEdGraphNode* MeshNode = ConnectedPin->GetOwningNode();

		if (MeshNode->IsA(UCustomizableObjectNodeStaticMesh::StaticClass()) || MeshNode->IsA(UCustomizableObjectNodeSkeletalMesh::StaticClass()))
		{
			LastMeshNodeConnected = Cast<UCustomizableObjectNode>(MeshNode);
			LastMeshNodeConnected->PostEditChangePropertyRegularDelegate.AddUObject(this, &UCustomizableObjectNodeLayoutBlocks::MeshPostEditChangeProperty);

			SetLayoutSkeletalMesh();
		}
	}
}


void UCustomizableObjectNodeLayoutBlocks::SetLayoutSkeletalMesh()
{
	if (const UEdGraphPin* ConnectedPin = FollowOutputPin(*OutputPin()))
	{
		if (const UCustomizableObjectNodeMesh* MeshNode = Cast<UCustomizableObjectNodeMesh>(LastMeshNodeConnected))
		{
			int32 LODIndex;
			int32 SectionIndex;
			int32 LayoutIndex;
			MeshNode->GetPinSection(*ConnectedPin, LODIndex, SectionIndex, LayoutIndex);

			Layout->SetLayout(MeshNode->GetMesh(), LODIndex, SectionIndex, LayoutIndex);
		}
	}
}


void UCustomizableObjectNodeLayoutBlocks::PostPasteNode()
{
	Super::PostPasteNode();
	
	Layout->SetLayout(nullptr, -1, -1, -1);
}


void UCustomizableObjectNodeLayoutBlocks::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::LayoutClassAdded)
	{
		Layout->SetGridSize(GridSize_DEPRECATED);
		Layout->SetMaxGridSize(MaxGridSize_DEPRECATED);
		Layout->Blocks = Blocks_DEPRECATED;
		Layout->SetPackingStrategy(PackingStrategy_DEPRECATED);

		if (Layout->GetGridSize() == FIntPoint::ZeroValue)
		{
			Layout->SetGridSize(FIntPoint(4));
		}

		if(Layout->GetMaxGridSize() == FIntPoint::ZeroValue)
		{
			Layout->SetMaxGridSize(FIntPoint(4));
		}
	}
}

#undef LOCTEXT_NAMESPACE

