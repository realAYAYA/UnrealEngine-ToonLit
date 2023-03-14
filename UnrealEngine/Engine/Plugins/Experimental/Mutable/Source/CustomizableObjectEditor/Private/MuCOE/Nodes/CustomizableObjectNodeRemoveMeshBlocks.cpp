// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocks.h"

#include "EdGraph/EdGraph.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeRemoveMeshBlocks::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();
	
	// Convert deprecated node index list to the node id list.
	if (GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PostLoadToCustomVersion
		&& BlockIds.Num() < Blocks_DEPRECATED.Num())
	{
		if (UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode())
		{
			TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

			if (!Layouts.IsValidIndex(ParentLayoutIndex))
			{
				UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid texture layout index %d. Parent node has %d layouts."),
					*GetOutermost()->GetName(), ParentLayoutIndex, Layouts.Num());
			}
			else if (UCustomizableObjectNodeMaterial* ParentMaterial = Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
			{
				UCustomizableObjectLayout* Layout = Layouts[ParentLayoutIndex];

				for (int IndexIndex = BlockIds.Num(); IndexIndex < Blocks_DEPRECATED.Num(); ++IndexIndex)
				{
					const int BlockIndex = Blocks_DEPRECATED[IndexIndex];
					if (!Layout->Blocks.IsValidIndex(BlockIndex))
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an invalid layout block index %d. Parent node has %d blocks."),
							*GetOutermost()->GetName(), BlockIndex, Layout->Blocks.Num());

						continue;
					}
					
					const FGuid Id = Layout->Blocks[BlockIndex].Id;
					if (!Id.IsValid())
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeRemoveMeshBlocks refers to an valid layout block %d but that block doesn't have an id."),
							*GetOutermost()->GetName(), BlockIndex);

						continue;
					}

					BlockIds.Add(Id);
				}
			}
		}
	}
}


void UCustomizableObjectNodeRemoveMeshBlocks::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


void UCustomizableObjectNodeRemoveMeshBlocks::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged 
		&& (PropertyThatChanged->GetName() == TEXT("ParentMaterialObject") || PropertyThatChanged->GetName() == TEXT("ParentLayoutIndex")))
	{
		// Reset selected blocks
		BlockIds.Empty();
		
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UCustomizableObjectNodeRemoveMeshBlocks::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName("Material"));
}


FText UCustomizableObjectNodeRemoveMeshBlocks::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Remove_Mesh_Blocks", "Remove Mesh Blocks");
}


FLinearColor UCustomizableObjectNodeRemoveMeshBlocks::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


void UCustomizableObjectNodeRemoveMeshBlocks::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin == OutputPin())
	{
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
		}
	}
}

bool UCustomizableObjectNodeRemoveMeshBlocks::IsNodeOutDatedAndNeedsRefresh()
{
	bool Result = false;

	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode();
	if (ParentMaterialNode)
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (!Layouts.IsValidIndex(ParentLayoutIndex))
		{
			Result = true;
		}
		else
		{
			UCustomizableObjectLayout* Layout = Layouts[ParentLayoutIndex];
				
			if (BlockIds.Num() > Layout->Blocks.Num())
			{
				Result = true;
			}
		}
	}

	// Remove previous compilation warnings
	if (!Result && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

	return Result;
}

FString UCustomizableObjectNodeRemoveMeshBlocks::GetRefreshMessage() const
{
	return "Source Layout has changed, layout blocks might have changed. Please Refresh Node to reflect those changes.";
}


FText UCustomizableObjectNodeRemoveMeshBlocks::GetTooltipText() const
{
	return LOCTEXT("Remove_Mesh_Blocks_Tooltip", "Remove all the geometry in the chosen layout blocks from a material.");
}


bool UCustomizableObjectNodeRemoveMeshBlocks::IsSingleOutputNode() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
