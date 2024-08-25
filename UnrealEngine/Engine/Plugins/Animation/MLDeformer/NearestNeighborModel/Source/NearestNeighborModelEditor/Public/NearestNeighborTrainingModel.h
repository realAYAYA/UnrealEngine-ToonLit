// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerGeomCacheTrainingModel.h"
#include "NearestNeighborGeomCacheSampler.h"

#include "NearestNeighborTrainingModel.generated.h"

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;
}

class UAnimSequence;
class UMLDeformerModel;
class UNearestNeighborKMeansData;
class UNearestNeighborModel;
class UNearestNeighborStatsData;
class UNearestNeighborModelInstance;

UCLASS(Blueprintable)
class NEARESTNEIGHBORMODELEDITOR_API UNearestNeighborTrainingModel
	: public UMLDeformerGeomCacheTrainingModel
{
	GENERATED_BODY()

public:
	virtual ~UNearestNeighborTrainingModel();
	virtual void Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	int32 Train() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	int32 UpdateNearestNeighborData();

	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	int32 KmeansClusterPoses(const UNearestNeighborKMeansData* KMeansData);

	UFUNCTION(BlueprintImplementableEvent, Category = "Python")
	bool GetNeighborStats(const UNearestNeighborStatsData* StatsData);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Python")
	TArray<float> CustomSamplerBoneRotations;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Python")
	TArray<float> CustomSamplerDeltas;

private:
	UFUNCTION(BlueprintPure, Category = "Python")
	int32 GetNumFramesAnimSequence(const UAnimSequence* Anim) const;

	UFUNCTION(BlueprintPure, Category = "Python")
	int32 GetNumFramesGeometryCache(const UGeometryCache* GeometryCache) const;
	
	UFUNCTION(BlueprintPure, Category = "Python")
	const USkeleton* GetModelSkeleton(const UMLDeformerModel* Model) const;
	
	UFUNCTION(BlueprintPure, Category = "Python")
	UNearestNeighborModel* GetNearestNeighborModel() const;

	UFUNCTION(BlueprintCallable, Category = "Python")
	bool SetCustomSamplerData(UAnimSequence* Anim, UGeometryCache* Cache = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Python")
	bool CustomSample(int32 Frame);

	UFUNCTION(BlueprintCallable, Category = "Python")
	bool SetCustomSamplerDataFromSection(int32 SectionIndex);

	UFUNCTION(BlueprintPure, Category = "Python")
	TArray<float> GetUnskinnedVertexPositions();

	UFUNCTION(BlueprintPure, Category = "Python")
	TArray<int32> GetMeshIndexBuffer();

	UFUNCTION(BlueprintCallable, Category = "Python")
	UNearestNeighborModelInstance* CreateModelInstance();

	UFUNCTION(BlueprintCallable, Category = "Python")
	void DestroyModelInstance(UNearestNeighborModelInstance* ModelInstance);

	void SetNewCustomSampler();
	UNearestNeighborModel* GetCastModel() const;
	UE::NearestNeighborModel::FNearestNeighborEditorModel* GetCastEditorModel() const;

	TUniquePtr<UE::NearestNeighborModel::FNearestNeighborGeomCacheSampler> CustomSampler;
};
