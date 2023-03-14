// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialExporterUSDOptions.h"

#include "AnalyticsEventAttribute.h"

void UsdUtils::AddAnalyticsAttributes(
	const UMaterialExporterUSDOptions& Options,
	TArray< FAnalyticsEventAttribute >& InOutAttributes
)
{
	UsdUtils::AddAnalyticsAttributes( Options.MaterialBakingOptions, InOutAttributes );
	InOutAttributes.Emplace( TEXT( "ReExportIdenticalAssets" ), Options.bReExportIdenticalAssets );
}

void UsdUtils::HashForMaterialExport( const UMaterialExporterUSDOptions& Options, FSHA1& HashToUpdate )
{
	UsdUtils::HashForMaterialExport( Options.MaterialBakingOptions, HashToUpdate );
}
