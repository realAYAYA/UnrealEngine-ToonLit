// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomizableObjectInstanceBakingUtils.h"
#include "MuCOE/CustomizableObjectEditor.h"

#include "Misc/MessageDialog.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "UnrealBakeHelpers.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstanceAssetUserData.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuT/UnrealPixelFormatOverride.h"
#include "Rendering/SkeletalMeshModel.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor" 


/**
 * Simple wrapper to be able to invoke the generation of a popup or log message depending on the execution context in which this code is being ran
 * @param InMessage The message to display
 * @param InTitle The title to be used for the popup or the log generated
 */
void ShowErrorNotification(const FText& InMessage,  const FText* InTitle = nullptr)
{
	if (!FApp::IsUnattended())
	{
		FMessageDialog::Open(EAppMsgType::Ok, InMessage, *InTitle);
	}
	else
	{
		if (InTitle)
		{
			UE_LOG(LogMutable, Error, TEXT("%s - %s"), *InTitle->ToString(), *InMessage.ToString());
		}
		else
		{
			UE_LOG(LogMutable, Error, TEXT("%s"), *InMessage.ToString());
		}
	}
}

/**
 * Utility functions for the baking operation.
 */

/**
 * Validates the filename chosen for the baking data
 * @param FileName The filename chosen by the user
 * @return True if validation was successful, false otherwise
 */
bool ValidateProvidedFileName(const FString& FileName)
{
	if (FileName.IsEmpty())
	{
		UE_LOG(LogMutable, Error, TEXT("Invalid baking configuration : FileName string is empty.."));
		return false;
	}

	// Check for invalid characters in the name of the object to be serialized
	{
		TCHAR InvalidCharacter = '0';
		{
			FString InvalidCharacters = FPaths::GetInvalidFileSystemChars();
			for (int32 i = 0; i < InvalidCharacters.Len(); ++i)
			{
				TCHAR Char = InvalidCharacters[i];
				FString SearchedChar = FString::Chr(Char);
				if (FileName.Contains(SearchedChar))
				{
					InvalidCharacter = InvalidCharacters[i];
					break;
				}
			}
		}

		if (InvalidCharacter != '0')
		{
			const FText InvalidCharacterText = FText::FromString(FString::Chr(InvalidCharacter));
			const FText ErrorText = FText::Format(LOCTEXT("FCustomizableObjectEditorViewportClient_BakeInstance_InvalidCharacter", "The selected contains an invalid character ({0})."), InvalidCharacterText);

			ShowErrorNotification(ErrorText);
		
			return false;
		}
	}

	return true;
}


/**
 * Validates the AssetPath chosen for the baking data
 * @param FileName The filename chosen by the user
 * @param AssetPath The AssetPath chosen by the user
 * @param InstanceCO The CustomizableObject from the provided COI
 * @return True if validation was successful, false otherwise
 */
bool ValidateProvidedAssetPath(const FString& FileName, const FString& AssetPath, const UCustomizableObject* InstanceCO)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogMutable, Error, TEXT("The AssetPath can not be empty!"));
		return false;
	}

	// Ensure we are not overriding the parent CO
	const FString FullAssetPath = AssetPath + FString("/") + FileName + FString(".") + FileName;		// Full asset path to the new asset we want to create
	const bool bWouldOverrideParentCO = InstanceCO->GetPathName() == FullAssetPath;
	if (bWouldOverrideParentCO)
	{
		const FText ErrorText = LOCTEXT("FCustomizableObjectEditorViewportClient_BakeInstance_OverwriteCO", "The selected path would overwrite the instance's parent Customizable Object.");

		ShowErrorNotification(ErrorText);
		
		return false;
	}

	return true;
}


/**
 * Outputs a string that we know it is unique.
 * @param InResource The resource we are working with
 * @param ResourceName The name of the resource we have provided. This should have the name of the current resource and will have the unique name for the resource once the method exits
 * @param InCachedResources Collection with all the already processed resources.
 * @param InCachedResourceNames Collection with all the already processed resources name's
 * @return True if the generation of the unique resource name was successful, false otherwise.
 */
bool GetUniqueResourceName(const UObject* InResource, FString& ResourceName, TArray<UObject*>& InCachedResources, const TArray<FString>& InCachedResourceNames)
{
	check(InResource);

	int32 FindResult = InCachedResourceNames.Find(ResourceName);
	if (FindResult != INDEX_NONE)
	{
		if (InResource == InCachedResources[FindResult])
		{
			return false;
		}

		uint32 Count = 0;
		while (FindResult != INDEX_NONE)
		{
			FindResult = InCachedResourceNames.Find(ResourceName + "_" + FString::FromInt(Count));
			Count++;
		}

		ResourceName += "_" + FString::FromInt(--Count);
	}

	return true;
}


/**
 * Ensures the resource we want to save is ready to be saved. It handles closing it's editor and warning the user about possible overriding of resources.
 * @param InAssetSavePath The directory path where to save the baked object
 * @param InObjName The name of the object to be baked
 * @param bUsedGrantedOverridingRights Control flag that determines if the user has given or not permission to override resources already in disk
 * @return True if the operation was successful, false otherwise
 */
bool ManageBakingAction(const FString& InAssetSavePath, const FString& InObjName, bool& bUsedGrantedOverridingRights)
{
	FString PackagePath = InAssetSavePath + "/" + InObjName;
	UPackage* ExistingPackage = FindPackage(NULL, *PackagePath);

	if (!ExistingPackage)
	{
		FString PackageFilePath = PackagePath + "." + InObjName;

		FString PackageFileName;
		if (FPackageName::DoesPackageExist(PackageFilePath, &PackageFileName))
		{
			ExistingPackage = LoadPackage(nullptr, *PackageFileName, LOAD_EditorOnly);
		}
		else
		{
			// if package does not exists
			bUsedGrantedOverridingRights = false;
			return true;
		}
	}

	if (ExistingPackage)
	{
		// Checking if the asset is open in an editor
		TArray<IAssetEditorInstance*> ObjectEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAssetAndSubObjects(ExistingPackage);
		if (ObjectEditors.Num())
		{
			for (IAssetEditorInstance* ObjectEditorInstance : ObjectEditors)
			{
				// Close the editors that contains this asset
				if (!ObjectEditorInstance->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed))
				{
					const FText Caption = LOCTEXT("OpenExisitngFile", "Open File");
					const FText Message = FText::Format(LOCTEXT("CantCloseAsset", "This Obejct \"{0}\" is open in an editor and can't be closed automatically. Please close the editor and try to bake it again"), FText::FromString(InObjName));

					ShowErrorNotification(Message, &Caption);
					
					return false;
				}
			}
		}

		if (!bUsedGrantedOverridingRights)
		{
			const FText Caption = LOCTEXT("Already existing baked files", "Already existing baked files");
			const FText Message = FText::Format(LOCTEXT("OverwriteBakedInstance", "Instance baked files already exist in selected destination \"{0}\", this action will overwrite them."), FText::AsCultureInvariant(InAssetSavePath));

			// We need to guard this case since it may crash the editor if we are running an unattended commandlet and we try to generate this dialog
			if (!FApp::IsUnattended())
			{
				if (FMessageDialog::Open(EAppMsgType::OkCancel, Message, Caption) == EAppReturnType::Cancel)
				{
					return false;
				}
			}
			else
			{
				UE_LOG(LogMutable, Error, TEXT("%s - %s"), *Caption.ToString(), *Message.ToString());
			}


			bUsedGrantedOverridingRights = true;
		}

		UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), ExistingPackage, *InObjName);
		if (ExistingObject)
		{
			ExistingPackage->FullyLoad();

			TArray<UObject*> ObjectsToDelete;
			ObjectsToDelete.Add(ExistingObject);

			// Delete objects in the package with the same name as the one we want to create
			const uint32 NumObjectsDeleted = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);

			return NumObjectsDeleted == ObjectsToDelete.Num();
		}
	}

	return true;
}


namespace PreBakeSystemSettings
{
	bool bIsProgressiveMipStreamingEnabled = false;
	bool bIsOnlyGenerateRequestedLODsEnabled = false;
}


void PrepareForBaking()
{
	// Implementation of the bake operation
	UCustomizableObjectSystem* System =  UCustomizableObjectSystem::GetInstanceChecked();
	
	// The instance in the editor viewport does not have high quality mips in the platform data because streaming is enabled.
	// Disable streaming and retry with a newly generated temp instance.
	PreBakeSystemSettings::bIsProgressiveMipStreamingEnabled = System->IsProgressiveMipStreamingEnabled();
	System->SetProgressiveMipStreamingEnabled(false);
	// Disable requested LOD generation as it will prevent the new instance from having all the LODs
	PreBakeSystemSettings::bIsOnlyGenerateRequestedLODsEnabled = System->IsOnlyGenerateRequestedLODsEnabled();
	System->SetOnlyGenerateRequestedLODsEnabled(false);
	// Force high quality texture compression for this instance
	PrepareUnrealCompression();
	System->SetImagePixelFormatOverride(UnrealPixelFormatFunc);
}


void RestoreCustomizableObjectSettings(const FUpdateContext& Result)
{
	// Reenable Mutable texture streaming and requested LOD generation as they had been disabled to bake the textures
	UCustomizableObjectSystem* System =  UCustomizableObjectSystem::GetInstanceChecked();
	System->SetProgressiveMipStreamingEnabled(PreBakeSystemSettings::bIsProgressiveMipStreamingEnabled);
	System->SetOnlyGenerateRequestedLODsEnabled(PreBakeSystemSettings::bIsOnlyGenerateRequestedLODsEnabled);
	System->SetImagePixelFormatOverride(nullptr);
}


void UpdateInstanceForBaking(UCustomizableObjectInstance& InInstance, FInstanceUpdateNativeDelegate& InInstanceUpdateDelegate)
{
	// Prepare the customizable object system for baking
	PrepareForBaking();
	
	// Ensure we clear the changes in the COSystem after performing the update so later updates do not get affected
	InInstanceUpdateDelegate.AddStatic(&RestoreCustomizableObjectSettings);
	
	// Schedule the update
	InInstance.UpdateSkeletalMeshAsyncResult(InInstanceUpdateDelegate,true,true);
}


void BakeCustomizableObjectInstance(
	UCustomizableObjectInstance& InInstance,
	const FString& FileName,
	const FString& AssetPath,
	const bool bExportAllResources,
	const bool bGenerateConstantMaterialInstances)
{
	// Ensure that the state of the COI provided is valid --------------------------------------------------------------------------------------------
	UCustomizableObject* InstanceCO = InInstance.GetCustomizableObject();
	check (InstanceCO);

	// Ensure the CO of the COI is accessible 
	if (!InstanceCO || InstanceCO->GetPrivate()->IsLocked())
	{
		FCustomizableObjectEditorLogger::CreateLog(
		LOCTEXT("CustomizableObjectCompilingTryLater_Baking", "Please wait until the Customizable Object is compiled"))
		.Category(ELoggerCategory::COInstanceBaking)
		.CustomNotification()
		.Notification(true)
		.Log();

		return;
	}
	
	if (InstanceCO->GetPrivate()->Status.Get() == FCustomizableObjectStatus::EState::Loading)
	{
		FCustomizableObjectEditorLogger::CreateLog(
			LOCTEXT("CustomizableObjectCompileTryLater_BakeInstance","Please wait unitl Customizable Object is loaded"))
		.Category(ELoggerCategory::COInstanceBaking)
		.CustomNotification()
		.Notification(true)
		.Log();

		return;
	}
	
	if (!ValidateProvidedFileName(FileName))
	{
		UE_LOG(LogMutable, Error, TEXT("The FileName for the instance baking is not valid."));
		return;
	}

	if (!ValidateProvidedAssetPath(FileName,AssetPath,InstanceCO))
	{
		UE_LOG(LogMutable, Error, TEXT("The AssetPath for the instance baking is not valid."));
		return;
	}
	
	// Exit early if the provided instance does not have a skeletal mesh
	if (!InInstance.HasAnySkeletalMesh())
	{
		return;
	}

	// COI Validation completed : Proceed with the baking operation ----------------------------------------------------------------------------------
	
	// Notify of better configuration -> Continue operation normally
	if (InstanceCO->CompileOptions.TextureCompression != ECustomizableObjectTextureCompression::HighQuality)
	{
		FCustomizableObjectEditorLogger::CreateLog(
		LOCTEXT("CustomizableObjectBakeLowQuality", "The Customizable Object wasn't compiled with high quality textures. For the best baking results, change the Texture Compression setting and recompile it."))
		.Category(ELoggerCategory::COInstanceBaking)
		.CustomNotification()
		.Notification(true)
		.Log();
	}
	
	
	// Set the overriding flag to false wo we ask the user at least once about if he is willing to override old baked data
	bool bUsedGrantedOverridingRights = false;

	TArray<UPackage*> PackagesToSave;

	const int32 NumComponents = InInstance.GetNumComponents();
	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		USkeletalMesh* Mesh = InInstance.GetSkeletalMesh(ComponentIndex);

		if (!Mesh)
		{
			continue;
		}

		FString ObjectName = FileName;
		if (NumComponents > 1)
		{
			ObjectName = FileName + "_Component_" + FString::FromInt(ComponentIndex);
		}

		TMap<UObject*, UObject*> ReplacementMap;
		TArray<FString> ArrayCachedElement;
		TArray<UObject*> ArrayCachedObject;

		if (bExportAllResources)
		{
			UMaterialInstance* Inst;
			UMaterial* Material;
			UTexture* Texture;
			FString MaterialName;
			FString ResourceName;
			FString PackageName;
			UObject* DuplicatedObject;
			TArray<TMap<int, UTexture*>> TextureReplacementMaps;

			// Duplicate Mutable generated textures
			for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
				Material = Interface->GetMaterial();
				MaterialName = Material ? Material->GetName() : "Material";
				Inst = Cast<UMaterialInstance>(Mesh->GetMaterials()[m].MaterialInterface);

				TMap<int, UTexture*> ReplacementTextures;
				TextureReplacementMaps.Add(ReplacementTextures);

				// The material will only have Mutable generated textures if it's actually a UMaterialInstance
				if (Material != nullptr && Inst != nullptr)
				{
					TArray<FName> ParameterNames = FUnrealBakeHelpers::GetTextureParameterNames(Material);

					for (int32 i = 0; i < ParameterNames.Num(); i++)
					{
						if (Inst->GetTextureParameterValue(ParameterNames[i], Texture))
						{
							UTexture2D* SrcTex = Cast<UTexture2D>(Texture);
							if (!SrcTex) continue;

							bool bIsMutableTexture = false;

							for (UAssetUserData* UserData : *SrcTex->GetAssetUserDataArray())
							{
								UTextureMipDataProviderFactory* CustomMipDataProviderFactory = Cast<UMutableTextureMipDataProviderFactory>(UserData);
								if (CustomMipDataProviderFactory)
								{
									bIsMutableTexture = true;
								}
							}

							if ((SrcTex->GetPlatformData() != nullptr) &&
								(SrcTex->GetPlatformData()->Mips.Num() > 0) &&
								bIsMutableTexture)
							{
								FString ParameterSanitized = ParameterNames[i].GetPlainNameString();
								RemoveRestrictedChars(ParameterSanitized);
								ResourceName = ObjectName + "_" + MaterialName + "_" + ParameterSanitized;

								if (!GetUniqueResourceName(SrcTex, ResourceName, ArrayCachedObject, ArrayCachedElement))
								{
									continue;
								}

								if (!ManageBakingAction(AssetPath, ResourceName, bUsedGrantedOverridingRights))
								{
									return;
								}

								// Recover original name of the texture parameter value, now substituted by the generated Mutable texture
								UTexture* OriginalTexture = nullptr;
								UMaterialInstanceDynamic* InstDynamic = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterials()[m].MaterialInterface);
								if (InstDynamic != nullptr)
								{
									InstDynamic->Parent->GetTextureParameterValue(FName(*ParameterNames[i].GetPlainNameString()), OriginalTexture);
								}
								else
								{
									UMaterialInstanceConstant* InstConstant = Cast<UMaterialInstanceConstant>(Mesh->GetMaterials()[m].MaterialInterface);

									if (InstConstant != nullptr)
									{
										InstConstant->Parent->GetTextureParameterValue(FName(*ParameterNames[i].GetPlainNameString()), OriginalTexture);
									}
								}

								PackageName = AssetPath + FString("/") + ResourceName;
								TMap<UObject*, UObject*> FakeReplacementMap;
								UTexture2D* DupTex = FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(SrcTex, ResourceName, PackageName, OriginalTexture, true, FakeReplacementMap, bUsedGrantedOverridingRights);
								ArrayCachedElement.Add(ResourceName);
								ArrayCachedObject.Add(DupTex);
								PackagesToSave.Add(DupTex->GetPackage());

								if (OriginalTexture != nullptr)
								{
									TextureReplacementMaps[m].Add(i, DupTex);
								}
							}
						}
					}
				}
			}

			// Duplicate non-Mutable material textures
			for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
				Material = Interface->GetMaterial();
				MaterialName = Material ? Material->GetName() : "Material";

				if (Material != nullptr)
				{
					TArray<FName> ParameterNames = FUnrealBakeHelpers::GetTextureParameterNames(Material);

					for (int32 i = 0; i < ParameterNames.Num(); i++)
					{
						TArray<FMaterialParameterInfo> InfoArray;
						TArray<FGuid> GuidArray;
						Material->GetAllTextureParameterInfo(InfoArray, GuidArray);
						
						if (Material->GetTextureParameterValue(InfoArray[i], Texture))
						{
							FString ParameterSanitized = ParameterNames[i].GetPlainNameString();
							RemoveRestrictedChars(ParameterSanitized);
							ResourceName = ObjectName + "_" + MaterialName + "_" + ParameterSanitized;

							if (ArrayCachedElement.Find(ResourceName) == INDEX_NONE)
							{
								if (!ManageBakingAction(AssetPath, ResourceName, bUsedGrantedOverridingRights))
								{
									return;
								}

								PackageName = AssetPath + FString("/") + ResourceName;
								TMap<UObject*, UObject*> FakeReplacementMap;
								DuplicatedObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Texture, ResourceName, PackageName, true, FakeReplacementMap, bUsedGrantedOverridingRights, false);
								ArrayCachedElement.Add(ResourceName);
								ArrayCachedObject.Add(DuplicatedObject);
								PackagesToSave.Add(DuplicatedObject->GetPackage());

								UTexture* DupTexture = Cast<UTexture>(DuplicatedObject);
								TextureReplacementMaps[m].Add(i, DupTexture);
							}
						}
					}
				}
			}


			// Duplicate the materials used by each material instance so that the replacement map has proper information 
			// when duplicating the material instances
			for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
				Material = Interface ? Interface->GetMaterial() : nullptr;

				if (Material)
				{
					ResourceName = ObjectName + "_Material_" + Material->GetName();

					if (!GetUniqueResourceName(Material, ResourceName, ArrayCachedObject, ArrayCachedElement))
					{
						continue;
					}

					if (!ManageBakingAction(AssetPath, ResourceName, bUsedGrantedOverridingRights))
					{
						return;
					}

					PackageName = AssetPath + FString("/") + ResourceName;
					TMap<UObject*, UObject*> FakeReplacementMap;
					DuplicatedObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Material, ResourceName, PackageName, 
						false, FakeReplacementMap, bUsedGrantedOverridingRights, bGenerateConstantMaterialInstances);
					ArrayCachedElement.Add(ResourceName);
					ArrayCachedObject.Add(DuplicatedObject);
					ReplacementMap.Add(Interface, DuplicatedObject);
					PackagesToSave.Add(DuplicatedObject->GetPackage());

					FUnrealBakeHelpers::CopyAllMaterialParameters(DuplicatedObject, Interface, TextureReplacementMaps[m]);
				}
			}
		}
		else
		{
			// Duplicate the material instances
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetMaterials().Num(); ++MaterialIndex)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[MaterialIndex].MaterialInterface;
				UMaterial* ParentMaterial = Interface->GetMaterial();
				FString MaterialName = ParentMaterial ? ParentMaterial->GetName() : "Material";

				// Material
				FString MatObjName = ObjectName + "_" + MaterialName;

				if (!GetUniqueResourceName(Interface, MatObjName, ArrayCachedObject, ArrayCachedElement))
				{
					continue;
				}

				if (!ManageBakingAction(AssetPath, MatObjName, bUsedGrantedOverridingRights))
				{
					return;
				}

				FString MatPkgName = AssetPath + FString("/") + MatObjName;
				UObject* DupMat = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Interface, MatObjName, 
					MatPkgName, false, ReplacementMap, bUsedGrantedOverridingRights, bGenerateConstantMaterialInstances);
				ArrayCachedObject.Add(DupMat);
				ArrayCachedElement.Add(MatObjName);
				PackagesToSave.Add(DupMat->GetPackage());

				UMaterialInstance* Inst = Cast<UMaterialInstance>(Interface);

				// Only need to duplicate the generate textures if the original material is a dynamic instance
				// If the material has Mutable textures, then it will be a dynamic material instance for sure
				if (Inst)
				{
					// Duplicate generated textures
					UMaterialInstanceDynamic* InstDynamic = Cast<UMaterialInstanceDynamic>(DupMat);
					UMaterialInstanceConstant* InstConstant = Cast<UMaterialInstanceConstant>(DupMat);

					if (InstDynamic || InstConstant)
					{
						for (int32 TextureIndex = 0; TextureIndex < Inst->TextureParameterValues.Num(); ++TextureIndex)
						{
							if (Inst->TextureParameterValues[TextureIndex].ParameterValue)
							{
								if (Inst->TextureParameterValues[TextureIndex].ParameterValue->HasAnyFlags(RF_Transient))
								{
									UTexture2D* SrcTex = Cast<UTexture2D>(Inst->TextureParameterValues[TextureIndex].ParameterValue);

									if (SrcTex)
									{
										FString ParameterSanitized = Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name.ToString();
										RemoveRestrictedChars(ParameterSanitized);

										FString TexObjName = ObjectName + "_" + MaterialName + "_" + ParameterSanitized;

										if (!GetUniqueResourceName(SrcTex, TexObjName, ArrayCachedObject, ArrayCachedElement))
										{
											UTexture* PrevTexture = Cast<UTexture>(ArrayCachedObject[ArrayCachedElement.Find(TexObjName)]);

											if (InstDynamic)
											{
												InstDynamic->SetTextureParameterValue(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, PrevTexture);
											}
											else if (InstConstant)
											{
												InstConstant->SetTextureParameterValueEditorOnly(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, PrevTexture);
											}
											
											continue;
										}

										if (!ManageBakingAction(AssetPath, TexObjName, bUsedGrantedOverridingRights))
										{
											return;
										}

										FString TexPkgName = AssetPath + FString("/") + TexObjName;
										TMap<UObject*, UObject*> FakeReplacementMap;
										UTexture2D* DupTex = FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(SrcTex, TexObjName, TexPkgName, nullptr, false, FakeReplacementMap, bUsedGrantedOverridingRights);
										ArrayCachedObject.Add(DupTex);
										ArrayCachedElement.Add(TexObjName);
										PackagesToSave.Add(DupTex->GetPackage());

										if (InstDynamic)
										{
											InstDynamic->SetTextureParameterValue(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, DupTex);
										}
										else if(InstConstant)
										{
											InstConstant->SetTextureParameterValueEditorOnly(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, DupTex);
										}
									}
									else
									{
										UE_LOG(LogMutable, Error, TEXT("A Mutable texture that is not a Texture2D has been found while baking a CustomizableObjectInstance."));
									}
								}
								else
								{
									// If it's not transient it's not a mutable texture, it's a pass-through texture
									// Just set the original texture
									if (InstDynamic)
									{
										InstDynamic->SetTextureParameterValue(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, Inst->TextureParameterValues[TextureIndex].ParameterValue);
									}
									else if (InstConstant)
									{
										InstConstant->SetTextureParameterValueEditorOnly(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, Inst->TextureParameterValues[TextureIndex].ParameterValue);
									}
								}
							}
						}
					}
				}
			}
		}
		
		// Skeletal Mesh's Skeleton
		if (Mesh->GetSkeleton())
		{
			const bool bTransient = Mesh->GetSkeleton()->GetPackage() == GetTransientPackage();

			// Don't duplicate if not transient or export all assets.
			if (bTransient || bExportAllResources)
			{
				FString SkeletonName = ObjectName + "_Skeleton";
				if (!ManageBakingAction(AssetPath, SkeletonName, bUsedGrantedOverridingRights))
				{
					return;
				}

				FString SkeletonPkgName = AssetPath + FString("/") + SkeletonName;
				UObject* DuplicatedSkeleton = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Mesh->GetSkeleton(), SkeletonName, 
					SkeletonPkgName, false, ReplacementMap, bUsedGrantedOverridingRights, false);

				ArrayCachedObject.Add(DuplicatedSkeleton);
				PackagesToSave.Add(DuplicatedSkeleton->GetPackage());
				ReplacementMap.Add(Mesh->GetSkeleton(), DuplicatedSkeleton);
			}
		}

		// Skeletal Mesh
		if (!ManageBakingAction(AssetPath, ObjectName, bUsedGrantedOverridingRights))
		{
			return;
		}

		FString PkgName = AssetPath + FString("/") + ObjectName;
		UObject* DupObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Mesh, ObjectName, PkgName, 
			false, ReplacementMap, bUsedGrantedOverridingRights, false);
		ArrayCachedObject.Add(DupObject);
		PackagesToSave.Add(DupObject->GetPackage());

		Mesh->Build();

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(DupObject);
		if (SkeletalMesh)
		{
			SkeletalMesh->GetLODInfoArray() = Mesh->GetLODInfoArray();

			SkeletalMesh->GetImportedModel()->SkeletalMeshModelGUID = FGuid::NewGuid();

			// Duplicate AssetUserData
			{
				const TArray<UAssetUserData*>* AssetUserDataArray = Mesh->GetAssetUserDataArray();
				for (const UAssetUserData* AssetUserData : *AssetUserDataArray)
				{
					if (AssetUserData)
					{
						// Duplicate to change ownership
						UAssetUserData* NewAssetUserData = Cast<UAssetUserData>(StaticDuplicateObject(AssetUserData, SkeletalMesh));
						SkeletalMesh->AddAssetUserData(NewAssetUserData);
					}
				}
			}

			// Add Instance Info in a custom AssetUserData
			{
				if (InInstance.GetAnimationGameplayTags().Num())
				{
					UCustomizableObjectInstanceUserData* InstanceData = NewObject<UCustomizableObjectInstanceUserData>(SkeletalMesh, NAME_None, RF_Public | RF_Transactional);
					InstanceData->SetAnimationGameplayTags(InInstance.GetAnimationGameplayTags());
					SkeletalMesh->AddAssetUserData(InstanceData);
				}
			}

			// Generate render data
			SkeletalMesh->Build();
		}

		// Remove duplicated UObjects from Root (previously added to avoid objects from being GC in the middle of the bake process)
		for (UObject* Obj : ArrayCachedObject)
		{
			Obj->RemoveFromRoot();
		}
	}

	// Save the packages generated during the baking operation  --------------------------------------------------------------------------------------
	
	// Complete the baking by saving the packages we have cached during the baking operation
	if (PackagesToSave.Num())
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, !FApp::IsUnattended());
	}
}

#undef LOCTEXT_NAMESPACE 
