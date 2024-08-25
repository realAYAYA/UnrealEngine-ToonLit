// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"

#include "Animation/AnimInstance.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2DArray.h"
#include "GameplayTagContainer.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuR/Mesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "MuCOE/CustomizableObjectVersionBridge.h"
#include "ClothingAsset.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


bool FillTableColumn(const UCustomizableObjectNodeTable* TableNode,	mu::TablePtr MutableTable,	const FString& ColumnName,	const FString& RowName,	const int32 RowIdx,	uint8* CellData, const FProperty* ColumnProperty,
	const int LODIndexConnected, const int32 SectionIndexConnected, int32 LODIndex, int32 SectionIndex, const bool bOnlyConnectedLOD, FMutableGraphGenerationContext& GenerationContext)
{
	int32 CurrentColumn;
	UDataTable* DataTablePtr = GetDataTable(TableNode, GenerationContext);

	// Getting property type
	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
	{
		UObject* Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();

		if (SoftObjectProperty->PropertyClass->IsChildOf(USkeletalMesh::StaticClass()))
		{
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object);

			if(!SkeletalMesh)
			{
				// Generating an Empty cell
				FString MutableColumnName = TableNode->GenerateSkeletalMeshMutableColumName(ColumnName, LODIndex, SectionIndex);

				CurrentColumn = MutableTable.get()->FindColumn(MutableColumnName);

				if (CurrentColumn == -1)
				{
					CurrentColumn = MutableTable->AddColumn(MutableColumnName, mu::ETableColumnType::Mesh);
				}

				mu::MeshPtr EmptySkeletalMesh = nullptr;
				MutableTable->SetCell(CurrentColumn, RowIdx, EmptySkeletalMesh.get());

				return true;
			}

			// Getting Animation Blueprint and Animation Slot
			FString AnimBP, AnimSlot, GameplayTag, AnimBPAssetTag;
			TArray<FGameplayTag> GameplayTags;
			FGuid ColumnPropertyId = FStructureEditorUtils::GetGuidForProperty(ColumnProperty);

			TableNode->GetAnimationColumns(ColumnPropertyId, AnimBP, AnimSlot, GameplayTag);

			if (!AnimBP.IsEmpty())
			{
				if (!AnimSlot.IsEmpty())
				{
					if (DataTablePtr)
					{
						uint8* AnimRowData = DataTablePtr->FindRowUnchecked(FName(*RowName));

						if (AnimRowData)
						{
							FName SlotIndex;

							// Getting animation slot row value from data table
							if (FProperty* AnimSlotProperty = DataTablePtr->FindTableProperty(FName(*AnimSlot)))
							{
								uint8* AnimSlotData = AnimSlotProperty->ContainerPtrToValuePtr<uint8>(AnimRowData, 0);

								if (AnimSlotData)
								{
									if (const FIntProperty* IntProperty = CastField<FIntProperty>(AnimSlotProperty))
									{
										FString Message = FString::Printf(
											TEXT("The column with name [%s] for the Anim Slot property should be an FName instead of an Integer, it will be internally converted to FName but should probaly be converted in the table itself."), 
											*AnimBP);
										GenerationContext.Compiler->CompilerLog(FText::FromString(Message), TableNode, EMessageSeverity::Info);

										SlotIndex = FName(FString::FromInt(IntProperty->GetPropertyValue(AnimSlotData)));
									}
									else if (const FNameProperty* NameProperty = CastField<FNameProperty>(AnimSlotProperty))
									{
										SlotIndex = NameProperty->GetPropertyValue(AnimSlotData);
									}
								}
							}

							if (SlotIndex.GetStringLength() != 0)
							{
								// Getting animation instance soft class from data table
								if (FProperty* AnimBPProperty = DataTablePtr->FindTableProperty(FName(*AnimBP)))
								{
									uint8* AnimBPData = AnimBPProperty->ContainerPtrToValuePtr<uint8>(AnimRowData, 0);

									if (AnimBPData)
									{
										if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(AnimBPProperty))
										{
											TSoftClassPtr<UAnimInstance> AnimInstance(SoftClassProperty->GetPropertyValue(AnimBPData).ToSoftObjectPath());

											if (!AnimInstance.IsNull())
											{
												if (UClass* Anim = AnimInstance.LoadSynchronous())
												{
													GenerationContext.AddParticipatingObject(*Anim);													
												}

												const int32 AnimInstanceIndex = GenerationContext.AnimBPAssets.AddUnique(AnimInstance);

												AnimBPAssetTag = GenerateAnimationInstanceTag(AnimInstanceIndex, SlotIndex);
											}
										}
									}
								}
							}
							else
							{
								FString msg = FString::Printf(TEXT("Could not find the Slot column of the animation blueprint column [%s] for the mesh column [%s] row [%s]."), *AnimBP, *ColumnName, *RowName);
								LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
							}
						}
					}
				}
				else
				{
					FString msg = FString::Printf(TEXT("Could not found the Slot column of the animation blueprint column [%s] for the mesh column [%s]."), *AnimBP, *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
				}
			}

			// Getting Gameplay tags
			if (!GameplayTag.IsEmpty())
			{
				if (DataTablePtr)
				{
					uint8* GameplayRowData = DataTablePtr->FindRowUnchecked(FName(*RowName));

					if (GameplayRowData)
					{
						// Getting animation tag row value from data table
						if (FProperty* GameplayTagProperty = DataTablePtr->FindTableProperty(FName(*GameplayTag)))
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
			USkeletalMesh* ReferenceSkeletalMesh = TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(ColumnName);

			if (!ReferenceSkeletalMesh)
			{
				FString msg = FString::Printf(TEXT("Reference Skeletal Mesh not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

				return false;
			}

			GetLODAndSectionForAutomaticLODs(GenerationContext, *TableNode, *SkeletalMesh, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD);
			
			// Parameter used for LOD differences
	
			if (GenerationContext.CurrentAutoLODStrategy != ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh || 
				SectionIndex == SectionIndexConnected)
			{
				const int32 NumLODs = SkeletalMesh->GetImportedModel()->LODModels.Num();

				if (NumLODs <= LODIndex)
				{
					LODIndex = NumLODs - 1;

					FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] needs LOD %d but has less LODs than the reference mesh. LOD %d will be used instead. This can cause some performance penalties."),
						*ColumnName, *RowName, LODIndex, LODIndex);

					LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
				}
			}

			FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
			
			if (ImportedModel->LODModels.IsValidIndex(LODIndex)) // Ignore error since this Section is empty due to Automatic LODs From Mesh
			{
				int32 NumSections = ImportedModel->LODModels[LODIndex].Sections.Num();
				int32 ReferenceNumMaterials = ImportedModel->LODModels[LODIndex].Sections.Num();

				if (NumSections != ReferenceNumMaterials)
				{
					FString Dif_1 = NumSections > ReferenceNumMaterials ? "more" : "less";
					FString Dif_2 = NumSections > ReferenceNumMaterials ? "Some will be ignored" : "This can cause some compilation errors.";

					FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has %s Sections than the reference mesh. %s"), *ColumnName, *RowName, *Dif_1, *Dif_2);
					LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
				}
			}

			FString MutableColumnName = TableNode->GenerateSkeletalMeshMutableColumName(ColumnName, LODIndex, SectionIndex);

			CurrentColumn = MutableTable.get()->FindColumn(MutableColumnName);

			if (CurrentColumn == -1)
			{
				CurrentColumn = MutableTable->AddColumn(MutableColumnName, mu::ETableColumnType::Mesh);
			}

			// First process the mesh tags that are going to make the mesh unique and affect whether it's repeated in 
			// the mesh cache or not
			FString MeshUniqueTags;

			if (!AnimBPAssetTag.IsEmpty())
			{
				MeshUniqueTags += AnimBPAssetTag;
			}

			TArray<FString> ArrayAnimBPTags;

			for (const FGameplayTag& Tag : GameplayTags)
			{
				MeshUniqueTags += GenerateGameplayTag(Tag.ToString());
			}
			
			TArray<int32> StreamedResources;

			if (GenerationContext.Object->bEnableAssetUserDataMerge)
			{
				const TArray<UAssetUserData*>* AssetUserDataArray = SkeletalMesh->GetAssetUserDataArray();

				if (AssetUserDataArray)
				{
					for (UAssetUserData* AssetUserData : *AssetUserDataArray)
					{
						if (!AssetUserData)
						{
							continue;
						}

						const int32 ResourceIndex = GenerationContext.AddAssetUserDataToStreamedResources(AssetUserData);
						
						if (ResourceIndex >= 0)
						{
							check(ResourceIndex < (1 << 24) - 1);

							FCustomizableObjectStreameableResourceId ResourceId;
							ResourceId.Id = GenerationContext.AddAssetUserDataToStreamedResources(AssetUserData);
							ResourceId.Type = (uint8)FCustomizableObjectStreameableResourceId::EType::AssetUserData;

							StreamedResources.Add(BitCast<uint32>(ResourceId));
						}

						MeshUniqueTags += AssetUserData->GetPathName();
					}
				}
			}

			//TODO: Add AnimBp physics to Tables.
			mu::Ptr<mu::Mesh> MutableMesh = GenerateMutableMesh(SkeletalMesh, TSoftClassPtr<UAnimInstance>(), LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, MeshUniqueTags, GenerationContext, TableNode);

			if (MutableMesh)
			{
 				if (SkeletalMesh->GetPhysicsAsset() && MutableMesh->GetPhysicsBody() && MutableMesh->GetPhysicsBody()->GetBodyCount())
				{
 					TSoftObjectPtr<UPhysicsAsset> PhysicsAsset = SkeletalMesh->GetPhysicsAsset();

					GenerationContext.AddParticipatingObject(*PhysicsAsset); 						

					const int32 AssetIndex = GenerationContext.PhysicsAssets.AddUnique(PhysicsAsset);
					FString PhysicsAssetTag = FString("__PA:") + FString::FromInt(AssetIndex);

					AddTagToMutableMeshUnique(*MutableMesh, PhysicsAssetTag);
				}

				if (GenerationContext.Options.bClothingEnabled)
				{
					UClothingAssetBase* ClothingAssetBase = SkeletalMesh->GetSectionClothingAsset(LODIndex, SectionIndex);	
					UClothingAssetCommon* ClothingAssetCommon = Cast<UClothingAssetCommon>(ClothingAssetBase);

					if (ClothingAssetCommon && ClothingAssetCommon->PhysicsAsset)
					{	
						int32 AssetIndex = GenerationContext.ContributingClothingAssetsData.IndexOfByPredicate( 
						[Guid = ClothingAssetBase->GetAssetGuid()](const FCustomizableObjectClothingAssetData& A)
						{
							return A.OriginalAssetGuid == Guid;
						});

						check(AssetIndex != INDEX_NONE);

						GenerationContext.AddParticipatingObject(*ClothingAssetCommon->PhysicsAsset);
						
						const int32 PhysicsAssetIndex = GenerationContext.PhysicsAssets.AddUnique(ClothingAssetCommon->PhysicsAsset);
						FString ClothPhysicsAssetTag = FString::Printf(TEXT("__ClothPA:%d_%d"), AssetIndex, PhysicsAssetIndex);
						AddTagToMutableMeshUnique(*MutableMesh, ClothPhysicsAssetTag);
					}
				}

				if (!AnimBPAssetTag.IsEmpty())
				{
					AddTagToMutableMeshUnique(*MutableMesh, AnimBPAssetTag);
				}

				for (const FGameplayTag& Tag : GameplayTags)
				{
					AddTagToMutableMeshUnique(*MutableMesh, GenerateGameplayTag(Tag.ToString()));
				}

				for (int32 ResourceIndex : StreamedResources)
				{
					MutableMesh->AddStreamedResource(ResourceIndex);
				}

				AddSocketTagsToMesh(SkeletalMesh, MutableMesh, GenerationContext);

				if (UCustomizableObjectSystem::GetInstance()->IsMutableAnimInfoDebuggingEnabled())
				{
					FString MeshPath;
					SkeletalMesh->GetOuter()->GetPathName(nullptr, MeshPath);
					FString MeshTag = FString("__MeshPath:") + MeshPath;
					AddTagToMutableMeshUnique(*MutableMesh, MeshTag);
				}

				MutableTable->SetCell(CurrentColumn, RowIdx, MutableMesh.get(), SkeletalMesh);
			}
			else
			{
				FString msg = FString::Printf(TEXT("Error converting skeletal mesh LOD %d, Section %d from column [%s] row [%s] to mutable."),
					LODIndex, SectionIndex, *ColumnName, *RowName);
				LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
			}
		}

		else if (SoftObjectProperty->PropertyClass->IsChildOf(UStaticMesh::StaticClass()))
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object);

			if (!StaticMesh)
			{
				return false;
			}

			// Getting reference Mesh
			UStaticMesh* ReferenceStaticMesh = TableNode->GetColumnDefaultAssetByType<UStaticMesh>(ColumnName);

			if (!ReferenceStaticMesh)
			{
				FString msg = FString::Printf(TEXT("Reference Static Mesh not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

				return false;
			}

			// Parameter used for LOD differences
			int32 CurrentLOD = LODIndex;

			int NumLODs = StaticMesh->GetRenderData()->LODResources.Num();

			if (NumLODs <= CurrentLOD)
			{
				CurrentLOD = NumLODs - 1;

				FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] needs LOD %d but has less LODs than the reference mesh. LOD %d will be used instead. This can cause some performance penalties."),
					*ColumnName, *RowName, LODIndex, CurrentLOD);
				LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
			}

			int32 NumMaterials = StaticMesh->GetRenderData()->LODResources[CurrentLOD].Sections.Num();
			int32 ReferenceNumMaterials = ReferenceStaticMesh->GetRenderData()->LODResources[CurrentLOD].Sections.Num();

			if (NumMaterials != ReferenceNumMaterials)
			{
				FString FirstTextOption = NumMaterials > ReferenceNumMaterials ? "more" : "less";
				FString SecondTextOption = NumMaterials > ReferenceNumMaterials ? "Some will be ignored" : "This can cause some compilation errors.";

				FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has %s Sections than the reference mesh. %s"), *ColumnName, *RowName, *FirstTextOption, *SecondTextOption);
				LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
			}

			FString MutableColumnName = TableNode->GenerateStaticMeshMutableColumName(ColumnName, SectionIndex);

			CurrentColumn = MutableTable.get()->FindColumn(MutableColumnName);

			if (CurrentColumn == -1)
			{
				CurrentColumn = MutableTable->AddColumn(MutableColumnName, mu::ETableColumnType::Mesh);
			}

			mu::MeshPtr MutableMesh = GenerateMutableMesh(StaticMesh, TSoftClassPtr<UAnimInstance>(), CurrentLOD, SectionIndex, CurrentLOD, SectionIndex, FString(), GenerationContext, TableNode);

			if (MutableMesh)
			{
				MutableTable->SetCell(CurrentColumn, RowIdx, MutableMesh.get(), StaticMesh);
			}
			else
			{
				FString msg = FString::Printf(TEXT("Error converting skeletal mesh LOD %d, Section %d from column [%s] row [%s] to mutable."),
					LODIndex, SectionIndex, *ColumnName, *RowName);
				LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
			}
		}

		else if (SoftObjectProperty->PropertyClass->IsChildOf(UTexture::StaticClass()))
		{
			UTexture* Texture = Cast<UTexture>(Object);

			// Removing encoding part
			const FString PinName = ColumnName.Replace(TEXT("--PassThrough"), TEXT(""), ESearchCase::CaseSensitive);

			if (!Texture)
			{
				Texture = TableNode->GetColumnDefaultAssetByType<UTexture>(PinName);

				FString Message = Cast<UObject>(Object) ? "not a suported Texture" : "null";
				FString WarningMessage = FString::Printf(TEXT("Texture from column [%s] row [%s] is %s. The default texture will be used instead."), *PinName, *RowName, *Message);
				LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, WarningMessage, RowName);
			}

			// There will be always one of the two options
			check(Texture);

			// Getting column index from column name
			CurrentColumn = MutableTable->FindColumn(ColumnName);

			if (CurrentColumn == INDEX_NONE)
			{
				CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Image);
			}

			bool bIsPassthroughTexture = TableNode->GetColumnImageMode(PinName) == ETableTextureType::PASSTHROUGH_TEXTURE;
			if (!bIsPassthroughTexture)
			{
				if (Texture)
				{
					GenerationContext.AddParticipatingObject(*Texture);
				}
			}

			mu::Ptr<mu::ResourceProxyMemory<mu::Image>> Proxy = new mu::ResourceProxyMemory<mu::Image>(GenerateImageConstant(Texture, GenerationContext, bIsPassthroughTexture));
			MutableTable->SetCell(CurrentColumn, RowIdx, Proxy.get());
		}

		else if (SoftObjectProperty->PropertyClass->IsChildOf(UMaterialInstance::StaticClass()))
		{
			// Getting the name of material column of the data table
			FString MaterialColumnName = ColumnProperty->GetDisplayNameText().ToString();

			UMaterialInstance* Material = Cast<UMaterialInstance>(Object);
			UMaterialInstance* ReferenceMaterial = TableNode->GetColumnDefaultAssetByType<UMaterialInstance>(MaterialColumnName);

			if (!ReferenceMaterial)
			{
				FString msg = FString::Printf(TEXT("Reference Material not found for column [%s]."), *MaterialColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

				return false;
			}

			GenerationContext.AddParticipatingObject(*ReferenceMaterial);

			const bool bTableMaterialCheckDisabled = GenerationContext.Object->bDisableTableMaterialsParentCheck;
			const bool bMaterialParentMismatch = !bTableMaterialCheckDisabled && Material 
												 && ReferenceMaterial->GetMaterial() != Material->GetMaterial();

			if (!Material || bMaterialParentMismatch)
			{
				FText Warning;

				if (!Material)
				{
					Warning = FText::Format(LOCTEXT("NullMaterialInstance", "Material Instance from column [{0}] row [{1}] is null. The default Material Instance will be used instead."),
						FText::FromString(MaterialColumnName), FText::FromString(RowName));
				}
				else
				{
					Warning = FText::Format(LOCTEXT("MatInstanceFromDifferentParent","Material Instance from column [{0}] row [{1}] has a different Material Parent than the Default Material Instance. The Default Material Instance will be used instead."),
						FText::FromString(MaterialColumnName), FText::FromString(RowName));
				}

				Material = ReferenceMaterial;

				LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, Warning.ToString(), RowName);
			}

			GenerationContext.AddParticipatingObject(*Material);
			
			FString EncodedSwitchParameterName = "__MutableMaterialId";
			if (ColumnName.Contains(EncodedSwitchParameterName))
			{
				CurrentColumn = MutableTable.get()->FindColumn(ColumnName);

				if (CurrentColumn == -1)
				{
					CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Scalar);
				}

				const int32 lastMaterialAmount = GenerationContext.ReferencedMaterials.Num();
				int32 ReferenceMaterialId = GenerationContext.ReferencedMaterials.AddUnique(Material);

				// Take slot name from skeletal mesh if one can be found, else leave empty.
				// Keep Referenced Materials and Materail Slot Names synchronized even if no material name can be found.
				const bool IsNewSlotName = GenerationContext.ReferencedMaterialSlotNames.Num() == ReferenceMaterialId;

				if (IsNewSlotName)
				{
					GenerationContext.ReferencedMaterialSlotNames.Add(FName(NAME_None));
				}

				MutableTable->SetCell(CurrentColumn, RowIdx, (float)ReferenceMaterialId);

				return true;
			}

			int32 ColumnIndex;

			// Getting parameter value
			TArray<FMaterialParameterInfo> ParameterInfos;
			TArray<FGuid> ParameterGuids;

			Material->GetMaterial()->GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfos, ParameterGuids);
			
			FGuid ParameterId(GenerationContext.CurrentMaterialTableParameterId);
			int32 ParameterIndex = ParameterGuids.Find(ParameterId);

			if (ParameterIndex != INDEX_NONE && ParameterInfos[ParameterIndex].Name == GenerationContext.CurrentMaterialTableParameter)
			{
				// Getting column index from parameter name
				ColumnIndex = MutableTable->FindColumn(ColumnName);

				if (ColumnIndex == INDEX_NONE)
				{
					// If there is no column with the parameters name, we generate a new one
					ColumnIndex = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Image);
				}

				UTexture* ParentTextureValue = nullptr;
				Material->GetMaterial()->GetTextureParameterValue(ParameterInfos[ParameterIndex], ParentTextureValue);
				
				UTexture2D* ParentParameterTexture = Cast<UTexture2D>(ParentTextureValue);
				if (!ParentParameterTexture)
				{
					FString ParamName = ParameterInfos[ParameterIndex].Name.ToString();
					FString Message = Cast<UObject>(ParentParameterTexture) ? "not a Texture2D" : "null";
					
					FString msg = FString::Printf(TEXT("Parameter [%s] from Default Material Instance of column [%s] is %s. This parameter will be ignored."), *ParamName, *MaterialColumnName, *Message);
					LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
					 
					 return false;
				}

				UTexture* TextureValue = nullptr;
				Material->GetTextureParameterValue(ParameterInfos[ParameterIndex], TextureValue);

				UTexture2D* ParameterTexture = Cast<UTexture2D>(TextureValue);

				if (!ParameterTexture)
				{
					ParameterTexture = ParentParameterTexture;

					FString ParamName = GenerationContext.CurrentMaterialTableParameter;
					FString Message = Cast<UObject>(TextureValue) ? "not a Texture2D" : "null";

					FString msg = FString::Printf(TEXT("Parameter [%s] from material instance of column [%s] row [%s] is %s. The parameter texture of the default material will be used instead."), *ParamName, *MaterialColumnName, *RowName, *Message);
					LogRowGenerationMessage(TableNode, DataTablePtr, GenerationContext, msg, RowName);
				}

				bool bIsPassthroughTexture = false;
				mu::Ptr<mu::ResourceProxyMemory<mu::Image>> Proxy = new mu::ResourceProxyMemory<mu::Image>(GenerateImageConstant(ParameterTexture, GenerationContext, bIsPassthroughTexture));
				MutableTable->SetCell(ColumnIndex, RowIdx, Proxy.get());

				return true;
			}
		}

		else
		{
			// Unsuported Variable Type
			return false;
		}
	}

	else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
	{
		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
		{
			CurrentColumn = MutableTable->FindColumn(ColumnName);

			if (CurrentColumn == INDEX_NONE)
			{
				CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Color);
			}

			// Setting cell value
			FLinearColor Value = *(FLinearColor*)CellData;
			MutableTable->SetCell(CurrentColumn, RowIdx, Value);
		}
		
		else
		{
			// Unsuported Variable Type
			return false;
		}
	}

	else if (const FNumericProperty* FloatNumProperty = CastField<FFloatProperty>(ColumnProperty))
	{
		CurrentColumn = MutableTable->FindColumn(ColumnName);

		if (CurrentColumn == INDEX_NONE)
		{
			CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Scalar);
		}

		// Setting cell value
		float Value = FloatNumProperty->GetFloatingPointPropertyValue(CellData);
		MutableTable->SetCell(CurrentColumn, RowIdx, Value);
	}

	else if (const FNumericProperty* DoubleNumProperty = CastField<FDoubleProperty>(ColumnProperty))
	{
		CurrentColumn = MutableTable->FindColumn(ColumnName);
	
		if (CurrentColumn == INDEX_NONE)
		{
			CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Scalar);
		}
	
		// Setting cell value
		float Value = DoubleNumProperty->GetFloatingPointPropertyValue(CellData);
		MutableTable->SetCell(CurrentColumn, RowIdx, Value);
	}

	else
	{
		// Unsuported Variable Type
		return false;
	}

	return true;
}


uint8* GetCellData(const FName& RowName, const UDataTable& DataTable, const FProperty& ColumnProperty)
{
	// Get Row Data
	uint8* RowData = DataTable.FindRowUnchecked(RowName);

	if (RowData)
	{
		return ColumnProperty.ContainerPtrToValuePtr<uint8>(RowData, 0);
	}

	return nullptr;
}


FName GetAnotherOption(FName SelectedOptionName, const TArray<FName>& RowNames)
{
	for (const FName& CandidateOption : RowNames)
	{
		if (CandidateOption != SelectedOptionName)
		{
			return CandidateOption;
		}
	}

	return FName("None");
}


TArray<FName> GetEnabledRows(const UDataTable& DataTable, const UCustomizableObjectNodeTable& TableNode)
{
	TArray<FName> RowNames;
	const UScriptStruct* TableStruct = DataTable.GetRowStruct();

	if (!TableStruct)
	{
		return RowNames;
	}

	TArray<FName> TableRowNames = DataTable.GetRowNames();
	FBoolProperty* BoolProperty = nullptr;

	for (TFieldIterator<FProperty> PropertyIt(TableStruct); PropertyIt && TableNode.bDisableCheckedRows; ++PropertyIt)
	{
		BoolProperty = CastField<FBoolProperty>(*PropertyIt);

		if (BoolProperty)
		{
			for (const FName& RowName : TableRowNames)
			{
				if (uint8* CellData = GetCellData(RowName, DataTable, *BoolProperty))
				{
					if (!BoolProperty->GetPropertyValue(CellData))
					{
						RowNames.Add(RowName);
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

	return RowNames;
}


void RestrictRowNamesToSelectedOption(TArray<FName>& InOutRowNames, const UCustomizableObjectNodeTable& TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	// If the param is in the map restrict to only the selected option
	FString* SelectedOptionString = GenerationContext.ParamNamesToSelectedOptions.Find(TableNode.ParameterName);

	if (SelectedOptionString)
	{
		FName SelectedOptionName = FName(*SelectedOptionString);

		if (InOutRowNames.Contains(SelectedOptionName))
		{
			FName AnotherOption = GetAnotherOption(SelectedOptionName, InOutRowNames);
			InOutRowNames.Empty(2);
			InOutRowNames.Add(SelectedOptionName);

			// To prevent the optimization of the parameter for having just one option, which would prevent the restriction 
			// of that parameter in the next compile only selected
			InOutRowNames.Add(AnotherOption);
		}
		else
		{
			InOutRowNames.Empty(0);
		}
	}
}


void RestrictRowContentByVersion( TArray<FName>& InOutRowNames, const UDataTable& DataTable, const UCustomizableObjectNodeTable& TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	FProperty* ColumnProperty = DataTable.FindTableProperty(TableNode.VersionColumn);

	if (!ColumnProperty)
	{
		return;
	}

	ICustomizableObjectVersionBridgeInterface* CustomizableObjectVersionBridgeInterface = Cast<ICustomizableObjectVersionBridgeInterface>(GenerationContext.Object->VersionBridge);
	if (!CustomizableObjectVersionBridgeInterface)
	{
		const FString Message = "Found a data table with at least a row with a Custom Version asset but the Root Object does not have a Version Bridge asset assigned.";
		GenerationContext.Compiler->CompilerLog(FText::FromString(Message), &TableNode, EMessageSeverity::Error);
		return;
	}

	TArray<FName> OutRowNames;
	OutRowNames.Reserve(InOutRowNames.Num());

	for (int32 RowIndex = 0; RowIndex < InOutRowNames.Num(); ++RowIndex)
	{
		if (uint8* CellData = GetCellData(InOutRowNames[RowIndex], DataTable, *ColumnProperty))
		{
			if (!CustomizableObjectVersionBridgeInterface->IsVersionPropertyIncludedInCurrentRelease(*ColumnProperty, CellData))
			{
				continue;
			}

			OutRowNames.Add(InOutRowNames[RowIndex]);
		}
	}

	InOutRowNames = OutRowNames;
}


TArray<FName> GetRowsToCompile(const UDataTable& DataTable, const UCustomizableObjectNodeTable& TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	TArray<FName> RowNames = GetEnabledRows(DataTable, TableNode);

	if (!RowNames.IsEmpty())
	{
		RestrictRowNamesToSelectedOption(RowNames, TableNode, GenerationContext);
		RestrictRowContentByVersion(RowNames, DataTable, TableNode, GenerationContext);
	}

	return RowNames;
}


bool GenerateTableColumn(const UCustomizableObjectNodeTable* TableNode, const UEdGraphPin* Pin, mu::TablePtr MutableTable, const FString& DataTableColumnName, const FProperty* ColumnProperty,
	const int32 LODIndexConnected, const int32 SectionIndexConnected, const int32 LODIndex, const int32 SectionIndex, const bool bOnlyConnectedLOD, FMutableGraphGenerationContext& GenerationContext)
{
	SCOPED_PIN_DATA(GenerationContext, Pin)

	if (!TableNode)
	{
		return false;
	}

	UDataTable* DataTable = GetDataTable(TableNode, GenerationContext);

	if (!DataTable || !DataTable->GetRowStruct())
	{
		return false;
	}
	
	GenerationContext.AddParticipatingObject(*DataTable);

	// Getting names of the rows to access the information
	TArray<FName> RowNames = GetRowsToCompile(*DataTable, *TableNode, GenerationContext);

	for (int32 RowIndex = 0; RowIndex < RowNames.Num(); ++RowIndex)
	{
		if (uint8* CellData = GetCellData(RowNames[RowIndex], *DataTable, *ColumnProperty))
		{
			bool bCellGenerated = FillTableColumn(TableNode, MutableTable, DataTableColumnName, RowNames[RowIndex].ToString(), RowIndex, CellData, ColumnProperty,
				LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD, GenerationContext);

			if (!bCellGenerated)
			{
				return false;
			}
		}
	}

	return true;
}


void GenerateTableParameterUIData(const UDataTable* DataTable, const UCustomizableObjectNodeTable* TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	// Checking if the parameter name already exists
	GenerationContext.AddParameterNameUnique(TableNode, TableNode->ParameterName);

	// Generating Parameter UI MetaData if not exists
	if (!GenerationContext.ParameterUIDataMap.Contains(TableNode->ParameterName))
	{
		// Getting Table and row names to access the information
		TArray<FName> RowNames = GetRowsToCompile(*DataTable, *TableNode, GenerationContext);

		FParameterUIData ParameterUIData(TableNode->ParameterName, TableNode->ParamUIMetadata, EMutableParameterType::Int);
		ParameterUIData.IntegerParameterGroupType = TableNode->bAddNoneOption ? ECustomizableObjectGroupType::COGT_ONE_OR_NONE : ECustomizableObjectGroupType::COGT_ONE;
		FParameterUIData& ParameterUIDataRef = GenerationContext.ParameterUIDataMap.Add(TableNode->ParameterName, ParameterUIData);

		if (TableNode->ParamUIMetadataColumn.IsNone())
		{
			return;
		}

		FProperty* ColumnProperty = DataTable->FindTableProperty(TableNode->ParamUIMetadataColumn);

		if (!ColumnProperty)
		{
			FString msg = "Couldn't find Options UI Metadata Column [" + TableNode->ParamUIMetadataColumn.ToString() + "] in the Structure of the Node.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

			return;
		}

		FString WrongTypeMessage = "Column with name [" + TableNode->ParamUIMetadataColumn.ToString() + "] is not a Mutable Param UI Metadata type.";

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
		{
			if (StructProperty->Struct != FMutableParamUIMetadata::StaticStruct())
			{
				GenerationContext.Compiler->CompilerLog(FText::FromString(WrongTypeMessage), TableNode);

				return;
			}
			
			for (int32 NameIndex = 0; NameIndex < RowNames.Num(); ++NameIndex)
			{
				if (uint8* CellData = GetCellData(RowNames[NameIndex], *DataTable, *ColumnProperty))
				{
					FMutableParamUIMetadata Value = *(FMutableParamUIMetadata*)CellData;
					ParameterUIDataRef.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(RowNames[NameIndex].ToString(), Value));
				}
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(FText::FromString(WrongTypeMessage), TableNode);
			return;
		}
	}
}


mu::TablePtr GenerateMutableSourceTable(const UDataTable* DataTable, const UCustomizableObjectNodeTable* TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	check(DataTable && TableNode);

	// Checking if the table in the cache
	const FString TableName = DataTable->GetName();

	if (mu::TablePtr* Result = GenerationContext.GeneratedTables.Find(TableName))
	{
		// Generating Parameter Metadata for parameters that reuse a Table
		GenerateTableParameterUIData(DataTable, TableNode, GenerationContext);

		return *Result;
	}

	mu::TablePtr MutableTable = new mu::Table();

	if (const UScriptStruct* TableStruct = DataTable->GetRowStruct())
	{
		// Getting Table and row names to access the information
		TArray<FName> RowNames = GetRowsToCompile(*DataTable, *TableNode, GenerationContext);

		// Adding and filling Name Column
		MutableTable->AddColumn("Name", mu::ETableColumnType::String);

		for (int32 NameIndex = 0; NameIndex < RowNames.Num(); ++NameIndex)
		{
			MutableTable->AddRow(NameIndex);
			MutableTable->SetCell(0, NameIndex, RowNames[NameIndex].ToString());
		}

		// Generating Parameter Metadata for new table parameters
		GenerateTableParameterUIData(DataTable, TableNode, GenerationContext);
	}
	else
	{
		FString msg = "Couldn't find the Data Table's Struct asset in the Node.";
		GenerationContext.Compiler->CompilerLog(FText::FromString(msg), DataTable);
		
		return nullptr;
	}

	GenerationContext.GeneratedTables.Add(TableName, MutableTable);

	return MutableTable;
}


void AddCompositeTablesToParticipatingObjetcts(const UDataTable* Table, FMutableGraphGenerationContext& GenerationContext)
{
	if (const UCompositeDataTable* CompositeTable = Cast<UCompositeDataTable>(Table))
	{
		GenerationContext.AddParticipatingObject(*CompositeTable);

		/*for (const TArray<TObjectPtr<UDataTable>>& ParentTable : CompositeTable.ParentTables) // TODO 
		{
			AddCompositeTablesToParticipatingObjetcts(ParentTable, GenerationContext);
		}*/
	}
}


UDataTable* GetDataTable(const UCustomizableObjectNodeTable* TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	UDataTable* OutDataTable = nullptr;

	if (TableNode->TableDataGatheringMode == ETableDataGatheringSource::ETDGM_AssetRegistry)
	{
		OutDataTable = GenerateDataTableFromStruct(TableNode, GenerationContext);
	}
	else
	{
		OutDataTable = TableNode->Table;
	}

	AddCompositeTablesToParticipatingObjetcts(OutDataTable, GenerationContext);

	return OutDataTable;
}


UDataTable* GenerateDataTableFromStruct(const UCustomizableObjectNodeTable* TableNode, FMutableGraphGenerationContext& GenerationContext)
{
	if (!TableNode->Structure)
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("EmptyStructureError", "Empty structure asset."), TableNode);
		return nullptr;
	}

	FMutableGraphGenerationContext::FGeneratedDataTablesData DataTableData;
	DataTableData.ParentStruct = TableNode->Structure;
	DataTableData.FilterPaths = TableNode->FilterPaths;
	
	//Checking cache of generated data tables
	int32 DataTableIndex = GenerationContext.GeneratedCompositeDataTables.Find(DataTableData);
	if (DataTableIndex != INDEX_NONE)
	{
		// DataTable already generated
		UCompositeDataTable* GeneratedDataTable = GenerationContext.GeneratedCompositeDataTables[DataTableIndex].GeneratedDataTable;
		return Cast<UDataTable>(GeneratedDataTable);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.GetRegistry();

	if (TableNode->FilterPaths.IsEmpty())
	{
		// Preventing load all data tables of the project
		GenerationContext.Compiler->CompilerLog(LOCTEXT("NoFilePathsError", "There are no filter paths selected. This is an error to prevent loading all data table of the project."), TableNode);

		return nullptr;
	}

	TArray<FAssetData> DataTableAssets = TableNode->GetParentTables();

	UCompositeDataTable* CompositeDataTable = NewObject<UCompositeDataTable>();
	CompositeDataTable->RowStruct = TableNode->Structure;

	TArray<UDataTable*> ParentTables;

	for (const FAssetData& DataTableAsset : DataTableAssets)
	{
		if (DataTableAsset.IsValid())
		{
			if (UDataTable* DataTable = Cast<UDataTable>(DataTableAsset.GetAsset()))
			{
				ParentTables.Add(DataTable);
			}
		}
	}

	if (ParentTables.IsEmpty())
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("NoDataTablesFoundWarning", "Could not find a data table with the specified struct in the selected paths."), TableNode);

		return nullptr;
	}

	// Map to find the original data table of a row
	TMap<FName, TArray<UDataTable*>> OriginalTableRowsMap;

	// Set to iterate faster the repeated rows inside the map
	TSet<FName> RepeatedRowNamesArray;

	// Checking if a row name is repeated in several tables
	for (int32 ParentIndx = 0; ParentIndx < ParentTables.Num(); ++ParentIndx)
	{
		const TArray<FName>& RowNames = ParentTables[ParentIndx]->GetRowNames();

		for (const FName& RowName : RowNames)
		{
			TArray<UDataTable*>* DataTablesNames = OriginalTableRowsMap.Find(RowName);

			if (DataTablesNames == nullptr)
			{
				TArray<UDataTable*> ArrayTemp;
				ArrayTemp.Add(ParentTables[ParentIndx]);
				OriginalTableRowsMap.Add(RowName, ArrayTemp);
			}
			else
			{
				DataTablesNames->Add(ParentTables[ParentIndx]);
				RepeatedRowNamesArray.Add(RowName);
			}
		}
	}

	for (const FName& RowName : RepeatedRowNamesArray)
	{
		const TArray<UDataTable*>& DataTablesNames = OriginalTableRowsMap[RowName];

		FString TableNames;

		for (int32 NameIndx = 0; NameIndx < DataTablesNames.Num(); ++NameIndx)
		{
			TableNames += DataTablesNames[NameIndx]->GetName();

			if (NameIndx + 1 < DataTablesNames.Num())
			{
				TableNames += ", ";
			}
		}

		FString Message = FString::Printf(TEXT("Row with name [%s] repeated in the following Data Tables: [%s]. The last row processed will be used [%s]."),
			*RowName.ToString(), *TableNames, *DataTablesNames.Last()->GetName());
		GenerationContext.Compiler->CompilerLog(FText::FromString(Message), TableNode);
	}

	CompositeDataTable->AppendParentTables(ParentTables);

	// Adding Generated Data Table to the cache
	DataTableData.GeneratedDataTable = CompositeDataTable;
	GenerationContext.GeneratedCompositeDataTables.Add(DataTableData);
	GenerationContext.CompositeDataTableRowToOriginalDataTableMap.Add(CompositeDataTable, OriginalTableRowsMap);
	
	return Cast<UDataTable>(CompositeDataTable);
}


void LogRowGenerationMessage(const UCustomizableObjectNodeTable* TableNode, const UDataTable* DataTable, FMutableGraphGenerationContext& GenerationContext, const FString& Message, const FString& RowName)
{
	FString FinalMessage = Message;

	if (TableNode->TableDataGatheringMode == ETableDataGatheringSource::ETDGM_AssetRegistry)
	{
		TMap<FName, TArray<UDataTable*>>* ParameterDataTableMap = GenerationContext.CompositeDataTableRowToOriginalDataTableMap.Find(DataTable);

		if (ParameterDataTableMap)
		{
			TArray<UDataTable*>* DataTables = ParameterDataTableMap->Find(FName(*RowName));

			if (DataTables)
			{
				FString TableNames;

				for (int32 NameIndx = 0; NameIndx < DataTables->Num(); ++NameIndx)
				{
					TableNames += (*DataTables)[NameIndx]->GetName();

					if (NameIndx + 1 < DataTables->Num())
					{
						TableNames += ", ";
					}
				}

				FinalMessage += " Row from Composite Data Table, original Data Table/s: " + TableNames;
			}
		}
	}

	GenerationContext.Compiler->CompilerLog(FText::FromString(FinalMessage), TableNode);
}


#undef LOCTEXT_NAMESPACE
