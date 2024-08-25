// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDatasmithTranslator.h"

#include "InterchangeDatasmithAreaLightNode.h"
#include "InterchangeDatasmithLog.h"
#include "InterchangeDatasmithMaterialNode.h"
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

#include "CADOptions.h"
#include "ExternalSourceModule.h"
#include "SourceUri.h"
#include "InterchangeCameraNode.h"
#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeLightNode.h"
#include "InterchangeDecalNode.h"
#include "InterchangeManager.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeMaterialInstanceNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureLightProfileNode.h"
#include "InterchangeTextureLightProfileFactoryNode.h"
#include "InterchangeVariantSetNode.h"
#include "StaticMeshOperations.h"

#include "Misc/App.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "Dialogs/DlgPickPath.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IMainFrameModule.h"
#include "ObjectTools.h"
#include "Styling/SlateIconFinder.h"
#include "UI/DatasmithImportOptionsWindow.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "DatasmithInterchange"

namespace UE::DatasmithInterchange
{
#if WITH_EDITOR
	bool DisplayOptionsDialog(IDatasmithTranslator& Translator)
	{
		TArray<TObjectPtr<UDatasmithOptionsBase>> ImportOptions;
		Translator.GetSceneImportOptions(ImportOptions);

		if (ImportOptions.Num() == 0)
		{
			return true;
		}

		const FString FilePath = Translator.GetSource().GetSourceFile();

		TSharedPtr<SWindow> ParentWindow;

		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		TArray<UObject*> Options;
		Options.SetNum(ImportOptions.Num());
		for (int32 Index = 0; Index < ImportOptions.Num(); ++Index)
		{
			Options[Index] = ImportOptions[Index];
		}

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("DatasmithImportSettingsTitle", "Datasmith Import Options"))
			.SizingRule(ESizingRule::Autosized);

		float SceneVersion = FDatasmithUtils::GetDatasmithFormatVersionAsFloat();
		FString FileSDKVersion = FDatasmithUtils::GetEnterpriseVersionAsString();

		TSharedPtr<SDatasmithOptionsWindow> OptionsWindow;
		Window->SetContent
		(
			SAssignNew(OptionsWindow, SDatasmithOptionsWindow)
			.ImportOptions(Options)
			.WidgetWindow(Window)
			// note: Spacing in text below is intentional for text alignment
			.FileNameText(FText::Format(LOCTEXT("DatasmithImportSettingsFileName", "  Import File  :    {0}"), FText::FromString(FPaths::GetCleanFilename(FilePath))))
			.FilePathText(FText::FromString(FilePath))
			.FileFormatVersion(SceneVersion)
			.FileSDKVersion(FText::FromString(FileSDKVersion))
//			.PackagePathText(FText::Format(LOCTEXT("DatasmithImportSettingsPackagePath", "  Import To   :    {0}"), FText::FromString(PackagePath)))
			.ProceedButtonLabel(LOCTEXT("DatasmithOptionWindow_ImportCurLevel", "Import"))
			.ProceedButtonTooltip(LOCTEXT("DatasmithOptionWindow_ImportCurLevel_ToolTip", "Import the file through Interchange and add to the current Level"))
			.CancelButtonLabel(LOCTEXT("DatasmithOptionWindow_Cancel", "Cancel"))
			.CancelButtonTooltip(LOCTEXT("DatasmithOptionWindow_Cancel_ToolTip", "Cancel importing this file"))
			.MinDetailHeight(320.f)
			.MinDetailWidth(450.f)
		);

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		return OptionsWindow->ShouldImport();
	}
#endif
}

TArray<FString> UInterchangeDatasmithTranslator::GetSupportedFormats() const
{
	const TArray<FString> DatasmithFormats = FDatasmithTranslatorManager::Get().GetSupportedFormats();
	TArray<FString> Formats;
	Formats.Reserve(DatasmithFormats.Num() - 1);

	for (const FString& Format : DatasmithFormats)
	{
		if (Format.Contains(TEXT("gltf")) || Format.Contains(TEXT("glb")) || Format.Contains(TEXT("fbx")))
		{
			continue;
		}

		Formats.Add(Format);
	}

	return Formats;
}

bool UInterchangeDatasmithTranslator::CanImportSourceData(const UInterchangeSourceData* InSourceData) const
{
	using namespace UE::DatasmithImporter;

	const FString FilePath = InSourceData->GetFilename();
	const FString FileExtension = FPaths::GetExtension(FilePath);
	if (FileExtension.Equals(TEXT("gltf"), ESearchCase::IgnoreCase) || FileExtension.Equals(TEXT("glb"), ESearchCase::IgnoreCase) || FileExtension.Equals(TEXT("fbx"), ESearchCase::IgnoreCase))
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
	LoadedExternalSource = IExternalSourceModule::GetOrCreateExternalSource(FileNameUri);

	if (!LoadedExternalSource.IsValid() || !LoadedExternalSource->IsAvailable())
	{
		return false;
	}

	StartTime = FPlatformTime::Cycles64();
	FPaths::NormalizeFilename(FilePath);

	TSharedPtr<IDatasmithScene> DatasmithScene;
	{
		TGuardValue<bool> EnableCADCache(CADLibrary::FImportParameters::bGEnableCADCache, true);

		if (GetSettings())
		{
			CADLibrary::FImportParameters::bGEnableCADCache = true;

			const TSharedPtr<IDatasmithTranslator>& DatasmithTranslator = LoadedExternalSource->GetAssetTranslator();
			if (DatasmithTranslator)
			{
				TArray<TObjectPtr<UDatasmithOptionsBase>> OptionArray;
				OptionArray.Add(CachedSettings->ImportOptions);
				DatasmithTranslator->SetSceneImportOptions(OptionArray);
			}
		}

		// Should it be mutable instead? If Translate is const should we really be doing this?.
		DatasmithScene = LoadedExternalSource->TryLoad();

		if (!DatasmithScene.IsValid())
		{
			return false;
		}
	}

	// Texture Nodes
	{
		FDatasmithUniqueNameProvider TextureNameProvider;

		for (int32 TextureIndex = 0, TextureNum = DatasmithScene->GetTexturesCount(); TextureIndex < TextureNum; ++TextureIndex)
		{
			if (TSharedPtr<IDatasmithTextureElement> TextureElement = DatasmithScene->GetTexture(TextureIndex))
			{
				const bool bIsIesProfile = FPaths::GetExtension(TextureElement->GetFile()).Equals(TEXT("ies"), ESearchCase::IgnoreCase);
				UClass* TextureClass = bIsIesProfile ? UInterchangeTextureLightProfileNode::StaticClass() : UInterchangeTexture2DNode::StaticClass();

				UInterchangeTextureNode* TextureNode = NewObject<UInterchangeTextureNode>(&BaseNodeContainer, TextureClass);

				const FString TextureNodeUid = NodeUtils::TexturePrefix + TextureElement->GetName();
				const FString DisplayLabel = TextureNameProvider.GenerateUniqueName(TextureElement->GetLabel());

				TextureNode->InitializeNode(TextureNodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);

				BaseNodeContainer.AddNode(TextureNode);

				if (bIsIesProfile)
				{
					TextureNode->SetPayLoadKey(TextureElement->GetFile());
				}
				else
				{
					TextureUtils::ApplyTextureElementToNode(TextureElement.ToSharedRef(), TextureNode);
					TextureNode->SetPayLoadKey(LexToString(TextureIndex));
				}
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

	// Log time spent to import incoming file in minutes and seconds
	double ElapsedSeconds = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTime);

	int ElapsedMin = int(ElapsedSeconds / 60.0);
	ElapsedSeconds -= 60.0 * (double)ElapsedMin;
	UE_LOG(LogInterchangeDatasmith, Log, TEXT("Translation of %s in[%d min %.3f s]"), *FileName, ElapsedMin, ElapsedSeconds);

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
	InterchangeSceneNode->SetAssetName(NodeName);
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
	else if (ActorElement->IsA(EDatasmithElementType::Decal))
	{
		TSharedRef<IDatasmithDecalActorElement> DecalActor = StaticCastSharedRef<IDatasmithDecalActorElement>(ActorElement);

		UInterchangeDecalNode* DecalNode= AddDecalNode(BaseNodeContainer, DecalActor);
		InterchangeSceneNode->SetCustomAssetInstanceUid(DecalNode->GetUniqueID());
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

	ProcessIesProfile(BaseNodeContainer, *LightActor, Cast<UInterchangeLightNode>(LightNode));

	const FString LightUid = NodeUtils::LightPrefix + LightActor->GetName();
	LightNode->InitializeNode(LightUid, LightActor->GetLabel(), EInterchangeNodeContainerType::TranslatedAsset);
	BaseNodeContainer.AddNode(LightNode);

	return LightNode;
}

void UInterchangeDatasmithTranslator::ProcessIesProfile(UInterchangeBaseNodeContainer& BaseNodeContainer, const IDatasmithLightActorElement& LightElement, UInterchangeLightNode* LightNode) const
{
	using namespace UE::DatasmithImporter;
	using namespace UE::DatasmithInterchange;

	if (!LightNode || !LightElement.GetUseIes())
	{
		return;
	}

	bool bUpdateLightNode = false;

	FString ProfileNodeUid = NodeUtils::TexturePrefix + LightElement.GetName() + TEXT("_IES");
	const FString DisplayLabel = FString(LightElement.GetName()) + TEXT("_IES");

	if (FPaths::FileExists(LightElement.GetIesTexturePathName()))
	{
		UInterchangeTextureNode* TextureNode = NewObject<UInterchangeTextureLightProfileNode>(&BaseNodeContainer);
		TextureNode->InitializeNode(ProfileNodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedAsset);
		BaseNodeContainer.AddNode(TextureNode);
		bUpdateLightNode = true;
	}
	else if(FSoftObjectPath(LightElement.GetIesTexturePathName()).IsValid())
	{
		FString IESFactoryTextureId = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(ProfileNodeUid);
		UInterchangeTextureLightProfileFactoryNode* FactoryNode = NewObject<UInterchangeTextureLightProfileFactoryNode>(&BaseNodeContainer);
		FactoryNode->InitializeNode(IESFactoryTextureId, DisplayLabel, EInterchangeNodeContainerType::FactoryData);
		FactoryNode->SetCustomReferenceObject(FSoftObjectPath(LightElement.GetIesTexturePathName()));
		BaseNodeContainer.AddNode(FactoryNode);
		bUpdateLightNode = true;
	}
	else
	{
		const FString TextureNodeUid = NodeUtils::TexturePrefix + FDatasmithUtils::SanitizeObjectName(LightElement.GetIesTexturePathName());
		if (BaseNodeContainer.GetNode(TextureNodeUid))
		{
			ProfileNodeUid = TextureNodeUid;
			bUpdateLightNode = true;
		}
	}

	if (bUpdateLightNode)
	{
		LightNode->SetCustomIESTexture(ProfileNodeUid);
		LightNode->SetCustomUseIESBrightness(LightElement.GetUseIesBrightness());
		LightNode->SetCustomIESBrightnessScale(LightElement.GetIesBrightnessScale());
		LightNode->SetCustomRotation(LightElement.GetIesRotation().Rotator());
	}
}

UInterchangeDecalNode* UInterchangeDatasmithTranslator::AddDecalNode(UInterchangeBaseNodeContainer& BaseNodeContainer, const TSharedRef<IDatasmithDecalActorElement>& DecalActor) const
{
	using namespace UE::DatasmithInterchange;

	UInterchangeDecalNode* DecalNode = NewObject<UInterchangeDecalNode>(&BaseNodeContainer);
	const FString DecalUid = NodeUtils::DecalPrefix + DecalActor->GetName();
	DecalNode->InitializeNode(DecalUid, DecalActor->GetLabel(), EInterchangeNodeContainerType::TranslatedAsset);
	BaseNodeContainer.AddNode(DecalNode);

	DecalNode->SetCustomSortOrder(DecalActor->GetSortOrder());
	DecalNode->SetCustomDecalSize(DecalActor->GetDimensions());

	FString DecalMaterialPathName = DecalActor->GetDecalMaterialPathName();
	if(!FPackageName::IsValidObjectPath(DecalActor->GetDecalMaterialPathName()))
	{
		const FString DecalMaterialUid = NodeUtils::DecalMaterialPrefix + DecalActor->GetDecalMaterialPathName();
		if (BaseNodeContainer.IsNodeUidValid(DecalMaterialUid))
		{
			DecalMaterialPathName = DecalMaterialUid;
		}
	}
	DecalNode->SetCustomDecalMaterialPathName(DecalMaterialPathName);

	return DecalNode;
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

	PayloadSourceData->ClearInternalFlags(EInternalObjectFlags::Async);

	UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
	FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
	const IInterchangeTexturePayloadInterface* TextureTranslator = Cast< IInterchangeTexturePayloadInterface >(SourceTranslator);
	if (!ensure(TextureTranslator))
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	SourceTranslator->ClearInternalFlags(EInternalObjectFlags::Async);
	SourceTranslator->SetResultsContainer(Results);

	AlternateTexturePath = TextureElement->GetFile();

	return TextureTranslator->GetTexturePayloadData(PayloadKey, AlternateTexturePath);
}

TOptional<UE::Interchange::FImportLightProfile> UInterchangeDatasmithTranslator::GetLightProfilePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const
{
	if (!LoadedExternalSource || !LoadedExternalSource->GetDatasmithScene())
	{
		return TOptional<UE::Interchange::FImportLightProfile>();
	}

	UInterchangeSourceData* PayloadSourceData = UInterchangeManager::GetInterchangeManager().CreateSourceData(PayloadKey);
	FGCObjectScopeGuard ScopedSourceData(PayloadSourceData);
	if (!PayloadSourceData)
	{
		return TOptional<UE::Interchange::FImportLightProfile>();
	}

	PayloadSourceData->ClearInternalFlags(EInternalObjectFlags::Async);

	UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
	FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
	const IInterchangeTextureLightProfilePayloadInterface* TextureTranslator = Cast< IInterchangeTextureLightProfilePayloadInterface >(SourceTranslator);
	if (!ensure(TextureTranslator))
	{
		return TOptional<UE::Interchange::FImportLightProfile>();
	}

	SourceTranslator->ClearInternalFlags(EInternalObjectFlags::Async);
	SourceTranslator->SetResultsContainer(Results);

	AlternateTexturePath = PayloadKey;

	AlternateTexturePath = PayloadKey;

	return TextureTranslator->GetLightProfilePayloadData(PayloadKey, AlternateTexturePath);
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
					if (!FStaticMeshOperations::ValidateAndFixData(StaticMeshPayloadData.MeshDescription, MeshElement->GetName()))
					{
						UInterchangeResultError_Generic* ErrorResult = AddMessage<UInterchangeResultError_Generic>();
						ErrorResult->SourceAssetName = SourceData ? SourceData->GetFilename() : FString();
						ErrorResult->Text = LOCTEXT("GetMeshPayloadData_ValidateMeshDescriptionFail", "Invalid mesh data (NAN) was found and fix to zero. Mesh render can be bad.");
					}
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

	if (PayLoadKey.Type != EInterchangeAnimationPayLoadType::NONE)
	{
		return Async(EAsyncExecution::TaskGraph, [this, AnimationElement = MoveTemp(AnimationElement), FrameRate, PayLoadType = PayLoadKey.Type]
			{

				UE::Interchange::FAnimationPayloadData TransformPayloadData(PayLoadType);
				TOptional<UE::Interchange::FAnimationPayloadData> Result;

				if (UE::DatasmithInterchange::AnimUtils::GetAnimationPayloadData(*AnimationElement, FrameRate, PayLoadType, TransformPayloadData))
				{
					Result.Emplace(MoveTemp(TransformPayloadData));
				}

				return Result;
			}
		);
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


UInterchangeTranslatorSettings* UInterchangeDatasmithTranslator::GetSettings() const
{
	using namespace UE::DatasmithImporter;
	using namespace UE::DatasmithInterchange;

	if (!CachedSettings)
	{
		if (!LoadedExternalSource.IsValid())
		{
			FString FilePath = FPaths::ConvertRelativePathToFull(SourceData->GetFilename());
			FileName = FPaths::GetCleanFilename(FilePath);
			const FSourceUri FileNameUri = FSourceUri::FromFilePath(FilePath);
			LoadedExternalSource = IExternalSourceModule::GetOrCreateExternalSource(FileNameUri);
		}

		if (!LoadedExternalSource.IsValid() || !LoadedExternalSource->IsAvailable())
		{
			return nullptr;
		}

		const TSharedPtr<IDatasmithTranslator>& DatasmithTranslator = LoadedExternalSource->GetAssetTranslator();
		if (!DatasmithTranslator)
		{
			return nullptr;
		}

		TArray<TObjectPtr<UDatasmithOptionsBase>> OptionArray;
		DatasmithTranslator->GetSceneImportOptions(OptionArray);
		if (OptionArray.Num() == 0)
		{
			return nullptr;
		}

		CachedSettings = DuplicateObject<UInterchangeDatasmithTranslatorSettings>(UInterchangeDatasmithTranslatorSettings::StaticClass()->GetDefaultObject<UInterchangeDatasmithTranslatorSettings>(), GetTransientPackage());
		CachedSettings->SetFlags(RF_Standalone);
		CachedSettings->ClearInternalFlags(EInternalObjectFlags::Async);
		CachedSettings->ImportOptions = OptionArray[0];
	}
	return CachedSettings;
}

void UInterchangeDatasmithTranslator::SetSettings(const UInterchangeTranslatorSettings* InterchangeTranslatorSettings)
{
	if (CachedSettings)
	{
		CachedSettings->ClearFlags(RF_Standalone);
		CachedSettings->ClearInternalFlags(EInternalObjectFlags::Async);
		CachedSettings = nullptr;
	}
	if (InterchangeTranslatorSettings)
	{
		CachedSettings = DuplicateObject<UInterchangeDatasmithTranslatorSettings>(Cast<UInterchangeDatasmithTranslatorSettings>(InterchangeTranslatorSettings), GetTransientPackage());
		CachedSettings->ClearInternalFlags(EInternalObjectFlags::Async);
		CachedSettings->SetFlags(RF_Standalone);
	}
}

#undef LOCTEXT_NAMESPACE