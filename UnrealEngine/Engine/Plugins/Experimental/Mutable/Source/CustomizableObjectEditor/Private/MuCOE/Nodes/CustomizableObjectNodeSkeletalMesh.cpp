// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"

#include "AssetThumbnail.h"
#include "Containers/EnumAsByte.h"
#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/ICursor.h"
#include "ISinglePropertyView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "MaterialTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByNameDefaultPin.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyEditorModule.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/Archive.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

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
		MarkForReconstruct();
	}
}


void UCustomizableObjectNodeSkeletalMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PostLoadToCustomVersion)
	{
		for (FCustomizableObjectNodeSkeletalMeshLOD& LOD : LODs)
		{
			for(FCustomizableObjectNodeSkeletalMeshMaterial& Material : LOD.Materials)
			{
				if (Material.MeshPin && !Material.MeshPinRef.Get())
				{
					UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(Material.MeshPin);
					Material.MeshPinRef.SetPin(AuxPin);
				}

				if (!Material.LayoutPinsRef.Num())
				{
					if (Material.LayoutPins.Num())
					{
						for (UEdGraphPin_Deprecated* LayoutPin : Material.LayoutPins)
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
					for (UEdGraphPin_Deprecated* ImagePin : Material.ImagePins)
					{
						UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(ImagePin);
						FEdGraphPinReference AuxEdGraphPinReference(AuxPin);
						Material.ImagePinsRef.Add(AuxEdGraphPinReference);
					}
				}
			}
		}
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

	TArray<FCustomizableObjectNodeSkeletalMeshLOD> OldLODs(LODs);
	LODs.Empty();

	if (SkeletalMesh)
	{
		int NumLODs = Helper_GetLODInfoArray(SkeletalMesh).Num();
		TMap<FString, TArray<int32>> MaterialNameUseCount; // Stores how many times the material name has been used per LOD
		TMap<FString, TArray<int32>> ImageNameUseCount; // Stores how many times the image name has been used per LOD

		if (const FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(SkeletalMesh))
		{
			for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
			{
				LODs.Add(FCustomizableObjectNodeSkeletalMeshLOD());

				// Reset the material array and handle the actual materials from the mesh
				int32 NumMaterials = ImportedModel->LODModels[LODIndex].Sections.Num();

				FString OldNameForMap;

				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					LODs[LODIndex].Materials.Add(FCustomizableObjectNodeSkeletalMeshMaterial());

					UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(LODIndex, MaterialIndex, ImportedModel);

					FString MaterialName = LOCTEXT("Unnamed Material", "Unnamed Material").ToString();
					if (MaterialInterface)
					{
						MaterialName = MaterialInterface->GetName();
						LODs[LODIndex].Materials[MaterialIndex].Name = MaterialName;
					}

					FString PrefixId; // Prefix is empty if only 1 LOD and 1 material
					FString FriendlyPrefixId; // For retrocompatibility, only friendly names have all indexes

					if (NumLODs > 1) // Add LOD ids to prefix if more than one LOD
					{
						FString Aux = FString::Printf(TEXT("LOD%d-"), LODIndex);
						if (LODIndex > 0)
						{
							PrefixId.Append(Aux);
						}
						FriendlyPrefixId.Append(Aux);
					}

					int32 MaterialCurrentCount = 0;
					if (TArray<int32>* MUsageCountP = MaterialNameUseCount.Find(MaterialName))
					{
						if (MUsageCountP->Num() > LODIndex)
						{
							(*MUsageCountP)[LODIndex]++;
							MaterialCurrentCount = (*MUsageCountP)[LODIndex];
						}
						else
						{
							MUsageCountP->AddZeroed(LODIndex - (MUsageCountP->Num() - 1));
							(*MUsageCountP)[LODIndex] = MaterialCurrentCount;
						}
					}
					else
					{
						TArray<int32> NewMaterialNameUseCount;
						NewMaterialNameUseCount.Add(MaterialCurrentCount);
						MaterialNameUseCount.Add(MaterialName, NewMaterialNameUseCount);
					}

					if (MaterialCurrentCount > 0) // Add section ids to prefix if a section/material name is used more than once
					{
						FString Aux = FString::Printf(TEXT("%d-"), MaterialCurrentCount);
						PrefixId.Append(Aux);
					}

					UEdGraphPin* Pin = nullptr;

					{
						// Mesh
						FString MeshName = PrefixId + MaterialName + FString(" Mesh");
						Pin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*MeshName));
						Pin->PinFriendlyName = FText::FromString(MeshName);
						Pin->PinToolTip = MeshName;
						LODs[LODIndex].Materials[MaterialIndex].MeshPinRef.SetPin(Pin);

						// Layout
						if (ImportedModel->LODModels.Num() > LODIndex)
						{
							const uint32 numberOfUVLayouts = ImportedModel->LODModels[LODIndex].NumTexCoords;
							for (uint32 LayoutIndex = 0; LayoutIndex < numberOfUVLayouts; ++LayoutIndex)
							{
								FString LayoutPrefix = "";
								FString FriendlyLayoutPrefix = "";
								if (LayoutIndex > 0)
								{
									LayoutPrefix = FString::Printf(TEXT("UV%d-"), LayoutIndex);
								}
								if (numberOfUVLayouts > 1)
								{
									FriendlyLayoutPrefix.Append(FString::Printf(TEXT("UV%d-"), LayoutIndex));
								}
								FString LayoutName = PrefixId + LayoutPrefix + MaterialName + FString(" Layout");
								FString PinFriendlyName = FriendlyPrefixId + FriendlyLayoutPrefix + MaterialName + FString(" Layout");
								FString PinTooltip = FString::Printf(TEXT("Material %d "), MaterialIndex) + PinFriendlyName;
								Pin = CustomCreatePin(EGPD_Input, Schema->PC_Layout, FName(*LayoutName));
								Pin->PinFriendlyName = FText::FromString(PinFriendlyName);
								Pin->PinToolTip = PinTooltip;
								Pin->bDefaultValueIsIgnored = true;
								LODs[LODIndex].Materials[MaterialIndex].LayoutPinsRef.Add(FEdGraphPinReference(Pin));
							}
						}
					}

					// Images
					if (MaterialInterface)
					{
						UMaterial* Material = MaterialInterface->GetMaterial();

						TArray<FMaterialParameterInfo> ImageInfos;
						TArray<FGuid> ImageIds;
						Material->GetAllTextureParameterInfo(ImageInfos, ImageIds);

						for (int32 ImageIndex = 0; ImageIndex < ImageInfos.Num(); ++ImageIndex)
						{
							FString ImageName = ImageInfos[ImageIndex].Name.ToString();
							FString Sufix = FString::Printf(TEXT("-mat-%s"), *MaterialName);


							int32 ImageCurrentCount = 0;
							if (TArray<int32>* IUsageCountP = ImageNameUseCount.Find(ImageName))
							{
								if (IUsageCountP->Num() > LODIndex)
								{
									(*IUsageCountP)[LODIndex]++;
									ImageCurrentCount = (*IUsageCountP)[LODIndex];
								}
								else
								{
									IUsageCountP->AddZeroed(LODIndex - (IUsageCountP->Num() - 1));
									(*IUsageCountP)[LODIndex] = ImageCurrentCount;
								}
							}
							else
							{
								TArray<int32> NewImageNameUseCount;
								NewImageNameUseCount.Add(ImageCurrentCount);
								ImageNameUseCount.Add(ImageName, NewImageNameUseCount);
							}

							FString PinName = PrefixId + ImageName + FString::Printf(TEXT("-mat-%s"), *MaterialName);
							FString PinFriendlyImageName = ImageName;

							if (ImageCurrentCount > 0)
							{
								PinName.Append(FString::Printf(TEXT("-%d"), ImageCurrentCount));
							}

							Pin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName)); // Store the sufix in the category to be able to remove the sufix from the pin name later to retrieve the textures
							Pin->bDefaultValueIsIgnored = true;
							Pin->bHidden = true;
							Pin->PinFriendlyName = FText::FromString(PrefixId + PinFriendlyImageName);
							Pin->PinToolTip = PinName;
							LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef.Add(FEdGraphPinReference(Pin));
						}
					}
				}
			}
		}

		DefaultPin = FEdGraphPinReference();
	}
	else
	{
		UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(SKELETAL_MESH_PIN_NAME));
		Pin->bDefaultValueIsIgnored = true;
		Pin->PinFriendlyName = FText::FromString(SKELETAL_MESH_PIN_NAME);
		
		DefaultPin = FEdGraphPinReference(Pin);
	}
}


UCustomizableObjectNodeRemapPinsByName* UCustomizableObjectNodeSkeletalMesh::CreateRemapPinsByName() const
{
	return NewObject<UCustomizableObjectNodeRemapPinsByNameDefaultPin>();
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
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	if (const FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(SkeletalMesh))
	{
		for (int LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
		{
			for (int MaterialIndex = 0; MaterialIndex < LODs[LODIndex].Materials.Num(); ++MaterialIndex)
			{
				if (UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(LODIndex, MaterialIndex, ImportedModel))
				{
					for (int ImageIndex = 0; ImageIndex < LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef.Num(); ++ImageIndex)
					{
						const UEdGraphPin* OtherPin = LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef[ImageIndex].Get();
						if (OtherPin == Pin)
						{
							UTexture* Texture = nullptr;
							FString TextureString = OtherPin->PinFriendlyName.IsEmptyOrWhitespace() ? Helper_GetPinName(OtherPin) : OtherPin->PinFriendlyName.ToString();
							FName TextureName = FName(*TextureString);
							MaterialInterface->GetTextureParameterValue(TextureName, Texture);
							return Cast<UTexture2D>(Texture);
						}
					}
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


TArray<UCustomizableObjectLayout*> UCustomizableObjectNodeSkeletalMesh::GetLayouts(const UEdGraphPin* OutPin) const
{
	TArray<UCustomizableObjectLayout*> Result;

	for (int LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		for (int MaterialIndex = 0; MaterialIndex < LODs[LODIndex].Materials.Num(); ++MaterialIndex)
		{
			if (LODs[LODIndex].Materials[MaterialIndex].MeshPinRef.Get() == OutPin)
			{
				for (int LayoutIndex = 0; LayoutIndex < LODs[LODIndex].Materials[MaterialIndex].LayoutPinsRef.Num(); ++LayoutIndex)
				{
					if (UEdGraphPin* LayoutInPin = LODs[LODIndex].Materials[MaterialIndex].LayoutPinsRef[LayoutIndex].Get())
					{
						if (const UEdGraphPin* ConnectedPin = FollowInputPin(*LayoutInPin))
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
		}
	}

	return Result;
}


UObject* UCustomizableObjectNodeSkeletalMesh::GetMesh() const
{
	return SkeletalMesh;
}


UEdGraphPin* UCustomizableObjectNodeSkeletalMesh::GetMeshPin(const int32 LODIndex, const int MaterialIndex) const
{
	if (LODIndex < LODs.Num())
	{
		if (const FCustomizableObjectNodeSkeletalMeshLOD& LOD = LODs[LODIndex];
			MaterialIndex < LOD.Materials.Num())
		{
			return LOD.Materials[MaterialIndex].MeshPinRef.Get();
		}
	}

	return nullptr;
}

void UCustomizableObjectNodeSkeletalMesh::GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const
{
	if (LODs.IsEmpty())
	{
		OutLODIndex = -1;
		OutSectionIndex = -1;
		OutLayoutIndex = -1;

		return;
	}

	for (OutLODIndex = 0; OutLODIndex < LODs.Num(); ++OutLODIndex)
	{
		const FCustomizableObjectNodeSkeletalMeshLOD& LOD = LODs[OutLODIndex];
		
		for (OutSectionIndex = 0; OutSectionIndex < LOD.Materials.Num(); ++OutSectionIndex)
		{
			const FCustomizableObjectNodeSkeletalMeshMaterial& Material = LOD.Materials[OutSectionIndex];
			
			for (OutLayoutIndex = 0; OutLayoutIndex < Material.LayoutPinsRef.Num(); ++OutLayoutIndex)
			{
				// Is a Layout pin?
				if (Material.LayoutPinsRef[OutLayoutIndex].Get() == &Pin)
				{
					return;
				}
			
				// Is a Mesh pin?
				if (Material.MeshPinRef.Get() == &Pin)
				{
					return;
				}

				// Is an Image pin?
				for (int ImageIndex = 0; ImageIndex < Material.ImagePinsRef.Num(); ++ImageIndex)
				{
					if (Material.ImagePinsRef[ImageIndex].Get() == &Pin)
					{
						return;
					}
				}
			}
		}
	}

	check(false); // Pin not found. Probably a new pin has been added which is not contemplated in this function.
}

UMaterialInterface* UCustomizableObjectNodeSkeletalMesh::GetMaterialFor(const UEdGraphPin* Pin) const
{
	if (SkeletalMesh)
	{
		if (FSkeletalMaterial* SkeletalMaterial = GetSkeletalMaterialFor(Pin))
		{
			return SkeletalMaterial->MaterialInterface;
		}
	}

	return nullptr;
}

FSkeletalMaterial* UCustomizableObjectNodeSkeletalMesh::GetSkeletalMaterialFor(const UEdGraphPin* Pin) const
{
	check(Pin);
	
	int32 LODIndex;
	int32 SectionIndex;
	int32 LayoutIndex;
	GetPinSection(*Pin, LODIndex, SectionIndex, LayoutIndex);

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
	bool Result = false;

	if (SkeletalMesh)
	{
		int32 numLods = Helper_GetLODNum(SkeletalMesh);
		if (LODs.Num() != numLods)
		{
			Result = true;
		}

		if (const FSkeletalMeshModel* ImportedModel = Helper_GetImportedModel(SkeletalMesh))
		{
			for (int LODIndex = 0; !Result && LODIndex < LODs.Num(); ++LODIndex)
			{
				int32 NumMaterials = ImportedModel->LODModels[LODIndex].Sections.Num();
				if (LODs[LODIndex].Materials.Num() != NumMaterials)
				{
					Result = true;
				}

				for (int MaterialIndex = 0; !Result && MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					if (UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor(LODIndex, MaterialIndex, ImportedModel))
					{
						if (LODs[LODIndex].Materials[MaterialIndex].Name != MaterialInterface->GetName())
						{
							Result = true;
							break;
						}
						UMaterial* Material = MaterialInterface->GetMaterial();

						TArray<FMaterialParameterInfo> ImageInfos;
						TArray<FGuid> ImageIds;
						Material->GetAllTextureParameterInfo(ImageInfos, ImageIds);

						if (LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef.Num() != ImageInfos.Num())
						{
							Result = true;
							break;
						}

						for (int ImageIndex = 0; ImageIndex < ImageInfos.Num(); ++ImageIndex)
						{
							const FString ImagePinName = LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef[ImageIndex].Get()->GetName();
							if (!ImagePinName.Contains(ImageInfos[ImageIndex].Name.ToString()))
							{
								Result = true;
								break;
							}
						}
					}
				}
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

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeSkeletalMeshCorruptedPinRef)
	{
		ReconstructNode();
	}
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeSkeletalMesh::CreateVisualWidget()
{
	TSharedPtr<SGraphNodeSkeletalMesh> GraphNode;
	SAssignNew(GraphNode, SGraphNodeSkeletalMesh,this);

	GraphNodeSkeletalMesh = GraphNode;

	return GraphNode;
}


bool UCustomizableObjectNodeSkeletalMesh::CheckIsValidLayout(const UEdGraphPin* Pin, int32& LayoutIndex, FString& MaterialName)
{
	if (const UEdGraphPin* ConnectedPin = FollowOutputPin(*Pin))
	{
		int32 LODIndex;
		int32 SectionIndex;
		GetPinSection(*ConnectedPin, LODIndex, SectionIndex, LayoutIndex);

		if (LayoutIndex == 0)
		{
			return true;
		}
		else
		{
			TArray<FEdGraphPinReference> LayoutPins = LODs[LODIndex].Materials[SectionIndex].LayoutPinsRef;
			MaterialName = LODs[LODIndex].Materials[SectionIndex].Name;

			for (int32 i = 0; i < LayoutPins.Num(); ++i)
			{
				if (i == LayoutIndex)
				{
					break;
				}

				if (!FollowInputPin(*LayoutPins[i].Get()))
				{
					return false;
				}
			}

		}
	}

	return true;
}

UMaterialInterface* UCustomizableObjectNodeSkeletalMesh::GetMaterialInterfaceFor(const int LODIndex, const int MaterialIndex, const FSkeletalMeshModel* ImportedModel) const
{
	if (FSkeletalMaterial* SkeletalMaterial = GetSkeletalMaterialFor(LODIndex, MaterialIndex, ImportedModel))
	{
		return SkeletalMaterial->MaterialInterface;
	}

	return nullptr;
}

FSkeletalMaterial* UCustomizableObjectNodeSkeletalMesh::GetSkeletalMaterialFor(const int LODIndex, const int MaterialIndex, const FSkeletalMeshModel* ImportedModel) const
{
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	// We assume that LODIndex and MaterialIndex are valid for the imported model
	int SkeletalMeshMaterialIndex;
	if (ImportedModel)
	{
		SkeletalMeshMaterialIndex = ImportedModel->LODModels[LODIndex].Sections[MaterialIndex].MaterialIndex;
	}
	else
	{
		SkeletalMeshMaterialIndex = Helper_GetImportedModel(SkeletalMesh)->LODModels[LODIndex].Sections[MaterialIndex].MaterialIndex;
	}
	
	// Check if we have lod info map to get the correct material index
	if (const FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(LODIndex))
	{
		if (LodInfo->LODMaterialMap.IsValidIndex(MaterialIndex))
		{
			SkeletalMeshMaterialIndex = LodInfo->LODMaterialMap[MaterialIndex];
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


#undef LOCTEXT_NAMESPACE
