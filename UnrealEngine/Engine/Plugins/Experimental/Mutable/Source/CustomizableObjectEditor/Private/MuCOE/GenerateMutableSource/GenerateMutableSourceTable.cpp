// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"

#include "Animation/AnimInstance.h"
#include "Containers/Array.h"
#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Containers/StringConv.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "MaterialTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Math/Color.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuR/Mesh.h"
#include "MuR/Ptr.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "StaticMeshResources.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void FillTableColumn(const UEdGraphPin* Pin, mu::TablePtr MutableTable, FString ColumnName, FString RowName, int32 RowIdx, uint8* CellData, FProperty* Property, FMutableGraphGenerationContext& GenerationContext)
{
	UCustomizableObjectNodeTable* CustomNodeTable = Cast<UCustomizableObjectNodeTable>(Pin->GetOwningNode());
	int32 CurrentColumn;

	// Getting property type
	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		UObject* Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();

		if (!Object)
		{
			return;
		}

		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
		{
			// Getting Animation Blueprint and Animation Slot
			FString AnimBP, AnimSlot, GameplayTag, AnimBPAssetTag;
			TArray<FGameplayTag> GameplayTags;

			CustomNodeTable->GetAnimationColumns(ColumnName, AnimBP, AnimSlot, GameplayTag);

			if (!AnimBP.IsEmpty())
			{
				if (!AnimSlot.IsEmpty())
				{
					if (CustomNodeTable->Table)
					{
						uint8* AnimRowData = CustomNodeTable->Table->FindRowUnchecked(FName(*RowName));

						if (AnimRowData)
						{
							int32 SlotIndex = -1;

							// Getting animation slot row value from data table
							if (FProperty* AnimSlotProperty = CustomNodeTable->Table->FindTableProperty(FName(*AnimSlot)))
							{
								uint8* AnimSlotData = AnimSlotProperty->ContainerPtrToValuePtr<uint8>(AnimRowData, 0);

								if (AnimSlotData)
								{
									if (const FIntProperty* IntProperty = CastField<FIntProperty>(AnimSlotProperty))
									{
										SlotIndex = IntProperty->GetPropertyValue(AnimSlotData);
									}
								}
							}

							if (SlotIndex > -1)
							{
								// Getting animation instance soft class from data table
								if (FProperty* AnimBPProperty = CustomNodeTable->Table->FindTableProperty(FName(*AnimBP)))
								{
									uint8* AnimBPData = AnimBPProperty->ContainerPtrToValuePtr<uint8>(AnimRowData, 0);

									if (AnimBPData)
									{
										if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(AnimBPProperty))
										{
											TSoftClassPtr<UAnimInstance> AnimInstance(SoftClassProperty->GetPropertyValue(AnimBPData).ToSoftObjectPath());

											if (AnimInstance)
											{
												GenerationContext.AnimBPAssetsMap.Add(AnimInstance.ToString(), AnimInstance);

												AnimBPAssetTag = GenerateAnimationInstanceTag(AnimInstance.ToString(), SlotIndex);
											}
										}
									}
								}
							}
							else
							{
								FString msg = FString::Printf(TEXT("Could not found the Slot column of the animation blueprint column [%s] for the mesh column [%s] row [%s]."), *AnimBP, *ColumnName, *RowName);
								GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
							}
						}
					}
				}
				else
				{
					FString msg = FString::Printf(TEXT("Could not found the Slot column of the animation blueprint column [%s] for the mesh column [%s]."), *AnimBP, *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
				}
			}

			// Getting Gameplay tags
			if (!GameplayTag.IsEmpty())
			{
				if (CustomNodeTable->Table)
				{
					uint8* GameplayRowData = CustomNodeTable->Table->FindRowUnchecked(FName(*RowName));

					if (GameplayRowData)
					{
						// Getting animation tag row value from data table
						if (FProperty* GameplayTagProperty = CustomNodeTable->Table->FindTableProperty(FName(*GameplayTag)))
						{
							uint8* GameplayTagData = GameplayTagProperty->ContainerPtrToValuePtr<uint8>(GameplayRowData, 0);

							if (const FStructProperty* StructProperty = CastField<FStructProperty>(GameplayTagProperty))
							{
								if (StructProperty->Struct == TBaseStructure<FGameplayTagContainer>::Get())
								{
									if (GameplayTagData)
									{
										const FGameplayTagContainer TagContainer = *(FGameplayTagContainer*)GameplayTagData;
										TagContainer.GetGameplayTagArray(GameplayTags);
									}
								}
							}
						}
					}
				}
			}

			// Getting reference Mesh
			USkeletalMesh* ReferenceSkeletalMesh = CustomNodeTable->GetColumnDefaultAssetByType<USkeletalMesh>(ColumnName);

			if (!ReferenceSkeletalMesh)
			{
				FString msg = FString::Printf(TEXT("Reference Skeletal Mesh not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);

				return;
			}

			int ReferenceNumLODs = 0;
			ReferenceNumLODs = Helper_GetLODInfoArray(ReferenceSkeletalMesh).Num();

			int NumLODs = Helper_GetLODInfoArray(SkeletalMesh).Num();

			if (NumLODs != ReferenceNumLODs)
			{
				FString Dif_1 = NumLODs > ReferenceNumLODs ? "more" : "less";
				FString Dif_2 = NumLODs > ReferenceNumLODs ? "Some will be ignored" : "LOD " + FString::FromInt(NumLODs - 1) + " will be used instead. This can cause some performance penalties.";

				FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has %s LODs than the reference mesh. %s"), *ColumnName, *RowName, *Dif_1, *Dif_2);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
			}

			// Parameter used for LOD differences
			int32 CurrentLOD = 0;

			// Generating skeletal mesh columns
			for (int32 LODIndex = 0; LODIndex < ReferenceNumLODs; ++LODIndex)
			{
				int32 NumMaterials = Helper_GetImportedModel(SkeletalMesh)->LODModels[CurrentLOD].Sections.Num();
				int32 ReferenceNumMaterials = Helper_GetImportedModel(ReferenceSkeletalMesh)->LODModels[CurrentLOD].Sections.Num();

				if (NumMaterials > ReferenceNumMaterials)
				{
					FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has more Sections than the reference mesh. Some will be ignored"), *ColumnName, *RowName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
				}

				int32 Materials = NumMaterials <= ReferenceNumMaterials ? NumMaterials : ReferenceNumMaterials;

				for (int32 MatIndex = 0; MatIndex < Materials; ++MatIndex)
				{
					FString MutableColumnName = ColumnName + FString::Printf(TEXT(" LOD_%d "), LODIndex) + FString::Printf(TEXT("Mat_%d"), MatIndex);

					CurrentColumn = MutableTable.get()->FindColumn(TCHAR_TO_ANSI(*MutableColumnName));

					if (CurrentColumn == -1)
					{
						CurrentColumn = MutableTable->AddColumn(TCHAR_TO_ANSI(*MutableColumnName), mu::TABLE_COLUMN_TYPE::TCT_MESH);
					}

					mu::MeshPtr MutableMesh = GenerateMutableMesh(SkeletalMesh, CurrentLOD, MatIndex, GenerationContext, CustomNodeTable);

					if (MutableMesh)
					{
 						if (SkeletalMesh->GetPhysicsAsset() && MutableMesh->GetPhysicsBody() && MutableMesh->GetPhysicsBody()->GetBodyCount())
						{	
							TSoftObjectPtr<UPhysicsAsset> PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
							int32 CurrentTagCount = MutableMesh->GetTagCount();
							MutableMesh->SetTagCount(CurrentTagCount + 1);
							GenerationContext.PhysicsAssetMap.Add(PhysicsAsset.ToString(), PhysicsAsset);
							FString PhysicsAssetTag = FString("__PhysicsAsset:") + PhysicsAsset.ToString();
							MutableMesh->SetTag(CurrentTagCount, TCHAR_TO_ANSI(*PhysicsAssetTag));						
						}

						if (!AnimBPAssetTag.IsEmpty())
						{
							int32 CurrentTagCount = MutableMesh->GetTagCount();
							MutableMesh->SetTagCount(CurrentTagCount + 1);
							MutableMesh->SetTag(CurrentTagCount, TCHAR_TO_ANSI(*AnimBPAssetTag));
						}

						for (const FGameplayTag& Tag : GameplayTags)
						{
							const FString AnimBPTag = GenerateGameplayTag(Tag.ToString());
							const int32 CurrentTagCount = MutableMesh->GetTagCount();
							MutableMesh->SetTagCount(CurrentTagCount + 1);
							MutableMesh->SetTag(CurrentTagCount, TCHAR_TO_ANSI(*AnimBPTag));
						}

						MutableTable->SetCell(CurrentColumn, RowIdx, MutableMesh.get());
					}
					else
					{
						FString msg = FString::Printf(TEXT("Error converting skeletal mesh LOD %d, Material %d from column [%s] row [%s] to mutable."),
							LODIndex, MatIndex, *ColumnName, *RowName);
						GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
					}
				}

				CurrentLOD = (CurrentLOD + 1) >= NumLODs ? CurrentLOD : (CurrentLOD + 1);
			}
		}

		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
		{
			// Getting reference Mesh
			UStaticMesh* ReferenceStaticMesh = CustomNodeTable->GetColumnDefaultAssetByType<UStaticMesh>(ColumnName);

			if (!ReferenceStaticMesh)
			{
				FString msg = FString::Printf(TEXT("Reference Static Mesh not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);

				return;
			}

			int NumLODs = StaticMesh->GetRenderData()->LODResources.Num();
			int ReferenceNumLODs = ReferenceStaticMesh->GetRenderData()->LODResources.Num();

			if (NumLODs != ReferenceNumLODs)
			{
				FString Dif_1 = NumLODs > ReferenceNumLODs ? "more" : "less";
				FString Dif_2 = NumLODs > ReferenceNumLODs ? "Some will be ignored" : "LOD " + FString::FromInt(NumLODs - 1) + " will be used instead. This can cause some performance penalties.";

				FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has %s LODs than the reference mesh. %s"), *ColumnName, *RowName, *Dif_1, *Dif_2);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
			}

			// Parameter used for LOD differences
			int32 CurrentLOD = 0;

			for (int32 LODIndex = 0; LODIndex < ReferenceNumLODs; ++LODIndex)
			{
				int32 NumMaterials = StaticMesh->GetRenderData()->LODResources[CurrentLOD].Sections.Num();
				int32 ReferenceNumMaterials = ReferenceStaticMesh->GetRenderData()->LODResources[CurrentLOD].Sections.Num();

				if (NumMaterials > ReferenceNumMaterials)
				{
					FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has more Sections than the reference mesh. Some will be ignored"), *ColumnName, *RowName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
				}

				int32 Materials = NumMaterials <= ReferenceNumMaterials ? NumMaterials : ReferenceNumMaterials;

				for (int32 MatIndex = 0; MatIndex < Materials; ++MatIndex)
				{
					FString MutableColumnName = ColumnName + FString::Printf(TEXT(" LOD_%d "), LODIndex) + FString::Printf(TEXT("Mat_%d"), MatIndex);

					CurrentColumn = MutableTable.get()->FindColumn(TCHAR_TO_ANSI(*MutableColumnName));

					if (CurrentColumn == -1)
					{
						CurrentColumn = MutableTable->AddColumn(TCHAR_TO_ANSI(*MutableColumnName), mu::TABLE_COLUMN_TYPE::TCT_MESH);
					}

					mu::MeshPtr MutableMesh = GenerateMutableMesh(StaticMesh, CurrentLOD, MatIndex, GenerationContext, CustomNodeTable);

					if (MutableMesh)
					{
						MutableTable->SetCell(CurrentColumn, RowIdx, MutableMesh.get());
					}
					else
					{
						FString msg = FString::Printf(TEXT("Error converting skeletal mesh LOD %d, Material %d from column [%s] row [%s] to mutable."),
							LODIndex, MatIndex, *ColumnName, *RowName);

						GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
					}
				}

				CurrentLOD = (CurrentLOD + 1) >= NumLODs ? CurrentLOD : (CurrentLOD + 1);
			}
		}

		else if (UTexture2D* Texture = Cast<UTexture2D>(Object))
		{
			// Getting column index from column name
			CurrentColumn = MutableTable->FindColumn(TCHAR_TO_ANSI(*ColumnName));

			if (CurrentColumn == -1)
			{
				CurrentColumn = MutableTable->AddColumn(TCHAR_TO_ANSI(*ColumnName), mu::TABLE_COLUMN_TYPE::TCT_IMAGE);
			}

			GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(MutableTable, Texture, CustomNodeTable, CurrentColumn, RowIdx));
		}

		else if (UMaterialInstance* Material = Cast<UMaterialInstance>(Object))
		{
			UMaterialInstance* ReferenceMaterial = CustomNodeTable->GetColumnDefaultAssetByType<UMaterialInstance>(ColumnName);
			
			if (!GenerationContext.GeneratedParametersInTables.Contains(CustomNodeTable))
			{
				GenerationContext.GeneratedParametersInTables.Add(CustomNodeTable);
			}

			TArray<FGuid>& Parameters = GenerationContext.GeneratedParametersInTables[CustomNodeTable];

			if (!ReferenceMaterial)
			{
				FString msg = FString::Printf(TEXT("Reference Material not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);

				return;
			}

			if (ReferenceMaterial->GetMaterial() == Material->GetMaterial())
			{
				// Getting parameter Guids
				TArray<FMaterialParameterInfo > ParameterInfos;
				TArray<FGuid> ParameterGuids;

				ReferenceMaterial->GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfos, ParameterGuids);

				// Number of modified texture parameters in the Material Instance
				int32 ModifiedTextureParameters = 0;
				
				for (FTextureParameterValue ReferenceTexture : ReferenceMaterial->TextureParameterValues)
				{
					if (Cast<UTexture2D>(ReferenceTexture.ParameterValue))
					{
						int32 ColumnIndex;

						FString TextureParameterName = ReferenceTexture.ParameterInfo.Name.ToString();
						FString ParameterGuid;
						
						for (int32 i = 0; i < ParameterInfos.Num(); ++i)
						{
							if (ParameterInfos[i].Name == ReferenceTexture.ParameterInfo.Name)
							{
								ParameterGuid = ParameterGuids[i].ToString();
								Parameters.Add(ParameterGuids[i]);

								break;
							}
						}

						// Getting column index from parameter name
						ColumnIndex = MutableTable->FindColumn(TCHAR_TO_ANSI(*ParameterGuid));

						if (ColumnIndex == -1)
						{
							// If there is no column with the parameters name, we generate a new one
							ColumnIndex = MutableTable->AddColumn(TCHAR_TO_ANSI(*ParameterGuid), mu::TABLE_COLUMN_TYPE::TCT_IMAGE);
						}

						UTexture* OutTexture = nullptr;
						UTexture2D* TextureToConvert = nullptr;
						
						// Getting the parameter value from the parent material
						if (Material->GetMaterial()->GetTextureParameterValue(ReferenceTexture.ParameterInfo.Name, OutTexture))
						{
							TextureToConvert = Cast<UTexture2D>(OutTexture);
						}

						// Getting the parameter value from the instance if it has been modified
						for (FTextureParameterValue InstanceTexture : Material->TextureParameterValues)
						{
							if (InstanceTexture.ParameterInfo.Name == ReferenceTexture.ParameterInfo.Name)
							{
								TextureToConvert = Cast<UTexture2D>(InstanceTexture.ParameterValue);
								ModifiedTextureParameters++;
								
								break;
							}
						}

						if (TextureToConvert)
						{
							GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(MutableTable, TextureToConvert, CustomNodeTable, ColumnIndex, RowIdx));
						}
						else
						{
							FString msg = FString::Printf(TEXT("Didn't find Material Texture Parameter [%s] from parent Material of column [%s] row [%s]."), *TextureParameterName, *ColumnName, *RowName);
							GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
						}
					}
				}

				// Checking if all modifiable textures of the material instance have been modified
				if (Material->TextureParameterValues.Num() > ModifiedTextureParameters)
				{
					int32 ParametersDiff = Material->TextureParameterValues.Num() - ModifiedTextureParameters;
					FString msg =
						FString::Printf(TEXT("Material Instance [%s] from column [%s] row [%s] has %d modifiable Textures that will not be modified, they are non-modifiable parameters in the Default Material Instance"),
						*Material->GetName(), *ColumnName, *RowName, ParametersDiff);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
				}

				ParameterInfos.Empty();
				ParameterGuids.Empty();

				ReferenceMaterial->GetAllParameterInfoOfType(EMaterialParameterType::Vector, ParameterInfos, ParameterGuids);

				// Number of modified Vector parameters in the Material Instance
				int32 ModifiedVectorParameters = 0;

				for (FVectorParameterValue ReferenceVector : ReferenceMaterial->VectorParameterValues)
				{
					int32 ColumnIndex;

					FString ParameterGuid;

					for (int32 i = 0; i < ParameterInfos.Num(); ++i)
					{
						if (ParameterInfos[i].Name == ReferenceVector.ParameterInfo.Name)
						{
							ParameterGuid = ParameterGuids[i].ToString();
							Parameters.Add(ParameterGuids[i]);

							break;
						}
					}
					
					// Getting column index from parameter name
					ColumnIndex = MutableTable->FindColumn(TCHAR_TO_ANSI(*ParameterGuid));

					if (ColumnIndex == -1)
					{
						// If there is no column with the parameters name, we generate a new one
						ColumnIndex = MutableTable->AddColumn(TCHAR_TO_ANSI(*ParameterGuid), mu::TABLE_COLUMN_TYPE::TCT_COLOUR);
					}

					// Getting the parameter value from the parent material
					FLinearColor VectorValue;

					Material->GetMaterial()->GetVectorParameterValue(ReferenceVector.ParameterInfo.Name, VectorValue);
					
					// Getting the parameter value from the instance if it has been modified
					for (FVectorParameterValue InstanceVector : Material->VectorParameterValues)
					{
						if (InstanceVector.ParameterInfo.Name == ReferenceVector.ParameterInfo.Name)
						{
							VectorValue = InstanceVector.ParameterValue;
							ModifiedVectorParameters++;
							
							break;
						}
					}

					// Setting cell value
					MutableTable->SetCell(ColumnIndex, RowIdx, VectorValue.R, VectorValue.G, VectorValue.B);
				}

				// Checking if all modifiable vectors of the material instance have been modified
				if (Material->VectorParameterValues.Num() > ModifiedVectorParameters)
				{
					int32 ParametersDiff = Material->VectorParameterValues.Num() - ModifiedVectorParameters;

					FString msg =
						FString::Printf(TEXT("Material Instance [%s] from column [%s] row [%s] has %d modifiable Vectors that will not be modified, they are non-modifiable parameters in the Default Material Instance"),
							*Material->GetName(), *ColumnName, *RowName, ParametersDiff);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
				}

				// Number of modified Float parameters in the Material Instance
				int32 ModifiedFloatParameters = 0;

				ParameterInfos.Empty();
				ParameterGuids.Empty();

				ReferenceMaterial->GetAllParameterInfoOfType(EMaterialParameterType::Scalar, ParameterInfos, ParameterGuids);

				for (FScalarParameterValue ReferenceScalar : ReferenceMaterial->ScalarParameterValues)
				{
					int32 ColumnIndex;

					FString ParameterGuid;

					for (int32 i = 0; i < ParameterInfos.Num(); ++i)
					{
						if (ParameterInfos[i].Name == ReferenceScalar.ParameterInfo.Name)
						{
							ParameterGuid = ParameterGuids[i].ToString();
							Parameters.Add(ParameterGuids[i]);
							
							break;
						}
					}

					// Getting column index from parameter name
					ColumnIndex = MutableTable->FindColumn(TCHAR_TO_ANSI(*ParameterGuid));

					if (ColumnIndex == -1)
					{
						// If there is no column with the parameters name, we generate a new one
						ColumnIndex = MutableTable->AddColumn(TCHAR_TO_ANSI(*ParameterGuid), mu::TABLE_COLUMN_TYPE::TCT_SCALAR);
					}

					// Getting the parameter value from the parent material
					float ScalarValue;
					Material->GetMaterial()->GetScalarParameterValue(ReferenceScalar.ParameterInfo.Name, ScalarValue);
					
					// Getting the parameter value from the instance if it has been modified
					for (FScalarParameterValue InstanceScalar : Material->ScalarParameterValues)
					{
						if (InstanceScalar.ParameterInfo.Name == ReferenceScalar.ParameterInfo.Name)
						{
							ScalarValue = InstanceScalar.ParameterValue;
							ModifiedFloatParameters++;
							
							break;
						}
					}

					// Setting cell value
					MutableTable->SetCell(ColumnIndex, RowIdx, ScalarValue);
				}

				// Checking if all modifiable floats of the material instance have been modified
				if (Material->ScalarParameterValues.Num() > ModifiedFloatParameters)
				{
					int32 ParametersDiff = Material->ScalarParameterValues.Num() - ModifiedFloatParameters;

					FString msg =
						FString::Printf(TEXT("Material Instance [%s] from column [%s] row [%s] has %d modifiable Scalars that will not be modified, they are non-modifiable parameters in the Default Material Instance"),
							*Material->GetName(), *ColumnName, *RowName, ParametersDiff);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
				}
			}
			else
			{
				FString msg = FString::Printf(TEXT("Material from column [%s] row [%s] is a diferent instance than the Reference Material of the table."), *ColumnName, *RowName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), CustomNodeTable);
			}
		}
	}

	else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
		{
			CurrentColumn = MutableTable->FindColumn(TCHAR_TO_ANSI(*ColumnName));

			if (CurrentColumn == -1)
			{
				CurrentColumn = MutableTable->AddColumn(TCHAR_TO_ANSI(*ColumnName), mu::TABLE_COLUMN_TYPE::TCT_COLOUR);
			}

			// Setting cell value
			FLinearColor Value = *(FLinearColor*)CellData;
			MutableTable->SetCell(CurrentColumn, RowIdx, Value.R, Value.G, Value.B);
		}
	}

	else if (const FNumericProperty* NumProperty = CastField<FFloatProperty>(Property))
	{
		CurrentColumn = MutableTable->FindColumn(TCHAR_TO_ANSI(*ColumnName));

		if (CurrentColumn == -1)
		{
			CurrentColumn = MutableTable->AddColumn(TCHAR_TO_ANSI(*ColumnName), mu::TABLE_COLUMN_TYPE::TCT_SCALAR);
		}

		// Setting cell value
		float Value = NumProperty->GetFloatingPointPropertyValue(CellData);
		MutableTable->SetCell(CurrentColumn, RowIdx, Value);
	}
}


mu::TablePtr GenerateMutableSourceTable(const FString& TableName, const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{
	SCOPED_PIN_DATA(GenerationContext, Pin)
	
	if (mu::TablePtr* Result = GenerationContext.GeneratedTables.Find(TableName))
	{
		return *Result;
	}

	mu::TablePtr MutableTable = new mu::Table();
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());
	if (Node->IsNodeOutDatedAndNeedsRefresh())
	{
		Node->SetRefreshNodeWarning();
	}

	if (const UCustomizableObjectNodeTable* TypedTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		UDataTable* Table = TypedTable->Table;

		if (!Table)
		{
			FString msg = "Couldn't find the Data Table asset in the Node.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);

			return MutableTable;
		}

		const UScriptStruct* TableStruct = Table->GetRowStruct();

		if (TableStruct)
		{
			// Getting names of the column headers
			TArray<FString> ColumnHeaders = Table->GetColumnTitles();

			// Getting names of the rows to access the information
			TArray<FName> RowNames = TypedTable->GetRowNames();

			// index to iterate the column headers
			int32 ColumnHeaderIndex = 1;

			// Adding and filling Name Column
			MutableTable->AddColumn("Name", mu::TABLE_COLUMN_TYPE::TCT_STRING);

			// Add metadata
			FParameterUIData ParameterUIData(
				TypedTable->ParameterName,
				TypedTable->ParamUIMetadata,
				EMutableParameterType::Int);
			
			ParameterUIData.IntegerParameterGroupType = TypedTable->bAddNoneOption ? ECustomizableObjectGroupType::COGT_ONE_OR_NONE : ECustomizableObjectGroupType::COGT_ONE;

			// Adding a None Option
			MutableTable->SetNoneOption(TypedTable->bAddNoneOption);

			for (int32 i = 0; i < RowNames.Num(); ++i)
			{
				MutableTable->AddRow(i);
				FString RowName= RowNames[i].ToString();
				MutableTable->SetCell(0, i, TCHAR_TO_ANSI(*RowName));
				ParameterUIData.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(
					RowName,
					FMutableParamUIMetadata()));
			}

			for (TFieldIterator<FProperty> It(TableStruct); It; ++It)
			{
				FProperty* Property = *It;

				for (int32 RowIndex = 0; RowIndex < RowNames.Num(); ++RowIndex)
				{
					// Getting Row Data
					uint8* RowData = Table->FindRowUnchecked(RowNames[RowIndex]);

					if (RowData)
					{
						// Getting Cell Data
						uint8* CellData = Property->ContainerPtrToValuePtr<uint8>(RowData, 0);

						if (CellData)
						{
							FillTableColumn(Pin, MutableTable, ColumnHeaders[ColumnHeaderIndex], RowNames[RowIndex].ToString(), RowIndex, CellData, Property, GenerationContext);
						}
					}
				}

				ColumnHeaderIndex++;
			}

			GenerationContext.ParameterUIDataMap.Add(TypedTable->ParameterName, ParameterUIData);
		}
	}
	
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.GeneratedTables.Add(TableName, MutableTable);

	return MutableTable;
}

#undef LOCTEXT_NAMESPACE

