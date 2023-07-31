// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticMeshExporterUSDOptions.h"

#include "AnalyticsEventAttribute.h"

void UsdUtils::AddAnalyticsAttributes(
	const UStaticMeshExporterUSDOptions& Options,
	TArray< FAnalyticsEventAttribute >& InOutAttributes
)
{
	UsdUtils::AddAnalyticsAttributes( Options.StageOptions, InOutAttributes );
	UsdUtils::AddAnalyticsAttributes( Options.MeshAssetOptions, InOutAttributes );
	InOutAttributes.Emplace( TEXT( "ReExportIdenticalAssets" ), Options.bReExportIdenticalAssets );
}

void UsdUtils::HashForStaticMeshExport( const UStaticMeshExporterUSDOptions& Options, FSHA1& HashToUpdate )
{
	UsdUtils::HashForExport( Options.StageOptions, HashToUpdate );
	UsdUtils::HashForMeshExport( Options.MeshAssetOptions, HashToUpdate );
}
