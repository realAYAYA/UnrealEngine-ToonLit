// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheEdModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_GeometryCache.h"
#include "ComponentAssetBroker.h"
#include "GeometryCache.h"
#include "GeometryCacheAssetBroker.h"
#include "GeometryCacheComponent.h"
#include "GeometryCacheModule.h"
#include "GeometryCacheThumbnailRenderer.h"
#include "NiagaraEditorModule.h"
#include "NiagaraGeometryCacheRendererProperties.h"
#include "ThumbnailRendering/ThumbnailManager.h"

IMPLEMENT_MODULE(FGeometryCacheEdModule, GeometryCacheEd)

void FGeometryCacheEdModule::StartupModule()
{
	LLM_SCOPE_BYTAG(GeometryCache);

	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();

	IAssetTools& AssetTools = AssetToolsModule.Get();
	AssetAction = new FAssetTypeActions_GeometryCache();
	AssetTools.RegisterAssetTypeActions(MakeShareable(AssetAction));

	AssetBroker = new FGeometryCacheAssetBroker();
	FComponentAssetBrokerage::RegisterBroker(MakeShareable(AssetBroker), UGeometryCacheComponent::StaticClass(), true, true);

	UThumbnailManager::Get().RegisterCustomRenderer(UGeometryCache::StaticClass(), UGeometryCacheThumbnailRenderer::StaticClass());

	FNiagaraEditorModule& NiagaraEditorModule = FNiagaraEditorModule::Get();
	NiagaraEditorModule.RegisterRendererCreationInfo(FNiagaraRendererCreationInfo(
		UNiagaraGeometryCacheRendererProperties::StaticClass()->GetDisplayNameText(),
		FText::FromString(UNiagaraGeometryCacheRendererProperties::StaticClass()->GetDescription()),
		UNiagaraGeometryCacheRendererProperties::StaticClass()->GetClassPathName(),
		FNiagaraRendererCreationInfo::FRendererFactory::CreateLambda([](UObject* OuterEmitter)
		{
			UNiagaraGeometryCacheRendererProperties* NewRenderer = NewObject<UNiagaraGeometryCacheRendererProperties>(OuterEmitter, NAME_None, RF_Transactional);
			if(ensure(NewRenderer->GeometryCaches.Num() == 1))
			{
				FSoftObjectPath DefaultGeometryCache(TEXT("GeometryCache'/Niagara/DefaultAssets/DefaultGeometryCacheAsset.DefaultGeometryCacheAsset'"));
				NewRenderer->GeometryCaches[0].GeometryCache = Cast<UGeometryCache>(DefaultGeometryCache.TryLoad());
			}
			return NewRenderer;
		})));
}

void FGeometryCacheEdModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();
		AssetTools.UnregisterAssetTypeActions(AssetAction->AsShared());
		FComponentAssetBrokerage::UnregisterBroker(MakeShareable(AssetBroker));
		UThumbnailManager::Get().UnregisterCustomRenderer(UGeometryCache::StaticClass());
	}
}
