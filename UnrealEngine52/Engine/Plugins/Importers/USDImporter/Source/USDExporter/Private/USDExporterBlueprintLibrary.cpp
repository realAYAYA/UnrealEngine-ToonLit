// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDExporterBlueprintLibrary.h"

#include "USDConversionBlueprintLibrary.h"

AInstancedFoliageActor* UUsdExporterBlueprintLibrary::GetInstancedFoliageActorForLevel( bool bCreateIfNone /*= false */, ULevel* Level /*= nullptr */ )
{
	return UUsdConversionBlueprintLibrary::GetInstancedFoliageActorForLevel( bCreateIfNone, Level );
}

TArray<UFoliageType*> UUsdExporterBlueprintLibrary::GetUsedFoliageTypes( AInstancedFoliageActor* Actor )
{
	return UUsdConversionBlueprintLibrary::GetUsedFoliageTypes( Actor );
}

UObject* UUsdExporterBlueprintLibrary::GetSource( UFoliageType* FoliageType )
{
	return UUsdConversionBlueprintLibrary::GetSource( FoliageType );
}

TArray<FTransform> UUsdExporterBlueprintLibrary::GetInstanceTransforms( AInstancedFoliageActor* Actor, UFoliageType* FoliageType, ULevel* InstancesLevel )
{
	return UUsdConversionBlueprintLibrary::GetInstanceTransforms( Actor, FoliageType, InstancesLevel );
}

void UUsdExporterBlueprintLibrary::SendAnalytics( const TArray<FAnalyticsEventAttr>& Attrs, const FString& EventName, bool bAutomated, double ElapsedSeconds, double NumberOfFrames, const FString& Extension )
{
	return UUsdConversionBlueprintLibrary::SendAnalytics( Attrs, EventName, bAutomated, ElapsedSeconds, NumberOfFrames, Extension );
}

