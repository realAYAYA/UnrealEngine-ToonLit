// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayersID.h"

#include "WorldPartition/DataLayer/DataLayerInstance.h"

#include "Algo/Transform.h"

#if WITH_EDITOR
FDataLayersID::FDataLayersID()
	: Hash(0)
{}

FDataLayersID::FDataLayersID(const TArray<const UDataLayerInstance*>& InDataLayerInstances)
	: Hash(0)
{
	TArray<FName> DataLayers;
	Algo::TransformIf(InDataLayerInstances, DataLayers, [](const UDataLayerInstance* Item) { return Item->IsRuntime(); }, [](const UDataLayerInstance* Item) { return Item->GetDataLayerFName(); });

	if (DataLayers.Num())
	{
		TArray<FName> SortedDataLayers = DataLayers;
		SortedDataLayers.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });

		for (FName LayerName : SortedDataLayers)
		{
			Hash = FCrc::StrCrc32(*LayerName.ToString(), Hash);
		}

		check(Hash);
	}
}
#endif
