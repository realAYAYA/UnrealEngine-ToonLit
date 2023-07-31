// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerTrainingModel.h"
#include "NearestNeighborTrainingModel.generated.h"

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;
}

class UNearestNeighborModel;

UCLASS(Blueprintable)
class NEARESTNEIGHBORMODELEDITOR_API UNearestNeighborTrainingModel
	: public UMLDeformerTrainingModel
{
	GENERATED_BODY()

public:
	virtual void Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel) override;

	/** Main training function, with implementation in python. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	int32 Train() const;

	UFUNCTION(BlueprintPure, Category = "Python")
	UNearestNeighborModel* GetNearestNeighborModel() const;

	UE::NearestNeighborModel::FNearestNeighborEditorModel* GetNearestNeighborEditorModel() const;

	UFUNCTION(BlueprintPure, Category = "Python")
	const TArray<int32> GetPartVertexMap(const int32 PartId) const;

	UFUNCTION(BlueprintCallable, Category = "Python")
	void SamplePart(int32 PartId, int32 Index);

	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	void UpdateNearestNeighborData();

	UFUNCTION(BlueprintCallable, Category = "Python")
	void SetSamplerPartData(const int32 PartId);

	UFUNCTION(BlueprintPure, Category = "Python")
	int32 GetPartNumNeighbors(const int32 PartId) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	void KmeansClusterPoses() const;

	UFUNCTION(BlueprintCallable, Category = "Python")
	void SampleKmeansAnim(const int32 SkeletonId);

	UFUNCTION(BlueprintCallable, Category = "Python")
	void SampleKmeansFrame(const int32 Frame);

	UFUNCTION(BlueprintPure, Category = "Python")
	int32 GetKmeansNumAnims() const;

	UFUNCTION(BlueprintPure, Category = "Python")
	int32 GetKmeansAnimNumFrames(const int32 SkeletonId) const;

	UFUNCTION(BlueprintPure, Category = "Python")
	int32 GetKmeansNumClusters() const;


public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Data")
	TArray<float> PartSampleDeltas;

	UNearestNeighborModel* NearestNeighborModel = nullptr;
};
