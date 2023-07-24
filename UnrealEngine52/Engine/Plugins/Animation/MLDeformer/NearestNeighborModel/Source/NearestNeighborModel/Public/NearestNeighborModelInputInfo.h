// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerInputInfo.h"
#include "NearestNeighborModelInputInfo.generated.h"

class UNearestNeighborModelInstance;
class UNearestNeighborModel;
namespace UE::NearestNeighborModel
{
    class FNearestNeighborEditorModel;
};

UCLASS()
class NEARESTNEIGHBORMODEL_API UNearestNeighborModelInputInfo
    : public UMLDeformerInputInfo
{
    GENERATED_BODY()

public:
    // UMLDeformerInputInfo overrides
    virtual int32 CalcNumNeuralNetInputs() const override;
    virtual void ExtractBoneRotations(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutRotations) const override;
    // ~END UMLDeformerInputInfo overrides

    friend class UNearestNeighborModel;
    friend class UNearestNeighborModelInstance;
    friend class UE::NearestNeighborModel::FNearestNeighborEditorModel;

private:
    void InitRefBoneRotations(USkeletalMesh* SkelMesh);
    void ComputeNetworkInput(const TArray<FQuat>& BoneRotations, float* OutputBuffer, int64 StartIndex = 0) const;

private:
    UPROPERTY()
    TArray<FQuat> RefBoneRotations;
};
