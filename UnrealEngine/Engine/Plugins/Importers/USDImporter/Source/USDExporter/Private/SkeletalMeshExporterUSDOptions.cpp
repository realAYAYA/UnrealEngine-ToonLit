// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshExporterUSDOptions.h"

#include "AnalyticsEventAttribute.h"

void UsdUtils::AddAnalyticsAttributes(
	const USkeletalMeshExporterUSDOptions& Options,
	TArray< FAnalyticsEventAttribute >& InOutAttributes
)
{
	UsdUtils::AddAnalyticsAttributes( Options.StageOptions, InOutAttributes );
	UsdUtils::AddAnalyticsAttributes( Options.MeshAssetOptions, InOutAttributes );
	InOutAttributes.Emplace( TEXT( "ReExportIdenticalAssets" ), Options.bReExportIdenticalAssets );
}

void UsdUtils::HashForSkeletalMeshExport( const USkeletalMeshExporterUSDOptions& Options, FSHA1& HashToUpdate )
{
	UsdUtils::HashForExport( Options.StageOptions, HashToUpdate );
	UsdUtils::HashForMeshExport( Options.MeshAssetOptions, HashToUpdate );
}
