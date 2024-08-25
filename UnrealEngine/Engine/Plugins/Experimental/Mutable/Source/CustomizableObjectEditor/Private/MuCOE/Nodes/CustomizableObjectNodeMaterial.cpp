// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"

#include "MaterialCachedData.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialInstance.h"
#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/SCustomizableObjectNodeMaterial.h"
#include "ObjectEditorUtils.h"
#include "PropertyCustomizationHelpers.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"

class SGraphNode;
class SWidget;
class UCustomizableObjectLayout;
class UObject;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


const TArray<EMaterialParameterType> UCustomizableObjectNodeMaterial::ParameterTypes = {
	EMaterialParameterType::Texture,
	EMaterialParameterType::Vector,
	EMaterialParameterType::Scalar };


bool UCustomizableObjectNodeMaterialRemapPinsByName::Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const
{
	const UCustomizableObjectNodeMaterialPinDataParameter* PinDataOldPin = Cast<UCustomizableObjectNodeMaterialPinDataParameter>(Node.GetPinData(OldPin));
	const UCustomizableObjectNodeMaterialPinDataParameter* PinDataNewPin = Cast<UCustomizableObjectNodeMaterialPinDataParameter>(Node.GetPinData(NewPin));
	if (PinDataOldPin && PinDataNewPin)
	{
		return PinDataOldPin->ParameterId == PinDataNewPin->ParameterId;
	}
	else
	{
		return Super::Equal(Node, OldPin, NewPin);
	}
}


void UCustomizableObjectNodeMaterialRemapPinsByName::RemapPins(const UCustomizableObjectNode& Node, const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan)
{
	for (UEdGraphPin* OldPin : OldPins)
	{
		bool bFound = false;

		for (UEdGraphPin* NewPin : NewPins)
		{
			if (Equal(Node, *OldPin, *NewPin))
			{
				bFound = true;

				PinsToRemap.Add(OldPin, NewPin);
				break;
			}
		}

		if (!bFound && (OldPin->LinkedTo.Num() || HasSavedPinData(Node, *OldPin)))
		{
			PinsToOrphan.Add(OldPin);
		}
	}
}


bool UCustomizableObjectNodeMaterialRemapPinsByName::HasSavedPinData(const UCustomizableObjectNode& Node, const UEdGraphPin &Pin) const
{
	if (const UCustomizableObjectNodeMaterialPinDataParameter* PinData = Cast<UCustomizableObjectNodeMaterialPinDataParameter>(Node.GetPinData(Pin)))
	{
		return !PinData->IsDefault();
	}
	else
	{
		return false;
	}
}


bool UCustomizableObjectNodeMaterialPinDataImage::IsDefault() const
{
	const UCustomizableObjectNodeMaterialPinDataImage* Default = Cast<UCustomizableObjectNodeMaterialPinDataImage>(GetClass()->ClassDefaultObject);

	return PinMode == Default->PinMode &&
		UVLayoutMode == Default->UVLayoutMode &&
		ReferenceTexture == Default->ReferenceTexture;
}


EPinMode UCustomizableObjectNodeMaterialPinDataImage::GetPinMode() const
{
	return PinMode;
}


void UCustomizableObjectNodeMaterialPinDataImage::SetPinMode(const EPinMode InPinMode)
{
	FObjectEditorUtils::SetPropertyValue(this, GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeMaterialPinDataImage, PinMode), InPinMode);
}


void UCustomizableObjectNodeMaterialPinDataImage::Init(UCustomizableObjectNodeMaterial& InNodeMaterial)
{
	NodeMaterial = &InNodeMaterial;
}


void UCustomizableObjectNodeMaterialPinDataImage::Copy(const UCustomizableObjectNodePinData& Other)
{
	if (const UCustomizableObjectNodeMaterialPinDataImage* PinDataOldPin = Cast<UCustomizableObjectNodeMaterialPinDataImage>(&Other))
	{
		PinMode = PinDataOldPin->PinMode;
		UVLayoutMode = PinDataOldPin->UVLayoutMode;
		UVLayout = PinDataOldPin->UVLayout;
		ReferenceTexture = PinDataOldPin->ReferenceTexture;

		if (NodeMaterial)
		{
			NodeMaterial->UpdateImagePinMode(ParameterId);
		}
	}
}


FName UCustomizableObjectNodeMaterial::GetPinName(const EMaterialParameterType Type, const int32 ParameterIndex) const
{
	const FString ParameterName = GetParameterName(Type, ParameterIndex).ToString();
	return FName(GetParameterLayerIndex(Type, ParameterIndex) != INDEX_NONE ? GetParameterLayerName(Type, ParameterIndex).ToString() + " - " + ParameterName : ParameterName);
}


int32 UCustomizableObjectNodeMaterial::GetExpressionTextureCoordinate(UMaterial* Material, const FGuid& ImageId)
{
	if (const UMaterialExpressionTextureSample* TextureSample = Material->FindExpressionByGUID<UMaterialExpressionTextureSample>(ImageId))
	{
		if (!TextureSample->Coordinates.Expression)
		{
			return TextureSample->ConstCoordinate;
		}
		else if (const UMaterialExpressionTextureCoordinate* TextureCoords = Cast<UMaterialExpressionTextureCoordinate>(TextureSample->Coordinates.Expression))
		{
			return TextureCoords->CoordinateIndex;
		}
	}

	return -1;
}


EPinMode UCustomizableObjectNodeMaterial::NodePinModeToImagePinMode(const ENodePinMode NodePinMode)
{
	switch (NodePinMode)
	{
	case ENodePinMode::Mutable:
		return EPinMode::Mutable;
	case ENodePinMode::Passthrough:
		return EPinMode::Passthrough;
	default:
		check(false); // Missing case.
		return EPinMode::Mutable;
	}
}


EPinMode UCustomizableObjectNodeMaterial::GetImagePinMode(const UEdGraphPin& Pin) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (UEdGraphPin *FollowedPin = FollowInputPin(Pin))
	{
		if (FollowedPin->PinType.PinCategory == Schema->PC_PassThroughImage)
		{
			return EPinMode::Passthrough;
		}

		return EPinMode::Mutable;
	}
	
	switch (GetPinData<UCustomizableObjectNodeMaterialPinDataImage>(Pin).GetPinMode())
	{
	case EPinMode::Default:
		return NodePinModeToImagePinMode(TextureParametersMode);
	case EPinMode::Mutable:
		return EPinMode::Mutable;
	case EPinMode::Passthrough:
		return EPinMode::Passthrough;
	default:
		check(false); // Missing case.
		return EPinMode::Mutable;
	}
}


int32 UCustomizableObjectNodeMaterial::GetImageUVLayoutFromMaterial(const int32 ImageIndex) const
{
	const FGuid ImageId = GetParameterId(EMaterialParameterType::Texture, ImageIndex);

	if (const int32 TextureCoordinate = GetExpressionTextureCoordinate(Material->GetMaterial(), ImageId);
		TextureCoordinate >= 0)
	{
		return TextureCoordinate;
	}

	FMaterialLayersFunctions Layers;
	Material->GetMaterialLayers(Layers);

	TArray<TArray<TObjectPtr<UMaterialFunctionInterface>>*> MaterialFunctionInterfaces;
	MaterialFunctionInterfaces.SetNumUninitialized(2);
	MaterialFunctionInterfaces[0] = &Layers.Layers;
	MaterialFunctionInterfaces[1] = &Layers.Blends;

	for (const TArray<TObjectPtr<UMaterialFunctionInterface>>* MaterialFunctionInterface : MaterialFunctionInterfaces)
	{
		for (const TObjectPtr<UMaterialFunctionInterface>& Layer : *MaterialFunctionInterface)
		{
			if (const int32 TextureCoordinate = GetExpressionTextureCoordinate(Layer->GetPreviewMaterial()->GetMaterial(), ImageId); TextureCoordinate >= 0)
			{
				return TextureCoordinate;
			}	
		}
	}

	return -1;
}


bool UCustomizableObjectNodeMaterialPinDataParameter::IsDefault() const
{
	return true;
}


/** Translates a given EPinMode to FText. */
FText EPinModeToText(const EPinMode PinMode)
{
	switch (PinMode)
	{
	case EPinMode::Default:
		return LOCTEXT("EPinModeDefault", "Node Defined");

	case EPinMode::Mutable:
		return LOCTEXT("EPinModeMutable", "Mutable");

	case EPinMode::Passthrough:
		return LOCTEXT("EPinModePassthrough", "Passthrough");

	default:
		check(false); // Missing case.
		return FText();
	}
}


void UCustomizableObjectNodeMaterialPinDataImage::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (const FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeMaterialPinDataImage, PinMode))
		{
			NodeMaterial->UpdateImagePinMode(ParameterId);
		}
	}
}


void UCustomizableObjectNodeMaterialPinDataImage::PostLoad()
{
	Super::PostLoad();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeMaterialPinDataImageDetails)
	{
		if (UVLayout == UV_LAYOUT_IGNORE)
		{
			UVLayoutMode = EUVLayoutMode::Ignore;
			UVLayout = 0;
		}
		else if (UVLayout == UV_LAYOUT_DEFAULT)
		{
			UVLayoutMode = EUVLayoutMode::FromMaterial;
			UVLayout = 0;
		}
		else
		{
			UVLayoutMode = EUVLayoutMode::Index;
		}
	}
}


void UCustomizableObjectNodeMaterial::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	{
		const FString PinFriendlyName = TEXT("Mesh");
		const FString PinName = PinFriendlyName + FString(TEXT("_Input_Pin"));
		UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(*PinName));
		MeshPin->PinFriendlyName = FText::FromString(PinFriendlyName);
		MeshPin->bDefaultValueIsIgnored = true;
	}
	
	{
		const FString PinFriendlyName = TEXT("Table Material");
		const FString PinName = PinFriendlyName + FString(TEXT("_Input_Pin"));
		UEdGraphPin* TableMaterialPin = CustomCreatePin(EGPD_Input, Schema->PC_MaterialAsset, FName(*PinName));
		TableMaterialPin->PinFriendlyName = FText::FromString(PinFriendlyName);
		TableMaterialPin->bDefaultValueIsIgnored = true;
		TableMaterialPin->PinToolTip = "Pin for a Material from a Table Node";
	}

	for (const EMaterialParameterType Type : ParameterTypes)
	{
		AllocateDefaultParameterPins(Type);
	}

	{
		const FString PinFriendlyName = TEXT("Material");
		const FString PinName = PinFriendlyName + FString(TEXT("_Output_Pin"));
		UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName(*PinName));
		OutputPin->PinFriendlyName = FText::FromString(PinFriendlyName);
	}
}


bool UCustomizableObjectNodeMaterial::CanPinBeHidden(const UEdGraphPin& Pin) const
{
	return Super::CanPinBeHidden(Pin) &&
		Pin.Direction == EGPD_Input && 
		Pin.PinType.PinCategory != UEdGraphSchema_CustomizableObject::PC_Mesh;
}


UCustomizableObjectNodeRemapPinsByName* UCustomizableObjectNodeMaterial::CreateRemapPinsDefault() const
{
	return NewObject<UCustomizableObjectNodeMaterialRemapPinsByName>();
}


void UCustomizableObjectNodeMaterial::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterial)
	{
		for (const FCustomizableObjectNodeMaterialImage& Image : Images_DEPRECATED)
		{
			const FString OldPinName = (Image.LayerIndex == -1 ? Image.Name : Image.PinName) + FString(TEXT("_Input_Image"));
			UEdGraphPin* OldPin = FindPin(OldPinName);
			if (!OldPin) // If we can not find a pin it means that the data was corrupted (old Image array and pins where not synchronized).
			{
				continue;
			}

			UCustomizableObjectNodeMaterialPinDataImage* PinData = NewObject<UCustomizableObjectNodeMaterialPinDataImage>(this);
			PinData->ParameterId = FGuid::NewGuid();
			PinData->ReferenceTexture = Image.ReferenceTexture;

			// Find referenced Material Parameter
			const int32 NumParameters = GetNumParameters(EMaterialParameterType::Texture);
			for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
			{
				if (GetParameterName(EMaterialParameterType::Texture, ParameterIndex).ToString() == Image.Name)
				{
					PinData->ParameterId = GetParameterId(EMaterialParameterType::Texture, ParameterIndex);

					if (Image.UVLayout == -1)
					{
						PinData->UVLayout = Image.UVLayout;
					}
					else
					{
						const int32 UVLayout = GetImageUVLayoutFromMaterial(ParameterIndex);
						if (UVLayout < 0) // Could not be deduced from the Material
						{
							PinData->UVLayout = Image.UVLayout;						
						}
						else if (UVLayout == Image.UVLayout)
						{
							PinData->UVLayout = UV_LAYOUT_DEFAULT;							
						}
						else
						{
							PinData->UVLayout = UVLayout;
						}					
					}
					
					break;
				}
			}
			
			AddPinData(*OldPin, *PinData);
		}

		for (const FCustomizableObjectNodeMaterialVector& Vector : VectorParams_DEPRECATED)
		{
			const FString OldPinName = (Vector.LayerIndex == -1 ? Vector.Name : Vector.PinName) + FString(TEXT("_Input_Vector"));
			const UEdGraphPin* OldPin = FindPin(OldPinName);
			if (!OldPin) // If we can not find a pin it means that the data was corrupted (old ScalarParams array and pins where not synchronized).
			{
				continue;
			}
			
			UCustomizableObjectNodeMaterialPinDataVector* PinData = NewObject<UCustomizableObjectNodeMaterialPinDataVector>(this);
			PinData->ParameterId = FGuid::NewGuid();
			
			// Find referenced Material Parameter
			const int32 NumParameters = GetNumParameters(EMaterialParameterType::Vector);
			for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
			{
				if (GetParameterName(EMaterialParameterType::Vector, ParameterIndex).ToString() == Vector.Name)
				{
					PinData->ParameterId = GetParameterId(EMaterialParameterType::Vector, ParameterIndex);
					break;
				}
			}
			
			AddPinData(*OldPin, *PinData);
		}

		for (const FCustomizableObjectNodeMaterialScalar& Scalar : ScalarParams_DEPRECATED)
		{
			const FString OldPinName = (Scalar.LayerIndex == -1 ? Scalar.Name : Scalar.PinName) + FString(TEXT("_Input_Scalar"));
			const UEdGraphPin* OldPin = FindPin(OldPinName);
			if (!OldPin) // If we can not find a pin it means that the data was corrupted (old ScalarParams array and pins where not synchronized).
			{
				continue;
			}
			
			UCustomizableObjectNodeMaterialPinDataScalar* PinData = NewObject<UCustomizableObjectNodeMaterialPinDataScalar>(this);
			PinData->ParameterId = FGuid::NewGuid();

			// Find referenced Material Parameter
			const int32 NumParameters = GetNumParameters(EMaterialParameterType::Scalar);
			for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
			{
				if (GetParameterName(EMaterialParameterType::Scalar, ParameterIndex).ToString() == Scalar.Name)
				{
					PinData->ParameterId = GetParameterId(EMaterialParameterType::Scalar, ParameterIndex);
					break;
				}
			}
			
			AddPinData(*OldPin, *PinData);
		}

		// Check if there are still pins which where not present in the Images, ScalarParams and ScalarParams arrays.
		for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			if ((Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image ||
 				 Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Color ||
				 Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Float) &&
				!GetPinData(*Pin))
			{
				UCustomizableObjectNodeMaterialPinDataParameter* PinData = [&](UObject* Outer) -> UCustomizableObjectNodeMaterialPinDataParameter*
				{
					if (Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image)
					{
						return NewObject<UCustomizableObjectNodeMaterialPinDataImage>(Outer);	
					}
					else if (Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Color)
					{
						return NewObject<UCustomizableObjectNodeMaterialPinDataVector>(Outer);
					}
					else if (Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Float)
					{
						return NewObject<UCustomizableObjectNodeMaterialPinDataScalar>(Outer);	
					}
					else
					{
						check(false); // Parameter type not contemplated.
						return nullptr;
					}
				}(this);
				
				PinData->ParameterId = FGuid::NewGuid();
				
				AddPinData(*Pin, *PinData);
			}
		}
		
		Images_DEPRECATED.Empty();
		VectorParams_DEPRECATED.Empty();
		ScalarParams_DEPRECATED.Empty();
		
		Super::ReconstructNode(); // Super required to avoid ambiguous call compilation error.
	}

	// Fill PinsParameter.
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterialPerformanceBug) // || CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterialPerformance
	{
		for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			if (const UCustomizableObjectNodeMaterialPinDataParameter* PinData = Cast<UCustomizableObjectNodeMaterialPinDataParameter>(GetPinData(*Pin)))
			{
				PinsParameter.Add(PinData->ParameterId, FEdGraphPinReference(Pin));
			}
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterialUXImprovements)
	{
		TextureParametersMode = bDefaultPinModeMutable_DEPRECATED ?
			ENodePinMode::Mutable :
			ENodePinMode::Passthrough;
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::ExtendMaterialOnlyMutableModeParameters)
	{
		const uint32 NumTextureParameters = GetNumParameters(EMaterialParameterType::Texture);
		for (uint32 ImageIndex = 0; ImageIndex < NumTextureParameters; ++ImageIndex)
		{
			if (const UEdGraphPin* Pin = GetParameterPin(EMaterialParameterType::Texture, ImageIndex))
			{
				UCustomizableObjectNodeMaterialPinDataImage* PinDataImage = Cast<UCustomizableObjectNodeMaterialPinDataImage>(GetPinData(*Pin));
				PinDataImage->NodeMaterial = this;
				
				PinsImagePinMode.Add(Pin->PinId, GetImagePinMode(*Pin));
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::ExtendMaterialOnlyMutableModeParametersBug)
	{
		for (const UEdGraphPin* Pin : GetAllPins())
		{
			if (UCustomizableObjectNodeMaterialPinDataImage* const PinDataImage = Cast<UCustomizableObjectNodeMaterialPinDataImage>(GetPinData(*Pin)))
			{
				PinDataImage->NodeMaterial = this;
				
				PinsImagePinMode.Add(Pin->PinId, GetImagePinMode(*Pin));
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeMaterialAddTablePin)
	{
		Super::ReconstructNode();
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AddedTableMaterialSwitch)
	{
		UMaterialInstance* DefaultPinValue = nullptr;

		if (const UEdGraphPin* MaterialAssetPin = GetMaterialAssetPin())
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MaterialAssetPin))
			{
				if (const UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(ConnectedPin->GetOwningNode()))
				{
					DefaultPinValue = TableNode->GetColumnDefaultAssetByType<UMaterialInstance>(ConnectedPin);
				}
			}
		}

		if (DefaultPinValue && DefaultPinValue->TextureParameterValues.Num())
		{
			const uint32 NumTextureParameters = GetNumParameters(EMaterialParameterType::Texture);
			for (uint32 ImageIndex = 0; ImageIndex < NumTextureParameters; ++ImageIndex)
			{
				if (const UEdGraphPin* ImagePin = GetParameterPin(EMaterialParameterType::Texture, ImageIndex))
				{
					FGuid ParameterId = GetParameterId(EMaterialParameterType::Texture, ImageIndex);

					TArray<FMaterialParameterInfo> TextureParameterInfo;
					TArray<FGuid> TextureGuids;

					// Getting parent's texture infos
					DefaultPinValue->GetMaterial()->GetAllTextureParameterInfo(TextureParameterInfo, TextureGuids);

					int32 TextureIndex = TextureGuids.Find(ParameterId);

					if (TextureIndex == INDEX_NONE)
					{
						continue;
					}

					FName TextureName = TextureParameterInfo[TextureIndex].Name;

					// Checking if the pin's texture has been modified in the material instance
					for (const FTextureParameterValue& Texture : DefaultPinValue->TextureParameterValues)
					{
						if (TextureName == Texture.ParameterInfo.Name)
						{
							if (UCustomizableObjectNodeMaterialPinDataImage* PinDataImage = Cast<UCustomizableObjectNodeMaterialPinDataImage>(GetPinData(*ImagePin)))
							{
								PinDataImage->PinMode = EPinMode::Mutable;

								PinsImagePinMode.Add(ImagePin->PinId, GetImagePinMode(*ImagePin));
							}
						}
					}
				}
			}
		}
	}
}


void UCustomizableObjectNodeMaterial::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();
	
	UpdateAllImagesPinMode(); // Could be changed when not loaded.
}


void UCustomizableObjectNodeMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


void UCustomizableObjectNodeMaterial::PostLoad()
{
	Super::PostLoad();
	
	if (Material)
	{
		Material->ConditionalPostLoad(); // Make sure the Material has been fully loaded.
	}
}


void UCustomizableObjectNodeMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (const FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeMaterial, Material))
		{
			Super::ReconstructNode(); // Super required to avoid ambiguous call compilation error.
		}
		else if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeMaterial, TextureParametersMode))
		{
			UpdateAllImagesPinMode();
		}
	}
}


FText UCustomizableObjectNodeMaterial::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Material)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MaterialName"), FText::FromString(Material->GetName()));

		return FText::Format(LOCTEXT("Material_Title", "{MaterialName}\nMaterial"), Args);
	}
	else
	{
		return LOCTEXT("Material", "Material");
	}
}


FLinearColor UCustomizableObjectNodeMaterial::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


UEdGraphPin* UCustomizableObjectNodeMaterial::OutputPin() const
{
	FString PinFriendlyName = TEXT("Material");
	FString PinName = PinFriendlyName + FString(TEXT("_Output_Pin"));

	UEdGraphPin* Pin = FindPin(PinName);

	if (Pin)
	{
		return Pin;
	}
	else
	{
		return FindPin(TEXT("Material"));
	}
}


UEdGraphPin* UCustomizableObjectNodeMaterial::GetMeshPin() const
{
	FString PinFriendlyName = TEXT("Mesh");
	FString PinName = PinFriendlyName + FString(TEXT("_Input_Pin"));

	UEdGraphPin* Pin = FindPin(PinName);

	if(Pin)
	{
		return Pin;
	}
	else
	{
		return FindPin(TEXT("Mesh"));
	}
}


UEdGraphPin* UCustomizableObjectNodeMaterial::GetMaterialAssetPin() const
{
	FString PinFriendlyName = TEXT("Table Material");
	FString PinName = PinFriendlyName + FString(TEXT("_Input_Pin"));

	UEdGraphPin* Pin = FindPin(PinName);

	if (Pin)
	{
		return Pin;
	}
	else
	{
		return FindPin(TEXT("Table Material"));
	}
}


bool UCustomizableObjectNodeMaterial::IsImageMutableMode(const int32 ImageIndex) const
{
	if (const UEdGraphPin* Pin = GetParameterPin(EMaterialParameterType::Texture, ImageIndex))
	{
		return IsImageMutableMode(*Pin);
	}
	else
	{
		return NodePinModeToImagePinMode(TextureParametersMode) == EPinMode::Mutable;
	}
}


bool UCustomizableObjectNodeMaterial::IsImageMutableMode(const UEdGraphPin& Pin) const
{
	return PinsImagePinMode[Pin.PinId] == EPinMode::Mutable;
}


void UCustomizableObjectNodeMaterial::UpdateImagePinMode(const FGuid ParameterId)
{
	UpdateImagePinMode(*PinsParameter[ParameterId].Get());
}


void UCustomizableObjectNodeMaterial::UpdateImagePinMode(const UEdGraphPin& Pin)
{
	EPinMode* PinMode = PinsImagePinMode.Find(Pin.PinId);
	const EPinMode OldPinMode = *PinMode;

	*PinMode = GetImagePinMode(Pin);
	
	if (*PinMode != OldPinMode)
	{
		PostImagePinModeChangedDelegate.Broadcast();
	}
}


void UCustomizableObjectNodeMaterial::UpdateAllImagesPinMode()
{
	bool bChanged = false;
	
	const int32 NumImages = GetNumParameters(EMaterialParameterType::Texture);
	for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
	{
		if (const UEdGraphPin* Pin = GetParameterPin(EMaterialParameterType::Texture, ImageIndex))
		{
			EPinMode* PinMode = PinsImagePinMode.Find(Pin->PinId);
			const EPinMode OldPinMode = *PinMode;

			*PinMode = GetImagePinMode(*Pin);

			bChanged = bChanged || (*PinMode != OldPinMode);
		}
	}

	if (bChanged)
	{
		PostImagePinModeChangedDelegate.Broadcast();
	}
}


UTexture2D* UCustomizableObjectNodeMaterial::GetImageReferenceTexture(const int32 ImageIndex) const
{
	if (const UEdGraphPin* Pin = GetParameterPin(EMaterialParameterType::Texture, ImageIndex))
	{
		return GetPinData<UCustomizableObjectNodeMaterialPinDataImage>(*Pin).ReferenceTexture;
	}
	else
	{
		return nullptr;
	}
}


UTexture2D* UCustomizableObjectNodeMaterial::GetImageValue(const int32 ImageIndex) const
{
	FName TextureName = GetParameterName(EMaterialParameterType::Texture, ImageIndex);
	
	UTexture* Texture;
	Material->GetTextureParameterValue(TextureName, Texture);

	return Cast<UTexture2D>(Texture);
}


TArray<UCustomizableObjectLayout*> UCustomizableObjectNodeMaterial::GetLayouts()
{
	TArray<UCustomizableObjectLayout*> Result;

	if (UEdGraphPin* MeshPin = GetMeshPin())
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MeshPin))
		{
			if (const UEdGraphPin* SourceMeshPin = FindMeshBaseSource(*ConnectedPin, false))
			{
				if (const UCustomizableObjectNodeSkeletalMesh* MeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SourceMeshPin->GetOwningNode()))
				{
					Result = MeshNode->GetLayouts(*SourceMeshPin);
				}
				else if (const UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(SourceMeshPin->GetOwningNode()))
				{
					Result = TableNode->GetLayouts(SourceMeshPin);
				}
			}
		}
	}

	return Result;
}


int32 UCustomizableObjectNodeMaterial::GetImageUVLayout(const int32 ImageIndex) const
{
	if (const UEdGraphPin* Pin = GetParameterPin(EMaterialParameterType::Texture, ImageIndex))
	{
		const UCustomizableObjectNodeMaterialPinDataImage& PinData = GetPinData<UCustomizableObjectNodeMaterialPinDataImage>(*Pin);
		switch(PinData.UVLayoutMode)
		{
		case EUVLayoutMode::FromMaterial:
			break;
		case EUVLayoutMode::Ignore:
			return UCustomizableObjectNodeMaterialPinDataImage::UV_LAYOUT_IGNORE;
		case EUVLayoutMode::Index:
			return PinData.UVLayout;
		default:
			unimplemented();
		}
	}

	const int32 UVIndex = GetImageUVLayoutFromMaterial(ImageIndex);
	if (UVIndex == -1)
	{
		FCustomizableObjectEditorLogger::CreateLog(LOCTEXT("UVLayoutMaterialError", "Could not deduce the UV Layout Index from the UMaterial. Nodes connected to the Texture Sample UVs pin are supported."))
			.Severity(EMessageSeverity::Warning)
			.Context(*this)
			.Log();
		
		return 0;
	}

	return UVIndex;
}


int32 UCustomizableObjectNodeMaterial::GetNumParameters(const EMaterialParameterType Type) const
{
	if (Material)
	{
#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=1
		return Material->GetCachedExpressionData().GetParameterTypeEntry(Type).ParameterInfoSet.Num();
#else
		return Material->GetCachedExpressionData().Parameters.GetNumParameters(Type);
#endif
	}
	else
	{
		return 0;
	}
}


FGuid UCustomizableObjectNodeMaterial::GetParameterId(const EMaterialParameterType Type, const int32 ParameterIndex) const
{
	const FMaterialCachedExpressionData& Data = Material->GetCachedExpressionData();

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=1
	if (Data.EditorOnlyData)
	{
		if (Data.EditorOnlyData->EditorEntries[(int32)Type].EditorInfo.Num() != 0)
		{
			return Data.EditorOnlyData->EditorEntries[(int32)Type].EditorInfo[ParameterIndex].ExpressionGuid;
		}
	}
#else
	const FMaterialCachedParameterEntry& Entry = Data.Parameters.GetParameterTypeEntry(Type);
	return Entry.EditorInfo[ParameterIndex].ExpressionGuid;
#endif
	
	return FGuid();
}


FName UCustomizableObjectNodeMaterial::GetParameterName(const EMaterialParameterType Type, const int32 ParameterIndex) const
{
	if (!Material)
	{
		return FName();
	}

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=1

	const FMaterialCachedParameterEntry& Entry = Material->GetCachedExpressionData().GetParameterTypeEntry(Type);

	for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
	{
		const int32 IteratorIndex = It.GetId().AsInteger();

		if (IteratorIndex == ParameterIndex)
		{
			return (*It).Name;
		}
	}
	
	// The parameter should exist
	check(false);

#else

	TArray<FGuid> ParameterIds;
	TArray<FMaterialParameterInfo> ParameterInfo;
	Material->GetAllParameterInfoOfType(Type, ParameterInfo, ParameterIds);

	return ParameterInfo.IsValidIndex(ParameterIndex) ? ParameterInfo[ParameterIndex].Name : FName();

#endif

	return FName();
}


int32 UCustomizableObjectNodeMaterial::GetParameterLayerIndex(const EMaterialParameterType Type, const int32 ParameterIndex) const
{
	if (!Material)
	{
		return -1;
	}

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=1

	const FMaterialCachedParameterEntry& Entry = Material->GetCachedExpressionData().GetParameterTypeEntry(Type);

	for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
	{
		const int32 IteratorIndex = It.GetId().AsInteger();

		if (IteratorIndex == ParameterIndex)
		{
			return (*It).Index;
		}
	}

	// The parameter should exist
	check(false);

#else

	TArray<FGuid> ParameterIds;
	TArray<FMaterialParameterInfo> ParameterInfo;
	Material->GetAllParameterInfoOfType(Type, ParameterInfo, ParameterIds);

	return ParameterInfo.IsValidIndex(ParameterIndex) ? ParameterInfo[ParameterIndex].Index : -1;

#endif

	return -1;
}


FText UCustomizableObjectNodeMaterial::GetParameterLayerName(const EMaterialParameterType Type, const int32 ParameterIndex) const
{
	ensure(Material);
	if (!Material)
	{
		return FText();
	}

	int32 LayerIndex = GetParameterLayerIndex(Type,ParameterIndex);

	FMaterialLayersFunctions LayersValue;
	Material->GetMaterialLayers(LayersValue);

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=1

	return LayersValue.EditorOnly.LayerNames.IsValidIndex(LayerIndex) ? LayersValue.EditorOnly.LayerNames[LayerIndex] : FText();

#else

	return LayersValue.LayerNames.IsValidIndex(LayerIndex) ? LayersValue.LayerNames[LayerIndex] : FText();

#endif
}


bool UCustomizableObjectNodeMaterial::HasParameter(const FGuid& ParameterId) const
{
	ensure(Material);
	if (!Material)
	{
		return false;
	}

#if ENGINE_MAJOR_VERSION==5 && ENGINE_MINOR_VERSION>=1

	for (const EMaterialParameterType Type : ParameterTypes)
	{
		const FMaterialCachedExpressionData& Data = Material->GetCachedExpressionData();
		const FMaterialCachedParameterEntry& Entry = Data.GetParameterTypeEntry(Type);

		if (!Data.EditorOnlyData || Data.EditorOnlyData->EditorEntries[(int32)Type].EditorInfo.IsEmpty())
		{
			continue;
		}

		for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
		{
			const int32 IteratorIndex = It.GetId().AsInteger();
			const FGuid& ParamGUid = Data.EditorOnlyData->EditorEntries[(int32)Type].EditorInfo[IteratorIndex].ExpressionGuid;

			if (ParamGUid == ParameterId)
			{
				return true;
			}
		}
	}

#else

	const FMaterialCachedExpressionData& Data = Material->GetCachedExpressionData();
	for (const FMaterialCachedParameterEntry& Entry : Data.Parameters.RuntimeEntries)
	{
		for (const FMaterialCachedParameterEditorInfo& Info : Entry.EditorInfo)
		{
			if (Info.ExpressionGuid == ParameterId)
			{
				return true;
			}
		}
	}
	for (const FMaterialCachedParameterEntry& Entry : Data.Parameters.EditorOnlyEntries)
	{
		for (const FMaterialCachedParameterEditorInfo& Info : Entry.EditorInfo)
		{
			if (Info.ExpressionGuid == ParameterId)
			{
				return true;
			}
		}
	}

#endif

	return false;
}


const UEdGraphPin* UCustomizableObjectNodeMaterial::GetParameterPin(const EMaterialParameterType Type, const int32 ParameterIndex) const
{
	const FGuid ParameterId = GetParameterId(Type, ParameterIndex);
	
	if (const FEdGraphPinReference* PinReference = PinsParameter.Find(ParameterId))
	{
		return PinReference->Get();
	}
	else
	{
		return nullptr;
	}
}


bool UCustomizableObjectNodeMaterial::IsNodeOutDatedAndNeedsRefresh()
{
	const bool bOutdated = RealMaterialDataHasChanged();

	// Remove previous compilation warnings
	if (!bOutdated && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

	return bOutdated;
}


FString UCustomizableObjectNodeMaterial::GetRefreshMessage() const
{
	return "Referenced material has changed, texture channels might have been added, removed or renamed. Please refresh the node material to reflect those changes.";
}


TSharedPtr<SWidget> UCustomizableObjectNodeMaterial::CustomizePinDetails(UEdGraphPin& Pin)
{
	if (UCustomizableObjectNodeMaterialPinDataImage* PinData = Cast<UCustomizableObjectNodeMaterialPinDataImage>(GetPinData(Pin)))
	{
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		
		const TSharedRef<IDetailsView> SettingsView = EditModule.CreateDetailView(DetailsViewArgs);
		SettingsView->SetObject(PinData);
		
		return SettingsView;
	}
	else
	{
		return nullptr;
	}
}


bool UCustomizableObjectNodeMaterial::CustomRemovePin(UEdGraphPin& Pin)
{
	if (const UCustomizableObjectNodeMaterialPinDataParameter* PinData = Cast<UCustomizableObjectNodeMaterialPinDataParameter>(GetPinData(Pin)))
	{
		if (const FEdGraphPinReference* Result = PinsParameter.Find(PinData->ParameterId);
			Result &&
			Result->Get() == &Pin)
		{
			PinsParameter.Remove(PinData->ParameterId);
		}
	}

	PinsImagePinMode.Remove(Pin.PinId);
	
	return Super::CustomRemovePin(Pin);
}

void UCustomizableObjectNodeMaterial::ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPinsMode)
{
	Super::ReconstructNode(RemapPinsMode);

	UpdateAllImagesPinMode();
}


bool UCustomizableObjectNodeMaterial::RealMaterialDataHasChanged() const
{
	for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (const UCustomizableObjectNodeMaterialPinDataParameter* PinData = Cast<UCustomizableObjectNodeMaterialPinDataParameter>(GetPinData(*Pin)))
		{
			if (!HasParameter(PinData->ParameterId) &&
				(FollowInputPin(*Pin) || !PinData->IsDefault()))
			{
				return true;
			}
		}
	}

	return false;
}


bool UCustomizableObjectNodeMaterial::IsPinRelevant(const UEdGraphPin* Pin) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{	
		return Pin->PinType.PinCategory == Schema->PC_Mesh;
	}
	else if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{	
		return Pin->PinType.PinCategory == Schema->PC_Material;
	}
	else
	{
		return false;
	}
}


FText UCustomizableObjectNodeMaterial::GetTooltipText() const
{
	return LOCTEXT("Material_Tooltip", "Defines a Customizable Object material.\nConsists of a mesh section, a material asset to paint it, and the runtime modifiable inputs to the material asset parameters.");
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeMaterial::CreateVisualWidget()
{
	return SNew(SCustomizableObjectNodeMaterial, this);
}


UCustomizableObjectNodeMaterialPinDataParameter* UCustomizableObjectNodeMaterial::CreatePinData(const EMaterialParameterType Type, const int32 ParameterIndex)
{
	UCustomizableObjectNodeMaterialPinDataParameter* PinData = nullptr;

	switch (Type)
	{
	case EMaterialParameterType::Texture:
		{
			UCustomizableObjectNodeMaterialPinDataImage* PinDataImage = NewObject<UCustomizableObjectNodeMaterialPinDataImage>(this);
			PinDataImage->Init(*this);
		
			PinData = PinDataImage;
			break;
		}
	case EMaterialParameterType::Vector:
		{
			PinData = NewObject<UCustomizableObjectNodeMaterialPinDataVector>(this);	
			break;
		}

	case EMaterialParameterType::Scalar:
		{
			PinData = NewObject<UCustomizableObjectNodeMaterialPinDataScalar>(this);	
			break;
		}

	default:
		check(false); // Parameter type not contemplated.
	}

	PinData->ParameterId = GetParameterId(Type, ParameterIndex);

	return PinData;
}

void UCustomizableObjectNodeMaterial::AllocateDefaultParameterPins(const EMaterialParameterType Type)
{
	const FName PinCategory = UEdGraphSchema_CustomizableObject::GetPinCategory(Type);

	const int32 NumParameters = GetNumParameters(Type);
	for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
	{
		UCustomizableObjectNodeMaterialPinDataParameter* PinData = CreatePinData(Type, ParameterIndex);
		
		const FName PinName = GetPinName(Type, ParameterIndex);
		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, PinCategory, PinName, PinData);
		Pin->bHidden = true;
		Pin->bDefaultValueIsIgnored = true;

		PinsParameter.Add(PinData->ParameterId, FEdGraphPinReference(Pin));

		if (Type == EMaterialParameterType::Texture)
		{
			PinsImagePinMode.Add(Pin->PinId, GetImagePinMode(*Pin));
		}
	}
}


void UCustomizableObjectNodeMaterial::SetDefaultMaterial()
{
	if (const UEdGraphPin* MeshPin = GetMeshPin();
		!Material && MeshPin)
	{
		if (const UEdGraphPin* LinkedMeshPin = FollowInputPin(*MeshPin))
		{
			UEdGraphNode* LinkedMeshNode = LinkedMeshPin->GetOwningNode();
	
			if (const UCustomizableObjectNodeSkeletalMesh* NodeSkeletalMesh = Cast<UCustomizableObjectNodeSkeletalMesh>(LinkedMeshNode))
			{
				Material = NodeSkeletalMesh->GetMaterialFor(LinkedMeshPin);
				if (Material)
				{
					Super::ReconstructNode(); // Super required to avoid ambiguous call compilation error.
				}
			}
			else if (const UCustomizableObjectNodeStaticMesh* NodeStaticMesh = Cast<UCustomizableObjectNodeStaticMesh>(LinkedMeshNode))
			{
				Material = NodeStaticMesh->GetMaterialFor(LinkedMeshPin);
				if (Material)
				{
					Super::ReconstructNode();
				}
			}
		}
	}
}


void UCustomizableObjectNodeMaterial::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == GetMeshPin())
	{
		if (LastMeshNodeConnected.IsValid())
		{
			LastMeshNodeConnected->PostEditChangePropertyDelegate.RemoveDynamic(this, &UCustomizableObjectNodeMaterial::MeshPostEditChangeProperty);
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Pin))
		{
			UEdGraphNode* MeshNode = ConnectedPin->GetOwningNode();

			if (MeshNode->IsA(UCustomizableObjectNodeStaticMesh::StaticClass()) || MeshNode->IsA(UCustomizableObjectNodeSkeletalMesh::StaticClass()))
			{
				SetDefaultMaterial();

				LastMeshNodeConnected = Cast<UCustomizableObjectNode>(MeshNode);
				LastMeshNodeConnected->PostEditChangePropertyDelegate.AddUniqueDynamic(this, &UCustomizableObjectNodeMaterial::MeshPostEditChangeProperty);
			}
		}
	}
	else if (Cast<UCustomizableObjectNodeMaterialPinDataImage>(GetPinData(*Pin)))
	{
		UpdateImagePinMode(*Pin);
	}
	else if (Pin == GetMaterialAssetPin())
	{
		UpdateAllImagesPinMode();	
	}
}


void UCustomizableObjectNodeMaterial::PostPasteNode()
{
	Super::PostPasteNode();
	SetDefaultMaterial();
}


bool UCustomizableObjectNodeMaterial::CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (InOwnedInputPin && InOwnedInputPin->PinType.PinCategory == Schema->PC_Image &&
		InOutputPin && InOutputPin->PinType.PinCategory == Schema->PC_PassThroughImage)
	{
		return true;
	}
	
	return Super::CanConnect(InOwnedInputPin, InOutputPin, bOutIsOtherNodeBlocklisted, bOutArePinsCompatible);
}


void UCustomizableObjectNodeMaterial::MeshPostEditChangeProperty(FPostEditChangePropertyDelegateParameters& Parameters)
{
	if (const UEdGraphPin* MeshPin = FindPin(TEXT("Mesh")))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MeshPin);
			ConnectedPin && ConnectedPin->GetOwningNode() == Parameters.Node)
		{
			SetDefaultMaterial();
		}
		else if (UCustomizableObjectNode* MeshNode = Cast<UCustomizableObjectNode>(Parameters.Node))
		{
			MeshNode->PostEditChangePropertyDelegate.RemoveDynamic(this, &UCustomizableObjectNodeMaterial::MeshPostEditChangeProperty);
		}
	}
}


void UCustomizableObjectNodeMaterial::BreakExistingConnectionsPostConnection(UEdGraphPin* InputPin, UEdGraphPin* OutputPin)
{
	if (OutputPin->GetOwningNode() == this) // Case: NodeMaterial (OutputPin) --> (InputPin) Other Node
	{
		// Allow multiple NodeCopyMaterial nodes to be connected
		if (InputPin->GetOwningNode()->IsA(UCustomizableObjectNodeCopyMaterial::StaticClass()))
		{
			return;
		}

		// Remove the previous NodeMaterial
		UEdGraphPin* RemovePin = nullptr;

		for (UEdGraphPin* Pin : OutputPin->LinkedTo)
		{
			if (Pin != InputPin && !Pin->GetOwningNode()->IsA(UCustomizableObjectNodeCopyMaterial::StaticClass())) {
				RemovePin = Pin;
				break;
			}
		}

		OutputPin->BreakLinkTo(RemovePin); // Can not be called inside the range for loop. Can be called with a null value.
	}
}

#undef LOCTEXT_NAMESPACE
