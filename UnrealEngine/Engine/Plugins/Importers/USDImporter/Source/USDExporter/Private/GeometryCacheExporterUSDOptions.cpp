// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheExporterUSDOptions.h"

#include "AnalyticsEventAttribute.h"

void UsdUtils::AddAnalyticsAttributes(const UGeometryCacheExporterUSDOptions& Options, TArray<FAnalyticsEventAttribute>& InOutAttributes)
{
	UsdUtils::AddAnalyticsAttributes(Options.StageOptions, InOutAttributes);
	UsdUtils::AddAnalyticsAttributes(Options.MeshAssetOptions, InOutAttributes);
	InOutAttributes.Emplace(TEXT("ReExportIdenticalAssets"), Options.bReExportIdenticalAssets);
}

void UsdUtils::HashForGeometryCacheExport(const UGeometryCacheExporterUSDOptions& Options, FSHA1& HashToUpdate)
{
	UsdUtils::HashForExport(Options.StageOptions, HashToUpdate);
	UsdUtils::HashForMeshExport(Options.MeshAssetOptions, HashToUpdate);
}
