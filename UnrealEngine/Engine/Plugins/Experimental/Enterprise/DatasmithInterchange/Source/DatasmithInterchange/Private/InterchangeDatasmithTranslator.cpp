// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDatasmithTranslator.h"

#include "InterchangeDatasmithAreaLightNode.h"
#include "InterchangeDatasmithLog.h"
#include "InterchangeDatasmithMaterialNode.h"
#include "InterchangeDatasmithSceneNode.h"
#include "InterchangeDatasmithTextureData.h"
#include "InterchangeDatasmithUtils.h"

#include "DatasmithAnimationElements.h"
#include "DatasmithMaterialElements.h"
#include "DatasmithScene.h"
#include "DatasmithSceneSource.h"
#include "DatasmithTranslatableSource.h"
#include "DatasmithTranslatorManager.h"
#include "DatasmithUtils.h"
#include "DatasmithVariantElements.h"
#include "IDatasmithSceneElements.h"

#include "ExternalSourceModule.h"
#include "SourceUri.h"
#include "InterchangeCameraNode.h"
#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeLightNode.h"
#include "InterchangeManager.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMaterialInstanceNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeVariantSetNode.h"

#include "StaticMeshOperations.h"

#include "Misc/App.h"

#define LOCTEXT_NAMESPACE "DatasmithInterchange"

bool UInterchangeDatasmithTranslator::CanImportSourceData(const UInterchangeSourceData* InSourceData) const
{
	using namespace UE::DatasmithImporter;

	const FString FilePath = InSourceData->GetFilename();
	const FString FileExtension = FPaths::GetExtension(FilePath);
	if (FileExtension.Equals(TEXT("gltf"), ESearchCase::IgnoreCase) || FileExtension.Equals(TEXT("glb"), ESearchCase::IgnoreCase))
	{
		// Do not translate gltf since there is already a native gltf interchange translator. 
		return false;
	}

	const FSourceUri FileNameUri = FSourceUri::FromFilePath(FilePath);
	TSharedPtr<FExternalSource> ExternalSource = IExternalSourceModule::GetOrCreateExternalSource(FileNameUri);

	return ExternalSource.IsValid() && ExternalSource->IsAvailable();
}

bool UInterchangeDatasmithTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	using namespace UE::DatasmithImporter;
	using namespace UE::DatasmithInterchange;

	// TODO: This code should eventually go into UInterchangeTranslatorBase once the FExternalSource module gets integrated into Interchange
	FString FilePath = FPaths::ConvertRelativePathToFull(SourceData->GetFilename());
	FileName = FPaths::GetCleanFilename(FilePath);
	const FSourceUri FileNameUri = FSourceUri::FromFilePath(FilePath);
	const_cast<UInterchangeDatasmithTranslator*>(this)->LoadedExternalSource = IExternalSourceModule::GetOrCreateExternalSource(FileNameUri);

	if (!LoadedExternalSource.IsValid() || !LoadedExternalSource->IsAvailable())
	{
		return false;
	}

	UClass* Class = UInterchangeDatasmithSceneNode::StaticClass();
	if (!ensure(Class))
	{
		return false;
	}

	// Temporary: Update the tessellation options of the associated translator
	{
		const TSharedPtr<IDatasmithTranslator>& DatasmithTranslator = LoadedExternalSource->GetAssetTranslator();

		const FString OptionFilePath = BuildConfigFilePath(FilePath);

		if (DatasmithTranslator.IsValid() && FPaths::FileExists(OptionFilePath))
		{
			TArray<TObjectPtr<UDatasmithOptionsBase>> ImportOptions;
			DatasmithTranslator->GetSceneImportOptions(ImportOptions);

			for (TObjectPtr<UDatasmithOptionsBase>& Option : ImportOptions)
			{
				Option->LoadConfig(nullptr, *OptionFilePath);
			}

			DatasmithTranslator->SetSceneImportOptions(ImportOptions);
		}
	}

	StartTime = FPlatformTime::Cycles64();
	FPaths::NormalizeFilename(FilePath);

	// Should it be mutable instead? If Translate is const should we really be doing this?.
	TSharedPtr<IDatasmithScene> DatasmithScene = LoadedExternalSource->TryLoad();
	if (!DatasmithScene.IsValid())
	{
		return false;
	}

	// Datasmith Scene Node
	{
		FString DisplayLabel = DatasmithScene->GetName();
		FString NodeUID(NodeUtils::DatasmithScenePrefix + FilePath);
		UInterchangeDatasmithSceneNode* DatasmithSceneNode = NewObject<UInterchangeDatasmithSceneNode>(&BaseNodeContainer, Class);
		if (!ensure(DatasmithSceneNode))
		{
			return false;
		}
		DatasmithSceneNode->InitializeNode(NodeUID, DisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);

		// Assigning those variables are our only way to pass the translated state to the part of the pipeline that does not use interchange yet.
		DatasmithSceneNode->ExternalSource = LoadedExternalSource;
		DatasmithSceneNode->DatasmithScene = DatasmithScene;

		BaseNodeContainer.AddNode(DatasmithSceneNode);
	}

	// Texture Nodes
	{
		FDatasmithUniqueNameProvider TextureNameProvider;

		for (int32 TextureIndex = 0, TextureNum = DatasmithScene->GetTexturesCount(); TextureIndex < TextureNum; ++TextureIndex)
		{
			if (TSharedPtr<IDatasmithTextureElement> TextureElement = DatasmithScene->GetTexture(TextureIndex))
			{
				UInterchangeTexture2DNode* TextureNode = NewObject<UInterchangeTexture2DNode>(&BaseNodeContainer);
				const FString TextureNodeUid = NodeUtils::TexturePrefix + TextureElement->GetName();
				const FString DisplayLabel = TextureNameProvider.GenerateUniqueName(TextureElement->GetLabel());

				TextureNode->InitializeNode(TextureNodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);
				TextureNode->SetPayLoadKey(LexToString(TextureIndex));
				TextureUtils::ApplyTextureElementToNode(TextureElement.ToSharedRef(), TextureNode);

				BaseNodeContainer.AddNode(TextureNode);
			}
		}
	}

	// Materials
	{
		FDatasmithUniqueNameProvider MaterialsNameProvider;
		const TCHAR* HostName = DatasmithScene->GetHost();

		TArray<TSharedPtr<IDatasmithBaseMaterialElement>> MaterialElements;
		MaterialElements.Reserve(DatasmithScene->GetMaterialsCount());

		for (int32 MaterialIndex = 0, MaterialNum = DatasmithScene->GetMaterialsCount(); MaterialIndex < MaterialNum; ++MaterialIndex)
		{
			if (TSharedPtr<IDatasmithBaseMaterialElement> MaterialElement = DatasmithScene->GetMaterial(MaterialIndex))
			{
				MaterialElements.Add(MaterialElement);
			}
		}

		MaterialUtils::ProcessMaterialElements(MaterialElements);

		for (TSharedPtr<IDatasmithBaseMaterialElement>& MaterialElement : MaterialElements)
		{
			if (UInterchangeBaseNode* MaterialNode = MaterialUtils::AddMaterialNode(MaterialElement, BaseNodeContainer))
			{
				const FString DisplayLabel = MaterialsNameProvider.GenerateUniqueName(MaterialNode->GetDisplayLabel());
				MaterialNode->SetDisplayLabel(DisplayLabel);

				if (MaterialElement->IsA(EDatasmithElementType::MaterialInstance))
				{
					UInterchangeMaterialInstanceNode* ReferenceMaterialNode = Cast<UInterchangeMaterialInstanceNode>(MaterialNode);
					if (int32 MaterialType; ReferenceMaterialNode->GetInt32Attribute(MaterialUtils::MaterialTypeAttrName, MaterialType) && EDatasmithReferenceMaterialType(MaterialType) == EDatasmithReferenceMaterialType::Custom)
					{
						ReferenceMaterialNode->SetCustomParent(static_cast<IDatasmithMaterialInstanceElement&>(*MaterialElement).GetCustomMaterialPathName());
					}
					else
					{
						ReferenceMaterialNode->SetCustomParent(HostName);
					}
				}
			}
		}
	}

	// Static Meshes
	{
		FDatasmithUniqueNameProvider StaticMeshNameProvider;
		for (int32 MeshIndex = 0, MeshNum = DatasmithScene->GetMeshesCount(); MeshIndex < MeshNum; ++MeshIndex)
		{
			if (const TSharedPtr<IDatasmithMeshElement> MeshElement = DatasmithScene->GetMesh(MeshIndex))
			{
				UInterchangeMeshNode* MeshNode = NewObject<UInterchangeMeshNode>(&BaseNodeContainer);
				const FString MeshNodeUid = NodeUtils::MeshPrefix + MeshElement->GetName();
				const FString DisplayLabel = StaticMeshNameProvider.GenerateUniqueName(MeshElement->GetLabel());

				MeshNode->InitializeNode(MeshNodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);
				MeshNode->SetPayLoadKey(LexToString(MeshIndex), EInterchangeMeshPayLoadType::STATIC);
				MeshNode->SetSkinnedMesh(false);
				MeshNode->SetCustomHasVertexNormal(true);
				// TODO: Interchange expect each LOD to have its own mesh node and to declare the number of vertices, however we don't know the content of a datasmith mesh until its bulk data is loaded.
				//       It is not clear what would be the proper way to properly translate the content of the Datasmith meshes without	loading all this data during the translation phase (done on the main thread).

				TSharedPtr<IDatasmithMaterialIDElement> GlobalMaterialID;
				for (int32 SlotIndex = 0, SlotCount = MeshElement->GetMaterialSlotCount(); SlotIndex < SlotCount; ++SlotIndex)
				{
					if (TSharedPtr<IDatasmithMaterialIDElement> MaterialID = MeshElement->GetMaterialSlotAt(SlotIndex))
					{
						if (MaterialID->GetId() == -1)
						{
							GlobalMaterialID = MaterialID;
							break;
						}
					}
				}

				if (GlobalMaterialID)
				{
					// Set dedicated attribute with value of material Uid.
					// Corresponding factory then mesh asset will be updated accordingly pre then post import in the pipeline
					const FString MaterialUid = NodeUtils::MaterialPrefix + GlobalMaterialID->GetName();
					MeshNode->AddStringAttribute(MeshUtils::MeshMaterialAttrName, MaterialUid);
				}
				else
				{
					for (int32 SlotIndex = 0, SlotCount = MeshElement->GetMaterialSlotCount(); SlotIndex < SlotCount; ++SlotIndex)
					{
						if (const TSharedPtr<IDatasmithMaterialIDElement> MaterialID = MeshElement->GetMaterialSlotAt(SlotIndex))
						{
							const FString MaterialUid = NodeUtils::MaterialPrefix + MaterialID->GetName();
							if (BaseNodeContainer.GetNode(MaterialUid) != nullptr)
							{
								MeshNode->SetSlotMaterialDependencyUid(*FString::FromInt(MaterialID->GetId()), MaterialUid);
							}
						}
					}
				}

				BaseNodeContainer.AddNode(MeshNode);
			}
		}
	}

	//Actors
	{
		// Add base scene node.
		UInterchangeSceneNode* SceneNode = NewObject< UInterchangeSceneNode >(&BaseNodeContainer);
		const FString SceneName = DatasmithScene->GetName();
		const FString SceneNodeUid = NodeUtils::ScenePrefix + SceneName;
		SceneNode->InitializeNode(SceneNodeUid, DatasmithScene->GetLabel(), EInterchangeNodeContainerType::TranslatedScene);
		// TODO: This should be the instantiation of the DatasmithScene asset, and create a DatasmithSceneActor.
		
		BaseNodeContainer.AddNode(SceneNode);

		for (int32 ActorIndex = 0, ActorNum = DatasmithScene->GetActorsCount(); ActorIndex < ActorNum; ++ActorIndex)
		{
			if (const TSharedPtr<IDatasmithActorElement> ActorElement = DatasmithScene->GetActor(ActorIndex))
			{
				HandleDatasmithActor(BaseNodeContainer, ActorElement.ToSharedRef(), SceneNode);
			}
		}
	}

	// Level sequences
	{
		const int32 SequencesCount = DatasmithScene->GetLevelSequencesCount();
		
		TArray<TSharedPtr<IDatasmithLevelSequenceElement>> LevelSequences;
		LevelSequences.Reserve(SequencesCount);


		for (int32 SequenceIndex = 0; SequenceIndex < SequencesCount; ++SequenceIndex)
		{
			TSharedPtr<IDatasmithLevelSequenceElement> SequenceElement = DatasmithScene->GetLevelSequence(SequenceIndex);
			if (!SequenceElement)
			{
				continue;
			}

			FDatasmithLevelSequencePayload LevelSequencePayload;
			LoadedExternalSource->GetAssetTranslator()->LoadLevelSequence(SequenceElement.ToSharedRef(), LevelSequencePayload);

			if (SequenceElement->GetAnimationsCount() > 0)
			{
				LevelSequences.Add(SequenceElement);
			}
		}

		AnimUtils::TranslateLevelSequences(LevelSequences, BaseNodeContainer, AnimationPayLoadMapping);
	}

	// Level variant sets
	// Note: Variants are not supported yet in game play mode
	if(!FApp::IsGame()) 
	{
		const int32 LevelVariantSetCount = DatasmithScene->GetLevelVariantSetsCount();

		TArray<TSharedPtr<IDatasmithLevelVariantSetsElement>> LevelVariantSets;
		LevelVariantSets.Reserve(LevelVariantSetCount);


		for (int32 LevelVariantSetIndex = 0; LevelVariantSetIndex < LevelVariantSetCount; ++LevelVariantSetIndex)
		{
			TSharedPtr<IDatasmithLevelVariantSetsElement> LevelVariantSetElement = DatasmithScene->GetLevelVariantSets(LevelVariantSetIndex);
			if (LevelVariantSetElement)
			{
				LevelVariantSets.Add(LevelVariantSetElement);
			}
		}

		VariantSetUtils::TranslateLevelVariantSets(LevelVariantSets, BaseNodeContainer);
	}

	return true;
}

void UInterchangeDatasmithTranslator::HandleDatasmithActor(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithActorElement>& ActorElement, const UInterchangeSceneNode* ParentNode) const
{
	using namespace UE::DatasmithInterchange;
	const FString NodeName = ActorElement->GetName();
	const FString ParentNodeUid = ParentNode->GetUniqueID();
	const FString NodeUid = NodeUtils::GetActorUid(*NodeName);

	UInterchangeSceneNode* InterchangeSceneNode = NewObject<UInterchangeSceneNode>(&BaseNodeContainer);
	InterchangeSceneNode->InitializeNode(NodeUid, ActorElement->GetLabel(), EInterchangeNodeContainerType::TranslatedScene);
	BaseNodeContainer.AddNode(InterchangeSceneNode);
	BaseNodeContainer.SetNodeParentUid(NodeUid, ParentNodeUid);

	const FTransform ActorTransform = ActorElement->GetRelativeTransform();
	InterchangeSceneNode->SetCustomLocalTransform(&BaseNodeContainer, ActorElement->GetRelativeTransform());
	// TODO: Layer association + component actors

	if (ActorElement->IsA(EDatasmithElementType::StaticMeshActor))
	{
		TSharedRef<IDatasmithMeshActorElement> MeshActor = StaticCastSharedRef<IDatasmithMeshActorElement>(ActorElement);
		// TODO: GetStaticMeshPathName() might reference an asset that was not imported.
		const FString MeshUid = NodeUtils::MeshPrefix + MeshActor->GetStaticMeshPathName();
		InterchangeSceneNode->SetCustomAssetInstanceUid(MeshUid);

		TSharedPtr<IDatasmithMaterialIDElement> GlobalMaterialID;
		for (int32 OverrideIndex = 0, OverrideCount = MeshActor->GetMaterialOverridesCount(); OverrideIndex < OverrideCount; ++OverrideIndex)
		{
			if (const TSharedPtr<IDatasmithMaterialIDElement> MaterialID = MeshActor->GetMaterialOverride(OverrideIndex))
			{
				if (MaterialID->GetId() == -1)
				{
					GlobalMaterialID = MaterialID;
					break;
				}
			}
		}

		if (GlobalMaterialID)
		{
			// Set dedicated attribute with value of material Uid.
			// Corresponding factory then mesh actor will be updated accordingly pre then post import in the pipeline
			const FString MaterialUid = NodeUtils::MaterialPrefix + GlobalMaterialID->GetName();
			InterchangeSceneNode->AddStringAttribute(MeshUtils::MeshMaterialAttrName, MaterialUid);
		}
		else
		{
			for (int32 OverrideIndex = 0, OverrideCount = MeshActor->GetMaterialOverridesCount(); OverrideIndex < OverrideCount; ++OverrideIndex)
			{
				if (const TSharedPtr<IDatasmithMaterialIDElement> MaterialID = MeshActor->GetMaterialOverride(OverrideIndex))
				{
					const FString MaterialUid = NodeUtils::MaterialPrefix + FDatasmithUtils::SanitizeObjectName(MaterialID->GetName());
					if (BaseNodeContainer.GetNode(MaterialUid) != nullptr)
					{
						InterchangeSceneNode->SetSlotMaterialDependencyUid(*FString::FromInt(MaterialID->GetId()), MaterialUid);
					}
				}
			}
		}
	}
	else if (ActorElement->IsA(EDatasmithElementType::Camera))
	{
		TSharedRef<IDatasmithCameraActorElement> CameraActor = StaticCastSharedRef<IDatasmithCameraActorElement>(ActorElement);

		// We need to add camera asset node and then instance it in the scene node.
		UInterchangePhysicalCameraNode* CameraNode = AddCameraNode(BaseNodeContainer, CameraActor);
		InterchangeSceneNode->SetCustomAssetInstanceUid(CameraNode->GetUniqueID());
	}
	else if (ActorElement->IsA(EDatasmithElementType::Light))
	{
		TSharedRef<IDatasmithLightActorElement> LightActor = StaticCastSharedRef<IDatasmithLightActorElement>(ActorElement);

		// We need to add light asset node and then instance it in the scene node.
		UInterchangeBaseLightNode* LightNode = AddLightNode(BaseNodeContainer, LightActor);
		InterchangeSceneNode->SetCustomAssetInstanceUid(LightNode->GetUniqueID());
	}

	for (int32 ChildIndex = 0, ChildrenCount = ActorElement->GetChildrenCount(); ChildIndex < ChildrenCount; ++ChildIndex)
	{
		if (const TSharedPtr<IDatasmithActorElement> ChildActorElement = ActorElement->GetChild(ChildIndex))
		{
			HandleDatasmithActor(BaseNodeContainer, ChildActorElement.ToSharedRef(), InterchangeSceneNode);
		}
	}
}

UInterchangePhysicalCameraNode* UInterchangeDatasmithTranslator::AddCameraNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithCameraActorElement>& CameraActor) const
{
	using namespace UE::DatasmithInterchange;

	UInterchangePhysicalCameraNode* CameraNode = NewObject<UInterchangePhysicalCameraNode>(&BaseNodeContainer);
	const FString CameraUid = NodeUtils::CameraPrefix + CameraActor->GetName();
	CameraNode->InitializeNode(CameraUid, CameraActor->GetLabel(), EInterchangeNodeContainerType::TranslatedAsset);
	BaseNodeContainer.AddNode(CameraNode);

	CameraNode->SetCustomFocalLength(CameraActor->GetFocalLength());
	CameraNode->SetCustomSensorWidth(CameraActor->GetSensorWidth());
	const float SensorHeight = CameraActor->GetSensorWidth() / CameraActor->GetSensorAspectRatio();
	CameraNode->SetCustomSensorHeight(SensorHeight);

	// TODO Add properties currently missing from the InterchangeCameraNode:
	//  - DepthOfField
	//  - FocusDistance
	//  - FStop
	//  - FocalLength
	//  - PostProcess
	//  - LookAtActor

	return CameraNode;
}

UInterchangeBaseLightNode* UInterchangeDatasmithTranslator::AddLightNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithLightActorElement>& LightActor) const
{
	using namespace UE::DatasmithInterchange;

	using FDatasmithLightUnits = std::underlying_type_t<EDatasmithLightUnits>;
	using FInterchangeLightUnits = std::underlying_type_t<EInterchangeLightUnits>;
	using FCommonLightUnits = std::common_type_t<FDatasmithLightUnits, FInterchangeLightUnits>;

	static_assert(FCommonLightUnits(EInterchangeLightUnits::Unitless) == FCommonLightUnits(EDatasmithLightUnits::Unitless), "EDatasmithLightUnits::Unitless differs from EInterchangeLightUnits::Unitless");
	static_assert(FCommonLightUnits(EInterchangeLightUnits::Lumens) == FCommonLightUnits(EDatasmithLightUnits::Lumens), "EDatasmithLightUnits::Lumens differs from EInterchangeLightUnits::Lumens");
	static_assert(FCommonLightUnits(EInterchangeLightUnits::Candelas) == FCommonLightUnits(EDatasmithLightUnits::Candelas), "EDatasmithLightUnits::Candelas differs from EInterchangeLightUnits::Candelas");
	static_assert(FCommonLightUnits(EInterchangeLightUnits::EV) == FCommonLightUnits(EDatasmithLightUnits::EV), "EDatasmithLightUnits::EV differs from EInterchangeLightUnits::EV");

	// TODO Add properties currently missing from the UInterchangeLightNode: everything
	UInterchangeBaseLightNode* LightNode = nullptr;
	if (LightActor->IsA(EDatasmithElementType::AreaLight))
	{
		const TSharedRef<IDatasmithAreaLightElement> AreaLightElement = StaticCastSharedRef<IDatasmithAreaLightElement>(LightActor);
		UInterchangeDatasmithAreaLightNode* AreaLightNode = NewObject<UInterchangeDatasmithAreaLightNode>(&BaseNodeContainer);

		const FString LightUid = NodeUtils::LightPrefix + LightActor->GetName();
		AreaLightNode->InitializeNode(LightUid, LightActor->GetLabel(), EInterchangeNodeContainerType::TranslatedAsset);

		AreaLightNode->SetCustomLightType(static_cast<EDatasmithAreaLightActorType>(AreaLightElement->GetLightType()));
		AreaLightNode->SetCustomLightShape(static_cast<EDatasmithAreaLightActorShape>(AreaLightElement->GetLightShape()));
		AreaLightNode->SetCustomDimensions(FVector2D(AreaLightElement->GetLength(), AreaLightElement->GetWidth()));
		AreaLightNode->SetCustomIntensity(AreaLightElement->GetIntensity());
		AreaLightNode->SetCustomIntensityUnits(static_cast<EInterchangeLightUnits>(AreaLightElement->GetIntensityUnits()));
		AreaLightNode->SetCustomColor(AreaLightElement->GetColor());
		if (AreaLightElement->GetUseTemperature())
		{
			AreaLightNode->SetCustomTemperature(AreaLightElement->GetTemperature());
		}

		if (AreaLightElement->GetUseIes())
		{
			//AreaLightNode->SetCustomIESTexture(const TObjectPtr<class UTextureLightProfile>&AttributeValue, bool bAddApplyDelegate = true);
			AreaLightNode->SetCustomUseIESBrightness(AreaLightElement->GetUseIesBrightness());
			AreaLightNode->SetCustomIESBrightnessScale(AreaLightElement->GetIesBrightnessScale());
			AreaLightNode->SetCustomRotation(AreaLightElement->GetIesRotation().Rotator());
		}

		AreaLightNode->SetCustomSourceRadius(AreaLightElement->GetSourceRadius());
		AreaLightNode->SetCustomSourceLength(AreaLightElement->GetSourceLength());
		AreaLightNode->SetCustomAttenuationRadius(AreaLightElement->GetAttenuationRadius());
		AreaLightNode->SetCustomSpotlightInnerAngle(AreaLightElement->GetInnerConeAngle());
		AreaLightNode->SetCustomSpotlightOuterAngle(AreaLightElement->GetOuterConeAngle());

		LightNode = AreaLightNode;
		BaseNodeContainer.AddNode(LightNode);
		return LightNode;
	}
	else if (LightActor->IsA(EDatasmithElementType::SpotLight))
	{
		LightNode = NewObject<UInterchangeSpotLightNode>(&BaseNodeContainer);
	}
	else if (LightActor->IsA(EDatasmithElementType::LightmassPortal))
	{
		// TODO Add lightmass portal support in interchange.
		LightNode = NewObject<UInterchangeRectLightNode>(&BaseNodeContainer);
	}
	else if (LightActor->IsA(EDatasmithElementType::PointLight))
	{
		LightNode = NewObject<UInterchangePointLightNode>(&BaseNodeContainer);
	}
	else if (LightActor->IsA(EDatasmithElementType::DirectionalLight))
	{
		LightNode = NewObject<UInterchangeDirectionalLightNode>(&BaseNodeContainer);
	}
	else
	{
		ensure(false);
		LightNode = NewObject<UInterchangeLightNode>(&BaseNodeContainer);
	}
	const FString LightUid = NodeUtils::LightPrefix + LightActor->GetName();
	LightNode->InitializeNode(LightUid, LightActor->GetLabel(), EInterchangeNodeContainerType::TranslatedAsset);
	BaseNodeContainer.AddNode(LightNode);

	return LightNode;
}

TOptional<UE::Interchange::FImportImage> UInterchangeDatasmithTranslator::GetTexturePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const
{
	if (!LoadedExternalSource || !LoadedExternalSource->GetDatasmithScene())
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	int32 TextureIndex = 0;
	LexFromString(TextureIndex, *PayloadKey);
	TSharedPtr<IDatasmithScene> DatasmithScene = LoadedExternalSource->GetDatasmithScene();
	if (TextureIndex < 0 || TextureIndex >= DatasmithScene->GetTexturesCount())
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	TSharedPtr<IDatasmithTextureElement> TextureElement = DatasmithScene->GetTexture(TextureIndex);
	if (!TextureElement.IsValid())
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	UInterchangeSourceData* PayloadSourceData = UInterchangeManager::GetInterchangeManager().CreateSourceData(TextureElement->GetFile());
	FGCObjectScopeGuard ScopedSourceData(PayloadSourceData);

	if (!PayloadSourceData)
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
	FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
	const IInterchangeTexturePayloadInterface* TextureTranslator = Cast< IInterchangeTexturePayloadInterface >(SourceTranslator);
	if (!ensure(TextureTranslator))
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	SourceTranslator->SetResultsContainer(Results);

	AlternateTexturePath = TextureElement->GetFile();

	return TextureTranslator->GetTexturePayloadData(PayloadKey, AlternateTexturePath);
}

TFuture<TOptional<UE::Interchange::FMeshPayloadData>> UInterchangeDatasmithTranslator::GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const
{
	TPromise<TOptional<UE::Interchange::FMeshPayloadData>> EmptyPromise;
	EmptyPromise.SetValue(TOptional<UE::Interchange::FMeshPayloadData>());

	if (!LoadedExternalSource || !LoadedExternalSource->GetDatasmithScene())
	{
		return EmptyPromise.GetFuture();
	}

	int32 MeshIndex = 0;
	LexFromString(MeshIndex, *PayLoadKey.UniqueId);
	TSharedPtr<IDatasmithScene> DatasmithScene = LoadedExternalSource->GetDatasmithScene();
	if (MeshIndex < 0 || MeshIndex >= DatasmithScene->GetMeshesCount())
	{
		return EmptyPromise.GetFuture();
	}

	TSharedPtr<IDatasmithMeshElement> MeshElement = DatasmithScene->GetMesh(MeshIndex);
	if (!MeshElement.IsValid())
	{
		return EmptyPromise.GetFuture();
	}

	return Async(EAsyncExecution::TaskGraph, [this, MeshElement = MoveTemp(MeshElement), MeshGlobalTransform]
		{
			TOptional<UE::Interchange::FMeshPayloadData> Result;

			FDatasmithMeshElementPayload DatasmithMeshPayload;
			if (LoadedExternalSource->GetAssetTranslator()->LoadStaticMesh(MeshElement.ToSharedRef(), DatasmithMeshPayload))
			{
				if (DatasmithMeshPayload.LodMeshes.Num() > 0)
				{
					UE::Interchange::FMeshPayloadData StaticMeshPayloadData;
					StaticMeshPayloadData.MeshDescription = MoveTemp(DatasmithMeshPayload.LodMeshes[0]);
					// Bake the payload mesh, with the provided transform
					if (!MeshGlobalTransform.Equals(FTransform::Identity))
					{
						FStaticMeshOperations::ApplyTransform(StaticMeshPayloadData.MeshDescription, MeshGlobalTransform);
					}
					Result.Emplace(MoveTemp(StaticMeshPayloadData));
				}
			}

			return Result;
		}
	);
}

TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> UInterchangeDatasmithTranslator::GetAnimationPayloadData(const FInterchangeAnimationPayLoadKey& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const
{
	UE::Interchange::FAnimationPayloadData TransformPayloadData(PayLoadKey.Type);

	TPromise<TOptional<UE::Interchange::FAnimationPayloadData>> EmptyPromise;
	EmptyPromise.SetValue(TOptional<UE::Interchange::FAnimationPayloadData>());
	

	if (!LoadedExternalSource || !LoadedExternalSource->GetDatasmithScene())
	{
		return EmptyPromise.GetFuture();
	}

	TSharedPtr<IDatasmithBaseAnimationElement> AnimationElement;
	float FrameRate = 0.f;
	if (UE::DatasmithInterchange::AnimUtils::FAnimationPayloadDesc* PayloadDescPtr = AnimationPayLoadMapping.Find(PayLoadKey.UniqueId))
	{
		AnimationElement = PayloadDescPtr->Value;
		if (!ensure(AnimationElement))
		{
			// #ueent_logwarning:
			return EmptyPromise.GetFuture();
		}

		FrameRate = PayloadDescPtr->Key;
	}

	switch (PayLoadKey.Type)
	{
	case EInterchangeAnimationPayLoadType::CURVE:
	case EInterchangeAnimationPayLoadType::MORPHTARGETCURVE:
		{
			return Async(EAsyncExecution::TaskGraph, [this, AnimationElement = MoveTemp(AnimationElement), FrameRate, &TransformPayloadData]
				{
					
					TOptional<UE::Interchange::FAnimationPayloadData> Result;

					if (UE::DatasmithInterchange::AnimUtils::GetAnimationPayloadData(*AnimationElement, FrameRate, TransformPayloadData.Curves))
					{
						Result.Emplace(MoveTemp(TransformPayloadData));
					}

					return Result;
					}
				);
		}
		break;
	case EInterchangeAnimationPayLoadType::STEPCURVE:
		{
			return Async(EAsyncExecution::TaskGraph, [this, AnimationElement = MoveTemp(AnimationElement), FrameRate, &TransformPayloadData]
				{
					TOptional<UE::Interchange::FAnimationPayloadData> Result;

					if (UE::DatasmithInterchange::AnimUtils::GetAnimationPayloadData(*AnimationElement, FrameRate, TransformPayloadData.StepCurves))
					{
						Result.Emplace(MoveTemp(TransformPayloadData));
					}

					return Result;
				}
			);
		}
		break;
	case EInterchangeAnimationPayLoadType::BAKED:
	case EInterchangeAnimationPayLoadType::NONE:
	default:
		break;
	}

	return EmptyPromise.GetFuture();
}

TFuture<TOptional<UE::Interchange::FVariantSetPayloadData>> UInterchangeDatasmithTranslator::GetVariantSetPayloadData(const FString& PayloadKey) const
{
	using namespace UE::Interchange;
	using namespace UE::DatasmithInterchange;

	TPromise<TOptional<FVariantSetPayloadData>> EmptyPromise;
	EmptyPromise.SetValue(TOptional<FVariantSetPayloadData>());

	if (!LoadedExternalSource || !LoadedExternalSource->GetDatasmithScene())
	{
		return EmptyPromise.GetFuture();
	}

	TSharedPtr<IDatasmithScene> DatasmithScene = LoadedExternalSource->GetDatasmithScene();
	
	TArray<FString> PayloadTokens;

	// We need two indices to build the payload: index of LevelVariantSet and index of VariantSetIndex
	if (2 != PayloadKey.ParseIntoArray(PayloadTokens, TEXT(";")))
	{
		// Invalid payload
		return EmptyPromise.GetFuture();
	}

	int32 LevelVariantSetIndex = FCString::Atoi(*PayloadTokens[0]);
	int32 VariantSetIndex = FCString::Atoi(*PayloadTokens[1]);

	TSharedPtr<IDatasmithLevelVariantSetsElement> LevelVariantSetElement = DatasmithScene->GetLevelVariantSets(LevelVariantSetIndex);
	if (ensure(LevelVariantSetElement))
	{
		TSharedPtr<IDatasmithVariantSetElement> VariantSet = LevelVariantSetElement->GetVariantSet(VariantSetIndex);
		if (ensure(VariantSet) && VariantSet->GetVariantsCount() > 0)
		{
			return Async(EAsyncExecution::TaskGraph, [this, VariantSet = MoveTemp(VariantSet)]
					{
						FVariantSetPayloadData PayloadData;
						TOptional<FVariantSetPayloadData> Result;

						if (VariantSetUtils::GetVariantSetPayloadData(*VariantSet, PayloadData))
						{
							Result.Emplace(MoveTemp(PayloadData));
						}

						return Result;
					}
				);
		}
	}

	return EmptyPromise.GetFuture();
}

void UInterchangeDatasmithTranslator::ImportFinish()
{
	double ElapsedSeconds = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTime);

	int ElapsedMin = int(ElapsedSeconds / 60.0);
	ElapsedSeconds -= 60.0 * (double)ElapsedMin;

	UE_LOG(LogInterchangeDatasmith, Log, TEXT("Imported %s in [%d min %.3f s]"), *FileName, ElapsedMin, ElapsedSeconds);
}

FString UInterchangeDatasmithTranslator::BuildConfigFilePath(const FString& FilePath)
{
	const FString OptionFileName = FMD5::HashAnsiString(*(FPaths::ConvertRelativePathToFull(FilePath) + TEXT("_Config"))) + TEXT(".ini");
	return  FPaths::Combine(FPlatformProcess::UserTempDir(), OptionFileName);
}


#undef LOCTEXT_NAMESPACE