// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PoseSearch/PoseSearch.h"

#include "PoseSearchDatabaseEditorReflection.generated.h"


namespace UE::PoseSearch
{
	class FDatabaseAssetTreeNode;
	class SDatabaseAssetTree;
}


UCLASS()
class UPoseSearchDatabaseReflectionBase : public UObject
{
	GENERATED_BODY()

public:
	void SetSourceLink(
		const TWeakPtr<UE::PoseSearch::FDatabaseAssetTreeNode>& InWeakAssetTreeNode,
		const TSharedPtr<UE::PoseSearch::SDatabaseAssetTree>& InAssetTreeWidget);

protected:
	TWeakPtr<UE::PoseSearch::FDatabaseAssetTreeNode> WeakAssetTreeNode;
	TSharedPtr<UE::PoseSearch::SDatabaseAssetTree> AssetTreeWidget;
};

UCLASS()
class UPoseSearchDatabaseSequenceReflection : public UPoseSearchDatabaseReflectionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Selected Sequence")
	FPoseSearchDatabaseSequence Sequence;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

UCLASS()
class UPoseSearchDatabaseBlendSpaceReflection : public UPoseSearchDatabaseReflectionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Selected Blend Space")
	FPoseSearchDatabaseBlendSpace BlendSpace;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

USTRUCT()
struct FPoseSearchDatabaseMemoryStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FText EstimatedDatabaseSize;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FText ValuesSize;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FText PCAValuesSize;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FText KDTreeSize;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FText PoseMetadataSize;
	
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FText AssetsSize;
	
	void Initialize(const UPoseSearchDatabase* PoseSearchDatabase);
	static FText ToMemoryBudgetText(int32 Size);
};

UCLASS()
class UPoseSearchDatabaseReflection : public UPoseSearchDatabaseReflectionBase
{
	GENERATED_BODY()

public:
	void Initialize(const UPoseSearchDatabase* PoseSearchDatabase);

	UPROPERTY(VisibleAnywhere, Category = "Selected Database Search Index")
	FPoseSearchDatabaseMemoryStats MemoryStats;

	UPROPERTY(VisibleAnywhere, Category = "Selected Database Search Index")
	FPoseSearchIndex SearchIndex;
};
