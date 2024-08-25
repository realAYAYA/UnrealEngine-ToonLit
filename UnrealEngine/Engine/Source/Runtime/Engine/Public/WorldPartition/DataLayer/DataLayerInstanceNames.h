// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "DataLayerInstanceNames.generated.h"

USTRUCT()
struct FDataLayerInstanceNames
{
	GENERATED_USTRUCT_BODY()

	FDataLayerInstanceNames()
	: bIsFirstDataLayerExternal(false)
	{
#if WITH_EDITOR
		bIsForcedEmptyNonExternalDataLayers = false;
#endif
	}

	FDataLayerInstanceNames(const TArray<FName>& InDataLayers, bool bInIsFirstDataLayerExternal)
	: bIsFirstDataLayerExternal(bInIsFirstDataLayerExternal)
	, DataLayers(InDataLayers)
	{
#if WITH_EDITOR
		bIsForcedEmptyNonExternalDataLayers = false;
#endif
	}

	FDataLayerInstanceNames(const TArray<FName>& InNonExternalDataLayers, FName InExternalDataLayer)
	{
#if WITH_EDITOR
		bIsForcedEmptyNonExternalDataLayers = false;
#endif
		bIsFirstDataLayerExternal = !InExternalDataLayer.IsNone();
		if (bIsFirstDataLayerExternal)
		{
			DataLayers.Add(InExternalDataLayer);
		}
		DataLayers.Append(InNonExternalDataLayers);
	}

	const FName GetExternalDataLayer() const
	{
		check(!bIsFirstDataLayerExternal || !DataLayers.IsEmpty());
		return bIsFirstDataLayerExternal ? DataLayers[0] : NAME_None;
	}

	/**
	* DO NOT USE DIRECTLY
	* STL-like iterators to enable range-based for loop support.
	*/
	TArray<FName>::RangedForIteratorType     		 begin() { return DataLayers.begin(); }
	TArray<FName>::RangedForConstIteratorType		 begin() const { return DataLayers.begin(); }
	TArray<FName>::RangedForIteratorType      		 end() { return DataLayers.end(); }
	TArray<FName>::RangedForConstIteratorType		 end() const { return DataLayers.end(); }
	TArray<FName>::RangedForReverseIteratorType      rbegin() { return DataLayers.rbegin(); }
	TArray<FName>::RangedForConstReverseIteratorType rbegin() const { return DataLayers.rbegin(); }
	TArray<FName>::RangedForReverseIteratorType      rend() { return DataLayers.rend(); }
	TArray<FName>::RangedForConstReverseIteratorType rend() const { return DataLayers.rend(); }

	TArrayView<const FName> GetNonExternalDataLayers() const
	{
		static TArray<FName> EmptyArray;
		check(!bIsFirstDataLayerExternal || !DataLayers.IsEmpty());
		const int32 Offset = bIsFirstDataLayerExternal ? 1 : 0;
		const int32 NonExternalDataLayersCount = DataLayers.Num() - Offset;
		return NonExternalDataLayersCount > 0 ? MakeArrayView(&DataLayers[Offset], NonExternalDataLayersCount) : EmptyArray;
	}

	int32 Num() const { return DataLayers.Num(); }
	bool IsEmpty() const { return DataLayers.IsEmpty(); }
	bool Contains(FName InDataLayer) const { return DataLayers.Contains(InDataLayer); }
	bool HasExternalDataLayer() const { return bIsFirstDataLayerExternal; }
	const TArray<FName>& ToArray() const { return DataLayers; }

private:
#if WITH_EDITOR
	bool IsForcedEmptyNonExternalDataLayers() const { return bIsForcedEmptyNonExternalDataLayers; }
	void ForceEmptyNonExternalDataLayers()
	{
		bIsForcedEmptyNonExternalDataLayers = true;
		if (bIsFirstDataLayerExternal)
		{
			DataLayers = { DataLayers[0] };
		}
		else
		{
			DataLayers.Empty();
		}
	}
	bool bIsForcedEmptyNonExternalDataLayers;
#endif

	UPROPERTY()
	bool bIsFirstDataLayerExternal;

	UPROPERTY()
	TArray<FName> DataLayers;

	friend class FStreamingGenerationActorDescView;
};