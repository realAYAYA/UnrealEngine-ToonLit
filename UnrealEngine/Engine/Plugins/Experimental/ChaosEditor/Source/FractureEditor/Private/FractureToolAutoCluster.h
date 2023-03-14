// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FractureTool.h"
#include "Algo/Count.h"

#include "FractureToolAutoCluster.generated.h"

// Note: Only Voronoi-based auto-clustering is currently supported
UENUM()
enum class EFractureAutoClusterMode : uint8
{
	/** Overlapping bounding box*/
	BoundingBox UMETA(DisplayName = "Bounding Box"),

	/** GC connectivity */
	Proximity UMETA(DisplayName = "Proximity"),

	/** Distance */
	Distance UMETA(DisplayName = "Distance"),

	Voronoi UMETA(DisplayName = "Voronoi"),
};

UENUM()
enum class EClusterSizeMethod : uint8
{
	// Cluster by specifying an absolute number of clusters
	ByNumber,
	// Cluster by specifying a fraction of the number of input bones
	ByFractionOfInput
};


UCLASS(DisplayName = "Auto Cluster", Category = "FractureTools")
class UFractureAutoClusterSettings : public UFractureToolSettings
{
public:
	GENERATED_BODY()

	UFractureAutoClusterSettings(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
		, SiteCount(10)
	{}

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Simplified interface now only supports Voronoi clustering."))
	EFractureAutoClusterMode AutoClusterMode_DEPRECATED;

	/** How to choose the size of the clusters to create */
	UPROPERTY(EditAnywhere, Category = ClusterSize)
	EClusterSizeMethod ClusterSizeMethod = EClusterSizeMethod::ByNumber;

	/** Use a Voronoi diagram with this many Voronoi sites as a guide for deciding cluster boundaries */
	UPROPERTY(EditAnywhere, Category = ClusterSize, meta = (DisplayName = "Cluster Sites", UIMin = "2", UIMax = "5000", ClampMin = "1", EditCondition = "ClusterSizeMethod == EClusterSizeMethod::ByNumber"))
	uint32 SiteCount=10;

	/** Choose the number of Voronoi sites used for clustering as a fraction of the number of child bones to process */
	UPROPERTY(EditAnywhere, Category = ClusterSize, meta = (DisplayName = "Cluster Fraction", ClampMin = "0", ClampMax = ".5", EditCondition = "ClusterSizeMethod == EClusterSizeMethod::ByFractionOfInput"))
	float SiteCountFraction = .25;

	/** If true, bones will only be added to the same cluster if they are physically connected (either directly, or via other bones in the same cluster) */
	UPROPERTY(EditAnywhere, Category = AutoCluster, meta = (DisplayName = "Enforce Cluster Connectivity"))
	bool bEnforceConnectivity=true;

	/** If true, prevent the creation of clusters with only a single child. Either by merging into a neighboring cluster, or not creating the cluster. */
	UPROPERTY(EditAnywhere, Category = AutoCluster)
	bool bAvoidIsolated = true;
};


UCLASS(DisplayName="AutoCluster", Category="FractureTools")
class UFractureToolAutoCluster: public UFractureModalTool
{
public:
	GENERATED_BODY()

	UFractureToolAutoCluster(const FObjectInitializer& ObjInit);

	// UFractureTool Interface
	virtual FText GetDisplayText() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetApplyText() const override; 
	virtual FSlateIcon GetToolIcon() const override;
	virtual TArray<UObject*> GetSettingsObjects() const override;
	virtual void RegisterUICommand( FFractureEditorCommands* BindingContext );

	virtual void Execute(TWeakPtr<FFractureEditorModeToolkit> InToolkit) override;

	UPROPERTY(EditAnywhere, Category = AutoCluster)
	TObjectPtr<UFractureAutoClusterSettings> AutoClusterSettings;
};


class FVoronoiPartitioner
{
public:
	FVoronoiPartitioner(const FGeometryCollection* GeometryCollection, int32 ClusterIndex);

	/** Cluster bodies into k partitions using K-Means. Connectivity is ignored: only spatial proximity is considered. */
	void KMeansPartition(int32 InPartitionCount);

	/** Split any partition islands into their own partition. This will possbily increase number of partitions to exceed desired count. */
	void SplitDisconnectedPartitions(FGeometryCollection* GeometryCollection);

	/** Merge any partitions w/ only 1 body into a connected, neighboring partition (if any).  This can decrease the number of partitions below the desired count. */
	void MergeSingleElementPartitions(FGeometryCollection* GeometryCollection);

	int32 GetPartitionCount() const { return PartitionCount; }

	int32 GetNonEmptyPartitionCount() const
	{
		return PartitionSize.Num() - Algo::Count(PartitionSize, 0);
	}

	/** return the GeometryCollection TranformIndices within the partition. */
	TArray<int32> GetPartition(int32 PartitionIndex) const;

private:
	void GenerateConnectivity(const FGeometryCollection* GeometryCollection);
	void CollectConnections(const FGeometryCollection* GeometryCollection, int32 Index, int32 OperatingLevel, TSet<int32>& OutConnections) const;
	void GenerateCentroids(const FGeometryCollection* GeometryCollection);
	FVector GenerateCentroid(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const;
	FBox GenerateBounds(const FGeometryCollection* GeometryCollection, int32 TransformIndex) const;
	void InitializePartitions();
	bool Refine();
	int32 FindClosestPartitionCenter(const FVector& Location) const;
	void MarkVisited(int32 Index, int32 PartitionIndex);

private:
	TArray<int32> TransformIndices;
	TArray<FVector> Centroids;
	TArray<int32> Partitions;
	int32 PartitionCount;
	TArray<int32> PartitionSize;
	TArray<FVector> PartitionCenters;
	TArray<TSet<int32>> Connectivity;
	TArray<bool> Visited;


	// Not generally necessary but this is a safety measure to prevent oscillating solves that never converge.
	const int32 MaxKMeansIterations = 500;
};