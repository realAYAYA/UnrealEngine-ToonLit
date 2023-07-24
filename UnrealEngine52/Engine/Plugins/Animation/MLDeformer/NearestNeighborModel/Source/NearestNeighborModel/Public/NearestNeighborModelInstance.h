// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MLDeformerMorphModelInstance.h"
#include "NeuralTensor.h"
#include "NearestNeighborModelInstance.generated.h"

class UNearestNeighborModelInputInfo;
class UNearestNeighborModelInstance;
class UNearestNeighborOptimizedNetworkInstance;
namespace UE::NearestNeighborModel
{
    class FNearestNeighborEditorModel;
    class FNearestNeighborEditorModelActor;
};

UCLASS()
class NEARESTNEIGHBORMODEL_API UNearestNeighborModelInstance
    : public UMLDeformerMorphModelInstance
{
    GENERATED_BODY()

public:
	// UMLDeformerModelInstance overrides
    virtual void Init(USkeletalMeshComponent* SkelMeshComponent) override;
    virtual void Execute(float ModelWeight) override;
    virtual bool SetupInputs() override;
    virtual FString CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool LogIssues=false) override;
    virtual void Tick(float DeltaTime, float ModelWeight) override;
    // ~END UMLDeformerModelInstance overrides

    friend class UNearestNeighborModelInputInfo;
    friend class UNearestNeighborModelInstance;
    friend class UNearestNeighborOptimizedNetworkInstance;
    friend class UE::NearestNeighborModel::FNearestNeighborEditorModel;
    friend class UE::NearestNeighborModel::FNearestNeighborEditorModelActor;

private:
#if WITH_EDITORONLY_DATA
    const TArray<uint32>& GetNearestNeighborIds() const { return NearestNeighborIds; }
    uint32 NearestNeighborId(int32 PartId) const { return NearestNeighborIds[PartId]; }
    int32 NeighborIdNum() const { return NearestNeighborIds.Num(); }
#endif

    void InitPreviousWeights();
    void InitOptimizedNetworkInstance();
    void GetInputDataPointer(float*& OutInputData, int32& OutNumInputFloats) const;
    void GetOutputDataPointer(float*& OutOutputData, int32& OutNumOutputFloats) const;

    virtual int64 SetBoneTransforms(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex) override;

    void RunNearestNeighborModel(float DeltaTime, float ModelWeight);
    int32 FindNearestNeighbor(const float* PCAData, int32 PartId);
    void UpdateWeightWithDecay(TArray<float>& MorphWeights, int32 Index, float W, float DecayCoeff);

#if WITH_EDITORONLY_DATA
    TArray<uint32> NearestNeighborIds;
#endif

    TArray<float> PreviousWeights;

    UPROPERTY(Transient)
    TObjectPtr<UNearestNeighborOptimizedNetworkInstance> OptimizedNetworkInstance = nullptr;
};
