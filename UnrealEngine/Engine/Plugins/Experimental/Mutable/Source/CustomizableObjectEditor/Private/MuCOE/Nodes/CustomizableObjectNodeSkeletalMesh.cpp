// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"

#include "AssetThumbnail.h"
#include "ISinglePropertyView.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByNameDefaultPin.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Widgets/Input/SCheckBox.h"

class UCustomizableObjectNodeRemapPinsByName;
class UObject;
struct FSlateBrush;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


/** Default node pin configuration pin name (node does not have an skeletal mesh). */
static const TCHAR* SKELETAL_MESH_PIN_NAME = TEXT("Skeletal Mesh");


void UCustomizableObjectNodeSkeletalMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("SkeletalMesh"))
	{
		ReconstructNode();
	}
}


void UCustomizableObjectNodeSkeletalMesh::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	// Pass information to the remap pins action context
	if (UCustomizableObjectNodeRemapPinsByNameDefaultPin* RemapPinsCustom = Cast<UCustomizableObjectNodeRemapPinsByNameDefaultPin>(RemapPins))
	{
		RemapPinsCustom->DefaultPin = DefaultPin.Get();
	}
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (!SkeletalMesh)
	{
		UCustomizableObjectNodeSkeletalMeshPinDataMesh* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(this);
		PinData->Init(-1, -1);
		
		DefaultPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(SKELETAL_MESH_PIN_NAME), PinData);
		return;
	}
	else
	{
		DefaultPin = {};
	}
	
	if (const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel())
	{
		const int32 NumLODs = SkeletalMesh->GetLODInfoArray().Num();
		for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			const int32 NumSections = ImportedModel->LODModels[LODIndex].Sections.Num();
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				// Ignore disabled sections.
				const bool bIsSectionDisabled = ImportedModel ->LODModels[LODIndex].Sections[SectionIndex].bDisabled;
				if (bIsSectionDisabled)
				{
					continue;
				}

				UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(LODIndex, SectionIndex, ImportedModel);
				
				FString SectionFriendlyName = MaterialInterface ? MaterialInterface->GetName() : FString::Printf(TEXT("Section %i"), SectionIndex);

				// Mesh
				{
					FString MeshName = FString::Printf(TEXT("LOD %i - Section %i - Mesh"), LODIndex, SectionIndex);

					UCustomizableObjectNodeSkeletalMeshPinDataMesh* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(this);
					PinData->Init(LODIndex, SectionIndex);
					
					UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*MeshName), PinData);
					Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("LOD %i - %s - Mesh"), LODIndex, *SectionFriendlyName));
					Pin->PinToolTip = MeshName;
				}
				
				// Layout
				{
					if (ImportedModel->LODModels.Num() > LODIndex)
					{
						const uint32 NumUVs = ImportedModel->LODModels[LODIndex].NumTexCoords;
						for (uint32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex)
						{
							FString PinName = FString::Printf(TEXT("LOD %i - Section %i - UV %i"), LODIndex, SectionIndex, UVIndex);
							
							UCustomizableObjectNodeSkeletalMeshPinDataLayout* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(this);
							PinData->Init(LODIndex, SectionIndex, UVIndex);
							
							UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, Schema->PC_Layout, FName(*PinName), PinData);
							Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("LOD %i - %s - UV %i"), LODIndex, *SectionFriendlyName, UVIndex));
							Pin->PinToolTip = PinName;
						}
					}
				}

				// Images
				if (MaterialInterface)
				{
					const UMaterial* Material = MaterialInterface->GetMaterial();

					TArray<FMaterialParameterInfo> ImageInfos;
					TArray<FGuid> ImageIds;
					Material->GetAllTextureParameterInfo(ImageInfos, ImageIds);

					check(ImageInfos.Num() == ImageIds.Num())
					for (int32 Index = 0; Index < ImageInfos.Num(); ++Index)
					{
						const FMaterialParameterInfo& ImageInfo = ImageInfos[Index];
						FGuid& ImageId = ImageIds[Index];
						
						FString ImageNameStr = *ImageInfo.Name.ToString();
						FString PinName = FString::Printf(TEXT("LOD %i - Section %i - Texture Parameter %s"), LODIndex, SectionIndex, *ImageNameStr);
						
						UCustomizableObjectNodeSkeletalMeshPinDataImage* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataImage>(this);
						PinData->Init(LODIndex, SectionIndex, ImageId);
						
						UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName), PinData);
						Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("LOD %i - %s - %s"), LODIndex, *SectionFriendlyName, *ImageNameStr));
						Pin->PinToolTip = PinName;
						Pin->bHidden = true;
					}
				}
			}
		}
	}
}


UCustomizableObjectNodeRemapPins* UCustomizableObjectNodeSkeletalMesh::CreateRemapPinsDefault() const
{
	return NewObject<UCustomizableObjectNodeSkeletalMeshRemapPinsBySection>();
}


FText UCustomizableObjectNodeSkeletalMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (SkeletalMesh)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MeshName"), FText::FromString(SkeletalMesh->GetName()));

		return FText::Format(LOCTEXT("SkeletalMesh_Title", "{MeshName}\nSkeletal Mesh"), Args);
	}
	else
	{
		return LOCTEXT("Skeletal_Mesh", "Skeletal Mesh");
	}
}


FLinearColor UCustomizableObjectNodeSkeletalMesh::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


UTexture2D* UCustomizableObjectNodeSkeletalMesh::FindTextureForPin(const UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return nullptr;
	}
	
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
	if (!ImportedModel)
	{
		return nullptr;
	}	

	if (const UCustomizableObjectNodeSkeletalMeshPinDataImage* PinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataImage>(GetPinData(*Pin)))
	{
		if (const UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(PinData->GetLODIndex(), PinData->GetSectionIndex(), ImportedModel))
		{
			const UMaterialInterface* Material = GetMaterialFor(Pin);
			
			TArray<FGuid> ParameterIds;
			TArray<FMaterialParameterInfo> ParameterInfo;
			Material->GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfo, ParameterIds);

			check(ParameterIds.Num() == ParameterInfo.Num())
			for (int32 Index = 0; Index < ParameterIds.Num(); ++Index)
			{
				if (ParameterIds[Index] == PinData->GetTextureParameterId())
				{
					UTexture* Texture = nullptr;
					MaterialInterface->GetTextureParameterValue(ParameterInfo[Index].Name, Texture);

					return Cast<UTexture2D>(Texture);
				}
			}
		}
	}
	
	return nullptr;
}


void UCustomizableObjectNodeSkeletalMesh::GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const
{
	check(Pin);
	
	if (!SkeletalMesh)
	{
		return;
	}

	int32 LODIndex;
	int32 SectionIndex;
	int32 LayoutIndex;
	GetPinSection(*Pin, LODIndex, SectionIndex, LayoutIndex);

	OutSegments = GetUV(*SkeletalMesh, LODIndex, SectionIndex, UVIndex);
}


TArray<UCustomizableObjectLayout*> UCustomizableObjectNodeSkeletalMesh::GetLayouts(const UEdGraphPin& MeshPin) const
{
	TArray<UCustomizableObjectLayout*> Result;

	const UCustomizableObjectNodeSkeletalMeshPinDataMesh* MeshPinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(GetPinData(MeshPin));
	check(MeshPinData); // Not a mesh pin

	for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (const UCustomizableObjectNodeSkeletalMeshPinDataLayout* PinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(GetPinData(*Pin)))
		{
			if (PinData->GetLODIndex() == MeshPinData->GetLODIndex() &&
				PinData->GetSectionIndex() == MeshPinData->GetSectionIndex())
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Pin))
				{
					const UCustomizableObjectNodeLayoutBlocks* LayoutNode = Cast<UCustomizableObjectNodeLayoutBlocks>(ConnectedPin->GetOwningNode());
					if (LayoutNode && LayoutNode->Layout)
					{
						Result.Add(LayoutNode->Layout);
					}
				}
			}
		}
	}

	return Result;
}


UObject* UCustomizableObjectNodeSkeletalMesh::GetMesh() const
{
	return SkeletalMesh;
}


UEdGraphPin* UCustomizableObjectNodeSkeletalMesh::GetMeshPin(const int32 LODIndex, const int32 SectionIndex) const
{
	for (UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (const UCustomizableObjectNodeSkeletalMeshPinDataMesh* PinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(GetPinData(*Pin)))
		{
			if (PinData->GetLODIndex() == LODIndex &&
				PinData->GetSectionIndex() == SectionIndex)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}


UEdGraphPin* UCustomizableObjectNodeSkeletalMesh::GetLayoutPin(int32 LODIndex, int32 SectionIndex, int32 LayoutIndex) const
{
	for (UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (const UCustomizableObjectNodeSkeletalMeshPinDataLayout* PinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(GetPinData(*Pin)))
		{
			if (PinData->GetLODIndex() == LODIndex &&
				PinData->GetSectionIndex() == SectionIndex &&
				PinData->GetUVIndex() == LayoutIndex)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}


void UCustomizableObjectNodeSkeletalMesh::GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const
{
	if (const UCustomizableObjectNodeSkeletalMeshPinDataSection* PinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataSection>(GetPinData(Pin)))
	{
		OutLODIndex = PinData->GetLODIndex();
		OutSectionIndex = PinData->GetSectionIndex();

		if (const UCustomizableObjectNodeSkeletalMeshPinDataLayout* LayoutPinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(GetPinData(Pin)))
		{
			OutLayoutIndex = LayoutPinData->GetUVIndex();
		}
		else
		{
			OutLayoutIndex = -1;
		}
		
		return;
	}
	
	OutLODIndex = -1;
	OutSectionIndex = -1;
	OutLayoutIndex = -1;
}


UMaterialInterface* UCustomizableObjectNodeSkeletalMesh::GetMaterialFor(const UEdGraphPin* Pin) const
{
	if (SkeletalMesh)
	{
		if (FSkeletalMaterial* SkeletalMaterial = GetSkeletalMaterialFor(*Pin))
		{
			return SkeletalMaterial->MaterialInterface;
		}
	}

	return nullptr;
}


FSkeletalMaterial* UCustomizableObjectNodeSkeletalMesh::GetSkeletalMaterialFor(const UEdGraphPin& Pin) const
{
	int32 LODIndex;
	int32 SectionIndex;
	int32 LayoutIndex;
	GetPinSection(Pin, LODIndex, SectionIndex, LayoutIndex);

	return GetSkeletalMaterialFor(LODIndex, SectionIndex);
}


bool UCustomizableObjectNodeSkeletalMesh::IsPinRelevant(const UEdGraphPin* Pin) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		return Pin->PinType.PinCategory == Schema->PC_Layout;
	}

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{	
		return Pin->PinType.PinCategory == Schema->PC_Mesh;
	}

	return false;
}


bool UCustomizableObjectNodeSkeletalMesh::IsNodeOutDatedAndNeedsRefresh()
{
	const bool bOutdated = [&]()
	{
		if (!SkeletalMesh)
		{
			return false;
		}

		const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		if (!ImportedModel)
		{
			return false;
		}
	
		for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			auto Connected = [](const UEdGraphPin& Pin)
			{
				return Pin.Direction == EGPD_Input ? FollowInputPin(Pin) != nullptr : !FollowOutputPinArray(Pin).IsEmpty();	
			};
			
			auto OutdatedSectionPinData = [&](const UCustomizableObjectNodeSkeletalMeshPinDataSection& PinData) -> bool
			{
				return !ImportedModel->LODModels.IsValidIndex(PinData.GetLODIndex()) ||
					!ImportedModel->LODModels[PinData.GetLODIndex()].Sections.IsValidIndex(PinData.GetSectionIndex()) ||
					ImportedModel->LODModels[PinData.GetLODIndex()].Sections[PinData.GetSectionIndex()].bDisabled;
			};
		
			if (const UCustomizableObjectNodeSkeletalMeshPinDataLayout* LayoutPinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(GetPinData(*Pin)))
			{
				if (Connected(*Pin) &&
					(OutdatedSectionPinData(*LayoutPinData) ||
					LayoutPinData->GetUVIndex() < 0 || LayoutPinData->GetUVIndex() >= static_cast<int32>(ImportedModel->LODModels[LayoutPinData->GetLODIndex()].NumTexCoords)))
				{
					return true;
				}
			}
			else if (const UCustomizableObjectNodeSkeletalMeshPinDataMesh* MeshPinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(GetPinData(*Pin)))
			{
				if (Connected(*Pin) &&
					OutdatedSectionPinData(*MeshPinData))
				{
					return true;
				}
			}
			else if (const UCustomizableObjectNodeSkeletalMeshPinDataImage* ImagePinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataImage>(GetPinData(*Pin)))
			{				
				const UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(ImagePinData->GetLODIndex(), ImagePinData->GetSectionIndex(), ImportedModel);
				if (!MaterialInterface) // If we had an Image pin for sure we had a MaterialInstance.
				{
					return true;
				}

				TArray<FGuid> ParameterIds;
				TArray<FMaterialParameterInfo> ParameterInfo;
				MaterialInterface->GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfo, ParameterIds);
				
				UTexture* Texture = nullptr;
				if (Connected(*Pin) &&
					(OutdatedSectionPinData(*ImagePinData) ||
					!ParameterIds.Contains(ImagePinData->GetTextureParameterId()))) // Check that the Texture Parameter still exists.
				{
					return true; 
				}
			}
		}

		return false;
	}();
	
	// Remove previous compilation warnings
	if (!bOutdated && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

    return bOutdated;
}


FString UCustomizableObjectNodeSkeletalMesh::GetRefreshMessage() const
{
    return "Node data outdated. Please refresh node.";
}


FText UCustomizableObjectNodeSkeletalMesh::GetTooltipText() const
{
	return LOCTEXT("Skeletal_Mesh_Tooltip", "Get access to the sections (also known as material slots) of a skeletal mesh and to each of the sections texture parameters.");
}


void UCustomizableObjectNodeSkeletalMesh::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::PostLoadToCustomVersion)
	{
		for (FCustomizableObjectNodeSkeletalMeshLOD& LOD : LODs_DEPRECATED)
		{
			for(FCustomizableObjectNodeSkeletalMeshMaterial& Material : LOD.Materials)
			{
				if (Material.MeshPin_DEPRECATED && !Material.MeshPinRef.Get())
				{
					UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(Material.MeshPin_DEPRECATED);
					Material.MeshPinRef.SetPin(AuxPin);
				}

				if (!Material.LayoutPinsRef.Num())
				{
					if (Material.LayoutPins_DEPRECATED.Num())
					{
						for (UEdGraphPin_Deprecated* LayoutPin : Material.LayoutPins_DEPRECATED)
						{
							UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(LayoutPin);
							FEdGraphPinReference AuxEdGraphPinReference(AuxPin);
							Material.LayoutPinsRef.Add(AuxEdGraphPinReference);
						}
					}
					else
					{
						FString MaterialLayoutName = Material.Name + " Layout";
						for (UEdGraphPin* Pin : GetAllNonOrphanPins())
						{
							if (Pin
								&& Pin->Direction == EEdGraphPinDirection::EGPD_Input
								&& (MaterialLayoutName == Helper_GetPinName(Pin)
									|| MaterialLayoutName == Pin->PinFriendlyName.ToString()))
							{
								FEdGraphPinReference AuxEdGraphPinReference(Pin);
								Material.LayoutPinsRef.Add(AuxEdGraphPinReference);
								break;
							}
						}
					}
				}

				if (!Material.ImagePinsRef.Num())
				{
					for (UEdGraphPin_Deprecated* ImagePin : Material.ImagePins_DEPRECATED)
					{
						UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(ImagePin);
						FEdGraphPinReference AuxEdGraphPinReference(AuxPin);
						Material.ImagePinsRef.Add(AuxEdGraphPinReference);
					}
				}
			}
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::ConvertAnimationSlotToFName)
	{
		if (AnimBlueprintSlotName.IsNone() && AnimBlueprintSlot_DEPRECATED != -1)
		{
			AnimBlueprintSlotName = FName(FString::FromInt(AnimBlueprintSlot_DEPRECATED));
			AnimBlueprintSlot_DEPRECATED = -1; // Unnecessary, just in case anyone tried to use it later in this method.
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeSkeletalMesh)
	{
		for (int32 LODIndex = 0; LODIndex < LODs_DEPRECATED.Num(); ++LODIndex)
		{
			const FCustomizableObjectNodeSkeletalMeshLOD& LOD = LODs_DEPRECATED[LODIndex];
			
			for (int32 SectionIndex = 0; SectionIndex < LOD.Materials.Num(); ++SectionIndex)
			{
				const FCustomizableObjectNodeSkeletalMeshMaterial& Section = LOD.Materials[SectionIndex];

				{
					UCustomizableObjectNodeSkeletalMeshPinDataMesh* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(this);
					PinData->Init(LODIndex, SectionIndex);
				
					AddPinData(*Section.MeshPinRef.Get(), *PinData);
				}

				if (SkeletalMesh)
				{
					if (const FSkeletalMaterial* SkeletalMaterial = GetSkeletalMaterialFor(LODIndex, SectionIndex))
					{
						if (SkeletalMaterial && SkeletalMaterial->MaterialInterface)
						{
							TArray<FGuid> ParameterIds;
							TArray<FMaterialParameterInfo> ParameterInfo;
							SkeletalMaterial->MaterialInterface->GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfo, ParameterIds);
							check(ParameterIds.Num() == ParameterInfo.Num());

							for (int32 ImageIndex = 0; ImageIndex < Section.ImagePinsRef.Num(); ++ImageIndex)
							{
								const UEdGraphPin* ImagePin = Section.ImagePinsRef[ImageIndex].Get();

								FGuid TextureParameterId;

								for (int32 Index = 0; Index < ParameterIds.Num(); ++Index)
								{
									if (ParameterInfo[Index].Name == ImagePin->PinFriendlyName.ToString())
									{
										TextureParameterId = ParameterIds[Index];
										break;
									}
								}

								UCustomizableObjectNodeSkeletalMeshPinDataImage* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataImage>(this);
								PinData->Init(LODIndex, SectionIndex, TextureParameterId);

								AddPinData(*ImagePin, *PinData);
							}
						}
					}
				}

				for (int32 LayoutIndex = 0; LayoutIndex < Section.LayoutPinsRef.Num(); ++LayoutIndex)
				{
					UCustomizableObjectNodeSkeletalMeshPinDataLayout* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(this);
					PinData->Init(LODIndex, SectionIndex, LayoutIndex);
				
					AddPinData(*Section.LayoutPinsRef[LayoutIndex].Get(), *PinData);
				}
			}
		}
			
		ReconstructNode();
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeSkeletalMeshPinDataOuter)
	{
		ReconstructNode(); // Pins did not have Pin Data. Reconstruct them.
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeSkeletalMeshPinDataUProperty)
	{
		ReconstructNode(CreateRemapPinsByName()); // Correct pins but incorrect Pin Data. Reconstruct and remap pins only by name, no Pin Data.
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::IgnoreDisabledSections)
	{
		ReconstructNode(); // Pins representing disabled sections could be present. 
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::SkeletalMeshNodeDefaultPinWithoutPinData)
	{
		if (const UEdGraphPin* Pin = DefaultPin.Get())
		{
			UCustomizableObjectNodeSkeletalMeshPinDataMesh* PinData = NewObject<UCustomizableObjectNodeSkeletalMeshPinDataMesh>(this);
			PinData->Init(-1, -1);

			AddPinData(*Pin, *PinData);
		}
	}
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeSkeletalMesh::CreateVisualWidget()
{
	TSharedPtr<SGraphNodeSkeletalMesh> GraphNode;
	SAssignNew(GraphNode, SGraphNodeSkeletalMesh,this);

	GraphNodeSkeletalMesh = GraphNode;

	return GraphNode;
}


bool UCustomizableObjectNodeSkeletalMesh::CheckIsValidLayout(const UEdGraphPin* InPin, int32& LayoutIndex, FString& MaterialName)
{
	const UEdGraphPin* ConnectedPin = FollowOutputPin(*InPin);
	if (!ConnectedPin)
	{
		return true;
	}

	int32 LODIndex;
	int32 SectionIndex;
	GetPinSection(*ConnectedPin, LODIndex, SectionIndex, LayoutIndex);

	if (const UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(LODIndex, SectionIndex))
	{
		MaterialName = MaterialInterface->GetName();
	}
	
	if (LayoutIndex == 0)
	{
		return true;
	}
	
	TBitArray VisitedLayouts(false, LayoutIndex);
	
	for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (const UCustomizableObjectNodeSkeletalMeshPinDataLayout* PinData = Cast<UCustomizableObjectNodeSkeletalMeshPinDataLayout>(GetPinData(*Pin)))
		{
			if (PinData->GetLODIndex() == LODIndex &&
				PinData->GetSectionIndex() == SectionIndex &&
				PinData->GetUVIndex() < LayoutIndex)
			{
				VisitedLayouts[PinData->GetUVIndex()] = true;
			}
		}
	}

	return !VisitedLayouts.Contains(false);
}

UMaterialInterface* UCustomizableObjectNodeSkeletalMesh::GetMaterialInterfaceFor(const int32 LODIndex, const int32 SectionIndex, const FSkeletalMeshModel* ImportedModel) const
{
	if (FSkeletalMaterial* SkeletalMaterial = GetSkeletalMaterialFor(LODIndex, SectionIndex, ImportedModel))
	{
		return SkeletalMaterial->MaterialInterface;
	}

	return nullptr;
}


FSkeletalMaterial* UCustomizableObjectNodeSkeletalMesh::GetSkeletalMaterialFor(const int32 LODIndex, const int32 SectionIndex, const FSkeletalMeshModel* ImportedModel) const
{
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	// We assume that LODIndex and MaterialIndex are valid for the imported model
	int32 SkeletalMeshMaterialIndex = INDEX_NONE;
	
	// Check if we have lod info map to get the correct material index
	if (const FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(LODIndex))
	{
		if (LodInfo->LODMaterialMap.IsValidIndex(SectionIndex))
		{
			SkeletalMeshMaterialIndex = LodInfo->LODMaterialMap[SectionIndex];
		}
	}

	// Only deduce index when the explicit mapping is not found or there is no remap
	if (SkeletalMeshMaterialIndex == INDEX_NONE)
	{
	if (ImportedModel && ImportedModel->LODModels.IsValidIndex(LODIndex) && ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
	{
		SkeletalMeshMaterialIndex = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
	}
	else
	{
		FSkeletalMeshModel* AuxImportedModel = SkeletalMesh->GetImportedModel();

		if (AuxImportedModel)
		{
			if (AuxImportedModel->LODModels.IsValidIndex(LODIndex) && AuxImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
			{
				SkeletalMeshMaterialIndex = AuxImportedModel->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
			}
		}
	}
	}
	
	if (SkeletalMesh->GetMaterials().IsValidIndex(SkeletalMeshMaterialIndex))
	{
		return &SkeletalMesh->GetMaterials()[SkeletalMeshMaterialIndex];
	}
	
	return nullptr;
}


//SGraphNode --------------------------------------------

void SGraphNodeSkeletalMesh::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	NodeSkeletalMesh = Cast< UCustomizableObjectNodeSkeletalMesh >(GraphNode);

	WidgetSize = 128.0f;
	ThumbnailSize = 128;

	TSharedPtr<FCustomizableObjectEditor> Editor = StaticCastSharedPtr< FCustomizableObjectEditor >(NodeSkeletalMesh->GetGraphEditor());

	// Thumbnail
	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(32));
	AssetThumbnail = MakeShareable(new FAssetThumbnail(NodeSkeletalMesh->SkeletalMesh, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

	// Selector
	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FSinglePropertyParams SingleDetails;
	SingleDetails.NamePlacement = EPropertyNamePlacement::Hidden;
	SingleDetails.NotifyHook = Editor.Get();
	SingleDetails.bHideAssetThumbnail = true;

	SkeletalMeshSelector = PropPlugin.CreateSingleProperty(NodeSkeletalMesh, "SkeletalMesh", SingleDetails);

	UpdateGraphNode();
}


void SGraphNodeSkeletalMesh::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();
}


void SGraphNodeSkeletalMesh::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	DefaultTitleAreaWidget->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(5))
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SGraphNodeSkeletalMesh::OnExpressionPreviewChanged)
			.IsChecked(IsExpressionPreviewChecked())
			.Cursor(EMouseCursor::Default)
			.Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(GetExpressionPreviewArrow())
				]
			]
	];
}


void SGraphNodeSkeletalMesh::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	LeftNodeBox->AddSlot()
		.AutoHeight()
		.MaxHeight(WidgetSize)
		.Padding(10.0f,10.0f,0.0f,0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(ExpressionPreviewVisibility())

			+SHorizontalBox::Slot()
			.MaxWidth(WidgetSize)
			.Padding(5.0f,5.0f,5.0f,5.0f)
			[
				AssetThumbnail->MakeThumbnailWidget()
			]
		];

	if (SkeletalMeshSelector.IsValid())
	{
		LeftNodeBox->AddSlot()
		.AutoHeight()
		.Padding(10.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(ExpressionPreviewVisibility())

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f,0.0f, 5.0f, 5.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SkeletalMeshSelector.ToSharedRef()
			]
		];
	}
}


void SGraphNodeSkeletalMesh::OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState)
{
	NodeSkeletalMesh->bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	UpdateGraphNode();
}


ECheckBoxState SGraphNodeSkeletalMesh::IsExpressionPreviewChecked() const
{
	return NodeSkeletalMesh->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}


const FSlateBrush* SGraphNodeSkeletalMesh::GetExpressionPreviewArrow() const
{
	return FCustomizableObjectEditorStyle::Get().GetBrush(NodeSkeletalMesh->bCollapsed ? TEXT("Nodes.ArrowDown") : TEXT("Nodes.ArrowUp"));
}


EVisibility SGraphNodeSkeletalMesh::ExpressionPreviewVisibility() const
{
	return NodeSkeletalMesh->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}


bool UCustomizableObjectNodeSkeletalMeshRemapPinsBySection::Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const
{
	const UCustomizableObjectNodeSkeletalMeshPinDataSection* PinDataOldPin = Cast<UCustomizableObjectNodeSkeletalMeshPinDataSection>(Node.GetPinData(OldPin));
	const UCustomizableObjectNodeSkeletalMeshPinDataSection* PinDataNewPin = Cast<UCustomizableObjectNodeSkeletalMeshPinDataSection>(Node.GetPinData(NewPin));
	if (PinDataOldPin && PinDataNewPin)
	{
		return *PinDataOldPin == *PinDataNewPin;
	}
	else
	{
		return Super::Equal(Node, OldPin, NewPin);	
	}
}


void UCustomizableObjectNodeSkeletalMeshPinDataSection::Init(int32 InLODIndex, int32 InSectionIndex)
{
	LODIndex = InLODIndex;
	SectionIndex = InSectionIndex;
}


int32 UCustomizableObjectNodeSkeletalMeshPinDataSection::GetLODIndex() const
{
	return LODIndex;
}


int32 UCustomizableObjectNodeSkeletalMeshPinDataSection::GetSectionIndex() const
{
	return SectionIndex;
}


bool UCustomizableObjectNodeSkeletalMeshPinDataSection::Equals(const UCustomizableObjectNodePinData& Other) const
{
	if (GetClass() != Other.GetClass())
	{
		return false;	
	}

	const UCustomizableObjectNodeSkeletalMeshPinDataSection& OtherTyped = static_cast<const UCustomizableObjectNodeSkeletalMeshPinDataSection&>(Other);
    if (LODIndex != OtherTyped.LODIndex ||
    	SectionIndex != OtherTyped.SectionIndex)
    {
        return false;	            
    }
	
    return Super::Equals(Other);	
}


void UCustomizableObjectNodeSkeletalMeshPinDataImage::Init(int32 InLODIndex, int32 InSectionIndex, FGuid InTextureParameterId)
{
	Super::Init(InLODIndex, InSectionIndex);

	TextureParameterId = InTextureParameterId;
}


FGuid UCustomizableObjectNodeSkeletalMeshPinDataImage::GetTextureParameterId() const
{
	return TextureParameterId;
}


bool UCustomizableObjectNodeSkeletalMeshPinDataImage::Equals(const UCustomizableObjectNodePinData& Other) const
{
	if (GetClass() != Other.GetClass())
	{
		return false;	
	}

	const UCustomizableObjectNodeSkeletalMeshPinDataImage& OtherTyped = static_cast<const UCustomizableObjectNodeSkeletalMeshPinDataImage&>(Other);
    if (TextureParameterId != OtherTyped.TextureParameterId)
    {
        return false;
    }
	
    return Super::Equals(Other);	
}


void UCustomizableObjectNodeSkeletalMeshPinDataLayout::Init(int32 InLODIndex, int32 InSectionIndex, int32 InUVIndex)
{
	Super::Init(InLODIndex, InSectionIndex);

	UVIndex = InUVIndex;
}


int32 UCustomizableObjectNodeSkeletalMeshPinDataLayout::GetUVIndex() const
{
	return UVIndex;
}


bool UCustomizableObjectNodeSkeletalMeshPinDataLayout::Equals(const UCustomizableObjectNodePinData& Other) const
{
	if (GetClass() != Other.GetClass())
	{
		return false;	
	}

	const UCustomizableObjectNodeSkeletalMeshPinDataLayout& OtherTyped = static_cast<const UCustomizableObjectNodeSkeletalMeshPinDataLayout&>(Other);
    if (UVIndex != OtherTyped.UVIndex)
    {
        return false;	            
    }
	
    return Super::Equals(Other);
}


#undef LOCTEXT_NAMESPACE
