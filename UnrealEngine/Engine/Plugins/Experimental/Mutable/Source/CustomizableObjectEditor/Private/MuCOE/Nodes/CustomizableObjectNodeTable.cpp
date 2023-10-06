// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2DArray.h"
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

class ICustomizableObjectEditor;
class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


bool UCustomizableObjectNodeTableRemapPins::Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const
{
	if (OldPin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image ||
		OldPin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_PassThroughImage)
	{
		// In this case pin type may have changed but we consider them the same type
		return Helper_GetPinName(&OldPin) == Helper_GetPinName(&NewPin) &&
			OldPin.Direction == NewPin.Direction;
	}

	return Helper_GetPinName(&OldPin) == Helper_GetPinName(&NewPin) &&
		OldPin.PinType == NewPin.PinType &&
		OldPin.Direction == NewPin.Direction;
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

		return FText::Format(LOCTEXT("TableNode_Title", "{TableName}\nTable"), Args);
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
			if (!ImagePinData->IsArrayTexture() && ImagePinData->IsDefaultImageMode() && ImagePinData->ImageMode != DefaultImageMode)
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
		if (PropertyThatChanged->GetName() == TEXT("Table"))
		{
			ReconstructNode();
		}
		else if (PropertyThatChanged->GetName() == TEXT("DefaultImageMode"))
		{
			bool bReconstruct = false;

			for (UEdGraphPin* Pin : Pins)
			{
				if (UCustomizableObjectNodeTableImagePinData* TexturePinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
				{
					if (!Pin->LinkedTo.Num() && !TexturePinData->IsArrayTexture() && TexturePinData->IsDefaultImageMode())
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
	}
}


void UCustomizableObjectNodeTable::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	if (!Table)
	{
		return;
	}

	// Getting Struct Pointer
	const UScriptStruct* TableStruct = Table->GetRowStruct();

	if (!TableStruct)
	{
		return;
	}

	NumProperties = Table->GetColumnTitles().Num();

	// Getting Default Struct Values
	uint8* DefaultRowData = (uint8*)FMemory::Malloc(TableStruct->GetStructureSize());
	
	if (!DefaultRowData)
	{
		return;
	}

	TArray<UEdGraphPin*> OldPins(Pins);

	TableStruct->InitializeStruct(DefaultRowData);
	
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

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
		{
			UObject* Object = nullptr;

			// Getting default UObject
			uint8* CellData = SoftObjectProperty->ContainerPtrToValuePtr<uint8>(DefaultRowData);

			if (CellData)
			{
				Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();
			}

			if (Object)
			{
				if (Object->IsA(USkeletalMesh::StaticClass()) || Object->IsA(UStaticMesh::StaticClass()))
				{
					GenerateMeshPins(Object, ColumnName);
				}

				else if (Object->IsA(UTexture2D::StaticClass()))
				{
					UCustomizableObjectNodeTableImagePinData* PinData = NewObject<UCustomizableObjectNodeTableImagePinData>(this);
					PinData->ColumnName = ColumnName;
					PinData->SetIsArrayTexture(false);

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

				else if (Object->IsA(UTexture2DArray::StaticClass()))
				{
					UCustomizableObjectNodeTableImagePinData* PinData = NewObject<UCustomizableObjectNodeTableImagePinData>(this);
					PinData->ColumnName = ColumnName;
					PinData->ImageMode = ETableTextureType::PASSTHROUGH_TEXTURE;
					PinData->SetIsArrayTexture(true);

					OutPin = CustomCreatePin(EGPD_Output, Schema->PC_PassThroughImage, FName(*PinName), PinData);
				}

				else if (Object->IsA(UMaterialInstance::StaticClass()))
				{
					UCustomizableObjectNodeTableObjectPinData* PinData = NewObject<UCustomizableObjectNodeTableObjectPinData>(this);
					PinData->ColumnName = ColumnName;

					OutPin = CustomCreatePin(EGPD_Output, Schema->PC_MaterialAsset, FName(*PinName), PinData);
				}
			}
			else
			{
				const FText Text = FText::FromString(FString::Printf(TEXT("Could not find a Default Value in Structure member [%s]"),*ColumnName));

				FCustomizableObjectEditorLogger::CreateLog(Text)
					.Category(ELoggerCategory::General)
					.Severity(EMessageSeverity::Warning)
					.Node(*this)
					.Log();
			}
		}

		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				OutPin = CustomCreatePin(EGPD_Output, Schema->PC_Color, FName(*PinName));
			}
		}

		else if (const FNumericProperty* NumFloatProperty = CastField<FFloatProperty>(ColumnProperty))
		{
			OutPin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName(*PinName));
		}
		
		else if (const FNumericProperty* NumDoubleProperty = CastField<FDoubleProperty>(ColumnProperty))
		{
			OutPin = CustomCreatePin(EGPD_Output, Schema->PC_Float, FName(*PinName));
		}

		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ColumnProperty))
		{
			const FText Text = FText::FromString(FString::Printf(TEXT("Asset format not supported in Structure member [%s]. All assets should be Soft References."), *ColumnName));

			FCustomizableObjectEditorLogger::CreateLog(Text)
				.Category(ELoggerCategory::General)
				.Severity(EMessageSeverity::Warning)
				.Node(*this)
				.Log();
		}

		if (OutPin)
		{
			OutPin->PinFriendlyName = FText::FromString(PinName);
			OutPin->PinToolTip = PinName;
			OutPin->SafeSetHidden(false);
		}
	}

	// Cleaning Default Structure Pointer
	TableStruct->DestroyStruct(DefaultRowData);
	FMemory::Free(DefaultRowData);
}


void UCustomizableObjectNodeTable::GenerateMeshPins(UObject* Mesh, const FString& Name)
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
				FString TableMeshPinName = GenerateSkeletalMeshMutableColumName(Name, LODIndex, MatIndex);

				UCustomizableObjectNodeTableMeshPinData* PinData = NewObject<UCustomizableObjectNodeTableMeshPinData>(this);
				PinData->ColumnName = Name;
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
				FString TableMeshPinName = GenerateStaticMeshMutableColumName(Name, MatIndex);

				UCustomizableObjectNodeTableMeshPinData* PinData = NewObject<UCustomizableObjectNodeTableMeshPinData>(this);
				PinData->ColumnName = Name;
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
	if (!Table)
	{
		return Pins.Num() > 0;
	}

	// Getting Struct Pointer
	const UScriptStruct* TableStruct = Table->GetRowStruct();

	if (!TableStruct)
	{
		return Pins.Num() > 0;
	}

	if (NumProperties != Table->GetColumnTitles().Num())
	{
		return true;
	}

	// Getting Default Struct 
	uint8* DefaultRowData = (uint8*)FMemory::Malloc(TableStruct->GetStructureSize());

	if (!DefaultRowData)
	{
		return Pins.Num() > 0;
	}

	TableStruct->InitializeStruct(DefaultRowData);

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
				uint8* CellData = SoftObjectProperty->ContainerPtrToValuePtr<uint8>(DefaultRowData);

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

	// Cleaning Default Structure Pointer
	TableStruct->DestroyStruct(DefaultRowData);
	FMemory::Free(DefaultRowData);

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


void UCustomizableObjectNodeTable::RemapPinsData(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	for (const TTuple<UEdGraphPin*, UEdGraphPin*>& Pair : PinsToRemap)
	{
		if (Pair.Key->PinType.PinCategory == Schema->PC_Mesh)
		{
			UCustomizableObjectNodeTableMeshPinData* PinDataOldPin = Cast<UCustomizableObjectNodeTableMeshPinData>(GetPinData(*(Pair.Key)));
			UCustomizableObjectNodeTableMeshPinData* PinDataNewPin = Cast<UCustomizableObjectNodeTableMeshPinData>(GetPinData(*(Pair.Value)));

			if (PinDataOldPin && PinDataNewPin)
			{
				if (Table->FindTableProperty(FName(*PinDataOldPin->AnimInstanceColumnName)))
				{
					PinDataNewPin->AnimInstanceColumnName = PinDataOldPin->AnimInstanceColumnName;
				}

				if (Table->FindTableProperty(FName(*PinDataOldPin->AnimSlotColumnName)))
				{
					PinDataNewPin->AnimSlotColumnName = PinDataOldPin->AnimSlotColumnName;
				}

				if (Table->FindTableProperty(FName(*PinDataOldPin->AnimTagColumnName)))
				{
					PinDataNewPin->AnimTagColumnName = PinDataOldPin->AnimTagColumnName;
				}

				if (GetPinMeshType(Pair.Value) == ETableMeshPinType::SKELETAL_MESH)
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
		}
		else if (Pair.Key->PinType.PinCategory == Schema->PC_Image || Pair.Key->PinType.PinCategory == Schema->PC_PassThroughImage)
		{
			UCustomizableObjectNodeTableImagePinData* PinDataOldPin = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pair.Key)));
			UCustomizableObjectNodeTableImagePinData* PinDataNewPin = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pair.Value)));

			if (PinDataOldPin && PinDataNewPin)
			{
				PinDataNewPin->ImageMode = PinDataOldPin->ImageMode;
				PinDataNewPin->SetDefaultImageMode(PinDataOldPin->IsDefaultImageMode());
				PinDataNewPin->SetIsArrayTexture(PinDataOldPin->IsArrayTexture());
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


FString UCustomizableObjectNodeTable::GetMutableColumnName(const UEdGraphPin* Pin, const int32& LOD) const
{
	UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(GetPinData(*Pin));

	if (PinData)
	{
		FString ColumnName = PinData->ColumnName;
		int32 MaterialIndex = PinData->Material;

		for (const UEdGraphPin* NodePin : Pins)
		{
			const UCustomizableObjectNodeTableMeshPinData* MeshPinData = Cast<UCustomizableObjectNodeTableMeshPinData >(GetPinData(*NodePin));

			if (MeshPinData && MeshPinData->ColumnName == ColumnName && MeshPinData->LOD == LOD && MeshPinData->Material == MaterialIndex)
			{
				return MeshPinData->MutableColumnName;
			}
		}
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


void UCustomizableObjectNodeTable::GetAnimationColumns(const FString& ColumnName, FString& AnimBPColumnName, FString& AnimSlotColumnName, FString& AnimTagColumnName) const
{
	for (const UEdGraphPin* Pin : Pins)
	{
		const UCustomizableObjectNodeTableMeshPinData* PinData = Cast<UCustomizableObjectNodeTableMeshPinData >(GetPinData(*Pin));

		if (PinData)
		{
			if (PinData->ColumnName == ColumnName)
			{
				AnimBPColumnName = PinData->AnimInstanceColumnName;
				AnimSlotColumnName = PinData->AnimSlotColumnName;
				AnimTagColumnName = PinData->AnimTagColumnName;
			}
		}
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


USkeletalMesh* UCustomizableObjectNodeTable::GetSkeletalMeshAt(const UEdGraphPin* Pin, const FName& RowName) const
{
	if (!Table || !Table->GetRowStruct() || !Pin || !Table->GetRowNames().Contains(RowName))
	{
		return nullptr;
	}

	// Getting Struct Pointer
	const UScriptStruct* TableStruct = Table->GetRowStruct();

	FString ColumnName = GetColumnNameByPin(Pin);

	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty || ColumnName != DataTableUtils::GetPropertyExportName(ColumnProperty))
		{
			continue;
		}

		if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
		{
			if (uint8* RowData = Table->FindRowUnchecked(RowName))
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
	}

	return nullptr;
}

TSoftClassPtr<UAnimInstance> UCustomizableObjectNodeTable::GetAnimInstanceAt(const UEdGraphPin* Pin, const FName& RowName) const
{
	if (!Table || !Table->GetRowStruct() || !Pin || !Table->GetRowNames().Contains(RowName))
	{
		return TSoftClassPtr<UAnimInstance>();
	}

	// Getting Struct Pointer
	const UScriptStruct* TableStruct = Table->GetRowStruct();

	FString ColumnName = GetColumnNameByPin(Pin);

	for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
	{
		FProperty* ColumnProperty = *It;

		if (!ColumnProperty || ColumnName != DataTableUtils::GetPropertyExportName(ColumnProperty))
		{
			continue;
		}

		if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(ColumnProperty))
		{
			TSoftClassPtr<UAnimInstance> AnimInstance(SoftClassProperty->GetPropertyValue(SoftClassProperty).ToSoftObjectPath());

			if (!AnimInstance.IsNull())
			{
				return AnimInstance;
			}
		}
	}

	return TSoftClassPtr<UAnimInstance>();
}

TArray<FName> UCustomizableObjectNodeTable::GetRowNames() const
{
	TArray<FName> RowNames;

	if (Table)
	{
		const UScriptStruct* TableStruct = Table->GetRowStruct();

		if (!TableStruct)
		{
			return RowNames;
		}

		TArray<FName> TableRowNames = Table->GetRowNames();
		FBoolProperty* BoolProperty = nullptr;

		for (TFieldIterator<FProperty> PropertyIt(TableStruct); PropertyIt && bDisableCheckedRows; ++PropertyIt)
		{
			BoolProperty = CastField<FBoolProperty>(*PropertyIt);

			if (BoolProperty)
			{
				for (const FName& RowName : TableRowNames)
				{
					if (uint8* RowData = Table->FindRowUnchecked(RowName))
					{
						if (uint8* CellData = BoolProperty->ContainerPtrToValuePtr<uint8>(RowData, 0))
						{
							if (!BoolProperty->GetPropertyValue(CellData))
							{
								RowNames.Add(RowName);
							}
						}
					}
				}

				// There should be only one Bool column
				break;
			}
		}

		// There is no Bool column or we don't want to disable rows
		if (!BoolProperty)
		{
			return TableRowNames;
		}
	}

	return RowNames;
}


void UCustomizableObjectNodeTable::ChangeImagePinMode(UEdGraphPin* Pin, bool bSetDefault)
{
	if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
	{
		if (PinData->IsArrayTexture())
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


bool UCustomizableObjectNodeTable::IsImagePinDefault(UEdGraphPin* Pin)
{
	if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
	{
		return PinData->IsDefaultImageMode();
	}

	return true;
}


bool UCustomizableObjectNodeTable::IsImageArrayPin(UEdGraphPin* Pin)
{
	if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
	{
		return PinData->IsArrayTexture();
	}

	return false;
}


ETableTextureType UCustomizableObjectNodeTable::GetColumnImageMode(const FString& ColumnName) const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (UCustomizableObjectNodeTableImagePinData* PinData = Cast<UCustomizableObjectNodeTableImagePinData>(GetPinData(*(Pin))))
		{
			if (PinData->ColumnName == ColumnName)
			{
				return PinData->ImageMode;
			}
		}
	}

	return DefaultImageMode;
}


ETableMeshPinType UCustomizableObjectNodeTable::GetPinMeshType(const UEdGraphPin* Pin) const
{
	if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Mesh)
	{
		FString ColumnName = GetColumnNameByPin(Pin);
		FProperty* Property = Table->FindTableProperty(FName(*ColumnName));

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

#undef LOCTEXT_NAMESPACE
