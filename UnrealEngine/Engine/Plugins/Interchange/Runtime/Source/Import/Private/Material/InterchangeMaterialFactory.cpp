// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Material/InterchangeMaterialFactory.h"

#include "InterchangeImportCommon.h"
#include "InterchangeImportLog.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeTextureNode.h"
#include "InterchangeTextureFactoryNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeSourceData.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMaterialFactory)


#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif //WITH_EDITORONLY_DATA

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "InterchangeMaterialFactory"

namespace UE
{
	namespace Interchange
	{
		namespace MaterialFactory
		{
			namespace Internal
			{
#if WITH_EDITOR
				/**
				 * Finds a UMaterialExpression class by name.
				 * @param ClassName		The name of the class to look for (ie:UClass*->GetName()).
				 * @return				A sub class of UMaterialExpression or nullptr.
				 */
				TSubclassOf<UMaterialExpression> FindExpressionClass(const TCHAR* ClassName)
				{
					check(ClassName);

					UClass* MaterialExpressionClass = FindFirstObject<UClass>(ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous);

					if (!MaterialExpressionClass)
					{
						if (UObjectRedirector* RenamedClassRedirector = FindFirstObject<UObjectRedirector>(ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous))
						{
							MaterialExpressionClass = CastChecked<UClass>(RenamedClassRedirector->DestinationObject);
						}
					}

					if (MaterialExpressionClass && MaterialExpressionClass->IsChildOf<UMaterialExpression>())
					{
						return MaterialExpressionClass;
					}
					else
					{
						return nullptr;
					}
				}

				int32 GetInputIndex(UMaterialExpression& MaterialExpression, const FString& InputName)
				{
					int32 ExpressionInputIndex = 0;

					for (const FExpressionInput* ExpressionInput : MaterialExpression.GetInputs())
					{
						// MaterialFuncCall appends the type to the input name when calling GetInputName
						// and the InputName in FExpressionInput is optional so we'll check both here to be safe
						if (MaterialExpression.GetInputName(ExpressionInputIndex) == *InputName ||
							(ExpressionInput && ExpressionInput->InputName == *InputName))
						{
							return ExpressionInputIndex;
						}

						++ExpressionInputIndex;
					}

					return INDEX_NONE;
				}

				int32 GetOutputIndex(UMaterialExpression& MaterialExpression, const FString& OutputName)
				{
					int32 ExpressionOutputIndex = 0;

					for (const FExpressionOutput& ExpressionOutput : MaterialExpression.GetOutputs())
					{
						if (ExpressionOutput.OutputName == *OutputName)
						{
							return ExpressionOutputIndex;
						}

						++ExpressionOutputIndex;
					}

					return 0; // Consider 0 as the default output to connect to since most expressions have a single output
				}

				void SetupFunctionCallExpression(const UInterchangeFactoryBase::FCreateAssetParams& Arguments, const UInterchangeMaterialExpressionFactoryNode& ExpressionNode, UMaterialExpressionMaterialFunctionCall* FunctionCallExpression)
				{
					FInterchangeImportMaterialAsyncHelper& AsyncHelper = FInterchangeImportMaterialAsyncHelper::GetInstance();
					if (const UInterchangeMaterialFunctionCallExpressionFactoryNode* FunctionCallFactoryNode = Cast<UInterchangeMaterialFunctionCallExpressionFactoryNode>(&ExpressionNode))
					{
						FString MaterialFactoryNodeUid;
						FunctionCallFactoryNode->GetCustomMaterialFunctionDependency(MaterialFactoryNodeUid);

						if (const UInterchangeMaterialFunctionFactoryNode* MaterialFactoryNode = Cast<UInterchangeMaterialFunctionFactoryNode>(Arguments.NodeContainer->GetNode(MaterialFactoryNodeUid)))
						{
							FSoftObjectPath ReferenceObject;
							MaterialFactoryNode->GetCustomReferenceObject(ReferenceObject);
							if (UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(ReferenceObject.TryLoad()))
							{
								AsyncHelper.UpdateFromFunctionResource(MaterialFunction);
								FunctionCallExpression->SetMaterialFunction(MaterialFunction);
							}
						}
					}

					AsyncHelper.UpdateFromFunctionResource(FunctionCallExpression);
				}

				void SetupTextureExpression(const UInterchangeFactoryBase::FCreateAssetParams& Arguments, const UInterchangeMaterialExpressionFactoryNode* ExpressionNode, UMaterialExpressionTextureBase* TextureExpression)
				{
					using namespace UE::Interchange::Materials::Standard::Nodes::TextureSample;

					FString TextureFactoryNodeUid;
					ExpressionNode->GetStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Inputs::Texture.ToString()), TextureFactoryNodeUid);

					if (const UInterchangeTextureFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureFactoryNode>(Arguments.NodeContainer->GetNode(TextureFactoryNodeUid)))
					{
						FSoftObjectPath ReferenceObject;
						TextureFactoryNode->GetCustomReferenceObject(ReferenceObject);
						if (UTexture* Texture = Cast<UTexture>(ReferenceObject.TryLoad()))
						{
							TextureExpression->Texture = Texture;
						}
					}

					TextureExpression->AutoSetSampleType();
				}

				UMaterialExpression* CreateMaterialExpression(UMaterial* Material, UMaterialFunction* MaterialFunction, const TSubclassOf<UMaterialExpression>& ExpressionClass)
				{
					UObject* const SelectedAsset = nullptr;
					const int32 NodePosX = 0;
					const int32 NodePosY = 0;
					const bool bAllowMarkingPackageDirty = false;

					return UMaterialEditingLibrary::CreateMaterialExpressionEx(Material, MaterialFunction, ExpressionClass, SelectedAsset, NodePosX, NodePosY, bAllowMarkingPackageDirty);
				}

				template<class T>
				T* CreateMaterialExpression(UMaterial* Material, UMaterialFunction* MaterialFunction)
				{
					return Cast<T>(CreateMaterialExpression(Material, MaterialFunction, T::StaticClass()));
				}


				class FMaterialExpressionBuilder
				{
				public:
					FMaterialExpressionBuilder(UMaterial* InMaterial, UMaterialFunction* InMaterialFunction, const UInterchangeFactoryBase::FCreateAssetParams& InArguments)
						: Material(InMaterial)
						, MaterialFunction(InMaterialFunction)
						, Arguments(InArguments)
					{
						check((Material || MaterialFunction) && !(Material && MaterialFunction));
					}

					UMaterialExpression* CreateExpressionsForNode(const UInterchangeMaterialExpressionFactoryNode& ExpressionNode)
					{
						UMaterialExpression* MaterialExpression = Expressions.FindRef(ExpressionNode.GetUniqueID());

						if (MaterialExpression)
						{
							return MaterialExpression;
						}

						MaterialExpression = CreateExpression(ExpressionNode);
						if (!MaterialExpression)
						{
							return nullptr;
						}

						Expressions.Add(ExpressionNode.GetUniqueID()) = MaterialExpression;

						TArray<FString> Inputs;
						UInterchangeShaderPortsAPI::GatherInputs(&ExpressionNode, Inputs);

						for (const FString& InputName : Inputs)
						{
							FString ConnectedExpressionUid;
							FString OutputName;
							if (UInterchangeShaderPortsAPI::GetInputConnection(&ExpressionNode, InputName, ConnectedExpressionUid, OutputName))
							{
								const UInterchangeMaterialExpressionFactoryNode* ConnectedExpressionNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(ConnectedExpressionUid));
								if (ConnectedExpressionNode)
								{
									UMaterialExpression* ConnectedExpression = Expressions.FindRef(ConnectedExpressionUid);
									if (!ConnectedExpression)
									{
										ConnectedExpression = CreateExpressionsForNode(*ConnectedExpressionNode);
									}

									const int32 InputIndex = GetInputIndex(*MaterialExpression, InputName);
									const int32 OutputIndex = GetOutputIndex(*ConnectedExpression, OutputName);

									if (InputIndex != INDEX_NONE)
									{
										FExpressionInput* ExpressionInput = MaterialExpression->GetInput(InputIndex);
										if (ExpressionInput)
										{
											ConnectedExpression->ConnectExpression(ExpressionInput, OutputIndex);
										}
									}
									else
									{
										Messages.Add(FText::Format(LOCTEXT("InputNotFound", "Invalid input {0} for material expression node {1}."),
											FText::FromString(InputName),
											FText::FromString(ExpressionNode.GetDisplayLabel())));
									}
								}
							}
						}

						return MaterialExpression;
					}

					TArray<FText> Messages;

				private:
					UMaterialExpression* CreateExpression(const UInterchangeMaterialExpressionFactoryNode& ExpressionNode)
					{
						FString ExpressionClassName;
						ExpressionNode.GetCustomExpressionClassName(ExpressionClassName);

						TSubclassOf<UMaterialExpression> ExpressionClass = FindExpressionClass(*ExpressionClassName);

						if (!ExpressionClass.Get())
						{
							Messages.Add(FText::Format(LOCTEXT("ExpressionClassNotFound", "Invalid class {0} for material expression node {1}."),
								FText::FromString(ExpressionClassName),
								FText::FromString(ExpressionNode.GetDisplayLabel())));

							return nullptr;
						}

						UMaterialExpression* MaterialExpression = CreateMaterialExpression(Material, MaterialFunction, ExpressionClass);

						if (!MaterialExpression)
						{
							Messages.Add(FText::Format(LOCTEXT("MaterialExpressionCreationFailed", "Failed to create {0} object for material expression node {1}."),
								FText::FromString(ExpressionClassName),
								FText::FromString(ExpressionNode.GetDisplayLabel())));

							return nullptr;
						}

						if (Material)
						{
							// Set the parameter name if the material expression has one (some material expressions don't inherit from UMaterialExpressionParameter, ie: UMaterialExpressionTextureSampleParameter
							if (FNameProperty* Property = FindFProperty<FNameProperty>(MaterialExpression->GetClass(), GET_MEMBER_NAME_CHECKED(UMaterialExpressionParameter, ParameterName)))
							{
								*(Property->ContainerPtrToValuePtr<FName>(MaterialExpression)) = FName(*(ExpressionNode.GetDisplayLabel() + LexToString(Material->GetExpressions().Num())));
							}
						}

						ExpressionNode.ApplyAllCustomAttributeToObject(MaterialExpression);

						if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(MaterialExpression))
						{
							SetupTextureExpression(Arguments, &ExpressionNode, TextureExpression);
						}
						else if (UMaterialExpressionMaterialFunctionCall* FunctionCallExpression = Cast<UMaterialExpressionMaterialFunctionCall>(MaterialExpression))
						{
							SetupFunctionCallExpression(Arguments, ExpressionNode, FunctionCallExpression);
						}

						return MaterialExpression;
					}


					UMaterial* Material = nullptr;
					UMaterialFunction* MaterialFunction = nullptr;
					const UInterchangeFactoryBase::FCreateAssetParams& Arguments;
					TMap<FString, UMaterialExpression*> Expressions;
				};
#endif // #if WITH_EDITOR
			}
		}
	}
}

UClass* UInterchangeMaterialFactory::GetFactoryClass() const
{
	return UMaterialInterface::StaticClass();
}

UObject* UInterchangeMaterialFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
	UObject* Material = nullptr;

	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(Arguments.AssetNode);
	if (MaterialFactoryNode == nullptr)
	{
		return nullptr;
	}

	const UClass* MaterialClass = MaterialFactoryNode->GetObjectClass();
	if (!ensure(MaterialClass && MaterialClass->IsChildOf(GetFactoryClass())))
	{
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		if (MaterialClass->IsChildOf<UMaterialInstanceDynamic>())
		{
			if (const UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode = Cast<UInterchangeMaterialInstanceFactoryNode>(MaterialFactoryNode))
			{
				FString ParentPath;
				if (MaterialInstanceFactoryNode->GetCustomParent(ParentPath))
				{
					FSoftObjectPath ParentMaterial(ParentPath);
					Material = UMaterialInstanceDynamic::Create(Cast<UMaterialInterface>(ParentMaterial.TryLoad()), Arguments.Parent, *Arguments.AssetName);
				}
			}
		}
		else
		{
			Material = NewObject<UObject>(Arguments.Parent, MaterialClass, *Arguments.AssetName, RF_Public | RF_Standalone);
		}
	}
	else if (ExistingAsset->GetClass()->IsChildOf(MaterialClass))
	{
		//This is a reimport, we are just re-updating the source data
		Material = ExistingAsset;
	}

	if (!Material)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Material asset %s"), *Arguments.AssetName);
		return nullptr;
	}

#if WITH_EDITOR
	Material->PreEditChange(nullptr);

	if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(Material))
	{
		if (const UInterchangeMaterialInstanceFactoryNode* MaterialInstanceFactoryNode = Cast<UInterchangeMaterialInstanceFactoryNode>(MaterialFactoryNode))
		{
			FString ParentPath;
			if (MaterialInstanceFactoryNode->GetCustomParent(ParentPath))
			{
				FSoftObjectPath ParentMaterial(ParentPath);
				MaterialInstanceConstant->SetParentEditorOnly(Cast<UMaterialInterface>(ParentMaterial.TryLoad()));
			}
		}
	}
#endif //WITH_EDITOR

	return Material;
}

UObject* UInterchangeMaterialFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(Arguments.AssetNode);
	if (MaterialFactoryNode == nullptr)
	{
		return nullptr;
	}

	const UClass* MaterialClass = MaterialFactoryNode->GetObjectClass();
	check(MaterialClass && MaterialClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	UObject* MaterialObject = nullptr;
	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been created by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		MaterialObject = NewObject<UObject>(Arguments.Parent, MaterialClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if(ExistingAsset->GetClass()->IsChildOf(MaterialClass))
	{
		//This is a reimport, we are just re-updating the source data
		MaterialObject = ExistingAsset;
	}

	if (!MaterialObject)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Material asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	if (MaterialObject)
	{
		//Currently material re-import will not touch the material at all
		//TODO design a re-import process for the material (expressions and input connections)
		if(!Arguments.ReimportObject)
		{
			if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(MaterialObject))
			{
#if WITH_EDITOR
				if (UMaterial* Material = Cast<UMaterial>(MaterialObject))
				{
					SetupMaterial(Material, Arguments, MaterialFactoryNode);
				}
#endif // #if WITH_EDITOR

				MaterialFactoryNode->ApplyAllCustomAttributeToObject(MaterialObject);
			}
		}
		
		//Getting the file Hash will cache it into the source data
		Arguments.SourceData->GetFileContentHash();

		//The interchange completion task (call in the GameThread after the factories pass), will call PostEditChange which will trig another asynchronous system that will build all material in parallel
	}

	return MaterialObject;
}

/* This function is call in the completion task on the main thread, use it to call main thread post creation step for your assets*/
void UInterchangeMaterialFactory::PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments)
{
	check(IsInGameThread());
	Super::PreImportPreCompletedCallback(Arguments);

	if (ensure(Arguments.ImportedObject && Arguments.SourceData))
	{
		//We must call the Update of the asset source file in the main thread because UAssetImportData::Update execute some delegate we do not control
		UMaterialInterface* ImportedMaterialInterface = CastChecked<UMaterialInterface>(Arguments.ImportedObject);

#if WITH_EDITOR
		//Update the samplers type in case the textures were changed during their PreImportPreCompletedCallback
		if (UMaterial* ImportedMaterial = Cast<UMaterial>(ImportedMaterialInterface))
		{
			for (UMaterialExpression* Expression : ImportedMaterial->GetExpressions())
			{
				if (UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>(Expression))
				{
					TextureSample->AutoSetSampleType();
				}
			}
		}
		else
#endif // WITH_EDITOR
		if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(ImportedMaterialInterface))
		{
			// Material instances expect their parameters to only be updated from the game thread
			SetupMaterialInstance(MaterialInstance, Arguments.NodeContainer, Cast<UInterchangeBaseMaterialFactoryNode>(Arguments.FactoryNode));
		}

#if WITH_EDITORONLY_DATA
		UE::Interchange::FFactoryCommon::FUpdateImportAssetDataParameters UpdateImportAssetDataParameters(ImportedMaterialInterface
																										  , ImportedMaterialInterface->AssetImportData
																										  , Arguments.SourceData
																										  , Arguments.NodeUniqueID
																										  , Arguments.NodeContainer
																										  , Arguments.OriginalPipelines);

		ImportedMaterialInterface->AssetImportData = UE::Interchange::FFactoryCommon::UpdateImportAssetData(UpdateImportAssetDataParameters);
#endif // WITH_EDITORONLY_DATA
	}
}

bool UInterchangeMaterialFactory::GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const
{
#if WITH_EDITORONLY_DATA
	if (const UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(Object))
	{
		return UE::Interchange::FFactoryCommon::GetSourceFilenames(MaterialInterface->AssetImportData.Get(), OutSourceFilenames);
	}
#endif

	return false;
}

bool UInterchangeMaterialFactory::SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const
{
#if WITH_EDITORONLY_DATA
	if (const UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(Object))
	{
		return UE::Interchange::FFactoryCommon::SetSourceFilename(MaterialInterface->AssetImportData.Get(), SourceFilename, SourceIndex);
	}
#endif

	return false;
}

#if WITH_EDITOR
void UInterchangeMaterialFactory::SetupMaterial(UMaterial* Material, const FCreateAssetParams& Arguments, const UInterchangeBaseMaterialFactoryNode* BaseMaterialFactoryNode)
{
	using namespace UE::Interchange::Materials;
	using namespace UE::Interchange::MaterialFactory::Internal;

	const UInterchangeMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeMaterialFactoryNode>(BaseMaterialFactoryNode);

	FMaterialExpressionBuilder Builder(Material, nullptr, Arguments);

	if (UInterchangeShaderPortsAPI::HasInput(MaterialFactoryNode, Common::Parameters::BxDF))
	{
		FString ExpressionNodeUid;
		FString OutputName;

		UInterchangeShaderPortsAPI::GetInputConnection(MaterialFactoryNode, Common::Parameters::BxDF.ToString(), ExpressionNodeUid, OutputName);

		const UInterchangeMaterialExpressionFactoryNode* MaterialAttributes = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(ExpressionNodeUid));

		if (MaterialAttributes)
		{
			if (UMaterialExpression* MaterialAttributesExpression = Builder.CreateExpressionsForNode(*MaterialAttributes))
			{
				if (FExpressionInput* MaterialAttributesInput = Material->GetExpressionInputForProperty(MP_MaterialAttributes))
				{
					MaterialAttributesExpression->ConnectExpression(MaterialAttributesInput, GetOutputIndex(*MaterialAttributesExpression, OutputName));
					Material->bUseMaterialAttributes = true;
				}
			}
		}

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);

		return;
	}

	// Base Color
	{
		FString BaseColorUid;
		FString OutputName;

		if (MaterialFactoryNode->GetBaseColorConnection(BaseColorUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* BaseColor = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(BaseColorUid));

			if (BaseColor)
			{
				if (UMaterialExpression* BaseColorExpression = Builder.CreateExpressionsForNode(*BaseColor))
				{
					if (FExpressionInput* BaseColorInput = Material->GetExpressionInputForProperty(MP_BaseColor))
					{
						BaseColorExpression->ConnectExpression(BaseColorInput, GetOutputIndex(*BaseColorExpression, OutputName));
					}
				}
			}
		}
	}
	
	// Metallic
	{
		FString MetallicUid;
		FString OutputName;

		if (MaterialFactoryNode->GetMetallicConnection(MetallicUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* MetallicNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(MetallicUid));

			if (MetallicNode)
			{
				if (UMaterialExpression* MetallicExpression = Builder.CreateExpressionsForNode(*MetallicNode))
				{
					if (FExpressionInput* MetallicInput = Material->GetExpressionInputForProperty(MP_Metallic))
					{
						MetallicExpression->ConnectExpression(MetallicInput, GetOutputIndex(*MetallicExpression, OutputName));
					}
				}
			}
		}
	}

	// Specular
	{
		FString SpecularUid;
		FString OutputName;
		
		if (MaterialFactoryNode->GetSpecularConnection(SpecularUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* SpecularNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(SpecularUid));

			if (SpecularNode)
			{
				if (UMaterialExpression* SpecularExpression = Builder.CreateExpressionsForNode(*SpecularNode))
				{
					if (FExpressionInput* SpecularInput = Material->GetExpressionInputForProperty(MP_Specular))
					{
						SpecularExpression->ConnectExpression(SpecularInput, GetOutputIndex(*SpecularExpression, OutputName));
					}
				}
			}
		}
	}

	// Roughness
	{
		FString RoughnessUid;
		FString OutputName;

		if (MaterialFactoryNode->GetRoughnessConnection(RoughnessUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* Roughness = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(RoughnessUid));

			if (Roughness)
			{
				if (UMaterialExpression* RoughnessExpression = Builder.CreateExpressionsForNode(*Roughness))
				{
					if (FExpressionInput* RoughnessInput = Material->GetExpressionInputForProperty(MP_Roughness))
					{
						RoughnessExpression->ConnectExpression(RoughnessInput, GetOutputIndex(*RoughnessExpression, OutputName));
					}
				}
			}
		}
	}

	// Anisotropy
	{
		FString AnisotropyUid;
		FString OutputName;

		if (MaterialFactoryNode->GetAnisotropyConnection(AnisotropyUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* Anisotropy = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(AnisotropyUid));

			if (Anisotropy)
			{
				if (UMaterialExpression* AnisotropyExpression = Builder.CreateExpressionsForNode(*Anisotropy))
				{
					if (FExpressionInput* AnisotropyInput = Material->GetExpressionInputForProperty(MP_Anisotropy))
					{
						AnisotropyExpression->ConnectExpression(AnisotropyInput, GetOutputIndex(*AnisotropyExpression, OutputName));
					}
				}
			}
		}
	}

	// Emissive
	{
		FString EmissiveColorUid;
		FString OutputName;
		
		if (MaterialFactoryNode->GetEmissiveColorConnection(EmissiveColorUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* EmissiveColor = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(EmissiveColorUid));

			if (EmissiveColor)
			{
				if (UMaterialExpression* EmissiveExpression = Builder.CreateExpressionsForNode(*EmissiveColor))
				{
					if (FExpressionInput* EmissiveInput = Material->GetExpressionInputForProperty(MP_EmissiveColor))
					{
						EmissiveExpression->ConnectExpression(EmissiveInput, GetOutputIndex(*EmissiveExpression, OutputName));
					}
				}
			}
		}
	}
	
	// Normal
	{
		FString NormalUid;
		FString OutputName;
		
		if (MaterialFactoryNode->GetNormalConnection(NormalUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* NormalNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(NormalUid));

			if (NormalNode)
			{
				if (UMaterialExpression* NormalExpression = Builder.CreateExpressionsForNode(*NormalNode))
				{
					if (FExpressionInput* NormalInput = Material->GetExpressionInputForProperty(MP_Normal))
					{
						NormalExpression->ConnectExpression(NormalInput, GetOutputIndex(*NormalExpression, OutputName));
					}
				}
			}
		}
	}

	// Tangent
	{
		FString TangentUid;
		FString OutputName;

		if(MaterialFactoryNode->GetTangentConnection(TangentUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* TangentNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(TangentUid));

			if(TangentNode)
			{
				if(UMaterialExpression* TangentExpression = Builder.CreateExpressionsForNode(*TangentNode))
				{
					if(FExpressionInput* TangentInput = Material->GetExpressionInputForProperty(MP_Tangent))
					{
						TangentExpression->ConnectExpression(TangentInput, GetOutputIndex(*TangentExpression, OutputName));
					}
				}
			}
		}
	}

	// Subsurface
	{
		FString SubsurfaceColorUid;
		FString OutputName;

		if(MaterialFactoryNode->GetSubsurfaceConnection(SubsurfaceColorUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* SubsurfaceColorNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(SubsurfaceColorUid));

			if(SubsurfaceColorNode)
			{
				if(UMaterialExpression* SubsurfaceColorExpression = Builder.CreateExpressionsForNode(*SubsurfaceColorNode))
				{
					if(FExpressionInput* SubsurfaceColorInput = Material->GetExpressionInputForProperty(MP_SubsurfaceColor))
					{
						SubsurfaceColorExpression->ConnectExpression(SubsurfaceColorInput, GetOutputIndex(*SubsurfaceColorExpression, OutputName));
					}
				}
			}
		}
	}

	// Opacity
	{
		FString OpacityUid;
		FString OutputName;

		if (MaterialFactoryNode->GetOpacityConnection(OpacityUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* OpacityNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(OpacityUid));

			if (OpacityNode)
			{
				if (UMaterialExpression* OpacityExpression = Builder.CreateExpressionsForNode(*OpacityNode))
				{
					FExpressionInput* OpacityInput = Material->GetExpressionInputForProperty(MP_Opacity);

					TEnumAsByte<EBlendMode> BlendMode;
					if (MaterialFactoryNode->GetCustomBlendMode(BlendMode))
					{
						if (BlendMode == EBlendMode::BLEND_Masked)
						{
							OpacityInput = Material->GetExpressionInputForProperty(MP_OpacityMask);
						}
					}
					
					if (OpacityInput)
					{
						OpacityExpression->ConnectExpression(OpacityInput, GetOutputIndex(*OpacityExpression, OutputName));
					}
				}
			}
		}
	}

	// Occlusion
	{
		FString OcclusionUid;
		FString OutputName;

		if (MaterialFactoryNode->GetOcclusionConnection(OcclusionUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* OcclusionNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(OcclusionUid));

			if (OcclusionNode)
			{
				if (UMaterialExpression* OcclusionExpression = Builder.CreateExpressionsForNode(*OcclusionNode))
				{
					if (FExpressionInput* OcclusionInput = Material->GetExpressionInputForProperty(MP_AmbientOcclusion))
					{
						OcclusionExpression->ConnectExpression(OcclusionInput, GetOutputIndex(*OcclusionExpression, OutputName));
					}
				}
			}
		}
	}

	// Refraction
	{
		FString RefractionUid;
		FString OutputName;

		if (MaterialFactoryNode->GetRefractionConnection(RefractionUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* RefractionNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(RefractionUid));

			if (RefractionNode)
			{
				if (UMaterialExpression* RefractionExpression = Builder.CreateExpressionsForNode(*RefractionNode))
				{
					if (FExpressionInput* RefractionInput = Material->GetExpressionInputForProperty(MP_Refraction))
					{
						RefractionExpression->ConnectExpression(RefractionInput, GetOutputIndex(*RefractionExpression, OutputName));
					}
				}
			}
		}
	}

	// Clear Coat
	{
		FString ClearCoatUid;
		FString OutputName;

		if (MaterialFactoryNode->GetClearCoatConnection(ClearCoatUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* ClearCoatNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(ClearCoatUid));

			if (ClearCoatNode)
			{
				if (UMaterialExpression* ClearCoatExpression = Builder.CreateExpressionsForNode(*ClearCoatNode))
				{
					if (FExpressionInput* ClearCoatInput = Material->GetExpressionInputForProperty(MP_CustomData0))
					{
						ClearCoatExpression->ConnectExpression(ClearCoatInput, GetOutputIndex(*ClearCoatExpression, OutputName));
					}
				}
			}
		}
	}

	// Clear Coat Roughness
	{
		FString ClearCoatRoughnessUid;
		FString OutputName;

		if (MaterialFactoryNode->GetClearCoatRoughnessConnection(ClearCoatRoughnessUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* ClearCoatRoughnessNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(ClearCoatRoughnessUid));

			if (ClearCoatRoughnessNode)
			{
				if (UMaterialExpression* ClearCoatRoughnessExpression = Builder.CreateExpressionsForNode(*ClearCoatRoughnessNode))
				{
					if (FExpressionInput* ClearCoatRoughnessInput = Material->GetExpressionInputForProperty(MP_CustomData1))
					{
						ClearCoatRoughnessExpression->ConnectExpression(ClearCoatRoughnessInput, GetOutputIndex(*ClearCoatRoughnessExpression, OutputName));
					}
				}
			}
		}
	}

	// Clear Coat Normal
	{
		FString ClearCoatNormalUid;
		FString OutputName;

		if (MaterialFactoryNode->GetClearCoatNormalConnection(ClearCoatNormalUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* ClearCoatNormalNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(ClearCoatNormalUid));

			if (ClearCoatNormalNode)
			{
				if (UMaterialExpression* ClearCoatNormalExpression = Builder.CreateExpressionsForNode(*ClearCoatNormalNode))
				{
					UMaterialExpression* ClearCoatNormalCustomOutput = CreateMaterialExpression(Material, nullptr, UMaterialExpressionClearCoatNormalCustomOutput::StaticClass());
					
					ClearCoatNormalExpression->ConnectExpression(ClearCoatNormalCustomOutput->GetInput(0), GetOutputIndex(*ClearCoatNormalExpression, OutputName));
				}
			}
		}
	}

	// Thin Translucent
	{
		FString TransmissionColorUid;
		FString OutputName;

		if (MaterialFactoryNode->GetTransmissionColorConnection(TransmissionColorUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* TransmissionColorNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(TransmissionColorUid));

			if (TransmissionColorNode)
			{
				if (UMaterialExpression* TransmissionColorExpression = Builder.CreateExpressionsForNode(*TransmissionColorNode))
				{
					UMaterialExpression* ThinTranslucentMaterialOutput = CreateMaterialExpression(Material, nullptr, UMaterialExpressionThinTranslucentMaterialOutput::StaticClass());
					
					TransmissionColorExpression->ConnectExpression(ThinTranslucentMaterialOutput->GetInput(0), GetOutputIndex(*TransmissionColorExpression, OutputName));
				}
			}
		}
	}

	// Fuzz Color
	{
		FString FuzzColorUid;
		FString OutputName;

		if (MaterialFactoryNode->GetFuzzColorConnection(FuzzColorUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* FuzzColorNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(FuzzColorUid));

			if (FuzzColorNode)
			{
				if (UMaterialExpression* FuzzColorExpression = Builder.CreateExpressionsForNode(*FuzzColorNode))
				{
					if (FExpressionInput* FuzzColorInput = Material->GetExpressionInputForProperty(MP_SubsurfaceColor))
					{
						FuzzColorExpression->ConnectExpression(FuzzColorInput, GetOutputIndex(*FuzzColorExpression, OutputName));
					}
				}
			}
		}
	}

	// Cloth
	{
		FString ClothUid;
		FString OutputName;

		if (MaterialFactoryNode->GetClothConnection(ClothUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* ClothNode = Cast<UInterchangeMaterialExpressionFactoryNode>(Arguments.NodeContainer->GetNode(ClothUid));

			if (ClothNode)
			{
				if (UMaterialExpression* ClothExpression = Builder.CreateExpressionsForNode(*ClothNode))
				{
					if (FExpressionInput* ClothInput = Material->GetExpressionInputForProperty(MP_CustomData0))
					{
						ClothExpression->ConnectExpression(ClothInput, GetOutputIndex(*ClothExpression, OutputName));
					}
				}
			}
		}
	}

	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
}
#endif // #if WITH_EDITOR

void UInterchangeMaterialFactory::SetupMaterialInstance(UMaterialInstance* MaterialInstance, const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode)
{
	if (!MaterialFactoryNode || !NodeContainer)
	{
		return;
	}

	TArray<FString> Inputs;
	UInterchangeShaderPortsAPI::GatherInputs(MaterialFactoryNode, Inputs);

	for (const FString& InputName : Inputs)
	{
		const FName ParameterName = *InputName;

		switch(UInterchangeShaderPortsAPI::GetInputType(MaterialFactoryNode, InputName))
		{
		case UE::Interchange::EAttributeTypes::Float:
			{
				float InstanceValue;
				if (MaterialInstance->GetScalarParameterValue(ParameterName, InstanceValue))
				{
					float InputValue = 0.f;
					MaterialFactoryNode->GetFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue);

					if ( !FMath::IsNearlyEqual(InputValue, InstanceValue) )
					{
#if WITH_EDITOR
						if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInstance))
						{
							MaterialInstanceConstant->SetScalarParameterValueEditorOnly(ParameterName, InputValue);
						}
						else
#endif // #if WITH_EDITOR
						if (UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>(MaterialInstance))
						{
							MaterialInstanceDynamic->SetScalarParameterValue(ParameterName, InputValue);
						}
					}
				}
			}
			break;
		case UE::Interchange::EAttributeTypes::LinearColor:
			{
				FLinearColor InstanceValue;
				if (MaterialInstance->GetVectorParameterValue(ParameterName, InstanceValue))
				{
					FLinearColor InputValue;
					if (MaterialFactoryNode->GetLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue))
					{
						if ( !InputValue.Equals(InstanceValue) )
						{
#if WITH_EDITOR
							if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInstance))
							{
								MaterialInstanceConstant->SetVectorParameterValueEditorOnly(ParameterName, InputValue);
							}
							else
#endif // #if WITH_EDITOR
							if (UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>(MaterialInstance))
							{
								MaterialInstanceDynamic->SetVectorParameterValue(ParameterName, InputValue);
							}
						}
					}
				}
			}
			break;
		case UE::Interchange::EAttributeTypes::String:
			{
				UTexture* InstanceValue;
				if (MaterialInstance->GetTextureParameterValue(ParameterName, InstanceValue))
				{
					FString InputValue;
					if (MaterialFactoryNode->GetStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputName), InputValue))
					{
						if (const UInterchangeTextureFactoryNode* TextureFactoryNode = Cast<UInterchangeTextureFactoryNode>(NodeContainer->GetNode(InputValue)))
						{
							FSoftObjectPath ReferenceObject;
							TextureFactoryNode->GetCustomReferenceObject(ReferenceObject);
							if (UTexture* InputTexture = Cast<UTexture>(ReferenceObject.TryLoad()))
							{
								if ( InputTexture != InstanceValue )
								{
#if WITH_EDITOR
									if (UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInstance))
									{
										MaterialInstanceConstant->SetTextureParameterValueEditorOnly(ParameterName, InputTexture);
									}
									else
#endif // #if WITH_EDITOR
									if (UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>(MaterialInstance))
									{
										MaterialInstanceDynamic->SetTextureParameterValue(ParameterName, InputTexture);
									}
								}
							}
						}
					}
				}
			}
			break;
		}
	}
}

UClass* UInterchangeMaterialFunctionFactory::GetFactoryClass() const
{
	return UMaterialFunctionInterface::StaticClass();
}

UObject* UInterchangeMaterialFunctionFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
	UObject* Material = nullptr;

	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	const UInterchangeMaterialFunctionFactoryNode* MaterialFactoryNode = Cast<UInterchangeMaterialFunctionFactoryNode>(Arguments.AssetNode);
	if (!ensure(MaterialFactoryNode))
	{
		return nullptr;
	}

	const UClass* MaterialClass = MaterialFactoryNode->GetObjectClass();
	if (!ensure(MaterialClass && MaterialClass->IsChildOf(GetFactoryClass())))
	{
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		Material = NewObject<UObject>(Arguments.Parent, MaterialClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(MaterialClass))
	{
		//This is a reimport, we are just re-updating the source data
		Material = ExistingAsset;
	}

	if (!Material)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Material asset %s"), *Arguments.AssetName);
		return nullptr;
	}

#if WITH_EDITOR
	Material->PreEditChange(nullptr);
#endif //WITH_EDITOR

	return Material;
}

UObject* UInterchangeMaterialFunctionFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	const UInterchangeMaterialFunctionFactoryNode* MaterialFactoryNode = Cast<UInterchangeMaterialFunctionFactoryNode>(Arguments.AssetNode);
	if (!ensure(MaterialFactoryNode))
	{
		return nullptr;
	}

	const UClass* MaterialClass = MaterialFactoryNode->GetObjectClass();
	check(MaterialClass && MaterialClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	UObject* MaterialObject = nullptr;
	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been created by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		MaterialObject = NewObject<UObject>(Arguments.Parent, MaterialClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(MaterialClass))
	{
		//This is a reimport, we are just re-updating the source data
		MaterialObject = ExistingAsset;
	}

	if (!MaterialObject)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create Material asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	if (MaterialObject)
	{
		//Currently material re-import will not touch the material at all
		//TODO design a re-import process for the material (expressions and input connections)
		if (!Arguments.ReimportObject)
		{
			if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(MaterialObject))
			{
#if WITH_EDITOR
				SetupMaterial(MaterialFunction, Arguments, MaterialFactoryNode);
#endif // #if WITH_EDITOR

				MaterialFactoryNode->ApplyAllCustomAttributeToObject(MaterialObject);
			}
		}

		//Getting the file Hash will cache it into the source data
		Arguments.SourceData->GetFileContentHash();

		//The interchange completion task (call in the GameThread after the factories pass), will call PostEditChange which will trig another asynchronous system that will build all material in parallel
	}

	return MaterialObject;
}

/* This function is call in the completion task on the main thread, use it to call main thread post creation step for your assets*/
void UInterchangeMaterialFunctionFactory::PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments)
{
	Super::PreImportPreCompletedCallback(Arguments);

#if WITH_EDITORONLY_DATA
	if (ensure(Arguments.ImportedObject && Arguments.SourceData))
	{
		UMaterialFunction* ImportedMaterialFunction = CastChecked<UMaterialFunction>(Arguments.ImportedObject);

		if (UMaterialFunctionEditorOnlyData* Data = ImportedMaterialFunction->GetEditorOnlyData())
		{
			//Update the samplers type in case the textures were changed during their PreImportPreCompletedCallback
			for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
			{
				if (UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>(Expression))
				{
					TextureSample->AutoSetSampleType();
				}
			}
		}
	}
#endif
}

#if WITH_EDITOR
void UInterchangeMaterialFunctionFactory::SetupMaterial(UMaterialFunction* MaterialFunction, const FCreateAssetParams& Arguments, const UInterchangeMaterialFunctionFactoryNode* MaterialFunctionFactoryNode)
{
	using namespace UE::Interchange::MaterialFactory::Internal;
	using namespace UE::Interchange::Materials;

	FMaterialExpressionBuilder Builder(nullptr, MaterialFunction, Arguments);

	UMaterialExpressionFunctionOutput* Output = CreateMaterialExpression<UMaterialExpressionFunctionOutput>(nullptr, MaterialFunction);

	UMaterialExpressionMakeMaterialAttributes* Attrib = CreateMaterialExpression<UMaterialExpressionMakeMaterialAttributes>(nullptr, MaterialFunction);
	Attrib->ConnectExpression(Output->GetInput(0), 0);

	TFunction<bool(const FString& , FExpressionInput*)> ConnectInput;
	ConnectInput = [&MaterialFunctionFactoryNode, &Builder, NodeContainer = Arguments.NodeContainer](const FString& InputName, FExpressionInput* Input) -> bool
	{
		FString ExpressionNodeUid;
		FString OutputName;

		if (MaterialFunctionFactoryNode->GetInputConnection(InputName, ExpressionNodeUid, OutputName))
		{
			const UInterchangeMaterialExpressionFactoryNode* ExpressionFactoryNode = Cast<UInterchangeMaterialExpressionFactoryNode>(NodeContainer->GetNode(ExpressionNodeUid));

			if (ExpressionFactoryNode)
			{
				if (UMaterialExpression* MaterialExpression = Builder.CreateExpressionsForNode(*ExpressionFactoryNode))
				{
					MaterialExpression->ConnectExpression(Input, 0);
					return true;
				}
			}
		}

		return false;
	};

	// Only material functions with BSDF output are supported for now
	if (!ConnectInput(Common::Parameters::BxDF.ToString(), Attrib->GetInput(0)))
	{
		ConnectInput(PBR::Parameters::BaseColor.ToString(), &Attrib->BaseColor);
		ConnectInput(PBR::Parameters::Metallic.ToString(), &Attrib->Metallic);
		ConnectInput(PBR::Parameters::Specular.ToString(), &Attrib->Specular);
		ConnectInput(PBR::Parameters::Roughness.ToString(), &Attrib->Roughness);
		ConnectInput(PBR::Parameters::EmissiveColor.ToString(), &Attrib->EmissiveColor);
		ConnectInput(PBR::Parameters::Normal.ToString(), &Attrib->Normal);
		ConnectInput(PBR::Parameters::Anisotropy.ToString(), &Attrib->Anisotropy);
		ConnectInput(PBR::Parameters::Tangent.ToString(), &Attrib->Tangent);
		ConnectInput(PBR::Parameters::IndexOfRefraction.ToString(), &Attrib->Refraction);
		ConnectInput(PBR::Parameters::Occlusion.ToString(), &Attrib->AmbientOcclusion);
		ConnectInput(ClearCoat::Parameters::ClearCoat.ToString(), &Attrib->ClearCoat);
		ConnectInput(ClearCoat::Parameters::ClearCoatRoughness.ToString(), &Attrib->ClearCoatRoughness);
		ConnectInput(Subsurface::Parameters::SubsurfaceColor.ToString(), &Attrib->SubsurfaceColor);
	}

	UMaterialEditingLibrary::LayoutMaterialFunctionExpressions(MaterialFunction);

	MaterialFunction->UpdateDependentFunctionCandidates();
}

void FInterchangeImportMaterialAsyncHelper::UpdateFromFunctionResource(UMaterialExpressionMaterialFunctionCall* MaterialFunctionCall)
{
	FScopeLock Lock(&UpdatedMaterialFunctionCallsLock);
	if(UpdatedMaterialFunctionCalls.Find(MaterialFunctionCall) == nullptr)
	{
		UpdatedMaterialFunctionCalls.Add(MaterialFunctionCall);
		MaterialFunctionCall->UpdateFromFunctionResource();
	}
}

void FInterchangeImportMaterialAsyncHelper::UpdateFromFunctionResource(UMaterialFunctionInterface* MaterialFunction)
{
	FScopeLock Lock(&UpdatedMaterialFunctionsLock);
	if(UpdatedMaterialFunctions.Find(MaterialFunction) == nullptr)
	{
		UpdatedMaterialFunctions.Add(MaterialFunction);
		MaterialFunction->UpdateFromFunctionResource();
	}
}
#endif // #if WITH_EDITOR

FInterchangeImportMaterialAsyncHelper& FInterchangeImportMaterialAsyncHelper::GetInstance()
{
	static FInterchangeImportMaterialAsyncHelper Instance;
	return Instance;
}

void FInterchangeImportMaterialAsyncHelper::CleanUp()
{
	UpdatedMaterialFunctionCalls.Empty();
	UpdatedMaterialFunctions.Empty();
}

#undef LOCTEXT_NAMESPACE

