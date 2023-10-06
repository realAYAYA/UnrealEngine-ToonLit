// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraSimCache.h"

#include "NiagaraEditorStyle.h"
#include "Toolkits/NiagaraSimCacheToolkit.h"

FLinearColor UAssetDefinition_NiagaraSimCache::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.SimCache").ToFColor(true);
}

EAssetCommandResult UAssetDefinition_NiagaraSimCache::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UNiagaraSimCache* SimCache : OpenArgs.LoadObjects<UNiagaraSimCache>())
	{
		const TSharedRef< FNiagaraSimCacheToolkit > NewNiagaraSimCacheToolkit(new FNiagaraSimCacheToolkit());
		NewNiagaraSimCacheToolkit->Initialize(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, SimCache);
	}

	return EAssetCommandResult::Handled;
}
