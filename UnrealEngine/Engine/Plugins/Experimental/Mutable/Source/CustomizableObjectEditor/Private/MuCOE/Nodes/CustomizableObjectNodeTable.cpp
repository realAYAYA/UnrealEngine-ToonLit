// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2DArray.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

class ICustomizableObjectEditor;
class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


bool UCustomizableObjectNodeTableRemapPins::Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const
{
	const UCustomizableObjectNodeTableObjectPinData& OldPinData = Node.GetPinData<UCustomizableObjectNodeTableObjectPinData>(OldPin);
	const UCustomizableObjectNodeTableObjectPinData& NewPinData = Node.GetPinData<UCustomizableObjectNodeTableObjectPinData>(NewPin);

	// If one of these two option fails, pins are different
	if (OldPinData.StructColumnId != NewPinData.StructColumnId || OldPin.Direction != NewPin.Direction)
	{
		return false;
	}

	// In this case pin type may have changed but we consider them the same type
	if (OldPin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image ||
		OldPin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_PassThroughImage)
	{
		return true;
	}

	// Non image pins must remain the same pin type
	if (OldPin.PinType != NewPin.PinType)
	{
		return false;
	}

	if (OldPin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Mesh)
	{
		const UCustomizableObjectNodeTableMeshPinData& OldMeshPinData = Node.GetPinData<UCustomizableObjectNodeTableMeshPinData>(OldPin);
		const UCustomizableObjectNodeTableMeshPinData& NewMeshPinData = Node.GetPinData<UCustomizableObjectNodeTableMeshPinData>(NewPin);

		// LOD and Section must remain the same
		return 	OldMeshPinData.LOD == NewMeshPinData.LOD &&
			OldMeshPinData.Material == NewMeshPinData.Material;
	}

	return true;
}


void UCustomizableObjectNodeTableRemapPins::RemapPins(const UCustomizableObjectNode& Node, const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan)
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

		if (!bFound && OldPin->LinkedTo.Num())
		{
			PinsToOrphan.Add(OldPin);
		}
	}
}


// Table Node -------

void UCustomizableObjectNodeTable::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AddedTableNodesTextureMode)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			if (Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image)
			{
				UCustomizableObjectNodeTableObjectPinData* OldPinData = Cast<UCustomizableObjectNodeTableObjectPinData>(GetPinData(*(Pin)));
				UCustomizableObjectNodeTableImagePinData* NewPinData = NewObject<UCustomizableObjectNodeTableImagePinData>(this);

				if (OldPinData && NewPinData)
				{
					NewPinData->ColumnName = OldPinData->ColumnName;
					NewPinData->ImageMode = ETableTextureType::MUTABLE_TEXTURE; // Old pin type by default
				}

				AddPinData(*Pin, *NewPinData);
			}
		}
	}
	
	// Adding StructColumnID
	if(CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AddedColumnIdDataToTableNodePins)
	{
		const UScriptStruct* TableStruct = GetTableNodeStruct();

		if (TableStruct)
		{
			for (UEdGraphPin* Pin : Pins)
			{
				UCustomizableObjectNodeTableObjectPinData* PinData = Cast<UCustomizableObjectNodeTableObjectPinData>(GetPinData(*(Pin)));

				// Adding pindata to float and colors
				if (!PinData)
				{
					PinData = NewObject<UCustomizableObjectNodeTableObjectPinData>(this);
					PinData->ColumnName = Pin->PinFriendlyName.ToString();

					AddPinData(*Pin, *PinData);
				}

				FProperty* ColumnProperty = FindTableProperty(TableStruct, FName(*PinData->ColumnName));

				if (!ColumnProperty)
				{
					continue;
				}

				FGuid ColumnPropertyId = FStructureEditorUtils::GetGuidForProperty(ColumnProperty);
				PinData->StructColumnId = ColumnPropertyId;

				// Store anim columns in the node instead of the pin
				if (UCustomizableObjectNodeTableMeshPinData* MeshPinData = Cast<UCustomizableObjectNodeTableMeshPinData>(GetPinData(*(Pin))))
				{
					if (!MeshPinData->AnimInstanceColumnName_DEPRECATED.IsEmpty())
					{
						FTableNodeColumnData NewColumnData;
						NewColumnData.AnimInstanceColumnName = MeshPinData->AnimInstanceColumnName_DEPRECATED;
						NewColumnData.AnimSlotColumnName = MeshPinData->AnimSlotColumnName_DEPRECATED;
						NewColumnData.AnimTagColumnName = MeshPinData->AnimTagColumnName_DEPRECATED;

						ColumnDataMap.Add(ColumnPropertyId, NewColumnData);
					}
				}
			}
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AddedAnyTextureTypeToPassThroughTextures)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			UCustomizableObjectNodePinData* PinData = GetPinData(*Pin);

			if (UCustomizableObjectNodeTableImagePinData* ImagePinData = Cast<UCustomizableObjectNodeTableImagePinData>(PinData))
			{
				ImagePinData->ConvertArrayTextureToAnyTextureFixup();
			}
		}
	}
}


void UCustomizableObjectNodeTable::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	if (Table)
	{
		OnTableChangedDelegateHandle = Table->OnDataTableChanged().AddUObject(this, &UCustomizableObjectNodeTable::OnTableChanged);
	}
}


void UCustomizableObjectNodeTable::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeTable, Table) &&
		Table)
	{
		Table->OnDataTableChanged().Remove(OnTableChangedDelegateHandle);
	}
}


void UCustomizableObjectNodeTable::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (const FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeTable, Table))
		{
			if (Table)
			{
				OnTableChangedDelegateHandle = Table->OnDataTableChanged().AddUObject(this, &UCustomizableObjectNodeTable::OnTableChanged);
			}
			
			OnTableChanged();
		}
	}
}


FText UCustomizableObjectNodeTable::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Table)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("TableName"), FText::FromString(Table->GetName()));

		return FText::Format(LOCTEXT("TableNode_Title_DataTable", "{TableName}\nData Table"), Args);
	}
	else if (Structure)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("StructureName"), FText::FromString(Structure->GetName()));

		return FText::Format(LOCTEXT("TableNode_Title_ScriptedStruct", "{StructureName}\nScript Struct"), Args);
	}
	
	return LOCTEXT("Mutable Table", "Table");
}


FLinearColor UCustomizableObjectNodeTable::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Object);
}


FText UCustomizableObjectNodeTable::GetTooltipText() const
{
	return LOCTEXT("Node_Table_Tooltip", "Represents all the columns of Data Table asset.");
}


void UCustomizableObjectNodeTable::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin && !Pin->LinkedTo.Num() && 
		(Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image || Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_PassThroughImage))
	{
		if (UCustomizableObjectNodeTableImagePinData* ImagePinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
		{
			if (!ImagePinData->IsNotTexture2D() && ImagePinData->IsDefaultImageMode() && ImagePinData->ImageMode != DefaultImageMode)
			{
				ImagePinData->ImageMode = DefaultImageMode;
				ReconstructNode();
			}
		}
	}
}


void UCustomizableObjectNodeTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName() == TEXT("Table") || PropertyThatChanged->GetName() == TEXT("Structure"))
		{
			ParamUIMetadataColumn = NAME_None;
			VersionColumn = NAME_None;
			ReconstructNode();
		}
		else if (PropertyThatChanged->GetName() == TEXT("DefaultImageMode"))
		{
			bool bReconstruct = false;

			for (UEdGraphPin* Pin : Pins)
			{
				if (UCustomizableObjectNodeTableImagePinData* TexturePinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
				{
					if (!Pin->LinkedTo.Num() && !TexturePinData->IsNotTexture2D() && TexturePinData->IsDefaultImageMode())
					{
						ETableTextureType Mode = TexturePinData->ImageMode == ETableTextureType::PASSTHROUGH_TEXTURE ? ETableTextureType::MUTABLE_TEXTURE : ETableTextureType::PASSTHROUGH_TEXTURE;
						TexturePinData->ImageMode = Mode;

						bReconstruct = true;
					}
				}
			}

			if (bReconstruct)
			{
				ReconstructNode();
			}
		}
		else if (PropertyThatChanged->GetName() == TEXT("TableDataGatheringMode"))
		{
			if (Table)
			{
				Table->OnDataTableChanged().Remove(OnTableChangedDelegateHandle);
			}

			Table = nullptr;
			Structure = nullptr;

			FilterPaths.Empty();

			ReconstructNode();
		}
	}
}


void UCustomizableObjectNodeTable::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	// Reset of the column data map
	TMap<FGuid, FTableNodeColumnData> AuxOldColumnData = ColumnDataMap;
	ColumnDataMap.Reset();

	// Getting Struct Pointer
	const UScriptStruct* TableStruct = GetTableNodeStruct();

	if (!TableStruct)
	{
		return;
	}

	NumProperties = GetColumnTitles(TableStruct).Num();

	// Getting Default Struct Values
	// A Script Struct always has at leaset one property
	TArray<int8> DefaultDataArray;
	DefaultDataArray.SetNumZeroed(TableStruct->GetStructureSize());
	TableStruct->InitializeStruct(DefaultDataArray.GetData());

	TArray<UEdGraphPin*> OldPins(Pins);
	
	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty)
		{
			continue;
		}

		const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

		UEdGraphPin* OutPin = nullptr;

		FString ColumnName = DataTableUtils::GetPropertyExportName(ColumnProperty);
		FString PinName = ColumnName;
		FGuid ColumnPropertyId = FStructureEditorUtils::GetGuidForProperty(ColumnProperty);

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
		{
			UObject* Object = nullptr;

			// Getting default UObject
			uint8* CellData = SoftObjectProperty->ContainerPtrToValuePtr<uint8>(DefaultDataArray.GetData());

			if (CellData)
			{
				Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();
			}

			if (Object)
			{
				if (Object->IsA(USkeletalMesh::StaticClass()) || Object->IsA(UStaticMesh::StaticClass()))
				{
					GenerateMeshPins(Object, ColumnName, ColumnPropertyId);

					if (FTableNodeColumnData* ColumnData = AuxOldColumnData.Find(ColumnPropertyId))
					{
						ColumnDataMap.Add(ColumnPropertyId, *ColumnData);
					}
				}

				else if (Object->IsA(UTexture2D::StaticClass()))
				{
					UCustomizableObjectNodeTableImagePinData* PinData = NewObject<UCustomizableObjectNodeTableImagePinData>(this);
					PinData->ColumnName = ColumnName;
					PinData->StructColumnId = ColumnPropertyId;
					PinData->SetIsNotTexture2D(false);

					FName PinCategory = DefaultImageMode == ETableTextureType::PASSTHROUGH_TEXTURE ? Schema->PC_PassThroughImage : Schema->PC_Image;

					for (UEdGraphPin* Pin : OldPins)
					{
						// Checking if this column already exist
						if (UCustomizableObjectNodeTableImagePinData* OldPinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
						{
							if (OldPinData->ColumnName == ColumnName)
							{
								PinCategory = OldPinData->ImageMode == ETableTextureType::PASSTHROUGH_TEXTURE ? Schema->PC_PassThroughImage : Schema->PC_Image;
								break;
							}
						}
					}

					OutPin = CustomCreatePin(EGPD_Output, PinCategory, FName(*PinName), PinData);
				}

				else if (Object->IsA(UTexture::StaticClass()))
				{
					UCustomizableObjectNodeTableImagePinData* PinData = NewObject<UCustomizableObjectNodeTableImagePinData>(this);
					PinData->ColumnName = ColumnName;
					PinData->StructColumnId = ColumnPropertyId;
					PinData->ImageMode = ETableTextureType::PASSTHROUGH_TEXTURE;
					PinData->SetIsNotTexture2D(true);

					OutPin = CustomCreatePin(EGPD_Output, Schema->PC_PassThroughImage, FName(*PinName), PinData);
				}

				else if (Object->IsA(UMaterialInstance::StaticClass()))
				{
					UCustomizableObjectNodeTableObjectPinData* PinData = NewObject<UCustomizableObjectNodeTableObjectPinData>(this);
					PinData->ColumnName = ColumnName;
					PinData->StructColumnId = ColumnPropertyId;

					OutPin = CustomCreatePin(EGPD_Output, Schema->PC_MaterialAsset, FName(*PinName), PinData);
				}
			}
			else
			{
				const FText Text = FText::FromString(FString::Printf(TEXT("Could not find a Default Value in Structure member [%s]"),*ColumnName));

				FCustomizableObjectEditorLogger::CreateLog(Text)
					.Category(ELoggerCategory::General)
					.Severity(EMessageSeverity::Warning)
					.Context(*this)
					.Log();
			}
		}

		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				UCustomizableObjectNodeTableObjectPinData* PinData = NewObject<UCustomizableObjectNodeTableObjectPinData>(this);
				PinData->ColumnName = ColumnName;
				PinData->StructColumnId = ColumnPropertyId;

				OutPin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName(*PinName), PinData);
			}
		}

		else if (const FNumericProperty* NumFloatProperty = CastField<FFloatProperty>(ColumnProperty))
		{
			UCustomizableObjectNodeTableObjectPinData* PinData = NewObject<UCustomizableObjectNodeTableObjectPinData>(this);
			PinData->ColumnName = ColumnName;
			PinData->StructColumnId = ColumnPropertyId;

			OutPin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName(*PinName), PinData);
		}
		
		else if (const FNumericProperty* NumDoubleProperty = CastField<FDoubleProperty>(ColumnProperty))
		{
			UCustomizableObjectNodeTableObjectPinData* PinData = NewObject<UCustomizableObjectNodeTableObjectPinData>(this);
			PinData->ColumnName = ColumnName;
			PinData->StructColumnId = ColumnPropertyId;

			OutPin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName(*PinName), PinData);
		}

		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ColumnProperty))
		{
			const FText Text = FText::FromString(FString::Printf(TEXT("Asset format not supported in Structure member [%s]. All assets should be Soft References."), *ColumnName));

			FCustomizableObjectEditorLogger::CreateLog(Text)
				.Category(ELoggerCategory::General)
				.Severity(EMessageSeverity::Warning)
				.Context(*this)
				.Log();
		}

		if (OutPin)
		{
			OutPin->PinFriendlyName = FText::FromString(PinName);
			OutPin->PinToolTip = PinName;
			OutPin->SafeSetHidden(false);
		}
	}

	TableStruct->DestroyStruct(DefaultDataArray.GetData());
}


void UCustomizableObjectNodeTable::GenerateMeshPins(UObject* Mesh, const FString& ColumnName, const FGuid& ColumnId)
{
	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
	{
		int NumLODs = SkeletalMesh->GetLODInfoArray().Num();
		const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

		for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			int32 NumMaterials = SkeletalMesh->GetImportedModel()->LODModels[LODIndex].Sections.Num();

			for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
			{
				FString TableMeshPinName = GenerateSkeletalMeshMutableColumName(ColumnName, LODIndex, MatIndex);

				// Pin Data
				UCustomizableObjectNodeTableMeshPinData* PinData = NewObject<UCustomizableObjectNodeTableMeshPinData>(this);
				PinData->ColumnName = ColumnName;
				PinData->StructColumnId = ColumnId;

				// Mesh Data
				PinData->MutableColumnName = TableMeshPinName;
				PinData->LOD = LODIndex;
				PinData->Material = MatIndex;

				UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*TableMeshPinName), PinData);
				MeshPin->PinFriendlyName = FText::FromString(TableMeshPinName);
				MeshPin->SafeSetHidden(false);

				FSkeletalMeshModel* importModel = SkeletalMesh->GetImportedModel();
				if (importModel && importModel->LODModels.Num() > LODIndex)
				{
					const uint32 NumberOfUVLayouts = importModel->LODModels[LODIndex].NumTexCoords;

					for (uint32 LayoutIndex = 0; LayoutIndex < NumberOfUVLayouts; ++LayoutIndex)
					{
						UCustomizableObjectLayout* Layout = NewObject<UCustomizableObjectLayout>(this);
						FString LayoutName = TableMeshPinName;

						if (NumberOfUVLayouts > 1)
						{
							LayoutName += FString::Printf(TEXT(" UV_%d"), LayoutIndex);
						}

						if (Layout)
						{
							PinData->Layouts.Add(Layout);

							Layout->SetLayout(SkeletalMesh, LODIndex, MatIndex, LayoutIndex);
							Layout->SetLayoutName(LayoutName);
						}
					}
				}
			}
		}
	}
	
	else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
	{
		const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

		if (StaticMesh->GetRenderData()->LODResources.Num())
		{
			int32 NumMaterials = StaticMesh->GetRenderData()->LODResources[0].Sections.Num();

			for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
			{
				FString TableMeshPinName = GenerateStaticMeshMutableColumName(ColumnName, MatIndex);

				UCustomizableObjectNodeTableMeshPinData* PinData = NewObject<UCustomizableObjectNodeTableMeshPinData>(this);
				PinData->ColumnName = ColumnName;
				PinData->StructColumnId = ColumnId;

				// Mesh Data
				PinData->MutableColumnName = TableMeshPinName;
				PinData->LOD = 0;
				PinData->Material = MatIndex;

				UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*TableMeshPinName), PinData);
				MeshPin->PinFriendlyName = FText::FromString(TableMeshPinName);
				MeshPin->SafeSetHidden(false);
			}
		}
	}
}


bool UCustomizableObjectNodeTable::IsNodeOutDatedAndNeedsRefresh()
{
	// Getting Struct Pointer
	const UScriptStruct* TableStruct = GetTableNodeStruct();

	if (!TableStruct)
	{
		return Pins.Num() > 0;
	}

	if (NumProperties != GetColumnTitles(TableStruct).Num())
	{
		return true;
	}

	// Getting Default Struct Values
	// A Script Struct always has at leaset one property
	TArray<int8> DefaultDataArray;
	DefaultDataArray.SetNumZeroed(TableStruct->GetStructureSize());
	TableStruct->InitializeStruct(DefaultDataArray.GetData());

	int32  NumPins = 0;

	bool bNeedsUpdate = false;

	for (TFieldIterator<FProperty> It(TableStruct); It && !bNeedsUpdate; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (ColumnProperty)
		{
			const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

			UEdGraphPin* OutPin = nullptr;

			if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
			{
				UObject* Object = nullptr;

				// Getting default UObject
				uint8* CellData = SoftObjectProperty->ContainerPtrToValuePtr<uint8>(DefaultDataArray.GetData());

				if (CellData)
				{
					Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();
				}

				if (!Object)
				{
					continue;
				}

				if (Object->IsA(USkeletalMesh::StaticClass()) || Object->IsA(UStaticMesh::StaticClass()))
				{
					if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
					{
						int NumLODs = SkeletalMesh->GetLODInfoArray().Num();

						for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
						{
							int32 NumMaterials = SkeletalMesh->GetImportedModel()->LODModels[LODIndex].Sections.Num();

							for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
							{
								FString PropertyName = DataTableUtils::GetPropertyExportName(ColumnProperty);
								FString PinName = GenerateSkeletalMeshMutableColumName(PropertyName, LODIndex, MatIndex);

								if (CheckPinUpdated(PinName, Schema->PC_Mesh))
								{
									bNeedsUpdate = true;
								}

								NumPins++;
							}
						}
					}
					else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
					{
						if (StaticMesh->GetRenderData()->LODResources.Num())
						{
							int32 NumMaterials = StaticMesh->GetRenderData()->LODResources[0].Sections.Num();

							for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
							{
								FString PropertyName = DataTableUtils::GetPropertyExportName(ColumnProperty);
								FString PinName = GenerateStaticMeshMutableColumName(PropertyName, MatIndex);

								if (CheckPinUpdated(PinName, Schema->PC_Mesh))
								{
									bNeedsUpdate = true;
								}

								NumPins++;
							}
						}
					}
				}
				else if (Object->IsA(UTexture2D::StaticClass()))
				{
					FString PinName = DataTableUtils::GetPropertyExportName(ColumnProperty);
					
					if (CheckPinUpdated(PinName, Schema->PC_Image) && CheckPinUpdated(PinName, Schema->PC_PassThroughImage))
					{
						bNeedsUpdate = true;
					}

					NumPins++;
				}
				else if (Object->IsA(UTexture2DArray::StaticClass()))
				{
					FString PinName = DataTableUtils::GetPropertyExportName(ColumnProperty);

					if (CheckPinUpdated(PinName, Schema->PC_PassThroughImage))
					{
						bNeedsUpdate = true;
					}

					NumPins++;
				}
				else if (Object->IsA(UMaterialInstance::StaticClass()))
				{
					FString PinName = DataTableUtils::GetPropertyExportName(ColumnProperty);
					
					if (CheckPinUpdated(PinName, Schema->PC_MaterialAsset))
					{
						bNeedsUpdate = true;
					}

					NumPins++;
				}
				
			}
			else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
			{
				if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
				{
					FString PinName = DataTableUtils::GetPropertyExportName(ColumnProperty);

					if (CheckPinUpdated(PinName, Schema->PC_Color))
					{
						bNeedsUpdate = true;
					}

					NumPins++;
				}
			}
			else if (const FFloatProperty* FloatNumProperty = CastField<FFloatProperty>(ColumnProperty))
			{
				FString PinName = DataTableUtils::GetPropertyExportName(ColumnProperty);

				if (CheckPinUpdated(PinName, Schema->PC_Float))
				{
					bNeedsUpdate = true;
				}

				NumPins++;
			}
			else if (const FDoubleProperty* DoubleNumProperty = CastField<FDoubleProperty>(ColumnProperty))
			{
				FString PinName = DataTableUtils::GetPropertyExportName(ColumnProperty);

				if (CheckPinUpdated(PinName, Schema->PC_Float))
				{
					bNeedsUpdate = true;
				}

				NumPins++;
			}
		}
	}

	TableStruct->DestroyStruct(DefaultDataArray.GetData());

	if (Pins.Num() != NumPins)
	{
		bNeedsUpdate = true;
	}

	return bNeedsUpdate;
}


FString UCustomizableObjectNodeTable::GetRefreshMessage() const
{
	return "Node data outdated. Please refresh node.";
}

// TODO(MTBL-1652): Move this to the Copy method of the NodePinData class which is more PinType specific (and the right plece to do this)
void UCustomizableObjectNodeTable::RemapPinsData(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	for (const TTuple<UEdGraphPin*, UEdGraphPin*>& Pair : PinsToRemap)
	{
		if (Pair.Key->PinType.PinCategory == Schema->PC_Mesh)
		{
			UCustomizableObjectNodeTableMeshPinData* PinDataOldPin = Cast<UCustomizableObjectNodeTableMeshPinData>(GetPinData(*(Pair.Key)));
			UCustomizableObjectNodeTableMeshPinData* PinDataNewPin = Cast<UCustomizableObjectNodeTableMeshPinData>(GetPinData(*(Pair.Value)));

			const UScriptStruct* ScriptStruct = GetTableNodeStruct();

			if (PinDataOldPin && PinDataNewPin && ScriptStruct && GetPinMeshType(Pair.Value) == ETableMeshPinType::SKELETAL_MESH)
			{
				// Keeping information added in layout editor if the layout is the same
				for (TObjectPtr<UCustomizableObjectLayout>& NewLayout : PinDataNewPin->Layouts)
				{
					for (TObjectPtr<UCustomizableObjectLayout>& OldLayout : PinDataOldPin->Layouts)
					{
						if (NewLayout->GetLayoutName() == OldLayout->GetLayoutName())
						{
							NewLayout->Blocks = OldLayout->Blocks;
							NewLayout->SetGridSize(OldLayout->GetGridSize());
							NewLayout->SetMaxGridSize(OldLayout->GetMaxGridSize());
							NewLayout->SetPackingStrategy(OldLayout->GetPackingStrategy());
							NewLayout->SetBlockReductionMethod(OldLayout->GetBlockReductionMethod());

							break;
						}
					}
				}
			}
		}
		else if (Pair.Key->PinType.PinCategory == Schema->PC_Image || Pair.Key->PinType.PinCategory == Schema->PC_PassThroughImage)
		{
			UCustomizableObjectNodeTableImagePinData* PinDataOldPin = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pair.Key)));
			UCustomizableObjectNodeTableImagePinData* PinDataNewPin = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pair.Value)));

			if (PinDataOldPin && PinDataNewPin)
			{
				PinDataNewPin->ImageMode = PinDataOldPin->ImageMode;
				PinDataNewPin->SetDefaultImageMode(PinDataOldPin->IsDefaultImageMode());
				PinDataNewPin->SetIsNotTexture2D(PinDataOldPin->IsNotTexture2D());
			}
		}
	}
}


bool UCustomizableObjectNodeTable::ProvidesCustomPinRelevancyTest() const
{
	return true;
}


bool UCustomizableObjectNodeTable::IsPinRelevant(const UEdGraphPin* Pin) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	
	return Pin->Direction == EGPD_Input &&
		(Pin->PinType.PinCategory == Schema->PC_MaterialAsset ||
		Pin->PinType.PinCategory == Schema->PC_Image ||
		Pin->PinType.PinCategory == Schema->PC_PassThroughImage ||
		Pin->PinType.PinCategory == Schema->PC_Color ||
		Pin->PinType.PinCategory == Schema->PC_Float ||
		Pin->PinType.PinCategory == Schema->PC_Mesh);
}


void UCustomizableObjectNodeTable::SetLayoutInLayoutEditor(UCustomizableObjectLayout* CurrentLayout)
{
	TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

	if (TSharedPtr<FCustomizableObjectEditor> GraphEditor = StaticCastSharedPtr<FCustomizableObjectEditor>(GetGraphEditor()))
	{
		if (GraphEditor->GetLayoutBlocksEditor().IsValid())
		{
			GraphEditor->GetLayoutBlocksEditor()->SetCurrentLayout(CurrentLayout);
		}
	}
}


UTexture2D* UCustomizableObjectNodeTable::FindReferenceTextureParameter(const UEdGraphPin* Pin, FString ParameterImageName) const
{
	UMaterialInterface* Material = GetColumnDefaultAssetByType<UMaterialInterface>(Pin);

	if (Material)
	{
		UTexture* OutTexture;
		bool bFound = Material->GetTextureParameterValue(FName(*ParameterImageName), OutTexture);

		if (bFound)
		{
			if (UTexture2D* Texture = Cast<UTexture2D>(OutTexture))
			{
				return Texture;
			}
		}
	}

	return nullptr;
}


void UCustomizableObjectNodeTable::GetUVChannel(const UCustomizableObjectLayout* CurrentLayout, TArray<FVector2f>& OutSegments) const
{
	FString ColumnName;

	for (const UEdGraphPin* Pin : Pins)
	{
		const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData > (GetPinData(*Pin));
		
		if (!PinData)
		{
			continue;
		}

		bool bFound = false;

		for (const UCustomizableObjectLayout* Layout : PinData->Layouts)
		{
			if (Layout == CurrentLayout)
			{
				ColumnName = PinData->ColumnName;
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			break;
		}
	}

	const int32 LODIndex = CurrentLayout->GetLOD();
	const int32 MaterialIndex = CurrentLayout->GetMaterial();
	const int32 LayoutIndex = CurrentLayout->GetUVChannel();

	if (const USkeletalMesh* SkeletalMesh = GetColumnDefaultAssetByType<USkeletalMesh>(ColumnName))
	{
		OutSegments = GetUV(*SkeletalMesh, LODIndex, MaterialIndex, LayoutIndex);
	}
	else if (const UStaticMesh* StaticMesh = GetColumnDefaultAssetByType<UStaticMesh>(ColumnName))
	{
		OutSegments = GetUV(*StaticMesh, LODIndex, MaterialIndex, LayoutIndex);
	}
}


void UCustomizableObjectNodeTable::GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVChannel) const
{
	const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(GetPinData(*Pin));

	for (int32 LayoutIndex = 0; LayoutIndex < PinData->Layouts.Num(); ++LayoutIndex)
	{
		if (PinData->Layouts[LayoutIndex]->GetUVChannel() == UVChannel)
		{
			// Getting LOD and Mat index from Layouts
			const uint32 LODIndex = PinData->Layouts[LayoutIndex]->GetLOD();
			const uint32 MaterialIndex = PinData->Layouts[LayoutIndex]->GetMaterial();

			if (const USkeletalMesh* SkeletalMesh = GetColumnDefaultAssetByType<USkeletalMesh>(PinData->ColumnName))
			{
				OutSegments.Append(GetUV(*SkeletalMesh, LODIndex, MaterialIndex, LayoutIndex));
			}
			else if (const UStaticMesh* StaticMesh = GetColumnDefaultAssetByType<UStaticMesh>(PinData->ColumnName))
			{
				OutSegments.Append(GetUV(*StaticMesh, LODIndex, MaterialIndex, LayoutIndex));
			}
		}
	}
}


TArray<UCustomizableObjectLayout*> UCustomizableObjectNodeTable::GetLayouts(const UEdGraphPin* Pin) const
{
	TArray<UCustomizableObjectLayout*> Result;

	const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(GetPinData(*Pin));

	if (PinData)
	{
		for (int32 LayoutIndex = 0; LayoutIndex < PinData->Layouts.Num(); ++LayoutIndex)
		{
			Result.Add(PinData->Layouts[LayoutIndex]);
		}
	}
	
	return Result;
}


FString UCustomizableObjectNodeTable::GetColumnNameByPin(const UEdGraphPin* Pin) const
{
	const UCustomizableObjectNodeTableObjectPinData* PinData = Cast<UCustomizableObjectNodeTableObjectPinData >(GetPinData(*Pin));
	
	if (PinData)
	{
		return PinData->ColumnName;
	}

	return FString();
}


void UCustomizableObjectNodeTable::GetPinLODAndSection(const UEdGraphPin* Pin, int32& LODIndex, int32& SectionIndex) const
{
	if (const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(GetPinData(*Pin)))
	{
		LODIndex = PinData->LOD;
		SectionIndex = PinData->Material;
	}
	else
	{
		LODIndex = -1;
		SectionIndex = -1;
	}
}


void UCustomizableObjectNodeTable::GetAnimationColumns(const FGuid& ColumnId, FString& AnimBPColumnName, FString& AnimSlotColumnName, FString& AnimTagColumnName) const
{
	if (ColumnDataMap.Contains(ColumnId))
	{
		const FTableNodeColumnData* ColumnData = ColumnDataMap.Find(ColumnId);
		AnimBPColumnName = ColumnData->AnimInstanceColumnName;
		AnimSlotColumnName = ColumnData->AnimSlotColumnName;
		AnimTagColumnName = ColumnData->AnimTagColumnName;
	}
}


void UCustomizableObjectNodeTable::OnTableChanged()
{	
	for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (Pin->Direction == EGPD_Output)
		{
			for (const UEdGraphPin* ConnectedPin : FollowOutputPinArray(*Pin))
        	{
        		if (UCustomizableObjectNodeMaterial* NodeMaterial = Cast<UCustomizableObjectNodeMaterial>(ConnectedPin->GetOwningNode()))
        		{
        			NodeMaterial->UpdateAllImagesPinMode();
        		}
        	}
		}
	}
}


bool UCustomizableObjectNodeTable::CheckPinUpdated(const FString& PinName, const FName& PinType) const
{
	if (UEdGraphPin* Pin = FindPin(PinName))
	{
		if (Pin->PinType.PinCategory != PinType)
		{
			return true;
		}
	}
	else
	{
		return true;
	}

	return false;
}


USkeletalMesh* UCustomizableObjectNodeTable::GetSkeletalMeshAt(const UEdGraphPin* Pin, const UDataTable* DataTable, const FName& RowName) const
{
	if (!DataTable || !DataTable->GetRowStruct() || !Pin || !DataTable->GetRowNames().Contains(RowName))
	{
		return nullptr;
	}
	
	const UScriptStruct* TableStruct = DataTable->GetRowStruct();

	FString ColumnName = GetColumnNameByPin(Pin);

	// Here we are using the DataTable function because we are only calling GetSkeletalMeshAt() at Generation Time
	// If we want to use it elswhere, we can use our FindTablreProperty method
	FProperty* ColumnProperty = DataTable->FindTableProperty(FName(*ColumnName));

	if (!ColumnProperty)
	{
		return nullptr;
	}

	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
	{
		if (uint8* RowData = DataTable->FindRowUnchecked(RowName))
		{
			if (uint8* CellData = ColumnProperty->ContainerPtrToValuePtr<uint8>(RowData, 0))
			{
				if (UObject* Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous())
				{
					if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
					{
						return SkeletalMesh;
					}
				}
			}
		}
	}

	return nullptr;
}


TSoftClassPtr<UAnimInstance> UCustomizableObjectNodeTable::GetAnimInstanceAt(const UEdGraphPin* Pin, const UDataTable* DataTable, const FName& RowName) const
{
	if (!DataTable || !DataTable->GetRowStruct() || !Pin || !DataTable->GetRowNames().Contains(RowName))
	{
		return TSoftClassPtr<UAnimInstance>();
	}

	const UCustomizableObjectNodeTableObjectPinData* PinData = Cast<UCustomizableObjectNodeTableObjectPinData>(GetPinData(*Pin));

	if (!PinData)
	{
		return TSoftClassPtr<UAnimInstance>();
	}

	if (const FTableNodeColumnData* ColumnData = ColumnDataMap.Find(PinData->StructColumnId))
	{
		FString AnimColumn = ColumnData->AnimInstanceColumnName;

		// Here we are using the DataTable function because we are only calling GetAnimInstanceAt() at Generation Time
		// If we want to use it elswhere, we can use our FindTablreProperty method
		FProperty* ColumnProperty = DataTable->FindTableProperty(FName(*AnimColumn));

		if (!ColumnProperty)
		{
			return TSoftClassPtr<UAnimInstance>();
		}

		if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(ColumnProperty))
		{
			if (uint8* RowData = DataTable->FindRowUnchecked(RowName))
			{
				if (uint8* CellData = ColumnProperty->ContainerPtrToValuePtr<uint8>(RowData, 0))
				{
					TSoftClassPtr<UAnimInstance> AnimInstance(SoftClassProperty->GetPropertyValue(CellData).ToSoftObjectPath());

					if (!AnimInstance.IsNull())
					{
						return AnimInstance;
					}
				}
			}
		}
	}

	return TSoftClassPtr<UAnimInstance>();
}


void UCustomizableObjectNodeTableImagePinData::ConvertArrayTextureToAnyTextureFixup()
{
	if (bIsArrayTexture_DEPRECATED)
	{
		bIsNotTexture2D = true;
		bIsArrayTexture_DEPRECATED = false;
	}
}


void UCustomizableObjectNodeTable::ChangeImagePinMode(UEdGraphPin* Pin, bool bSetDefault)
{
	if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
	{
		if (PinData->IsNotTexture2D())
		{
			return;
		}

		if (PinData->IsDefaultImageMode() && !bSetDefault)
		{
			ETableTextureType Mode = DefaultImageMode == ETableTextureType::PASSTHROUGH_TEXTURE ? ETableTextureType::MUTABLE_TEXTURE : ETableTextureType::PASSTHROUGH_TEXTURE;
			PinData->ImageMode = Mode;
			PinData->SetDefaultImageMode(false);

			ReconstructNode();
		}
		else
		{
			if (bSetDefault)
			{
				PinData->ImageMode = DefaultImageMode;
				PinData->SetDefaultImageMode(true);
			}
			else
			{
				ETableTextureType Mode = PinData->ImageMode == ETableTextureType::PASSTHROUGH_TEXTURE ? ETableTextureType::MUTABLE_TEXTURE : ETableTextureType::PASSTHROUGH_TEXTURE;
				PinData->ImageMode = Mode;
			}

			ReconstructNode();
		}
	}
}


UCustomizableObjectNodeTableRemapPins* UCustomizableObjectNodeTable::CreateRemapPinsDefault() const
{
	return NewObject<UCustomizableObjectNodeTableRemapPins>();
}


bool UCustomizableObjectNodeTable::IsImagePinDefault(const UEdGraphPin* Pin) const
{
	if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
	{
		return PinData->IsDefaultImageMode();
	}

	return true;
}


ETableTextureType UCustomizableObjectNodeTable::GetColumnImageMode(const FString& ColumnName) const
{
	for (const UEdGraphPin* Pin : Pins)
	{
		if (const UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
		{
			if (PinData->ColumnName == ColumnName)
			{
				return PinData->ImageMode;
			}
		}
	}

	return DefaultImageMode;
}


bool UCustomizableObjectNodeTable::IsNotTexture2DPin(const UEdGraphPin* Pin) const
{
	if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
	{
		return PinData->IsNotTexture2D();
	}

	return false;
}


ETableMeshPinType UCustomizableObjectNodeTable::GetPinMeshType(const UEdGraphPin* Pin) const
{
	// Getting Struct Pointer
	const UScriptStruct* TableStruct = GetTableNodeStruct();

	if (TableStruct && Pin && Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Mesh)
	{
		FString ColumnName = GetColumnNameByPin(Pin);
		FProperty* Property = FindTableProperty(TableStruct, FName(*ColumnName));

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		{
			if (SoftObjectProperty->PropertyClass->IsChildOf(USkeletalMesh::StaticClass()))
			{
				return ETableMeshPinType::SKELETAL_MESH;
			}
			else if (SoftObjectProperty->PropertyClass->IsChildOf(UStaticMesh::StaticClass()))
			{
				return ETableMeshPinType::STATIC_MESH;
			}
		}
	}

	return ETableMeshPinType::NONE;
}


FString UCustomizableObjectNodeTable::GenerateSkeletalMeshMutableColumName(const FString& PinName, int32 LODIndex, int32 MaterialIndex) const
{
	return PinName + FString::Printf(TEXT(" LOD_%d "), LODIndex) + FString::Printf(TEXT("Mat_%d"), MaterialIndex);
}


FString UCustomizableObjectNodeTable::GenerateStaticMeshMutableColumName(const FString& PinName, int32 MaterialIndex) const
{
	return PinName + FString::Printf(TEXT(" Mat_%d"), MaterialIndex);
}


const UScriptStruct* UCustomizableObjectNodeTable::GetTableNodeStruct() const
{
	const UScriptStruct* TableStruct = nullptr;

	if (TableDataGatheringMode == ETableDataGatheringSource::ETDGM_AssetRegistry)
	{
		if (Structure)
		{
			TableStruct = Structure;
		}
	}
	else
	{
		if (Table)
		{
			TableStruct = Table->GetRowStruct();
		}
	}

	return TableStruct;
}


TArray<FString> UCustomizableObjectNodeTable::GetColumnTitles(const UScriptStruct* ScriptStruct) const
{
	TArray<FString> Result;
	Result.Add(TEXT("Name"));
	if (ScriptStruct)
	{
		for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
		{
			FProperty* Prop = *It;
			check(Prop != nullptr);
			const FString DisplayName = DataTableUtils::GetPropertyExportName(Prop);
			Result.Add(DisplayName);
		}
	}

	return Result;
}


FProperty* UCustomizableObjectNodeTable::FindTableProperty(const UScriptStruct* ScriptStruct, const FName& PropertyName) const
{
	FProperty* Property = nullptr;

	if (ScriptStruct)
	{
		Property = ScriptStruct->FindPropertyByName(PropertyName);
		if (Property == nullptr && ScriptStruct->IsA<UUserDefinedStruct>())
		{
			const FString PropertyNameStr = PropertyName.ToString();

			for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
			{
				if (PropertyNameStr == ScriptStruct->GetAuthoredNameForField(*It))
				{
					Property = *It;
					break;
				}
			}
		}
		if (!DataTableUtils::IsSupportedTableProperty(Property))
		{
			Property = nullptr;
		}
	}

	return Property;
}


FGuid UCustomizableObjectNodeTable::GetColumnIdByName(const FName& ColumnName) const
{
	if (const UScriptStruct* TableStruct = GetTableNodeStruct())
	{
		if(FProperty* ColumnProperty = FindTableProperty(TableStruct, ColumnName))
		{
			return FStructureEditorUtils::GetGuidForProperty(ColumnProperty);
		}
		else
		{
			// We could not find the property by name
			for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
			{
				if (UCustomizableObjectNodeTableMeshPinData* PinData = Cast< UCustomizableObjectNodeTableMeshPinData>(GetPinData(*Pin)))
				{
					if (PinData->ColumnName != ColumnName)
					{
						continue;
					}

					return PinData->StructColumnId;
				}
			}
		}
	}

	return FGuid();
}


TArray<FAssetData> UCustomizableObjectNodeTable::GetParentTables() const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.GetRegistry();
	
	TArray<FName> ReferencedTables;
	AssetRegistry.Get()->GetReferencers(Structure.GetPackage().GetFName(), ReferencedTables, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::NoRequirements);
	
	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(UDataTable::StaticClass()));

	for (const FName& ReferencedTable : ReferencedTables)
	{
		Filter.PackageNames.Add(ReferencedTable);
	}

	for (int32 PathIndex = 0; PathIndex < FilterPaths.Num(); ++PathIndex)
	{
		Filter.PackagePaths.Add(FilterPaths[PathIndex]);
	}

	Filter.bRecursivePaths = true;

	TArray<FAssetData> DataTableAssets;
	AssetRegistry.Get()->GetAssets(Filter, DataTableAssets);

	return DataTableAssets;
}


#undef LOCTEXT_NAMESPACE
