// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerMorphModelInputInfo.h"
#include "NearestNeighborModelInputInfo.generated.h"

namespace UE::NearestNeighborModel
{
    class FNearestNeighborEditorModel;
};

UCLASS()
class NEARESTNEIGHBORMODEL_API UNearestNeighborModelInputInfo
    : public UMLDeformerMorphModelInputInfo
{
    GENERATED_BODY()

public:
    // UMLDeformerInputInfo overrides
    virtual int32 CalcNumNeuralNetInputs() const override;
    virtual void ExtractBoneRotations(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutRotations) const override;
    // ~END UMLDeformerInputInfo overrides

    void ComputeNetworkInput(TConstArrayView<FQuat> BoneRotations, TArrayView<float> OutputView) const;

private:
    friend class UE::NearestNeighborModel::FNearestNeighborEditorModel;

    void InitRefBoneRotations(const USkeletalMesh* SkelMesh);

private:
    UPROPERTY()
    TArray<FQuat> RefBoneRotations;
};
