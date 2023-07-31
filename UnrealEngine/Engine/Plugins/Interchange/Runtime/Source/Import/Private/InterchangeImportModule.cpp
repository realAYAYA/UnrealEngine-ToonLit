// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeImportModule.h"

#include "Animation/InterchangeAnimSequenceFactory.h"
#include "Animation/InterchangeAnimationTrackSetFactory.h"
#include "CoreMinimal.h"
#include "Fbx/InterchangeFbxTranslator.h"
#include "Gltf/InterchangeGltfTranslator.h"
#include "InterchangeImportLog.h"
#include "InterchangeManager.h"
#include "Material/InterchangeMaterialFactory.h"
#include "MaterialX/InterchangeMaterialXTranslator.h"
#include "Mesh/InterchangeOBJTranslator.h"
#include "Mesh/InterchangePhysicsAssetFactory.h"
#include "Mesh/InterchangeSkeletalMeshFactory.h"
#include "Mesh/InterchangeSkeletonFactory.h"
#include "Mesh/InterchangeStaticMeshFactory.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Scene/InterchangeActorFactory.h"
#include "Scene/InterchangeCineCameraActorFactory.h"
#include "Scene/InterchangeLightActorFactory.h"
#include "Scene/InterchangeSceneVariantSetsFactory.h"
#include "Scene/InterchangeStaticMeshActorFactory.h"
#include "Scene/InterchangeSkeletalMeshActorFactory.h"
#include "Texture/InterchangeImageWrapperTranslator.h"
#include "Texture/InterchangeDDSTranslator.h"
#include "Texture/InterchangeIESTranslator.h"
#include "Texture/InterchangeJPGTranslator.h"
#include "Texture/InterchangePCXTranslator.h"
#include "Texture/InterchangePSDTranslator.h"
#include "Texture/InterchangeTextureFactory.h"

DEFINE_LOG_CATEGORY(LogInterchangeImport);

class FInterchangeImportModule : public IInterchangeImportModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FInterchangeImportModule, InterchangeImport)



void FInterchangeImportModule::StartupModule()
{
	FInterchangeImportMaterialAsyncHelper& InterchangeMaterialAsyncHelper = FInterchangeImportMaterialAsyncHelper::GetInstance();

	auto RegisterItems = []()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

		//Register the translators
		//Scenes
		InterchangeManager.RegisterTranslator(UInterchangeFbxTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangeGltfTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangeOBJTranslator::StaticClass());

		//Materials
		InterchangeManager.RegisterTranslator(UInterchangeMaterialXTranslator::StaticClass());

		//Textures
		InterchangeManager.RegisterTranslator(UInterchangeImageWrapperTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangeDDSTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangeJPGTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangePCXTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangePSDTranslator::StaticClass());
		InterchangeManager.RegisterTranslator(UInterchangeIESTranslator::StaticClass());

		//Register the factories
		InterchangeManager.RegisterFactory(UInterchangeTextureFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeMaterialFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeMaterialFunctionFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeSkeletonFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeSkeletalMeshFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeStaticMeshFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangePhysicsAssetFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeActorFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeAnimationTrackSetFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeAnimSequenceFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeCineCameraActorFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeStaticMeshActorFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeSkeletalMeshActorFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeSceneVariantSetsFactory::StaticClass());
		InterchangeManager.RegisterFactory(UInterchangeLightActorFactory::StaticClass());
	};
	
	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}
}


void FInterchangeImportModule::ShutdownModule()
{
	FInterchangeImportMaterialAsyncHelper& InterchangeMaterialAsyncHelper = FInterchangeImportMaterialAsyncHelper::GetInstance();
	InterchangeMaterialAsyncHelper.CleanUp();
}



