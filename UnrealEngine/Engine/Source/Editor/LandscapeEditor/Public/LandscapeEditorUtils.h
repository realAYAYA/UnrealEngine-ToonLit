// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FileHelpers.h"

class ALandscapeProxy;
class ALandscapeStreamingProxy;
class ULandscapeLayerInfoObject;

namespace LandscapeEditorUtils
{
	bool LANDSCAPEEDITOR_API SetHeightmapData(ALandscapeProxy* Landscape, const TArray<uint16>& Data);
	bool LANDSCAPEEDITOR_API SetWeightmapData(ALandscapeProxy* Landscape, ULandscapeLayerInfoObject* LayerObject, const TArray<uint8>& Data);

	int32 GetMaxSizeInComponents();
	TOptional<FString> GetImportExportFilename(const FString& InDialogTitle, const FString& InStartPath, const FString& InDialogTypeString, bool bInImporting);

	template<typename T>
	void SaveObjects(TArrayView<T*> InObjects)
	{
		TArray<UPackage*> Packages;
		Algo::Transform(InObjects, Packages, [](UObject* InObject) { return InObject->GetPackage(); });
		UEditorLoadingAndSavingUtils::SavePackages(Packages, /* bOnlyDirty = */ false);
	}

	void SaveLandscapeProxies(TArrayView<ALandscapeProxy*> Proxies);
}
