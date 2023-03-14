// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_NiagaraSimCache.h"

#include "NiagaraEditorStyle.h"
#include "NiagaraSimCacheToolkit.h"

FColor FAssetTypeActions_NiagaraSimCache::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.SimCache").ToFColor(true);
}

UClass* FAssetTypeActions_NiagaraSimCache::GetSupportedClass() const
{
	return UNiagaraSimCache::StaticClass();
}

void FAssetTypeActions_NiagaraSimCache::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		const auto SimCache = Cast<UNiagaraSimCache>(*ObjIt);
		if (SimCache != nullptr)
		{
			const TSharedRef< FNiagaraSimCacheToolkit > NewNiagaraSimCacheToolkit(new FNiagaraSimCacheToolkit());
			NewNiagaraSimCacheToolkit->Initialize(Mode, EditWithinLevelEditor, SimCache);
		}
	}
}
