// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceCluster.h"

TArray<TArray<FGuid>> GenerateObjectsClusters(TArray<TPair<FGuid, TArray<FGuid>>> InObjects)
{
	TMap<FGuid, FGuid> ObjectToClusters;
	TMap<FGuid, TSet<FGuid>> Clusters;

	for (const auto& Object : InObjects)
	{
		const FGuid& ObjectGuid = Object.Key;
		FGuid ClusterGuid = ObjectToClusters.FindRef(ObjectGuid);

		if (!ClusterGuid.IsValid())
		{
			ClusterGuid = FGuid::NewGuid();
			TSet<FGuid>& Cluster = Clusters.Add(ClusterGuid);
			ObjectToClusters.Add(ObjectGuid, ClusterGuid);
			Cluster.Add(ObjectGuid);
		}

		for (const FGuid& ReferenceGuid : Object.Value)
		{
			FGuid ReferenceClusterGuid = ObjectToClusters.FindRef(ReferenceGuid);

			if (ReferenceClusterGuid.IsValid())
			{
				if (ReferenceClusterGuid != ClusterGuid)
				{
					TSet<FGuid> ReferenceCluster = Clusters.FindAndRemoveChecked(ReferenceClusterGuid);
					TSet<FGuid>& Cluster = Clusters.FindChecked(ClusterGuid);
					Cluster.Append(ReferenceCluster);
					for (const FGuid& OtherReferenceGuid : ReferenceCluster)
					{
						ObjectToClusters.FindChecked(OtherReferenceGuid) = ClusterGuid;
					}
				}
			}
			else
			{
				TSet<FGuid>& Cluster = Clusters.FindChecked(ClusterGuid);
				Cluster.Add(ReferenceGuid);
			}

			ObjectToClusters.Add(ReferenceGuid, ClusterGuid);
		}
	}
	
	TArray<TArray<FGuid>> Result;
	Result.Reserve(Clusters.Num());

	for (const auto& Cluster : Clusters)
	{
		Result.AddDefaulted_GetRef() = Cluster.Value.Array();
	}

	return MoveTemp(Result);
}